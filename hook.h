// ////////////////////////////////////////////////////////////////////////////
// hook.h


#ifndef __hook_h__
#define __hook_h__


#include "misc.h"

///
#define mailslotNotify							  \
"\\\\.\\mailslot\\GANAware\\mayu\\{330F7914-EB5B-49be-ACCE-D2B8DF585B32}" \
VERSION
///
#define WM_Targetted_name "GANAware\\mayu\\WM_Targetted"


///
class Notify
{
public:
  ///
  enum Type
  {
    TypeMayuExit,	/// Notify
    TypeSetFocus,	/// NotifySetFocus
    TypeName,		/// NotifySetFocus
    TypeLockState,	/// NotifyLockState
    TypeSync,		/// Notify
    TypeThreadDetach,	/// NotifyThreadDetach
    TypeCommand,	/// NotifyThreadDetach
  };
  Type type;		///
};


///
class NotifySetFocus
{
public:
  Notify::Type type;			///
  DWORD threadId;			///
  HWND hwnd;				///
  char className[GANA_MAX_PATH];	///
  char titleName[GANA_MAX_PATH];	///
};


///
class NotifyLockState
{
public:
  Notify::Type type;		///
  bool isNumLockToggled;	///
  bool isCapsLockToggled;	///
  bool isScrollLockToggled;	///
  bool isImeLockToggled;	///
  bool isImeCompToggled;	///
};


///
class NotifyThreadDetach
{
public:
  Notify::Type type;		///
  DWORD threadId;		///
};


///
class NotifyCommand
{
public:
  Notify::Type type;	///
  HWND hwnd;		///
  UINT message;		///
  WPARAM wParam;	///
  LPARAM lParam;	///
};


/** @name ANONYMOUS */
enum {
  notifyMessageSize = sizeof(NotifySetFocus),	///
};


///
class HookData
{
public:
  HHOOK hhook[2];		///
  BYTE syncKey;			///
  bool syncKeyIsExtended;	///
  bool doesNotifyCommand;	///
};


///
#define DllExport __declspec(dllexport)
///
#define DllImport __declspec(dllimport)


#ifndef __hook_cpp__
extern DllImport HookData *hookData;
extern DllImport int installHooks();
extern DllImport int uninstallHooks();
extern DllImport bool notify(void *data, size_t sizeof_data);
extern DllImport void notifyLockState();
#endif __hook_cpp__


#endif // __hook_h__
