// ////////////////////////////////////////////////////////////////////////////
// setting.h


#ifndef __setting_h__
#define __setting_h__


#include "keymap.h"
#include "parser.h"
#include "multithread.h"

#include <set>


///
class Setting
{
public:
  Keyboard keyboard;		///
  Keymaps keymaps;		///
  KeySeqs keySeqs;		///
  std::set<StringTool::istring> symbols;	///
  std::list<Modifier> modifiers;		///
};


///
class SettingLoader
{
  Setting *setting;			/// loaded setting
  bool isThereAnyError;			/// is there any error ?

  SyncObject *soLog;			/// guard log output stream
  std::ostream *log;			/// log output stream
  
  istring currentFilename;		/// current filename
  
  std::vector<Token> tokens;		/// tokens for current line
  std::vector<Token>::iterator ti;	/// current processing token

  static std::vector<istring> *prefix;	/// prefix terminal symbol
  static size_t refcountof_prefix;	/// reference count of prefix

  Keymap *currentKeymap;		/// current keymap

  std::vector<bool> canRead;		/// for <COND_SYMBOL>

  Modifier defaultAssignModifier;	/// default <ASSIGN_MODIFIER>
  Modifier defaultKeySeqModifier;	/// default <KEYSEQ_MODIFIER>

private:
  bool isEOL();					/// is there no more tokens ?
  Token *getToken();				/// get next token
  Token *lookToken();				/// look next token
  
  void load_LINE();				/// <LINE>
  void load_DEFINE();				/// <DEFINE>
  void load_IF();				/// <IF>
  void load_ELSE(bool isElseIf, const istring &t); /// <ELSE> <ELSEIF>
  bool load_ENDIF(const istring &t);		/// <ENDIF>
  void load_INCLUDE();				/// <INCLUDE>
  void load_SCAN_CODES(Key *key_r);		/// <SCAN_CODES>
  void load_DEFINE_KEY();			/// <DEFINE_KEY>
  void load_DEFINE_MODIFIER();			/// <DEFINE_MODIFIER>
  void load_DEFINE_SYNC_KEY();			/// <DEFINE_SYNC_KEY>
  void load_DEFINE_ALIAS();			/// <DEFINE_ALIAS>
  void load_KEYBOARD_DEFINITION();		/// <KEYBOARD_DEFINITION>
  Modifier load_MODIFIER(Modifier::Type mode,
			 Modifier modifier);	/// <..._MODIFIER>
  Key *load_KEY_NAME();				/// <KEY_NAME>
  void load_KEYMAP_DEFINITION(Token *which);	/// <KEYMAP_DEFINITION>
  KeySeq *load_KEY_SEQUENCE(const istring &name = "",
			    bool isInParen = false);	/// <KEY_SEQUENCE>
  void load_KEY_ASSIGN();			/// <KEY_ASSIGN>
  void load_MODIFIER_ASSIGNMENT();		/// <MODIFIER_ASSIGN>
  void load_LOCK_ASSIGNMENT();			/// <LOCK_ASSIGN>
  void load_KEYSEQ_DEFINITION();		/// <KEYSEQ_DEFINITION>

  /// load
  void load(const istring &filename);

  /// is the filename readable ?
  bool isReadable(const istring &filename) const;

  /// get filename from registry
  bool getFilenameFromRegistry(istring *path_r) const;

  /// get filename
  bool getFilename(const istring &name_, istring *path_r) const;

public:
  ///
  SettingLoader(SyncObject *soLog_, std::ostream *log_);

  /// load setting
  bool load(Setting *setting_r, const istring &filename = "");
};


/// get home directory path
extern bool getHomeDirectory(int index, istring *path_r);

#endif // __setting_h__
