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
  return ti == tokens.end();
}


// get next token
Token *SettingLoader::getToken()
{
  if (isEOL())
    throw ErrorMessage() << "too few words.";
  return &*(ti++);
}

  
// look next token
Token *SettingLoader::lookToken()
{
  if (isEOL())
    throw ErrorMessage() << "too few words.";
  return &*ti;
}


// <INCLUDE>
void SettingLoader::load_INCLUDE()
{
  SettingLoader loader(soLog, log);
  loader.defaultAssignModifier = defaultAssignModifier;
  loader.defaultKeySeqModifier = defaultKeySeqModifier;
  if (!loader.load(setting, (*getToken()).getString()))
    isThereAnyError = true;
}


// <SCAN_CODES>
void SettingLoader::load_SCAN_CODES(Key *key_r)
{
  for (int j = 0; j < Key::maxlengthof_scanCodes && !isEOL(); j++)
  {
    ScanCode sc;
    sc.flags = 0;
    while (true)
    {
      Token *t = getToken();
      if (t->isNumber())
      {
	sc.scan = (u_char)t->getNumber();
	key_r->addScanCode(sc);
	break;
      }
      if      (*t == "E0-") sc.flags |= ScanCode::E0;
      else if (*t == "E1-") sc.flags |= ScanCode::E1;
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
  setting->keyboard.addKey(key);
}


// <DEFINE_MODIFIER>
void SettingLoader::load_DEFINE_MODIFIER()
{
  Token *t = getToken();
  Modifier::Type mt;
  if      (*t == "shift"  ) mt = Modifier::Shift;
  else if (*t == "alt"     ||
	   *t == "meta"    ||
	   *t == "menu"   ) mt = Modifier::Alt;
  else if (*t == "control" ||
	   *t == "ctrl"   ) mt = Modifier::Control;
  else if (*t == "windows" ||
	   *t == "win"    ) mt = Modifier::Windows;
  else throw ErrorMessage() << "`" << *t << "': invalid modifier name.";
    
  if (*getToken() != "=")
    throw ErrorMessage() << "there must be `=' after modifier name.";
    
  while (!isEOL())
  {
    t = getToken();
    Key *key =
      setting->keyboard.searchKeyByNonAliasName(t->getString());
    if (!key)
      throw ErrorMessage() << "`" << *t << "': invalid key name.";
    setting->keyboard.addModifier(mt, key);
  }
}


// <DEFINE_SYNC_KEY>
void SettingLoader::load_DEFINE_SYNC_KEY()
{
  Key *key = setting->keyboard.getSyncKey();
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
  Key *key = setting->keyboard.searchKeyByNonAliasName(t->getString());
  if (!key)
    throw ErrorMessage() << "`" << *t << "': invalid key name.";
  setting->keyboard.addAlias(name->getString(), key);
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
Modifier SettingLoader::load_MODIFIER(Modifier::Type mode, Modifier modifier)
{
  Modifier isModifierSpecified;
  enum { PRESS, RELEASE, DONTCARE } flag = PRESS;

  int i;
  for (i = mode; i < Modifier::ASSIGN; i++)
  {
    modifier.dontcare(Modifier::Type(i));
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
      { "S-",  Modifier::Shift },
      { "A-",  Modifier::Alt },
      { "M-",  Modifier::Alt },
      { "C-",  Modifier::Control },
      { "W-",  Modifier::Windows },
      // <KEYSEQ_MODIFIER>
      { "U-",  Modifier::Up },
      { "D-",  Modifier::Down },
      // <ASSIGN_MODIFIER>
      { "IL-", Modifier::ImeLock },
      { "IC-", Modifier::ImeComp },
      { "I-",  Modifier::ImeComp },
      { "NL-", Modifier::NumLock },
      { "CL-", Modifier::CapsLock },
      { "SL-", Modifier::ScrollLock },
      { "M0-", Modifier::Mod0 },
      { "M1-", Modifier::Mod1 },
      { "M2-", Modifier::Mod2 },
      { "M3-", Modifier::Mod3 },
      { "M4-", Modifier::Mod4 },
      { "M5-", Modifier::Mod5 },
      { "M6-", Modifier::Mod6 },
      { "M7-", Modifier::Mod7 },
      { "M8-", Modifier::Mod8 },
      { "M9-", Modifier::Mod9 },
      { "L0-", Modifier::Lock0 },
      { "L1-", Modifier::Lock1 },
      { "L2-", Modifier::Lock2 },
      { "L3-", Modifier::Lock3 },
      { "L4-", Modifier::Lock4 },
      { "L5-", Modifier::Lock5 },
      { "L6-", Modifier::Lock6 },
      { "L7-", Modifier::Lock7 },
      { "L8-", Modifier::Lock8 },
      { "L9-", Modifier::Lock9 },
    };

    for (int i = 0; i < NUMBER_OF(map); i++)
      if (*t == map[i].s)
      {
	getToken();
	Modifier::Type mt = map[i].mt;
	if ((int)mode <= (int)mt)
	  throw ErrorMessage() << "`" << *t
			       << "': invalid modifier at this context.";
	switch (flag)
	{
	  case PRESS: modifier.press(mt); break;
	  case RELEASE: modifier.release(mt); break;
	  case DONTCARE: modifier.dontcare(mt); break;
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
  
  for (i = Modifier::begin; i != Modifier::end; i++)
    if (!isModifierSpecified.isOn(Modifier::Type(i)))
      switch (flag)
      {
	case PRESS: break;
	case RELEASE: modifier.release(Modifier::Type(i)); break;
	case DONTCARE: modifier.dontcare(Modifier::Type(i)); break;
      }

  // fix up and down
  bool isDontcareUp   = modifier.isDontcare(Modifier::Up);
  bool isDontcareDown = modifier.isDontcare(Modifier::Down);
  bool isOnUp         = modifier.isOn(Modifier::Up);
  bool isOnDown       = modifier.isOn(Modifier::Down);
  if (isDontcareUp && isDontcareDown)
    ;
  else if (isDontcareUp)
    modifier.on(Modifier::Up, !isOnDown);
  else if (isDontcareDown)
    modifier.on(Modifier::Down, !isOnUp);
  else if (isOnUp == isOnDown)
  {
    modifier.dontcare(Modifier::Up);
    modifier.dontcare(Modifier::Down);
  }
  return modifier;
}


// <KEY_NAME>
Key *SettingLoader::load_KEY_NAME()
{
  Token *t = getToken();
  Key *key = setting->keyboard.searchKey(t->getString());
  if (!key)
    throw ErrorMessage() << "`" << *t << "': invalid key name.";
  return key;
}


// <KEYMAP_DEFINITION>
void SettingLoader::load_KEYMAP_DEFINITION(Token *which)
{
  Keymap::Type type = Keymap::TypeKeymap;
  Token *name = getToken();	// <KEYMAP_NAME>
  istring windowClassName;
  istring windowTitleName;
  KeySeq *keySeq = NULL;
  Keymap *parentKeymap = NULL;
  bool isKeymap2 = false;

  if (!isEOL())
  {
    Token *t = lookToken();
    if (*which == "window")	// <WINDOW>
    {
      if (t->isOpenParen())
	// "(" <WINDOW_CLASS_NAME> "&&" <WINDOW_TITLE_NAME> ")"
	// "(" <WINDOW_CLASS_NAME> "||" <WINDOW_TITLE_NAME> ")"
      {
	getToken();
	windowClassName = getToken()->getRegexp();
	t = getToken();
	if (*t == "&&")
	  type = Keymap::TypeWindowAnd;
	else if (*t == "||")
	  type = Keymap::TypeWindowOr;
	else
	  throw ErrorMessage() << "`" << *t << "': unknown operator.";
	windowTitleName = getToken()->getRegexp();
	if (!getToken()->isCloseParen())
	  throw ErrorMessage() << "there must be `)'.";
      }
      else if (t->isRegexp())	// <WINDOW_CLASS_NAME>
      {
	getToken();
	type = Keymap::TypeWindowAnd;
	windowClassName = t->getRegexp();
      }
    }
    else if (*which == "keymap")
      ;
    else if (*which == "keymap2")
      isKeymap2 = true;
    else
      assert(false);
    
    if (!isEOL())
    {
      t = lookToken();
      // <KEYMAP_PARENT>
      if (*t == ":")
      {
	getToken();
	t = getToken();
	parentKeymap = setting->keymaps.searchByName(t->getString());
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
    if (type == Keymap::TypeKeymap && !isKeymap2)
      id = Function::KeymapParent;
    else if (type == Keymap::TypeKeymap && !isKeymap2)
      id = Function::Undefined;
    else // (type == Keymap::TypeWindowAnd || type == Keymap::TypeWindowOr)
      id = Function::KeymapParent;
    ActionFunction af;
    af.function = Function::search(id);
    assert( af.function );
    keySeq = setting->keySeqs.add(KeySeq(name->getString()).add(af));
  }
  
  currentKeymap =
    setting->keymaps.add(
      Keymap(type, name->getString(), windowClassName, windowTitleName,
	     keySeq, parentKeymap));
}


// <KEY_SEQUENCE>
KeySeq *SettingLoader::load_KEY_SEQUENCE(const istring &name, bool isInParen)
{
  KeySeq keySeq(name);
  while (!isEOL())
  {
    Modifier modifier = load_MODIFIER(Modifier::KEYSEQ, defaultKeySeqModifier);
    Token *t = lookToken();
    if (t->isCloseParen() && isInParen)
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
      KeySeq *ks = setting->keySeqs.searchByName(t->getString());
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
      af.modifier = modifier;
      af.function = Function::search(t->getString());
      if (af.function == NULL)
	throw ErrorMessage() << "`&" << *t << "': unknown function name.";

      // get arguments
      const char *type = af.function->argType;
      if (type == NULL)
	// no argument
      {
	if (!isEOL() && lookToken()->isOpenParen())
	{
	  getToken();
	  if (!getToken()->isCloseParen())
	    throw ErrorMessage() << "`&" << af.function->name
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
			       << af.function->name << "'.";
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
	    throw ErrorMessage() << "`&"  << af.function->name
				 << "': too few arguments.";
	  switch (*type)
	  {
	    case 'K': // keymap
	    {
	      getToken();
	      Keymap *keymap = setting->keymaps.searchByName(t->getString());
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
		keySeq = setting->keySeqs.searchByName(t->getString());
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
		{ "lock0", Modifier::Lock0 },
		{ "lock1", Modifier::Lock1 },
		{ "lock2", Modifier::Lock2 },
		{ "lock3", Modifier::Lock3 },
		{ "lock4", Modifier::Lock4 },
		{ "lock5", Modifier::Lock5 },
		{ "lock6", Modifier::Lock6 },
		{ "lock7", Modifier::Lock7 },
		{ "lock8", Modifier::Lock8 },
		{ "lock9", Modifier::Lock9 },
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
		else if (*t == "E-") vkey |= Function::vkExtended;
		else if (*t == "U-") vkey |= Function::vkUp;
		else if (*t == "D-") vkey |= Function::vkDown;
		else
		{
		  const VKeyTable *vkt;
		  for (vkt = vkeyTable; vkt->name; vkt ++)
		    if (*t == vkt->name)
		      break;
		  if (!vkt->name)
		    throw ErrorMessage() << "`" << *t
					 << "': unknown virtual key name.";
		  vkey |= vkt->code;
		  break;
		}
		t = getToken();
	      }
	      if (!(vkey & Function::vkUp) && !(vkey & Function::vkDown))
		vkey |= Function::vkUp | Function::vkDown;
	      t->setData(vkey);
	      break;
	    }

	    case 'w': // window
	    {
	      getToken();
	      if (t->isNumber())
	      {
		if (t->getNumber() < Function::toBegin)
		  throw ErrorMessage() << "`" << *t
				       << "': invalid target window";
		t->setData(t->getNumber());
		break;
	      }
	      const struct { int window; char *name; } window[] =
	      {
		{ Function::toOverlappedWindow, "toOverlappedWindow" },
		{ Function::toMainWindow, "toMainWindow" },
		{ Function::toItself, "toItself" },
		{ Function::toParentWindow, "toParentWindow" },
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
	      const struct { int show; char *name; } show[] =
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
	      for (i = 0; i < NUMBER_OF(show); i++)
		if (*t == show[i].name)
		{
		  t->setData(show[i].show);
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
		Function::GravityC, "C", 
		Function::GravityN, "N", Function::GravityE, "E",
		Function::GravityW, "W", Function::GravityS, "S",
		Function::GravityNW, "NW", Function::GravityNW, "WN",
		Function::GravityNE, "NE", Function::GravityNE, "EN",
		Function::GravitySW, "SW", Function::GravitySW, "WS",
		Function::GravitySE, "SE", Function::GravitySE, "ES",
	      };
	      t->setData(Function::GravityNW);
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
		t->setData(Function::mayuDlgInvestigate);
	      else if (*t == "Log")
		t->setData(Function::mayuDlgLog);
	      else
		throw ErrorMessage() << "`" << *t
				     << "': unknown dialog box.";
	      break;
	    }

	    case 'M': // modifier
	    {
	      Modifier modifier;
	      for (int i = Modifier::begin; i != Modifier::end; i ++)
		modifier.dontcare((Modifier::Type)i);
	      modifier = load_MODIFIER(Modifier::ASSIGN, modifier);
	      setting->modifiers.push_front(modifier);
	      t->setData((long)&*setting->modifiers.begin());
	      break;
	    }

	    case '&': // rest is optional
	    default:
	      throw ErrorMessage() << "internal error.";
	  }
	  af.args.push_back(*t);
	}
	if (!getToken()->isCloseParen())
	  throw ErrorMessage() << "`&"  << af.function->name
			       << "': too many arguments.";
	ok: ;
      }
      keySeq.add(af);
    }
    else // <KEYSEQ_MODIFIED_KEY_NAME>
    {
      ModifiedKey mkey;
      mkey.modifier = modifier;
      mkey.key = load_KEY_NAME();
      keySeq.add(ActionKey(mkey));
    }
  }
  return setting->keySeqs.add(keySeq);
}


// <KEY_ASSIGN>
void SettingLoader::load_KEY_ASSIGN()
{
  list<ModifiedKey> assignedKeys;
  
  ModifiedKey mkey;
  mkey.modifier = load_MODIFIER(Modifier::ASSIGN, defaultAssignModifier);
  if (*lookToken() == "=")
  {
    getToken();
    defaultKeySeqModifier = load_MODIFIER(Modifier::KEYSEQ,
					  defaultKeySeqModifier);
    defaultAssignModifier = mkey.modifier;
    return;
  }
  
  while (true)
  {
    mkey.key = load_KEY_NAME();
    assignedKeys.push_back(mkey);
    if (*lookToken() == "=>")
      break;
    mkey.modifier = load_MODIFIER(Modifier::ASSIGN, defaultAssignModifier);
  }
  getToken();

  assert(currentKeymap);
  KeySeq *keySeq = load_KEY_SEQUENCE();
  for (list<ModifiedKey>::iterator i = assignedKeys.begin();
       i != assignedKeys.end(); i++)
    currentKeymap->addAssignment(*i, keySeq);
}


// <EVENT_ASSIGN>
void SettingLoader::load_EVENT_ASSIGN()
{
  list<ModifiedKey> assignedKeys;

  ModifiedKey mkey;
  mkey.modifier.dontcare();			//set all modifiers to dontcare
  
  Token *t = getToken();
  Key **e;
  for (e = Event::events; *e; e ++)
    if (*t == (*e)->getName())
    {
      mkey.key = *e;
      break;
    }
  if (!*e)
    throw ErrorMessage() << "`" << *t << "': invalid event name.";

  if (*getToken() != "=>")
    throw ErrorMessage() << "`=>' is expected.";

  assert(currentKeymap);
  KeySeq *keySeq = load_KEY_SEQUENCE();
  currentKeymap->addAssignment(mkey, keySeq);
}


// <MODIFIER_ASSIGNMENT>
void SettingLoader::load_MODIFIER_ASSIGNMENT()
{
  // <MODIFIER_NAME>
  Token *t = getToken();
  Modifier::Type mt;

  while (true)
  {
    Keymap::AssignMode am = Keymap::AMnotModifier;
    if      (*t == "!" ) am = Keymap::AMtrue, t = getToken();
    else if (*t == "!!") am = Keymap::AMoneShot, t = getToken();
    
    if      (*t == "shift") mt = Modifier::Shift;
    else if (*t == "alt"  ||
	     *t == "meta" ||
	     *t == "menu" ) mt = Modifier::Alt;
    else if (*t == "control" ||
	     *t == "ctrl" ) mt = Modifier::Control;
    else if (*t == "windows" ||
	     *t == "win"  ) mt = Modifier::Windows;
    else if (*t == "mod0" ) mt = Modifier::Mod0;
    else if (*t == "mod1" ) mt = Modifier::Mod1;
    else if (*t == "mod2" ) mt = Modifier::Mod2;
    else if (*t == "mod3" ) mt = Modifier::Mod3;
    else if (*t == "mod4" ) mt = Modifier::Mod4;
    else if (*t == "mod5" ) mt = Modifier::Mod5;
    else if (*t == "mod6" ) mt = Modifier::Mod6;
    else if (*t == "mod7" ) mt = Modifier::Mod7;
    else if (*t == "mod8" ) mt = Modifier::Mod8;
    else if (*t == "mod9" ) mt = Modifier::Mod9;
    else throw ErrorMessage() << "`" << *t << "': invalid modifier name.";

    if (am == Keymap::AMnotModifier)
      break;
    
    currentKeymap->addModifier(mt, Keymap::AOoverwrite, am, NULL);
    if (isEOL())
      return;
    t = getToken();
  }
  
  // <ASSIGN_OP>
  t = getToken();
  Keymap::AssignOperator ao;
  if      (*t == "=" ) ao = Keymap::AOnew;
  else if (*t == "+=") ao = Keymap::AOadd;
  else if (*t == "-=") ao = Keymap::AOsub;
  else  throw ErrorMessage() << "`" << *t << "': is unknown operator.";

  // <ASSIGN_MODE>? <KEY_NAME>
  while (!isEOL())
  {
    // <ASSIGN_MODE>? 
    t = getToken();
    Keymap::AssignMode am = Keymap::AMnormal;
    if      (*t == "!" ) am = Keymap::AMtrue, t = getToken();
    else if (*t == "!!") am = Keymap::AMoneShot, t = getToken();
    
    // <KEY_NAME>
    Key *key = setting->keyboard.searchKey(t->getString());
    if (!key)
      throw ErrorMessage() << "`" << *t << "': invalid key name.";

    currentKeymap->addModifier(mt, ao, am, key); // we can ignore warning C4701
    if (ao == Keymap::AOnew)
      ao = Keymap::AOadd;
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
  setting->symbols.insert(getToken()->getString());
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
  
  bool doesSymbolExist = (setting->symbols.find(t->getString())
			  != setting->symbols.end());
  bool doesRead = ((doesSymbolExist && !not) ||
		   (!doesSymbolExist && not));
  if (0 < canRead.size())
    doesRead = doesRead && canRead.back();
  
  if (!getToken()->isCloseParen())
    throw ErrorMessage() << "there must be `)'.";

  canRead.push_back(doesRead);
  if (!isEOL())
  {
    size_t len = canRead.size();
    load_LINE();
    if (len < canRead.size())
    {
      bool r = canRead.back();
      canRead.pop_back();
      canRead[len - 1] = r && doesRead;
    }
    else if (len == canRead.size())
      canRead.pop_back();
    else
      ; // `end' found
  }
}


// <ELSE> <ELSEIF>
void SettingLoader::load_ELSE(bool isElseIf, const istring &t)
{
  bool doesRead = !load_ENDIF(t);
  if (0 < canRead.size())
    doesRead = doesRead && canRead.back();
  canRead.push_back(doesRead);
  if (!isEOL())
  {
    size_t len = canRead.size();
    if (isElseIf)
      load_IF();
    else
      load_LINE();
    if (len < canRead.size())
    {
      bool r = canRead.back();
      canRead.pop_back();
      canRead[len - 1] = doesRead && r;
    }
    else if (len == canRead.size())
      canRead.pop_back();
    else
      ; // `end' found
  }
}


// <ENDIF>
bool SettingLoader::load_ENDIF(const istring &t)
{
  if (canRead.size() == 0)
    throw ErrorMessage() << "unbalanced `" << t << "'";
  bool r = canRead.back();
  canRead.pop_back();
  return r;
}


// <LINE>
void SettingLoader::load_LINE()
{
  Token *t = getToken();

  // <COND_SYMBOL>
  if      (*t == "if" ||
	   *t == "and") load_IF();
  else if (*t == "else") load_ELSE(false, t->getString());
  else if (*t == "elseif" ||
	   *t == "elsif"  ||
	   *t == "elif"   ||
	   *t == "or") load_ELSE(true, t->getString());
  else if (*t == "endif") load_ENDIF("endif");
  else if (0 < canRead.size() && !canRead.back())
  {
    while (!isEOL())
      getToken();
  }
  else if (*t == "define") load_DEFINE();
  // <INCLUDE>
  else if (*t == "include") load_INCLUDE();
  // <KEYBOARD_DEFINITION>
  else if (*t == "def") load_KEYBOARD_DEFINITION();
  // <KEYMAP_DEFINITION>
  else if (*t == "keymap"  ||
	   *t == "keymap2" ||
	   *t == "window") load_KEYMAP_DEFINITION(t);
  // <KEY_ASSIGN>
  else if (*t == "key") load_KEY_ASSIGN();
  // <EVENT_ASSIGN>
  else if (*t == "event") load_EVENT_ASSIGN();
  // <MODIFIER_ASSIGNMENT>
  else if (*t == "mod") load_MODIFIER_ASSIGNMENT();
  // <KEYSEQ_DEFINITION>
  else if (*t == "keyseq") load_KEYSEQ_DEFINITION();
  else
    throw ErrorMessage() << "syntax error `" << *t << "'.";
}

  
// prefix sort predicate used in load(const string &)
static bool prefixSortPred(const istring &a, const istring &b)
{
  return b.size() < a.size();
}


// load (called from load(Setting *, const istring &) only)
void SettingLoader::load(const istring &filename)
{
  currentFilename = filename;
  ifstream ist(currentFilename.c_str());

  // prefix
  if (refcountof_prefix == 0)
  {
    static const char *prefix_[] =
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
    prefix = new vector<istring>;
    for (int i = 0; i < NUMBER_OF(prefix_); i++)
      prefix->push_back(prefix_[i]);
    sort(prefix->begin(), prefix->end(), prefixSortPred);
  }
  refcountof_prefix ++;

  // create parser
  Parser parser(ist);
  parser.setPrefix(prefix);
    
  while (true)
  {
    try
    {
      if (!parser.getLine(&tokens))
	break;
      ti = tokens.begin();
    }
    catch (ErrorMessage &e)
    {
      if (log && soLog)
      {
	Acquire a(soLog);
	*log << currentFilename << "(" << parser.getLineNumber()
	     << ") : error: " << e << endl;
      }
      isThereAnyError = true;
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
      if (log && soLog)
      {
	Acquire a(soLog);
	*log << filename << "(" << parser.getLineNumber()
	     << ") : warning: " << w << endl;
      }
    }
    catch (ErrorMessage &e)
    {
      if (log && soLog)
      {
	Acquire a(soLog);
	*log << filename << "(" << parser.getLineNumber()
	     << ") : error: " << e << endl;
      }
      isThereAnyError = true;
    }
  }
    
  // prefix
  refcountof_prefix --;
  if (refcountof_prefix == 0)
    delete prefix;

  if (0 < canRead.size())
  {
    Acquire a(soLog);
    *log << currentFilename << "(" << parser.getLineNumber()
	 << ") : error: unbalanced `if'.  you forget `endif', didn't you?"
	 << endl;
    isThereAnyError = true;
  }
}


// is the filename readable ?
bool SettingLoader::isReadable(const istring &filename) const 
{
  if (filename.empty())
    return false;
  ifstream ist(filename.c_str());
  if (ist.good())
  {
    if (log && soLog)
    {
      Acquire a(soLog, 1);
      *log << "loading: " << filename << endl;
    }
    return true;
  }
  else
  {
    if (log && soLog)
    {
      Acquire a(soLog, 1);
      *log << "     no: " << filename << endl;
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
    setting->symbols.insert(symbol[1]);
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
	  setting->symbols.insert(*i);
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
    
    if (!DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_setting),
		   NULL, dlgSetting_dlgProc))
      return false;
  }
}


// constructor
SettingLoader::SettingLoader(SyncObject *soLog_, ostream *log_)
  : setting(NULL),
    isThereAnyError(false),
    soLog(soLog_),
    log(log_),
    currentKeymap(NULL)
{
  defaultKeySeqModifier =
    defaultAssignModifier.release(Modifier::ImeComp);
}


/* load setting
   If called by "include", 'filename' describes filename.
   Otherwise the 'filename' is empty.
 */
bool SettingLoader::load(Setting *setting_r, const istring &filename)
{
  setting = setting_r;
  isThereAnyError = false;
    
  istring path;
  if (!getFilename(filename, &path))
  {
    if (filename.empty())
    {
      Acquire a(soLog);
      *log << "error: failed to load ~/.mayu" << endl;
      return false;
    }
    else
      throw ErrorMessage() << "`" << filename
			   << "': no such file or other error.";
  }

  // create global keymap's default keySeq
  ActionFunction af;
  af.function = Function::search(Function::OtherWindowClass);
  assert( af.function );
  KeySeq *globalDefault = setting->keySeqs.add(KeySeq("").add(af));
  
  // add default keymap
  currentKeymap = setting->keymaps.add(
    Keymap(Keymap::TypeWindowOr, "Global", "", "", globalDefault, NULL));

  /*
  // add keyboard layout name
  if (filename.empty())
  {
    char keyboardLayoutName[KL_NAMELENGTH];
    if (GetKeyboardLayoutName(keyboardLayoutName))
    {
      istring kl = istring("KeyboardLayout/") + keyboardLayoutName;
      setting->symbols.insert(kl);
      Acquire a(soLog);
      *log << "KeyboardLayout: " << kl << endl;
    }
  }
  */
  
  // load
  load(path);

  // finalize
  if (filename.empty())
    setting->keymaps.adjustModifier(setting->keyboard);
  
  return !isThereAnyError;
}


vector<istring> *SettingLoader::prefix;		// prefix terminal symbol
size_t SettingLoader::refcountof_prefix;	// reference count of prefix
