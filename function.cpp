// ////////////////////////////////////////////////////////////////////////////
// function.cpp


#include "function.h"
#include "windowstool.h"
#include "engine.h"
#include "hook.h"

#include <algorithm>


/// undefined key
static void undefined(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;
  MessageBeep(MB_OK);
}


/// PostMessage
static void postMessage(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;

  int window = i_fd.m_args[0].getData();
  
  HWND hwnd = i_fd.m_hwnd;
  if (0 < window)
  {
    for (int i = 0; i < window; ++ i)
      hwnd = GetParent(hwnd);
  }
  else if (window == Function::PM_toMainWindow)
  {
    while (true)
    {
      HWND p = GetParent(hwnd);
      if (!p)
	break;
      hwnd = p;
    }
  }
  else if (window == Function::PM_toOverlappedWindow)
  {
    while (hwnd)
    {
      LONG style = GetWindowLong(hwnd, GWL_STYLE);
      if ((style & WS_CHILD) == 0)
	break;
      hwnd = GetParent(hwnd);
    }
  }

  if (hwnd)
    PostMessage(hwnd, i_fd.m_args[1].getNumber(), i_fd.m_args[2].getNumber(),
		i_fd.m_args[3].getNumber());
}


/// virtual key
static void vk(const Function::FuncData &i_fd)
{
  long key = i_fd.m_args[0].getData();
  BYTE vkey = (BYTE)key;
  bool isExtended = !!(key & Function::VK_extended);
  bool isUp       = !i_fd.m_isPressed && !!(key & Function::VK_up);
  bool isDown     = i_fd.m_isPressed && !!(key & Function::VK_down);
  
  if (vkey == VK_LBUTTON && isDown)
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
  else if (vkey == VK_LBUTTON && isUp)
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
  else if (vkey == VK_MBUTTON && isDown)
    mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0);
  else if (vkey == VK_MBUTTON && isUp)
    mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);
  else if (vkey == VK_RBUTTON && isDown)
    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
  else if (vkey == VK_RBUTTON && isUp)
    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
  else if (isUp || isDown)
    keybd_event(vkey, (BYTE)MapVirtualKey((BYTE)vkey, 0),
		(isExtended ? KEYEVENTF_EXTENDEDKEY : 0) |
		(i_fd.m_isPressed ? 0 : KEYEVENTF_KEYUP), 0);
}


/// investigate WM_COMMAND, WM_SYSCOMMAND
static void investigateCommand(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;
  Acquire a(&i_fd.m_engine.m_log, 0);
  g_hookData->m_doesNotifyCommand = !g_hookData->m_doesNotifyCommand;
  if (g_hookData->m_doesNotifyCommand)
    i_fd.m_engine.m_log << _T(" begin") << std::endl;
  else
    i_fd.m_engine.m_log << _T(" end") << std::endl;
}


/// mayu dialog
static void mayuDialog(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;
  PostMessage(i_fd.m_engine.getAssociatedWndow(), WM_APP_engineNotify,
	      EngineNotify_showDlg,
	      i_fd.m_args[0].getData() | i_fd.m_args[1].getData());
}


/// help message
static void helpMessage(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;

  bool doesShow = false;
  if (i_fd.m_args.size() == 2)
  {
    doesShow = true;
    i_fd.m_engine.m_helpMessage = i_fd.m_args[1].getString();
    i_fd.m_engine.m_helpTitle = i_fd.m_args[0].getString();
  }
  PostMessage(i_fd.m_engine.getAssociatedWndow(), WM_APP_engineNotify,
	      EngineNotify_helpMessage, doesShow);
}

/// help variable
static void helpVariable(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;

  _TCHAR buf[20];
  _sntprintf(buf, NUMBER_OF(buf), _T("%d"), i_fd.m_engine.m_variable);

  i_fd.m_engine.m_helpTitle = i_fd.m_args[0].getString();
  i_fd.m_engine.m_helpMessage = buf;
  PostMessage(i_fd.m_engine.getAssociatedWndow(), WM_APP_engineNotify,
	      EngineNotify_helpMessage, true);
}


// input string
//  static void INPUT(const Function::FuncData &i_fd)
//  {
//    if (!i_fd.m_isPressed)
//      return;
//    std::string text = i_fd.m_args[0].getString();
//    for (const char *t = text.c_str(); *t; ++ t)
//      PostMessage(i_fd.m_hwnd, WM_CHAR, *t, 0);
//  }


///
#define WINDOW_ROUTINE					\
  if (!i_fd.m_isPressed)				\
    return;						\
  HWND hwnd = getToplevelWindow(i_fd.m_hwnd, NULL);	\
  if (!hwnd)						\
    return;

///
#define MDI_WINDOW_ROUTINE(i)						  \
  if (!i_fd.m_isPressed)						  \
    return;								  \
  bool isMDI = (i_fd.m_args.size() == i + 1 && i_fd.m_args[i].getData()); \
  HWND hwnd = getToplevelWindow(i_fd.m_hwnd, &isMDI);			  \
  if (!hwnd)								  \
    return;								  \
  RECT rc, rcd;								  \
  if (isMDI)								  \
  {									  \
    getChildWindowRect(hwnd, &rc);					  \
    GetClientRect(GetParent(hwnd), &rcd);				  \
  }									  \
  else									  \
  {									  \
    GetWindowRect(hwnd, &rc);						  \
    SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rcd, FALSE);	  \
  }


/// window cling to ...
static void windowClingTo(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(0);
  int x = rc.left, y = rc.top;
  if (i_fd.m_id == Function::Id_WindowClingToLeft)
    x = rcd.left;
  else if (i_fd.m_id == Function::Id_WindowClingToRight)
    x = rcd.right - rcWidth(&rc);
  else if (i_fd.m_id == Function::Id_WindowClingToTop)
    y = rcd.top;
  else // id == Function::Id_WindowClingToBottom
    y = rcd.bottom - rcHeight(&rc);
  asyncMoveWindow(hwnd, x, y);
}


/// rise window
static void windowRaise(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(0);
  SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
	       SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
}


/// lower window
static void windowLower(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(0);
  SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
	       SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
}


/// minimize window
static void windowMinimize(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(0);
  PostMessage(hwnd, WM_SYSCOMMAND,
	      IsIconic(hwnd) ? SC_RESTORE : SC_MINIMIZE, 0);
}


/// maximize window
static void windowMaximize(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(0);
  PostMessage(hwnd, WM_SYSCOMMAND,
	      IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
}


// screen saver (TODO: this does not work)
//  static void screenSaver(const Function::FuncData &i_fd)
//  {
//    WINDOW_ROUTINE;
//    PostMessage(hwnd, WM_SYSCOMMAND, SC_SCREENSAVE, 0);
//  }


/// close window
static void windowClose(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(0);
  PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
}


/// toggle top most
static void windowToggleTopMost(const Function::FuncData &i_fd)
{
  WINDOW_ROUTINE;
  SetWindowPos(hwnd,
	       (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) ?
	       HWND_NOTOPMOST : HWND_TOPMOST,
	       0, 0, 0, 0,
	       SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
}


/// identify
static void windowIdentify(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;

  _TCHAR className[GANA_MAX_ATOM_LENGTH];
  bool ok = false;
  if (GetClassName(i_fd.m_hwnd, className, NUMBER_OF(className)))
  {
    if (_tcsicmp(className, _T("ConsoleWindowClass")) == 0)
    {
      _TCHAR titleName[1024];
      if (GetWindowText(i_fd.m_hwnd, titleName, NUMBER_OF(titleName)) == 0)
	titleName[0] = _T('\0');
      {
	Acquire a(&i_fd.m_engine.m_log, 1);
	i_fd.m_engine.m_log << _T("HWND:\t") << std::hex
			    << (int)i_fd.m_hwnd << std::dec << std::endl;
      }
      Acquire a(&i_fd.m_engine.m_log, 0);
      i_fd.m_engine.m_log << _T("CLASS:\t") << className << std::endl;
      i_fd.m_engine.m_log << _T("TITLE:\t") << titleName << std::endl;

      HWND hwnd = getToplevelWindow(i_fd.m_hwnd, NULL);
      RECT rc;
      GetWindowRect(hwnd, &rc);
      i_fd.m_engine.m_log << _T("Toplevel Window Position/Size: (")
			  << rc.left << _T(", ") << rc.top << _T(") / (")
			  << rcWidth(&rc) << _T("x") << rcHeight(&rc)
			  << _T(")") << std::endl;
      
      SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rc, FALSE);
      i_fd.m_engine.m_log << _T("Desktop Window Position/Size: (")
			  << rc.left << _T(", ") << rc.top << _T(") / (")
			  << rcWidth(&rc) << _T("x") << rcHeight(&rc)
			  << _T(")") << std::endl;

      i_fd.m_engine.m_log << std::endl;
      ok = true;
    }
  }
  if (!ok)
  {
    UINT WM_MAYU_TARGETTED = RegisterWindowMessage(WM_MAYU_TARGETTED_NAME);
    CHECK_TRUE( PostMessage(i_fd.m_hwnd, WM_MAYU_TARGETTED, 0, 0) );
  }
}


/// maximize horizontally or virtically
static void windowHVMaximize(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(0);

  bool isHorizontal = i_fd.m_id == Function::Id_WindowHMaximize;
  typedef std::list<Engine::WindowPosition>::iterator WPiter;
    
  // erase non window
  while (true)
  {
    WPiter i = i_fd.m_engine.m_windowPositions.begin();
    WPiter end = i_fd.m_engine.m_windowPositions.end();
    for (; i != end; ++ i)
      if (!IsWindow((*i).m_hwnd))
	break;
    if (i == end)
      break;
    i_fd.m_engine.m_windowPositions.erase(i);
  }

  // find target
  WPiter i = i_fd.m_engine.m_windowPositions.begin();
  WPiter end = i_fd.m_engine.m_windowPositions.end();
  WPiter target = end;
  for (; i != end; ++ i)
    if ((*i).m_hwnd == hwnd)
    {
      target = i;
      break;
    }
  
  if (IsZoomed(hwnd))
    PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
  else
  {
    Engine::WindowPosition::Mode mode = Engine::WindowPosition::Mode_normal;
    
    if (target != end)
    {
      Engine::WindowPosition &wp = *target;
      rc = wp.m_rc;
      if (wp.m_mode == Engine::WindowPosition::Mode_HV)
	mode = wp.m_mode = isHorizontal ?
	  Engine::WindowPosition::Mode_V : Engine::WindowPosition::Mode_H;
      else if (( isHorizontal &&
		 wp.m_mode == Engine::WindowPosition::Mode_V) ||
	       (!isHorizontal &&
		wp.m_mode == Engine::WindowPosition::Mode_H))
	mode = wp.m_mode = Engine::WindowPosition::Mode_HV;
      else
	i_fd.m_engine.m_windowPositions.erase(target);
    }
    else
    {
      mode = isHorizontal ?
	Engine::WindowPosition::Mode_H : Engine::WindowPosition::Mode_V;
      i_fd.m_engine.m_windowPositions.push_front(
	Engine::WindowPosition(hwnd, rc, mode));
    }
    
    if ((int)mode & (int)Engine::WindowPosition::Mode_H)
      rc.left = rcd.left, rc.right = rcd.right;
    if ((int)mode & (int)Engine::WindowPosition::Mode_V)
      rc.top = rcd.top, rc.bottom = rcd.bottom;
    
    asyncMoveWindow(hwnd, rc.left, rc.top, rcWidth(&rc), rcHeight(&rc));
  }
}


/// move window
static void windowMove(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(2);
  asyncMoveWindow(hwnd, rc.left + i_fd.m_args[0].getNumber(),
		  rc.top  + i_fd.m_args[1].getNumber());
}


/// move window visibly
static void windowMoveVisibly(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(0);
  int x = rc.left, y = rc.top;
  if (rc.left < rcd.left)
    x = rcd.left;
  else if (rcd.right < rc.right)
    x = rcd.right - rcWidth(&rc);
  if (rc.top < rcd.top)
    y = rcd.top;
  else if (rcd.bottom < rc.bottom)
    y = rcd.bottom - rcHeight(&rc);
  asyncMoveWindow(hwnd, x, y);
}


/// initialize layerd window
static BOOL WINAPI
initalizeLayerdWindow(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);

/// SetLayeredWindowAttributes API
static BOOL (WINAPI *SetLayeredWindowAttributes_)
  (HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags)
  = initalizeLayerdWindow;

// initialize layerd window
static BOOL WINAPI initalizeLayerdWindow(
  HWND i_hwnd, COLORREF i_crKey, BYTE i_bAlpha, DWORD i_dwFlags)
{
  HMODULE hModule = GetModuleHandle(_T("user32.dll"));
  if (!hModule)
    return FALSE;
  SetLayeredWindowAttributes_ =
    (BOOL (WINAPI *)(HWND, COLORREF, BYTE, DWORD))
    GetProcAddress(hModule, "SetLayeredWindowAttributes");
  if (SetLayeredWindowAttributes_)
    return SetLayeredWindowAttributes_(i_hwnd, i_crKey, i_bAlpha, i_dwFlags);
  else
    return FALSE;
}

//static const LONG LWA_ALPHA_ = 0x00000002;		///
//static const DWORD WS_EX_LAYERED_ = 0x00080000;		///


/// set window alpha
static void windowSetAlpha(const Function::FuncData &i_fd)
{
  WINDOW_ROUTINE;
  
  int alpha = i_fd.m_args[0].getNumber();

  if (alpha < 0)	// remove all alpha
  {
    for (Engine::WindowsWithAlpha::iterator
	   i = i_fd.m_engine.m_windowsWithAlpha.begin(); 
	 i != i_fd.m_engine.m_windowsWithAlpha.end(); ++ i)
    {
      SetWindowLong(*i, GWL_EXSTYLE,
		    GetWindowLong(*i, GWL_EXSTYLE) & ~WS_EX_LAYERED);
      RedrawWindow(*i, NULL, NULL,
		   RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    i_fd.m_engine.m_windowsWithAlpha.clear();
  }
  else
  {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_LAYERED)	// remove alpha
    {
      Engine::WindowsWithAlpha::iterator
	i = std::find(i_fd.m_engine.m_windowsWithAlpha.begin(),
		      i_fd.m_engine.m_windowsWithAlpha.end(), hwnd);
      if (i == i_fd.m_engine.m_windowsWithAlpha.end())
	return;	// already layered by the application
    
      i_fd.m_engine.m_windowsWithAlpha.erase(i);
    
      SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
    }
    else	// add alpha
    {
      SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
      alpha %= 101;
      if (!SetLayeredWindowAttributes_(hwnd, 0,
				       (BYTE)(255 * alpha / 100), LWA_ALPHA))
      {
	Acquire a(&i_fd.m_engine.m_log, 0);
	i_fd.m_engine.m_log << _T("error: &WindowSetAlpha(") << alpha
			    << _T(") failed for HWND: ") << std::hex
			    << hwnd << std::dec << std::endl;
	return;
      }
      i_fd.m_engine.m_windowsWithAlpha.push_front(hwnd);
    }
    RedrawWindow(hwnd, NULL, NULL,
		 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
  }
}


/// redraw
static void windowRedraw(const Function::FuncData &i_fd)
{
  WINDOW_ROUTINE;
  RedrawWindow(hwnd, NULL, NULL,
	       RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}


/// resize
static void windowResizeTo(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(2);
  int w = i_fd.m_args[0].getNumber();
  if (w == 0)
    w = rcWidth(&rc);
  else if (w < 0)
    w += rcWidth(&rcd);
  
  int h = i_fd.m_args[1].getNumber();
  if (h == 0)
    h = rcHeight(&rc);
  else if (h < 0)
    h += rcHeight(&rcd);
  
  asyncResize(hwnd, w, h);
}

/// move
static void windowMoveTo(const Function::FuncData &i_fd)
{
  MDI_WINDOW_ROUTINE(3);
  int arg_x = i_fd.m_args[1].getNumber(), arg_y = i_fd.m_args[2].getNumber();
  int gravity = i_fd.m_args[0].getData();
  int x = rc.left + arg_x, y = rc.top + arg_y;

  if (gravity & Function::Gravity_N)
    y = arg_y + rcd.top;
  if (gravity & Function::Gravity_E)
    x = arg_x + rcd.right - rcWidth(&rc);
  if (gravity & Function::Gravity_W)
    x = arg_x + rcd.left;
  if (gravity & Function::Gravity_S)
    y = arg_y + rcd.bottom - rcHeight(&rc);
  asyncMoveWindow(hwnd, x, y);
}


/// move mouse
static void mouseMove(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;
  POINT pt;
  GetCursorPos(&pt);
  SetCursorPos(pt.x + i_fd.m_args[0].getNumber(), pt.y + i_fd.m_args[1].getNumber());
}


/// move wheel
static void mouseWheel(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;
  mouse_event(MOUSEEVENTF_WHEEL, 0, 0, i_fd.m_args[0].getNumber(), 0);
}


/// get clipboard text (you must call closeClopboard())
static const _TCHAR *getTextFromClipboard(HGLOBAL *o_hdata)
{
  *o_hdata = NULL;
  
  if (!OpenClipboard(NULL))
    return NULL;

#ifdef UNICODE
  *o_hdata = GetClipboardData(CF_UNICODETEXT);
#else
  *o_hdata = GetClipboardData(CF_TEXT);
#endif
  if (!*o_hdata)
    return NULL;
  
  _TCHAR *data = reinterpret_cast<_TCHAR *>(GlobalLock(*o_hdata));
  if (!data)
    return NULL;
  return data;
}


/// close clipboard that opend by getTextFromClipboard()
static void closeClipboard(HGLOBAL i_hdata, HGLOBAL i_hdataNew = NULL)
{
  if (i_hdata)
    GlobalUnlock(i_hdata);
  if (i_hdataNew)
  {
    EmptyClipboard();
#ifdef UNICODE
    SetClipboardData(CF_UNICODETEXT, i_hdataNew);
#else
    SetClipboardData(CF_TEXT, i_hdataNew);
#endif
  }
  CloseClipboard();
}


/// chenge case of the text in the clipboard
static void clipboardChangeCase(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;
  HGLOBAL hdata;
  const _TCHAR *text = getTextFromClipboard(&hdata);
  HGLOBAL hdataNew = NULL;
  if (text)
  {
    int size = static_cast<int>(GlobalSize(hdata));
    hdataNew = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size);
    if (hdataNew)
    {
      if (_TCHAR *dataNew = reinterpret_cast<_TCHAR *>(GlobalLock(hdataNew)))
      {
	std::memcpy(dataNew, text, size);
	_TCHAR *dataEnd = dataNew + size;
	while (dataNew < dataEnd && *dataNew)
	{
	  _TCHAR c = *dataNew;
	  if (_istlead(c))
	    dataNew += 2;
	  else
	    *dataNew++ =
	      (i_fd.m_id == Function::Id_ClipboardUpcaseWord)
	      ? _totupper(c) : _totlower(c);
	}
	GlobalUnlock(hdataNew);
      }
    }
  }
  closeClipboard(hdata, hdataNew);
}


/// copy string to clipboard
static void clipboardCopy(const Function::FuncData &i_fd)
{
  if (!i_fd.m_isPressed)
    return;
  if (!OpenClipboard(NULL))
    return;
  tstringi str = i_fd.m_args[0].getString();
  HGLOBAL hdataNew =
    GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,
		(str.size() + 1) * sizeof(_TCHAR));
  if (!hdataNew)
    return;
  _TCHAR *dataNew = reinterpret_cast<_TCHAR *>(GlobalLock(hdataNew));
  _tcscpy(dataNew, str.c_str());
  GlobalUnlock(hdataNew);
  closeClipboard(NULL, hdataNew);
}


// EmacsEditKillLineFunc.
// clear the contents of the clopboard
// at that time, confirm if it is the result of the previous kill-line
void EmacsEditKillLine::func()
{
  if (!m_buf.empty())
  {
    HGLOBAL g;
    const _TCHAR *text = getTextFromClipboard(&g);
    if (text == NULL || m_buf != text)
      reset();
    closeClipboard(g);
  }
  if (OpenClipboard(NULL))
  {
    EmptyClipboard();
    CloseClipboard();
  }
}


/** if the text of the clipboard is
@doc
<pre>
1: EDIT Control (at EOL C-K): ""            =&gt; buf + "\r\n", Delete   
0: EDIT Control (other  C-K): "(.+)"        =&gt; buf + "\1"             
0: IE FORM TEXTAREA (at EOL C-K): "\r\n"    =&gt; buf + "\r\n"           
2: IE FORM TEXTAREA (other C-K): "(.+)\r\n" =&gt; buf + "\1", Return Left
^retval
</pre>
*/
HGLOBAL EmacsEditKillLine::makeNewKillLineBuf(const _TCHAR *i_data,
					      int *o_retval)
{
  size_t len = m_buf.size();
  len += _tcslen(i_data) + 3;
  
  HGLOBAL hdata = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,
			      len * sizeof(_TCHAR));
  if (!hdata)
    return NULL;
  _TCHAR *dataNew = reinterpret_cast<_TCHAR *>(GlobalLock(hdata));
  *dataNew = _T('\0');
  if (!m_buf.empty())
    _tcscpy(dataNew, m_buf.c_str());
  
  len = _tcslen(i_data);
  if (3 <= len &&
      i_data[len - 2] == _T('\r') && i_data[len - 1] == _T('\n'))
  {
    _tcscat(dataNew, i_data);
    len = _tcslen(dataNew);
    dataNew[len - 2] = _T('\0'); // chomp
    *o_retval = 2;
  }
  else if (len == 0)
  {
    _tcscat(dataNew, _T("\r\n"));
    *o_retval = 1;
  }
  else
  {
    _tcscat(dataNew, i_data);
    *o_retval = 0;
  }
  
  m_buf = dataNew;
  
  GlobalUnlock(hdata);
  return hdata;
}


// EmacsEditKillLinePred
int EmacsEditKillLine::pred()
{
  HGLOBAL g;
  const _TCHAR *text = getTextFromClipboard(&g);
  int retval;
  HGLOBAL hdata = makeNewKillLineBuf(text ? text : _T(""), &retval);
  closeClipboard(g, hdata);
  return retval;
}


///
#define FUNC(id) Id_##id, _T(#id)
///
const Function Function::m_functions[] =
{
  // special
  { FUNC(Default),			NULL, NULL, },
  { FUNC(KeymapParent),			NULL, NULL, },
  { FUNC(KeymapWindow),			NULL, NULL, },
  { FUNC(OtherWindowClass),		NULL, NULL, },
  { FUNC(Prefix),			_T("K&b"), NULL, },
  { FUNC(Keymap),			_T("K") , NULL, },
  { FUNC(Sync),				NULL, NULL, },
  { FUNC(Toggle),			_T("l"), NULL, },
  { FUNC(EditNextModifier),		_T("M"), NULL, },
  { FUNC(Variable),			_T("dd"), NULL, },
  { FUNC(Repeat),			_T("S&d"), NULL, },
  
  // other
  { FUNC(Undefined),			NULL, undefined, },
  { FUNC(Ignore),			NULL, NULL, },
  { FUNC(PostMessage),			_T("wddd"), postMessage, },
  { FUNC(ShellExecute),			_T("ssssW"), NULL, },
  { FUNC(LoadSetting),			NULL, NULL, },
  { FUNC(VK),				_T("V"), vk, },
  { FUNC(Wait),				_T("d"), NULL, },
  { FUNC(InvestigateCommand),		NULL, investigateCommand, },
  { FUNC(MayuDialog),			_T("DW"), mayuDialog, },
  { FUNC(DescribeBindings),		NULL, NULL, },
  { FUNC(HelpMessage),			_T("&ss"), helpMessage, },
  { FUNC(HelpVariable),			_T("s"), helpVariable, },
  //{ FUNC(Input),			1, INPUT, },
  
  // IME
  //{ FUNC(ToggleIME),			0, NULL, },
  //{ FUNC(ToggleNativeAlphanumeric),	0, NULL, },
  //{ FUNC(ToggleKanaRoman),		0, NULL, },
  //{ FUNC(ToggleKatakanaHiragana),	0, NULL, },
  
  // window
  { FUNC(WindowRaise),			_T("&b"), windowRaise, },
  { FUNC(WindowLower),			_T("&b"), windowLower, },
  { FUNC(WindowMinimize),		_T("&b"), windowMinimize, },
  { FUNC(WindowMaximize),		_T("&b"), windowMaximize, },
  { FUNC(WindowHMaximize),		_T("&b"), windowHVMaximize, },
  { FUNC(WindowVMaximize),		_T("&b"), windowHVMaximize, },
  { FUNC(WindowMove),			_T("dd&b"), windowMove, },
  { FUNC(WindowMoveTo),			_T("Gdd&b"), windowMoveTo, },
  { FUNC(WindowMoveVisibly),		_T("&b"), windowMoveVisibly, },
  { FUNC(WindowClingToLeft),		_T("&b"), windowClingTo, },
  { FUNC(WindowClingToRight),		_T("&b"), windowClingTo, },
  { FUNC(WindowClingToTop),		_T("&b"), windowClingTo, },
  { FUNC(WindowClingToBottom),		_T("&b"), windowClingTo, },
  { FUNC(WindowClose),			_T("&b"), windowClose, },
  { FUNC(WindowToggleTopMost),		NULL, windowToggleTopMost, },
  { FUNC(WindowIdentify),		NULL, windowIdentify, },
  { FUNC(WindowSetAlpha),		_T("d"),  windowSetAlpha, },
  { FUNC(WindowRedraw),			NULL, windowRedraw, },
  { FUNC(WindowResizeTo),		_T("dd&b"), windowResizeTo, },
  //{ FUNC(ScreenSaver),		NULL, screenSaver, },

  // mouse
  { FUNC(MouseMove),			_T("dd"), mouseMove, },
  { FUNC(MouseWheel),			_T("d"), mouseWheel, },
  //{ FUNC(MouseScrollIntelliMouse),	0, NULL, },
  //{ FUNC(MouseScrollAcrobatReader),	0, NULL, },
  //{ FUNC(MouseScrollAcrobatReader2),	0, NULL, },
  //{ FUNC(MouseScrollTrack),		0, NULL, },
  //{ FUNC(MouseScrollReverseTrack),	0, NULL, },
  //{ FUNC(MouseScrollTrack2),		0, NULL, },
  //{ FUNC(MouseScrollReverseTrack2),	0, NULL, },
  //{ FUNC(MouseScrollCancel),		0, NULL, },
  //{ FUNC(MouseScrollLock),		0, NULL, },
  //{ FUNC(LButtonInMouseScroll),	0, NULL, },
  //{ FUNC(MButtonInMouseScroll),	0, NULL, },
  //{ FUNC(RButtonInMouseScroll),	0, NULL, },

  // clipboard
  { FUNC(ClipboardUpcaseWord),		NULL, clipboardChangeCase, },
  { FUNC(ClipboardDowncaseWord),	NULL, clipboardChangeCase, },
  { FUNC(ClipboardCopy),		_T("s"), clipboardCopy, },
  
  // EmacsEdit
  { FUNC(EmacsEditKillLinePred),	_T("SS"), NULL, },
  { FUNC(EmacsEditKillLineFunc),	NULL, NULL, },

  { Id_NONE, NULL, NULL, NULL, },
};


// search function in functions
const Function *Function::search(Id i_id)
{
  for (const Function *f = m_functions; f->m_id != Id_NONE; ++ f)
    if (i_id == f->m_id)
      return f;
  return NULL;
}
const Function *Function::search(const tstringi &i_name)
{
  for (const Function *f = m_functions; f->m_id != Id_NONE; ++ f)
    if (i_name == f->m_name)
      return f;
  return NULL;
}

/// stream output
tostream &operator<<(tostream &i_ost, const Function &i_f)
{
  return i_ost << _T("&") << i_f.m_name;
}
