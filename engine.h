// ////////////////////////////////////////////////////////////////////////////
// engine.h


#ifndef _ENGINE_H
#  define _ENGINE_H

#  include "multithread.h"
#  include "setting.h"
#  include "msgstream.h"

#  include <set>

///
#  define WM_engineNotify (WM_APP + 110)
/** @name ANONYMOUS */
enum {
  engineNotify_shellExecute,			///
  engineNotify_loadSetting,			///
  engineNotify_showDlg,				///
  engineNotify_helpMessage,			///
};


///
class Engine
{
  CriticalSection cs;		/// criticalSection
  
  // setting
  HWND hwndAssocWindow;		/// associated window (we post message to it)
  Setting * volatile setting;	/// setting
  
  // engine thread state
  HANDLE device;		/// mayu device
  bool didMayuStartDevice;	/// Did the mayu start the mayu-device ?
  HANDLE eEvent;		/// event for engine thread
  bool volatile doForceTerminate; /// terminate engine thread
  bool volatile isLogMode;	/// is logging mode ?
  bool volatile isEnabled;	/// is enabled  ?
  bool volatile isSynchronizing;/// is synchronizing ?
  HANDLE eSync;			/// event for synchronization
  int generateKeyboardEventsRecursionGuard;/// guard against too many recursion
  /** @name ANONYMOUS */
  enum {
    maxGenerateKeyboardEventsRecursion = 64,		///
  };

  // current key state
  Modifier /*volatile*/ currentLock;	/// current lock key's state
  int currentKeyPressCount;	/// how many keys are pressed phisically ?
  int currentKeyPressCountOnWin32; /// how many keys are pressed on win32 ?
  Key *lastPressedKey;		/// last pressed key
  Key *oneShotKey;		/// one shot key
  bool isPrefix;		/// is prefix ?
  bool doesIgnoreModifierForPrefix;/// does ignore modifier key when prefixed ?
  bool doesEditNextModifier;	/// does edit next user input key's modifier ?
  Modifier modifierForNextKey;	/// modifier for next key if above is true

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
  Keymap * volatile currentKeymap; // current keymap
  
  /// focus of a thread
  class FocusOfThread
  {
  public:
    DWORD threadId;			/// thread id
    HWND hwndFocus;			/// window that has focus on the thread
    istring className;			/// class name of hwndFocus
    istring titleName;			/// title name of hwndFocus
    bool isConsole;			/// is hwndFocus console ?
    std::list<Keymap *> keymaps;	/// keymaps
    ///
    FocusOfThread() : threadId(0), hwndFocus(NULL), isConsole(false) { }
  };
  typedef std::map<DWORD /*ThreadId*/, FocusOfThread> FocusOfThreads;	///
  FocusOfThreads /*volatile*/ focusOfThreads;			///
  FocusOfThread * volatile currentFocusOfThread;		///
  FocusOfThread globalFocus;	///
  HWND hwndFocus;		/// current focus window
  typedef std::list<DWORD /*ThreadId*/> DetachedThreadIds;	///
  DetachedThreadIds detachedThreadIds;				///
  
  // for functions
  EmacsEditKillLine emacsEditKillLine;		/// for &amp;EmacsEditKillLine
  ActionFunction *afShellExecute;		/// for &amp;ShellExecute
  
  /// current status in generateKeyboardEvents
  class Current
  {
  public:
    Keymap *keymap;		/// current keymap
    ModifiedKey mkey;		/// current processing key that user inputed
    				/// index in currentFocusOfThread-&gt;keymaps
    std::list<Keymap *>::iterator i;
    ///
    bool isPressed() const { return mkey.modifier.isOn(Modifier::Down); }
  };
  
  /// part of keySeq
  enum Part
  {
    PartAll,		///
    PartUp,		///
    PartDown,		///
  };

public:
  /// window positon for &amp;WindowHMaximize, &amp;WindowVMaximize
  class WindowPosition
  {
  public:
    HWND hwnd;		///
    RECT rc;		///
    ///
    enum Mode {
      ModeNormal,	///
      ModeH,		///
      ModeV,		///
      ModeHV,		///
    };
    Mode mode;		///
    ///
    WindowPosition(HWND hwnd_, const RECT &rc_, Mode mode_)
      : hwnd(hwnd_), rc(rc_), mode(mode_) { }
  };
  std::list<WindowPosition> windowPositions; ///
  
  typedef std::list<HWND> WindowsWithAlpha; /// windows for &amp;WindowSetAlpha
  WindowsWithAlpha windowsWithAlpha; ///
  
  std::string m_helpMessage;			/// for &amp;HelpMessage
  std::string m_helpTitle;			/// for &amp;HelpMessage
  int m_variable;				/// for &amp;Variable,
						///  &amp;Repeat

private:
  /// keyboard handler thread
  static void keyboardHandler(void *This);
  ///
  void keyboardHandler();

  /// check focus window
  void checkFocusWindow();
  /// is modifier pressed ?
  bool isPressed(Modifier::Type mt);
  /// fix modifier key
  bool fixModifierKey(ModifiedKey *mkey_r, Keymap::AssignMode *am_r);

  /// output to log
  void outputToLog(const Key *key, const ModifiedKey &mkey, int debugLevel);

  /// genete modifier events
  void generateModifierEvents(const Modifier &mod);
  
  /// genete event
  void generateEvents(Current i_c, Keymap *i_keymap, Key *i_event);
  
  /// generate keyboard event
  void generateKeyEvent(Key *key, bool doPress, bool isByAssign);
  ///
  void generateActionEvents(const Current &c, const Action *a, bool doPress);
  ///
  void generateKeySeqEvents(const Current &c, const KeySeq *keySeq, Part part);
  ///
  void generateKeyboardEvents(const Current &c);
  ///
  void generateKeyboardEvents(const Current &c, bool isModifier);
  
  /// pop all pressed key on win32
  void keyboardResetOnWin32();
  
  /// get current modifiers
  Modifier getCurrentModifiers(bool isPressed_);

  // describe bindings
  void describeBindings();
  
public:
  /// log stream (output to log dialog's edit)
  omsgstream &log;
  
public:
  ///
  Engine(omsgstream &log_);
  ///
  ~Engine();

  /// start/stop keyboard handler thread
  void start();
  ///
  void stop();

  /// logging mode
  void enableLogMode(bool isLogMode_ = true) { isLogMode = isLogMode_; }
  ///
  void disableLogMode() { isLogMode = false; }
  
  /// enable/disable engine
  void enable(bool isEnabled_ = true) { isEnabled = isEnabled_; }
  ///
  void disable() { isEnabled = false; }
  ///
  bool getIsEnabled() const { return isEnabled; }

  /// associated window
  void setAssociatedWndow(HWND hwnd) { hwndAssocWindow = hwnd; }
  
  /// associated window
  HWND getAssociatedWndow() const { return hwndAssocWindow; }
  
  /// setting
  bool setSetting(Setting *setting_);

  /// focus
  bool setFocus(HWND hwndFocus, DWORD threadId, 
		const char *className, const char *titleName, bool isConsole);

  /// lock state
  bool setLockState(bool isNumLockToggled,
		    bool isCapsLockToggled,
		    bool isScrollLockToggled,
		    bool isImeLockToggled,
		    bool isImeCompToggled);

  /// sync
  bool syncNotify();

  /// thread detach notify
  bool threadDetachNotify(DWORD threadId);

  /// shell execute
  void shellExecute();

  /// get help message
  void getHelpMessages(std::string *o_helpMessage, std::string *o_helpTitle);

  /// command notify
  void commandNotify(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};


#endif // _ENGINE_H
