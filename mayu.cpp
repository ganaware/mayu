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

  omsgstream log;		/// log stream (output to log dialog's edit)

  HANDLE mhEvent;		/// event for message handler thread

  HMENU hMenuTaskTray;		/// tasktray menu
  
  Setting *setting;		/// current setting
  
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
    _true( SetEvent(mhEvent) );
    
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
	  _true( CloseHandle(hMailslot) );
	  _true( SetEvent(mhEvent) );
	  return;
	}
	
	case Notify::TypeSetFocus:
	case Notify::TypeName:
	{
	  NotifySetFocus *n = (NotifySetFocus *)buf;
	  n->className[lengthof(n->className) - 1] = '\0';
	  n->titleName[lengthof(n->titleName) - 1] = '\0';
	  
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
	  log << "TITLE:\t" << n->titleName << endl << endl;
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
		_true( GetCursorPos(&p) );
		SetForegroundWindow(hwnd);
		HMENU hMenuSub = GetSubMenu(This->hMenuTaskTray, 0);
		if (This->engine.getIsEnabled())
		  CheckMenuItem(hMenuSub, ID_MENUITEM_disable,
				MF_UNCHECKED | MF_BYCOMMAND);
		else
		  CheckMenuItem(hMenuSub, ID_MENUITEM_disable,
				MF_CHECKED | MF_BYCOMMAND);
		_true( SetMenuDefaultItem(hMenuSub,
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
		if (DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_setting),
			      NULL, dlgSetting_dlgProc))
		  This->load();
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
		_true( GetModuleFileName(hInst, buf, lengthof(buf)) );
		_true( PathRemoveFileSpec(buf) );
	    
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
	  }
	  return 0;
	}
	
	default:
	  if (message == This->WM_TaskbarRestart)
	  {
	    _true( Shell_NotifyIcon(NIM_ADD, &This->ni) );
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

public:
  ///
  Mayu()
    : hwndTaskTray(NULL),
      hwndLog(NULL),
      WM_TaskbarRestart(RegisterWindowMessage(TEXT("TaskbarCreated"))),
      log(WM_MSGSTREAMNOTIFY),
      setting(NULL),
      engine(log)
  {
    _true( Register_focus() );
    _true( Register_target() );
    _true( Register_tasktray() );

    // change dir
    istring path;
    for (int i = 0; getHomeDirectory(i, &path); i ++)
      if (SetCurrentDirectory(path.c_str()))
	break;
      
    // create windows, dialogs
    istring title = loadString(IDS_mayu);
    hwndTaskTray = CreateWindow("mayuTasktray", title.c_str(),
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, 
				CW_USEDEFAULT, CW_USEDEFAULT, 
				NULL, NULL, hInst, this);
    _true( hwndTaskTray );
    
    hwndLog =
      CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_log), NULL,
			dlgLog_dlgProc, (LPARAM)&log);
    _true( hwndLog );
    
    hwndInvestigate =
      CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG_investigate), NULL,
			dlgInvestigate_dlgProc, (LPARAM)&engine);
    _true( hwndInvestigate );

    hwndVersion =
      CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG_version), NULL,
		   dlgVersion_dlgProc);
    _true( hwndVersion );

    // attach log
    SendMessage(GetDlgItem(hwndLog, IDC_EDIT_log), EM_SETLIMITTEXT, 0, 0);
    log.attach(hwndTaskTray);

    // start keyboard handler thread
    engine.setAssociatedWndow(hwndTaskTray);
    engine.start();
    
    // show tasktray icon
    ni.uID    = ID_TaskTrayIcon;
    ni.hWnd   = hwndTaskTray;
    ni.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    ni.hIcon  = loadSmallIcon(IDI_ICON_mayu);
    ni.uCallbackMessage = WM_TASKTRAYNOTIFY;
    string tip = loadString(IDS_mayu);
    strncpy(ni.szTip, tip.c_str(), sizeof(ni.szTip));
    ni.szTip[lengthof(ni.szTip) - 1] = '\0';
    ni.cbSize = sizeof(ni);
    _true( Shell_NotifyIcon(NIM_ADD, &ni) );

    // create menu
    hMenuTaskTray = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU_tasktray));
    assert(hMenuTaskTray);
    
    // start message handler thread
    _true( mhEvent = CreateEvent(NULL, FALSE, FALSE, NULL) );
    _true( 0 <= _beginthread(notifyHandler, 0, this) );
    _must_be( WaitForSingleObject(mhEvent, INFINITE), ==, WAIT_OBJECT_0 );

    // set initial lock state
    notifyLockState();
  }

  ///
  ~Mayu()
  {
    // first, detach log from edit control to avoid deadlock
    log.detach();
    
    // destroy windows
    _true( DestroyWindow(hwndVersion) );
    _true( DestroyWindow(hwndInvestigate) );
    _true( DestroyWindow(hwndLog) );
    _true( DestroyWindow(hwndTaskTray) );
    
    // wait for message handler thread terminate
    Notify n = { Notify::TypeMayuExit };
    notify(&n, sizeof(n));
    _must_be( WaitForSingleObject(mhEvent, INFINITE), ==, WAIT_OBJECT_0 );
    _true( CloseHandle(mhEvent) );

    // destroy menu
    DestroyMenu(hMenuTaskTray);
    
    // delete tasktray icon
    _true( Shell_NotifyIcon(NIM_DELETE, &ni) );
    _true( DestroyIcon(ni.hIcon) );

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
      strftime(buf, lengthof(buf),
	       "%Y/%m/%d %H:%M:%S (Compiled by Borland C++)", localtime(&now));
#else
      strftime(buf, lengthof(buf), "%#c", localtime(&now));
#endif
      
      Acquire a(&log, 0);
      log << loadString(IDS_mayu) << " "VERSION" @ " << buf << endl;
      _true( GetModuleFileName(hInst, buf, lengthof(buf)) );
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
  _true( setlocale(LC_ALL, "") );

  // common controls
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_LISTVIEW_CLASSES;
  _true( InitCommonControlsEx(&icc) );

  // convert old registry to new registry
  convertRegistry();
  
  // is another mayu running ?
  HANDLE mutex = CreateMutex((SECURITY_ATTRIBUTES *)NULL,
			     TRUE, mutexMayuExclusiveRunning);
  if (GetLastError() == ERROR_ALREADY_EXISTS)
  {
    // another mayu already running
    string text = loadString(IDS_mayuAlreadyExists);
    string title = loadString(IDS_mayu);
    MessageBox((HWND)NULL, text.c_str(), title.c_str(), MB_OK | MB_ICONSTOP);
    return 1;
  }
  
  _false( installHooks() );
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
  _false( uninstallHooks() );
  
  _true( CloseHandle(mutex) );
  return 0;
}
