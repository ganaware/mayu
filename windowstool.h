// ////////////////////////////////////////////////////////////////////////////
// windowstool.h


#ifndef __windowstool_h__
#define __windowstool_h__


#include <string>

#include <windows.h>
#include <shlwapi.h>


/// instance handle of this application
extern HINSTANCE hInst;


// ////////////////////////////////////////////////////////////////////////////
// resource

/// load resource string
extern std::string loadString(UINT id);

/// load small/big icon resource (it must be deleted by DestroyIcon())
extern HICON loadSmallIcon(UINT id);
///
extern HICON loadBigIcon(UINT id);


// ////////////////////////////////////////////////////////////////////////////
// window

/// resize the window (it does not move the window)
extern bool resizeWindow(HWND hwnd, int w, int h, bool doRepaint);

/** get rect of the window in client coordinates.
    @return rect of the window in client coordinates */
extern bool getChildWindowRect(HWND hwnd, RECT *rc);

/** set small icon to the specified window.
    @return handle of previous icon or NULL */
extern HICON setSmallIcon(HWND hwnd, UINT id);
/** set big icon to the specified window.
    @return handle of previous icon or NULL */
extern HICON setBigIcon(HWND hwnd, UINT id);

/// remove icon from a window that is set by setSmallIcon/setBigIcon
extern void unsetSmallIcon(HWND hwnd);
///
extern void unsetBigIcon(HWND hwnd);

/// get toplevel (non-child) window
extern HWND getToplevelWindow(HWND hwnd, bool *isMDI);

/// move window asynchronously
extern void asyncMoveWindow(HWND hwnd, int x, int y);

/// move window asynchronously
extern void asyncMoveWindow(HWND hwnd, int x, int y, int w, int h);

/// resize asynchronously
extern void asyncResize(HWND hwnd, int w, int h);

/// get dll version
extern DWORD getDllVersion(const char *i_dllname);
#define PACKVERSION(major,minor) MAKELONG(minor,major)

// ////////////////////////////////////////////////////////////////////////////
// dialog

/// get/set GWL_USERDATA
template <class T> static T getUserData(HWND hwnd, T *wc)
{ return (*wc = (T)GetWindowLong(hwnd, GWL_USERDATA)); }
///
template <class T> static T setUserData(HWND hwnd, T wc)
{ SetWindowLong(hwnd, GWL_USERDATA, (long)wc); return wc; }


// ////////////////////////////////////////////////////////////////////////////
// RECT

///
inline int rcWidth(const RECT *rc) { return rc->right - rc->left; }
///
inline int rcHeight(const RECT *rc) { return rc->bottom - rc->top; }
///
inline bool isRectInRect(const RECT *rcin, const RECT *rcout)
{
  return (rcout->left <= rcin->left &&
	  rcin->right <= rcout->right &&
	  rcout->top <= rcin->top &&
	  rcin->bottom <= rcout->bottom);
}


// ////////////////////////////////////////////////////////////////////////////
// edit control

/// returns bytes of text
extern size_t editGetTextBytes(HWND hwnd);
/// delete a line
extern void editDeleteLine(HWND hwnd, size_t n);
/// insert text at last
extern void editInsertTextAtLast(HWND hwnd, const std::string &text,
				 size_t threshold);


#endif // __windowstool_h__
