// ////////////////////////////////////////////////////////////////////////////
// dlglog.cpp


#include "misc.h"

#include "mayu.h"
#include "mayurc.h"
#include "registry.h"
#include "windowstool.h"
#include "msgstream.h"

#include <windowsx.h>


///
class DlgLog
{
  HWND m_hwnd;					///
  HWND m_hwndEdit;				///
  LOGFONT m_lf;					///
  HFONT m_hfontOriginal;			///
  HFONT m_hfont;				///
  omsgstream *m_log;				///
  
public:
  ///
  DlgLog(HWND i_hwnd)
    : m_hwnd(i_hwnd),
      m_hwndEdit(GetDlgItem(m_hwnd, IDC_EDIT_log)),
      m_hfontOriginal(GetWindowFont(m_hwnd)),
      m_hfont(NULL)
  {
  }
  
  /// WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM i_lParam)
  {
    m_log = reinterpret_cast<omsgstream *>(i_lParam);
    
    // set icons
    setSmallIcon(m_hwnd, IDI_ICON_mayu);
    setBigIcon(m_hwnd, IDI_ICON_mayu);
    
    // set font
    Registry::read(MAYU_REGISTRY_ROOT, "logFont", &m_lf,
		   loadString(IDS_logFont));
    m_hfont = CreateFontIndirect(&m_lf);
    SetWindowFont(m_hwndEdit, m_hfont, false);
    
    // resize
    RECT rc;
    CHECK_TRUE( GetClientRect(m_hwnd, &rc) );
    wmSize(0, (short)rc.right, (short)rc.bottom);

    // debug level
    bool isChecked =
      (IsDlgButtonChecked(m_hwnd, IDC_CHECK_detail) == BST_CHECKED);
    m_log->setDebugLevel(isChecked ? 1 : 0);

    return TRUE;
  }

  /// WM_DESTROY
  BOOL wmDestroy()
  {
    // unset font
    SetWindowFont(m_hwndEdit, m_hfontOriginal, false);
    DeleteObject(m_hfont);
    
    // unset icons
    unsetBigIcon(m_hwnd);
    unsetSmallIcon(m_hwnd);
    return TRUE;
  }
  
  /// WM_SIZE
  BOOL wmSize(DWORD /* fwSizeType */, short i_nWidth, short i_nHeight)
  {
    RECT rcLog, rcCl, rcCf, rcD, rcOK;
    HWND hwndCl = GetDlgItem(m_hwnd, IDC_BUTTON_clearLog);
    HWND hwndCf = GetDlgItem(m_hwnd, IDC_BUTTON_changeFont);
    HWND hwndD  = GetDlgItem(m_hwnd, IDC_CHECK_detail);
    HWND hwndOK = GetDlgItem(m_hwnd, IDOK);
    CHECK_TRUE( getChildWindowRect(m_hwndEdit, &rcLog) );
    CHECK_TRUE( getChildWindowRect(hwndCl, &rcCl) );
    CHECK_TRUE( getChildWindowRect(hwndCf, &rcCf) );
    CHECK_TRUE( getChildWindowRect(hwndD, &rcD) );
    CHECK_TRUE( getChildWindowRect(hwndOK, &rcOK) );
    int d = rcCl.top - rcLog.bottom;
    int tail = d * 2 + rcHeight(&rcCl);
    MoveWindow(m_hwndEdit, 0, 0, i_nWidth, i_nHeight - tail, TRUE);
    MoveWindow(hwndCl, rcCl.left, i_nHeight - (tail + rcHeight(&rcCl)) / 2,
	       rcWidth(&rcCl), rcHeight(&rcCl), TRUE);
    MoveWindow(hwndCf, rcCf.left, i_nHeight - (tail + rcHeight(&rcCf)) / 2,
	       rcWidth(&rcCf), rcHeight(&rcCf), TRUE);
    MoveWindow(hwndD, rcD.left, i_nHeight - (tail + rcHeight(&rcD)) / 2,
	       rcWidth(&rcD), rcHeight(&rcD), TRUE);
    MoveWindow(hwndOK, rcOK.left, i_nHeight - (tail + rcHeight(&rcOK)) / 2,
	       rcWidth(&rcOK), rcHeight(&rcOK), TRUE);
    return TRUE;
  }
  
  /// WM_CLOSE
  BOOL wmClose()
  {
    ShowWindow(m_hwnd, SW_HIDE);
    return TRUE;
  }

  /// WM_COMMAND
  BOOL wmCommand(int /* notify_code */, int i_id, HWND /* m_hwnd_control */)
  {
    switch (i_id)
    {
      case IDOK:
      {
	ShowWindow(m_hwnd, SW_HIDE);
	return TRUE;
      }
      
      case IDC_BUTTON_clearLog:
      {
	Edit_SetSel(m_hwndEdit, 0, Edit_GetTextLength(m_hwndEdit));
	Edit_ReplaceSel(m_hwndEdit, "");
	return TRUE;
      }
      
      case IDC_BUTTON_changeFont:
      {
	CHOOSEFONT cf;
	memset(&cf, 0, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner = m_hwnd;
	cf.lpLogFont = &m_lf;
	cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
	if (ChooseFont(&cf))
	{
	  HFONT hfontNew = CreateFontIndirect(&m_lf);
	  SetWindowFont(m_hwnd, hfontNew, true);
	  DeleteObject(m_hfont);
	  m_hfont = hfontNew;
	  Registry::write(MAYU_REGISTRY_ROOT, "logFont", m_lf);
	}
	return TRUE;
      }
      
      case IDC_CHECK_detail:
      {
	bool isChecked =
	  (IsDlgButtonChecked(m_hwnd, IDC_CHECK_detail) == BST_CHECKED);
	m_log->setDebugLevel(isChecked ? 1 : 0);
	return TRUE;
      }
    }
    return FALSE;
  }
};


///
BOOL CALLBACK dlgLog_dlgProc(HWND i_hwnd, UINT i_message,
			     WPARAM i_wParam, LPARAM i_lParam)
{
  DlgLog *wc;
  getUserData(i_hwnd, &wc);
  if (!wc)
    switch (i_message)
    {
      case WM_INITDIALOG:
	wc = setUserData(i_hwnd, new DlgLog(i_hwnd));
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
      case WM_DESTROY:
	return wc->wmDestroy();
      case WM_NCDESTROY:
	delete wc;
	return TRUE;
    }
  return FALSE;
}
