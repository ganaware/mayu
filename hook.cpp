//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// hook.cpp


#define _HOOK_CPP

#include "misc.h"

#include "hook.h"
#include "stringtool.h"

#include <locale.h>
#include <imm.h>
#include <richedit.h>


///
#define HOOK_DATA_NAME _T("{08D6E55C-5103-4e00-8209-A1C4AB13BBEF}") _T(VERSION)


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Global Variables


DllExport HookData *g_hookData;			///

struct Globals
{
  HANDLE m_hHookData;				///
  HWND m_hwndFocus;				/// 
  HINSTANCE m_hInstDLL;				///
  bool m_isInMenu;				///
  UINT m_WM_MAYU_MESSAGE;			///
  bool m_isImeLock;				///
  bool m_isImeCompositioning;			///
};

static Globals g;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Prototypes


static void notifyThreadDetach();
static void notifyShow(NotifyShow::Show i_show, bool i_isMDI);
static void notifyLog(_TCHAR *i_msg);
static bool mapHookData();
static void unmapHookData();


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions


/// EntryPoint
BOOL WINAPI DllMain(HINSTANCE i_hInstDLL, DWORD i_fdwReason,
		    LPVOID /* i_lpvReserved */)
{
  switch (i_fdwReason)
  {
    case DLL_PROCESS_ATTACH:
    {
      if (!mapHookData())
	return FALSE;
      g.m_hInstDLL = i_hInstDLL;
      _tsetlocale(LC_ALL, _T(""));
      g.m_WM_MAYU_MESSAGE = RegisterWindowMessage(WM_MAYU_MESSAGE_NAME);
      break;
    }
    case DLL_THREAD_ATTACH:
      break;
    case DLL_PROCESS_DETACH:
      notifyThreadDetach();
      unmapHookData();
      break;
    case DLL_THREAD_DETACH:
      notifyThreadDetach();
      break;
    default:
      break;
  }
  return TRUE;
}


/// map hook data
static bool mapHookData()
{
  g.m_hHookData = CreateFileMapping((HANDLE)0xffffffff, NULL, PAGE_READWRITE,
				    0, sizeof(HookData), HOOK_DATA_NAME);
  if (!g.m_hHookData)
    return false;
  
  g_hookData =
    (HookData *)MapViewOfFile(g.m_hHookData, FILE_MAP_READ | FILE_MAP_WRITE,
			      0, 0, sizeof(HookData));
  if (!g_hookData)
  {
    unmapHookData();
    return false;
  }
  return true;
}


/// unmap hook data
static void unmapHookData()
{
  if (g_hookData)
    if (!UnmapViewOfFile(g_hookData))
      return;
  g_hookData = NULL;
  if (g.m_hHookData)
    CloseHandle(g.m_hHookData);
  g.m_hHookData = NULL;
}


/// notify
DllExport bool notify(void *i_data, size_t i_dataSize)
{
  COPYDATASTRUCT cd;
  DWORD result;

  cd.dwData = reinterpret_cast<Notify *>(i_data)->m_type;
  cd.cbData = i_dataSize;
  cd.lpData = i_data;
  if (g_hookData->m_hwndTaskTray == NULL)
    return false;
  if (!SendMessageTimeout(g_hookData->m_hwndTaskTray, WM_COPYDATA,
			  NULL, reinterpret_cast<LPARAM>(&cd),
			  SMTO_ABORTIFHUNG | SMTO_NORMAL, 5000, &result))
    return false;
  return true;
}


/// get class name and title name
static void getClassNameTitleName(HWND i_hwnd, bool i_isInMenu, 
				  tstringi *o_className,
				  tstring *o_titleName)
{
  tstringi &className = *o_className;
  tstring &titleName = *o_titleName;
  
  bool isTheFirstTime = true;
  
  if (i_isInMenu)
  {
    className = titleName = _T("MENU");
    isTheFirstTime = false;
  }

  while (true)
  {
    _TCHAR buf[MAX(GANA_MAX_PATH, GANA_MAX_ATOM_LENGTH)];

    // get class name
    if (i_hwnd)
      GetClassName(i_hwnd, buf, NUMBER_OF(buf));
    else
      GetModuleFileName(GetModuleHandle(NULL), buf, NUMBER_OF(buf));
    buf[NUMBER_OF(buf) - 1] = _T('\0');
    if (isTheFirstTime)
      className = buf;
    else
      className = tstringi(buf) + _T(":") + className;
    
    // get title name
    if (i_hwnd)
    {
      GetWindowText(i_hwnd, buf, NUMBER_OF(buf));
      buf[NUMBER_OF(buf) - 1] = _T('\0');
      for (_TCHAR *b = buf; *b; ++ b)
	if (_istlead(*b) && b[1])
	  b ++;
	else if (_istcntrl(*b))
	  *b = _T('?');
    }
    if (isTheFirstTime)
      titleName = buf;
    else
      titleName = tstring(buf) + _T(":") + titleName;

    // next loop or exit
    if (!i_hwnd)
      break;
    i_hwnd = GetParent(i_hwnd);
    isTheFirstTime = false;
  }
}


/// update show
static void updateShow(HWND i_hwnd, WPARAM i_show)
{
  bool isMDI = false;
  _TCHAR className[MAX(GANA_MAX_PATH, GANA_MAX_ATOM_LENGTH)];

  if (!i_hwnd)
    return;

  // check class name to avoid invalid notify
  GetClassName(i_hwnd, className, NUMBER_OF(className));
  className[NUMBER_OF(className) - 1] = _T('\0');
  if (_tcsicmp(className, _T("SysShadow")) == 0 ||
      _tcsicmp(className, _T("UserEventWindow")) == 0)
    return;

  LONG style = GetWindowLong(i_hwnd, GWL_STYLE);
  if (style & WS_CHILD)
  {
    LONG exStyle = GetWindowLong(i_hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_MDICHILD)
    {
      isMDI = true;
    }
    else
      return; // ignore non-MDI child window case
  }

  switch (i_show)
  {
    case SIZE_MAXIMIZED:
      notifyShow(NotifyShow::Show_Maximized, isMDI);
      break;
    case SIZE_MINIMIZED:
      notifyShow(NotifyShow::Show_Minimized, isMDI);
      break;
    case SIZE_RESTORED:
      notifyShow(NotifyShow::Show_Normal, isMDI);
      break;
    default:
      break;
  }
}


/// notify WM_Targetted
static void notifyName(HWND i_hwnd, Notify::Type i_type = Notify::Type_name)
{
  tstringi className;
  tstring titleName;
  getClassNameTitleName(i_hwnd, g.m_isInMenu, &className, &titleName);
  
  NotifySetFocus *nfc = new NotifySetFocus;
  nfc->m_type = i_type;
  nfc->m_threadId = GetCurrentThreadId();
  nfc->m_hwnd = i_hwnd;
  tcslcpy(nfc->m_className, className.c_str(), NUMBER_OF(nfc->m_className));
  tcslcpy(nfc->m_titleName, titleName.c_str(), NUMBER_OF(nfc->m_titleName));

  NotifyShow::Show show = NotifyShow::Show_Normal;
  NotifyShow::Show showMDI = NotifyShow::Show_Normal;
  while (i_hwnd)
  {
    LONG exStyle = GetWindowLong(i_hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_MDICHILD)
    {
      WINDOWPLACEMENT placement;
      placement.length = sizeof(WINDOWPLACEMENT);
      if (GetWindowPlacement(i_hwnd, &placement))
      {
	 switch (placement.showCmd)
	 {
	   case SW_SHOWMAXIMIZED:
	     showMDI = NotifyShow::Show_Maximized;
	     break;
	   case SW_SHOWMINIMIZED:
	     showMDI = NotifyShow::Show_Minimized;
	     break;
	   case SW_SHOWNORMAL:
	   default:
	     showMDI = NotifyShow::Show_Normal;
	     break;
	 }
      }
    }

    LONG style = GetWindowLong(i_hwnd, GWL_STYLE);
    if ((style & WS_CHILD) == 0)
    {
      WINDOWPLACEMENT placement;
      placement.length = sizeof(WINDOWPLACEMENT);
      if (GetWindowPlacement(i_hwnd, &placement))
      {
	 switch (placement.showCmd)
	 {
	   case SW_SHOWMAXIMIZED:
	     show = NotifyShow::Show_Maximized;
	     break;
	   case SW_SHOWMINIMIZED:
	     show = NotifyShow::Show_Minimized;
	     break;
	   case SW_SHOWNORMAL:
	   default:
	     show = NotifyShow::Show_Normal;
	     break;
	 }
      }
    }  
    i_hwnd = GetParent(i_hwnd);
  }
  notifyShow(showMDI, true);
  notifyShow(show, false);

  notify(nfc, sizeof(*nfc));
  delete nfc;
}


/// notify WM_SETFOCUS
static void notifySetFocus(bool i_doesForce = false)
{
  HWND hwnd = GetFocus();
  if (i_doesForce || hwnd != g.m_hwndFocus)
  {
    g.m_hwndFocus = hwnd;
    notifyName(hwnd, Notify::Type_setFocus);
  }
}


/// notify sync
static void notifySync()
{
  Notify n;
  n.m_type = Notify::Type_sync;
  notify(&n, sizeof(n));
}


/// notify DLL_THREAD_DETACH
static void notifyThreadDetach()
{
  NotifyThreadDetach ntd;
  ntd.m_type = Notify::Type_threadDetach;
  ntd.m_threadId = GetCurrentThreadId();
  notify(&ntd, sizeof(ntd));
}


/// notify WM_COMMAND, WM_SYSCOMMAND
static void notifyCommand(
  HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
  if (g_hookData->m_doesNotifyCommand)
  {
    NotifyCommand ntc;
    ntc.m_type = Notify::Type_command;
    ntc.m_hwnd = i_hwnd;
    ntc.m_message = i_message;
    ntc.m_wParam = i_wParam;
    ntc.m_lParam = i_lParam;
    notify(&ntc, sizeof(ntc));
  }
}


/// notify show of current window
static void notifyShow(NotifyShow::Show i_show, bool i_isMDI)
{
  NotifyShow ns;
  ns.m_type = Notify::Type_show;
  ns.m_show = i_show;
  ns.m_isMDI = i_isMDI;
  notify(&ns, sizeof(ns));
}


/// notify log
static void notifyLog(_TCHAR *i_msg)
{
  NotifyLog nl;
  nl.m_type = Notify::Type_log;
  tcslcpy(nl.m_msg, i_msg, NUMBER_OF(nl.m_msg));
  notify(&nl, sizeof(nl));
}


/// &Recenter
static void funcRecenter(HWND i_hwnd)
{
  _TCHAR buf[MAX(GANA_MAX_PATH, GANA_MAX_ATOM_LENGTH)];
  GetClassName(i_hwnd, buf, NUMBER_OF(buf));
  bool isEdit;
  if (_tcsicmp(buf, _T("Edit")) == 0)
    isEdit = true;
  else if (_tcsnicmp(buf, _T("RichEdit"), 8) == 0)
    isEdit = false;
  else
    return;	// this function only works for Edit control

  LONG style = GetWindowLong(i_hwnd, GWL_STYLE);
  if (!(style & ES_MULTILINE))
    return;	// this function only works for multi line Edit control

  RECT rc;
  GetClientRect(i_hwnd, &rc);
  POINTL p = { (rc.right + rc.left) / 2, (rc.top + rc.bottom) / 2 };
  int line;
  if (isEdit)
  {
    line = SendMessage(i_hwnd, EM_CHARFROMPOS, 0, MAKELPARAM(p.x, p.y));
    line = HIWORD(line);
  }
  else
  {
    int ci = SendMessage(i_hwnd, EM_CHARFROMPOS, 0, (LPARAM)&p);
    line = SendMessage(i_hwnd, EM_EXLINEFROMCHAR, 0, ci);
  }
  int caretLine = SendMessage(i_hwnd, EM_LINEFROMCHAR, -1, 0);
  SendMessage(i_hwnd, EM_LINESCROLL, 0, caretLine - line);
}


// &SetImeStatus
static void funcSetImeStatus(HWND i_hwnd, int i_status)
{
  HIMC hIMC;

  hIMC = ImmGetContext(i_hwnd);
  if (hIMC == INVALID_HANDLE_VALUE)
    return;

  if (i_status < 0)
    i_status = !ImmGetOpenStatus(hIMC);

  ImmSetOpenStatus(hIMC, i_status);
  ImmReleaseContext(i_hwnd, hIMC);
}


// &SetImeString
static void funcSetImeString(HWND i_hwnd, int i_size)
{
#if defined(_WINNT)
  _TCHAR *buf = new _TCHAR(i_size);
  DWORD len = 0;
  _TCHAR ImeDesc[GANA_MAX_ATOM_LENGTH];
  UINT ImeDescLen;
  DWORD error;
  DWORD denom = 1;
  HANDLE hPipe
    = CreateFile(HOOK_PIPE_NAME, GENERIC_READ,
		 FILE_SHARE_READ, (SECURITY_ATTRIBUTES *)NULL,
		 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
  error = ReadFile(hPipe, buf, i_size, &len, NULL);
  CloseHandle(hPipe);

  ImeDescLen = ImmGetDescription(GetKeyboardLayout(0),
				 ImeDesc, sizeof(ImeDesc));
  if (_tcsncmp(ImeDesc, _T("SKKIME"), ImeDescLen) > 0)
    denom = sizeof(_TCHAR);

  HIMC hIMC = ImmGetContext(i_hwnd);
  if (hIMC == INVALID_HANDLE_VALUE)
    return;

  int status = ImmGetOpenStatus(hIMC);
  ImmSetCompositionString(hIMC, SCS_SETSTR, buf, len / denom, NULL, 0);
  delete buf;
  ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
  if (!status)
    ImmSetOpenStatus(hIMC, status);
  ImmReleaseContext(i_hwnd, hIMC);
#endif // _WINNT
}


/// notify lock state
DllExport void notifyLockState()
{
  NotifyLockState n;
  n.m_type = Notify::Type_lockState;
  n.m_isNumLockToggled = !!(GetKeyState(VK_NUMLOCK) & 1);
  n.m_isCapsLockToggled = !!(GetKeyState(VK_CAPITAL) & 1);
  n.m_isScrollLockToggled = !!(GetKeyState(VK_SCROLL) & 1);
  n.m_isImeLockToggled = g.m_isImeLock;
  n.m_isImeCompToggled = g.m_isImeCompositioning;
  notify(&n, sizeof(n));
}


/// hook of GetMessage
LRESULT CALLBACK getMessageProc(int i_nCode, WPARAM i_wParam, LPARAM i_lParam)
{
  if (!g_hookData)
    return 0;
  
  MSG &msg = (*(MSG *)i_lParam);

  switch (msg.message)
  {
    case WM_COMMAND:
    case WM_SYSCOMMAND:
      notifyCommand(msg.hwnd, msg.message, msg.wParam, msg.lParam);
      break;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
      int nVirtKey = (int)msg.wParam;
      // int repeatCount = (msg.lParam & 0xffff);
      BYTE scanCode   = (BYTE)((msg.lParam >> 16) & 0xff);
      bool isExtended = !!(msg.lParam & (1 << 24));
      // bool isAltDown  = !!(msg.lParam & (1 << 29));
      // bool isKeyup    = !!(msg.lParam & (1 << 31));
      
      if (nVirtKey == VK_CAPITAL ||
	  nVirtKey == VK_NUMLOCK ||
	  nVirtKey == VK_SCROLL)
	notifyLockState();
      else if (scanCode == g_hookData->m_syncKey &&
	       isExtended == g_hookData->m_syncKeyIsExtended)
	notifySync();
      break;
    }
    case WM_IME_STARTCOMPOSITION:
      g.m_isImeCompositioning = true;
      notifyLockState();
      break;
    case WM_IME_ENDCOMPOSITION:
      g.m_isImeCompositioning = false;
      notifyLockState();
      break;
    default:
      if (i_wParam == PM_REMOVE && msg.message == g.m_WM_MAYU_MESSAGE)
      {
	switch (msg.wParam)
	{
	  case MayuMessage_notifyName:
	    notifyName(msg.hwnd);
	    break;
	  case MayuMessage_funcRecenter:
	    funcRecenter(msg.hwnd);
	    break;
	  case MayuMessage_funcSetImeStatus:
	    funcSetImeStatus(msg.hwnd, msg.lParam);
	    break;
	  case MayuMessage_funcSetImeString:
	    funcSetImeString(msg.hwnd, msg.lParam);
	    break;
	}
      }
      break;
  }
  return CallNextHookEx(g_hookData->m_hHookGetMessage,
			i_nCode, i_wParam, i_lParam);
}


/// hook of SendMessage
LRESULT CALLBACK callWndProc(int i_nCode, WPARAM i_wParam, LPARAM i_lParam)
{
  if (!g_hookData)
    return 0;
  
  CWPSTRUCT &cwps = *(CWPSTRUCT *)i_lParam;
  
  if (0 <= i_nCode)
  {
    switch (cwps.message)
    {
      case WM_ACTIVATEAPP:
      case WM_NCACTIVATE:
	if (i_wParam)
	  notifySetFocus();
	break;
      case WM_COMMAND:
      case WM_SYSCOMMAND:
	notifyCommand(cwps.hwnd, cwps.message, cwps.wParam, cwps.lParam);
	break;
      case WM_SIZE:
	updateShow(cwps.hwnd, cwps.wParam);
	break;
      case WM_MOUSEACTIVATE:
	notifySetFocus();
	break;
      case WM_ACTIVATE:
	if (LOWORD(i_wParam) != WA_INACTIVE)
	  notifySetFocus();
	break;
      case WM_ENTERMENULOOP:
	g.m_isInMenu = true;
	notifySetFocus(true);
	break;
      case WM_EXITMENULOOP:
	g.m_isInMenu = false;
	notifySetFocus(true);
	break;
      case WM_SETFOCUS:
	g.m_isInMenu = false;
	notifySetFocus();
	notifyLockState();
	break;
      case WM_IME_STARTCOMPOSITION:
	g.m_isImeCompositioning = true;
	notifyLockState();
	break;
      case WM_IME_ENDCOMPOSITION:
	g.m_isImeCompositioning = false;
	notifyLockState();
	break;
      case WM_IME_NOTIFY:
	if (cwps.wParam == IMN_SETOPENSTATUS)
	  if (HIMC hIMC = ImmGetContext(cwps.hwnd))
	  {
	    g.m_isImeLock = !!ImmGetOpenStatus(hIMC);
	    ImmReleaseContext(cwps.hwnd, hIMC);
	    notifyLockState();
	  }
	break;
    }
  }
  return CallNextHookEx(g_hookData->m_hHookCallWndProc, i_nCode,
			i_wParam, i_lParam);
}


/// install hooks
DllExport int installHooks()
{
  g_hookData->m_hwndTaskTray = NULL;
  g_hookData->m_hHookGetMessage =
    SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)getMessageProc,
		     g.m_hInstDLL, 0);
  g_hookData->m_hHookCallWndProc =
    SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)callWndProc, g.m_hInstDLL, 0);
  return 0;
}


/// uninstall hooks
DllExport int uninstallHooks()
{
  if (g_hookData->m_hHookGetMessage)
    UnhookWindowsHookEx(g_hookData->m_hHookGetMessage);
  g_hookData->m_hHookGetMessage = NULL;
  if (g_hookData->m_hHookCallWndProc)
    UnhookWindowsHookEx(g_hookData->m_hHookCallWndProc);
  g_hookData->m_hHookCallWndProc = NULL;
  return 0;
}
