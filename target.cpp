///////////////////////////////////////////////////////////////////////////////
// target.cpp


#include "misc.h"

#include "mayurc.h"
#include "target.h"
#include "windowstool.h"


class Target
{
  HWND hwnd;
  HWND preHwnd;
  HICON hCursor;

  static void invertFrame(HWND hwnd)
  {
    HDC hdc = GetWindowDC(hwnd);
    assert(hdc);
    int rop2 = SetROP2(hdc, R2_XORPEN);
    if (rop2)
    {
      RECT rc;
      _true( GetWindowRect(hwnd, &rc) );
      int width = rcWidth(&rc);
      int height = rcHeight(&rc);
    
      HANDLE hpen = SelectObject(hdc, GetStockObject(WHITE_PEN));
      HANDLE hbr  = SelectObject(hdc, GetStockObject(NULL_BRUSH));
      _true( Rectangle(hdc, 0, 0, width    , height    ) );
      _true( Rectangle(hdc, 1, 1, width - 1, height - 1) );
      _true( Rectangle(hdc, 2, 2, width - 2, height - 2) );
      SelectObject(hdc, hpen);
      SelectObject(hdc, hbr);
      // no need to DeleteObject StockObject
      SetROP2(hdc, rop2);
    }
    _true( ReleaseDC(hwnd, hdc) );
  }
  
  Target(HWND hwnd_)
    : hwnd(hwnd_),
      preHwnd(NULL),
      hCursor(NULL)
  {
  }

  // WM_CREATE
  int wmCreate(CREATESTRUCT * /* cs */)
  {
    _true( hCursor = LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR_target)) );
    return 0;
  }

  // WM_PAINT
  int wmPaint()
  {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    assert(hdc);

    if (GetCapture() != hwnd)
    {
      RECT rc;
      _true( GetClientRect(hwnd, &rc) );
      _true( DrawIcon(hdc, (rcWidth(&rc) - GetSystemMetrics(SM_CXICON)) / 2,
		      (rcHeight(&rc) - GetSystemMetrics(SM_CYICON)) / 2,
		      hCursor) );
    }
    
    EndPaint(hwnd, &ps);
    return 0;
  }

  struct PointWindow
  {
    POINT p;
    HWND hwnd;
    RECT rc;
  };
  
  static BOOL CALLBACK childWindowFromPoint(HWND hwnd, LPARAM lParam)
  {
    if (IsWindowVisible(hwnd))
    {
      PointWindow &pw = *(PointWindow *)lParam;
      RECT rc;
      _true( GetWindowRect(hwnd, &rc) );
      if (PtInRect(&rc, pw.p))
	if (isRectInRect(&rc, &pw.rc))
	{
	  pw.hwnd = hwnd;
	  pw.rc = rc;
	}
    }
    return TRUE;
  }
  
  static BOOL CALLBACK windowFromPoint(HWND hwnd, LPARAM lParam)
  {
    if (IsWindowVisible(hwnd))
    {
      PointWindow &pw = *(PointWindow *)lParam;
      RECT rc;
      _true( GetWindowRect(hwnd, &rc) );
      if (PtInRect(&rc, pw.p))
      {
	pw.hwnd = hwnd;
	pw.rc = rc;
	return FALSE;
      }
    }
    return TRUE;
  }

  // WM_MOUSEMOVE
  int wmMouseMove(WORD /* keys */, int /* x */, int /* y */)
  {
    if (GetCapture() == hwnd)
    {
      PointWindow pw;
      _true( GetCursorPos(&pw.p) );
      pw.hwnd = 0;
      _true( GetWindowRect(GetDesktopWindow(), &pw.rc) );
      EnumWindows(windowFromPoint, (LPARAM)&pw);
      while (1)
      {
	HWND hwndParent = pw.hwnd;
	if (!EnumChildWindows(pw.hwnd, childWindowFromPoint, (LPARAM)&pw))
	  break;
	if (hwndParent == pw.hwnd)
	  break;
      }
      if (pw.hwnd != preHwnd)
      {
	if (preHwnd)
	  invertFrame(preHwnd);
	preHwnd = pw.hwnd;
	invertFrame(preHwnd);
	SendMessage(GetParent(hwnd), WM_targetNotify, 0, (LPARAM)preHwnd);
      }
      SetCursor(hCursor);
    }
    return 0;
  }

  // WM_LBUTTONDOWN
  int wmLButtonDown(WORD /* keys */, int /* x */, int /* y */)
  {
    SetCapture(hwnd);
    SetCursor(hCursor);
    _true( InvalidateRect(hwnd, NULL, TRUE) );
    _true( UpdateWindow(hwnd) );
    return 0;
  }

  // WM_LBUTTONUP
  int wmLButtonUp(WORD /* keys */, int /* x */, int /* y */)
  {
    if (preHwnd)
      invertFrame(preHwnd);
    preHwnd = NULL;
    ReleaseCapture();
    _true( InvalidateRect(hwnd, NULL, TRUE) );
    _true( UpdateWindow(hwnd) );
    return 0;
  }

public:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
				  WPARAM wParam, LPARAM lParam)
  {
    Target *wc;
    getUserData(hwnd, &wc);
    if (!wc)
      switch (message)
      {
	case WM_CREATE:
	  wc = setUserData(hwnd, new Target(hwnd));
	  return wc->wmCreate((CREATESTRUCT *)lParam);
      }
    else 
      switch (message)
      {
	case WM_PAINT:
	  return wc->wmPaint();
	case WM_LBUTTONDOWN:
	  return wc->wmLButtonDown((WORD)wParam, (short)LOWORD(lParam),
				   (short)HIWORD(lParam));
	case WM_LBUTTONUP:
	  return wc->wmLButtonUp((WORD)wParam, (short)LOWORD(lParam),
				 (short)HIWORD(lParam));
	case WM_MOUSEMOVE:
	  return wc->wmMouseMove((WORD)wParam, (short)LOWORD(lParam),
				 (short)HIWORD(lParam));
	case WM_NCDESTROY:
	  delete wc;
	  return 0;
      }
    return DefWindowProc(hwnd, message, wParam, lParam);
  }
};
  

ATOM Register_target()
{
  WNDCLASS wc;
  wc.style         = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc   = Target::WndProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = hInst;
  wc.hIcon         = NULL;
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = "mayuTarget";
  return RegisterClass(&wc);
}
