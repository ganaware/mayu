///////////////////////////////////////////////////////////////////////////////
// dlglog.cpp


#include "misc.h"

#include "mayu.h"
#include "mayurc.h"
#include "registry.h"
#include "windowstool.h"
#include "msgstream.h"

#include <windowsx.h>


class DlgLog
{
  HWND hwnd;
  HWND hwndEdit;
  LOGFONT lf;
  HFONT hfontOriginal;
  HFONT hfont;
  omsgstream *log;
  
public:
  DlgLog(HWND hwnd_)
    : hwnd(hwnd_),
      hwndEdit(GetDlgItem(hwnd, IDC_EDIT_log)),
      hfontOriginal(GetWindowFont(hwnd)),
      hfont(NULL)
  {
  }
  
  // WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM lParam)
  {
    log = (omsgstream *)lParam;
    
    // set icons
    setSmallIcon(hwnd, IDI_ICON_mayu);
    setBigIcon(hwnd, IDI_ICON_mayu);
    
    // set font
    Registry::read(MAYU_REGISTRY_ROOT, "logFont", &lf,
		   loadString(IDS_logFont));
    hfont = CreateFontIndirect(&lf);
    SetWindowFont(hwndEdit, hfont, false);
    
    // resize
    RECT rc;
    _true( GetClientRect(hwnd, &rc) );
    wmSize(0, (short)rc.right, (short)rc.bottom);

    // debug level
    bool isChecked =
      (IsDlgButtonChecked(hwnd, IDC_CHECK_detail) == BST_CHECKED);
    log->setDebugLevel(isChecked ? 1 : 0);

    return TRUE;
  }

  // WM_DESTROY
  BOOL wmDestroy()
  {
    // unset font
    SetWindowFont(hwndEdit, hfontOriginal, false);
    DeleteObject(hfont);
    
    // unset icons
    unsetBigIcon(hwnd);
    unsetSmallIcon(hwnd);
    return TRUE;
  }
  
  // WM_SIZE
  BOOL wmSize(DWORD /* fwSizeType */, short nWidth, short nHeight)
  {
    RECT rcLog, rcCl, rcCf, rcD, rcOK;
    HWND hwndCl = GetDlgItem(hwnd, IDC_BUTTON_clearLog);
    HWND hwndCf = GetDlgItem(hwnd, IDC_BUTTON_changeFont);
    HWND hwndD  = GetDlgItem(hwnd, IDC_CHECK_detail);
    HWND hwndOK = GetDlgItem(hwnd, IDOK);
    _true( getChildWindowRect(hwndEdit, &rcLog) );
    _true( getChildWindowRect(hwndCl, &rcCl) );
    _true( getChildWindowRect(hwndCf, &rcCf) );
    _true( getChildWindowRect(hwndD, &rcD) );
    _true( getChildWindowRect(hwndOK, &rcOK) );
    int d = rcCl.top - rcLog.bottom;
    int tail = d * 2 + rcHeight(&rcCl);
    MoveWindow(hwndEdit, 0, 0, nWidth, nHeight - tail, TRUE);
    MoveWindow(hwndCl, rcCl.left, nHeight - (tail + rcHeight(&rcCl)) / 2,
	       rcWidth(&rcCl), rcHeight(&rcCl), TRUE);
    MoveWindow(hwndCf, rcCf.left, nHeight - (tail + rcHeight(&rcCf)) / 2,
	       rcWidth(&rcCf), rcHeight(&rcCf), TRUE);
    MoveWindow(hwndD, rcD.left, nHeight - (tail + rcHeight(&rcD)) / 2,
	       rcWidth(&rcD), rcHeight(&rcD), TRUE);
    MoveWindow(hwndOK, rcOK.left, nHeight - (tail + rcHeight(&rcOK)) / 2,
	       rcWidth(&rcOK), rcHeight(&rcOK), TRUE);
    return TRUE;
  }
  
  // WM_CLOSE
  BOOL wmClose()
  {
    ShowWindow(hwnd, SW_HIDE);
    return TRUE;
  }

  // WM_COMMAND
  BOOL wmCommand(int /* notify_code */, int id, HWND /* hwnd_control */)
  {
    switch (id)
    {
      case IDOK:
      {
	ShowWindow(hwnd, SW_HIDE);
	return TRUE;
      }
      
      case IDC_BUTTON_clearLog:
      {
	Edit_SetSel(hwndEdit, 0, Edit_GetTextLength(hwndEdit));
	Edit_ReplaceSel(hwndEdit, "");
	return TRUE;
      }
      
      case IDC_BUTTON_changeFont:
      {
	CHOOSEFONT cf;
	memset(&cf, 0, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner = hwnd;
	cf.lpLogFont = &lf;
	cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
	if (ChooseFont(&cf))
	{
	  HFONT hfontNew = CreateFontIndirect(&lf);
	  SetWindowFont(hwnd, hfontNew, true);
	  DeleteObject(hfont);
	  hfont = hfontNew;
	  Registry::write(MAYU_REGISTRY_ROOT, "logFont", lf);
	}
	return TRUE;
      }
      
      case IDC_CHECK_detail:
      {
	bool isChecked =
	  (IsDlgButtonChecked(hwnd, IDC_CHECK_detail) == BST_CHECKED);
	log->setDebugLevel(isChecked ? 1 : 0);
	return TRUE;
      }
    }
    return FALSE;
  }
};


BOOL CALLBACK dlgLog_dlgProc(HWND hwnd, UINT message,
			     WPARAM wParam, LPARAM lParam)
{
  DlgLog *wc;
  getUserData(hwnd, &wc);
  if (!wc)
    switch (message)
    {
      case WM_INITDIALOG:
	wc = setUserData(hwnd, new DlgLog(hwnd));
	return wc->wmInitDialog((HWND)wParam, lParam);
    }
  else
    switch (message)
    {
      case WM_SIZE:
	return wc->wmSize(wParam, LOWORD(lParam), HIWORD(lParam));
      case WM_COMMAND:
	return wc->wmCommand(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
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
