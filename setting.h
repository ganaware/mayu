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

  std::vector<bool> canRead;		/// for &lt;COND_SYMBOL&gt;

  Modifier defaultAssignModifier;	/// default &lt;ASSIGN_MODIFIER&gt;
  Modifier defaultKeySeqModifier;	/// default &lt;KEYSEQ_MODIFIER&gt;

private:
  bool isEOL();					/// is there no more tokens ?
  Token *getToken();				/// get next token
  Token *lookToken();				/// look next token
  
  void load_LINE();				/// &lt;LINE&gt;
  void load_DEFINE();				/// &lt;DEFINE&gt;
  void load_IF();				/// &lt;IF&gt;
  void load_ELSE(bool isElseIf,
		 const istring &t);		/// &lt;ELSE&gt; &lt;ELSEIF&gt;
  bool load_ENDIF(const istring &t);		/// &lt;ENDIF&gt;
  void load_INCLUDE();				/// &lt;INCLUDE&gt;
  void load_SCAN_CODES(Key *key_r);		/// &lt;SCAN_CODES&gt;
  void load_DEFINE_KEY();			/// &lt;DEFINE_KEY&gt;
  void load_DEFINE_MODIFIER();			/// &lt;DEFINE_MODIFIER&gt;
  void load_DEFINE_SYNC_KEY();			/// &lt;DEFINE_SYNC_KEY&gt;
  void load_DEFINE_ALIAS();			/// &lt;DEFINE_ALIAS&gt;
  void load_KEYBOARD_DEFINITION();		/// &lt;KEYBOARD_DEFINITION&gt;
  Modifier load_MODIFIER(Modifier::Type mode,
			 Modifier modifier);	/// &lt;..._MODIFIER&gt;
  Key *load_KEY_NAME();				/// &lt;KEY_NAME&gt;
  void load_KEYMAP_DEFINITION(Token *which);	/// &lt;KEYMAP_DEFINITION&gt;
  KeySeq *load_KEY_SEQUENCE(const istring &name = "",
			    bool isInParen = false); /// &lt;KEY_SEQUENCE&gt;
  void load_KEY_ASSIGN();			/// &lt;KEY_ASSIGN&gt;
  void load_MODIFIER_ASSIGNMENT();		/// &lt;MODIFIER_ASSIGN&gt;
  void load_LOCK_ASSIGNMENT();			/// &lt;LOCK_ASSIGN&gt;
  void load_KEYSEQ_DEFINITION();		/// &lt;KEYSEQ_DEFINITION&gt;

  /// load
  void load(const istring &filename);

  /// is the filename readable ?
  bool isReadable(const istring &filename) const;

  /// get filename
  bool getFilename(const istring &name_, istring *path_r) const;

public:
  ///
  SettingLoader(SyncObject *soLog_, std::ostream *log_);

  /// load setting
  bool load(Setting *setting_r, const istring &filename = "");
};


/// get home directory path
extern void getHomeDirectories(std::list<istring> *o_path);

#endif // __setting_h__
