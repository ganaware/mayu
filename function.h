// ////////////////////////////////////////////////////////////////////////////
// function.h


#ifndef _FUNCTION_H
#  define _FUNCTION_H

#  include "parser.h"
#  include <vector>


///
class Engine;


///
class EmacsEditKillLine
{
  tstring m_buf;	/// previous kill-line contents

public:
  bool m_doForceReset;	///
  
private:
  ///
  HGLOBAL makeNewKillLineBuf(const _TCHAR *i_data, int *i_retval);

public:
  ///
  void reset() { m_buf.resize(0); }
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
    Id_NONE = 0,				///
    
    /// special
    Id_SPECIAL = 0x10000,
    Id_Default,					/// default key
    Id_KeymapParent,				/// parent keymap's key
    Id_KeymapWindow,				/** keymap's key of current
                                                    window */
    Id_OtherWindowClass,			/** search other window class
                                                    for the key or default */
    Id_Prefix,					/// prefix key
    Id_Keymap,					/// other keymap's key
    Id_Sync,					/// sync
    Id_Toggle,					/// toggle lock
    Id_EditNextModifier,			/** edit next user input key's
                                                    modifier */
    Id_Variable,				/// &amp;Variable(mag, inc)
    Id_Repeat,					/// repeat N times
    
    /// other
    Id_OTHER = 0x20000,
    Id_Undefined,				/// undefined (bell)
    Id_Ignore,					/// ignore
    Id_PostMessage,				/// post message
    Id_ShellExecute,				/// ShellExecute
    Id_LoadSetting,				/// load setting
    Id_VK,					/// virtual key
    Id_Wait,					/// wait
    Id_InvestigateCommand,			/** investigate WM_COMMAND,
						    WM_SYSCOMMAND */
    Id_MayuDialog,				/// show mayu dialog box
    Id_DescribeBindings,			/// describe bindings
    Id_HelpMessage,				/// show help message
    Id_HelpVariable,				/// show variable
    //input,					/// input string
    
    /// IME
    Id_IME = 0x30000,
    //ToggleIME,				///
    //ToggleNativeAlphanumeric,			///
    //ToggleKanaRoman,				///
    //ToggleKatakanaHiragana,			///
    
    /// window
    Id_Window = 0x40000,
    Id_WindowRaise,				///
    Id_WindowLower,				///
    Id_WindowMinimize,				///
    Id_WindowMaximize,				///
    Id_WindowHMaximize,				///
    Id_WindowVMaximize,				///
    Id_WindowMove,				///
    Id_WindowMoveVisibly,			///
    Id_WindowMoveTo,				///
    Id_WindowClingToLeft,			///
    Id_WindowClingToRight,			///
    Id_WindowClingToTop,			///
    Id_WindowClingToBottom,			///
    Id_WindowClose,				///
    Id_WindowToggleTopMost,			///
    Id_WindowIdentify,				///
    Id_WindowSetAlpha,				///
    Id_WindowRedraw,				///
    Id_WindowResizeTo,				///
    //ScreenSaver,				///
    
    /// mouse
    Id_Mouse = 0x50000,
    Id_MouseMove,				///
    Id_MouseWheel,				///
    //MouseScrollIntelliMouse,  		///
    //MouseScrollAcrobatReader,			///
    //MouseScrollAcrobatReader2,		///
    //MouseScrollTrack,				///
    //MouseScrollReverseTrack,			///
    //MouseScrollTrack2,			///
    //MouseScrollReverseTrack2,			///
    //MouseScrollCancel,			///
    //MouseScrollLock,				///
    //LButtonInMouseScroll,			///
    //MButtonInMouseScroll,			///
    //RButtonInMouseScroll,			///
    
    /// clipboard
    Id_Clipboard = 0x60000,
    Id_ClipboardUpcaseWord,			///
    Id_ClipboardDowncaseWord,			///
    Id_ClipboardCopy,				///
    
    /// EmacsEdit
    Id_EmacsEdit = 0x70000,
    Id_EmacsEditKillLinePred,			///
    Id_EmacsEditKillLineFunc,			///
  };

  /** for VK
      @name ANONYMOUS1 */
  enum VK
  {
    VK_extended = 0x100,			///
    VK_up       = 0x200,			///
    VK_down     = 0x400,			///
  };

  /** for PostMessage
      @name ANONYMOUS2 */
  enum PostMessage
  {
    PM_toBegin = -2,				///
    PM_toMainWindow = -2,			///
    PM_toOverlappedWindow = -1,			///
    PM_toItself = 0,				///
    PM_toParentWindow = 1,			///
  };

  /** for WindowMoveTo
      @name ANONYMOUS3 */
  enum Gravity
  {
    Gravity_C = 0,				/// center
    Gravity_N = 1 << 0,				/// north
    Gravity_E = 1 << 1,				/// east
    Gravity_W = 1 << 2,				/// west
    Gravity_S = 1 << 3,				/// south
    Gravity_NW = Gravity_N | Gravity_W,		/// north west
    Gravity_NE = Gravity_N | Gravity_E,		/// north east
    Gravity_SW = Gravity_S | Gravity_W,		/// south west
    Gravity_SE = Gravity_S | Gravity_E,		/// south east
  };

  /** for MayuDialog
      @name ANONYMOUS3 */
  enum MayuDlg
  {
    MayuDlg_investigate = 0x1000,		/// 
    MayuDlg_log         = 0x2000,		/// 
    MayuDlg_mask        = 0xffff000,		/// 
  };


  ///
  class FuncData
  {
  public:
    typedef std::vector<Token> Tokens;		/// 
    
  public:
    Id m_id;					///
    const Tokens &m_args;			///
    HWND m_hwnd;				///
    bool m_isPressed;				///
    Engine &m_engine;				///
    
    /// 
    FuncData(const Tokens &i_args, Engine &i_engine)
      : m_args(i_args), m_engine(i_engine) { }
  };
  
  typedef void (*Func)(const FuncData &i_fd);	///

public:
  Id m_id;					/// function id
  _TCHAR *m_name;				/// function name
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
  _TCHAR *m_argType;
  Func m_func;	///
  
  static const Function m_functions[];	///

public:
  /// search function in functions
  static const Function *search(Id i_id);
  static const Function *search(const tstringi &i_name); ///

  /// stream output
  friend tostream &operator<<(tostream &i_ost, const Function &i_f);
};


#endif // _FUNCTION_H

