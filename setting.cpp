// ////////////////////////////////////////////////////////////////////////////
// setting.cpp


#include "misc.h"

#include "dlgsetting.h"
#include "errormessage.h"
#include "mayu.h"
#include "mayurc.h"
#include "registry.h"
#include "setting.h"
#include "windowstool.h"
#include "vkeytable.h"

#include <algorithm>
#include <fstream>
#include <iomanip>

#include <shlwapi.h>


namespace Event
{
  Key prefixed("prefixed");
  Key before_key_down("before-key-down");
  Key after_key_up("before-key-down");
  Key *events[] =
  {
    &prefixed,
    &before_key_down,
    &after_key_up,
    NULL,
  };
}


// get mayu filename
static bool getFilenameFromRegistry(
  istring *o_name, istring *o_filename, std::list<istring> *o_symbols)
{
  Registry reg(MAYU_REGISTRY_ROOT);
  int index;
  reg.read(".mayuIndex", &index, 0);
  char buf[100];
  snprintf(buf, sizeof(buf), ".mayu%d", index);

  istring entry;
  if (!reg.read(buf, &entry))
    return false;
  
  Regexp getFilename("^([^;]*);([^;]*);(.*)$");
  if (!getFilename.doesMatch(entry))
    return false;
  
  if (o_name)
    *o_name = getFilename[1];
  if (o_filename)
    *o_filename = getFilename[2];
  if (o_symbols)
  {
    istring symbols = getFilename[3];
    Regexp symbol("-D([^;]*)");
    while (symbol.doesMatch(symbols))
    {
      o_symbols->push_back(symbol[1]);
      symbols = symbols.substr(symbol.subBegin(1));
    }
  }
  return true;
}


// get home directory path
void getHomeDirectories(std::list<istring> *o_pathes)
{
  istring filename;
  if (getFilenameFromRegistry(NULL, &filename, NULL))
  {
    Regexp getPath("^(.*[/\\\\])[^/\\\\]*$");
    if (getPath.doesMatch(filename))
      o_pathes->push_back(getPath[1]);
  }
  
  const char *home = getenv("HOME");
  if (home)
    o_pathes->push_back(home);

  const char *homedrive = getenv("HOMEDRIVE");
  const char *homepath = getenv("HOMEPATH");
  if (homedrive && homepath)
    o_pathes->push_back(istring(homedrive) + homepath);

  const char *userprofile = getenv("USERPROFILE");
  if (userprofile)
    o_pathes->push_back(userprofile);
  
  char buf[GANA_MAX_PATH];
  if (GetModuleFileName(GetModuleHandle(NULL), buf, NUMBER_OF(buf)))
  {
    PathRemoveFileSpec(buf);
    o_pathes->push_back(buf);
  }
}


using namespace std;


// ////////////////////////////////////////////////////////////////////////////
// SettingLoader


// is there no more tokens ?
bool SettingLoader::isEOL()
{
  return m_ti == m_tokens.end();
}


// get next token
Token *SettingLoader::getToken()
{
  if (isEOL())
    throw ErrorMessage() << "too few words.";
  return &*(m_ti ++);
}

  
// look next token
Token *SettingLoader::lookToken()
{
  if (isEOL())
    throw ErrorMessage() << "too few words.";
  return &*m_ti;
}


// <INCLUDE>
void SettingLoader::load_INCLUDE()
{
  SettingLoader loader(m_soLog, m_log);
  loader.m_defaultAssignModifier = m_defaultAssignModifier;
  loader.m_defaultKeySeqModifier = m_defaultKeySeqModifier;
  if (!loader.load(m_setting, (*getToken()).getString()))
    m_isThereAnyError = true;
}


// <SCAN_CODES>
void SettingLoader::load_SCAN_CODES(Key *o_key)
{
  for (int j = 0; j < Key::MAX_SCAN_CODES_SIZE && !isEOL(); ++ j)
  {
    ScanCode sc;
    sc.m_flags = 0;
    while (true)
    {
      Token *t = getToken();
      if (t->isNumber())
      {
	sc.m_scan = (u_char)t->getNumber();
	o_key->addScanCode(sc);
	break;
      }
      if      (*t == "E0-") sc.m_flags |= ScanCode::E0;
      else if (*t == "E1-") sc.m_flags |= ScanCode::E1;
      else  throw ErrorMessage() << "`" << *t << "': invalid modifier.";
    }
  }
}


// <DEFINE_KEY>
void SettingLoader::load_DEFINE_KEY()
{
  Token *t = getToken();
  Key key;
      
  // <KEY_NAMES>
  if (*t == '(')
  {
    key.addName(getToken()->getString());
    while (t = getToken(), *t != ')')
      key.addName(t->getString());
    if (*getToken() != "=")
      throw ErrorMessage() << "there must be `=' after `)'.";
  }
  else
  {
    key.addName(t->getString());
    while (t = getToken(), *t != "=")
      key.addName(t->getString());
  }

  load_SCAN_CODES(&key);
  m_setting->m_keyboard.addKey(key);
}


// <DEFINE_MODIFIER>
void SettingLoader::load_DEFINE_MODIFIER()
{
  Token *t = getToken();
  Modifier::Type mt;
  if      (*t == "shift"  ) mt = Modifier::Type_Shift;
  else if (*t == "alt"     ||
	   *t == "meta"    ||
	   *t == "menu"   ) mt = Modifier::Type_Alt;
  else if (*t == "control" ||
	   *t == "ctrl"   ) mt = Modifier::Type_Control;
  else if (*t == "windows" ||
	   *t == "win"    ) mt = Modifier::Type_Windows;
  else throw ErrorMessage() << "`" << *t << "': invalid modifier name.";
    
  if (*getToken() != "=")
    throw ErrorMessage() << "there must be `=' after modifier name.";
    
  while (!isEOL())
  {
    t = getToken();
    Key *key =
      m_setting->m_keyboard.searchKeyByNonAliasName(t->getString());
    if (!key)
      throw ErrorMessage() << "`" << *t << "': invalid key name.";
    m_setting->m_keyboard.addModifier(mt, key);
  }
}


// <DEFINE_SYNC_KEY>
void SettingLoader::load_DEFINE_SYNC_KEY()
{
  Key *key = m_setting->m_keyboard.getSyncKey();
  key->initialize();
  key->addName("sync");
  
  if (*getToken() != "=")
    throw ErrorMessage() << "there must be `=' after `sync'.";
  
  load_SCAN_CODES(key);
}


// <DEFINE_ALIAS>
void SettingLoader::load_DEFINE_ALIAS()
{
  Token *name = getToken();
  
  if (*getToken() != "=")
    throw ErrorMessage() << "there must be `=' after `alias'.";

  Token *t = getToken();
  Key *key = m_setting->m_keyboard.searchKeyByNonAliasName(t->getString());
  if (!key)
    throw ErrorMessage() << "`" << *t << "': invalid key name.";
  m_setting->m_keyboard.addAlias(name->getString(), key);
}


// <KEYBOARD_DEFINITION>
void SettingLoader::load_KEYBOARD_DEFINITION()
{
  Token *t = getToken();
    
  // <DEFINE_KEY>
  if (*t == "key") load_DEFINE_KEY();

  // <DEFINE_MODIFIER>
  else if (*t == "mod") load_DEFINE_MODIFIER();
  
  // <DEFINE_SYNC_KEY>
  else if (*t == "sync") load_DEFINE_SYNC_KEY();
  
  // <DEFINE_ALIAS>
  else if (*t == "alias") load_DEFINE_ALIAS();
  
  //
  else throw ErrorMessage() << "syntax error `" << *t << "'.";
}


// <..._MODIFIER>
Modifier SettingLoader::load_MODIFIER(Modifier::Type i_mode,
				      Modifier i_modifier)
{
  Modifier isModifierSpecified;
  enum { PRESS, RELEASE, DONTCARE } flag = PRESS;

  int i;
  for (i = i_mode; i < Modifier::Type_ASSIGN; ++ i)
  {
    i_modifier.dontcare(Modifier::Type(i));
    isModifierSpecified.on(Modifier::Type(i));
  }
  
  Token *t = NULL;
  
  continue_loop:
  while (!isEOL())
  {
    t = lookToken();

    const static struct { const char *s; Modifier::Type mt; } map[] =
    {
      // <BASIC_MODIFIER>
      { "S-",  Modifier::Type_Shift },
      { "A-",  Modifier::Type_Alt },
      { "M-",  Modifier::Type_Alt },
      { "C-",  Modifier::Type_Control },
      { "W-",  Modifier::Type_Windows },
      // <KEYSEQ_MODIFIER>
      { "U-",  Modifier::Type_Up },
      { "D-",  Modifier::Type_Down },
      // <ASSIGN_MODIFIER>
      { "IL-", Modifier::Type_ImeLock },
      { "IC-", Modifier::Type_ImeComp },
      { "I-",  Modifier::Type_ImeComp },
      { "NL-", Modifier::Type_NumLock },
      { "CL-", Modifier::Type_CapsLock },
      { "SL-", Modifier::Type_ScrollLock },
      { "M0-", Modifier::Type_Mod0 },
      { "M1-", Modifier::Type_Mod1 },
      { "M2-", Modifier::Type_Mod2 },
      { "M3-", Modifier::Type_Mod3 },
      { "M4-", Modifier::Type_Mod4 },
      { "M5-", Modifier::Type_Mod5 },
      { "M6-", Modifier::Type_Mod6 },
      { "M7-", Modifier::Type_Mod7 },
      { "M8-", Modifier::Type_Mod8 },
      { "M9-", Modifier::Type_Mod9 },
      { "L0-", Modifier::Type_Lock0 },
      { "L1-", Modifier::Type_Lock1 },
      { "L2-", Modifier::Type_Lock2 },
      { "L3-", Modifier::Type_Lock3 },
      { "L4-", Modifier::Type_Lock4 },
      { "L5-", Modifier::Type_Lock5 },
      { "L6-", Modifier::Type_Lock6 },
      { "L7-", Modifier::Type_Lock7 },
      { "L8-", Modifier::Type_Lock8 },
      { "L9-", Modifier::Type_Lock9 },
    };

    for (int i = 0; i < NUMBER_OF(map); ++ i)
      if (*t == map[i].s)
      {
	getToken();
	Modifier::Type mt = map[i].mt;
	if (static_cast<int>(i_mode) <= static_cast<int>(mt))
	  throw ErrorMessage() << "`" << *t
			       << "': invalid modifier at this context.";
	switch (flag)
	{
	  case PRESS: i_modifier.press(mt); break;
	  case RELEASE: i_modifier.release(mt); break;
	  case DONTCARE: i_modifier.dontcare(mt); break;
	}
	isModifierSpecified.on(mt);
	flag = PRESS;
	goto continue_loop;
      }
      
    if (*t == "*")
    {
      getToken();
      flag = DONTCARE;
      continue;
    }
      
    if (*t == "~")
    {
      getToken();
      flag = RELEASE;
      continue;
    }

    break;
  }
  
  for (i = Modifier::Type_begin; i != Modifier::Type_end; i++)
    if (!isModifierSpecified.isOn(Modifier::Type(i)))
      switch (flag)
      {
	case PRESS: break;
	case RELEASE: i_modifier.release(Modifier::Type(i)); break;
	case DONTCARE: i_modifier.dontcare(Modifier::Type(i)); break;
      }

  // fix up and down
  bool isDontcareUp   = i_modifier.isDontcare(Modifier::Type_Up);
  bool isDontcareDown = i_modifier.isDontcare(Modifier::Type_Down);
  bool isOnUp         = i_modifier.isOn(Modifier::Type_Up);
  bool isOnDown       = i_modifier.isOn(Modifier::Type_Down);
  if (isDontcareUp && isDontcareDown)
    ;
  else if (isDontcareUp)
    i_modifier.on(Modifier::Type_Up, !isOnDown);
  else if (isDontcareDown)
    i_modifier.on(Modifier::Type_Down, !isOnUp);
  else if (isOnUp == isOnDown)
  {
    i_modifier.dontcare(Modifier::Type_Up);
    i_modifier.dontcare(Modifier::Type_Down);
  }
  return i_modifier;
}


// <KEY_NAME>
Key *SettingLoader::load_KEY_NAME()
{
  Token *t = getToken();
  Key *key = m_setting->m_keyboard.searchKey(t->getString());
  if (!key)
    throw ErrorMessage() << "`" << *t << "': invalid key name.";
  return key;
}


// <KEYMAP_DEFINITION>
void SettingLoader::load_KEYMAP_DEFINITION(const Token *i_which)
{
  Keymap::Type type = Keymap::Type_keymap;
  Token *name = getToken();	// <KEYMAP_NAME>
  istring windowClassName;
  istring windowTitleName;
  KeySeq *keySeq = NULL;
  Keymap *parentKeymap = NULL;
  bool isKeymap2 = false;

  if (!isEOL())
  {
    Token *t = lookToken();
    if (*i_which == "window")	// <WINDOW>
    {
      if (t->isOpenParen())
	// "(" <WINDOW_CLASS_NAME> "&&" <WINDOW_TITLE_NAME> ")"
	// "(" <WINDOW_CLASS_NAME> "||" <WINDOW_TITLE_NAME> ")"
      {
	getToken();
	windowClassName = getToken()->getRegexp();
	t = getToken();
	if (*t == "&&")
	  type = Keymap::Type_windowAnd;
	else if (*t == "||")
	  type = Keymap::Type_windowOr;
	else
	  throw ErrorMessage() << "`" << *t << "': unknown operator.";
	windowTitleName = getToken()->getRegexp();
	if (!getToken()->isCloseParen())
	  throw ErrorMessage() << "there must be `)'.";
      }
      else if (t->isRegexp())	// <WINDOW_CLASS_NAME>
      {
	getToken();
	type = Keymap::Type_windowAnd;
	windowClassName = t->getRegexp();
      }
    }
    else if (*i_which == "keymap")
      ;
    else if (*i_which == "keymap2")
      isKeymap2 = true;
    else
      ASSERT(false);
    
    if (!isEOL())
    {
      t = lookToken();
      // <KEYMAP_PARENT>
      if (*t == ":")
      {
	getToken();
	t = getToken();
	parentKeymap = m_setting->m_keymaps.searchByName(t->getString());
	if (!parentKeymap)
	  throw ErrorMessage() << "`" << *t << "': unknown keymap name.";
      }
      if (!isEOL())
      {
	t = getToken();
	if (*t != "=>")
	  throw ErrorMessage() << "`" << *t << "': syntax error.";
	keySeq = SettingLoader::load_KEY_SEQUENCE();
      }
    }
  }

  if (keySeq == NULL)
  {
    Function::Id id;
    if (type == Keymap::Type_keymap && !isKeymap2)
      id = Function::Id_KeymapParent;
    else if (type == Keymap::Type_keymap && !isKeymap2)
      id = Function::Id_Undefined;
    else // (type == Keymap::Type_windowAnd || type == Keymap::Type_windowOr)
      id = Function::Id_KeymapParent;
    ActionFunction af;
    af.m_function = Function::search(id);
    ASSERT( af.m_function );
    keySeq = m_setting->m_keySeqs.add(KeySeq(name->getString()).add(af));
  }
  
  m_currentKeymap =
    m_setting->m_keymaps.add(
      Keymap(type, name->getString(), windowClassName, windowTitleName,
	     keySeq, parentKeymap));
}


// <KEY_SEQUENCE>
KeySeq *SettingLoader::load_KEY_SEQUENCE(const istring &i_name,
					 bool i_isInParen)
{
  KeySeq keySeq(i_name);
  while (!isEOL())
  {
    Modifier modifier =
      load_MODIFIER(Modifier::Type_KEYSEQ, m_defaultKeySeqModifier);
    Token *t = lookToken();
    if (t->isCloseParen() && i_isInParen)
      break;
    else if (t->isOpenParen())
    {
      getToken(); // open paren
      KeySeq *ks = load_KEY_SEQUENCE("", true);
      getToken(); // close paren
      keySeq.add(ActionKeySeq(ks));
    }
    else if (*t == "$") // <KEYSEQ_NAME>
    {
      getToken();
      t = getToken();
      KeySeq *ks = m_setting->m_keySeqs.searchByName(t->getString());
      if (ks == NULL)
	throw ErrorMessage() << "`$" << *t << "': unknown keyseq name.";
      keySeq.add(ActionKeySeq(ks));
    }
    else if (*t == "&") // <FUNCTION_NAME>
    {
      getToken();
      t = getToken();
      
      // search function
      ActionFunction af;
      af.m_modifier = modifier;
      af.m_function = Function::search(t->getString());
      if (af.m_function == NULL)
	throw ErrorMessage() << "`&" << *t << "': unknown function name.";

      // get arguments
      const char *type = af.m_function->m_argType;
      if (type == NULL)
	// no argument
      {
	if (!isEOL() && lookToken()->isOpenParen())
	{
	  getToken();
	  if (!getToken()->isCloseParen())
	    throw ErrorMessage() << "`&" << af.m_function->m_name
				 << "': too many arguments.";
	}
      }
      else if (isEOL() && *type == '&')
	;
      else
	// has some arguments
      {
	if (!lookToken()->isOpenParen())
	{
	  if (*type == '&')
	    goto ok;
	  throw ErrorMessage() << "there must be `(' after `&"
			       << af.m_function->m_name << "'.";
	}
	
	getToken();
	
	for (; *type; type ++)
	{
	  t = lookToken();
	  if (*type == '&')
	    if (t->isCloseParen())
	      break;
	    else
	      type ++;
	  
	  if (t->isCloseParen())
	    throw ErrorMessage() << "`&"  << af.m_function->m_name
				 << "': too few arguments.";
	  switch (*type)
	  {
	    case 'K': // keymap
	    {
	      getToken();
	      Keymap *keymap =
		m_setting->m_keymaps.searchByName(t->getString());
	      if (!keymap)
		throw ErrorMessage() << "`" << *t << "': unknown keymap name.";
	      t->setData((long)keymap);
	      break;
	    }

	    case 'S': // keyseq
	    {
	      getToken();
	      KeySeq *keySeq;
	      if (t->isOpenParen())
	      {
		keySeq = load_KEY_SEQUENCE("", true);
		getToken(); // close paren
	      }
	      else if (*t == "$")
	      {
		t = getToken();
		keySeq =
		  m_setting->m_keySeqs.searchByName(t->getString());
		if (!keySeq)
		  throw ErrorMessage() << "`$" << *t
				       << "': unknown keyseq name.";
	      }
	      else
		throw ErrorMessage() << "`" << *t << "': it is not keyseq.";
	      t->setData((long)keySeq);
	      break;
	    }

	    case 'l': // /lock\d/
	    {
	      getToken();
	      const static struct { const char *s; Modifier::Type mt; } map[] =
	      {
		{ "lock0", Modifier::Type_Lock0 },
		{ "lock1", Modifier::Type_Lock1 },
		{ "lock2", Modifier::Type_Lock2 },
		{ "lock3", Modifier::Type_Lock3 },
		{ "lock4", Modifier::Type_Lock4 },
		{ "lock5", Modifier::Type_Lock5 },
		{ "lock6", Modifier::Type_Lock6 },
		{ "lock7", Modifier::Type_Lock7 },
		{ "lock8", Modifier::Type_Lock8 },
		{ "lock9", Modifier::Type_Lock9 },
	      };
	      int i;
	      for (i = 0; i < NUMBER_OF(map); i++)
		if (*t == map[i].s)
		{
		  t->setData((long)map[i].mt);
		  break;
		}
	      if (i == NUMBER_OF(map))
		throw ErrorMessage() << "`" << *t << "' unknown lock name.";
	      break;
	    }

	    case 's': // string
	      getToken();
	      t->getString();
	      break;

	    case 'd': // digit
	      getToken();
	      t->getNumber();
	      break;

	    case 'V': // vkey
	    {
	      getToken();
	      long vkey = 0;
	      while (true)
	      {
		if (t->isNumber()) { vkey |= (BYTE)t->getNumber(); break; }
		else if (*t == "E-") vkey |= Function::VK_extended;
		else if (*t == "U-") vkey |= Function::VK_up;
		else if (*t == "D-") vkey |= Function::VK_down;
		else
		{
		  const VKeyTable *vkt;
		  for (vkt = g_vkeyTable; vkt->m_name; vkt ++)
		    if (*t == vkt->m_name)
		      break;
		  if (!vkt->m_name)
		    throw ErrorMessage() << "`" << *t
					 << "': unknown virtual key name.";
		  vkey |= vkt->m_code;
		  break;
		}
		t = getToken();
	      }
	      if (!(vkey & Function::VK_up) && !(vkey & Function::VK_down))
		vkey |= Function::VK_up | Function::VK_down;
	      t->setData(vkey);
	      break;
	    }

	    case 'w': // window
	    {
	      getToken();
	      if (t->isNumber())
	      {
		if (t->getNumber() < Function::PM_toBegin)
		  throw ErrorMessage() << "`" << *t
				       << "': invalid target window";
		t->setData(t->getNumber());
		break;
	      }
	      const struct { int window; char *name; } window[] =
	      {
		{ Function::PM_toOverlappedWindow, "toOverlappedWindow" },
		{ Function::PM_toMainWindow, "toMainWindow" },
		{ Function::PM_toItself, "toItself" },
		{ Function::PM_toParentWindow, "toParentWindow" },
	      };
	      int i;
	      for (i = 0; i < NUMBER_OF(window); i++)
		if (*t == window[i].name)
		{
		  t->setData(window[i].window);
		  break;
		}
	      if (i == NUMBER_OF(window))
		throw ErrorMessage() << "`" << *t
				     << "': invalid target window.";
	      break;
	    }
	    
	    case 'W': // showWindow
	    {
	      getToken();
	      struct ShowType { int m_show; char *m_name; };
	      static const ShowType show[] =
	      {
#define SHOW(name) { SW_##name, #name }
		SHOW(HIDE), SHOW(MAXIMIZE), SHOW(MINIMIZE), SHOW(RESTORE),
		SHOW(SHOW), SHOW(SHOWDEFAULT), SHOW(SHOWMAXIMIZED),
		SHOW(SHOWMINIMIZED), SHOW(SHOWMINNOACTIVE), SHOW(SHOWNA),
		SHOW(SHOWNOACTIVATE), SHOW(SHOWNORMAL),
#undef SHOW
	      };
	      t->setData(SW_SHOW);
	      int i;
	      for (i = 0; i < NUMBER_OF(show); ++ i)
		if (*t == show[i].m_name)
		{
		  t->setData(show[i].m_show);
		  break;
		}
	      if (i == NUMBER_OF(show))
		throw ErrorMessage() << "`" << *t
				     << "': unknown show command.";
	      break;
	    }

	    case 'G': // gravity
	    {
	      getToken();
	      const struct { int m_gravity; char *m_name; } gravity[] =
	      {
		Function::Gravity_C, "C", 
		Function::Gravity_N, "N", Function::Gravity_E, "E",
		Function::Gravity_W, "W", Function::Gravity_S, "S",
		Function::Gravity_NW, "NW", Function::Gravity_NW, "WN",
		Function::Gravity_NE, "NE", Function::Gravity_NE, "EN",
		Function::Gravity_SW, "SW", Function::Gravity_SW, "WS",
		Function::Gravity_SE, "SE", Function::Gravity_SE, "ES",
	      };
	      t->setData(Function::Gravity_NW);
	      int i;
	      for (i = 0; i < NUMBER_OF(gravity); i++)
		if (*t == gravity[i].m_name)
		{
		  t->setData(gravity[i].m_gravity);
		  break;
		}
	      if (i == NUMBER_OF(gravity))
		throw ErrorMessage() << "`" << *t
				     << "': unknown gravity symbol.";
	      break;
	    }

	    case 'b': // bool
	    {
	      getToken();
	      if (*t == "false")
		t->setData(0);
	      else
		t->setData(1);
	      break;
	    }

	    case 'D': // dialog
	    {
	      getToken();
	      if (*t == "Investigate")
		t->setData(Function::MayuDlg_investigate);
	      else if (*t == "Log")
		t->setData(Function::MayuDlg_log);
	      else
		throw ErrorMessage() << "`" << *t
				     << "': unknown dialog box.";
	      break;
	    }

	    case 'M': // modifier
	    {
	      Modifier modifier;
	      for (int i = Modifier::Type_begin; i != Modifier::Type_end; i ++)
		modifier.dontcare((Modifier::Type)i);
	      modifier = load_MODIFIER(Modifier::Type_ASSIGN, modifier);
	      m_setting->m_modifiers.push_front(modifier);
	      t->setData((long)&*m_setting->m_modifiers.begin());
	      break;
	    }

	    case '&': // rest is optional
	    default:
	      throw ErrorMessage() << "internal error.";
	  }
	  af.m_args.push_back(*t);
	}
	if (!getToken()->isCloseParen())
	  throw ErrorMessage() << "`&"  << af.m_function->m_name
			       << "': too many arguments.";
	ok: ;
      }
      keySeq.add(af);
    }
    else // <KEYSEQ_MODIFIED_KEY_NAME>
    {
      ModifiedKey mkey;
      mkey.m_modifier = modifier;
      mkey.m_key = load_KEY_NAME();
      keySeq.add(ActionKey(mkey));
    }
  }
  return m_setting->m_keySeqs.add(keySeq);
}


// <KEY_ASSIGN>
void SettingLoader::load_KEY_ASSIGN()
{
  list<ModifiedKey> assignedKeys;
  
  ModifiedKey mkey;
  mkey.m_modifier =
    load_MODIFIER(Modifier::Type_ASSIGN, m_defaultAssignModifier);
  if (*lookToken() == "=")
  {
    getToken();
    m_defaultKeySeqModifier = load_MODIFIER(Modifier::Type_KEYSEQ,
					  m_defaultKeySeqModifier);
    m_defaultAssignModifier = mkey.m_modifier;
    return;
  }
  
  while (true)
  {
    mkey.m_key = load_KEY_NAME();
    assignedKeys.push_back(mkey);
    if (*lookToken() == "=>")
      break;
    mkey.m_modifier =
      load_MODIFIER(Modifier::Type_ASSIGN, m_defaultAssignModifier);
  }
  getToken();

  ASSERT(m_currentKeymap);
  KeySeq *keySeq = load_KEY_SEQUENCE();
  for (list<ModifiedKey>::iterator i = assignedKeys.begin();
       i != assignedKeys.end(); i++)
    m_currentKeymap->addAssignment(*i, keySeq);
}


// <EVENT_ASSIGN>
void SettingLoader::load_EVENT_ASSIGN()
{
  list<ModifiedKey> assignedKeys;

  ModifiedKey mkey;
  mkey.m_modifier.dontcare();			//set all modifiers to dontcare
  
  Token *t = getToken();
  Key **e;
  for (e = Event::events; *e; e ++)
    if (*t == (*e)->getName())
    {
      mkey.m_key = *e;
      break;
    }
  if (!*e)
    throw ErrorMessage() << "`" << *t << "': invalid event name.";

  if (*getToken() != "=>")
    throw ErrorMessage() << "`=>' is expected.";

  ASSERT(m_currentKeymap);
  KeySeq *keySeq = load_KEY_SEQUENCE();
  m_currentKeymap->addAssignment(mkey, keySeq);
}


// <MODIFIER_ASSIGNMENT>
void SettingLoader::load_MODIFIER_ASSIGNMENT()
{
  // <MODIFIER_NAME>
  Token *t = getToken();
  Modifier::Type mt;

  while (true)
  {
    Keymap::AssignMode am = Keymap::AM_notModifier;
    if      (*t == "!" ) am = Keymap::AM_true, t = getToken();
    else if (*t == "!!") am = Keymap::AM_oneShot, t = getToken();
    
    if      (*t == "shift") mt = Modifier::Type_Shift;
    else if (*t == "alt"  ||
	     *t == "meta" ||
	     *t == "menu" ) mt = Modifier::Type_Alt;
    else if (*t == "control" ||
	     *t == "ctrl" ) mt = Modifier::Type_Control;
    else if (*t == "windows" ||
	     *t == "win"  ) mt = Modifier::Type_Windows;
    else if (*t == "mod0" ) mt = Modifier::Type_Mod0;
    else if (*t == "mod1" ) mt = Modifier::Type_Mod1;
    else if (*t == "mod2" ) mt = Modifier::Type_Mod2;
    else if (*t == "mod3" ) mt = Modifier::Type_Mod3;
    else if (*t == "mod4" ) mt = Modifier::Type_Mod4;
    else if (*t == "mod5" ) mt = Modifier::Type_Mod5;
    else if (*t == "mod6" ) mt = Modifier::Type_Mod6;
    else if (*t == "mod7" ) mt = Modifier::Type_Mod7;
    else if (*t == "mod8" ) mt = Modifier::Type_Mod8;
    else if (*t == "mod9" ) mt = Modifier::Type_Mod9;
    else throw ErrorMessage() << "`" << *t << "': invalid modifier name.";

    if (am == Keymap::AM_notModifier)
      break;
    
    m_currentKeymap->addModifier(mt, Keymap::AO_overwrite, am, NULL);
    if (isEOL())
      return;
    t = getToken();
  }
  
  // <ASSIGN_OP>
  t = getToken();
  Keymap::AssignOperator ao;
  if      (*t == "=" ) ao = Keymap::AO_new;
  else if (*t == "+=") ao = Keymap::AO_add;
  else if (*t == "-=") ao = Keymap::AO_sub;
  else  throw ErrorMessage() << "`" << *t << "': is unknown operator.";

  // <ASSIGN_MODE>? <KEY_NAME>
  while (!isEOL())
  {
    // <ASSIGN_MODE>? 
    t = getToken();
    Keymap::AssignMode am = Keymap::AM_normal;
    if      (*t == "!" ) am = Keymap::AM_true, t = getToken();
    else if (*t == "!!") am = Keymap::AM_oneShot, t = getToken();
    
    // <KEY_NAME>
    Key *key = m_setting->m_keyboard.searchKey(t->getString());
    if (!key)
      throw ErrorMessage() << "`" << *t << "': invalid key name.";

    // we can ignore warning C4701
    m_currentKeymap->addModifier(mt, ao, am, key);
    if (ao == Keymap::AO_new)
      ao = Keymap::AO_add;
  }
}


// <KEYSEQ_DEFINITION>
void SettingLoader::load_KEYSEQ_DEFINITION()
{
  if (*getToken() != "$")
    throw ErrorMessage() << "there must be `$' after `keyseq'";
  Token *name = getToken();
  if (*getToken() != "=")
    throw ErrorMessage() << "there must be `=' after keyseq naem";
  load_KEY_SEQUENCE(name->getString());
}


// <DEFINE>
void SettingLoader::load_DEFINE()
{
  m_setting->m_symbols.insert(getToken()->getString());
}


// <IF>
void SettingLoader::load_IF()
{
  if (!getToken()->isOpenParen())
    throw ErrorMessage() << "there must be `(' after `if'.";
  Token *t = getToken(); // <SYMBOL> or !
  bool not = false;
  if (*t == "!")
  {
    not = true;
    t = getToken(); // <SYMBOL>
  }
  
  bool doesSymbolExist = (m_setting->m_symbols.find(t->getString())
			  != m_setting->m_symbols.end());
  bool doesRead = ((doesSymbolExist && !not) ||
		   (!doesSymbolExist && not));
  if (0 < m_canReadStack.size())
    doesRead = doesRead && m_canReadStack.back();
  
  if (!getToken()->isCloseParen())
    throw ErrorMessage() << "there must be `)'.";

  m_canReadStack.push_back(doesRead);
  if (!isEOL())
  {
    size_t len = m_canReadStack.size();
    load_LINE();
    if (len < m_canReadStack.size())
    {
      bool r = m_canReadStack.back();
      m_canReadStack.pop_back();
      m_canReadStack[len - 1] = r && doesRead;
    }
    else if (len == m_canReadStack.size())
      m_canReadStack.pop_back();
    else
      ; // `end' found
  }
}


// <ELSE> <ELSEIF>
void SettingLoader::load_ELSE(bool i_isElseIf, const istring &i_token)
{
  bool doesRead = !load_ENDIF(i_token);
  if (0 < m_canReadStack.size())
    doesRead = doesRead && m_canReadStack.back();
  m_canReadStack.push_back(doesRead);
  if (!isEOL())
  {
    size_t len = m_canReadStack.size();
    if (i_isElseIf)
      load_IF();
    else
      load_LINE();
    if (len < m_canReadStack.size())
    {
      bool r = m_canReadStack.back();
      m_canReadStack.pop_back();
      m_canReadStack[len - 1] = doesRead && r;
    }
    else if (len == m_canReadStack.size())
      m_canReadStack.pop_back();
    else
      ; // `end' found
  }
}


// <ENDIF>
bool SettingLoader::load_ENDIF(const istring &i_token)
{
  if (m_canReadStack.size() == 0)
    throw ErrorMessage() << "unbalanced `" << i_token << "'";
  bool r = m_canReadStack.back();
  m_canReadStack.pop_back();
  return r;
}


// <LINE>
void SettingLoader::load_LINE()
{
  Token *i_token = getToken();

  // <COND_SYMBOL>
  if      (*i_token == "if" ||
	   *i_token == "and") load_IF();
  else if (*i_token == "else") load_ELSE(false, i_token->getString());
  else if (*i_token == "elseif" ||
	   *i_token == "elsif"  ||
	   *i_token == "elif"   ||
	   *i_token == "or") load_ELSE(true, i_token->getString());
  else if (*i_token == "endif") load_ENDIF("endif");
  else if (0 < m_canReadStack.size() && !m_canReadStack.back())
  {
    while (!isEOL())
      getToken();
  }
  else if (*i_token == "define") load_DEFINE();
  // <INCLUDE>
  else if (*i_token == "include") load_INCLUDE();
  // <KEYBOARD_DEFINITION>
  else if (*i_token == "def") load_KEYBOARD_DEFINITION();
  // <KEYMAP_DEFINITION>
  else if (*i_token == "keymap"  ||
	   *i_token == "keymap2" ||
	   *i_token == "window") load_KEYMAP_DEFINITION(i_token);
  // <KEY_ASSIGN>
  else if (*i_token == "key") load_KEY_ASSIGN();
  // <EVENT_ASSIGN>
  else if (*i_token == "event") load_EVENT_ASSIGN();
  // <MODIFIER_ASSIGNMENT>
  else if (*i_token == "mod") load_MODIFIER_ASSIGNMENT();
  // <KEYSEQ_DEFINITION>
  else if (*i_token == "keyseq") load_KEYSEQ_DEFINITION();
  else
    throw ErrorMessage() << "syntax error `" << *i_token << "'.";
}

  
// prefix sort predicate used in load(const string &)
static bool prefixSortPred(const istring &i_a, const istring &i_b)
{
  return i_b.size() < i_a.size();
}


// load (called from load(Setting *, const istring &) only)
void SettingLoader::load(const istring &i_filename)
{
  m_currentFilename = i_filename;
  ifstream ist(m_currentFilename.c_str());

  // prefix
  if (m_prefixesRefCcount == 0)
  {
    static const char *prefixes[] =
    {
      "=", "=>", "&&", "||", ":", "$", "&",
      "-=", "+=", "!", "!!", 
      "E0-", "E1-",				// <SCAN_CODE_EXTENTION>
      "S-", "A-", "M-", "C-", "W-", "*", "~",	// <BASIC_MODIFIER>
      "U-", "D-",				// <KEYSEQ_MODIFIER>
      "IL-", "IC-", "I-", "NL-", "CL-", "SL-", 	// <ASSIGN_MODIFIER>
      "M0-", "M1-", "M2-", "M3-", "M4-",
      "M5-", "M6-", "M7-", "M8-", "M9-", 
      "L0-", "L1-", "L2-", "L3-", "L4-",
      "L5-", "L6-", "L7-", "L8-", "L9-", 
    };
    m_prefixes = new vector<istring>;
    for (size_t i = 0; i < NUMBER_OF(prefixes); ++ i)
      m_prefixes->push_back(prefixes[i]);
    sort(m_prefixes->begin(), m_prefixes->end(), prefixSortPred);
  }
  m_prefixesRefCcount ++;

  // create parser
  Parser parser(ist);
  parser.setPrefixes(m_prefixes);
    
  while (true)
  {
    try
    {
      if (!parser.getLine(&m_tokens))
	break;
      m_ti = m_tokens.begin();
    }
    catch (ErrorMessage &e)
    {
      if (m_log && m_soLog)
      {
	Acquire a(m_soLog);
	*m_log << m_currentFilename << "(" << parser.getLineNumber()
	     << ") : error: " << e << endl;
      }
      m_isThereAnyError = true;
      continue;
    }
      
    try
    {
      load_LINE();
      if (!isEOL())
	throw WarningMessage() << "back garbage is ignored.";
    }
    catch (WarningMessage &w)
    {
      if (m_log && m_soLog)
      {
	Acquire a(m_soLog);
	*m_log << i_filename << "(" << parser.getLineNumber()
	       << ") : warning: " << w << endl;
      }
    }
    catch (ErrorMessage &e)
    {
      if (m_log && m_soLog)
      {
	Acquire a(m_soLog);
	*m_log << i_filename << "(" << parser.getLineNumber()
	       << ") : error: " << e << endl;
      }
      m_isThereAnyError = true;
    }
  }
    
  // m_prefixes
  m_prefixesRefCcount --;
  if (m_prefixesRefCcount == 0)
    delete m_prefixes;

  if (0 < m_canReadStack.size())
  {
    Acquire a(m_soLog);
    *m_log << m_currentFilename << "(" << parser.getLineNumber()
	 << ") : error: unbalanced `if'.  you forget `endif', didn'i_token you?"
	 << endl;
    m_isThereAnyError = true;
  }
}


// is the filename readable ?
bool SettingLoader::isReadable(const istring &i_filename) const 
{
  if (i_filename.empty())
    return false;
  ifstream ist(i_filename.c_str());
  if (ist.good())
  {
    if (m_log && m_soLog)
    {
      Acquire a(m_soLog, 1);
      *m_log << "loading: " << i_filename << endl;
    }
    return true;
  }
  else
  {
    if (m_log && m_soLog)
    {
      Acquire a(m_soLog, 1);
      *m_log << "     no: " << i_filename << endl;
    }
    return false;
  }
}

#if 0
// get filename from registry
bool SettingLoader::getFilenameFromRegistry(istring *o_path) const
{
  // get from registry
  Registry reg(MAYU_REGISTRY_ROOT);
  int index;
  reg.read(".mayuIndex", &index, 0);
  char buf[100];
  snprintf(buf, sizeof(buf), ".mayu%d", index);
  if (!reg.read(buf, o_path))
    return false;

  // parse registry entry
  Regexp getFilename("^[^;]*;([^;]*);(.*)$");
  if (!getFilename.doesMatch(*o_path))
    return false;
  
  istring path = getFilename[1];
  istring options = getFilename[2];
  
  if (!(0 < path.size() && isReadable(path)))
    return false;
  *o_path = path;
  
  // set symbols
  Regexp symbol("-D([^;]*)");
  while (symbol.doesMatch(options))
  {
    m_setting->symbols.insert(symbol[1]);
    options = options.substr(symbol.subBegin(1));
  }
  
  return true;
}
#endif


// get filename
bool SettingLoader::getFilename(const istring &i_name, istring *o_path) const
{
  // the default filename is ".mayu"
  const istring &name = i_name.empty() ? istring(".mayu") : i_name;
  
  bool isFirstTime = true;

  while (true)
  {
    // find file from registry
    if (i_name.empty())				// called not from 'include'
    {
      std::list<istring> symbols;
      if (getFilenameFromRegistry(NULL, o_path, &symbols))
      {
	if (!isReadable(*o_path))
	  return false;
	for (std::list<istring>::iterator
	       i = symbols.begin(); i != symbols.end(); i ++)
	  m_setting->m_symbols.insert(*i);
	return true;
      }
    }
    
    if (!isFirstTime)
      return false;
    
    // find file from home directory
    std::list<istring> pathes;
    getHomeDirectories(&pathes);
    for (std::list<istring>::iterator
	   i = pathes.begin(); i != pathes.end(); i ++)
    {
      *o_path = *i + "\\" + name;
      if (isReadable(*o_path))
	return true;
    }
    
    if (!i_name.empty())
      return false;				// called by 'include'
    
    if (!DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_setting),
		   NULL, dlgSetting_dlgProc))
      return false;
  }
}


// constructor
SettingLoader::SettingLoader(SyncObject *i_soLog, ostream *i_log)
  : m_setting(NULL),
    m_isThereAnyError(false),
    m_soLog(i_soLog),
    m_log(i_log),
    m_currentKeymap(NULL)
{
  m_defaultKeySeqModifier =
    m_defaultAssignModifier.release(Modifier::Type_ImeComp);
}


/* load m_setting
   If called by "include", 'filename' describes filename.
   Otherwise the 'filename' is empty.
 */
bool SettingLoader::load(Setting *i_setting, const istring &i_filename)
{
  m_setting = i_setting;
  m_isThereAnyError = false;
    
  istring path;
  if (!getFilename(i_filename, &path))
  {
    if (i_filename.empty())
    {
      Acquire a(m_soLog);
      *m_log << "error: failed to load ~/.mayu" << endl;
      return false;
    }
    else
      throw ErrorMessage() << "`" << i_filename
			   << "': no such file or other error.";
  }

  // create global keymap's default keySeq
  ActionFunction af;
  af.m_function = Function::search(Function::Id_OtherWindowClass);
  ASSERT( af.m_function );
  KeySeq *globalDefault = m_setting->m_keySeqs.add(KeySeq("").add(af));
  
  // add default keymap
  m_currentKeymap = m_setting->m_keymaps.add(
    Keymap(Keymap::Type_windowOr, "Global", "", "", globalDefault, NULL));

  /*
  // add keyboard layout name
  if (filename.empty())
  {
    char keyboardLayoutName[KL_NAMELENGTH];
    if (GetKeyboardLayoutName(keyboardLayoutName))
    {
      istring kl = istring("KeyboardLayout/") + keyboardLayoutName;
      m_setting->symbols.insert(kl);
      Acquire a(m_soLog);
      *m_log << "KeyboardLayout: " << kl << endl;
    }
  }
  */
  
  // load
  load(path);

  // finalize
  if (i_filename.empty())
    m_setting->m_keymaps.adjustModifier(m_setting->m_keyboard);
  
  return !m_isThereAnyError;
}


vector<istring> *SettingLoader::m_prefixes;	// m_prefixes terminal symbol
size_t SettingLoader::m_prefixesRefCcount;	// reference count of
						// m_prefixes
