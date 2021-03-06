
/**
 *  \file   ihex.c
 *  \brief  Intel HEX file loader
 *  \author Antoine Fraboulet
 *  \date   2009
 **/

#include <stdio.h>
#include <string.h>
#include "hardware.h"

/* ************************************** */
/* ************************************** */
/* ************************************** */

#define MAXLINE 100

/* ************************************** */
/* ************************************** */
/* ************************************** */

#define DATA_RECORD   00  /* Data                  */
#define EOF_RECORD    01  /* End of File           */
#define ESA_RECORD    02  /* Extended Segment Addr */
#define SSA_RECORD    03  /* Segment Start Addr    */
#define ELA_RECORD    04  /* Extended Linear Addr  */
#define SLA_RECORD    05  /* Start Linear Addr     */


int htoi(char c)
{
  if ((c >= '0') && (c <= '9'))
    return c - '0';
  if ((c >= 'a') && (c <= 'f'))
    return c -'a' + 10;
  if ((c >= 'A') && (c <= 'F'))
    return c -'A' + 10;
  return 0;
}

uint8_t hex2int(char c1, char c2)
{
#define HEX_BIGENDIAN
#if defined(HEX_BIGENDIAN)
  return (htoi(c1) << 4) + htoi(c2); // bigendian
#else
  return (htoi(c2) << 4) + htoi(c1); // littleendian
#endif
}

/* ************************************** */
/* ************************************** */
/* ************************************** */

void remove_trailing_char(char *line, char c)
{
  int len = strlen(line);
  if ((len > 0) && (line[len - 1] == c))
    {
      line[len-1] = 0;
    }
}

int ihex_read_line(char line[MAXLINE])
{
  char   *hexline;
  int     length;
  uint8_t i,nbytes,checksum;
  uint8_t bytes[MAXLINE];
  
  DMSG_LIB_iHEX("wsim:ihex:line: %-44s",line);

  remove_trailing_char(line,'\n');
  remove_trailing_char(line,'\r');

  length = strlen(line);
  if (length == 0)
    {
      return 0; /* skip empty lines */
    }

  if (line[0] != ':')
    {
      DMSG_LIB_iHEX("does not begin with :\n");
      return -1;
    }
  
  hexline = &line[1];
  if ((strlen(hexline) & 0x01) == 0x1)
    {
      DMSG_LIB_iHEX("size is not a multiple of 2\n");
      return -1;
    }

  for(i=0, nbytes=0, checksum=0; i<(strlen(hexline)/2); i++)
    {
      bytes[i]  = hex2int(hexline[i*2], hexline[i*2+1]);
      checksum += bytes[i];
      nbytes ++;
    }

  if (checksum != 0)
    {
      DMSG_LIB_iHEX("bad checksum %02x %02x\n",checksum,bytes[nbytes - 1]);
      return -1;
    }

#if defined(DEBUG_ME_HARDER)
  for(i=0; i<nbytes; i++)
    {
      DMSG_LIB_iHEX("%02x",bytes[i]);
    }
  DMSG_LIB_iHEX("\n");
#endif

  int      count = (bytes[0]);
  uint16_t addr  = (bytes[1] << 8) | bytes[2];
  uint8_t  type  = (bytes[3]); 
  DMSG_LIB_iHEX(":: %02d bytes, addr 0x%04x, type %02x=",count,addr,type);

  switch (type)
    {
    case DATA_RECORD   :  /* Data                  */
      DMSG_LIB_iHEX("Data :: ");
      for(i=0; i<count; i++)
	{
	  DMSG_LIB_iHEX("%02x ",bytes[4+i]);
	  mcu_jtag_write_byte(addr+i,bytes[4+i]);
	}
      DMSG_LIB_iHEX("\n");
      return 0;
    case EOF_RECORD    :  /* End of File           */
      DMSG_LIB_iHEX("End of File\n");
      return 0;
    case ESA_RECORD    :  /* Extended Segment Addr */
      DMSG_LIB_iHEX("Extended Segment Addr\n");
      return -1;
    case SSA_RECORD    :  /* Segment Start Addr    */
      DMSG_LIB_iHEX("Segment Start Addr\n");
      return -1;
    case ELA_RECORD    :  /* Extended Linear Addr  */
      DMSG_LIB_iHEX("Extended Linear Addr\n");
      return -1;
    case SLA_RECORD    :  /* Start Linear Addr     */
      DMSG_LIB_iHEX("Start Linear Addr\n");
      return -1;
    default:
      DMSG_LIB_iHEX("Unknown line type\n");
      return -1;
    }

  return -1;
}

/* ************************************** */
/* ************************************** */
/* ************************************** */

int mcu_hexfile_load(char *filename)
{
  FILE *f;
  char buff[MAXLINE];

  MESSAGE("wsim:precharge: loading hexfile %s\n",filename);

  if ((f = fopen(filename,"rb")) == NULL)
    {
      perror("wsim:ihex");
      return 0;
    }

  while(fgets(buff,MAXLINE,f) != NULL)
    {
      if (buff[strlen(buff) -1] == '\n' || buff[strlen(buff) -1] == '\r')
	buff[strlen(buff) -1] = '\0';

      if (ihex_read_line(buff) == -1)
	{
	  fclose(f);
	  return -1;
	}
    }

  fclose(f);
  return 1;
}

/* ************************************** */
/* ************************************** */
/* ************************************** */
