// ////////////////////////////////////////////////////////////////////////////
// dlgeditsetting.cpp


#include "misc.h"

#include "mayurc.h"
#include "windowstool.h"
#include "dlgeditsetting.h"

#include <windowsx.h>


using namespace std;


///
class DlgEditSetting
{
  HWND m_hwnd;					///
  HWND m_hwndMayuPathName;			///
  HWND m_hwndMayuPath;				///
  HWND m_hwndSymbols;				///

  DlgEditSettingData *m_data;			///

public:
  ///
  DlgEditSetting(HWND i_hwnd)
    : m_hwnd(i_hwnd),
      m_hwndMayuPathName(NULL),
      m_hwndMayuPath(NULL),
      m_hwndSymbols(NULL),
      m_data(NULL)
  {
  }
  
  /// WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM i_lParam)
  {
    m_data = reinterpret_cast<DlgEditSettingData *>(i_lParam);
    
    setSmallIcon(m_hwnd, IDI_ICON_mayu);
    setBigIcon(m_hwnd, IDI_ICON_mayu);
    
    CHECK_TRUE( m_hwndMayuPathName
		= GetDlgItem(m_hwnd, IDC_EDIT_mayuPathName) );
    CHECK_TRUE( m_hwndMayuPath = GetDlgItem(m_hwnd, IDC_EDIT_mayuPath) );
    CHECK_TRUE( m_hwndSymbols = GetDlgItem(m_hwnd, IDC_EDIT_symbols) );
    
    Edit_SetText(m_hwndMayuPathName, m_data->m_name.c_str());
    Edit_SetText(m_hwndMayuPath, m_data->m_filename.c_str());
    Edit_SetText(m_hwndSymbols, m_data->m_symbols.c_str());
    
    return TRUE;
  }
  
  /// WM_CLOSE
  BOOL wmClose()
  {
    CHECK_TRUE( EndDialog(m_hwnd, 0) );
    return TRUE;
  }
  
  /// WM_COMMAND
  BOOL wmCommand(int /* i_notify_code */, int i_id, HWND /* i_hwnd_control */)
  {
    char buf[GANA_MAX_PATH];
    switch (i_id)
    {
      case IDC_BUTTON_browse:
      {
	string title = loadString(IDS_openMayu);
	string filter = loadString(IDS_openMayuFilter);
	for (size_t i = 0; i < filter.size(); ++ i)
	  if (filter[i] == '|')
	    filter[i] = '\0';

	strcpy(buf, ".mayu");
	OPENFILENAME of;
	memset(&of, 0, sizeof(of));
	of.lStructSize = sizeof(of);
	of.hwndOwner = m_hwnd;
	of.lpstrFilter = filter.c_str();
	of.nFilterIndex = 1;
	of.lpstrFile = buf;
	of.nMaxFile = NUMBER_OF(buf);
	of.lpstrTitle = title.c_str();
	of.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST |
	  OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	if (GetOpenFileName(&of))
	  Edit_SetText(m_hwndMayuPath, buf);
	return TRUE;
      }
      
      case IDOK:
      {
	Edit_GetText(m_hwndMayuPathName, buf, NUMBER_OF(buf));
	m_data->m_name = buf;
	Edit_GetText(m_hwndMayuPath, buf, NUMBER_OF(buf));
	m_data->m_filename = buf;
	Edit_GetText(m_hwndSymbols, buf, NUMBER_OF(buf));
	m_data->m_symbols = buf;
	CHECK_TRUE( EndDialog(m_hwnd, 1) );
	return TRUE;
      }
      
      case IDCANCEL:
      {
	CHECK_TRUE( EndDialog(m_hwnd, 0) );
	return TRUE;
      }
    }
    return FALSE;
  }
};


BOOL CALLBACK dlgEditSetting_dlgProc(HWND i_hwnd, UINT i_message,
				     WPARAM i_wParam, LPARAM i_lParam)
{
  DlgEditSetting *wc;
  getUserData(i_hwnd, &wc);
  if (!wc)
    switch (i_message)
    {
      case WM_INITDIALOG:
	wc = setUserData(i_hwnd, new DlgEditSetting(i_hwnd));
	return wc->wmInitDialog(
	  reinterpret_cast<HWND>(i_wParam), i_lParam);
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
