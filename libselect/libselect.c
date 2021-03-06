
/**
 * \file   libselect.c
 * \brief  Multiple client select library
 * \author Antoine Fraboulet
 * \date   2006
 **/


#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#if defined(_WIN32)
#define ORIG_WIN32 1
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#define WINPIPES 1 /* convenience macro to enable these features
		      and keep the decision logic in one place */
#endif

#if defined(ORIG_WIN32)
#define snprintf _snprintf
#endif

#if defined(__MINGW32__)
  #include <winsock2.h>
#else
  #include <sys/select.h>
#endif

#include "arch/common/hardware.h"
#include "liblogger/logger.h"
#include "libselect_fifomem.h"
#include "libselect_socket.h"
#include "libselect_file.h"
#include "libselect.h"


/****************************************
 * For performance purpose and because this
 * is an early version most of the libselect
 * dimensions are fixed
 ****************************************/

#define SELECT_SKIP_UPDATES       0

#if SELECT_SKIP_UPDATES != 0
#define LIBSELECT_UPDATE_SKIP     200
#endif

#define DEFAULT_FIFO_SIZE         5120
#define LIBSELECT_MAX_ENTRY       20
#define BUFFER_MAX                DEFAULT_FIFO_SIZE /* max 64ko == IP datagram max size */

#define BACKTRACK_DEFAULT_SETTING 1 /* on */
/****************************************
 * libselect internal structure
 *
 ****************************************/

/*
  NONE         : empty id
  FILE         : input/output data from a file descriptor
  TCP_LISTEN   : tcp listen socket
  TCP_SERV     : tcp socket after accept has been called
  UDP          : udp socket
  FD_ONLY      : I/O data is not handled by libselect
  STDIO        : stdin | stdout
  WIN32_PIPE   : windows win32 pipe 
*/

enum entry_type_t {
  ENTRY_NONE       = 0,
  ENTRY_FILE       = 1,
  ENTRY_TCP        = 2,
  ENTRY_TCP_SRV    = 3,
  ENTRY_UDP        = 4,
  ENTRY_FD_ONLY    = 5,
  ENTRY_STDIO      = 6,
  ENTRY_WIN32_PIPE = 7,
};

struct libselect_entry_t {
  enum entry_type_t         entry_type;    /* type of Entry                      */
  int                       registered;    /* input in use ?                     */

  int                       fd_in;         /* from fd to wsim                    */
  int                       fd_out;        /* from wsim to fd                    */
  struct libselect_socket_t skt;           /* unix socket                        */

  unsigned int              fifo_size;     /* i/o fifo size                      */
  libselect_fifo_input_t    fifo_input;    /* input data fifo : from fd to wsim  */
  libselect_fifo_output_t   fifo_output;   /* output data fifo : from wsim to fd */

  libselect_callback        callback;      /* callback function()                */
  void                     *cb_ptr;        /* registered data for callback func. */
  unsigned int              signal;        /* signal associated with fifo events */

  int                       backtrack;     /* should we commit on save           */
};

struct libselect_t {
  struct libselect_entry_t entry[LIBSELECT_MAX_ENTRY];
  int state;   /* number of id                    */
};

static struct libselect_t libselect;
static int                libselect_init_done = 0;
static int                libselect_ws_mode   = WS_MODE_WSNET0;

/*****************************************
 * libselect update function pointer
 *
 *****************************************/

int (*libselect_update_ptr)      () = NULL;
int libselect_update_registered  ();

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int libselect_init(int ws_mode)
{
  int id;
  memset(&libselect, 0, sizeof(struct libselect_t));
  libselect_init_done  = 1;
  libselect_update_ptr = NULL;
  libselect_ws_mode    = ws_mode;

  for(id=0; id < LIBSELECT_MAX_ENTRY; id++)
    {
      libselect.entry[id].entry_type = ENTRY_NONE;
      libselect.entry[id].fd_in      = -1;
      libselect.entry[id].fd_out     = -1;
      libselect.entry[id].registered = 0;
      libselect.entry[id].signal     = 0;
      libselect.entry[id].backtrack  = 0;
      libselect.entry[id].callback   = NULL;
    }
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int libselect_close (void)
{
  int id;
  for (id=0; id < LIBSELECT_MAX_ENTRY; id++)
    {
      if (libselect.entry[id].entry_type != ENTRY_NONE &&
	  libselect.entry[id].entry_type != ENTRY_FD_ONLY)
	{
	  libselect_id_close(id);
	}
    }
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

char* entry_type_str(int type)
{
  switch (type)
    {
    case ENTRY_NONE:       return "NONE";
    case ENTRY_FILE:       return "FILE";
    case ENTRY_TCP:        return "TCP";
    case ENTRY_TCP_SRV:    return "TCP (listen)";
    case ENTRY_UDP:        return "UDP";
    case ENTRY_FD_ONLY:    return "External";
    case ENTRY_STDIO:      return "STDIO";
    case ENTRY_WIN32_PIPE: return "Win32 pipe";
    default:               return "Unknown";
    }
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

static inline int libselect_max(int a, int b) { return ((a)<(b) ? (b):(a)); }

int libselect_update_registered()
{
  int res;
  int id, n = 0;
  int fd_max;
  fd_set readfds;
  struct timeval timeout;
  unsigned char buffer[BUFFER_MAX];

#if SELECT_SKIP_UPDATES != 0
  static int skippy = 0;
  if (skippy < LIBSELECT_UPDATE_SKIP)
    {
      skippy++;
      return 0;
    }
  skippy = 0;
#endif

#if defined(__MINGW32__) 
	  /* comparison between signed and unsigned warning */
#define MINGWMOD (unsigned)
#else
#define MINGWMOD
#endif

  fd_max=0;
  FD_ZERO(&readfds);
  for(id=0; id < LIBSELECT_MAX_ENTRY; id++)
    {
      switch (libselect.entry[id].entry_type)
	{
	case ENTRY_NONE:
	  break;
	case ENTRY_WIN32_PIPE:
	  break;
	case ENTRY_FILE:
	case ENTRY_TCP:
	case ENTRY_TCP_SRV:
	case ENTRY_UDP:
	case ENTRY_FD_ONLY:
	case ENTRY_STDIO:
	  if (libselect.entry[id].registered)
	    {
	      /* *DMSG("wsim:libselect:update: add id=%d fd=%d\n",id,libselect.entry[id].fd_in); */
  	      FD_SET(MINGWMOD libselect.entry[id].fd_in, &readfds);
	      fd_max = libselect_max(libselect.entry[id].fd_in , fd_max);
	    }
	  break;
	}
    }

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  switch (res = select(fd_max + 1, &readfds, NULL, NULL, &timeout))
    {
    case -1: /* error */
      perror("wsim:libselect:update: error during select(), host interrupt\n");
      mcu_signal_set(SIG_HOST);
      return 1;
    case 0:  /* timeout */
      // DMSG("wsim:libselect: nothing to read on timeout\n");
      return 0;
    default: /* something to read */
      break;
    }

  for(id=0; id < LIBSELECT_MAX_ENTRY; id++)
    {
      int fd_in = libselect.entry[id].fd_in;
      if (libselect.entry[id].registered && FD_ISSET(fd_in,&readfds))
	{
	  switch (libselect.entry[id].entry_type)
	    {
	    case ENTRY_NONE:
	      ERROR("wsim:libselect:update: select returns on fd entry type NONE (id=%d)\n",id);
	      break;
	      
	    case ENTRY_FILE:
	    case ENTRY_UDP:
	    case ENTRY_TCP:
	    case ENTRY_STDIO:
	      switch (n = read(fd_in,buffer,BUFFER_MAX)) 
		{
		case -1:
		  ERROR("wsim:libselect:update: error on descriptor (id=%d:%d) type %s\n",id,fd_in,
			entry_type_str(libselect.entry[id].entry_type) );
		case 0:
		  if (libselect.entry[id].callback)
		    {
		      WARNING("wsim:libselect:update: fifo id %d has been closed\n",id);
		      libselect.entry[id].callback(id,LIBSELECT_EVT_CLOSE,libselect.entry[id].cb_ptr);
		    }
		  libselect_id_unregister(id);
		  break;
		default:
		  DMSG("wsim:libselect:update: something to read on id %d = %d bytes\n",id,n);
		  if (libselect_fifo_input_putblock(libselect.entry[id].fifo_input,buffer,n) < n)
		    {
		      ERROR("wsim: ===========================================\n");
		      ERROR("wsim: Overrun error on input %d (type %s)\n",id,
		            entry_type_str(libselect.entry[id].entry_type));
		      ERROR("wsim: - data have been lost during simulation\n");
		      ERROR("wsim: - the sender does not set its sending speed according to \n");
		      ERROR("wsim:   the target capabilities\n");
		      ERROR("wsim: ===========================================\n");
		    }
		  break;
		}
	      break;
	      
	    case ENTRY_TCP_SRV: /* accept() -> create an open ENTRY_TCP */
	      if (fd_in == libselect.entry[id].skt.socket_listen)
		{
		  if (libselect_skt_accept( & libselect.entry[id].skt )) 
		    {
		      libselect.entry[id].skt.socket = -1;
		      libselect.entry[id].fd_in      = libselect.entry[id].skt.socket_listen;
		      libselect.entry[id].fd_out     = -1;
		    }
		  else
		    {
		      DMSG("wsim:libselect:update:accepted connection on port %d\n",libselect.entry[id].skt.port);
		      libselect.entry[id].fd_in  = libselect.entry[id].skt.socket;
		      libselect.entry[id].fd_out = libselect.entry[id].skt.socket;
		    }
		}
	      else 
		{
		  if ((n = read(fd_in,buffer,BUFFER_MAX)) > 0)
		    {
		      DMSG("wsim:libselect:update: something to read on id %d = %d bytes\n",id,n);
		      if (libselect_fifo_input_putblock(libselect.entry[id].fifo_input,buffer,n) < n)
			{
			  ERROR("wsim:libselect:update: fifo overrun on descriptor %d\n",id);
			}
		    }
		  else
		    {
		      libselect.entry[id].skt.socket = -1;
		      libselect.entry[id].fd_in      = libselect.entry[id].skt.socket_listen;
		      libselect.entry[id].fd_out     = -1;
		      DMSG("wsim:libselect:update: read id %d returned %d\n",id,n);
		      if (libselect.entry[id].callback)
			{
			  WARNING("wsim:libselect:update: fifo id %d has been closed\n",id);
			  libselect.entry[id].callback(id,LIBSELECT_EVT_CLOSE,libselect.entry[id].cb_ptr);
			}
		    }
		}
	      break;

	    case ENTRY_WIN32_PIPE:
	      break;

	    case ENTRY_FD_ONLY:
	      mcu_signal_add(libselect.entry[id].signal);
	      DMSG("wsim:libselect: something to read on id %d (signal)\n",id);
	      break;
	    } /* switch */
	} /* if registered and isset */
    } /* for (id) */
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int file_exists(const char *filename)
{
  struct stat statbuf;
  if (stat(filename, &statbuf) == -1)
    {
      /* file does not exists */
      return 0;
    }
  return 1;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

#define CREATE_IO_IN  1
#define CREATE_IO_OUT 2

int libselect_id_create_io(int type, const char* cmdline)
{
  if (strcmp(cmdline,"stdin") == 0)
    {
      switch (type) {
      case CREATE_IO_IN:
	return 0;
      case CREATE_IO_OUT:
	ERROR("wsim:libselect: Cannot open stdout as output stream\n");
	return -1;
      }
    }
  else if (strcmp(cmdline,"stdout") == 0)
    {
      switch (type) {
      case CREATE_IO_IN:
	ERROR("wsim:libselect: Cannot open stdout as input stream\n");
	return -1;
      case CREATE_IO_OUT:
	return 1;
      }
    }
  else if (strcmp(cmdline,"stderr") == 0)
    {
      switch (type) {
      case CREATE_IO_IN:
	ERROR("wsim:libselect: Cannot open stderr as input stream\n");
	return -1;
      case CREATE_IO_OUT:
	return 2;
      }
    }
  else if (strcmp(cmdline,"create") == 0)
    {
      int fd;
      char f_local [MAX_FILENAME];
      char f_remote[MAX_FILENAME];
      if ((fd = libselect_get_system_fifo(f_local, f_remote)) == -1)
	{
	  ERROR("wsim:libselect: Cannot create system fifo\n");
	  return -1;
	}
      WARNING("wsim:libselect: creating fifo in write only mode\n");
      return fd;
    }
  else
    {
      int fd;
      int flags;
      switch (type) {
      case CREATE_IO_IN:  
	flags = O_RDONLY; 
	break;
      case CREATE_IO_OUT:
	/* check if file exists */
	flags = O_WRONLY | O_TRUNC;
	if (! file_exists(cmdline))
	  {
	    flags |= O_CREAT;
	  }
	break;
      default: return -1;
      }
      
      if (flags & O_CREAT) // O_CREAT flag requires a third argument named "mode" 
	fd = open(cmdline, flags, S_IRWXU);
      else
	fd = open(cmdline, flags);
     
      if (fd == -1)
	{
	  ERROR("wsim:libselect: cannot open file %s - %s\n",cmdline, strerror(errno));
	  return -1;
	}
      fchmod(fd, S_IRUSR | S_IWUSR);
      return fd;
    }
  return -1;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

libselect_id_t libselect_id_create(const char *argname, int UNUSED flags)
{
  int  id;
  const char *cmdline;

  if (libselect_init_done == 0)
    {
      ERROR("wsim:libselect: id_create before libselect_init()\n");
      return -1;
    }

  /* find the first available id */
  for(id=0; id < LIBSELECT_MAX_ENTRY; id++)
    {
      if (libselect.entry[id].entry_type == ENTRY_NONE)
	{
	  break;
	}
    }

  if (id == LIBSELECT_MAX_ENTRY)
    {
      return -1;
    }

  cmdline = argname;
  if (strstr(cmdline,"bk:") == cmdline)
    {
      cmdline += 3;
      DMSG("wsim:libselect: open file %s *with* backtrack buffer on input/output\n",cmdline);
      libselect.entry[id].backtrack = 1;
    }
  else if (strstr(cmdline,"nobk:") == cmdline)
    {
      cmdline += 5;
      DMSG("wsim:libselect: open file %s *without* backtrack buffer on input/output\n",cmdline);
      libselect.entry[id].backtrack = 0;
    }
  else 
    {
      libselect.entry[id].backtrack = BACKTRACK_DEFAULT_SETTING; 
    }

  libselect.entry[id].fd_in       = -1;
  libselect.entry[id].fd_out      = -1;
  libselect.entry[id].signal      = 0;
  libselect.entry[id].callback    = NULL;
  libselect.entry[id].cb_ptr      = NULL;
  libselect.entry[id].fifo_size   = DEFAULT_FIFO_SIZE;
  libselect.entry[id].fifo_input  = NULL;
  libselect.entry[id].fifo_output = NULL;
  
  if (strstr(cmdline,"tcp:s:") == cmdline)
    {
      if (libselect_skt_init(& libselect.entry[id].skt, cmdline) == -1)
	{
	  ERROR("wsim:libselect: Cannot open TCP SRV socket %s\n",cmdline);
	  return -1;
	}
      libselect.entry[id].entry_type = ENTRY_TCP_SRV;
      libselect.entry[id].fd_in      = libselect.entry[id].skt.socket_listen;
      libselect.entry[id].fd_out     = -1;
    }
  else if (strstr(cmdline,"tcp:c:") == cmdline)
    {
      if (libselect_skt_init(& libselect.entry[id].skt, cmdline) == -1)
	{
	  ERROR("wsim:libselect: Cannot open TCP socket %s\n",cmdline);
	  return -1;
	}
      libselect.entry[id].entry_type = ENTRY_TCP;
      libselect.entry[id].fd_in      = libselect.entry[id].skt.socket;
      libselect.entry[id].fd_out     = libselect.entry[id].skt.socket;
    }
  else if (strstr(cmdline,"udp:") == cmdline)
    {
      if (libselect_skt_init(& libselect.entry[id].skt, cmdline) == -1)
	{
	  ERROR("wsim:libselect: Cannot open UDP socket %s\n",id,cmdline);
	  return -1;
	}
      libselect.entry[id].entry_type = ENTRY_UDP;
      libselect.entry[id].fd_in      = libselect.entry[id].skt.socket;
      libselect.entry[id].fd_out     = libselect.entry[id].skt.socket;
    }
#ifdef WINPIPES
  else if (strstr(cmdline,"pipe:") == cmdline)
    {
      char szPipe[MAX_PATH] = {0,};
      snprintf(szPipe, sizeof(szPipe), "\\\\.\\pipe\\%s", cmdline + 5);
      HANDLE hPipe = CreateNamedPipe(szPipe, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 32768, 32768, INFINITE, NULL);
      if (hPipe == INVALID_HANDLE_VALUE)
	{
	  ERROR("wsim:libselect: Cannot create named pipe %s\n", szPipe);
	  return -1;
	}
      ConnectNamedPipe(hPipe, NULL);
      
      libselect.entry[id].entry_type = ENTRY_WIN32_PIPE;
      //libselect.entry[id].fd_in      = (int)hPipe;
      libselect.entry[id].fd_out     = (int)hPipe;
    }
#endif
  else if (strchr(cmdline,',') != NULL)
    {
      // cmdline = output, input 
      char *c;
      int fd_in, fd_out;
      
      c = strchr(cmdline,',');
      c[0] = '\0';
      c = c+1;

      if ((fd_out = libselect_id_create_io(CREATE_IO_OUT, cmdline)) == -1)
	{
	  ERROR("wsim:libselect: Cannot open output file %s\n",cmdline);
	  return -1;
	}
      if ((fd_in = libselect_id_create_io(CREATE_IO_IN, c)) == -1)
	{
	  ERROR("wsim:libselect: Cannot open input file %s\n",c);
	  return -1;
	}

      libselect.entry[id].entry_type = ENTRY_FILE;
      libselect.entry[id].fd_in      = fd_in;
      libselect.entry[id].fd_out     = fd_out;
    }
  else // cmdline = fifo_out
    {
      int fd;
      if ((fd = libselect_id_create_io(CREATE_IO_OUT, cmdline)) == -1)
	{
	  ERROR("wsim:libselect: Cannot open output file %s\n",cmdline);
	  return -1;
	}
      WARNING("wsim:libselect: opening '%s' in write only mode\n",cmdline);
      libselect.entry[id].entry_type = ENTRY_FILE;
      libselect.entry[id].fd_in      = -1;
      libselect.entry[id].fd_out     = fd;
    }

  /* allocate fifo memory if needed */
  if (libselect.entry[id].fifo_size > 0)
    {
      libselect.entry[id].fifo_input  = libselect_fifo_input_create(  id, libselect.entry[id].fifo_size );
      libselect.entry[id].fifo_output = libselect_fifo_output_create( id, libselect.entry[id].fifo_size );
    }
  else
    {
      libselect.entry[id].fifo_input  = NULL;
      libselect.entry[id].fifo_output = NULL;
    }

  DMSG("wsim:libselect:create: %s, id=%d, fd_in=%d\n",cmdline,id,libselect.entry[id].fd_in);
  return id;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

inline int libselect_id_is_valid(libselect_id_t id)
{
  int ret = 0;
  ret += (libselect.entry[id].entry_type != ENTRY_NONE);
  return ret;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int libselect_id_close(libselect_id_t id)
{
  switch (libselect.entry[id].entry_type)
    {
    case ENTRY_NONE:
      /* */
      return 1;
    case ENTRY_FILE:
      if (libselect.entry[id].fd_in != -1)
	{
	  close(libselect.entry[id].fd_in);
	}
      if (libselect.entry[id].fd_out != -1)
	{
	  close(libselect.entry[id].fd_out);
	}
      break;
    case ENTRY_TCP:
      libselect_skt_close_client (& libselect.entry[id].skt);
      break;
    case ENTRY_TCP_SRV:
      libselect_skt_close_client (& libselect.entry[id].skt);
      libselect_skt_close        (& libselect.entry[id].skt);
      break;
    case ENTRY_UDP:
      libselect_skt_close_client (& libselect.entry[id].skt);
      break;
    case ENTRY_FD_ONLY: /* I/O data is not handled by libselect */
      /*  */
      return 0;
    case ENTRY_STDIO: /* do not close stdin and/or stdout */
      /*  */
      break;
    case ENTRY_WIN32_PIPE:
#ifdef WINPIPES
      CloseHandle((HANDLE)libselect.entry[id].fd_out);
#endif
      return 1;
    }

  libselect_fifo_input_delete(libselect.entry[id].fifo_input);
  libselect_fifo_output_delete(libselect.entry[id].fifo_output);
  libselect.entry[id].entry_type = ENTRY_NONE;
  libselect.entry[id].registered = 0;
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int libselect_id_is_input(libselect_id_t id)
{
  return libselect.entry[id].fd_in != -1;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int libselect_id_register(libselect_id_t id)
{
  if (libselect_init_done == 0)
    {
      ERROR("wsim:libselect: registering fifo file descriptor before libselect_init()\n");
      return 1;
    }

  if (libselect_id_is_valid(id) == 0)
    {
      ERROR("wsim:libselect: trying to register invalid id %d\n",id);
      return 1;
    }

  if (libselect.entry[id].registered == 1)
    {
      ERROR("wsim:libselect: trying to register already registered file descriptor %d\n",id);
      return 1;
    }

  if (libselect.entry[id].fd_in == -1)
    {
      WARNING("wsim:libselect: trying to register closed Input descriptor %d\n",id);
      return 1;
    }

  DMSG("wsim:libselect:register: id=%d, fd_in=%d\n",id,libselect.entry[id].fd_in);
  libselect.entry[id].registered = 1;
  libselect.state               += 1;
  libselect_update_ptr           = libselect_update_registered;

  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int libselect_id_unregister(libselect_id_t id)
{
  if (libselect.state == 0)
    {
      ERROR("libselect: internal state error, want to unregister id %d \n",id);
      return 1;
    }

  if (libselect_id_is_valid(id) == 0)
    {
      ERROR("libselect: trying to register invalid id %d\n",id);
      return 1;
    }

  if (libselect.entry[id].registered == 0)
    {
      ERROR("libselect: trying to un-register id %d that is not registered\n",id);
      return 1;
    }

  libselect.entry[id].registered  = 0;
  libselect.state                -= 1;
  if (libselect.state == 0)
    {
      libselect_update_ptr        = NULL;
    }
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int libselect_id_add_callback(libselect_id_t id, libselect_callback callback, void *ptr)
{
  if (libselect_id_is_valid(id) == 0)
    {
      ERROR("libselect: trying to add callback to invalid id %d\n",id);
      return 1;
    }

  DMSG("libselect: add callback for id %d\n",id);
  libselect.entry[id].callback = callback;
  libselect.entry[id].cb_ptr   = ptr;
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

uint32_t libselect_id_read(libselect_id_t id, uint8_t *data, uint32_t size)
{
  uint32_t ret = 0;
  if (libselect.entry[id].fifo_input)
    {
      if (libselect.entry[id].backtrack && (libselect_ws_mode != WS_MODE_WSNET0))
	{
	  ret = libselect_fifo_input_readblock(libselect.entry[id].fifo_input,data,size);
	  if (ret > 0)
	    {
	      DMSG_BK("wsim:libselect:bk: READ %d bytes to id=%d, fd=%d, fifo=%04d\n",
	              size, id, libselect.entry[id].fd_in,
	              libselect_fifo_input_avail ( libselect.entry[id].fifo_input ) );
	    }
	}
      else
	{
	  ret = libselect_fifo_input_getblock(libselect.entry[id].fifo_input,data,size);
	  if (ret > 0)
	    {
	      DMSG("wsim:libselect: WRITE %d bytes to id=%d, fd=%d, val=%c\n",
	           size, id, libselect.entry[id].fd_in,
	           isprint(data[0]) ? data[0] : '.');
	    }
	}
    }
  return ret;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

uint32_t libselect_id_write(libselect_id_t id, uint8_t *data, uint32_t size)
{
  uint32_t ret = -1;
  if (libselect.entry[id].fd_out != -1)
    {
      if (libselect.entry[id].backtrack && (libselect_ws_mode != WS_MODE_WSNET0))
	{
	  ret = libselect_fifo_output_putblock (libselect.entry[id].fifo_output, data, size);
	  DMSG_BK("wsim:libselect:bk: WRITE %d bytes to id=%d, fd=%d, fifo=%04d\n",
		  size, id, libselect.entry[id].fd_out,
		  libselect_fifo_output_avail ( libselect.entry[id].fifo_output ) );
	}
      else
	{
#ifdef WINPIPES
	  if (libselect.entry[id].entry_type == ENTRY_WIN32_PIPE) 
	    {
		  DWORD r;
		  if (WriteFile((HANDLE)(libselect.entry[id].fd_out), data, 
		                (size > UINT32_MAX - 1) ? UINT32_MAX - 1 : size, &r, NULL))
		    {
		      ret = r; /* safe as we limit the number of bytes */
		    }
		  else
		    {
		      ret = -1;
		    }
	    } 
	  else
#endif
	    {
	      ret = write(libselect.entry[id].fd_out, data, size);
	    }
	  DMSG("wsim:libselect: WRITE %d bytes to id=%d, fd=%d, val=%c\n",
	       size, id, libselect.entry[id].fd_out,
	       isprint(data[0]) ? data[0] : '.');
	}
    }
  return ret;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int libselect_fd_register(int fd, unsigned int signal)
{
  int id;
  if (libselect_init_done == 0)
    {
      ERROR("libselect: registering signal file descriptor before libselect_init()\n");
      return 1;
    }

  for(id=0; id < LIBSELECT_MAX_ENTRY; id++)
    {
      if (libselect.entry[id].entry_type == ENTRY_NONE)
	{
	  libselect.entry[id].entry_type = ENTRY_FD_ONLY;
	  libselect.entry[id].fd_in      = fd;
	  libselect.entry[id].registered = 1;
	  libselect.entry[id].signal     = signal;
	  libselect.entry[id].backtrack  = 0;
	  libselect.state               += 1;
	  libselect_update_ptr           = libselect_update_registered;
	  return id;
	}
    }
  return -1;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int libselect_fd_unregister(int fd)
{
  int id;
  for(id=0; id < LIBSELECT_MAX_ENTRY; id++)
    {
      if (libselect.entry[id].entry_type == ENTRY_FD_ONLY &&
	  libselect.entry[id].fd_in      == fd            &&
	  libselect.entry[id].registered == 1 )
	{
	  libselect.entry[id].entry_type = ENTRY_NONE;
	  libselect.entry[id].fd_in      = -1;
	  libselect.entry[id].fd_out     = -1;
	  libselect.entry[id].registered = 0;
	  libselect.entry[id].signal     = 0;
	  libselect.entry[id].backtrack  = 0;
	  libselect.state               -= 1;
	  if (libselect.state == 0)
	    {
	      libselect_update_ptr       = NULL;
	    }
	  return id;
	}
    }
  ERROR("libselect:fd: trying to un-register fd %d that is not registered\n",fd);
  return -1;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

void libselect_state_save(void)
{
  int size;
  uint8_t data[BUFFER_MAX];
  libselect_id_t id;
  for(id=0; id < LIBSELECT_MAX_ENTRY; id++)
    {
      if (libselect_id_is_valid(id) && 
	  libselect.entry[id].backtrack)
	{
	  /* input
	   *   - commit reads
	   */
	  size = libselect_fifo_input_readcommit  ( libselect.entry[id].fifo_input );
	  if (size > 0)
	    {
	      DMSG_BK("wsim:libselect:bk: SAVE+READ id=%d, fd=%d, read %d bytes\n",
	              id,libselect.entry[id].fd_in,size);
	    }

	  /* output 
	   *   - write the whole fifo content  
	   */
	  size = libselect_fifo_output_avail ( libselect.entry[id].fifo_output );
	  if (size > 0)
	    {
	      libselect_fifo_output_getblock    ( libselect.entry[id].fifo_output, data, size );
	      libselect_fifo_output_flush       ( libselect.entry[id].fifo_output) ;
	      if (write(libselect.entry[id].fd_out, data, size) != size)
		{
		  ERROR("wsim:libselect:bk: error on write id=%d, fd=%d\n",
			id,libselect.entry[id].fd_out);
		}
	      else
		{
		  DMSG_BK("wsim:libselect:bk: SAVE+WRITE id=%d, fd=%d, write %d bytes\n",
			  id,libselect.entry[id].fd_out,size);
		}
	    }
	  else
	    {
	      DMSG_BK("wsim:libselect:bk: SAVE id=%d, fd=%d, size %d bytes\n",
		      id,libselect.entry[id].fd_out,size);
	    }
	}      
    }
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

void libselect_state_restore(void)
{
  int UNUSED size;
  libselect_id_t id;
  for(id=0; id < LIBSELECT_MAX_ENTRY; id++)
    {
      if (libselect_id_is_valid(id) &&
	  libselect.entry[id].backtrack)
	{
	  /* input
	   *
	   */
	  size = libselect_fifo_input_readcancel ( libselect.entry[id].fifo_input );
	  DMSG_BK("wsim:libselect:bk: RESTORE id=%d, fd=%d, cancel read %d bytes\n",
		  id,libselect.entry[id].fd_in,size);

	  /* output 
	   *
	   */
	  size = libselect_fifo_output_avail ( libselect.entry[id].fifo_output );
	  libselect_fifo_output_flush( libselect.entry[id].fifo_output );
	  DMSG_BK("wsim:libselect:bk: RESTORE id=%d, fd=%d, cancel write %d bytes\n",
		  id,libselect.entry[id].fd_out,size);
	}      
    }
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */
