// ////////////////////////////////////////////////////////////////////////////
// function.h


#ifndef __function_h__
#define __function_h__


#include "parser.h"

#include <vector>


///
class Engine;


///
class EmacsEditKillLine
{
  std::string buf;	/// previous kill-line contents

public:
  bool doForceReset;	///
  
private:
  ///
  HGLOBAL makeNewKillLineBuf(const char *data, int *retval);

public:
  ///
  void reset() { buf.resize(0); }
  /** EmacsEditKillLineFunc.
      clear the contents of the clopboard
      at that time, confirm if it is the result of the previous kill-line
  */
  void func();
  /// EmacsEditKillLinePred
  int pred();
};


///
class Function
{
public:
  ///
  enum Id
  {
    NONE = 0,			///

    /// special
    SPECIAL = 0x10000,
    Default,			/// default key
    KeymapParent,		/// parent keymap's key
    OtherWindowClass,	/// search other window class for the key or default
    Prefix,			/// prefix key
    Keymap,			/// other keymap's key
    Sync,			/// sync
    Toggle,			/// toggle lock
    EditNextModifier,		/// edit next user input key's modifier
    
    /// other
    OTHER = 0x20000,
    Undefined,			/// undefined (bell)
    Ignore,			/// ignore
    PostMessage,		/// post message
    ShellExecute,		/// ShellExecute
    LoadSetting,		/// load setting
    VK,				/// virtual key
    Wait,			/// wait
    InvestigateCommand,		/// investigate WM_COMMAND, WM_SYSCOMMAND
    //Input,			/// input string
    
    /// IME
    IME = 0x30000,
    //ToggleIME,		///
    //ToggleNativeAlphanumeric,	///
    //ToggleKanaRoman,		///
    //ToggleKatakanaHiragana,	///
    
    /// window
    Window = 0x40000,
    WindowRaise,		///
    WindowLower,		///
    WindowMinimize,		///
    WindowMaximize,		///
    WindowHMaximize,		///
    WindowVMaximize,		///
    WindowMove,			///
    WindowMoveVisibly,		///
    WindowClingToLeft,		///
    WindowClingToRight,		///
    WindowClingToTop,		///
    WindowClingToBottom,	///
    WindowClose,		///
    WindowToggleTopMost,	///
    WindowIdentify,		///
    WindowSetAlpha,		///
    WindowRedraw,		///
    //ScreenSaver,		///
    
    /// mouse
    Mouse = 0x50000,
    MouseMove,			///
    MouseWheel,			///
    //MouseScrollIntelliMouse,  ///
    //MouseScrollAcrobatReader,	///
    //MouseScrollAcrobatReader2,///
    //MouseScrollTrack,		///
    //MouseScrollReverseTrack,	///
    //MouseScrollTrack2,	///
    //MouseScrollReverseTrack2,	///
    //MouseScrollCancel,	///
    //MouseScrollLock,		///
    //LButtonInMouseScroll,	///
    //MButtonInMouseScroll,	///
    //RButtonInMouseScroll,	///

    /// clipboard
    Clipboard = 0x60000,
    ClipboardUpcaseWord,	///
    ClipboardDowncaseWord,	///
    ClipboardCopy,		///
    
    /// EmacsEdit
    EmacsEdit = 0x70000,
    EmacsEditKillLinePred,	///
    EmacsEditKillLineFunc,	///
  };

  /** for VK
      @name ANONYMOUS1 */
  enum
  {
    vkExtended = 0x100,		///
    vkUp       = 0x200,		///
    vkDown     = 0x400,		///
  };

  /** for PostMessage
      @name ANONYMOUS2 */
  enum
  {
    toBegin = -2,		///
    toMainWindow = -2,		///
    toOverlappedWindow = -1,	///
    toItself = 0,		///
    toParentWindow = 1,		///
  };

  ///
  class FuncData
  {
  public:
    Id id;		///
    const std::vector<Token> &args;	///
    HWND hwnd;		///
    bool isPressed;	///
    Engine &engine;	///
    /// uha
    FuncData(const std::vector<Token> &args_, Engine &engine_)
      : args(args_), engine(engine_) { }
  };
  
  typedef void (*Func)(const FuncData &fd);	///
  
  Id id;	/// function id
  char *name;	/// function name
  /** argument's type.
      'K':keymap, 'S':keyseq, 'l':/lock\d/, 's':string,
      'd':digit, 'V':vkey, 'W':showWindow, 'w':window,
      'M':modifier, 'b':bool,
      '&':rest is optional
  */
  char *argType;
  Func func;	///
  
  static const Function functions[];	///
  
  static const Function *search(Id id);	/// search function in functions
  static const Function *search(const istring &name);	///
};


#endif // __function_h__

