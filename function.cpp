// ////////////////////////////////////////////////////////////////////////////
// function.cpp


#include "function.h"
#include "windowstool.h"
#include "engine.h"
#include "hook.h"

#include <algorithm>


using namespace std;


/// get toplevel (non-child) window
static HWND getToplevelWindow(HWND hwnd, bool *isMDI)
{
  while (hwnd)
  {
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if ((style & WS_CHILD) == 0)
      break;
    if (isMDI && *isMDI)
    {
      LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
      if (exStyle & WS_EX_MDICHILD)
	return hwnd;
    }
    hwnd = GetParent(hwnd);
  }
  if (isMDI)
    *isMDI = false;
  return hwnd;
}


/// move window asynchronously
static void asyncMoveWindow(HWND hwnd, int x, int y)
{
  SetWindowPos(hwnd, NULL, x, y, 0, 0,
	       SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
	       SWP_NOSIZE | SWP_NOZORDER);
}
/// move window asynchronously
static void asyncMoveWindow(HWND hwnd, int x, int y, int w, int h)
{
  SetWindowPos(hwnd, NULL, x, y, w, h,
	       SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
	       SWP_NOZORDER);
}


/// undefined key
static void undefined(const Function::FuncData &fd)
{
  if (!fd.isPressed)
    return;
  MessageBeep(MB_OK);
}


/// PostMessage
static void postMessage(const Function::FuncData &fd)
{
  if (!fd.isPressed)
    return;

  int window = fd.args[0].getData();
  
  HWND hwnd = fd.hwnd;
  if (0 < window)
  {
    for (int i = 0; i < window; i ++)
      hwnd = GetParent(hwnd);
  }
  else if (window == Function::toMainWindow)
  {
    while (true)
    {
      HWND p = GetParent(hwnd);
      if (!p)
	break;
      hwnd = p;
    }
  }
  else if (window == Function::toOverlappedWindow)
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
    PostMessage(hwnd, fd.args[1].getNumber(), fd.args[2].getNumber(),
		fd.args[3].getNumber());
}


/// virtual key
static void vk(const Function::FuncData &fd)
{
  long key = fd.args[0].getData();
  BYTE vkey = (BYTE)key;
  bool isExtended = !!(key & Function::vkExtended);
  bool isUp       = !fd.isPressed && !!(key & Function::vkUp);
  bool isDown     = fd.isPressed && !!(key & Function::vkDown);
  
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
		(fd.isPressed ? 0 : KEYEVENTF_KEYUP), 0);
}


/// investigate WM_COMMAND, WM_SYSCOMMAND
static void investigateCommand(const Function::FuncData &fd)
{
  if (!fd.isPressed)
    return;
  Acquire a(&fd.engine.log, 0);
  hookData->doesNotifyCommand = !hookData->doesNotifyCommand;
  if (hookData->doesNotifyCommand)
    fd.engine.log << " begin" << endl;
  else
    fd.engine.log << " end" << endl;
}


// input string
//  static void INPUT(const Function::FuncData &fd)
//  {
//    if (!fd.isPressed)
//      return;
//    std::string text = fd.args[0].getString();
//    for (const char *t = text.c_str(); *t; t++)
//      PostMessage(fd.hwnd, WM_CHAR, *t, 0);
//  }


///
#define WINDOW_ROUTINE				\
  if (!fd.isPressed)				\
    return;					\
  HWND hwnd = getToplevelWindow(fd.hwnd, NULL);	\
  if (!hwnd)					\
    return;

///
#define MDI_WINDOW_ROUTINE(i)						\
  if (!fd.isPressed)							\
    return;								\
  bool isMDI = (fd.args.size() == i + 1 && fd.args[i].getData());	\
  HWND hwnd = getToplevelWindow(fd.hwnd, &isMDI);			\
  if (!hwnd)								\
    return;


/// window cling to ...
static void windowClingTo(const Function::FuncData &fd)
{
  MDI_WINDOW_ROUTINE(0);
  RECT rc, rcd;
  if (isMDI)
  {
    getChildWindowRect(hwnd, &rc);
    GetClientRect(GetParent(hwnd), &rcd);
  }
  else
  {
    GetWindowRect(hwnd, &rc);
    SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rcd, FALSE);
  }
  int x = rc.left, y = rc.top;
  if (fd.id == Function::WindowClingToLeft)
    x = rcd.left;
  else if (fd.id == Function::WindowClingToRight)
    x = rcd.right - rcWidth(&rc);
  else if (fd.id == Function::WindowClingToTop)
    y = rcd.top;
  else // id == Function::WindowClingToBottom
    y = rcd.bottom - rcHeight(&rc);
  asyncMoveWindow(hwnd, x, y);
}


/// rise window
static void windowRaise(const Function::FuncData &fd)
{
  MDI_WINDOW_ROUTINE(0);
  SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
	       SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
}


/// lower window
static void windowLower(const Function::FuncData &fd)
{
  MDI_WINDOW_ROUTINE(0);
  SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
	       SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
}


/// minimize window
static void windowMinimize(const Function::FuncData &fd)
{
  MDI_WINDOW_ROUTINE(0);
  PostMessage(hwnd, WM_SYSCOMMAND,
	      IsIconic(hwnd) ? SC_RESTORE : SC_MINIMIZE, 0);
}


/// maximize window
static void windowMaximize(const Function::FuncData &fd)
{
  MDI_WINDOW_ROUTINE(0);
  PostMessage(hwnd, WM_SYSCOMMAND,
	      IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
}


// screen saver (TODO: this does not work)
//  static void screenSaver(const Function::FuncData &fd)
//  {
//    WINDOW_ROUTINE;
//    PostMessage(hwnd, WM_SYSCOMMAND, SC_SCREENSAVE, 0);
//  }


/// close window
static void windowClose(const Function::FuncData &fd)
{
  MDI_WINDOW_ROUTINE(0);
  PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
}


/// toggle top most
static void windowToggleTopMost(const Function::FuncData &fd)
{
  WINDOW_ROUTINE;
  SetWindowPos(hwnd,
	       (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) ?
	       HWND_NOTOPMOST : HWND_TOPMOST,
	       0, 0, 0, 0,
	       SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
}


/// identify
static void windowIdentify(const Function::FuncData &fd)
{
  if (!fd.isPressed)
    return;

  char className[GANA_MAX_ATOM_LENGTH];
  bool ok = false;
  if (GetClassName(fd.hwnd, className, sizeof(className)))
  {
    if (StringTool::mbsiequal_(className, "ConsoleWindowClass"))
    {
      char titleName[1024];
      if (GetWindowText(fd.hwnd, titleName, sizeof(titleName)) == 0)
	titleName[0] = '\0';
      {
	Acquire a(&fd.engine.log, 1);
	fd.engine.log << "HWND:\t" << hex << (int)fd.hwnd << dec << endl;
      }
      Acquire a(&fd.engine.log, 0);
      fd.engine.log << "CLASS:\t" << className << endl;
      fd.engine.log << "TITLE:\t" << titleName << endl;
      ok = true;
    }
  }
  if (!ok)
  {
    UINT WM_Targetted = RegisterWindowMessage(WM_Targetted_name);
    _true( PostMessage(fd.hwnd, WM_Targetted, 0, 0) );
  }
}


/// maximize horizontally or virtically
static void windowHVMaximize(const Function::FuncData &fd)
{
  MDI_WINDOW_ROUTINE(0);

  bool isHorizontal = fd.id == Function::WindowHMaximize;
  typedef std::list<Engine::WindowPosition>::iterator WPiter;
    
  // erase non window
  while (true)
  {
    WPiter i = fd.engine.windowPositions.begin();
    WPiter end = fd.engine.windowPositions.end();
    for (; i != end; i ++)
      if (!IsWindow((*i).hwnd))
	break;
    if (i == end)
      break;
    fd.engine.windowPositions.erase(i);
  }

  // find target
  WPiter i = fd.engine.windowPositions.begin();
  WPiter end = fd.engine.windowPositions.end();
  WPiter target = end;
  for (; i != end; i ++)
    if ((*i).hwnd == hwnd)
    {
      target = i;
      break;
    }
  
  if (IsZoomed(hwnd))
    PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
  else
  {
    RECT rc;
    if (isMDI)
      getChildWindowRect(hwnd, &rc);
    else
      GetWindowRect(hwnd, &rc);
    
    Engine::WindowPosition::Mode mode = Engine::WindowPosition::ModeNormal;
    
    if (target != end)
    {
      Engine::WindowPosition &wp = *target;
      rc = wp.rc;
      if (wp.mode == Engine::WindowPosition::ModeHV)
	mode = wp.mode = isHorizontal ?
	  Engine::WindowPosition::ModeV : Engine::WindowPosition::ModeH;
      else if (( isHorizontal && wp.mode == Engine::WindowPosition::ModeV) ||
	       (!isHorizontal && wp.mode == Engine::WindowPosition::ModeH))
	mode = wp.mode = Engine::WindowPosition::ModeHV;
      else
	fd.engine.windowPositions.erase(target);
    }
    else
    {
      mode = isHorizontal ?
	Engine::WindowPosition::ModeH : Engine::WindowPosition::ModeV;
      fd.engine.windowPositions.push_front(
	Engine::WindowPosition(hwnd, rc, mode));
    }
    
    RECT rc_max;
    if (isMDI)
      GetClientRect(GetParent(hwnd), &rc_max);
    else
      SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rc_max, FALSE);
    
    if ((int)mode & (int)Engine::WindowPosition::ModeH)
      rc.left = rc_max.left, rc.right = rc_max.right;
    if ((int)mode & (int)Engine::WindowPosition::ModeV)
      rc.top = rc_max.top, rc.bottom = rc_max.bottom;
    
    asyncMoveWindow(hwnd, rc.left, rc.top, rcWidth(&rc), rcHeight(&rc));
  }
}


/// move window
static void windowMove(const Function::FuncData &fd)
{
  MDI_WINDOW_ROUTINE(2);
  
  RECT rc;
  if (isMDI)
    getChildWindowRect(hwnd, &rc);
  else
    GetWindowRect(hwnd, &rc);
  asyncMoveWindow(hwnd, rc.left + fd.args[0].getNumber(),
		  rc.top  + fd.args[1].getNumber());
}


/// move window visibly
static void windowMoveVisibly(const Function::FuncData &fd)
{
  MDI_WINDOW_ROUTINE(0);
  
  RECT rc, rcd;
  if (isMDI)
  {
    getChildWindowRect(hwnd, &rc);
    GetClientRect(GetParent(hwnd), &rcd);
  }
  else
  {
    GetWindowRect(hwnd, &rc);
    SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rcd, FALSE);
  }
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
  HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags)
{
  HMODULE hModule = GetModuleHandle("user32.dll");
  if (!hModule)
    return FALSE;
  SetLayeredWindowAttributes_ =
    (BOOL (WINAPI *)(HWND, COLORREF, BYTE, DWORD))
    GetProcAddress(hModule, "SetLayeredWindowAttributes");
  if (SetLayeredWindowAttributes_)
    return SetLayeredWindowAttributes_(hwnd, crKey, bAlpha, dwFlags);
  else
    return FALSE;
}

static const LONG LWA_ALPHA_ = 0x00000002;		///
static const DWORD WS_EX_LAYERED_ = 0x00080000;		///


/// set window alpha
static void windowSetAlpha(const Function::FuncData &fd)
{
  WINDOW_ROUTINE;
  
  int alpha = fd.args[0].getNumber();

  if (alpha < 0)	// remove all alpha
  {
    for (Engine::WindowsWithAlpha::iterator
	   i = fd.engine.windowsWithAlpha.begin(); 
	 i != fd.engine.windowsWithAlpha.end(); i ++)
    {
      SetWindowLong(*i, GWL_EXSTYLE,
		    GetWindowLong(*i, GWL_EXSTYLE) & ~WS_EX_LAYERED_);
      RedrawWindow(*i, NULL, NULL,
		   RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    fd.engine.windowsWithAlpha.clear();
  }
  else
  {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_LAYERED_)	// remove alpha
    {
      Engine::WindowsWithAlpha::iterator
	i = find(fd.engine.windowsWithAlpha.begin(),
		 fd.engine.windowsWithAlpha.end(), hwnd);
      if (i == fd.engine.windowsWithAlpha.end())
	return;	// already layered by the application
    
      fd.engine.windowsWithAlpha.erase(i);
    
      SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED_);
    }
    else	// add alpha
    {
      SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED_);
      alpha %= 101;
      if (!SetLayeredWindowAttributes_(hwnd, 0,
				       (BYTE)(255 * alpha / 100), LWA_ALPHA_))
      {
	Acquire a(&fd.engine.log, 0);
	fd.engine.log << "error: &WindowSetAlpha(" << alpha
		      << ") failed for HWND: " << hex << hwnd << dec << endl;
	return;
      }
      fd.engine.windowsWithAlpha.push_front(hwnd);
    }
    RedrawWindow(hwnd, NULL, NULL,
		 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
  }
}


/// redraw
static void windowRedraw(const Function::FuncData &fd)
{
  WINDOW_ROUTINE;
  RedrawWindow(hwnd, NULL, NULL,
	       RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}


/// move mouse
static void mouseMove(const Function::FuncData &fd)
{
  if (!fd.isPressed)
    return;
  POINT pt;
  GetCursorPos(&pt);
  SetCursorPos(pt.x + fd.args[0].getNumber(), pt.y + fd.args[1].getNumber());
}


/// move wheel
static void mouseWheel(const Function::FuncData &fd)
{
  if (!fd.isPressed)
    return;
  mouse_event(MOUSEEVENTF_WHEEL, 0, 0, fd.args[0].getNumber(), 0);
}


/// get clipboard text (you must call closeClopboard())
static char *getTextFromClipboard(HGLOBAL *hdata)
{
  *hdata = NULL;
  
  if (!OpenClipboard(NULL))
    return NULL;
  
  *hdata = GetClipboardData(CF_TEXT);
  if (!*hdata)
    return NULL;
  
  char *data = (char *)GlobalLock(*hdata);
  if (!data)
    return NULL;
  return data;
}


/// close clipboard that opend by getTextFromClipboard()
static void closeClipboard(HGLOBAL hdata, HGLOBAL hdataNew = NULL)
{
  if (hdata)
    GlobalUnlock(hdata);
  if (hdataNew)
  {
    EmptyClipboard();
    SetClipboardData(CF_TEXT, hdataNew);
  }
  CloseClipboard();
}


/// chenge case of the text in the clipboard
static void clipboardChangeCase(const Function::FuncData &fd)
{
  if (!fd.isPressed)
    return;
  HGLOBAL hdata;
  char *text = getTextFromClipboard(&hdata);
  HGLOBAL hdataNew = NULL;
  if (text)
  {
    int size = (int)GlobalSize(hdata);
    hdataNew = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size);
    if (hdataNew)
    {
      char *dataNew = (char *)GlobalLock(hdataNew);
      if (dataNew)
      {
	memcpy(dataNew, text, size);
	char *dataEnd = dataNew + size;
	while (dataNew < dataEnd && *dataNew)
	{
	  char c = (BYTE)*dataNew;
	  if (StringTool::ismbblead_(c))
	    dataNew += 2;
	  else
	    *dataNew++ = (fd.id == Function::ClipboardUpcaseWord)
	      ? StringTool::toupper_(c) : StringTool::tolower_(c);
	}
	GlobalUnlock(hdataNew);
      }
    }
  }
  closeClipboard(hdata, hdataNew);
}


/// copy string to clipboard
static void clipboardCopy(const Function::FuncData &fd)
{
  if (!fd.isPressed)
    return;
  if (!OpenClipboard(NULL))
    return;
  istring str = fd.args[0].getString();
  HGLOBAL hdataNew =
    GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, str.size() + 1);
  if (!hdataNew)
    return;
  char *dataNew = (char *)GlobalLock(hdataNew);
  strcpy(dataNew, str.c_str());
  closeClipboard(NULL, hdataNew);
}


// EmacsEditKillLineFunc.
// clear the contents of the clopboard
// at that time, confirm if it is the result of the previous kill-line
void EmacsEditKillLine::func()
{
  if (!buf.empty())
  {
    HGLOBAL g;
    char *text = getTextFromClipboard(&g);
    if (text == NULL || strcmp(text, buf.c_str()) != 0)
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
\begin{verbatim}
1: EDIT Control (at EOL C-K): ""            => buf + "\r\n", Delete   
0: EDIT Control (other  C-K): "(.+)"        => buf + "\1"             
0: IE FORM TEXTAREA (at EOL C-K): "\r\n"    => buf + "\r\n"           
2: IE FORM TEXTAREA (other C-K): "(.+)\r\n" => buf + "\1", Return Left
^retval
\end{verbatim}
*/
HGLOBAL EmacsEditKillLine::makeNewKillLineBuf(const char *data, int *retval)
{
  int len = buf.size();
  len += strlen(data) + 3;
  
  HGLOBAL hdata = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, len);
  if (!hdata)
    return NULL;
  char *dataNew = (char *)GlobalLock(hdata);
  *dataNew = '\0';
  if (!buf.empty())
    strcpy(dataNew, buf.c_str());
  
  len = strlen(data);
  if (3 <= len &&
      data[len - 2] == '\r' && data[len - 1] == '\n')
  {
    strcat(dataNew, data);
    len = strlen(dataNew);
    dataNew[len - 2] = '\0'; // chomp
    *retval = 2;
  }
  else if (len == 0)
  {
    strcat(dataNew, "\r\n");
    *retval = 1;
  }
  else
  {
    strcat(dataNew, data);
    *retval = 0;
  }
  
  buf = dataNew;
  
  GlobalUnlock(hdata);
  return hdata;
}


// EmacsEditKillLinePred
int EmacsEditKillLine::pred()
{
  HGLOBAL g;
  char *text = getTextFromClipboard(&g);
  int retval;
  HGLOBAL hdata = makeNewKillLineBuf(text ? text : "", &retval);
  closeClipboard(g, hdata);
  return retval;
}


///
#define FUNC(id) id, #id
///
const Function Function::functions[] =
{
  // special
  { FUNC(Default),			NULL, NULL, },
  { FUNC(KeymapParent),			NULL, NULL, },
  { FUNC(OtherWindowClass),		NULL, NULL, },
  { FUNC(Prefix),			"K&b", NULL, },
  { FUNC(Keymap),			"K" , NULL, },
  { FUNC(Sync),				NULL, NULL, },
  { FUNC(Toggle),			"l", NULL, },
  { FUNC(EditNextModifier),		"M", NULL, },
  
  // other
  { FUNC(Undefined),			NULL, undefined, },
  { FUNC(Ignore),			NULL, NULL, },
  { FUNC(PostMessage),			"wddd", postMessage, },
  { FUNC(ShellExecute),			"ssssW", NULL, },
  { FUNC(LoadSetting),			NULL, NULL, },
  { FUNC(VK),				"V", vk, },
  { FUNC(Wait),				"d", NULL, },
  { FUNC(InvestigateCommand),		NULL, investigateCommand, },
  //{ FUNC(Input),			1, INPUT, },
  
  // IME
  //{ FUNC(ToggleIME),			0, NULL, },
  //{ FUNC(ToggleNativeAlphanumeric),	0, NULL, },
  //{ FUNC(ToggleKanaRoman),		0, NULL, },
  //{ FUNC(ToggleKatakanaHiragana),	0, NULL, },
  
  // window
  { FUNC(WindowRaise),			"&b", windowRaise, },
  { FUNC(WindowLower),			"&b", windowLower, },
  { FUNC(WindowMinimize),		"&b", windowMinimize, },
  { FUNC(WindowMaximize),		"&b", windowMaximize, },
  { FUNC(WindowHMaximize),		"&b", windowHVMaximize, },
  { FUNC(WindowVMaximize),		"&b", windowHVMaximize, },
  { FUNC(WindowMove),			"dd&b", windowMove, },
  { FUNC(WindowMoveVisibly),		"&b", windowMoveVisibly, },
  { FUNC(WindowClingToLeft),		"&b", windowClingTo, },
  { FUNC(WindowClingToRight),		"&b", windowClingTo, },
  { FUNC(WindowClingToTop),		"&b", windowClingTo, },
  { FUNC(WindowClingToBottom),		"&b", windowClingTo, },
  { FUNC(WindowClose),			"&b", windowClose, },
  { FUNC(WindowToggleTopMost),		NULL, windowToggleTopMost, },
  { FUNC(WindowIdentify),		NULL, windowIdentify, },
  { FUNC(WindowSetAlpha),		"d",  windowSetAlpha, },
  { FUNC(WindowRedraw),			NULL, windowRedraw, },
  //{ FUNC(ScreenSaver),		NULL, screenSaver, },

  // mouse
  { FUNC(MouseMove),			"dd", mouseMove, },
  { FUNC(MouseWheel),			"d", mouseWheel, },
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
  { FUNC(ClipboardCopy),		"s", clipboardCopy, },
  
  // EmacsEdit
  { FUNC(EmacsEditKillLinePred),	"SS", NULL, },
  { FUNC(EmacsEditKillLineFunc),	NULL, NULL, },

  { NONE, NULL, NULL, NULL, },
};


// search function in functions
const Function *Function::search(Id id)
{
  for (const Function *f = functions; f->id != NONE; f ++)
    if (id == f->id)
      return f;
  return NULL;
}
const Function *Function::search(const istring &name)
{
  for (const Function *f = functions; f->id != NONE; f ++)
    if (name == f->name)
      return f;
  return NULL;
}

