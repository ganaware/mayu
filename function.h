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
    KeymapWindow,		/// keymap's key of current window
    OtherWindowClass,	/// search other window class for the key or default
    Prefix,			/// prefix key
    Keymap,			/// other keymap's key
    Sync,			/// sync
    Toggle,			/// toggle lock
    EditNextModifier,		/// edit next user input key's modifier
    Variable,			/// &amp;Variable(mag, inc)
    Repeat,			/// repeat N times
    
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
    MayuDialog,			/// show mayu dialog box
    DescribeBindings,		/// describe bindings
    HelpMessage,		/// show help message
    HelpVariable,		/// show variable
    //input,			/// input string
    
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
    WindowMoveTo,		///
    WindowClingToLeft,		///
    WindowClingToRight,		///
    WindowClingToTop,		///
    WindowClingToBottom,	///
    WindowClose,		///
    WindowToggleTopMost,	///
    WindowIdentify,		///
    WindowSetAlpha,		///
    WindowRedraw,		///
    WindowResizeTo,		///
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

  /** for WindowMoveTo
      @name ANONYMOUS3 */
  enum
  {
    GravityC = 0,				/// center
    GravityN = 1 << 0,				/// north
    GravityE = 1 << 1,				/// east
    GravityW = 1 << 2,				/// west
    GravityS = 1 << 3,				/// south
    GravityNW = GravityN | GravityW,		/// north west
    GravityNE = GravityN | GravityE,		/// north east
    GravitySW = GravityS | GravityW,		/// south west
    GravitySE = GravityS | GravityE,		/// south east
  };

  /** for MayuDialog
      @name ANONYMOUS3 */
  enum
  {
    mayuDlgInvestigate = 0x1000,		/// 
    mayuDlgLog         = 0x2000,		/// 
    mayuDlgMask        = 0xffff000,		/// 
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
      '<code>&amp;</code>':rest is optional
      '<code>D</code>':dialog,
      '<code>G</code>':gravity,
      '<code>K</code>':keymap,
      '<code>M</code>':modifier,
      '<code>S</code>':keyseq,
      '<code>V</code>':vkey,
      '<code>W</code>':showWindow,
      '<code>b</code>':bool,
      '<code>d</code>':digit,
      '<code>l</code>':/lock\d/,
      '<code>s</code>':string,
      '<code>w</code>':window,
  */
  char *argType;
  Func func;	///
  
  static const Function functions[];	///
  
  static const Function *search(Id id);	/// search function in functions
  static const Function *search(const istring &name);	///

  /// stream output
  friend std::ostream &
  operator<<(std::ostream &i_ost, const Function &i_f);
};


#endif // __function_h__

