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
  Key prefixed(_T("prefixed"));
  Key before_key_down(_T("before-key-down"));
  Key after_key_up(_T("before-key-down"));
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
  tstringi *o_name, tstringi *o_filename, std::list<tstringi> *o_symbols)
{
  Registry reg(MAYU_REGISTRY_ROOT);
  int index;
  reg.read(_T(".mayuIndex"), &index, 0);
  _TCHAR buf[100];
  _sntprintf(buf, NUMBER_OF(buf), _T(".mayu%d"), index);

  tstringi entry;
  if (!reg.read(buf, &entry))
    return false;
  
  tregex getFilename(_T("^([^;]*);([^;]*);(.*)$"));
  tcmatch_results getFilenameResult;
  if (!boost::regex_match(entry, getFilenameResult, getFilename))
    return false;
  
  if (o_name)
    *o_name = getFilenameResult.str(1);
  if (o_filename)
    *o_filename = getFilenameResult.str(2);
  if (o_symbols)
  {
    tstringi symbols = getFilenameResult.str(3);
    tregex symbol(_T("-D([^;]*)(.*)$"));
    tcmatch_results symbolResult;
    while (boost::regex_search(symbols, symbolResult, symbol))
    {
      o_symbols->push_back(symbolResult.str(1));
      symbols = symbolResult.str(2);
    }
  }
  return true;
}


// get home directory path
void getHomeDirectories(std::list<tstringi> *o_pathes)
{
  tstringi filename;
  if (getFilenameFromRegistry(NULL, &filename, NULL))
  {
    tregex getPath(_T("^(.*[/\\\\])[^/\\\\]*$"));
    tcmatch_results getPathResult;
    if (boost::regex_match(filename, getPathResult, getPath))
      o_pathes->push_back(getPathResult.str(1));
  }
  
  const _TCHAR *home = _tgetenv(_T("HOME"));
  if (home)
    o_pathes->push_back(home);

  const _TCHAR *homedrive = _tgetenv(_T("HOMEDRIVE"));
  const _TCHAR *homepath = _tgetenv(_T("HOMEPATH"));
  if (homedrive && homepath)
    o_pathes->push_back(tstringi(homedrive) + homepath);

  const _TCHAR *userprofile = _tgetenv(_T("USERPROFILE"));
  if (userprofile)
    o_pathes->push_back(userprofile);
  
  _TCHAR buf[GANA_MAX_PATH];
  if (GetModuleFileName(GetModuleHandle(NULL), buf, NUMBER_OF(buf)))
  {
    PathRemoveFileSpec(buf);
    o_pathes->push_back(buf);
  }
}


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
    throw ErrorMessage() << _T("too few words.");
  return &*(m_ti ++);
}

  
// look next token
Token *SettingLoader::lookToken()
{
  if (isEOL())
    throw ErrorMessage() << _T("too few words.");
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
      if      (*t == _T("E0-")) sc.m_flags |= ScanCode::E0;
      else if (*t == _T("E1-")) sc.m_flags |= ScanCode::E1;
      else  throw ErrorMessage() << _T("`") << *t
				 << _T("': invalid modifier.");
    }
  }
}


// <DEFINE_KEY>
void SettingLoader::load_DEFINE_KEY()
{
  Token *t = getToken();
  Key key;
      
  // <KEY_NAMES>
  if (*t == _T('('))
  {
    key.addName(getToken()->getString());
    while (t = getToken(), *t != _T(')'))
      key.addName(t->getString());
    if (*getToken() != _T("="))
      throw ErrorMessage() << _T("there must be `=' after `)'.");
  }
  else
  {
    key.addName(t->getString());
    while (t = getToken(), *t != _T("="))
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
  if      (*t == _T("shift")  ) mt = Modifier::Type_Shift;
  else if (*t == _T("alt")     ||
	   *t == _T("meta")    ||
	   *t == _T("menu")   ) mt = Modifier::Type_Alt;
  else if (*t == _T("control") ||
	   *t == _T("ctrl")   ) mt = Modifier::Type_Control;
  else if (*t == _T("windows") ||
	   *t == _T("win")    ) mt = Modifier::Type_Windows;
  else throw ErrorMessage() << _T("`") << *t
			    << _T("': invalid modifier name.");
    
  if (*getToken() != _T("="))
    throw ErrorMessage() << _T("there must be `=' after modifier name.");
    
  while (!isEOL())
  {
    t = getToken();
    Key *key =
      m_setting->m_keyboard.searchKeyByNonAliasName(t->getString());
    if (!key)
      throw ErrorMessage() << _T("`") << *t << _T("': invalid key name.");
    m_setting->m_keyboard.addModifier(mt, key);
  }
}


// <DEFINE_SYNC_KEY>
void SettingLoader::load_DEFINE_SYNC_KEY()
{
  Key *key = m_setting->m_keyboard.getSyncKey();
  key->initialize();
  key->addName(_T("sync"));
  
  if (*getToken() != _T("="))
    throw ErrorMessage() << _T("there must be `=' after `sync'.");
  
  load_SCAN_CODES(key);
}


// <DEFINE_ALIAS>
void SettingLoader::load_DEFINE_ALIAS()
{
  Token *name = getToken();
  
  if (*getToken() != _T("="))
    throw ErrorMessage() << _T("there must be `=' after `alias'.");

  Token *t = getToken();
  Key *key = m_setting->m_keyboard.searchKeyByNonAliasName(t->getString());
  if (!key)
    throw ErrorMessage() << _T("`") << *t << _T("': invalid key name.");
  m_setting->m_keyboard.addAlias(name->getString(), key);
}


// <KEYBOARD_DEFINITION>
void SettingLoader::load_KEYBOARD_DEFINITION()
{
  Token *t = getToken();
    
  // <DEFINE_KEY>
  if (*t == _T("key")) load_DEFINE_KEY();

  // <DEFINE_MODIFIER>
  else if (*t == _T("mod")) load_DEFINE_MODIFIER();
  
  // <DEFINE_SYNC_KEY>
  else if (*t == _T("sync")) load_DEFINE_SYNC_KEY();
  
  // <DEFINE_ALIAS>
  else if (*t == _T("alias")) load_DEFINE_ALIAS();
  
  //
  else throw ErrorMessage() << _T("syntax error `") << *t << _T("'.");
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

    const static struct { const _TCHAR *s; Modifier::Type mt; } map[] =
    {
      // <BASIC_MODIFIER>
      { _T("S-"),  Modifier::Type_Shift },
      { _T("A-"),  Modifier::Type_Alt },
      { _T("M-"),  Modifier::Type_Alt },
      { _T("C-"),  Modifier::Type_Control },
      { _T("W-"),  Modifier::Type_Windows },
      // <KEYSEQ_MODIFIER>
      { _T("U-"),  Modifier::Type_Up },
      { _T("D-"),  Modifier::Type_Down },
      // <ASSIGN_MODIFIER>
      { _T("IL-"), Modifier::Type_ImeLock },
      { _T("IC-"), Modifier::Type_ImeComp },
      { _T("I-"),  Modifier::Type_ImeComp },
      { _T("NL-"), Modifier::Type_NumLock },
      { _T("CL-"), Modifier::Type_CapsLock },
      { _T("SL-"), Modifier::Type_ScrollLock },
      { _T("M0-"), Modifier::Type_Mod0 },
      { _T("M1-"), Modifier::Type_Mod1 },
      { _T("M2-"), Modifier::Type_Mod2 },
      { _T("M3-"), Modifier::Type_Mod3 },
      { _T("M4-"), Modifier::Type_Mod4 },
      { _T("M5-"), Modifier::Type_Mod5 },
      { _T("M6-"), Modifier::Type_Mod6 },
      { _T("M7-"), Modifier::Type_Mod7 },
      { _T("M8-"), Modifier::Type_Mod8 },
      { _T("M9-"), Modifier::Type_Mod9 },
      { _T("L0-"), Modifier::Type_Lock0 },
      { _T("L1-"), Modifier::Type_Lock1 },
      { _T("L2-"), Modifier::Type_Lock2 },
      { _T("L3-"), Modifier::Type_Lock3 },
      { _T("L4-"), Modifier::Type_Lock4 },
      { _T("L5-"), Modifier::Type_Lock5 },
      { _T("L6-"), Modifier::Type_Lock6 },
      { _T("L7-"), Modifier::Type_Lock7 },
      { _T("L8-"), Modifier::Type_Lock8 },
      { _T("L9-"), Modifier::Type_Lock9 },
    };

    for (int i = 0; i < NUMBER_OF(map); ++ i)
      if (*t == map[i].s)
      {
	getToken();
	Modifier::Type mt = map[i].mt;
	if (static_cast<int>(i_mode) <= static_cast<int>(mt))
	  throw ErrorMessage() << _T("`") << *t
			       << _T("': invalid modifier at this context.");
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
      
    if (*t == _T("*"))
    {
      getToken();
      flag = DONTCARE;
      continue;
    }
      
    if (*t == _T("~"))
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
    throw ErrorMessage() << _T("`") << *t << _T("': invalid key name.");
  return key;
}


// <KEYMAP_DEFINITION>
void SettingLoader::load_KEYMAP_DEFINITION(const Token *i_which)
{
  Keymap::Type type = Keymap::Type_keymap;
  Token *name = getToken();	// <KEYMAP_NAME>
  tstringi windowClassName;
  tstringi windowTitleName;
  KeySeq *keySeq = NULL;
  Keymap *parentKeymap = NULL;
  bool isKeymap2 = false;

  if (!isEOL())
  {
    Token *t = lookToken();
    if (*i_which == _T("window"))	// <WINDOW>
    {
      if (t->isOpenParen())
	// "(" <WINDOW_CLASS_NAME> "&&" <WINDOW_TITLE_NAME> ")"
	// "(" <WINDOW_CLASS_NAME> "||" <WINDOW_TITLE_NAME> ")"
      {
	getToken();
	windowClassName = getToken()->getRegexp();
	t = getToken();
	if (*t == _T("&&"))
	  type = Keymap::Type_windowAnd;
	else if (*t == _T("||"))
	  type = Keymap::Type_windowOr;
	else
	  throw ErrorMessage() << _T("`") << *t << _T("': unknown operator.");
	windowTitleName = getToken()->getRegexp();
	if (!getToken()->isCloseParen())
	  throw ErrorMessage() << _T("there must be `)'.");
      }
      else if (t->isRegexp())	// <WINDOW_CLASS_NAME>
      {
	getToken();
	type = Keymap::Type_windowAnd;
	windowClassName = t->getRegexp();
      }
    }
    else if (*i_which == _T("keymap"))
      ;
    else if (*i_which == _T("keymap2"))
      isKeymap2 = true;
    else
      ASSERT(false);
    
    if (!isEOL())
    {
      t = lookToken();
      // <KEYMAP_PARENT>
      if (*t == _T(":"))
      {
	getToken();
	t = getToken();
	parentKeymap = m_setting->m_keymaps.searchByName(t->getString());
	if (!parentKeymap)
	  throw ErrorMessage() << _T("`") << *t
			       << _T("': unknown keymap name.");
      }
      if (!isEOL())
      {
	t = getToken();
	if (*t != _T("=>"))
	  throw ErrorMessage() << _T("`") << *t << _T("': syntax error.");
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
KeySeq *SettingLoader::load_KEY_SEQUENCE(const tstringi &i_name,
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
      KeySeq *ks = load_KEY_SEQUENCE(_T(""), true);
      getToken(); // close paren
      keySeq.add(ActionKeySeq(ks));
    }
    else if (*t == _T("$")) // <KEYSEQ_NAME>
    {
      getToken();
      t = getToken();
      KeySeq *ks = m_setting->m_keySeqs.searchByName(t->getString());
      if (ks == NULL)
	throw ErrorMessage() << _T("`$") << *t
			     << _T("': unknown keyseq name.");
      keySeq.add(ActionKeySeq(ks));
    }
    else if (*t == _T("&")) // <FUNCTION_NAME>
    {
      getToken();
      t = getToken();
      
      // search function
      ActionFunction af;
      af.m_modifier = modifier;
      af.m_function = Function::search(t->getString());
      if (af.m_function == NULL)
	throw ErrorMessage() << _T("`&") << *t
			     << _T("': unknown function name.");

      // get arguments
      const _TCHAR *type = af.m_function->m_argType;
      if (type == NULL)
	// no argument
      {
	if (!isEOL() && lookToken()->isOpenParen())
	{
	  getToken();
	  if (!getToken()->isCloseParen())
	    throw ErrorMessage() << _T("`&") << af.m_function->m_name
				 << _T("': too many arguments.");
	}
      }
      else if (isEOL() && *type == _T('&'))
	;
      else
	// has some arguments
      {
	if (!lookToken()->isOpenParen())
	{
	  if (*type == _T('&'))
	    goto ok;
	  throw ErrorMessage() << _T("there must be `(' after `&")
			       << af.m_function->m_name << _T("'.");
	}
	
	getToken();
	
	for (; *type; type ++)
	{
	  t = lookToken();
	  if (*type == _T('&'))
	    if (t->isCloseParen())
	      break;
	    else
	      type ++;
	  
	  if (t->isCloseParen())
	    throw ErrorMessage() << _T("`&")  << af.m_function->m_name
				 << _T("': too few arguments.");
	  switch (*type)
	  {
	    case _T('K'): // keymap
	    {
	      getToken();
	      Keymap *keymap =
		m_setting->m_keymaps.searchByName(t->getString());
	      if (!keymap)
		throw ErrorMessage() << _T("`") << *t
				     << _T("': unknown keymap name.");
	      t->setData((long)keymap);
	      break;
	    }

	    case _T('S'): // keyseq
	    {
	      getToken();
	      KeySeq *keySeq;
	      if (t->isOpenParen())
	      {
		keySeq = load_KEY_SEQUENCE(_T(""), true);
		getToken(); // close paren
	      }
	      else if (*t == _T("$"))
	      {
		t = getToken();
		keySeq =
		  m_setting->m_keySeqs.searchByName(t->getString());
		if (!keySeq)
		  throw ErrorMessage() << _T("`$") << *t
				       << _T("': unknown keyseq name.");
	      }
	      else
		throw ErrorMessage() << _T("`") << *t
				     << _T("': it is not keyseq.");
	      t->setData((long)keySeq);
	      break;
	    }

	    case _T('l'): // /lock\d/
	    {
	      getToken();
	      const static struct { const _TCHAR *s; Modifier::Type mt; }
	      map[] =
	      {
		{ _T("lock0"), Modifier::Type_Lock0 },
		{ _T("lock1"), Modifier::Type_Lock1 },
		{ _T("lock2"), Modifier::Type_Lock2 },
		{ _T("lock3"), Modifier::Type_Lock3 },
		{ _T("lock4"), Modifier::Type_Lock4 },
		{ _T("lock5"), Modifier::Type_Lock5 },
		{ _T("lock6"), Modifier::Type_Lock6 },
		{ _T("lock7"), Modifier::Type_Lock7 },
		{ _T("lock8"), Modifier::Type_Lock8 },
		{ _T("lock9"), Modifier::Type_Lock9 },
	      };
	      int i;
	      for (i = 0; i < NUMBER_OF(map); i++)
		if (*t == map[i].s)
		{
		  t->setData((long)map[i].mt);
		  break;
		}
	      if (i == NUMBER_OF(map))
		throw ErrorMessage() << _T("`") << *t
				     << _T("' unknown lock name.");
	      break;
	    }

	    case _T('s'): // string
	      getToken();
	      t->getString();
	      break;

	    case _T('d'): // digit
	      getToken();
	      t->getNumber();
	      break;

	    case _T('V'): // vkey
	    {
	      getToken();
	      long vkey = 0;
	      while (true)
	      {
		if (t->isNumber()) { vkey |= (BYTE)t->getNumber(); break; }
		else if (*t == _T("E-")) vkey |= Function::VK_extended;
		else if (*t == _T("U-")) vkey |= Function::VK_up;
		else if (*t == _T("D-")) vkey |= Function::VK_down;
		else
		{
		  const VKeyTable *vkt;
		  for (vkt = g_vkeyTable; vkt->m_name; vkt ++)
		    if (*t == vkt->m_name)
		      break;
		  if (!vkt->m_name)
		    throw ErrorMessage() << _T("`") << *t
					 << _T("': unknown virtual key name.");
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

	    case _T('w'): // window
	    {
	      getToken();
	      if (t->isNumber())
	      {
		if (t->getNumber() < Function::PM_toBegin)
		  throw ErrorMessage() << _T("`") << *t
				       << _T("': invalid target window");
		t->setData(t->getNumber());
		break;
	      }
	      const struct { int window; const _TCHAR *name; } window[] =
	      {
		{ Function::PM_toOverlappedWindow, _T("toOverlappedWindow") },
		{ Function::PM_toMainWindow, _T("toMainWindow") },
		{ Function::PM_toItself, _T("toItself") },
		{ Function::PM_toParentWindow, _T("toParentWindow") },
	      };
	      int i;
	      for (i = 0; i < NUMBER_OF(window); i++)
		if (*t == window[i].name)
		{
		  t->setData(window[i].window);
		  break;
		}
	      if (i == NUMBER_OF(window))
		throw ErrorMessage() << _T("`") << *t
				     << _T("': invalid target window.");
	      break;
	    }
	    
	    case _T('W'): // showWindow
	    {
	      getToken();
	      struct ShowType { int m_show; const _TCHAR *m_name; };
	      static const ShowType show[] =
	      {
#define SHOW(name) { SW_##name, _T(#name) }
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
		throw ErrorMessage() << _T("`") << *t
				     << _T("': unknown show command.");
	      break;
	    }

	    case _T('G'): // gravity
	    {
	      getToken();
	      const struct { int m_gravity; const _TCHAR *m_name; } gravity[] =
	      {
		Function::Gravity_C, _T("C"), 
		Function::Gravity_N, _T("N"), Function::Gravity_E, _T("E"),
		Function::Gravity_W, _T("W"), Function::Gravity_S, _T("S"),
		Function::Gravity_NW, _T("NW"), Function::Gravity_NW, _T("WN"),
		Function::Gravity_NE, _T("NE"), Function::Gravity_NE, _T("EN"),
		Function::Gravity_SW, _T("SW"), Function::Gravity_SW, _T("WS"),
		Function::Gravity_SE, _T("SE"), Function::Gravity_SE, _T("ES"),
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
		throw ErrorMessage() << _T("`") << *t
				     << _T("': unknown gravity symbol.");
	      break;
	    }

	    case _T('b'): // bool
	    {
	      getToken();
	      if (*t == _T("false"))
		t->setData(0);
	      else
		t->setData(1);
	      break;
	    }

	    case _T('D'): // dialog
	    {
	      getToken();
	      if (*t == _T("Investigate"))
		t->setData(Function::MayuDlg_investigate);
	      else if (*t == _T("Log"))
		t->setData(Function::MayuDlg_log);
	      else
		throw ErrorMessage() << _T("`") << *t
				     << _T("': unknown dialog box.");
	      break;
	    }

	    case _T('M'): // modifier
	    {
	      Modifier modifier;
	      for (int i = Modifier::Type_begin; i != Modifier::Type_end; i ++)
		modifier.dontcare((Modifier::Type)i);
	      modifier = load_MODIFIER(Modifier::Type_ASSIGN, modifier);
	      m_setting->m_modifiers.push_front(modifier);
	      t->setData((long)&*m_setting->m_modifiers.begin());
	      break;
	    }

	    case _T('&'): // rest is optional
	    default:
	      throw ErrorMessage() << _T("internal error.");
	  }
	  af.m_args.push_back(*t);
	}
	if (!getToken()->isCloseParen())
	  throw ErrorMessage() << _T("`&")  << af.m_function->m_name
			       << _T("': too many arguments.");
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
  std::list<ModifiedKey> assignedKeys;
  
  ModifiedKey mkey;
  mkey.m_modifier =
    load_MODIFIER(Modifier::Type_ASSIGN, m_defaultAssignModifier);
  if (*lookToken() == _T("="))
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
    if (*lookToken() == _T("=>"))
      break;
    mkey.m_modifier =
      load_MODIFIER(Modifier::Type_ASSIGN, m_defaultAssignModifier);
  }
  getToken();

  ASSERT(m_currentKeymap);
  KeySeq *keySeq = load_KEY_SEQUENCE();
  for (std::list<ModifiedKey>::iterator i = assignedKeys.begin();
       i != assignedKeys.end(); i++)
    m_currentKeymap->addAssignment(*i, keySeq);
}


// <EVENT_ASSIGN>
void SettingLoader::load_EVENT_ASSIGN()
{
  std::list<ModifiedKey> assignedKeys;

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
    throw ErrorMessage() << _T("`") << *t << _T("': invalid event name.");

  if (*getToken() != _T("=>"))
    throw ErrorMessage() << _T("`=>' is expected.");

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
    if      (*t == _T("!") ) am = Keymap::AM_true, t = getToken();
    else if (*t == _T("!!")) am = Keymap::AM_oneShot, t = getToken();
    
    if      (*t == _T("shift")) mt = Modifier::Type_Shift;
    else if (*t == _T("alt")  ||
	     *t == _T("meta") ||
	     *t == _T("menu") ) mt = Modifier::Type_Alt;
    else if (*t == _T("control") ||
	     *t == _T("ctrl") ) mt = Modifier::Type_Control;
    else if (*t == _T("windows") ||
	     *t == _T("win")  ) mt = Modifier::Type_Windows;
    else if (*t == _T("mod0") ) mt = Modifier::Type_Mod0;
    else if (*t == _T("mod1") ) mt = Modifier::Type_Mod1;
    else if (*t == _T("mod2") ) mt = Modifier::Type_Mod2;
    else if (*t == _T("mod3") ) mt = Modifier::Type_Mod3;
    else if (*t == _T("mod4") ) mt = Modifier::Type_Mod4;
    else if (*t == _T("mod5") ) mt = Modifier::Type_Mod5;
    else if (*t == _T("mod6") ) mt = Modifier::Type_Mod6;
    else if (*t == _T("mod7") ) mt = Modifier::Type_Mod7;
    else if (*t == _T("mod8") ) mt = Modifier::Type_Mod8;
    else if (*t == _T("mod9") ) mt = Modifier::Type_Mod9;
    else throw ErrorMessage() << _T("`") << *t
			      << _T("': invalid modifier name.");

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
  if      (*t == _T("=") ) ao = Keymap::AO_new;
  else if (*t == _T("+=")) ao = Keymap::AO_add;
  else if (*t == _T("-=")) ao = Keymap::AO_sub;
  else  throw ErrorMessage() << _T("`") << *t << _T("': is unknown operator.");

  // <ASSIGN_MODE>? <KEY_NAME>
  while (!isEOL())
  {
    // <ASSIGN_MODE>? 
    t = getToken();
    Keymap::AssignMode am = Keymap::AM_normal;
    if      (*t == _T("!") ) am = Keymap::AM_true, t = getToken();
    else if (*t == _T("!!")) am = Keymap::AM_oneShot, t = getToken();
    
    // <KEY_NAME>
    Key *key = m_setting->m_keyboard.searchKey(t->getString());
    if (!key)
      throw ErrorMessage() << _T("`") << *t << _T("': invalid key name.");

    // we can ignore warning C4701
    m_currentKeymap->addModifier(mt, ao, am, key);
    if (ao == Keymap::AO_new)
      ao = Keymap::AO_add;
  }
}


// <KEYSEQ_DEFINITION>
void SettingLoader::load_KEYSEQ_DEFINITION()
{
  if (*getToken() != _T("$"))
    throw ErrorMessage() << _T("there must be `$' after `keyseq'");
  Token *name = getToken();
  if (*getToken() != _T("="))
    throw ErrorMessage() << _T("there must be `=' after keyseq naem");
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
    throw ErrorMessage() << _T("there must be `(' after `if'.");
  Token *t = getToken(); // <SYMBOL> or !
  bool not = false;
  if (*t == _T("!"))
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
    throw ErrorMessage() << _T("there must be `)'.");

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
void SettingLoader::load_ELSE(bool i_isElseIf, const tstringi &i_token)
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
bool SettingLoader::load_ENDIF(const tstringi &i_token)
{
  if (m_canReadStack.size() == 0)
    throw ErrorMessage() << _T("unbalanced `") << i_token << _T("'");
  bool r = m_canReadStack.back();
  m_canReadStack.pop_back();
  return r;
}


// <LINE>
void SettingLoader::load_LINE()
{
  Token *i_token = getToken();

  // <COND_SYMBOL>
  if      (*i_token == _T("if") ||
	   *i_token == _T("and")) load_IF();
  else if (*i_token == _T("else")) load_ELSE(false, i_token->getString());
  else if (*i_token == _T("elseif") ||
	   *i_token == _T("elsif")  ||
	   *i_token == _T("elif")   ||
	   *i_token == _T("or")) load_ELSE(true, i_token->getString());
  else if (*i_token == _T("endif")) load_ENDIF(_T("endif"));
  else if (0 < m_canReadStack.size() && !m_canReadStack.back())
  {
    while (!isEOL())
      getToken();
  }
  else if (*i_token == _T("define")) load_DEFINE();
  // <INCLUDE>
  else if (*i_token == _T("include")) load_INCLUDE();
  // <KEYBOARD_DEFINITION>
  else if (*i_token == _T("def")) load_KEYBOARD_DEFINITION();
  // <KEYMAP_DEFINITION>
  else if (*i_token == _T("keymap")  ||
	   *i_token == _T("keymap2") ||
	   *i_token == _T("window")) load_KEYMAP_DEFINITION(i_token);
  // <KEY_ASSIGN>
  else if (*i_token == _T("key")) load_KEY_ASSIGN();
  // <EVENT_ASSIGN>
  else if (*i_token == _T("event")) load_EVENT_ASSIGN();
  // <MODIFIER_ASSIGNMENT>
  else if (*i_token == _T("mod")) load_MODIFIER_ASSIGNMENT();
  // <KEYSEQ_DEFINITION>
  else if (*i_token == _T("keyseq")) load_KEYSEQ_DEFINITION();
  else
    throw ErrorMessage() << _T("syntax error `") << *i_token << _T("'.");
}

  
// prefix sort predicate used in load(const string &)
static bool prefixSortPred(const tstringi &i_a, const tstringi &i_b)
{
  return i_b.size() < i_a.size();
}


// read file (UTF-16 LE/BE, UTF-8, locale specific multibyte encoding)
bool readFile(std::wstring *o_data, const std::wstring &i_filename)
{
  // get size of file
  struct _stat sbuf;
  if (_wstat(i_filename.c_str(), &sbuf) < 0 || sbuf.st_size == 0)
    return false;

  // open
  FILE *fp = _wfopen(i_filename.c_str(), _L("rb"));
  if (!fp)
    return false;
  
  // read file
  std::auto_ptr<BYTE> buf(new BYTE[sbuf.st_size + 1]);
  if (fread(buf.get(), sbuf.st_size, 1, fp) != 1)
  {
    fclose(fp);
    return false;
  }
  buf.get()[sbuf.st_size] = 0;			// mbstowcs() requires null
						// terminated string

  //
  if (buf.get()[0] == 0xffU && buf.get()[1] == 0xfeU &&
      sbuf.st_size % 2 == 0)
    // UTF-16 Little Endien
  {
    size_t size = sbuf.st_size / 2;
    o_data->resize(size);
    BYTE *p = buf.get();
    for (size_t i = 0; i < size; ++ i)
    {
      wchar_t c = static_cast<wchar_t>(*p ++);
      c |= static_cast<wchar_t>(*p ++) << 8;
      (*o_data)[i] = c;
    }
    fclose(fp);
    return true;
  }
  
  //
  if (buf.get()[0] == 0xfeU && buf.get()[1] == 0xffU &&
      sbuf.st_size % 2 == 0)
    // UTF-16 Big Endien
  {
    size_t size = sbuf.st_size / 2;
    o_data->resize(size);
    BYTE *p = buf.get();
    for (size_t i = 0; i < size; ++ i)
    {
      wchar_t c = static_cast<wchar_t>(*p ++) << 8;
      c |= static_cast<wchar_t>(*p ++);
      (*o_data)[i] = c;
    }
    fclose(fp);
    return true;
  }

  // try multibyte charset 
  size_t wsize = mbstowcs(NULL, reinterpret_cast<char *>(buf.get()), 0);
  if (wsize != size_t(-1))
  {
    std::auto_ptr<wchar_t> wbuf(new wchar_t[wsize]);
    mbstowcs(wbuf.get(), reinterpret_cast<char *>(buf.get()), wsize);
    o_data->assign(wbuf.get(), wbuf.get() + wsize);
    fclose(fp);
    return true;
  }
  
  // try UTF-8
  {
    std::auto_ptr<wchar_t> wbuf(new wchar_t[sbuf.st_size]);
    BYTE *f = buf.get();
    BYTE *end = buf.get() + sbuf.st_size;
    wchar_t *d = wbuf.get();
    enum { STATE_1, STATE_2of2, STATE_2of3, STATE_3of3 } state = STATE_1;
    
    while (f != end)
    {
      switch (state)
      {
	case STATE_1:
	  if (!(*f & 0x80))			// 0xxxxxxx: 00-7F
	    *d++ = static_cast<wchar_t>(*f++);
	  else if ((*f & 0xe0) == 0xc0)		// 110xxxxx 10xxxxxx: 0080-07FF
	  {
	    *d = ((static_cast<wchar_t>(*f++) & 0x1f) << 6);
	    state = STATE_2of2;
	  }
	  else if ((*f & 0xf0) == 0xe0)		// 1110xxxx 10xxxxxx 10xxxxxx:
						// 0800 - FFFF
	  {
	    *d = ((static_cast<wchar_t>(*f++) & 0x0f) << 12);
	    state = STATE_2of3;
	  }
	  else
	    goto not_UTF_8;
	  break;
	  
	case STATE_2of2:
	case STATE_3of3:
	  if ((*f & 0xc0) != 0x80)
	    goto not_UTF_8;
	  *d++ |= (static_cast<wchar_t>(*f++) & 0x3f);
	  state = STATE_1;
	  break;

	case STATE_2of3:
	  if ((*f & 0xc0) != 0x80)
	    goto not_UTF_8;
	  *d |= ((static_cast<wchar_t>(*f++) & 0x3f) << 6);
	  state = STATE_3of3;
	  break;
      }
    }
    o_data->assign(wbuf.get(), d);
    fclose(fp);
    return true;
    
    not_UTF_8: ;
  }

  // assume ascii
  o_data->resize(sbuf.st_size);
  for (size_t i = 0; i < sbuf.st_size; ++ i)
    (*o_data)[i] = buf.get()[i];
  fclose(fp);
  return true;
}


// load (called from load(Setting *, const tstringi &) only)
void SettingLoader::load(const tstringi &i_filename)
{
  m_currentFilename = i_filename;
#ifdef UNICODE
  std::wstring data;
  if (!readFile(&data, m_currentFilename))
  {
    Acquire a(m_soLog);
    *m_log << m_currentFilename << _T(" : error: file not found");
    m_isThereAnyError = true;
    return;
  }
  std::wstringstream ist(data);
#else
  tifstream ist(m_currentFilename.c_str());
#endif

  // prefix
  if (m_prefixesRefCcount == 0)
  {
    static const _TCHAR *prefixes[] =
    {
      _T("="), _T("=>"), _T("&&"), _T("||"), _T(":"), _T("$"), _T("&"),
      _T("-="), _T("+="), _T("!"), _T("!!"), 
      _T("E0-"), _T("E1-"),			// <SCAN_CODE_EXTENTION>
      _T("S-"), _T("A-"), _T("M-"), _T("C-"),	// <BASIC_MODIFIER>
      _T("W-"), _T("*"), _T("~"),
      _T("U-"), _T("D-"),			// <KEYSEQ_MODIFIER>
      _T("IL-"), _T("IC-"), _T("I-"),		// <ASSIGN_MODIFIER>
      _T("NL-"), _T("CL-"), _T("SL-"),
      _T("M0-"), _T("M1-"), _T("M2-"), _T("M3-"), _T("M4-"),
      _T("M5-"), _T("M6-"), _T("M7-"), _T("M8-"), _T("M9-"), 
      _T("L0-"), _T("L1-"), _T("L2-"), _T("L3-"), _T("L4-"),
      _T("L5-"), _T("L6-"), _T("L7-"), _T("L8-"), _T("L9-"), 
    };
    m_prefixes = new std::vector<tstringi>;
    for (size_t i = 0; i < NUMBER_OF(prefixes); ++ i)
      m_prefixes->push_back(prefixes[i]);
    std::sort(m_prefixes->begin(), m_prefixes->end(), prefixSortPred);
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
	*m_log << m_currentFilename << _T("(") << parser.getLineNumber()
	     << _T(") : error: ") << e << std::endl;
      }
      m_isThereAnyError = true;
      continue;
    }
      
    try
    {
      load_LINE();
      if (!isEOL())
	throw WarningMessage() << _T("back garbage is ignored.");
    }
    catch (WarningMessage &w)
    {
      if (m_log && m_soLog)
      {
	Acquire a(m_soLog);
	*m_log << i_filename << _T("(") << parser.getLineNumber()
	       << _T(") : warning: ") << w << std::endl;
      }
    }
    catch (ErrorMessage &e)
    {
      if (m_log && m_soLog)
      {
	Acquire a(m_soLog);
	*m_log << i_filename << _T("(") << parser.getLineNumber()
	       << _T(") : error: ") << e << std::endl;
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
    *m_log << m_currentFilename << _T("(") << parser.getLineNumber()
	   << _T(") : error: unbalanced `if'.  ")
	   << _T("you forget `endif', didn'i_token you?")
	   << std::endl;
    m_isThereAnyError = true;
  }
}


// is the filename readable ?
bool SettingLoader::isReadable(const tstringi &i_filename) const 
{
  if (i_filename.empty())
    return false;
#ifdef UNICODE
  tifstream ist(to_string(i_filename).c_str());
#else
  tifstream ist(i_filename.c_str());
#endif
  if (ist.good())
  {
    if (m_log && m_soLog)
    {
      Acquire a(m_soLog, 1);
      *m_log << _T("loading: ") << i_filename << std::endl;
    }
    return true;
  }
  else
  {
    if (m_log && m_soLog)
    {
      Acquire a(m_soLog, 1);
      *m_log << _T("     no: ") << i_filename << std::endl;
    }
    return false;
  }
}

#if 0
// get filename from registry
bool SettingLoader::getFilenameFromRegistry(tstringi *o_path) const
{
  // get from registry
  Registry reg(MAYU_REGISTRY_ROOT);
  int index;
  reg.read(_T(".mayuIndex"), &index, 0);
  char buf[100];
  snprintf(buf, NUMBER_OF(buf), _T(".mayu%d"), index);
  if (!reg.read(buf, o_path))
    return false;

  // parse registry entry
  Regexp getFilename(_T("^[^;]*;([^;]*);(.*)$"));
  if (!getFilename.doesMatch(*o_path))
    return false;
  
  tstringi path = getFilename[1];
  tstringi options = getFilename[2];
  
  if (!(0 < path.size() && isReadable(path)))
    return false;
  *o_path = path;
  
  // set symbols
  Regexp symbol(_T("-D([^;]*)"));
  while (symbol.doesMatch(options))
  {
    m_setting->symbols.insert(symbol[1]);
    options = options.substr(symbol.subBegin(1));
  }
  
  return true;
}
#endif


// get filename
bool SettingLoader::getFilename(const tstringi &i_name, tstringi *o_path) const
{
  // the default filename is ".mayu"
  const tstringi &name = i_name.empty() ? tstringi(_T(".mayu")) : i_name;
  
  bool isFirstTime = true;

  while (true)
  {
    // find file from registry
    if (i_name.empty())				// called not from 'include'
    {
      std::list<tstringi> symbols;
      if (getFilenameFromRegistry(NULL, o_path, &symbols))
      {
	if (!isReadable(*o_path))
	  return false;
	for (std::list<tstringi>::iterator
	       i = symbols.begin(); i != symbols.end(); i ++)
	  m_setting->m_symbols.insert(*i);
	return true;
      }
    }
    
    if (!isFirstTime)
      return false;
    
    // find file from home directory
    std::list<tstringi> pathes;
    getHomeDirectories(&pathes);
    for (std::list<tstringi>::iterator
	   i = pathes.begin(); i != pathes.end(); i ++)
    {
      *o_path = *i + _T("\\") + name;
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
SettingLoader::SettingLoader(SyncObject *i_soLog, tostream *i_log)
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
bool SettingLoader::load(Setting *i_setting, const tstringi &i_filename)
{
  m_setting = i_setting;
  m_isThereAnyError = false;
    
  tstringi path;
  if (!getFilename(i_filename, &path))
  {
    if (i_filename.empty())
    {
      Acquire a(m_soLog);
      *m_log << _T("error: failed to load ~/.mayu") << std::endl;
      return false;
    }
    else
      throw ErrorMessage() << _T("`") << i_filename
			   << _T("': no such file or other error.");
  }

  // create global keymap's default keySeq
  ActionFunction af;
  af.m_function = Function::search(Function::Id_OtherWindowClass);
  ASSERT( af.m_function );
  KeySeq *globalDefault = m_setting->m_keySeqs.add(KeySeq(_T("")).add(af));
  
  // add default keymap
  m_currentKeymap = m_setting->m_keymaps.add(
    Keymap(Keymap::Type_windowOr, _T("Global"), _T(""), _T(""),
	   globalDefault, NULL));

  /*
  // add keyboard layout name
  if (filename.empty())
  {
    char keyboardLayoutName[KL_NAMELENGTH];
    if (GetKeyboardLayoutName(keyboardLayoutName))
    {
      tstringi kl = tstringi(_T("KeyboardLayout/")) + keyboardLayoutName;
      m_setting->symbols.insert(kl);
      Acquire a(m_soLog);
      *m_log << _T("KeyboardLayout: ") << kl << std::endl;
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


std::vector<tstringi> *SettingLoader::m_prefixes; // m_prefixes terminal symbol
size_t SettingLoader::m_prefixesRefCcount;	// reference count of
						// m_prefixes
