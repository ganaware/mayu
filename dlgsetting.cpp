///////////////////////////////////////////////////////////////////////////////
// dlgsetting.cpp


#include "misc.h"

#include "mayu.h"
#include "mayurc.h"
#include "registry.h"
#include "stringtool.h"
#include "windowstool.h"
#include "setting.h"

#include <malloc.h>
#include <windowsx.h>


using namespace std;


class DlgSetting
{
  HWND hwnd;
  HWND hwndMayuPath;
  HWND hwndMayuPathName;
  HWND hwndMayuPaths;
  Registry reg;
  enum { maxMayuPaths = 256 };

  StringTool::istring ListBox_GetString(int index)
  {
    int len = ListBox_GetTextLen(hwndMayuPaths, index);
    char *text = (char *)_alloca(len + 1);
    ListBox_GetText(hwndMayuPaths, index, text);
    return StringTool::istring(text);
  }
  
public:
  DlgSetting(HWND hwnd_)
    : hwnd(hwnd_),
      hwndMayuPath(NULL),
      hwndMayuPathName(NULL),
      hwndMayuPaths(NULL),
      reg(MAYU_REGISTRY_ROOT)
  {
  }
  
  // WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM /* lParam */)
  {
    setSmallIcon(hwnd, IDI_ICON_mayu);
    setBigIcon(hwnd, IDI_ICON_mayu);
    
    _true( hwndMayuPath = GetDlgItem(hwnd, IDC_EDIT_mayuPath) );
    _true( hwndMayuPathName = GetDlgItem(hwnd, IDC_EDIT_mayuPathName) );
    _true( hwndMayuPaths = GetDlgItem(hwnd, IDC_LIST_mayuPaths) );
    
    StringTool::istring dot_mayu;
    for (int i = 0; i < maxMayuPaths; i ++)
    {
      char buf[100];
      snprintf(buf, sizeof(buf), ".mayu%d", i);
      if (!reg.read(buf, &dot_mayu))
	break;
      ListBox_AddString(hwndMayuPaths, dot_mayu.c_str());
    }
    
    int index;
    reg.read(".mayuIndex", &index, 0);
    ListBox_SetCurSel(hwndMayuPaths, index);
    
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
	int count = ListBox_GetCount(hwndMayuPaths);
	if (count < 2)
	  return TRUE;
	int index = ListBox_GetCurSel(hwndMayuPaths);
	if (index == LB_ERR ||
	    (id == IDC_BUTTON_up && index == 0) ||
	    (id == IDC_BUTTON_down && index == count - 1))
	  return TRUE;

	int target = (id == IDC_BUTTON_up) ? index - 1 : index + 1;

	StringTool::istring indexText(ListBox_GetString(index));
	StringTool::istring targetText(ListBox_GetString(target));
	
	ListBox_DeleteString(hwndMayuPaths, index);
	ListBox_InsertString(hwndMayuPaths, index, targetText.c_str());
	ListBox_DeleteString(hwndMayuPaths, target);
	ListBox_InsertString(hwndMayuPaths, target, indexText.c_str());
	ListBox_SetCurSel(hwndMayuPaths, target);
	return TRUE;
      }
      
      case IDC_BUTTON_add:
      {
	Edit_GetText(hwndMayuPathName, buf, lengthof(buf));
	for (char *p = buf; *p; p ++)
	{
	  if (*p == ';')
	    *p = ' ';
	  if (StringTool::ismbblead_(*p) && p[1])
	    p ++;
	}
	
	StringTool::istring name(buf);
	name += ';';
	
	Edit_GetText(hwndMayuPath, buf, lengthof(buf));
	name += buf;
	
	int index = ListBox_GetCurSel(hwndMayuPaths);
	if (index == LB_ERR)
	  index = 0;
	ListBox_InsertString(hwndMayuPaths, index, name.c_str());
	ListBox_SetCurSel(hwndMayuPaths, index);
	return TRUE;
      }
      
      case IDC_BUTTON_delete:
      case IDC_BUTTON_edit:
      {
	int index = ListBox_GetCurSel(hwndMayuPaths);
	if (index == LB_ERR)
	  return TRUE;

	StringTool::istring text(ListBox_GetString(index));
	if (id == IDC_BUTTON_delete)
	  ListBox_DeleteString(hwndMayuPaths, index);
	int count = ListBox_GetCount(hwndMayuPaths);
	if (0 < count)
	{
	  if (index == count)
	    index --;
	  ListBox_SetCurSel(hwndMayuPaths, index);
	}

	StringTool::istring name, filename;
	parseDotMayuFilename(text, &name, &filename);
	Edit_SetText(hwndMayuPathName, name.c_str());
	Edit_SetText(hwndMayuPath, filename.c_str());
	return TRUE;
      }
      
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
	int count = ListBox_GetCount(hwndMayuPaths);
	int index;
	for (index = 0; index < count; index ++)
	{
	  snprintf(buf, sizeof(buf), ".mayu%d", index);
	  reg.write(buf, ListBox_GetString(index).c_str());
	}
	for (; ; index ++)
	{
	  snprintf(buf, sizeof(buf), ".mayu%d", index);
	  if (!reg.remove(buf))
	    break;
	}
	index = ListBox_GetCurSel(hwndMayuPaths);
	if (index == LB_ERR)
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
