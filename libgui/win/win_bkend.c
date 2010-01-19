
/**
 *  \file   win_bkend.c
 *  \brief  WorldSens Graphical Windows UI definition 
 *  \author Antoine Fraboulet
 *  \date   2010
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "arch/common/hardware.h"
#include "devices/devices.h"
#include "src/mgetopt.h"
#include "src/options.h"

#include "ui.h"
#include "ui_bkend.h"

/**************************************************/
/**************************************************/
/**************************************************/

#if defined(DEBUG)
#define DMSG_UI(x...) HW_DMSG_UI(x)
#else
#define DMSG_UI(x...) do { } while (0)
#endif

#define UNUSED __attribute__((unused))  

#include <windows.h>

struct win_display_t {
  HINSTANCE   handle;
  HWND        hWnd;
  HBITMAP     hbitmap;
  HDC         MemDC;
  int         width,height,depth;
};

static struct win_display_t win_display;

/**************************************************/
/**************************************************/
/**************************************************/

LPCTSTR ClsName = "WSim";
LPCTSTR WndName = "WSim application";


LRESULT CALLBACK WndProcedure(HWND hWnd, UINT uMsg,
			      WPARAM wParam, LPARAM lParam);

void ClientResize(HWND hWnd, int nWidth, int nHeight)
{
  RECT rcClient, rcWindow;
  POINT ptDiff;
  GetClientRect(hWnd, &rcClient);
  GetWindowRect(hWnd, &rcWindow);
  ptDiff.x = (rcWindow.right - rcWindow.left) - rcClient.right;
  ptDiff.y = (rcWindow.bottom - rcWindow.top) - rcClient.bottom;
  MoveWindow(hWnd,rcWindow.left, rcWindow.top, nWidth + ptDiff.x, nHeight + ptDiff.y, TRUE);
}

void* ui_backend_create(int w, int h, char *title, int *mustlock)
{
  WNDCLASSEX WndClsEx;
  static struct win_display_t *win = &win_display;

  *mustlock = 0;

  /* Sam:
     We still need to pass in the application handle so that
     DirectInput will initialize properly when SDL_RegisterApp()
     is called later in the video initialization.
  */
  win->handle  = GetModuleHandle(NULL);
  win->hWnd    = 0;
  win->hbitmap = 0;
  
  // Create the application window
  WndClsEx.cbSize        = sizeof(WNDCLASSEX);
  WndClsEx.style         = CS_HREDRAW | CS_VREDRAW;
  WndClsEx.lpfnWndProc   = WndProcedure;
  WndClsEx.cbClsExtra    = 0;
  WndClsEx.cbWndExtra    = 0;
  WndClsEx.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
  WndClsEx.hCursor       = LoadCursor(NULL, IDC_ARROW);
  WndClsEx.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  WndClsEx.lpszMenuName  = NULL;
  WndClsEx.lpszClassName = ClsName;
  WndClsEx.hInstance     = win->handle;
  WndClsEx.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
  
  // Register the application
  RegisterClassEx(&WndClsEx);
  
  // Create the window object
  win->hWnd = CreateWindow(ClsName,
		      title, // WndName,
		      WS_OVERLAPPEDWINDOW,
		      CW_USEDEFAULT, // x
		      CW_USEDEFAULT, // y
		      CW_USEDEFAULT, // w
		      CW_USEDEFAULT, // h
		      NULL,
		      NULL,
		      win->handle,
		      NULL);
  
  // Find out if the window was created
  if( !win->hWnd ) // If the window was not created,
    {
      return NULL; // stop the application
    }

  ClientResize(win->hWnd, w, h);

  // Display the window to the user
  InvalidateRect (win->hWnd,0,0);
  UpdateWindow(win->hWnd);

  ShowWindow(win->hWnd, SW_SHOWNORMAL);
  UpdateWindow(win->hWnd);

  /* Image */
  win->width   = w;
  win->height  = h;
  win->depth   = 24;

  /* Bimap */
  {
    HDC    hDC;
    BITMAP bm;

    hDC = GetDC(win->hWnd);

    win->MemDC = CreateCompatibleDC(hDC);
    win->hbitmap = CreateCompatibleBitmap(hDC, win->width, win->height); 
    SelectObject(win->MemDC, win->hbitmap);
    GetObject(win->hbitmap,sizeof(BITMAP),(LPSTR)&bm);

    ReleaseDC(win->hWnd, hDC);

    /*
    printf("type %d w:%d h:%d wb:%d p:%d bpp:%d\n",
	   bm.bmType, bm.bmWidth, bm.bmHeight, bm.bmWidthBytes, 
	   bm.bmPlanes, bm.bmBitsPixel);
    */
  }

  return win;
}


/**************************************************/
/**************************************************/
/**************************************************/

void ui_backend_delete(void *ptr)
{
  struct win_display_t *win = (struct win_display_t*)ptr;
  if (win->hbitmap)
    {
      DeleteObject(win->hbitmap);
      win->hbitmap = 0;
    }
  if (win->MemDC)
    {
      DeleteDC(win->MemDC);
      win->MemDC = 0;
    }
  PostQuitMessage(WM_QUIT);
}

/**************************************************/
/**************************************************/
/**************************************************/
 
int ui_backend_framebuffer_blit(void *ptr, uint8_t UNUSED *fb)
{
  int w,h;
  int idx_buff;
  struct win_display_t *win = (struct win_display_t*)ptr;

  for(h=0; h < win->height; h++)
    {
      idx_buff =  h * win->width * 3;
      for(w=0; w < win->width; w++)
	{
	  COLORREF rgbval = RGB(
			    (fb[idx_buff + 2]),
			    (fb[idx_buff + 1]),
			    (fb[idx_buff + 0])
			    );

	  SetPixel(win->MemDC, w, h, rgbval);
	  idx_buff += 3;
	}
    }
  return UI_OK;
}

/**************************************************/
/**************************************************/
/**************************************************/

int ui_backend_lock(void UNUSED *ptr)
{
  // struct win_display_t *win = (struct win_display_t*)ptr;
  return 0;
}

/**************************************************/
/**************************************************/
/**************************************************/

int ui_backend_unlock(void UNUSED *ptr)
{
  // struct win_display_t *win = (struct win_display_t*)ptr;
  return 0;
}

/**************************************************/
/**************************************************/
/**************************************************/

int ui_backend_update(void *ptr)
{
  struct win_display_t *win = (struct win_display_t*)ptr;

  InvalidateRect (win->hWnd,0,0);
  UpdateWindow   (win->hWnd    );
}

/**************************************************/
/**************************************************/
/**************************************************/

int ui_backend_getevent(void UNUSED *ptr, uint32_t *b_up, uint32_t* b_down)
{
  *b_up   = 0;
  *b_down = 0;

  /* PeekMessage() */

  /*  
      MSG  Msg;
      do {
      GetMessage(&Msg, NULL, 0, 0);
      TranslateMessage(&Msg);
      DispatchMessage(&Msg);
      } while (Msg.message != WM_PAINT);
      return 0;
  */

  return UI_EVENT_NONE;
}

/**************************************************/
/**************************************************/
/**************************************************/

LRESULT CALLBACK WndProcedure(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  HDC hDC;
  PAINTSTRUCT Ps;

  switch(Msg)
    {
    case WM_PAINT:
      hDC = BeginPaint(hWnd, &Ps);
      BitBlt(hDC, 0, 0, win_display.width, win_display.height, 
	     win_display.MemDC, 0, 0, SRCCOPY);
      EndPaint(hWnd, &Ps);
      break;

      
    case WM_DESTROY:
      /* PostQuitMessage(WM_QUIT); */
      mcu_signal_add(SIG_HOST | 3);
      break;
    default:
      return DefWindowProc(hWnd, Msg, wParam, lParam);
    }
    return 0;
}

/**************************************************/
/**************************************************/
/**************************************************/

INT WINAPI WinMain(HINSTANCE UNUSED hInstance, HINSTANCE UNUSED hPrevInstance,
               LPSTR UNUSED lpCmdLine, int UNUSED nCmdShow)
{
  /* put a warning messagebox and exit */
  
  MessageBox(NULL,
	     "Although This program has a graphical interface,\n\r"
	     "it can only be stared using a console command line.",
	     "WSim: error",
	     MB_APPLMODAL | MB_OK
	     );
  
  return 0;
}

/**************************************************/
/**************************************************/
/**************************************************/