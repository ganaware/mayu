///////////////////////////////////////////////////////////////////////////////
// dlgversion.cpp


#include "misc.h"

#include "mayu.h"
#include "mayurc.h"
#include "windowstool.h"

#include <cstdio>
#include <windowsx.h>


using namespace std;

class DlgVersion
{
  HWND hwnd;
  
public:
  DlgVersion(HWND hwnd_)
    : hwnd(hwnd_)
  {
  }
  
  // WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM /* lParam */)
  {
    setSmallIcon(hwnd, IDI_ICON_mayu);
    setBigIcon(hwnd, IDI_ICON_mayu);
    
    char buf[1024], buf2[1024];
    Edit_GetText(GetDlgItem(hwnd, IDC_STATIC_version), buf,
		 lengthof(buf));
    snprintf(buf2, lengthof(buf2), buf, VERSION);
    Edit_SetText(GetDlgItem(hwnd, IDC_STATIC_version), buf2);
    return TRUE;
  }
  
  // WM_CLOSE
  BOOL wmClose()
  {
    _true( EndDialog(hwnd, 0) );
    return TRUE;
  }

  // WM_COMMAND
  BOOL wmCommand(int /* notify_code */, int id, HWND /* hwnd_control */)
  {
    switch (id)
    {
      case IDOK:
      {
	_true( EndDialog(hwnd, 0) );
	return TRUE;
      }
      case IDC_BUTTON_download:
      {
	char buf[1024];
	Edit_GetText(GetDlgItem(hwnd, IDC_STATIC_url), buf, lengthof(buf));
	ShellExecute(NULL, "open", buf, NULL, NULL, SW_SHOWNORMAL);
	_true( EndDialog(hwnd, 0) );
	return TRUE;
      }
    }
    return FALSE;
  }
};


BOOL CALLBACK dlgVersion_dlgProc(HWND hwnd, UINT message,
				 WPARAM wParam, LPARAM lParam)
{
  DlgVersion *wc;
  getUserData(hwnd, &wc);
  if (!wc)
    switch (message)
    {
      case WM_INITDIALOG:
	wc = setUserData(hwnd, new DlgVersion(hwnd));
	return wc->wmInitDialog((HWND)wParam, lParam);
    }
  else
    switch (message)
    {
      case WM_COMMAND:
	return wc->wmCommand(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
      case WM_CLOSE:
	return wc->wmClose();
      case WM_NCDESTROY:
	delete wc;
	return TRUE;
    }
  return FALSE;
}
