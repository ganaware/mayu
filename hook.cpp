// ////////////////////////////////////////////////////////////////////////////
// hook.cpp


#define _HOOK_CPP

#include "misc.h"

#include "hook.h"
#include "stringtool.h"

#include <locale.h>
#include <imm.h>


///
#define HOOK_DATA_NAME "{08D6E55C-5103-4e00-8209-A1C4AB13BBEF}" VERSION


using namespace std;


// ////////////////////////////////////////////////////////////////////////////
// Global Variables


DllExport HookData *g_hookData;			///

struct Globals
{
  HANDLE m_hHookData;				///
  HWND m_hwndFocus;				/// 
  HINSTANCE m_hInstDLL;				///
  bool m_isInMenu;				///
  UINT m_WM_MAYU_TARGETTED;			///
  bool m_isImeLock;				///
  bool m_isImeCompositioning;			///
};

static Globals g;


// ////////////////////////////////////////////////////////////////////////////
// Prototypes


static void notifyThreadDetach();
static bool mapHookData();
static void unmapHookData();


// ////////////////////////////////////////////////////////////////////////////
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
      setlocale(LC_ALL, "");
      g.m_WM_MAYU_TARGETTED = RegisterWindowMessage(WM_MAYU_TARGETTED_NAME);
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
  HANDLE hMailslot =
    CreateFile(NOTIFY_MAILSLOT_NAME, GENERIC_WRITE, FILE_SHARE_READ,
	       (SECURITY_ATTRIBUTES *)NULL, OPEN_EXISTING,
	       FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
  if (hMailslot == INVALID_HANDLE_VALUE)
    return false;
    
  DWORD len;
  WriteFile(hMailslot, i_data, i_dataSize, &len, (OVERLAPPED *)NULL);
  CloseHandle(hMailslot);
  return true;
}


/// get class name and title name
static void getClassNameTitleName(HWND i_hwnd, bool i_isInMenu, 
				  StringTool::istring *o_className,
				  string *o_titleName)
{
  StringTool::istring &className = *o_className;
  string &titleName = *o_titleName;
  
  bool isTheFirstTime = true;
  
  if (i_isInMenu)
  {
    className = titleName = "MENU";
    isTheFirstTime = false;
  }

  while (true)
  {
    char buf[MAX(GANA_MAX_PATH, GANA_MAX_ATOM_LENGTH)];

    // get class name
    if (i_hwnd)
      GetClassName(i_hwnd, buf, sizeof(buf));
    else
      GetModuleFileName(GetModuleHandle(NULL), buf, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    if (isTheFirstTime)
      className = buf;
    else
      className = StringTool::istring(buf) + ":" + className;
    
    // get title name
    if (i_hwnd)
    {
      GetWindowText(i_hwnd, buf, sizeof(buf));
      buf[sizeof(buf) - 1] = '\0';
      for (char *b = buf; *b; b++)
	if (StringTool::ismbblead_(*b) && b[1])
	  b ++;
	else if (StringTool::iscntrl_(*b))
	  *b = '?';
    }
    if (isTheFirstTime)
      titleName = buf;
    else
      titleName = string(buf) + ":" + titleName;

    // next loop or exit
    if (!i_hwnd)
      break;
    i_hwnd = GetParent(i_hwnd);
    isTheFirstTime = false;
  }
}


/// notify WM_Targetted
static void notifyName(HWND i_hwnd, Notify::Type i_type = Notify::Type_name)
{
  StringTool::istring className;
  string titleName;
  getClassNameTitleName(i_hwnd, g.m_isInMenu, &className, &titleName);
  
  NotifySetFocus *nfc = new NotifySetFocus;
  nfc->m_type = i_type;
  nfc->m_threadId = GetCurrentThreadId();
  nfc->m_hwnd = i_hwnd;
  strncpy(nfc->m_className, className.c_str(), NUMBER_OF(nfc->m_className));
  nfc->m_className[NUMBER_OF(nfc->m_className) - 1] = '\0';
  strncpy(nfc->m_titleName, titleName.c_str(), NUMBER_OF(nfc->m_titleName));
  nfc->m_titleName[NUMBER_OF(nfc->m_titleName) - 1] = '\0';
  notify(nfc, sizeof(*nfc));
  delete nfc;
}


/// notify WM_SETFOCUS
static void notifySetFocus()
{
  HWND hwnd = GetFocus();
  if (hwnd != g.m_hwndFocus)
  {
    g.m_hwndFocus = hwnd;
    notifyName(g.m_hwndFocus, Notify::Type_setFocus);
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
      if (msg.message == g.m_WM_MAYU_TARGETTED)
	notifyName(msg.hwnd);
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
      case WM_MOUSEACTIVATE:
	notifySetFocus();
	break;
      case WM_ACTIVATE:
	if (LOWORD(i_wParam) != WA_INACTIVE)
	  notifySetFocus();
	break;
      case WM_ENTERMENULOOP:
	g.m_isInMenu = true;
	notifySetFocus();
	break;
      case WM_EXITMENULOOP:
	g.m_isInMenu = false;
	notifySetFocus();
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
