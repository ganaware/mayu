///////////////////////////////////////////////////////////////////////////////
// dlginvestigate.cpp


#include "misc.h"

#include "engine.h"
#include "focus.h"
#include "hook.h"
#include "mayurc.h"
#include "stringtool.h"
#include "target.h"
#include "windowstool.h"
#include "vkeytable.h"

#include <iomanip>


using namespace std;


class DlgInvestigate
{
  HWND hwnd;
  UINT WM_Targetted;
  Engine *engine;
  
public:
  DlgInvestigate(HWND hwnd_)
    : hwnd(hwnd_),
      WM_Targetted(RegisterWindowMessage(WM_Targetted_name)),
      engine(NULL)
  {
  }
  
  // WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM lParam)
  {
    engine = (Engine *)lParam;
    setSmallIcon(hwnd, IDI_ICON_mayu);
    setBigIcon(hwnd, IDI_ICON_mayu);
    return TRUE;
  }
  
  // WM_DESTROY
  BOOL wmDestroy()
  {
    unsetSmallIcon(hwnd);
    unsetBigIcon(hwnd);
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
    }
    return FALSE;
  }

  // WM_focusNotify
  BOOL wmFocusNotify(bool isFocused, HWND hwndFocus)
  {
    if (engine && hwndFocus == GetDlgItem(hwnd, IDC_CUSTOM_scancode))
      engine->enableLogMode(isFocused);
    return TRUE;
  }
  
  // WM_targetNotify
  BOOL wmTargetNotify(HWND hwndTarget)
  {
    char className[GANA_MAX_ATOM_LENGTH];
    bool ok = false;
    if (GetClassName(hwndTarget, className, sizeof(className)))
    {
      if (StringTool::mbsiequal_(className, "ConsoleWindowClass"))
      {
	char titleName[1024];
	if (GetWindowText(hwndTarget, titleName, sizeof(titleName)) == 0)
	  titleName[0] = '\0';
	{
	  Acquire a(&engine->log, 1);
	  engine->log << "HWND:\t" << hex << (int)hwndTarget << dec << endl;
	}
	Acquire a(&engine->log, 0);
	engine->log << "CLASS:\t" << className << endl;
	engine->log << "TITLE:\t" << titleName << endl;
	ok = true;
      }
    }
    if (!ok)
      _true( PostMessage(hwndTarget, WM_Targetted, 0, 0) );
    return TRUE;
  }
  
  // WM_vkeyNotify
  BOOL wmVkeyNotify(int nVirtKey, int /* repeatCount*/, BYTE /*scanCode*/,
		    bool isExtended, bool /*isAltDown*/, bool isKeyup)
  {
    Acquire a(&engine->log, 0);
    engine->log << (isExtended ? " E-" : "   ")
		<< "0x" << hex << setw(2) << setfill('0') << nVirtKey << dec
		<< "  &VK( "
		<< (isExtended ? "E-" : "  ")
		<< (isKeyup ? "U-" : "D-");
    
    for (const VKeyTable *vkt = vkeyTable; vkt->name; vkt ++)
    {
      if (vkt->code == nVirtKey)
      {
	engine->log << vkt->name << " )" << endl;
	return TRUE;
      }
    }
    engine->log << "0x" << hex << setw(2) << setfill('0') << nVirtKey << dec
		<< " )" << endl;
    return TRUE;
  }
};


BOOL CALLBACK dlgInvestigate_dlgProc(HWND hwnd, UINT message,
				     WPARAM wParam, LPARAM lParam)
{
  DlgInvestigate *wc;
  getUserData(hwnd, &wc);
  if (!wc)
    switch (message)
    {
      case WM_INITDIALOG:
	wc = setUserData(hwnd, new DlgInvestigate(hwnd));
	return wc->wmInitDialog((HWND)wParam, lParam);
    }
  else
    switch (message)
    {
      case WM_COMMAND:
	return wc->wmCommand(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
      case WM_CLOSE:
	return wc->wmClose();
      case WM_DESTROY:
	return wc->wmDestroy();
      case WM_focusNotify:
	return wc->wmFocusNotify(!!wParam, (HWND)lParam);
      case WM_targetNotify:
	return wc->wmTargetNotify((HWND)lParam);
      case WM_vkeyNotify:
	return wc->wmVkeyNotify((int)wParam, (int)(lParam & 0xffff),
				(BYTE)((lParam >> 16) & 0xff),
				!!(lParam & (1 << 24)),
				!!(lParam & (1 << 29)),
				!!(lParam & (1 << 31)));
      case WM_NCDESTROY:
	delete wc;
	return TRUE;
    }
  return FALSE;
}
