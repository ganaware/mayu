// ////////////////////////////////////////////////////////////////////////////
// dlgversion.cpp


#include "misc.h"

#include "mayu.h"
#include "mayurc.h"
#include "windowstool.h"

#include <cstdio>
#include <windowsx.h>


using namespace std;

///
class DlgVersion
{
  HWND m_hwnd;		///
  
public:
  ///
  DlgVersion(HWND i_hwnd)
    : m_hwnd(i_hwnd)
  {
  }
  
  /// WM_INITDIALOG
  BOOL wmInitDialog(HWND /* i_focus */, LPARAM /* i_lParam */)
  {
    setSmallIcon(m_hwnd, IDI_ICON_mayu);
    setBigIcon(m_hwnd, IDI_ICON_mayu);
    
    char buf[1024], buf2[1024];
    Edit_GetText(GetDlgItem(m_hwnd, IDC_STATIC_version), buf,
		 NUMBER_OF(buf));
    snprintf(buf2, NUMBER_OF(buf2), buf, VERSION);
    Edit_SetText(GetDlgItem(m_hwnd, IDC_STATIC_version), buf2);
    return TRUE;
  }
  
  /// WM_CLOSE
  BOOL wmClose()
  {
    CHECK_TRUE( EndDialog(m_hwnd, 0) );
    return TRUE;
  }

  /// WM_COMMAND
  BOOL wmCommand(int /* i_notifyCode */, int i_id, HWND /* i_hwndControl */)
  {
    switch (i_id)
    {
      case IDOK:
      {
	CHECK_TRUE( EndDialog(m_hwnd, 0) );
	return TRUE;
      }
      case IDC_BUTTON_download:
      {
	char buf[1024];
	Edit_GetText(GetDlgItem(m_hwnd, IDC_STATIC_url), buf, NUMBER_OF(buf));
	ShellExecute(NULL, "open", buf, NULL, NULL, SW_SHOWNORMAL);
	CHECK_TRUE( EndDialog(m_hwnd, 0) );
	return TRUE;
      }
    }
    return FALSE;
  }
};


///
BOOL CALLBACK dlgVersion_dlgProc(
  HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
  DlgVersion *wc;
  getUserData(i_hwnd, &wc);
  if (!wc)
    switch (i_message)
    {
      case WM_INITDIALOG:
	wc = setUserData(i_hwnd, new DlgVersion(i_hwnd));
	return wc->wmInitDialog(reinterpret_cast<HWND>(i_wParam), i_lParam);
    }
  else
    switch (i_message)
    {
      case WM_COMMAND:
	return wc->wmCommand(HIWORD(i_wParam), LOWORD(i_wParam),
			     reinterpret_cast<HWND>(i_lParam));
      case WM_CLOSE:
	return wc->wmClose();
      case WM_NCDESTROY:
	delete wc;
	return TRUE;
    }
  return FALSE;
}
