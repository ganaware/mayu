//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// hook.h


#ifndef _HOOK_H
#  define _HOOK_H

#  include "misc.h"
#  include <tchar.h>

///
#  define WM_MAYU_MESSAGE_NAME _T("GANAware\\mayu\\WM_MAYU_MESSAGE")

///
enum MayuMessage
{
  MayuMessage_notifyName,
  MayuMessage_funcRecenter,
};


///
struct Notify
{
  ///
  enum Type
  {
    Type_setFocus,				/// NotifySetFocus
    Type_name,					/// NotifySetFocus
    Type_lockState,				/// NotifyLockState
    Type_sync,					/// Notify
    Type_threadDetach,				/// NotifyThreadDetach
    Type_command,				/// NotifyThreadDetach
  };
  Type m_type;					///
};


///
struct NotifySetFocus
{
  Notify::Type m_type;				///
  DWORD m_threadId;				///
  HWND m_hwnd;					///
  _TCHAR m_className[GANA_MAX_PATH];		///
  _TCHAR m_titleName[GANA_MAX_PATH];		///
};


///
struct NotifyLockState
{
  Notify::Type m_type;				///
  bool m_isNumLockToggled;			///
  bool m_isCapsLockToggled;			///
  bool m_isScrollLockToggled;			///
  bool m_isImeLockToggled;			///
  bool m_isImeCompToggled;			///
};


///
struct NotifyThreadDetach
{
  Notify::Type m_type;				///
  DWORD m_threadId;				///
};


///
class NotifyCommand
{
public:
  Notify::Type m_type;				///
  HWND m_hwnd;					///
  UINT m_message;				///
  WPARAM m_wParam;				///
  LPARAM m_lParam;				///
};


enum
{
  NOTIFY_MESSAGE_SIZE = sizeof(NotifySetFocus),	///
};


///
class HookData
{
public:
  HHOOK m_hHookGetMessage;			///
  HHOOK m_hHookCallWndProc;			///
  BYTE m_syncKey;				///
  bool m_syncKeyIsExtended;			///
  bool m_doesNotifyCommand;			///
  HWND m_hwndTaskTray;				///
};


///
#  define DllExport __declspec(dllexport)
///
#  define DllImport __declspec(dllimport)


#  ifndef _HOOK_CPP
extern DllImport HookData *g_hookData;
extern DllImport int installHooks();
extern DllImport int uninstallHooks();
extern DllImport bool notify(void *data, size_t sizeof_data);
extern DllImport void notifyLockState();
#  endif // !_HOOK_CPP


#endif // !_HOOK_H
