// ////////////////////////////////////////////////////////////////////////////
// mayu.cpp


#define APSTUDIO_INVOKED


#include "misc.h"

#include "dlginvestigate.h"
#include "dlglog.h"
#include "dlgsetting.h"
#include "dlgversion.h"
#include "engine.h"
#include "errormessage.h"
#include "focus.h"
#include "function.h"
#include "hook.h"
#include "mayu.h"
#include "mayurc.h"
#include "msgstream.h"
#include "multithread.h"
#include "registry.h"
#include "setting.h"
#include "target.h"
#include "windowstool.h"

#include <shlwapi.h>
#include <process.h>
#include <time.h>
#include <commctrl.h>


using namespace std;


///
#define ID_MENUITEM_reloadBegin _APS_NEXT_COMMAND_VALUE


// ////////////////////////////////////////////////////////////////////////////
// Mayu


///
class Mayu
{
  HWND hwndTaskTray;		/// tasktray window
  HWND hwndLog;			/// log dialog
  HWND hwndInvestigate;		/// investigate dialog
  HWND hwndVersion;		/// version dialog
  
  UINT WM_TaskbarRestart;	/// window message sent when taskber restarts
  NOTIFYICONDATA ni;		/// taskbar icon data
  bool m_canUseTasktrayBaloon;	/// 

  omsgstream log;		/// log stream (output to log dialog's edit)

  HANDLE mhEvent;		/// event for message handler thread

  HMENU hMenuTaskTray;		/// tasktray menu
  
  Setting *setting;		/// current setting
  bool isSettingDialogOpened;	/// is setting dialog opened ?
  
  Engine engine;		/// engine

  /** @name ANONYMOUS */
  enum
  { 
    WM_TASKTRAYNOTIFY = WM_APP + 101,	///
    WM_MSGSTREAMNOTIFY = WM_APP + 102,	///
    ID_TaskTrayIcon = 1,		///
  };

private:
  /// register class for tasktray
  ATOM Register_tasktray()
  {
    WNDCLASS wc;
    wc.style         = 0;
    wc.lpfnWndProc   = tasktray_wndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(Mayu *);
    wc.hInstance     = hInst;
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "mayuTasktray";
    return RegisterClass(&wc);
  }

  /// notify handler thread
  static void notifyHandler(void *This)
  {
    ((Mayu *)This)->notifyHandler();
  }
  ///
  void notifyHandler()
  {
    HANDLE hMailslot =
      CreateMailslot(mailslotNotify, notifyMessageSize,
		     MAILSLOT_WAIT_FOREVER, (SECURITY_ATTRIBUTES *)NULL);
    assert(hMailslot != INVALID_HANDLE_VALUE);

    // initialize ok
    CHECK_TRUE( SetEvent(mhEvent) );
    
    // loop
    while (true)
    {
      DWORD len = 0;
      BYTE buf[notifyMessageSize];
      
      // wait for message
      if (!ReadFile(hMailslot, buf, notifyMessageSize, &len,
		    (OVERLAPPED *)NULL))
	continue;

      switch (((Notify *)buf)->type)
      {
	case Notify::TypeMayuExit: // terminate thread
	{
	  CHECK_TRUE( CloseHandle(hMailslot) );
	  CHECK_TRUE( SetEvent(mhEvent) );
	  return;
	}
	
	case Notify::TypeSetFocus:
	case Notify::TypeName:
	{
	  NotifySetFocus *n = (NotifySetFocus *)buf;
	  n->className[NUMBER_OF(n->className) - 1] = '\0';
	  n->titleName[NUMBER_OF(n->titleName) - 1] = '\0';
	  
	  if (n->type == Notify::TypeSetFocus)
	    engine.setFocus(n->hwnd, n->threadId,
			    n->className, n->titleName, false);

	  {
	    Acquire a(&log, 1);
	    log << "HWND:\t" << hex << (int)n->hwnd << dec << endl;
	    log << "THREADID:" << (int)n->threadId << endl;
	  }
	  Acquire a(&log, (n->type == Notify::TypeName) ? 0 : 1);
	  log << "CLASS:\t" << n->className << endl;
	  log << "TITLE:\t" << n->titleName << endl;
	  
	  bool isMDI = true;
	  HWND hwnd = getToplevelWindow(n->hwnd, &isMDI);
	  RECT rc;
	  if (isMDI)
	  {
	    getChildWindowRect(hwnd, &rc);
	    log << "MDI Window Position/Size: ("
		<< rc.left << ", "<< rc.top << ") / ("
		<< rcWidth(&rc) << "x" << rcHeight(&rc) << ")" << endl;
	    hwnd = getToplevelWindow(n->hwnd, NULL);
	  }
	  
	  GetWindowRect(hwnd, &rc);
	  log << "Toplevel Window Position/Size: ("
	      << rc.left << ", "<< rc.top << ") / ("
	      << rcWidth(&rc) << "x" << rcHeight(&rc) << ")" << endl;

	  SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rc, FALSE);
	  log << "Desktop Window Position/Size: ("
	      << rc.left << ", "<< rc.top << ") / ("
	      << rcWidth(&rc) << "x" << rcHeight(&rc) << ")" << endl;
	  
	  log << endl;
	  break;
	}
	
	case Notify::TypeLockState:
	{
	  NotifyLockState *n = (NotifyLockState *)buf;
	  engine.setLockState(n->isNumLockToggled,
			      n->isCapsLockToggled,
			      n->isScrollLockToggled,
			      n->isImeLockToggled,
			      n->isImeCompToggled);
	  break;
	}

	case Notify::TypeSync:
	{
	  engine.syncNotify();
	  break;
	}

	case Notify::TypeThreadDetach:
	{
	  NotifyThreadDetach *n = (NotifyThreadDetach *)buf;
	  engine.threadDetachNotify(n->threadId);
	  break;
	}

	case Notify::TypeCommand:
	{
	  NotifyCommand *n = (NotifyCommand *)buf;
	  engine.commandNotify(n->hwnd, n->message, n->wParam, n->lParam);
	  break;
	}
      }
    }
  }
  
  /// window procedure for tasktray
  static LRESULT CALLBACK
  tasktray_wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    Mayu *This = (Mayu *)GetWindowLong(hwnd, 0);

    if (!This)
      switch (message)
      {
	case WM_CREATE:
	  This = (Mayu *)((CREATESTRUCT *)lParam)->lpCreateParams;
	  SetWindowLong(hwnd, 0, (long)This);
	  return 0;
      }
    else
      switch (message)
      {
	case WM_MSGSTREAMNOTIFY:
	{
	  omsgbuf *log = (omsgbuf *)lParam;
	  const string &str = log->acquireString();
	  editInsertTextAtLast(GetDlgItem(This->hwndLog, IDC_EDIT_log),
			       str, 65000);
	  log->releaseString();
	  return 0;
	}
	
	case WM_TASKTRAYNOTIFY:
	{
	  if (wParam == ID_TaskTrayIcon)
	    switch (lParam)
	    {
	      case WM_RBUTTONUP:
	      {
		POINT p;
		CHECK_TRUE( GetCursorPos(&p) );
		SetForegroundWindow(hwnd);
		HMENU hMenuSub = GetSubMenu(This->hMenuTaskTray, 0);
		if (This->engine.getIsEnabled())
		  CheckMenuItem(hMenuSub, ID_MENUITEM_disable,
				MF_UNCHECKED | MF_BYCOMMAND);
		else
		  CheckMenuItem(hMenuSub, ID_MENUITEM_disable,
				MF_CHECKED | MF_BYCOMMAND);
		CHECK_TRUE( SetMenuDefaultItem(hMenuSub,
					  ID_MENUITEM_investigate, FALSE) );

		// create reload menu
		HMENU hMenuSubSub = GetSubMenu(hMenuSub, 1);
		Registry reg(MAYU_REGISTRY_ROOT);
		int mayuIndex;
		reg.read(".mayuIndex", &mayuIndex, 0);
		while (DeleteMenu(hMenuSubSub, 0, MF_BYPOSITION))
		  ;
		Regexp getName("^([^;]*);");
		for (int index = 0; ; index ++)
		{
		  char buf[100];
		  snprintf(buf, sizeof(buf), ".mayu%d", index);
		  StringTool::istring dot_mayu;
		  if (!reg.read(buf, &dot_mayu))
		    break;
		  if (getName.doesMatch(dot_mayu))
		  {
		    MENUITEMINFO mii;
		    memset(&mii, 0, sizeof(mii));
		    mii.cbSize = sizeof(mii);
		    mii.fMask = MIIM_ID | MIIM_STATE | MIIM_TYPE;
		    mii.fType = MFT_STRING;
		    mii.fState =
		      MFS_ENABLED | ((mayuIndex == index) ? MFS_CHECKED : 0);
		    mii.wID = ID_MENUITEM_reloadBegin + index;
		    StringTool::istring name = getName[1];
		    mii.dwTypeData = (char *)name.c_str();
		    mii.cch = name.size();
		    
		    InsertMenuItem(hMenuSubSub, index, TRUE, &mii);
		  }
		}

		// show popup menu
		TrackPopupMenu(hMenuSub, TPM_LEFTALIGN,
			       p.x, p.y, 0, hwnd, NULL);
		// TrackPopupMenu may fail (ERROR_POPUP_ALREADY_ACTIVE)
		break;
	      }
	      
	      case WM_LBUTTONDBLCLK:
		SendMessage(hwnd, WM_COMMAND,
			    MAKELONG(ID_MENUITEM_investigate, 0), 0);
		break;
	    }
	  return 0;
	}
      
	case WM_COMMAND:
	{
	  int notify_code = HIWORD(wParam);
	  int id = LOWORD(wParam);
	  if (notify_code == 0) // menu
	    switch (id)
	    {
	      default:
		if (ID_MENUITEM_reloadBegin <= id)
		{
		  Registry reg(MAYU_REGISTRY_ROOT);
		  reg.write(".mayuIndex", id - ID_MENUITEM_reloadBegin);
		  This->load();
		}
		break;
	      case ID_MENUITEM_reload:
		This->load();
		break;
	      case ID_MENUITEM_investigate:
	      {
		ShowWindow(This->hwndLog, SW_SHOW);
		ShowWindow(This->hwndInvestigate, SW_SHOW);
		
		RECT rc1, rc2;
		GetWindowRect(This->hwndInvestigate, &rc1);
		GetWindowRect(This->hwndLog, &rc2);

		MoveWindow(This->hwndLog, rc1.left, rc1.bottom,
			   rcWidth(&rc1), rcHeight(&rc2), TRUE);
		
		SetForegroundWindow(This->hwndLog);
		SetForegroundWindow(This->hwndInvestigate);
		break;
	      }
	      case ID_MENUITEM_setting:
		if (!This->isSettingDialogOpened)
		{
		  This->isSettingDialogOpened = true;
		  if (DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_setting),
				NULL, dlgSetting_dlgProc))
		    This->load();
		  This->isSettingDialogOpened = false;
		}
		break;
	      case ID_MENUITEM_log:
		ShowWindow(This->hwndLog, SW_SHOW);
		SetForegroundWindow(This->hwndLog);
		break;
	      case ID_MENUITEM_version:
		ShowWindow(This->hwndVersion, SW_SHOW);
		SetForegroundWindow(This->hwndVersion);
		break;
	      case ID_MENUITEM_help:
	      {
		char buf[GANA_MAX_PATH];
		CHECK_TRUE( GetModuleFileName(hInst, buf, NUMBER_OF(buf)) );
		CHECK_TRUE( PathRemoveFileSpec(buf) );
	    
		istring helpFilename = buf;
		helpFilename += "\\";
		helpFilename += loadString(IDS_helpFilename);
		ShellExecute(NULL, "open", helpFilename.c_str(),
			     NULL, NULL, SW_SHOWNORMAL);
		break;
	      }
	      case ID_MENUITEM_disable:
		This->engine.enable(!This->engine.getIsEnabled());
		break;
	      case ID_MENUITEM_quit:
		PostQuitMessage(0);
		break;
	    }
	  return 0;
	}

	case WM_engineNotify:
	{
	  switch (wParam)
	  {
	    case engineNotify_shellExecute:
	      This->engine.shellExecute();
	      break;
	    case engineNotify_loadSetting:
	      This->load();
	      break;
	    case engineNotify_helpMessage:
	      This->showHelpMessage(false);
	      if (lParam)
		This->showHelpMessage(true);
	      break;
	    case engineNotify_showDlg:
	    {
	      // show investigate/log window
	      int sw = (lParam & ~Function::mayuDlgMask);
	      HWND hwnd = NULL;
	      if ((lParam & Function::mayuDlgMask)
		  == Function::mayuDlgInvestigate)
		hwnd = This->hwndInvestigate;
	      else if ((lParam & Function::mayuDlgMask)
		       == Function::mayuDlgLog)
		hwnd = This->hwndLog;
	      if (hwnd)
	      {
		ShowWindow(hwnd, sw);
		switch (sw)
		{
		  case SW_SHOWNORMAL:
		  case SW_SHOWMAXIMIZED:
		  case SW_SHOW:
		  case SW_RESTORE:
		  case SW_SHOWDEFAULT:
		    SetForegroundWindow(hwnd);
		    break;
		}
	      }
	      break;
	    }
	    default:
	      break;
	  }
	  return 0;
	}
	
	default:
	  if (message == This->WM_TaskbarRestart)
	  {
	    This->showHelpMessage(false, true);
	    return 0;
	  }
      }
    return DefWindowProc(hwnd, message, wParam, lParam);
  }

  /// load setting
  void load()
  {
    Setting *newSetting = new Setting;

    // set symbol
    for (int i = 1; i < __argc; i++)
    {
      if (__argv[i][0] == '-' && __argv[i][1] == 'D')
	newSetting->symbols.insert(__argv[i] + 2);
    }

    if (!SettingLoader(&log, &log).load(newSetting))
    {
      ShowWindow(hwndLog, SW_SHOW);
      SetForegroundWindow(hwndLog);
      delete newSetting;
      Acquire a(&log, 0);
      log << "error: failed to load." << endl;
      return;
    }
    while (!engine.setSetting(newSetting))
      Sleep(1000);
    delete setting;
    setting = newSetting;
  }

  // メッセージを表示
  void showHelpMessage(bool i_doesShow = true, bool i_doesAdd = false)
  {
    if (m_canUseTasktrayBaloon)
    {
      ni.uFlags |= NIF_INFO;
      if (i_doesShow)
      {
	std::string helpMessage, helpTitle;
	engine.getHelpMessages(&helpMessage, &helpTitle);
	StringTool::mbsfill(ni.szInfo, helpMessage.c_str(),
			    NUMBER_OF(ni.szInfo));
	StringTool::mbsfill(ni.szInfoTitle, helpTitle.c_str(),
			    NUMBER_OF(ni.szInfoTitle));
	ni.dwInfoFlags = NIIF_INFO;
      }
      else
	ni.szInfo[0] = ni.szInfoTitle[0] = '\0';
      CHECK_TRUE( Shell_NotifyIcon(i_doesAdd ? NIM_ADD : NIM_MODIFY, &ni) );
    }
  }

public:
  ///
  Mayu()
    : hwndTaskTray(NULL),
      hwndLog(NULL),
      WM_TaskbarRestart(RegisterWindowMessage(TEXT("TaskbarCreated"))),
      log(WM_MSGSTREAMNOTIFY),
      setting(NULL),
      isSettingDialogOpened(false),
      engine(log),
      m_canUseTasktrayBaloon(
	PACKVERSION(5, 0) <= getDllVersion(TEXT("shlwapi.dll")))
  {
    CHECK_TRUE( Register_focus() );
    CHECK_TRUE( Register_target() );
    CHECK_TRUE( Register_tasktray() );

    // change dir
    std::list<istring> pathes;
    getHomeDirectories(&pathes);
    for (std::list<istring>::iterator
	   i = pathes.begin(); i != pathes.end(); i ++)
      if (SetCurrentDirectory(i->c_str()))
	break;
    
    // create windows, dialogs
    istring title = loadString(IDS_mayu);
    hwndTaskTray = CreateWindow("mayuTasktray", title.c_str(),
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, 
				CW_USEDEFAULT, CW_USEDEFAULT, 
				NULL, NULL, hInst, this);
    CHECK_TRUE( hwndTaskTray );
    
    hwndLog =
      CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_log), NULL,
			dlgLog_dlgProc, (LPARAM)&log);
    CHECK_TRUE( hwndLog );

    DlgInvestigateData did;
    did.m_engine = &engine;
    did.m_hwndLog = hwndLog;
    hwndInvestigate =
      CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_investigate), NULL,
			dlgInvestigate_dlgProc, (LPARAM)&did);
    CHECK_TRUE( hwndInvestigate );

    hwndVersion =
      CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG_version), NULL,
		   dlgVersion_dlgProc);
    CHECK_TRUE( hwndVersion );

    // attach log
    SendMessage(GetDlgItem(hwndLog, IDC_EDIT_log), EM_SETLIMITTEXT, 0, 0);
    log.attach(hwndTaskTray);

    // start keyboard handler thread
    engine.setAssociatedWndow(hwndTaskTray);
    engine.start();
    
    // show tasktray icon
    memset(&ni, 0, sizeof(ni));
    ni.uID    = ID_TaskTrayIcon;
    ni.hWnd   = hwndTaskTray;
    ni.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    ni.hIcon  = loadSmallIcon(IDI_ICON_mayu);
    ni.uCallbackMessage = WM_TASKTRAYNOTIFY;
    string tip = loadString(IDS_mayu);
    strncpy(ni.szTip, tip.c_str(), sizeof(ni.szTip));
    ni.szTip[NUMBER_OF(ni.szTip) - 1] = '\0';
    if (m_canUseTasktrayBaloon)
      ni.cbSize = sizeof(ni);
    else
      ni.cbSize = NOTIFYICONDATA_V1_SIZE;
    showHelpMessage(false, true);

    // create menu
    hMenuTaskTray = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU_tasktray));
    assert(hMenuTaskTray);
    
    // start message handler thread
    CHECK_TRUE( mhEvent = CreateEvent(NULL, FALSE, FALSE, NULL) );
    CHECK_TRUE( 0 <= _beginthread(notifyHandler, 0, this) );
    CHECK( WAIT_OBJECT_0 ==, WaitForSingleObject(mhEvent, INFINITE) );

    // set initial lock state
    notifyLockState();
  }

  ///
  ~Mayu()
  {
    // first, detach log from edit control to avoid deadlock
    log.detach();
    
    // destroy windows
    CHECK_TRUE( DestroyWindow(hwndVersion) );
    CHECK_TRUE( DestroyWindow(hwndInvestigate) );
    CHECK_TRUE( DestroyWindow(hwndLog) );
    CHECK_TRUE( DestroyWindow(hwndTaskTray) );
    
    // wait for message handler thread terminate
    Notify n = { Notify::TypeMayuExit };
    notify(&n, sizeof(n));
    CHECK( WAIT_OBJECT_0 ==, WaitForSingleObject(mhEvent, INFINITE) );
    CHECK_TRUE( CloseHandle(mhEvent) );

    // destroy menu
    DestroyMenu(hMenuTaskTray);
    
    // delete tasktray icon
    CHECK_TRUE( Shell_NotifyIcon(NIM_DELETE, &ni) );
    CHECK_TRUE( DestroyIcon(ni.hIcon) );

    // stop keyboard handler thread
    engine.stop();
    
    // remove setting;
    delete setting;
  }
  
  /// message loop
  WPARAM messageLoop()
  {
    // banner
    {
      time_t now;
      time(&now);
      char buf[1024];
#ifdef __BORLANDC__
#pragma message("\t\t****\tAfter std::ostream() is called,  ")
#pragma message("\t\t****\tstrftime(... \"%%#c\" ...) fails.")
#pragma message("\t\t****\tWhy ? Bug of Borland C++ 5.5 ?   ")
#pragma message("\t\t****\t                         - nayuta")
      strftime(buf, NUMBER_OF(buf),
	       "%Y/%m/%d %H:%M:%S (Compiled by Borland C++)", localtime(&now));
#else
      strftime(buf, NUMBER_OF(buf), "%#c", localtime(&now));
#endif
      
      Acquire a(&log, 0);
      log << loadString(IDS_mayu) << " "VERSION" @ " << buf << endl;
      CHECK_TRUE( GetModuleFileName(hInst, buf, NUMBER_OF(buf)) );
      log << buf << endl << endl;
    }
    load();
    
    MSG msg;
    while (0 < GetMessage(&msg, NULL, 0, 0))
    {
      if (IsDialogMessage(hwndLog, &msg))
	continue;
      if (IsDialogMessage(hwndInvestigate, &msg))
	continue;
      if (IsDialogMessage(hwndVersion, &msg))
	continue;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    return msg.wParam;
  }  
};


// ////////////////////////////////////////////////////////////////////////////
// Functions


/// convert registry
void convertRegistry()
{
  Registry reg(MAYU_REGISTRY_ROOT);
  StringTool::istring dot_mayu;
  bool doesAdd = false;
  if (reg.read(".mayu", &dot_mayu))
  {
    reg.write(".mayu0", ";" + dot_mayu + ";");
    reg.remove(".mayu");
    doesAdd = true;
  }
  else if (!reg.read(".mayu0", &dot_mayu))
  {
    reg.write(".mayu0", loadString(IDS_readFromHomeDirectory) + ";;");
    doesAdd = true;
  }
  if (doesAdd)
  {
    Registry commonreg(HKEY_LOCAL_MACHINE, "Software\\GANAware\\mayu");
    StringTool::istring dir, layout;
    if (commonreg.read("dir", &dir) && commonreg.read("layout", &layout))
    {
      StringTool::istring tmp = ";" + dir + "\\dot.mayu";
      if (layout == "109")
      {
	reg.write(".mayu1", loadString(IDS_109Emacs) + tmp
		  + ";-DUSE109" ";-DUSEdefault");
	reg.write(".mayu2", loadString(IDS_104on109Emacs) + tmp
		  + ";-DUSE109" ";-DUSEdefault" ";-DUSE104on109");
	reg.write(".mayu3", loadString(IDS_109) + tmp
		  + ";-DUSE109");
	reg.write(".mayu4", loadString(IDS_104on109) + tmp
		  + ";-DUSE109" ";-DUSE104on109");
      }
      else
      {
	reg.write(".mayu1", loadString(IDS_104Emacs) + tmp
		  + ";-DUSE104" ";-DUSEdefault");
	reg.write(".mayu2", loadString(IDS_109on104Emacs) + tmp
		  + ";-DUSE104" ";-DUSEdefault" ";-DUSE109on104");
	reg.write(".mayu3", loadString(IDS_104) + tmp
		  + ";-DUSE104");
	reg.write(".mayu4", loadString(IDS_109on104) + tmp
		  + ";-DUSE104" ";-DUSE109on104");
      }
    }
  }
}


/// main
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
		   LPSTR /* lpszCmdLine */, int /* nCmdShow */)
{
  hInst = hInstance;

  // set locale
  CHECK_TRUE( setlocale(LC_ALL, "") );

  // common controls
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_LISTVIEW_CLASSES;
  CHECK_TRUE( InitCommonControlsEx(&icc) );

  // convert old registry to new registry
  convertRegistry();
  
  // is another mayu running ?
  HANDLE mutex = CreateMutex((SECURITY_ATTRIBUTES *)NULL,
			     TRUE, MUTEX_MAYU_EXCLUSIVE_RUNNING);
  if (GetLastError() == ERROR_ALREADY_EXISTS)
  {
    // another mayu already running
    string text = loadString(IDS_mayuAlreadyExists);
    string title = loadString(IDS_mayu);
    MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONSTOP);
    return 1;
  }
  
  CHECK_FALSE( installHooks() );
  try
  {
    Mayu().messageLoop();
  }
  catch (ErrorMessage &e)
  {
    string title = loadString(IDS_mayu);
    MessageBox((HWND)NULL, e.getMessage().c_str(), title.c_str(),
	       MB_OK | MB_ICONSTOP);
  }
  CHECK_FALSE( uninstallHooks() );
  
  CHECK_TRUE( CloseHandle(mutex) );
  return 0;
}
