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
  HWND m_hwndTaskTray;				/// tasktray window
  HWND m_hwndLog;				/// log dialog
  HWND m_hwndInvestigate;			/// investigate dialog
  HWND m_hwndVersion;				/// version dialog
  
  UINT m_WM_TaskbarRestart;			/** window message sent when
                                                    taskber restarts */
  NOTIFYICONDATA m_ni;				/// taskbar icon data
  bool m_canUseTasktrayBaloon;			/// 

  omsgstream m_log;				/** log stream (output to log
						    dialog's edit) */

  HANDLE m_mhEvent;				/** event for message handler
						    thread */

  HMENU m_hMenuTaskTray;			/// tasktray menu
  
  Setting *m_setting;				/// current setting
  bool m_isSettingDialogOpened;			/// is setting dialog opened ?
  
  Engine m_engine;				/// engine

  enum
  { 
    WM_APP_taskTrayNotify = WM_APP + 101,	///
    WM_APP_msgStreamNotify = WM_APP + 102,	///
    ID_TaskTrayIcon = 1,			///
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
    wc.hInstance     = g_hInst;
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "mayuTasktray";
    return RegisterClass(&wc);
  }

  /// notify handler thread
  static void notifyHandler(void *i_this)
  {
    reinterpret_cast<Mayu *>(i_this)->notifyHandler();
  }
  ///
  void notifyHandler()
  {
    HANDLE hMailslot =
      CreateMailslot(NOTIFY_MAILSLOT_NAME, NOTIFY_MESSAGE_SIZE,
		     MAILSLOT_WAIT_FOREVER, (SECURITY_ATTRIBUTES *)NULL);
    ASSERT(hMailslot != INVALID_HANDLE_VALUE);

    // initialize ok
    CHECK_TRUE( SetEvent(m_mhEvent) );
    
    // loop
    while (true)
    {
      DWORD len = 0;
      BYTE buf[NOTIFY_MESSAGE_SIZE];
      
      // wait for message
      if (!ReadFile(hMailslot, buf, NOTIFY_MESSAGE_SIZE, &len,
		    (OVERLAPPED *)NULL))
	continue;

      switch (reinterpret_cast<Notify *>(buf)->m_type)
      {
	case Notify::Type_mayuExit:		// terminate thread
	{
	  CHECK_TRUE( CloseHandle(hMailslot) );
	  CHECK_TRUE( SetEvent(m_mhEvent) );
	  return;
	}
	
	case Notify::Type_setFocus:
	case Notify::Type_name:
	{
	  NotifySetFocus *n = (NotifySetFocus *)buf;
	  n->m_className[NUMBER_OF(n->m_className) - 1] = '\0';
	  n->m_titleName[NUMBER_OF(n->m_titleName) - 1] = '\0';
	  
	  if (n->m_type == Notify::Type_setFocus)
	    m_engine.setFocus(n->m_hwnd, n->m_threadId,
			      n->m_className, n->m_titleName, false);

	  {
	    Acquire a(&m_log, 1);
	    m_log << "HWND:\t" << hex << reinterpret_cast<int>(n->m_hwnd)
		  << dec << endl;
	    m_log << "THREADID:" << static_cast<int>(n->m_threadId) << endl;
	  }
	  Acquire a(&m_log, (n->m_type == Notify::Type_name) ? 0 : 1);
	  m_log << "CLASS:\t" << n->m_className << endl;
	  m_log << "TITLE:\t" << n->m_titleName << endl;
	  
	  bool isMDI = true;
	  HWND hwnd = getToplevelWindow(n->m_hwnd, &isMDI);
	  RECT rc;
	  if (isMDI)
	  {
	    getChildWindowRect(hwnd, &rc);
	    m_log << "MDI Window Position/Size: ("
		<< rc.left << ", "<< rc.top << ") / ("
		<< rcWidth(&rc) << "x" << rcHeight(&rc) << ")" << endl;
	    hwnd = getToplevelWindow(n->m_hwnd, NULL);
	  }
	  
	  GetWindowRect(hwnd, &rc);
	  m_log << "Toplevel Window Position/Size: ("
		<< rc.left << ", "<< rc.top << ") / ("
		<< rcWidth(&rc) << "x" << rcHeight(&rc) << ")" << endl;

	  SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rc, FALSE);
	  m_log << "Desktop Window Position/Size: ("
	      << rc.left << ", "<< rc.top << ") / ("
	      << rcWidth(&rc) << "x" << rcHeight(&rc) << ")" << endl;
	  
	  m_log << endl;
	  break;
	}
	
	case Notify::Type_lockState:
	{
	  NotifyLockState *n = (NotifyLockState *)buf;
	  m_engine.setLockState(n->m_isNumLockToggled,
				n->m_isCapsLockToggled,
				n->m_isScrollLockToggled,
				n->m_isImeLockToggled,
				n->m_isImeCompToggled);
	  break;
	}

	case Notify::Type_sync:
	{
	  m_engine.syncNotify();
	  break;
	}

	case Notify::Type_threadDetach:
	{
	  NotifyThreadDetach *n = (NotifyThreadDetach *)buf;
	  m_engine.threadDetachNotify(n->m_threadId);
	  break;
	}

	case Notify::Type_command:
	{
	  NotifyCommand *n = (NotifyCommand *)buf;
	  m_engine.commandNotify(n->m_hwnd, n->m_message,
				 n->m_wParam, n->m_lParam);
	  break;
	}
      }
    }
  }
  
  /// window procedure for tasktray
  static LRESULT CALLBACK
  tasktray_wndProc(HWND i_hwnd, UINT i_message,
		   WPARAM i_wParam, LPARAM i_lParam)
  {
    Mayu *This = reinterpret_cast<Mayu *>(GetWindowLong(i_hwnd, 0));

    if (!This)
      switch (i_message)
      {
	case WM_CREATE:
	  This = reinterpret_cast<Mayu *>(
	    reinterpret_cast<CREATESTRUCT *>(i_lParam)->lpCreateParams);
	  SetWindowLong(i_hwnd, 0, (long)This);
	  return 0;
      }
    else
      switch (i_message)
      {
	case WM_APP_msgStreamNotify:
	{
	  omsgbuf *log = (omsgbuf *)i_lParam;
	  const string &str = log->acquireString();
	  editInsertTextAtLast(GetDlgItem(This->m_hwndLog, IDC_EDIT_log),
			       str, 65000);
	  log->releaseString();
	  return 0;
	}
	
	case WM_APP_taskTrayNotify:
	{
	  if (i_wParam == ID_TaskTrayIcon)
	    switch (i_lParam)
	    {
	      case WM_RBUTTONUP:
	      {
		POINT p;
		CHECK_TRUE( GetCursorPos(&p) );
		SetForegroundWindow(i_hwnd);
		HMENU hMenuSub = GetSubMenu(This->m_hMenuTaskTray, 0);
		if (This->m_engine.getIsEnabled())
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
			       p.x, p.y, 0, i_hwnd, NULL);
		// TrackPopupMenu may fail (ERROR_POPUP_ALREADY_ACTIVE)
		break;
	      }
	      
	      case WM_LBUTTONDBLCLK:
		SendMessage(i_hwnd, WM_COMMAND,
			    MAKELONG(ID_MENUITEM_investigate, 0), 0);
		break;
	    }
	  return 0;
	}
      
	case WM_COMMAND:
	{
	  int notify_code = HIWORD(i_wParam);
	  int id = LOWORD(i_wParam);
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
		ShowWindow(This->m_hwndLog, SW_SHOW);
		ShowWindow(This->m_hwndInvestigate, SW_SHOW);
		
		RECT rc1, rc2;
		GetWindowRect(This->m_hwndInvestigate, &rc1);
		GetWindowRect(This->m_hwndLog, &rc2);

		MoveWindow(This->m_hwndLog, rc1.left, rc1.bottom,
			   rcWidth(&rc1), rcHeight(&rc2), TRUE);
		
		SetForegroundWindow(This->m_hwndLog);
		SetForegroundWindow(This->m_hwndInvestigate);
		break;
	      }
	      case ID_MENUITEM_setting:
		if (!This->m_isSettingDialogOpened)
		{
		  This->m_isSettingDialogOpened = true;
		  if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_setting),
				NULL, dlgSetting_dlgProc))
		    This->load();
		  This->m_isSettingDialogOpened = false;
		}
		break;
	      case ID_MENUITEM_log:
		ShowWindow(This->m_hwndLog, SW_SHOW);
		SetForegroundWindow(This->m_hwndLog);
		break;
	      case ID_MENUITEM_version:
		ShowWindow(This->m_hwndVersion, SW_SHOW);
		SetForegroundWindow(This->m_hwndVersion);
		break;
	      case ID_MENUITEM_help:
	      {
		char buf[GANA_MAX_PATH];
		CHECK_TRUE( GetModuleFileName(g_hInst, buf, NUMBER_OF(buf)) );
		CHECK_TRUE( PathRemoveFileSpec(buf) );
	    
		istring helpFilename = buf;
		helpFilename += "\\";
		helpFilename += loadString(IDS_helpFilename);
		ShellExecute(NULL, "open", helpFilename.c_str(),
			     NULL, NULL, SW_SHOWNORMAL);
		break;
	      }
	      case ID_MENUITEM_disable:
		This->m_engine.enable(!This->m_engine.getIsEnabled());
		break;
	      case ID_MENUITEM_quit:
		PostQuitMessage(0);
		break;
	    }
	  return 0;
	}

	case WM_APP_engineNotify:
	{
	  switch (i_wParam)
	  {
	    case EngineNotify_shellExecute:
	      This->m_engine.shellExecute();
	      break;
	    case EngineNotify_loadSetting:
	      This->load();
	      break;
	    case EngineNotify_helpMessage:
	      This->showHelpMessage(false);
	      if (i_lParam)
		This->showHelpMessage(true);
	      break;
	    case EngineNotify_showDlg:
	    {
	      // show investigate/log window
	      int sw = (i_lParam & ~Function::MayuDlg_mask);
	      HWND hwnd = NULL;
	      if ((i_lParam & Function::MayuDlg_mask)
		  == Function::MayuDlg_investigate)
		hwnd = This->m_hwndInvestigate;
	      else if ((i_lParam & Function::MayuDlg_mask)
		       == Function::MayuDlg_log)
		hwnd = This->m_hwndLog;
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
	  if (i_message == This->m_WM_TaskbarRestart)
	  {
	    This->showHelpMessage(false, true);
	    return 0;
	  }
      }
    return DefWindowProc(i_hwnd, i_message, i_wParam, i_lParam);
  }

  /// load setting
  void load()
  {
    Setting *newSetting = new Setting;

    // set symbol
    for (int i = 1; i < __argc; ++ i)
    {
      if (__argv[i][0] == '-' && __argv[i][1] == 'D')
	newSetting->m_symbols.insert(__argv[i] + 2);
    }

    if (!SettingLoader(&m_log, &m_log).load(newSetting))
    {
      ShowWindow(m_hwndLog, SW_SHOW);
      SetForegroundWindow(m_hwndLog);
      delete newSetting;
      Acquire a(&m_log, 0);
      m_log << "error: failed to load." << endl;
      return;
    }
    while (!m_engine.setSetting(newSetting))
      Sleep(1000);
    delete m_setting;
    m_setting = newSetting;
  }

  // メッセージを表示
  void showHelpMessage(bool i_doesShow = true, bool i_doesAdd = false)
  {
    if (m_canUseTasktrayBaloon)
    {
      m_ni.uFlags |= NIF_INFO;
      if (i_doesShow)
      {
	std::string helpMessage, helpTitle;
	m_engine.getHelpMessages(&helpMessage, &helpTitle);
	StringTool::mbsfill(m_ni.szInfo, helpMessage.c_str(),
			    NUMBER_OF(m_ni.szInfo));
	StringTool::mbsfill(m_ni.szInfoTitle, helpTitle.c_str(),
			    NUMBER_OF(m_ni.szInfoTitle));
	m_ni.dwInfoFlags = NIIF_INFO;
      }
      else
	m_ni.szInfo[0] = m_ni.szInfoTitle[0] = '\0';
      CHECK_TRUE( Shell_NotifyIcon(i_doesAdd ? NIM_ADD : NIM_MODIFY, &m_ni) );
    }
  }

public:
  ///
  Mayu()
    : m_hwndTaskTray(NULL),
      m_hwndLog(NULL),
      m_WM_TaskbarRestart(RegisterWindowMessage(TEXT("TaskbarCreated"))),
      m_log(WM_APP_msgStreamNotify),
      m_setting(NULL),
      m_isSettingDialogOpened(false),
      m_engine(m_log),
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
    m_hwndTaskTray = CreateWindow("mayuTasktray", title.c_str(),
				  WS_OVERLAPPEDWINDOW,
				  CW_USEDEFAULT, CW_USEDEFAULT, 
				  CW_USEDEFAULT, CW_USEDEFAULT, 
				  NULL, NULL, g_hInst, this);
    CHECK_TRUE( m_hwndTaskTray );
    
    m_hwndLog =
      CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_log), NULL,
			dlgLog_dlgProc, (LPARAM)&m_log);
    CHECK_TRUE( m_hwndLog );

    DlgInvestigateData did;
    did.m_engine = &m_engine;
    did.m_hwndLog = m_hwndLog;
    m_hwndInvestigate =
      CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_investigate), NULL,
			dlgInvestigate_dlgProc, (LPARAM)&did);
    CHECK_TRUE( m_hwndInvestigate );

    m_hwndVersion =
      CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_version), NULL,
		   dlgVersion_dlgProc);
    CHECK_TRUE( m_hwndVersion );

    // attach log
    SendMessage(GetDlgItem(m_hwndLog, IDC_EDIT_log), EM_SETLIMITTEXT, 0, 0);
    m_log.attach(m_hwndTaskTray);

    // start keyboard handler thread
    m_engine.setAssociatedWndow(m_hwndTaskTray);
    m_engine.start();
    
    // show tasktray icon
    memset(&m_ni, 0, sizeof(m_ni));
    m_ni.uID    = ID_TaskTrayIcon;
    m_ni.hWnd   = m_hwndTaskTray;
    m_ni.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_ni.hIcon  = loadSmallIcon(IDI_ICON_mayu);
    m_ni.uCallbackMessage = WM_APP_taskTrayNotify;
    string tip = loadString(IDS_mayu);
    strncpy(m_ni.szTip, tip.c_str(), sizeof(m_ni.szTip));
    m_ni.szTip[NUMBER_OF(m_ni.szTip) - 1] = '\0';
    if (m_canUseTasktrayBaloon)
      m_ni.cbSize = sizeof(m_ni);
    else
      m_ni.cbSize = NOTIFYICONDATA_V1_SIZE;
    showHelpMessage(false, true);

    // create menu
    m_hMenuTaskTray = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU_tasktray));
    ASSERT(m_hMenuTaskTray);
    
    // start message handler thread
    CHECK_TRUE( m_mhEvent = CreateEvent(NULL, FALSE, FALSE, NULL) );
    CHECK_TRUE( 0 <= _beginthread(notifyHandler, 0, this) );
    CHECK( WAIT_OBJECT_0 ==, WaitForSingleObject(m_mhEvent, INFINITE) );

    // set initial lock state
    notifyLockState();
  }

  ///
  ~Mayu()
  {
    // first, detach log from edit control to avoid deadlock
    m_log.detach();
    
    // destroy windows
    CHECK_TRUE( DestroyWindow(m_hwndVersion) );
    CHECK_TRUE( DestroyWindow(m_hwndInvestigate) );
    CHECK_TRUE( DestroyWindow(m_hwndLog) );
    CHECK_TRUE( DestroyWindow(m_hwndTaskTray) );
    
    // wait for message handler thread terminate
    Notify n = { Notify::Type_mayuExit };
    notify(&n, sizeof(n));
    CHECK( WAIT_OBJECT_0 ==, WaitForSingleObject(m_mhEvent, INFINITE) );
    CHECK_TRUE( CloseHandle(m_mhEvent) );

    // destroy menu
    DestroyMenu(m_hMenuTaskTray);
    
    // delete tasktray icon
    CHECK_TRUE( Shell_NotifyIcon(NIM_DELETE, &m_ni) );
    CHECK_TRUE( DestroyIcon(m_ni.hIcon) );

    // stop keyboard handler thread
    m_engine.stop();
    
    // remove setting;
    delete m_setting;
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
      
      Acquire a(&m_log, 0);
      m_log << loadString(IDS_mayu) << " "VERSION" @ " << buf << endl;
      CHECK_TRUE( GetModuleFileName(g_hInst, buf, NUMBER_OF(buf)) );
      m_log << buf << endl << endl;
    }
    load();
    
    MSG msg;
    while (0 < GetMessage(&msg, NULL, 0, 0))
    {
      if (IsDialogMessage(m_hwndLog, &msg))
	continue;
      if (IsDialogMessage(m_hwndInvestigate, &msg))
	continue;
      if (IsDialogMessage(m_hwndVersion, &msg))
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
int WINAPI WinMain(HINSTANCE i_hInstance, HINSTANCE /* i_hPrevInstance */,
		   LPSTR /* i_lpszCmdLine */, int /* i_nCmdShow */)
{
  g_hInst = i_hInstance;

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
  catch (ErrorMessage &i_e)
  {
    string title = loadString(IDS_mayu);
    MessageBox((HWND)NULL, i_e.getMessage().c_str(), title.c_str(),
	       MB_OK | MB_ICONSTOP);
  }
  CHECK_FALSE( uninstallHooks() );
  
  CHECK_TRUE( CloseHandle(mutex) );
  return 0;
}
