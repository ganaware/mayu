// ////////////////////////////////////////////////////////////////////////////
// focus.h


#ifndef __focus_h__
#define __focus_h__


#include <windows.h>


///
extern ATOM Register_focus();
///
#define WM_focusNotify (WM_APP + 103)
///
#define WM_vkeyNotify (WM_APP + 104)


#endif // __focus_h__
