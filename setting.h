// ////////////////////////////////////////////////////////////////////////////
// setting.h


#ifndef _SETTING_H
#  define _SETTING_H


#  include "keymap.h"
#  include "parser.h"
#  include "multithread.h"
#  include <set>


///
class Setting
{
public:
  typedef std::set<tstringi> Symbols;		///
  typedef std::list<Modifier> Modifiers;	/// 
  
public:
  Keyboard m_keyboard;				///
  Keymaps m_keymaps;				///
  KeySeqs m_keySeqs;				///
  Symbols m_symbols;				///
  Modifiers m_modifiers;			///
};

///
namespace Event
{
  ///
  extern Key prefixed;
  ///
  extern Key before_key_down;
  ///
  extern Key after_key_up;
  ///
  extern Key *events[];
}

///
class SettingLoader
{
private:
  typedef std::vector<Token> Tokens;		///
  typedef std::vector<tstringi> Prefixes;	///
  typedef std::vector<bool> CanReadStack;	/// 
  
private:
  Setting *m_setting;				/// loaded setting
  bool m_isThereAnyError;			/// is there any error ?

  SyncObject *m_soLog;				/// guard log output stream
  std::wostream *m_log;				/// log output stream
  
  tstringi m_currentFilename;			/// current filename
  
  Tokens m_tokens;				/// tokens for current line
  Tokens::iterator m_ti;			/// current processing token

  static Prefixes *m_prefixes;			/// prefix terminal symbol
  static size_t m_prefixesRefCcount;		/// reference count of prefix

  Keymap *m_currentKeymap;			/// current keymap

  CanReadStack m_canReadStack;			/// for &lt;COND_SYMBOL&gt;

  Modifier m_defaultAssignModifier;		/** default
                                                    &lt;ASSIGN_MODIFIER&gt; */
  Modifier m_defaultKeySeqModifier;		/** default
                                                    &lt;KEYSEQ_MODIFIER&gt; */

private:
  bool isEOL();					/// is there no more tokens ?
  Token *getToken();				/// get next token
  Token *lookToken();				/// look next token
  
  void load_LINE();				/// &lt;LINE&gt;
  void load_DEFINE();				/// &lt;DEFINE&gt;
  void load_IF();				/// &lt;IF&gt;
  void load_ELSE(bool i_isElseIf,
		 const tstringi &i_token);	/// &lt;ELSE&gt; &lt;ELSEIF&gt;
  bool load_ENDIF(const tstringi &i_token);	/// &lt;ENDIF&gt;
  void load_INCLUDE();				/// &lt;INCLUDE&gt;
  void load_SCAN_CODES(Key *o_key);		/// &lt;SCAN_CODES&gt;
  void load_DEFINE_KEY();			/// &lt;DEFINE_KEY&gt;
  void load_DEFINE_MODIFIER();			/// &lt;DEFINE_MODIFIER&gt;
  void load_DEFINE_SYNC_KEY();			/// &lt;DEFINE_SYNC_KEY&gt;
  void load_DEFINE_ALIAS();			/// &lt;DEFINE_ALIAS&gt;
  void load_KEYBOARD_DEFINITION();		/// &lt;KEYBOARD_DEFINITION&gt;
  Modifier load_MODIFIER(Modifier::Type i_mode,
			 Modifier i_modifier);	/// &lt;..._MODIFIER&gt;
  Key *load_KEY_NAME();				/// &lt;KEY_NAME&gt;
  void load_KEYMAP_DEFINITION(
    const Token *i_which);			/// &lt;KEYMAP_DEFINITION&gt;
  KeySeq *load_KEY_SEQUENCE(const tstringi &i_name = _T(""),
			    bool i_isInParen = false); /// &lt;KEY_SEQUENCE&gt;
  void load_KEY_ASSIGN();			/// &lt;KEY_ASSIGN&gt;
  void load_EVENT_ASSIGN();			/// &lt;EVENT_ASSIGN&gt;
  void load_MODIFIER_ASSIGNMENT();		/// &lt;MODIFIER_ASSIGN&gt;
  void load_LOCK_ASSIGNMENT();			/// &lt;LOCK_ASSIGN&gt;
  void load_KEYSEQ_DEFINITION();		/// &lt;KEYSEQ_DEFINITION&gt;

  /// load
  void load(const tstringi &i_filename);

  /// is the filename readable ?
  bool isReadable(const tstringi &i_filename) const;

  /// get filename
  bool getFilename(const tstringi &i_name,
		   tstringi *o_path) const;

public:
  ///
  SettingLoader(SyncObject *i_soLog, std::wostream *i_log);

  /// load setting
  bool load(Setting *o_setting, const tstringi &i_filename = _T(""));
};


/// get home directory path
extern void getHomeDirectories(std::list<tstringi> *o_path);


#endif // _SETTING_H
