// ////////////////////////////////////////////////////////////////////////////
// focus.cpp


#include "focus.h"
#include "windowstool.h"


///
static LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
				WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
      SendMessage(GetParent(hwnd), WM_vkeyNotify, wParam, lParam);
      return 0;
    case WM_CHAR:
    case WM_DEADCHAR:
      return 0;
    case WM_LBUTTONDOWN:
    {
      SetFocus(hwnd);
      return 0;
    }
    case WM_SETFOCUS:
    {
      RECT rc;
      GetClientRect(hwnd, &rc);
      CreateCaret(hwnd, (HBITMAP)NULL, 2, rcHeight(&rc) / 2);
      ShowCaret(hwnd);
      SetCaretPos(rcWidth(&rc) / 2, rcHeight(&rc) / 4);
      SendMessage(GetParent(hwnd), WM_focusNotify, TRUE, (LPARAM)hwnd);
      return 0;
    }
    case WM_KILLFOCUS:
    {
      HideCaret(hwnd);
      DestroyCaret();
      SendMessage(GetParent(hwnd), WM_focusNotify, FALSE, (LPARAM)hwnd);
      return 0;
    }
    case WM_GETDLGCODE:
      return DLGC_WANTALLKEYS;
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}


ATOM Register_focus()
{
  WNDCLASS wc;
  wc.style         = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc   = WndProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = hInst;
  wc.hIcon         = NULL;
  wc.hCursor       = LoadCursor(NULL, IDC_IBEAM);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = "mayuFocus";
  return RegisterClass(&wc);
}
