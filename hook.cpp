// ////////////////////////////////////////////////////////////////////////////
// hook.cpp


#define __hook_cpp__

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


DllExport HookData *hookData;		///

static HANDLE hHookData;		///
static HWND hwndFocus = NULL;		///
static HINSTANCE hInstDLL;		///
static bool isInMenu;			///
static UINT WM_Targetted;		///
static bool isImeLock;			///
static bool isImeCompositioning;	///


// ////////////////////////////////////////////////////////////////////////////
// Prototypes


static void notifyThreadDetach();
static bool mapHookData();
static void unmapHookData();


// ////////////////////////////////////////////////////////////////////////////
// Functions


/// EntryPoint
BOOL WINAPI DllMain(HINSTANCE hInstDLL_, DWORD fdwReason,
		    LPVOID /* lpvReserved */)
{
  switch (fdwReason)
  {
    case DLL_PROCESS_ATTACH:
    {
      if (!mapHookData())
	return FALSE;
      hInstDLL = hInstDLL_;
      setlocale(LC_ALL, "");
      WM_Targetted = RegisterWindowMessage(WM_Targetted_name);
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
  hHookData = CreateFileMapping((HANDLE)0xffffffff, NULL, PAGE_READWRITE,
				0, sizeof(HookData), HOOK_DATA_NAME);
  if (!hHookData)
    return false;
  
  hookData =
    (HookData *)MapViewOfFile(hHookData, FILE_MAP_READ | FILE_MAP_WRITE,
				0, 0, sizeof(HookData));
  if (!hookData)
  {
    unmapHookData();
    return false;
  }
  return true;
}


/// unmap hook data
static void unmapHookData()
{
  if (hookData)
    if (!UnmapViewOfFile(hookData))
      return;
  hookData = NULL;
  if (hHookData)
    CloseHandle(hHookData);
  hHookData = NULL;
}


/// notify
DllExport bool notify(void *data, size_t sizeof_data)
{
  HANDLE hMailslot =
    CreateFile(mailslotNotify, GENERIC_WRITE, FILE_SHARE_READ,
	       (SECURITY_ATTRIBUTES *)NULL, OPEN_EXISTING,
	       FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
  if (hMailslot == INVALID_HANDLE_VALUE)
    return false;
    
  DWORD len;
  WriteFile(hMailslot, data, sizeof_data, &len, (OVERLAPPED *)NULL);
  CloseHandle(hMailslot);
  return true;
}


/// get class name and title name
static void getClassNameTitleName(HWND hwnd, bool isInMenu, 
				  StringTool::istring *className_r,
				  string *titleName_r)
{
  StringTool::istring &className = *className_r;
  string &titleName = *titleName_r;
  
  bool isTheFirstTime = true;
  
  if (isInMenu)
  {
    className = titleName = "MENU";
    isTheFirstTime = false;
  }

  while (true)
  {
    char buf[MAX(GANA_MAX_PATH, GANA_MAX_ATOM_LENGTH)];

    // get class name
    if (hwnd)
      GetClassName(hwnd, buf, sizeof(buf));
    else
      GetModuleFileName(GetModuleHandle(NULL), buf, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    if (isTheFirstTime)
      className = buf;
    else
      className = StringTool::istring(buf) + ":" + className;
    
    // get title name
    if (hwnd)
    {
      GetWindowText(hwnd, buf, sizeof(buf));
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
    if (!hwnd)
      break;
    hwnd = GetParent(hwnd);
    isTheFirstTime = false;
  }
}


/// notify WM_Targetted
static void notifyName(HWND hwnd, Notify::Type type = Notify::TypeName)
{
  StringTool::istring className;
  string titleName;
  getClassNameTitleName(hwnd, isInMenu, &className, &titleName);
  
  NotifySetFocus *nfc = new NotifySetFocus;
  nfc->type = type;
  nfc->threadId = GetCurrentThreadId();
  nfc->hwnd = hwnd;
  strncpy(nfc->className, className.c_str(), lengthof(nfc->className));
  nfc->className[lengthof(nfc->className) - 1] = '\0';
  strncpy(nfc->titleName, titleName.c_str(), lengthof(nfc->titleName));
  nfc->titleName[lengthof(nfc->titleName) - 1] = '\0';
  notify(nfc, sizeof(*nfc));
  delete nfc;
}


/// notify WM_SETFOCUS
static void notifySetFocus()
{
  HWND hwnd = GetFocus();
  if (hwnd != hwndFocus)
  {
    hwndFocus = hwnd;
    notifyName(hwndFocus, Notify::TypeSetFocus);
  }
}


/// notify sync
static void notifySync()
{
  Notify n;
  n.type = Notify::TypeSync;
  notify(&n, sizeof(n));
}


/// notify DLL_THREAD_DETACH
static void notifyThreadDetach()
{
  NotifyThreadDetach ntd;
  ntd.type = Notify::TypeThreadDetach;
  ntd.threadId = GetCurrentThreadId();
  notify(&ntd, sizeof(ntd));
}


/// notify WM_COMMAND, WM_SYSCOMMAND
static void notifyCommand(
  HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (hookData->doesNotifyCommand)
  {
    NotifyCommand ntc;
    ntc.type = Notify::TypeCommand;
    ntc.hwnd = hwnd;
    ntc.message = message;
    ntc.wParam = wParam;
    ntc.lParam = lParam;
    notify(&ntc, sizeof(ntc));
  }
}


/// notify lock state
DllExport void notifyLockState()
{
  NotifyLockState n;
  n.type = Notify::TypeLockState;
  n.isNumLockToggled = !!(GetKeyState(VK_NUMLOCK) & 1);
  n.isCapsLockToggled = !!(GetKeyState(VK_CAPITAL) & 1);
  n.isScrollLockToggled = !!(GetKeyState(VK_SCROLL) & 1);
  n.isImeLockToggled = isImeLock;
  n.isImeCompToggled = isImeCompositioning;
  notify(&n, sizeof(n));
}


/// hook of GetMessage
LRESULT CALLBACK getMessageProc(int nCode, WPARAM wParam_, LPARAM lParam_)
{
  if (!hookData)
    return 0;
  
  MSG &msg = (*(MSG *)lParam_);

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
      else if (scanCode == hookData->syncKey &&
	       isExtended == hookData->syncKeyIsExtended)
	notifySync();
      break;
    }
    default:
      if (msg.message == WM_Targetted)
	notifyName(msg.hwnd);
      break;
  }
  return CallNextHookEx(hookData->hhook[0], nCode, wParam_, lParam_);
}


/// hook of SendMessage
LRESULT CALLBACK callWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
  if (!hookData)
    return 0;
  
  CWPSTRUCT &cwps = *(CWPSTRUCT *)lParam;
  
  if (0 <= nCode)
  {
    switch (cwps.message)
    {
      case WM_ACTIVATEAPP:
      case WM_NCACTIVATE:
	if (wParam)
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
	if (LOWORD(wParam) != WA_INACTIVE)
	  notifySetFocus();
	break;
      case WM_ENTERMENULOOP:
	isInMenu = true;
	notifySetFocus();
	break;
      case WM_EXITMENULOOP:
	isInMenu = false;
	notifySetFocus();
	break;
      case WM_SETFOCUS:
	isInMenu = false;
	notifySetFocus();
	notifyLockState();
	break;
      case WM_IME_STARTCOMPOSITION:
	isImeCompositioning = true;
	notifyLockState();
	break;
      case WM_IME_ENDCOMPOSITION:
	isImeCompositioning = false;
	notifyLockState();
	break;
      case WM_IME_NOTIFY:
	if (cwps.wParam == IMN_SETOPENSTATUS)
	  if (HIMC hIMC = ImmGetContext(cwps.hwnd))
	  {
	    isImeLock = !!ImmGetOpenStatus(hIMC);
	    ImmReleaseContext(cwps.hwnd, hIMC);
	    notifyLockState();
	  }
	break;
    }
  }
  return CallNextHookEx(hookData->hhook[1], nCode, wParam, lParam);
}


/// install hooks
DllExport int installHooks()
{
  hookData->hhook[0] =
    SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)getMessageProc,  hInstDLL, 0);
  hookData->hhook[1] =
    SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)callWndProc, hInstDLL, 0);
  return 0;
}


/// uninstall hooks
DllExport int uninstallHooks()
{
  if (hookData->hhook[0])
    UnhookWindowsHookEx(hookData->hhook[0]);
  hookData->hhook[0] = NULL;
  if (hookData->hhook[1])
    UnhookWindowsHookEx(hookData->hhook[1]);
  hookData->hhook[1] = NULL;
  return 0;
}
