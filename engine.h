// ////////////////////////////////////////////////////////////////////////////
// engine.h


#ifndef _ENGINE_H
#  define _ENGINE_H

#  include "multithread.h"
#  include "setting.h"
#  include "msgstream.h"
#  include <set>


enum
{
  ///
  WM_APP_engineNotify = WM_APP + 110,
};

///
enum EngineNotify
{
  EngineNotify_shellExecute,			///
  EngineNotify_loadSetting,			///
  EngineNotify_showDlg,				///
  EngineNotify_helpMessage,			///
};


///
class Engine
{
private:
  enum
  {
    MAX_GENERATE_KEYBOARD_EVENTS_RECURSION_COUNT = 64,		///
  };

  typedef Keymaps::KeymapPtrList KeymapPtrList;
  
  /// focus of a thread
  class FocusOfThread
  {
  public:
    DWORD m_threadId;				/// thread id
    HWND m_hwndFocus;				/** window that has focus on
                                                    the thread */
    tstringi m_className;			/// class name of hwndFocus
    tstringi m_titleName;			/// title name of hwndFocus
    bool m_isConsole;				/// is hwndFocus console ?
    KeymapPtrList m_keymaps;			/// keymaps

  public:
    ///
    FocusOfThread() : m_threadId(0), m_hwndFocus(NULL), m_isConsole(false) { }
  };
  typedef std::map<DWORD /*ThreadId*/, FocusOfThread> FocusOfThreads;	///

  typedef std::list<DWORD /*ThreadId*/> DetachedThreadIds;	///

  /// current status in generateKeyboardEvents
  class Current
  {
  public:
    Keymap *m_keymap;		/// current keymap
    ModifiedKey m_mkey;		/// current processing key that user inputed
    				/// index in currentFocusOfThread-&gt;keymaps
    Keymaps::KeymapPtrList::iterator m_i;

  public:
    ///
    bool isPressed() const
    { return m_mkey.m_modifier.isOn(Modifier::Type_Down); }
  };
  
  /// part of keySeq
  enum Part
  {
    Part_all,					///
    Part_up,					///
    Part_down,					///
  };

public:
  /// window positon for &amp;WindowHMaximize, &amp;WindowVMaximize
  class WindowPosition
  {
  public:
    ///
    enum Mode
    {
      Mode_normal,				///
      Mode_H,					///
      Mode_V,					///
      Mode_HV,					///
    };

  public:
    HWND m_hwnd;				///
    RECT m_rc;					///
    Mode m_mode;				///

  public:
    ///
    WindowPosition(HWND i_hwnd, const RECT &i_rc, Mode i_mode)
      : m_hwnd(i_hwnd), m_rc(i_rc), m_mode(i_mode) { }
  };
  typedef std::list<WindowPosition> WindowPositions;
  
  typedef std::list<HWND> WindowsWithAlpha; /// windows for &amp;WindowSetAlpha
  
private:
  CriticalSection m_cs;				/// criticalSection
  
  // setting
  HWND m_hwndAssocWindow;			/** associated window (we post
                                                    message to it) */
  Setting * volatile m_setting;			/// setting
  
  // engine thread state
  HANDLE m_device;				/// mayu device
  bool m_didMayuStartDevice;			/** Did the mayu start the
                                                    mayu-device ? */
  HANDLE m_eEvent;				/// event for engine thread
  bool volatile m_doForceTerminate;		/// terminate engine thread
  bool volatile m_isLogMode;			/// is logging mode ?
  bool volatile m_isEnabled;			/// is enabled  ?
  bool volatile m_isSynchronizing;		/// is synchronizing ?
  HANDLE m_eSync;				/// event for synchronization
  int m_generateKeyboardEventsRecursionGuard;	/** guard against too many
                                                    recursion */
  
  // current key state
  Modifier m_currentLock;			/// current lock key's state
  int m_currentKeyPressCount;			/** how many keys are pressed
                                                    phisically ? */
  int m_currentKeyPressCountOnWin32;		/** how many keys are pressed
                                                    on win32 ? */
  Key *m_lastPressedKey;			/// last pressed key
  Key *m_oneShotKey;				/// one shot key
  bool m_isPrefix;				/// is prefix ?
  bool m_doesIgnoreModifierForPrefix;		/** does ignore modifier key
                                                    when prefixed ? */
  bool m_doesEditNextModifier;			/** does edit next user input
                                                    key's modifier ? */
  Modifier m_modifierForNextKey;		/** modifier for next key if
                                                    above is true */

  /** current keymaps.
      <dl>
      <dt>when &amp;OtherWindowClass
      <dd>currentKeymap becoms currentKeymaps[++ Current::i]
      <dt>when &amp;KeymapParent
      <dd>currentKeymap becoms currentKeyamp-&gt;parentKeymap
      <dt>other
      <dd>currentKeyamp becoms *Current::i
      </dl>
  */
  Keymap * volatile m_currentKeymap;		/// current keymap
  FocusOfThreads /*volatile*/ m_focusOfThreads;	///
  FocusOfThread * volatile m_currentFocusOfThread; ///
  FocusOfThread m_globalFocus;			///
  HWND m_hwndFocus;				/// current focus window
  DetachedThreadIds m_detachedThreadIds;	///
  
  // for functions
  EmacsEditKillLine m_emacsEditKillLine;	/// for &amp;EmacsEditKillLine
  const ActionFunction *m_afShellExecute;	/// for &amp;ShellExecute
  
public:
  WindowPositions m_windowPositions;		///
  WindowsWithAlpha m_windowsWithAlpha;		///
  
  tstring m_helpMessage;			/// for &amp;HelpMessage
  tstring m_helpTitle;				/// for &amp;HelpMessage
  int m_variable;				/// for &amp;Variable,
						///  &amp;Repeat
  tomsgstream &m_log;				/** log stream (output to log
                                                    dialog's edit) */
  
private:
  /// keyboard handler thread
  static void keyboardHandler(void *i_this);
  ///
  void keyboardHandler();

  /// check focus window
  void checkFocusWindow();
  /// is modifier pressed ?
  bool isPressed(Modifier::Type i_mt);
  /// fix modifier key
  bool fixModifierKey(ModifiedKey *o_mkey, Keymap::AssignMode *o_am);

  /// output to log
  void outputToLog(const Key *i_key, const ModifiedKey &i_mkey,
		   int i_debugLevel);

  /// genete modifier events
  void generateModifierEvents(const Modifier &i_mod);
  
  /// genete event
  void generateEvents(Current i_c, Keymap *i_keymap, Key *i_event);
  
  /// generate keyboard event
  void generateKeyEvent(Key *i_key, bool i_doPress, bool i_isByAssign);
  ///
  void generateActionEvents(const Current &i_c, const Action *i_a,
			    bool i_doPress);
  ///
  void generateKeySeqEvents(const Current &i_c, const KeySeq *i_keySeq,
			    Part i_part);
  ///
  void generateKeyboardEvents(const Current &i_c);
  ///
  void generateKeyboardEvents(const Current &i_c, bool i_isModifier);
  
  /// pop all pressed key on win32
  void keyboardResetOnWin32();
  
  /// get current modifiers
  Modifier getCurrentModifiers(bool i_isPressed);

  // describe bindings
  void describeBindings();
  
public:
  ///
  Engine(tomsgstream &i_log);
  ///
  ~Engine();

  /// start/stop keyboard handler thread
  void start();
  ///
  void stop();

  /// logging mode
  void enableLogMode(bool i_isLogMode = true) { m_isLogMode = i_isLogMode; }
  ///
  void disableLogMode() { m_isLogMode = false; }
  
  /// enable/disable engine
  void enable(bool i_isEnabled = true) { m_isEnabled = i_isEnabled; }
  ///
  void disable() { m_isEnabled = false; }
  ///
  bool getIsEnabled() const { return m_isEnabled; }

  /// associated window
  void setAssociatedWndow(HWND i_hwnd) { m_hwndAssocWindow = i_hwnd; }
  
  /// associated window
  HWND getAssociatedWndow() const { return m_hwndAssocWindow; }
  
  /// setting
  bool setSetting(Setting *i_setting);

  /// focus
  bool setFocus(HWND i_hwndFocus, DWORD i_threadId,
		const tstringi &i_className,
		const tstringi &i_titleName, bool i_isConsole);

  /// lock state
  bool setLockState(bool i_isNumLockToggled, bool i_isCapsLockToggled,
		    bool i_isScrollLockToggled, bool i_isImeLockToggled,
		    bool i_isImeCompToggled);

  /// sync
  bool syncNotify();

  /// thread detach notify
  bool threadDetachNotify(DWORD i_threadId);

  /// shell execute
  void shellExecute();

  /// get help message
  void getHelpMessages(tstring *o_helpMessage, tstring *o_helpTitle);

  /// command notify
  void commandNotify(HWND i_hwnd, UINT i_message, WPARAM i_wParam,
		     LPARAM i_lParam);
};


#endif // _ENGINE_H
