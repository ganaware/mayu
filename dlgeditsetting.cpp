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
  HWND hwnd;			///
  HWND hwndMayuPathName;	///
  HWND hwndMayuPath;		///
  HWND hwndSymbols;		///

  DlgEditSettingData *data;	///

public:
  ///
  DlgEditSetting(HWND hwnd_)
    : hwnd(hwnd_),
      hwndMayuPathName(NULL),
      hwndMayuPath(NULL),
      hwndSymbols(NULL),
      data(NULL)
  {
  }
  
  /// WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM lParam)
  {
    data = (DlgEditSettingData *)lParam;
    
    setSmallIcon(hwnd, IDI_ICON_mayu);
    setBigIcon(hwnd, IDI_ICON_mayu);
    
    _true( hwndMayuPathName = GetDlgItem(hwnd, IDC_EDIT_mayuPathName) );
    _true( hwndMayuPath = GetDlgItem(hwnd, IDC_EDIT_mayuPath) );
    _true( hwndSymbols = GetDlgItem(hwnd, IDC_EDIT_symbols) );
    
    Edit_SetText(hwndMayuPathName, data->name.c_str());
    Edit_SetText(hwndMayuPath, data->filename.c_str());
    Edit_SetText(hwndSymbols, data->symbols.c_str());
    
    return TRUE;
  }
  
  /// WM_CLOSE
  BOOL wmClose()
  {
    _true( EndDialog(hwnd, 0) );
    return TRUE;
  }
  
  /// WM_COMMAND
  BOOL wmCommand(int /* notify_code */, int id, HWND /* hwnd_control */)
  {
    char buf[GANA_MAX_PATH];
    switch (id)
    {
      case IDC_BUTTON_browse:
      {
	string title = loadString(IDS_openMayu);
	string filter = loadString(IDS_openMayuFilter);
	for (size_t i = 0; i < filter.size(); i++)
	  if (filter[i] == '|')
	    filter[i] = '\0';

	strcpy(buf, ".mayu");
	OPENFILENAME of;
	memset(&of, 0, sizeof(of));
	of.lStructSize = sizeof(of);
	of.hwndOwner = hwnd;
	of.lpstrFilter = filter.c_str();
	of.nFilterIndex = 1;
	of.lpstrFile = buf;
	of.nMaxFile = lengthof(buf);
	of.lpstrTitle = title.c_str();
	of.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST |
	  OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	if (GetOpenFileName(&of))
	  Edit_SetText(hwndMayuPath, buf);
	return TRUE;
      }
      
      case IDOK:
      {
	Edit_GetText(hwndMayuPathName, buf, lengthof(buf));
	data->name = buf;
	Edit_GetText(hwndMayuPath, buf, lengthof(buf));
	data->filename = buf;
	Edit_GetText(hwndSymbols, buf, lengthof(buf));
	data->symbols = buf;
	_true( EndDialog(hwnd, 1) );
	return TRUE;
      }
      
      case IDCANCEL:
      {
	_true( EndDialog(hwnd, 0) );
	return TRUE;
      }
    }
    return FALSE;
  }
};


BOOL CALLBACK dlgEditSetting_dlgProc(HWND hwnd, UINT message,
				     WPARAM wParam, LPARAM lParam)
{
  DlgEditSetting *wc;
  getUserData(hwnd, &wc);
  if (!wc)
    switch (message)
    {
      case WM_INITDIALOG:
	wc = setUserData(hwnd, new DlgEditSetting(hwnd));
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
