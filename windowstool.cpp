// ////////////////////////////////////////////////////////////////////////////
// windowstool.cpp


#include "misc.h"

#include "windowstool.h"

#include <windowsx.h>
#include <malloc.h>


// ////////////////////////////////////////////////////////////////////////////
// Global variables


// instance handle of this application
HINSTANCE hInst = NULL;


// ////////////////////////////////////////////////////////////////////////////
// Functions


// load resource string
std::string loadString(UINT id)
{
  char buf[1024];
  if (LoadString(hInst, id, buf, sizeof(buf)))
    return std::string(buf);
  else
    return "";
}


// load small icon resource
HICON loadSmallIcon(UINT id)
{
  return (HICON)LoadImage(hInst, MAKEINTRESOURCE(id), IMAGE_ICON, 16, 16, 0);
}


// load big icon resource
HICON loadBigIcon(UINT id)
{
  return (HICON)LoadImage(hInst, MAKEINTRESOURCE(id), IMAGE_ICON, 32, 32, 0);
}


// set small icon to the specified window.
//  @return handle of previous icon or NULL
HICON setSmallIcon(HWND hwnd, UINT id)
{
  HICON hicon = (id == (UINT)-1) ? NULL : loadSmallIcon(id);
  return (HICON)SendMessage(hwnd, WM_SETICON,(WPARAM)ICON_SMALL,(LPARAM)hicon);
}


// set big icon to the specified window.
// @return handle of previous icon or NULL
HICON setBigIcon(HWND hwnd, UINT id)
{
  HICON hicon = (id == (UINT)-1) ? NULL : loadBigIcon(id);
  return (HICON)SendMessage(hwnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)hicon);
}


// remove icon from a window that is set by setSmallIcon
void unsetSmallIcon(HWND hwnd)
{
  HICON hicon = setSmallIcon(hwnd, -1);
  if (hicon)
    CHECK_TRUE( DestroyIcon(hicon) );
}


// remove icon from a window that is set by setBigIcon
void unsetBigIcon(HWND hwnd)
{
  HICON hicon = setBigIcon(hwnd, -1);
  if (hicon)
    CHECK_TRUE( DestroyIcon(hicon) );
}


// resize the window (it does not move the window)
bool resizeWindow(HWND hwnd, int w, int h, bool doRepaint)
{
  UINT flag = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER;
  if (!doRepaint)
    flag |= SWP_NOREDRAW;
  return !!SetWindowPos(hwnd, NULL, 0, 0, w, h, flag);
}


// get rect of the window in client coordinates
// @return rect of the window in client coordinates
bool getChildWindowRect(HWND hwnd, RECT *rc)
{
  if (!GetWindowRect(hwnd, rc))
    return false;
  POINT p = { rc->left, rc->top };
  HWND phwnd = GetParent(hwnd);
  if (!phwnd)
    return false;
  if (!ScreenToClient(phwnd, &p))
    return false;
  rc->left = p.x;
  rc->top = p.y;
  p.x = rc->right;
  p.y = rc->bottom;
  ScreenToClient(phwnd, &p);
  rc->right = p.x;
  rc->bottom = p.y;
  return true;
}


/// get toplevel (non-child) window
HWND getToplevelWindow(HWND hwnd, bool *isMDI)
{
  while (hwnd)
  {
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if ((style & WS_CHILD) == 0)
      break;
    if (isMDI && *isMDI)
    {
      LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
      if (exStyle & WS_EX_MDICHILD)
	return hwnd;
    }
    hwnd = GetParent(hwnd);
  }
  if (isMDI)
    *isMDI = false;
  return hwnd;
}


/// move window asynchronously
void asyncMoveWindow(HWND hwnd, int x, int y)
{
  SetWindowPos(hwnd, NULL, x, y, 0, 0,
	       SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
	       SWP_NOSIZE | SWP_NOZORDER);
}


/// move window asynchronously
void asyncMoveWindow(HWND hwnd, int x, int y, int w, int h)
{
  SetWindowPos(hwnd, NULL, x, y, w, h,
	       SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
	       SWP_NOZORDER);
}


/// resize asynchronously
void asyncResize(HWND hwnd, int w, int h)
{
  SetWindowPos(hwnd, NULL, 0, 0, w, h,
	       SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
	       SWP_NOMOVE | SWP_NOZORDER);
}


/// get dll version
DWORD getDllVersion(const char *i_dllname)
{
  DWORD dwVersion = 0;
  
  if (HINSTANCE hinstDll = LoadLibrary(i_dllname))
  {
    DLLGETVERSIONPROC pDllGetVersion
      = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");
    /* Because some DLLs may not implement this function, you
     * must test for it explicitly. Depending on the particular 
     * DLL, the lack of a DllGetVersion function may
     * be a useful indicator of the version.
     */
    if (pDllGetVersion)
    {
      DLLVERSIONINFO dvi;
      ZeroMemory(&dvi, sizeof(dvi));
      dvi.cbSize = sizeof(dvi);

      HRESULT hr = (*pDllGetVersion)(&dvi);
      if (SUCCEEDED(hr))
	dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
    }
        
    FreeLibrary(hinstDll);
  }
  return dwVersion;
}

// ////////////////////////////////////////////////////////////////////////////
// edit control


// get edit control's text size
// @return bytes of text
size_t editGetTextBytes(HWND hwnd)
{
  return Edit_GetTextLength(hwnd);
}


// delete a line
void editDeleteLine(HWND hwnd, size_t n)
{
  int len = Edit_LineLength(hwnd, n);
  if (len < 0)
    return;
  len += 2;
  int index = Edit_LineIndex(hwnd, n);
  Edit_SetSel(hwnd, index, index + len);
  Edit_ReplaceSel(hwnd, "");
}
  

// insert text at last
void editInsertTextAtLast(HWND hwnd, const std::string &text,
			  size_t threshold)
{
  if (text.empty())
    return;
  
  size_t len = editGetTextBytes(hwnd);
  
  if (threshold < len)
  {
    Edit_SetSel(hwnd, 0, len / 3 * 2);
    Edit_ReplaceSel(hwnd, "");
    editDeleteLine(hwnd, 0);
    len = editGetTextBytes(hwnd);
  }
  
  Edit_SetSel(hwnd, len, len);
  
  // \n -> \r\n
  char *buf = (char *)_alloca(text.size() * 2 + 1);
  char *d = buf;
  const char *str = text.c_str();
  for (const char *s = str; s < str + text.size(); s++)
  {
    if (*s == '\n')
      *d++ = '\r';
    *d++ = *s;
  }
  *d = '\0';
  
  Edit_ReplaceSel(hwnd, buf);
}
