///////////////////////////////////////////////////////////////////////////////
// dlgsetting.cpp


#include "misc.h"

#include "mayu.h"
#include "mayurc.h"
#include "registry.h"
#include "stringtool.h"
#include "windowstool.h"
#include "setting.h"
#include "dlgeditsetting.h"
#include "regexp.h"

#include <commctrl.h>


using namespace std;


class DlgSetting
{
  HWND hwnd;
  HWND hwndMayuPaths;
  Registry reg;
  enum { maxMayuPaths = 256 };

  typedef DlgEditSettingData Data;

  void insertItem(int index, const Data &data)
  {
    LVITEM item;
    item.mask = LVIF_TEXT;
    item.iItem = index;
    
    item.iSubItem = 0;
    item.pszText = (char *)data.name.c_str();
    _true( ListView_InsertItem(hwndMayuPaths, &item) );

    ListView_SetItemText(hwndMayuPaths, index,1,(char *)data.filename.c_str());
    ListView_SetItemText(hwndMayuPaths, index,2,(char *)data.symbols.c_str());
  }
  
  void setItem(int index, const Data &data)
  {
    ListView_SetItemText(hwndMayuPaths, index,0,(char *)data.name.c_str());
    ListView_SetItemText(hwndMayuPaths, index,1,(char *)data.filename.c_str());
    ListView_SetItemText(hwndMayuPaths, index,2,(char *)data.symbols.c_str());
  }

  void getItem(int index, Data *data_r)
  {
    char buf[GANA_MAX_PATH];
    LVITEM item;
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.pszText = buf;
    item.cchTextMax = sizeof(buf);
    
    item.iSubItem = 0;
    _true( ListView_GetItem(hwndMayuPaths, &item) );
    data_r->name = item.pszText;
    
    item.iSubItem = 1;
    _true( ListView_GetItem(hwndMayuPaths, &item) );
    data_r->filename = item.pszText;

    item.iSubItem = 2;
    _true( ListView_GetItem(hwndMayuPaths, &item) );
    data_r->symbols = item.pszText;
  }

  void setSelectedItem(int index)
  {
    ListView_SetItemState(hwndMayuPaths, index, LVIS_SELECTED, LVIS_SELECTED);
  }

  int getSelectedItem()
  {
    if (ListView_GetSelectedCount(hwndMayuPaths) == 0)
      return -1;
    for (int i = 0; ; i ++)
    {
      if (ListView_GetItemState(hwndMayuPaths, i, LVIS_SELECTED))
	return i;
    }
  }

public:
  DlgSetting(HWND hwnd_)
    : hwnd(hwnd_),
      hwndMayuPaths(NULL),
      reg(MAYU_REGISTRY_ROOT)
  {
  }
  
  // WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM /* lParam */)
  {
    setSmallIcon(hwnd, IDI_ICON_mayu);
    setBigIcon(hwnd, IDI_ICON_mayu);
    
    _true( hwndMayuPaths = GetDlgItem(hwnd, IDC_LIST_mayuPaths) );

    // create list view colmn
    RECT rc;
    GetClientRect(hwndMayuPaths, &rc);
    
    LVCOLUMN lvc; 
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT; 
    lvc.fmt = LVCFMT_LEFT; 
    lvc.cx = (rc.right - rc.left) / 3;

    istring str = loadString(IDS_mayuPathName);
    lvc.pszText = (char *)str.c_str();
    _must_be( ListView_InsertColumn(hwndMayuPaths, 0, &lvc), ==, 0 );
    str = loadString(IDS_mayuPath);
    lvc.pszText = (char *)str.c_str();
    _must_be( ListView_InsertColumn(hwndMayuPaths, 1, &lvc), ==, 1 );
    str = loadString(IDS_mayuSymbols);
    lvc.pszText = (char *)str.c_str();
    _must_be( ListView_InsertColumn(hwndMayuPaths, 2, &lvc), ==, 2 );

    Data data;
    insertItem(0, data);				// TODO: why ?
    
    // set list view
    Regexp split("^([^;]*);([^;]*);(.*)$");
    StringTool::istring dot_mayu;
    int i;
    for (i = 0; i < maxMayuPaths; i ++)
    {
      char buf[100];
      snprintf(buf, sizeof(buf), ".mayu%d", i);
      if (!reg.read(buf, &dot_mayu))
	break;
      if (split.doesMatch(dot_mayu))
      {
	data.name = split[1];
	data.filename = split[2];
	data.symbols = split[3];
	insertItem(i, data);
      }
    }

    _true( ListView_DeleteItem(hwndMayuPaths, i) );	// TODO: why ?

    // arrange list view size
    ListView_SetColumnWidth(hwndMayuPaths, 0, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(hwndMayuPaths, 1, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(hwndMayuPaths, 2, LVSCW_AUTOSIZE);

    ListView_SetExtendedListViewStyle(hwndMayuPaths, LVS_EX_FULLROWSELECT);

    // set selection
    int index;
    reg.read(".mayuIndex", &index, 0);
    setSelectedItem(index);

    return TRUE;
  }
  
  // WM_CLOSE
  BOOL wmClose()
  {
    EndDialog(hwnd, 0);
    return TRUE;
  }
  
  // WM_COMMAND
  BOOL wmCommand(int /* notify_code */, int id, HWND /* hwnd_control */)
  {
    char buf[GANA_MAX_PATH];
    switch (id)
    {
      case IDC_BUTTON_up:
      case IDC_BUTTON_down:
      {
	int count = ListView_GetItemCount(hwndMayuPaths);
	if (count < 2)
	  return TRUE;
	int index = getSelectedItem();
	if (index < 0 ||
	    (id == IDC_BUTTON_up && index == 0) ||
	    (id == IDC_BUTTON_down && index == count - 1))
	  return TRUE;

	int target = (id == IDC_BUTTON_up) ? index - 1 : index + 1;

	Data dataIndex, dataTarget;
	getItem(index, &dataIndex);
	getItem(target, &dataTarget);
	setItem(index, dataTarget);
	setItem(target, dataIndex);
	
	setSelectedItem(target);
	return TRUE;
      }
      
      case IDC_BUTTON_add:
      {
	Data data;
	int index = getSelectedItem();
	if (0 <= index)
	  getItem(index, &data);
	if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_editSetting),
			   NULL, dlgEditSetting_dlgProc, (LPARAM)&data))
	  if (!data.filename.empty())
	  {
	    insertItem(0, data);
	    setSelectedItem(0);
	  }
	return TRUE;
      }
      
      case IDC_BUTTON_delete:
      {
	int index = getSelectedItem();
	if (0 <= index)
	{
	  _true( ListView_DeleteItem(hwndMayuPaths, index) );
	  int count = ListView_GetItemCount(hwndMayuPaths);
	  if (count == 0)
	    ;
	  else if (count == index)
	    setSelectedItem(index - 1);
	  else
	    setSelectedItem(index);
	}
	return TRUE;
      }
      
      case IDC_BUTTON_edit:
      {
	Data data;
	int index = getSelectedItem();
	if (index < 0)
	  return TRUE;
	getItem(index, &data);
	if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_editSetting),
			   NULL, dlgEditSetting_dlgProc, (LPARAM)&data))
	  if (!data.filename.empty())
	  {
	    setItem(index, data);
	    setSelectedItem(index);
	  }
	return TRUE;
      }
      
      case IDOK:
      {
	int count = ListView_GetItemCount(hwndMayuPaths);
	int index;
	for (index = 0; index < count; index ++)
	{
	  snprintf(buf, sizeof(buf), ".mayu%d", index);
	  Data data;
	  getItem(index, &data);
	  reg.write(buf, data.name + ";" + data.filename + ";" + data.symbols);
	}
	for (; ; index ++)
	{
	  snprintf(buf, sizeof(buf), ".mayu%d", index);
	  if (!reg.remove(buf))
	    break;
	}
	index = getSelectedItem();
	if (index < 0)
	  index = 0;
	reg.write(".mayuIndex", index);
	EndDialog(hwnd, 1);
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


BOOL CALLBACK dlgSetting_dlgProc(HWND hwnd, UINT message,
				 WPARAM wParam, LPARAM lParam)
{
  DlgSetting *wc;
  getUserData(hwnd, &wc);
  if (!wc)
    switch (message)
    {
      case WM_INITDIALOG:
	wc = setUserData(hwnd, new DlgSetting(hwnd));
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
