// ////////////////////////////////////////////////////////////////////////////
// dlgsetting.cpp


#include "misc.h"

#include "mayu.h"
#include "mayurc.h"
#include "registry.h"
#include "stringtool.h"
#include "windowstool.h"
#include "setting.h"
#include "dlgeditsetting.h"

#include <commctrl.h>
#include <windowsx.h>


class LayoutManager
{
public:
  enum Origin
  {
    ORIGIN_LEFT_EDGE,
    ORIGIN_TOP_EDGE = ORIGIN_LEFT_EDGE,
    ORIGIN_CENTER,
    ORIGIN_RIGHT_EDGE,
    ORIGIN_BOTTOM_EDGE = ORIGIN_RIGHT_EDGE,
  };

private:
  class Item
  {
  public:
    HWND m_hwnd;
    HWND m_hwndParent;
    RECT m_rc;
    RECT m_rcParent;
    Origin m_origin[4];
  };

  typedef std::list<Item> Items;
  Items m_items;
  
public:
  bool addItem(HWND i_hwnd, Origin i_originLeft, Origin i_originTop,
	       Origin i_originBottom, Origin i_originRight)
  {
    Item item;
    if (!i_hwnd)
      return false;
    item.m_hwnd = i_hwnd;
    if (!(GetWindowLong(i_hwnd, GWL_STYLE) & WS_CHILD))
      return false;
    item.m_hwndParent = GetParent(i_hwnd);
    if (!item.m_hwndParent)
      return false;
    getChildWindowRect(item.m_hwnd, &item.m_rc);
    GetWindowRect(item.m_hwndParent, &item.m_rcParent);
    item.m_origin[0] = i_originLeft;
    item.m_origin[1] = i_originTop;
    item.m_origin[2] = i_originBottom;
    item.m_origin[3] = i_originRight;
    
    m_items.push_back(item);
    return true;
  }
  
  void adjust() const
  {
    for (Items::const_iterator i = m_items.begin(); i != m_items.end(); ++ i)
    {
      RECT rc;
      GetWindowRect(i->m_hwndParent, &rc);

      struct { int m_width, m_pos; int m_curWidth; LONG *m_out; }
      pos[4] =
      {
	{ rcWidth(&i->m_rcParent), i->m_rc.left, rcWidth(&rc), &rc.left },
	{ rcHeight(&i->m_rcParent), i->m_rc.top, rcHeight(&rc), &rc.top },
	{ rcWidth(&i->m_rcParent), i->m_rc.right, rcWidth(&rc), &rc.right },
	{ rcHeight(&i->m_rcParent), i->m_rc.bottom, rcHeight(&rc), &rc.bottom }
      };
      for (int j = 0; j < 4; ++ j)
      {
	switch (i->m_origin[j])
	{
	  case ORIGIN_LEFT_EDGE:
	    *pos[j].m_out = pos[j].m_pos;
	    break;
	  case ORIGIN_CENTER:
	    *pos[j].m_out = pos[j].m_curWidth / 2
	      - (pos[j].m_width / 2 - pos[j].m_pos);
	    break;
	  case ORIGIN_RIGHT_EDGE:
	    *pos[j].m_out = pos[j].m_curWidth
	      - (pos[j].m_width - pos[j].m_pos);
	    break;
	}
      }
      MoveWindow(i->m_hwnd, rc.left, rc.top,
		 rcWidth(&rc), rcHeight(&rc), FALSE);
    }
  }
};


///
class DlgSetting
{
  HWND m_hwnd;					///
  HWND m_hwndMayuPaths;				///
  LayoutManager m_layoutManager;		/// 
  
  ///
  Registry m_reg;
  /** @name ANONYMOUS */
  enum
  {
    MAX_MAYU_PATHS = 256,			///
  };

  typedef DlgEditSettingData Data;		///

  ///
  void insertItem(int i_index, const Data &i_data)
  {
    LVITEM item;
    item.mask = LVIF_TEXT;
    item.iItem = i_index;
    
    item.iSubItem = 0;
    item.pszText = const_cast<_TCHAR *>(i_data.m_name.c_str());
    CHECK_TRUE( ListView_InsertItem(m_hwndMayuPaths, &item) != -1 );

    ListView_SetItemText(m_hwndMayuPaths, i_index, 1,
			 const_cast<_TCHAR *>(i_data.m_filename.c_str()));
    ListView_SetItemText(m_hwndMayuPaths, i_index, 2,
			 const_cast<_TCHAR *>(i_data.m_symbols.c_str()));
  }
  
  ///
  void setItem(int i_index, const Data &i_data)
  {
    ListView_SetItemText(m_hwndMayuPaths, i_index, 0,
			 const_cast<_TCHAR *>(i_data.m_name.c_str()));
    ListView_SetItemText(m_hwndMayuPaths, i_index, 1,
			 const_cast<_TCHAR *>(i_data.m_filename.c_str()));
    ListView_SetItemText(m_hwndMayuPaths, i_index, 2,
			 const_cast<_TCHAR *>(i_data.m_symbols.c_str()));
  }

  ///
  void getItem(int i_index, Data *o_data)
  {
    _TCHAR buf[GANA_MAX_PATH];
    LVITEM item;
    item.mask = LVIF_TEXT;
    item.iItem = i_index;
    item.pszText = buf;
    item.cchTextMax = NUMBER_OF(buf);
    
    item.iSubItem = 0;
    CHECK_TRUE( ListView_GetItem(m_hwndMayuPaths, &item) );
    o_data->m_name = item.pszText;
    
    item.iSubItem = 1;
    CHECK_TRUE( ListView_GetItem(m_hwndMayuPaths, &item) );
    o_data->m_filename = item.pszText;

    item.iSubItem = 2;
    CHECK_TRUE( ListView_GetItem(m_hwndMayuPaths, &item) );
    o_data->m_symbols = item.pszText;
  }

  ///
  void setSelectedItem(int i_index)
  {
    ListView_SetItemState(m_hwndMayuPaths, i_index,
			  LVIS_SELECTED, LVIS_SELECTED);
  }

  ///
  int getSelectedItem()
  {
    if (ListView_GetSelectedCount(m_hwndMayuPaths) == 0)
      return -1;
    for (int i = 0; ; ++ i)
    {
      if (ListView_GetItemState(m_hwndMayuPaths, i, LVIS_SELECTED))
	return i;
    }
  }

public:
  ///
  DlgSetting(HWND i_hwnd)
    : m_hwnd(i_hwnd),
      m_hwndMayuPaths(NULL),
      m_reg(MAYU_REGISTRY_ROOT)
  {
  }
  
  /// WM_INITDIALOG
  BOOL wmInitDialog(HWND /* i_focus */, LPARAM /* i_lParam */)
  {
    setSmallIcon(m_hwnd, IDI_ICON_mayu);
    setBigIcon(m_hwnd, IDI_ICON_mayu);
    
    CHECK_TRUE( m_hwndMayuPaths = GetDlgItem(m_hwnd, IDC_LIST_mayuPaths) );

    // create list view colmn
    RECT rc;
    GetClientRect(m_hwndMayuPaths, &rc);
    
    LVCOLUMN lvc; 
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT; 
    lvc.fmt = LVCFMT_LEFT; 
    lvc.cx = (rc.right - rc.left) / 3;

    tstringi str = loadString(IDS_mayuPathName);
    lvc.pszText = const_cast<_TCHAR *>(str.c_str());
    CHECK( 0 ==, ListView_InsertColumn(m_hwndMayuPaths, 0, &lvc) );
    str = loadString(IDS_mayuPath);
    lvc.pszText = const_cast<_TCHAR *>(str.c_str());
    CHECK( 1 ==, ListView_InsertColumn(m_hwndMayuPaths, 1, &lvc) );
    str = loadString(IDS_mayuSymbols);
    lvc.pszText = const_cast<_TCHAR *>(str.c_str());
    CHECK( 2 ==, ListView_InsertColumn(m_hwndMayuPaths, 2, &lvc) );

    Data data;
    insertItem(0, data);				// TODO: why ?
    
    // set list view
    tregex split(_T("^([^;]*);([^;]*);(.*)$"));
    tstringi dot_mayu;
    int i;
    for (i = 0; i < MAX_MAYU_PATHS; ++ i)
    {
      _TCHAR buf[100];
      _sntprintf(buf, NUMBER_OF(buf), _T(".mayu%d"), i);
      if (!m_reg.read(buf, &dot_mayu))
	break;

      tcmatch_results what;
      if (boost::regex_match(dot_mayu, what, split))
      {
	data.m_name = what.str(1);
	data.m_filename = what.str(2);
	data.m_symbols = what.str(3);
	insertItem(i, data);
      }
    }

    CHECK_TRUE( ListView_DeleteItem(m_hwndMayuPaths, i) );	// TODO: why ?

    // arrange list view size
    ListView_SetColumnWidth(m_hwndMayuPaths, 0, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_hwndMayuPaths, 1, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_hwndMayuPaths, 2, LVSCW_AUTOSIZE);

    ListView_SetExtendedListViewStyle(m_hwndMayuPaths, LVS_EX_FULLROWSELECT);

    // set selection
    int index;
    m_reg.read(_T(".mayuIndex"), &index, 0);
    setSelectedItem(index);

    // set layout manager
    m_layoutManager.addItem(GetDlgItem(m_hwnd, IDC_STATIC_mayuPaths),
			    LayoutManager::ORIGIN_LEFT_EDGE,
			    LayoutManager::ORIGIN_TOP_EDGE,
			    LayoutManager::ORIGIN_RIGHT_EDGE,
			    LayoutManager::ORIGIN_BOTTOM_EDGE);
    m_layoutManager.addItem(GetDlgItem(m_hwnd, IDC_LIST_mayuPaths),
			    LayoutManager::ORIGIN_LEFT_EDGE,
			    LayoutManager::ORIGIN_TOP_EDGE,
			    LayoutManager::ORIGIN_RIGHT_EDGE,
			    LayoutManager::ORIGIN_BOTTOM_EDGE);
    m_layoutManager.addItem(GetDlgItem(m_hwnd, IDC_BUTTON_up),
			    LayoutManager::ORIGIN_RIGHT_EDGE,
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_RIGHT_EDGE,
			    LayoutManager::ORIGIN_CENTER);
    m_layoutManager.addItem(GetDlgItem(m_hwnd, IDC_BUTTON_down),
			    LayoutManager::ORIGIN_RIGHT_EDGE,
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_RIGHT_EDGE,
			    LayoutManager::ORIGIN_CENTER);
    m_layoutManager.addItem(GetDlgItem(m_hwnd, IDC_BUTTON_add),
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE,
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE);
    m_layoutManager.addItem(GetDlgItem(m_hwnd, IDC_BUTTON_edit),
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE,
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE);
    m_layoutManager.addItem(GetDlgItem(m_hwnd, IDC_BUTTON_delete),
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE,
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE);
    m_layoutManager.addItem(GetDlgItem(m_hwnd, IDCANCEL),
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE,
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE);
    m_layoutManager.addItem(GetDlgItem(m_hwnd, IDOK),
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE,
			    LayoutManager::ORIGIN_CENTER,
			    LayoutManager::ORIGIN_BOTTOM_EDGE);
    return TRUE;
  }

  /// WM_SIZE
  BOOL wmSize(DWORD /* i_fwSizeType */, short /* i_nWidth */,
	      short /* i_nHeight */)
  {
    m_layoutManager.adjust();
    RedrawWindow(m_hwnd, NULL, NULL,
		 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    return TRUE;
  }
  
  /// WM_CLOSE
  BOOL wmClose()
  {
    EndDialog(m_hwnd, 0);
    return TRUE;
  }

  /// WM_NOTIFY
  BOOL wmNotify(int i_id, NMHDR *i_nmh)
  {
    switch (i_id)
    {
      case IDC_LIST_mayuPaths:
	if (i_nmh->code == NM_DBLCLK)
	  FORWARD_WM_COMMAND(m_hwnd, IDC_BUTTON_edit, NULL, 0, SendMessage);
	return TRUE;
    }
    return TRUE;
  }
  
  /// WM_COMMAND
  BOOL wmCommand(int /* i_notifyCode */, int i_id, HWND /* i_hwndControl */)
  {
    _TCHAR buf[GANA_MAX_PATH];
    switch (i_id)
    {
      case IDC_BUTTON_up:
      case IDC_BUTTON_down:
      {
	int count = ListView_GetItemCount(m_hwndMayuPaths);
	if (count < 2)
	  return TRUE;
	int index = getSelectedItem();
	if (index < 0 ||
	    (i_id == IDC_BUTTON_up && index == 0) ||
	    (i_id == IDC_BUTTON_down && index == count - 1))
	  return TRUE;

	int target = (i_id == IDC_BUTTON_up) ? index - 1 : index + 1;

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
	if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_editSetting),
			   m_hwnd, dlgEditSetting_dlgProc, (LPARAM)&data))
	  if (!data.m_name.empty())
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
	  CHECK_TRUE( ListView_DeleteItem(m_hwndMayuPaths, index) );
	  int count = ListView_GetItemCount(m_hwndMayuPaths);
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
	if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_editSetting),
			   m_hwnd, dlgEditSetting_dlgProc, (LPARAM)&data))
	{
	  setItem(index, data);
	  setSelectedItem(index);
	}
	return TRUE;
      }
      
      case IDOK:
      {
	int count = ListView_GetItemCount(m_hwndMayuPaths);
	int index;
	for (index = 0; index < count; ++ index)
	{
	  _sntprintf(buf, NUMBER_OF(buf), _T(".mayu%d"), index);
	  Data data;
	  getItem(index, &data);
	  m_reg.write(buf, data.m_name + _T(";") +
		      data.m_filename + _T(";") + data.m_symbols);
	}
	for (; ; ++ index)
	{
	  _sntprintf(buf, NUMBER_OF(buf), _T(".mayu%d"), index);
	  if (!m_reg.remove(buf))
	    break;
	}
	index = getSelectedItem();
	if (index < 0)
	  index = 0;
	m_reg.write(_T(".mayuIndex"), index);
	EndDialog(m_hwnd, 1);
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


BOOL CALLBACK dlgSetting_dlgProc(
  HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
  DlgSetting *wc;
  getUserData(i_hwnd, &wc);
  if (!wc)
    switch (i_message)
    {
      case WM_INITDIALOG:
	wc = setUserData(i_hwnd, new DlgSetting(i_hwnd));
	return wc->wmInitDialog(reinterpret_cast<HWND>(i_wParam), i_lParam);
    }
  else
    switch (i_message)
    {
      case WM_SIZE:
	return wc->wmSize(i_wParam, LOWORD(i_lParam), HIWORD(i_lParam));
      case WM_COMMAND:
	return wc->wmCommand(HIWORD(i_wParam), LOWORD(i_wParam),
			     reinterpret_cast<HWND>(i_lParam));
      case WM_CLOSE:
	return wc->wmClose();
      case WM_NCDESTROY:
	delete wc;
	return TRUE;
      case WM_NOTIFY:
	return wc->wmNotify(static_cast<int>(i_wParam),
			    reinterpret_cast<NMHDR *>(i_lParam));
    }
  return FALSE;
}
