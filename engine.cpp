// ////////////////////////////////////////////////////////////////////////////
// engine.cpp


#include "misc.h"

#include "engine.h"
#include "errormessage.h"
#include "hook.h"
#include "mayurc.h"
#include "windowstool.h"

#include <iomanip>

#include <process.h>


// check focus window
void Engine::checkFocusWindow()
{
  int count = 0;
  
  restart:
  count ++;
  
  HWND hwndFore = GetForegroundWindow();
  DWORD threadId = GetWindowThreadProcessId(hwndFore, NULL);

  if (hwndFore)
  {
    {
      Acquire a(&m_cs);
      if (m_currentFocusOfThread &&
	  m_currentFocusOfThread->m_threadId == threadId &&
	  m_currentFocusOfThread->m_hwndFocus == m_hwndFocus)
	return;

      m_emacsEditKillLine.reset();
      
      // erase dead thread
      if (!m_detachedThreadIds.empty())
      {
	for (DetachedThreadIds::iterator i = m_detachedThreadIds.begin();
	     i != m_detachedThreadIds.end(); i ++)
	{
	  FocusOfThreads::iterator j = m_focusOfThreads.find((*i));
	  if (j != m_focusOfThreads.end())
	  {
	    FocusOfThread *fot = &((*j).second);
	    Acquire a(&m_log, 1);
	    m_log << _T("RemoveThread") << std::endl;
	    m_log << _T("\tHWND:\t") << std::hex << (int)fot->m_hwndFocus
		  << std::dec << std::endl;
	    m_log << _T("\tTHREADID:") << fot->m_threadId << std::endl;
	    m_log << _T("\tCLASS:\t") << fot->m_className << std::endl;
	    m_log << _T("\tTITLE:\t") << fot->m_titleName << std::endl;
	    m_log << std::endl;
	    m_focusOfThreads.erase(j);
	  }
	}
	m_detachedThreadIds.erase
	  (m_detachedThreadIds.begin(), m_detachedThreadIds.end());
      }
      
      FocusOfThreads::iterator i = m_focusOfThreads.find(threadId);
      if (i != m_focusOfThreads.end())
      {
	m_currentFocusOfThread = &((*i).second);
	if (!m_currentFocusOfThread->m_isConsole || 2 <= count)
	{
	  if (m_currentFocusOfThread->m_keymaps.empty())
	    m_currentKeymap = NULL;
	  else
	    m_currentKeymap = *m_currentFocusOfThread->m_keymaps.begin();
	  m_hwndFocus = m_currentFocusOfThread->m_hwndFocus;
	
	  Acquire a(&m_log, 1);
	  m_log << _T("FocusChanged") << std::endl;
	  m_log << _T("\tHWND:\t")
		<< std::hex << (int)m_currentFocusOfThread->m_hwndFocus
		<< std::dec << std::endl;
	  m_log << _T("\tTHREADID:")
		<< m_currentFocusOfThread->m_threadId << std::endl;
	  m_log << _T("\tCLASS:\t")
		<< m_currentFocusOfThread->m_className << std::endl;
	  m_log << _T("\tTITLE:\t")
		<< m_currentFocusOfThread->m_titleName << std::endl;
	  m_log << std::endl;
	  return;
	}
      }
    }
    
    _TCHAR className[GANA_MAX_ATOM_LENGTH];
    if (GetClassName(hwndFore, className, NUMBER_OF(className)))
    {
      if (_tcsicmp(className, _T("ConsoleWindowClass")) == 0)
      {
	_TCHAR titleName[1024];
	if (GetWindowText(hwndFore, titleName, NUMBER_OF(titleName)) == 0)
	  titleName[0] = _T('\0');
	setFocus(hwndFore, threadId, className, titleName, true);
	Acquire a(&m_log, 1);
	m_log << _T("HWND:\t") << std::hex << reinterpret_cast<int>(hwndFore)
	      << std::dec << std::endl;
	m_log << _T("THREADID:") << threadId << std::endl;
	m_log << _T("CLASS:\t") << className << std::endl;
	m_log << _T("TITLE:\t") << titleName << std::endl << std::endl;
	goto restart;
      }
    }
  }
  
  Acquire a(&m_cs);
  if (m_globalFocus.m_keymaps.empty())
  {
    Acquire a(&m_log, 1);
    m_log << _T("NO GLOBAL FOCUS") << std::endl;
    m_currentFocusOfThread = NULL;
    m_currentKeymap = NULL;
  }
  else
  {
    Acquire a(&m_log, 1);
    m_log << _T("GLOBAL FOCUS") << std::endl;
    m_currentFocusOfThread = &m_globalFocus;
    m_currentKeymap = m_globalFocus.m_keymaps.front();
  }
  m_hwndFocus = NULL;
}



// is modifier pressed ?
bool Engine::isPressed(Modifier::Type i_mt)
{
  const Keymap::ModAssignments &ma = m_currentKeymap->getModAssignments(i_mt);
  for (Keymap::ModAssignments::const_iterator i = ma.begin();
       i != ma.end(); ++ i)
    if ((*i).m_key->m_isPressed)
      return true;
  return false;
}


// fix modifier key (if fixed, return true)
bool Engine::fixModifierKey(ModifiedKey *o_mkey, Keymap::AssignMode *o_am)
{
  for (int i = Modifier::Type_begin; i != Modifier::Type_end; ++ i)
  {
    const Keymap::ModAssignments &ma =
      m_currentKeymap->getModAssignments(static_cast<Modifier::Type>(i));
    for (Keymap::ModAssignments::const_iterator j = ma.begin();
	 j != ma.end(); ++ j)
      if (o_mkey->m_key == (*j).m_key)
      {
	{
	  Acquire a(&m_log, 1);
	  m_log << _T("* Modifier Key") << std::endl;
	}
	o_mkey->m_modifier.dontcare(static_cast<Modifier::Type>(i));
	*o_am = (*j).m_assignMode;
	return true;
      }
  }
  *o_am = Keymap::AM_notModifier;
  return false;
}


// output to m_log
void Engine::outputToLog(const Key *i_key, const ModifiedKey &i_mkey,
			 int i_debugLevel)
{
  size_t i;
  Acquire a(&m_log, i_debugLevel);

  // output scan codes
  for (i = 0; i < i_key->getScanCodesSize(); ++ i)
  {
    if (i_key->getScanCodes()[i].m_flags & ScanCode::E0) m_log << _T("E0-");
    if (i_key->getScanCodes()[i].m_flags & ScanCode::E1) m_log << _T("E1-");
    if (!(i_key->getScanCodes()[i].m_flags & ScanCode::E0E1))
      m_log << _T("   ");
    m_log << _T("0x") << std::hex << std::setw(2) << std::setfill(_T('0'))
	  << static_cast<int>(i_key->getScanCodes()[i].m_scan)
	  << std::dec << _T(" ");
  }
  
  if (!i_mkey.m_key) // key corresponds to no phisical key
  {
    m_log << std::endl;
    return;
  }
  
  m_log << _T("  ") << i_mkey << std::endl;
}


// describe bindings
void Engine::describeBindings()
{
  Acquire a(&m_log, 0);

  Keymap::DescribedKeys dk;
  
  for (KeymapPtrList::iterator i = m_currentFocusOfThread->m_keymaps.begin();
       i != m_currentFocusOfThread->m_keymaps.end(); i ++)
    (*i)->describe(m_log, &dk);
  m_log << std::endl;
}


// get current modifiers
Modifier Engine::getCurrentModifiers(bool i_isPressed)
{
  Modifier cmods;
  cmods.add(m_currentLock);

  cmods.press(Modifier::Type_Shift  , isPressed(Modifier::Type_Shift  ));
  cmods.press(Modifier::Type_Alt    , isPressed(Modifier::Type_Alt    ));
  cmods.press(Modifier::Type_Control, isPressed(Modifier::Type_Control));
  cmods.press(Modifier::Type_Windows, isPressed(Modifier::Type_Windows));
  cmods.press(Modifier::Type_Up     , !i_isPressed);
  cmods.press(Modifier::Type_Down   , i_isPressed);

  for (int i = Modifier::Type_Mod0; i <= Modifier::Type_Mod9; ++ i)
    cmods.press(static_cast<Modifier::Type>(i),
		isPressed(static_cast<Modifier::Type>(i)));
  
  return cmods;
}


// generate keyboard event for a key
void Engine::generateKeyEvent(Key *i_key, bool i_doPress, bool i_isByAssign)
{
  // check if key is event
  bool isEvent = false;
  for (Key **e = Event::events; *e; ++ e)
    if (*e == i_key)
    {
      isEvent = true;
      break;
    }

  if (!isEvent)
  {
    if (i_doPress && !i_key->m_isPressedOnWin32)
      ++ m_currentKeyPressCountOnWin32;
    else if (!i_doPress && i_key->m_isPressedOnWin32)
      -- m_currentKeyPressCountOnWin32;
    i_key->m_isPressedOnWin32 = i_doPress;
    
    if (i_isByAssign)
      i_key->m_isPressedByAssign = i_doPress;
    
    KEYBOARD_INPUT_DATA kid = { 0, 0, 0, 0, 0 };
    const ScanCode *sc = i_key->getScanCodes();
    for (size_t i = 0; i < i_key->getScanCodesSize(); ++ i)
    {
      kid.MakeCode = sc[i].m_scan;
      kid.Flags = sc[i].m_flags;
      if (!i_doPress)
	kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
      DWORD len;
      CHECK_TRUE( WriteFile(m_device, &kid, sizeof(kid), &len, NULL) );
    }
    
    m_lastPressedKey = i_doPress ? i_key : NULL;
  }
  
  {
    Acquire a(&m_log, 1);
    m_log << _T("\t\t    =>\t");
  }
  ModifiedKey mkey(i_key);
  mkey.m_modifier.on(Modifier::Type_Up, !i_doPress);
  mkey.m_modifier.on(Modifier::Type_Down, i_doPress);
  outputToLog(i_key, mkey, 1);
}


// genete event
void Engine::generateEvents(Current i_c, Keymap *i_keymap, Key *i_event)
{
  // generate
  i_c.m_keymap = i_keymap;
  i_c.m_mkey.m_key = i_event;
  if (const Keymap::KeyAssignment *keyAssign =
      i_c.m_keymap->searchAssignment(i_c.m_mkey))
  {
    {
      Acquire a(&m_log, 1);
      m_log << std::endl << _T("           ")
	    << i_event->getName() << std::endl;
    }
    generateKeySeqEvents(i_c, keyAssign->m_keySeq, Part_all);
  }
}


// genete modifier events
void Engine::generateModifierEvents(const Modifier &i_mod)
{
  {
    Acquire a(&m_log, 1);
    m_log << _T("* Gen Modifiers\t{") << std::endl;
  }

  for (int i = Modifier::Type_begin; i < Modifier::Type_BASIC; ++ i)
  {
    Keyboard::Mods &mods =
      m_setting->m_keyboard.getModifiers(static_cast<Modifier::Type>(i));

    if (i_mod.isDontcare(static_cast<Modifier::Type>(i)))
      // no need to process
      ;
    else if (i_mod.isPressed(static_cast<Modifier::Type>(i)))
      // we have to press this modifier
    {
      bool noneIsPressed = true;
      bool noneIsPressedByAssign = true;
      for (Keyboard::Mods::iterator i = mods.begin(); i != mods.end(); ++ i)
      {
	if ((*i)->m_isPressedOnWin32)
	  noneIsPressed = false;
	if ((*i)->m_isPressedByAssign)
	  noneIsPressedByAssign = false;
      }
      if (noneIsPressed)
      {
	if (noneIsPressedByAssign)
	  generateKeyEvent(mods.front(), true, false);
	else
	  for (Keyboard::Mods::iterator
		 i = mods.begin(); i != mods.end(); ++ i)
	    if ((*i)->m_isPressedByAssign)
	      generateKeyEvent((*i), true, false);
      }
    }

    else
      // we have to release this modifier
    {
      // avoid such sequences as  "Alt U-ALt" or "Windows U-Windows"
      if (i == Modifier::Type_Alt || i == Modifier::Type_Windows)
      {
	for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); ++ j)
	  if ((*j) == m_lastPressedKey)
	  {
	    Keyboard::Mods *mods =
	      &m_setting->m_keyboard.getModifiers(Modifier::Type_Shift);
	    if (mods->size() == 0)
	      mods = &m_setting->m_keyboard.getModifiers(
		Modifier::Type_Control);
	    if (0 < mods->size())
	    {
	      generateKeyEvent(mods->front(), true, false);
	      generateKeyEvent(mods->front(), false, false);
	    }
	    break;
	  }
      }
      
      for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); ++ j)
      {
	if ((*j)->m_isPressedOnWin32)
	  generateKeyEvent((*j), false, false);
      }
    }
  }
  
  {
    Acquire a(&m_log, 1);
    m_log << _T("\t\t}") << std::endl;
  }
}


// generate keyboard events for action
void Engine::generateActionEvents(const Current &i_c, const Action *i_a,
				  bool i_doPress)
{
  Current cnew = i_c;
  cnew.m_mkey.m_modifier.on(Modifier::Type_Up, !i_doPress);
  cnew.m_mkey.m_modifier.on(Modifier::Type_Down, i_doPress);

  switch (i_a->m_type)
  {
    // key
    case Action::Type_key:
    {
      const ModifiedKey &mkey
	= reinterpret_cast<ActionKey *>(
	  const_cast<Action *>(i_a))->m_modifiedKey;

      // release
      if (!i_doPress &&
	  (mkey.m_modifier.isOn(Modifier::Type_Up) ||
	   mkey.m_modifier.isDontcare(Modifier::Type_Up)))
	generateKeyEvent(mkey.m_key, false, true);

      // press
      else if (i_doPress &&
	       (mkey.m_modifier.isOn(Modifier::Type_Down) ||
		mkey.m_modifier.isDontcare(Modifier::Type_Down)))
      {
	Modifier modifier = mkey.m_modifier;
	modifier.add(i_c.m_mkey.m_modifier);
	generateModifierEvents(modifier);
	generateKeyEvent(mkey.m_key, true, true);
      }
      break;
    }

    // keyseq
    case Action::Type_keySeq:
    {
      const ActionKeySeq *aks = reinterpret_cast<const ActionKeySeq *>(i_a);
      generateKeySeqEvents(i_c, aks->m_keySeq,
			   i_doPress ? Part_down : Part_up);
      break;
    }

    // function
    case Action::Type_function:
    {
      const ActionFunction *af = reinterpret_cast<const ActionFunction *>(i_a);
      bool is_up = (!i_doPress &&
		    (af->m_modifier.isOn(Modifier::Type_Up) ||
		     af->m_modifier.isDontcare(Modifier::Type_Up)));
      bool is_down = (i_doPress &&
		      (af->m_modifier.isOn(Modifier::Type_Down) ||
		       af->m_modifier.isDontcare(Modifier::Type_Down)));
      if (!is_down && !is_up)
	break;
      try
      {
	bool doesNeedEndl = true;
	{
	  Acquire a(&m_log, 1);
	  m_log << _T("\t\t     >\t&") << af->m_function->m_name;
	}
	switch (af->m_function->m_id)
	{
	  Default:
	  case Function::Id_Default:
	  {
	    {
	      Acquire a(&m_log, 1);
	      m_log << std::endl;
	      doesNeedEndl = false;
	    }
	    if (is_down)
	      generateModifierEvents(cnew.m_mkey.m_modifier);
	    generateKeyEvent(cnew.m_mkey.m_key, i_doPress, true);
	    break;
	  }
	  
	  case Function::Id_KeymapParent:
	  {
	    cnew.m_keymap = i_c.m_keymap->getParentKeymap();
	    if (!cnew.m_keymap)
	      goto Default;
	    {
	      Acquire a(&m_log, 1);
	      m_log << _T("(") << cnew.m_keymap->getName()
		    << _T(")") << std::endl;
	      doesNeedEndl = false;
	    }
	    generateKeyboardEvents(cnew);
	    break;
	  }

	  case Function::Id_KeymapWindow:
	  {
	    cnew.m_keymap = m_currentFocusOfThread->m_keymaps.front();
	    cnew.m_i = m_currentFocusOfThread->m_keymaps.begin();
	    generateKeyboardEvents(cnew);
	    break;
	  }
	  
	  case Function::Id_OtherWindowClass:
	  {
	    cnew.m_i ++;
	    if (cnew.m_i == m_currentFocusOfThread->m_keymaps.end())
	      goto Default;
	    cnew.m_keymap = (*cnew.m_i);
	    {
	      Acquire a(&m_log, 1);
	      m_log << _T("(") << cnew.m_keymap->getName()
		    << _T(")") << std::endl;
	      doesNeedEndl = false;
	    }
	    generateKeyboardEvents(cnew);
	    break;
	  }
	  
	  case Function::Id_Prefix:
	  {
	    if (i_doPress)
	    {
	      Keymap *keymap = (Keymap *)af->m_args[0].getData();
	      ASSERT( keymap );
	      m_currentKeymap = keymap;

	      // generate prefixed event
	      generateEvents(cnew, m_currentKeymap, &Event::prefixed);
		
	      m_isPrefix = true;
	      m_doesEditNextModifier = false;
	      m_doesIgnoreModifierForPrefix = true;
	      if (af->m_args.size() == 2)	// optional function argument
		m_doesIgnoreModifierForPrefix = !!af->m_args[1].getData();
	      Acquire a(&m_log, 1);
	      m_log << _T("(") << keymap->getName() << _T(", ")
		    << (m_doesIgnoreModifierForPrefix
			? _T("true") : _T("false"))
		    << _T(")");
	    }
	    break;
	  }
	  
	  case Function::Id_Keymap:
	  {
	    cnew.m_keymap = (Keymap *)af->m_args[0].getData();
	    ASSERT( cnew.m_keymap );
	    {
	      Acquire a(&m_log, 1);
	      m_log << _T("(") << cnew.m_keymap->getName()
		    << _T(")") << std::endl;
	      doesNeedEndl = false;
	    }
	    generateKeyboardEvents(cnew);
	    break;
	  }
	  
	  case Function::Id_Sync:
	  {
	    if (is_down)
	      generateModifierEvents(af->m_modifier);
	    if (!i_doPress || m_currentFocusOfThread->m_isConsole)
	      break;
	    
	    Key *sync = m_setting->m_keyboard.getSyncKey();
	    if (sync->getScanCodesSize() == 0)
	      break;
	    const ScanCode *sc = sync->getScanCodes();

	    // set variables exported from mayu.dll
	    g_hookData->m_syncKey = sc->m_scan;
	    g_hookData->m_syncKeyIsExtended = !!(sc->m_flags & ScanCode::E0E1);
	    m_isSynchronizing = true;
	    generateKeyEvent(sync, false, false);
	    
	    m_cs.release();
	    DWORD r = WaitForSingleObject(m_eSync, 5000);
	    if (r == WAIT_TIMEOUT)
	    {
	      Acquire a(&m_log, 0);
	      m_log << _T(" *FAILED*");
	    }
	    m_cs.acquire();
	    m_isSynchronizing = false;
	    break;
	  }
	  
	  case Function::Id_Toggle:
	  {
	    if (!i_doPress)
	    {
	      Modifier::Type mt =
		static_cast<Modifier::Type>((int)af->m_args[0].getData());
	      m_currentLock.press(mt, !m_currentLock.isPressed(mt));
	    }
	    break;
	  }

	  case Function::Id_EditNextModifier:
	  {
	    if (i_doPress)
	    {
	      m_isPrefix = true;
	      m_doesEditNextModifier = true;
	      m_doesIgnoreModifierForPrefix = true;
	      m_modifierForNextKey = *(Modifier *)af->m_args[0].getData();
	    }
	    break;
	  }

	  case Function::Id_Variable:
	  {
	    if (i_doPress)
	    {
	      m_variable *= af->m_args[0].getNumber();
	      m_variable += af->m_args[1].getNumber();
	    }
	    break;
	  }

	  case Function::Id_Repeat:
	  {
	    KeySeq *keySeq = (KeySeq *)af->m_args[0].getData();
	    if (i_doPress)
	    {
	      int end = m_variable;
	      if (af->m_args.size() == 2)
		end = MIN(end, af->m_args[1].getNumber());
	      else
		end = MIN(end, 10);

	      for (int i = 0; i < end - 1; ++ i)
		generateKeySeqEvents(i_c, keySeq, Part_all);
	      if (0 < end)
		generateKeySeqEvents(i_c, keySeq, Part_down);
	    }
	    else
	      generateKeySeqEvents(i_c, keySeq, Part_up);
	    break;
	  }
	  
	  case Function::Id_Ignore:
	    break;
	    
	  case Function::Id_EmacsEditKillLineFunc:
	  {
	    if (!is_down)
	      break;
	    m_emacsEditKillLine.func();
	    m_emacsEditKillLine.m_doForceReset = false;
	    break;
	  }
	  
	  case Function::Id_EmacsEditKillLinePred:
	  {
	    m_emacsEditKillLine.m_doForceReset = false;
	    if (!is_down)
	      break;
	    int r = m_emacsEditKillLine.pred();
	    KeySeq *keySeq;
	    if (r == 1)
	      keySeq = (KeySeq *)af->m_args[0].getData();
	    else if (r == 2)
	      keySeq = (KeySeq *)af->m_args[1].getData();
	    else // r == 0
	      break;
	    ASSERT(keySeq);
	    generateKeySeqEvents(i_c, keySeq, Part_all);
	    break;
	  }

	  case Function::Id_ShellExecute:
	  {
	    if (!is_down)
	      break;
	    m_afShellExecute = af;
	    PostMessage(m_hwndAssocWindow,
			WM_APP_engineNotify, EngineNotify_shellExecute, 0);
	    break;
	  }
	  
	  case Function::Id_LoadSetting:
	  {
	    if (!is_down)
	      break;
	    PostMessage(m_hwndAssocWindow,
			WM_APP_engineNotify, EngineNotify_loadSetting, 0);
	    break;
	  }
	  
	  case Function::Id_Wait:
	  {
	    if (!is_down)
	      break;
	    m_isSynchronizing = true;
	    int ms = af->m_args[0].getNumber();
	    if (ms < 0 || 5000 < ms)	// too long wait
	      break;
	    m_cs.release();
	    Sleep(ms);
	    m_cs.acquire();
	    m_isSynchronizing = false;
	    break;
	  }

	  case Function::Id_DescribeBindings:
	  {
	    if (!is_down)
	      break;
	    {
	      Acquire a(&m_log, 1);
	      m_log << std::endl;
	    }
	    describeBindings();
	    break;
	  }
	  
	  default:
	  {
	    if (af->m_function->m_func)
	    {
	      Function::FuncData fd(af->m_args, *this);
	      fd.m_id = af->m_function->m_id;
	      fd.m_hwnd = m_currentFocusOfThread->m_hwndFocus;
	      fd.m_isPressed = i_doPress;
	      af->m_function->m_func(fd);
	    }
	    break;
	  }
	}
	
	if (doesNeedEndl)
	{
	  Acquire a(&m_log, 1);
	  m_log << std::endl;
	}
      }
      catch (ErrorMessage &i_e)
      {
	Acquire a(&m_log);
	m_log << _T("&") << af->m_function->m_name
	      << _T(": invalid arguments: ") << i_e << std::endl;
      }
      break;
    }
  }
}


// generate keyboard events for keySeq
void Engine::generateKeySeqEvents(const Current &i_c, const KeySeq *i_keySeq,
				  Part i_part)
{
  const std::vector<Action *> &actions = i_keySeq->getActions();
  if (actions.empty())
    return;
  if (i_part == Part_up)
    generateActionEvents(i_c, actions[actions.size() - 1], false);
  else
  {
    size_t i;
    for (i = 0 ; i < actions.size() - 1; ++ i)
    {
      generateActionEvents(i_c, actions[i], true);
      generateActionEvents(i_c, actions[i], false);
    }
    generateActionEvents(i_c, actions[i], true);
    if (i_part == Part_all)
      generateActionEvents(i_c, actions[i], false);
  }
}


// generate keyboard events for current key
void Engine::generateKeyboardEvents(const Current &i_c)
{
  if (++ m_generateKeyboardEventsRecursionGuard ==
      MAX_GENERATE_KEYBOARD_EVENTS_RECURSION_COUNT)
  {
    Acquire a(&m_log);
    m_log << _T("error: too deep keymap recursion.  there may be a loop.")
	  << std::endl;
    return;
  }
  
  const Keymap::KeyAssignment *keyAssign
    = i_c.m_keymap->searchAssignment(i_c.m_mkey);
  if (!keyAssign)
  {
    const KeySeq *keySeq = i_c.m_keymap->getDefaultKeySeq();
    ASSERT( keySeq );
    generateKeySeqEvents(i_c, keySeq, i_c.isPressed() ? Part_down : Part_up);
  }
  else
  {
    if (keyAssign->m_modifiedKey.m_modifier.isOn(Modifier::Type_Up) ||
	keyAssign->m_modifiedKey.m_modifier.isOn(Modifier::Type_Down))
      generateKeySeqEvents(i_c, keyAssign->m_keySeq, Part_all);
    else
      generateKeySeqEvents(i_c, keyAssign->m_keySeq,
			   i_c.isPressed() ? Part_down : Part_up);
  }
  m_generateKeyboardEventsRecursionGuard --;
}


// generate keyboard events for current key
void Engine::generateKeyboardEvents(const Current &i_c, bool i_isModifier)
{
  //           (1)           (2)           (3)  (4)   (1)
  // up/down:  D-            U-            D-   U-    D-
  // keymap:   m_currentKeymap m_currentKeymap X    X     m_currentKeymap
  // memo:     &Prefix(X)    ...           ...  ...   ...
  // m_isPrefix: false         true          true false false
  
  bool isPhysicallyPressed
    = i_c.m_mkey.m_modifier.isPressed(Modifier::Type_Down);
  
  // for prefix key
  Keymap *tmpKeymap = m_currentKeymap;
  if (i_isModifier || !m_isPrefix) ; 
  else if (isPhysicallyPressed)			// when (3)
    m_isPrefix = false;
  else if (!isPhysicallyPressed)		// when (2)
    m_currentKeymap = m_currentFocusOfThread->m_keymaps.front();
  
  // for m_emacsEditKillLine function
  m_emacsEditKillLine.m_doForceReset = !i_isModifier;

  // generate key evend !
  m_generateKeyboardEventsRecursionGuard = 0;
  if (isPhysicallyPressed)
    generateEvents(i_c, i_c.m_keymap, &Event::before_key_down);
  generateKeyboardEvents(i_c);
  if (!isPhysicallyPressed)
    generateEvents(i_c, i_c.m_keymap, &Event::after_key_up);
      
  // for m_emacsEditKillLine function
  if (m_emacsEditKillLine.m_doForceReset)
    m_emacsEditKillLine.reset();

  // for prefix key
  if (i_isModifier)
    ;
  else if (!m_isPrefix)				// when (1), (4)
    m_currentKeymap = m_currentFocusOfThread->m_keymaps.front();
  else if (!isPhysicallyPressed)		// when (2)
    m_currentKeymap = tmpKeymap;
}


// pop all pressed key on win32
void Engine::keyboardResetOnWin32()
{
  for (Keyboard::KeyIterator
	 i = m_setting->m_keyboard.getKeyIterator();  *i; ++ i)
  {
    if ((*i)->m_isPressedOnWin32)
      generateKeyEvent((*i), false, true);
  }
}


// keyboard handler thread
void Engine::keyboardHandler(void *i_this)
{
  reinterpret_cast<Engine *>(i_this)->keyboardHandler();
}
void Engine::keyboardHandler()
{
  // initialize ok
  CHECK_TRUE( SetEvent(m_eEvent) );
    
  // loop
  Key key;
  while (!m_doForceTerminate)
  {
    KEYBOARD_INPUT_DATA kid;
    
    DWORD len;
    if (!ReadFile(m_device, &kid, sizeof(kid), &len, NULL))
      continue; // TODO

    checkFocusWindow();

    if (!m_setting ||	// m_setting has not been loaded
	!m_isEnabled)	// disabled
    {
      if (m_isLogMode)
      {
	Key key;
	key.addScanCode(ScanCode(kid.MakeCode, kid.Flags));
	outputToLog(&key, ModifiedKey(), 0);
      }
      else
      {
	WriteFile(m_device, &kid, sizeof(kid), &len, NULL);
      }
      continue;
    }
    
    Acquire a(&m_cs);

    if (!m_currentFocusOfThread ||
	!m_currentKeymap)
    {
      WriteFile(m_device, &kid, sizeof(kid), &len, NULL);
      Acquire a(&m_log, 0);
      if (!m_currentFocusOfThread)
	m_log << _T("internal error: m_currentFocusOfThread == NULL")
	      << std::endl;
      if (!m_currentKeymap)
	m_log << _T("internal error: m_currentKeymap == NULL")
	      << std::endl;
      continue;
    }
    
    Current c;
    c.m_keymap = m_currentKeymap;
    c.m_i = m_currentFocusOfThread->m_keymaps.begin();
    
    // search key
    key.addScanCode(ScanCode(kid.MakeCode, kid.Flags));
    c.m_mkey = m_setting->m_keyboard.searchKey(key);
    if (!c.m_mkey.m_key)
    {
      c.m_mkey.m_key = m_setting->m_keyboard.searchPrefixKey(key);
      if (c.m_mkey.m_key)
	continue;
    }

    // press the key and update counter
    bool isPhysicallyPressed
      = !(key.getScanCodes()[0].m_flags & ScanCode::BREAK);
    if (c.m_mkey.m_key)
    {
      if (!c.m_mkey.m_key->m_isPressed && isPhysicallyPressed)
	m_currentKeyPressCount ++;
      else if (c.m_mkey.m_key->m_isPressed && !isPhysicallyPressed)
	m_currentKeyPressCount --;
      c.m_mkey.m_key->m_isPressed = isPhysicallyPressed;
    }
    
    // create modifiers
    c.m_mkey.m_modifier = getCurrentModifiers(isPhysicallyPressed);
    Keymap::AssignMode am;
    bool isModifier = fixModifierKey(&c.m_mkey, &am);
    if (m_isPrefix)
    {
      if (isModifier && m_doesIgnoreModifierForPrefix)
	am = Keymap::AM_true;
      if (m_doesEditNextModifier)
      {
	Modifier modifier = m_modifierForNextKey;
	modifier.add(c.m_mkey.m_modifier);
	c.m_mkey.m_modifier = modifier;
      }
    }
    
    if (m_isLogMode)
      outputToLog(&key, c.m_mkey, 0);
    else if (am == Keymap::AM_true)
    {
      {
	Acquire a(&m_log, 1);
	m_log << _T("* true modifier") << std::endl;
      }
      // true modifier doesn't generate scan code
      outputToLog(&key, c.m_mkey, 1);
    }
    else if (am == Keymap::AM_oneShot)
    {
      {
	Acquire a(&m_log, 1);
	m_log << _T("* one shot modifier") << std::endl;
      }
      // oneShot modifier doesn't generate scan code
      outputToLog(&key, c.m_mkey, 1);
      if (isPhysicallyPressed)
	m_oneShotKey = c.m_mkey.m_key;
      else
      {
	if (m_oneShotKey)
	{
	  Current cnew = c;
	  cnew.m_mkey.m_modifier.off(Modifier::Type_Up);
	  cnew.m_mkey.m_modifier.on(Modifier::Type_Down);
	  generateKeyboardEvents(cnew, true);
	  
	  cnew = c;
	  cnew.m_mkey.m_modifier.on(Modifier::Type_Up);
	  cnew.m_mkey.m_modifier.off(Modifier::Type_Down);
	  generateKeyboardEvents(cnew, true);
	}
	m_oneShotKey = NULL;
      }
    }
    else if (c.m_mkey.m_key)
      // normal key
    {
      outputToLog(&key, c.m_mkey, 1);
      m_oneShotKey = NULL;
      generateKeyboardEvents(c, isModifier);
    }
    
    // if counter is zero, reset modifiers and keys on win32
    if (m_currentKeyPressCount <= 0)
    {
      {
	Acquire a(&m_log, 1);
	m_log << _T("* No key is pressed") << std::endl;
      }
      generateModifierEvents(Modifier());
      if (0 < m_currentKeyPressCountOnWin32)
	keyboardResetOnWin32();
      m_currentKeyPressCount = 0;
      m_currentKeyPressCountOnWin32 = 0;
      m_oneShotKey = NULL;
    }
    
    key.initialize();
  }
  CHECK_TRUE( SetEvent(m_eEvent) );
}
  

Engine::Engine(tomsgstream &i_log)
  : m_hwndAssocWindow(NULL),
    m_setting(NULL),
    m_device(NULL),
    m_didMayuStartDevice(false),
    m_eEvent(NULL),
    m_doForceTerminate(false),
    m_isLogMode(false),
    m_isEnabled(true),
    m_isSynchronizing(false),
    m_eSync(NULL),
    m_generateKeyboardEventsRecursionGuard(0),
    m_currentKeyPressCount(0),
    m_currentKeyPressCountOnWin32(0),
    m_lastPressedKey(NULL),
    m_oneShotKey(NULL),
    m_isPrefix(false),
    m_currentKeymap(NULL),
    m_currentFocusOfThread(NULL),
    m_hwndFocus(NULL),
    m_afShellExecute(NULL),
    m_variable(0),
    m_log(i_log)
{
  int i;
  // set default lock state
  for (i = 0; i < Modifier::Type_end; ++ i)
    m_currentLock.dontcare(static_cast<Modifier::Type>(i));
  for (i = Modifier::Type_Lock0; i <= Modifier::Type_Lock9; ++ i)
    m_currentLock.release(static_cast<Modifier::Type>(i));
  
  // open mayu m_device
  m_device = CreateFile(MAYU_DEVICE_FILE_NAME, GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (m_device == INVALID_HANDLE_VALUE)
  {
    // start mayud
    SC_HANDLE hscm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (hscm)
    {
      SC_HANDLE hs = OpenService(hscm, MAYU_DRIVER_NAME, SERVICE_START);
      if (hs)
      {
	StartService(hs, 0, NULL);
	CloseServiceHandle(hs);
	m_didMayuStartDevice = true;
      }
      CloseServiceHandle(hscm);
    }
    
    // open mayu m_device
    m_device = CreateFile(MAYU_DEVICE_FILE_NAME, GENERIC_READ | GENERIC_WRITE,
			  0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_device == INVALID_HANDLE_VALUE)
      throw ErrorMessage() << loadString(IDS_driverNotInstalled);
//    throw ErrorMessage() << loadString(IDS_cannotOpenM_Device);
  }

  // create event for sync
  CHECK_TRUE( m_eSync = CreateEvent(NULL, FALSE, FALSE, NULL) );
}


// start keyboard handler thread
void Engine::start()
{
  CHECK_TRUE( m_eEvent = CreateEvent(NULL, FALSE, FALSE, NULL) );
  CHECK_TRUE( 0 <= _beginthread(keyboardHandler, 0, this) );
  CHECK( WAIT_OBJECT_0 ==, WaitForSingleObject(m_eEvent, INFINITE) );
}


// stop keyboard handler thread
void Engine::stop()
{
  if (m_eEvent)
  {
    m_doForceTerminate = true;
    do
    {
      // cancel m_device ... TODO: this does not work on W2k
      CancelIo(m_device);
      //DWORD buf;
      //M_DeviceIoControl(m_device, IOCTL_MAYU_DETOUR_CANCEL,
      //                &buf, sizeof(buf), &buf, sizeof(buf), &buf, NULL);
      
      // wait for message handler thread terminate
    } while (WaitForSingleObject(m_eEvent, 100) != WAIT_OBJECT_0);
    CHECK_TRUE( CloseHandle(m_eEvent) );
    m_eEvent = NULL;

    // stop mayud
    if (m_didMayuStartDevice)
    {
      SC_HANDLE hscm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
      if (hscm)
      {
	SC_HANDLE hs = OpenService(hscm, MAYU_DRIVER_NAME, SERVICE_STOP);
	if (hs)
	{
	  SERVICE_STATUS ss;
	  ControlService(hs, SERVICE_CONTROL_STOP, &ss);
	  CloseServiceHandle(hs);
	}
	CloseServiceHandle(hscm);
      }
    }
  }
}


Engine::~Engine()
{
  stop();
  CHECK_TRUE( CloseHandle(m_eSync) );
  
  // close m_device
  CHECK_TRUE( CloseHandle(m_device) );
}


// set m_setting
bool Engine::setSetting(Setting *i_setting)
{
  Acquire a(&m_cs);
  if (m_isSynchronizing)
    return false;

  if (m_setting)
    for (Keyboard::KeyIterator i = m_setting->m_keyboard.getKeyIterator();
	 *i; ++ i)
    {
      Key *key = i_setting->m_keyboard.searchKey(*(*i));
      if (key)
      {
	key->m_isPressed = (*i)->m_isPressed;
	key->m_isPressedOnWin32 = (*i)->m_isPressedOnWin32;
	key->m_isPressedByAssign = (*i)->m_isPressedByAssign;
      }
    }
  
  m_setting = i_setting;
  if (m_currentFocusOfThread)
  {
    for (FocusOfThreads::iterator i = m_focusOfThreads.begin();
	 i != m_focusOfThreads.end(); i ++)
    {
      FocusOfThread *fot = &(*i).second;
      m_setting->m_keymaps.searchWindow(&fot->m_keymaps,
					fot->m_className, fot->m_titleName);
    }
  }
  m_setting->m_keymaps.searchWindow(&m_globalFocus.m_keymaps, _T(""), _T(""));
  if (m_globalFocus.m_keymaps.empty())
  {
    Acquire a(&m_log, 0);
    m_log << _T("internal error: m_globalFocus.m_keymap is empty")
	  << std::endl;
  }
  m_currentFocusOfThread = &m_globalFocus;
  m_currentKeymap = m_globalFocus.m_keymaps.front();
  m_hwndFocus = NULL;
  return true;
}


// focus
bool Engine::setFocus(HWND i_hwndFocus, DWORD i_threadId, 
		      const tstringi &i_className, const tstringi &i_titleName,
		      bool i_isConsole)
{
  Acquire a(&m_cs);
  if (m_isSynchronizing)
    return false;
  if (i_hwndFocus == NULL)
    return true;

  // remove newly created thread's id from m_detachedThreadIds
  if (!m_detachedThreadIds.empty())
  {
    DetachedThreadIds::iterator i;
    do
    {
      for (i = m_detachedThreadIds.begin();
	   i != m_detachedThreadIds.end(); ++ i)
	if (*i == i_threadId)
	{
	  m_detachedThreadIds.erase(i);
	  break;
	}
    } while (i != m_detachedThreadIds.end());
  }
  
  FocusOfThread *fot;
  FocusOfThreads::iterator i = m_focusOfThreads.find(i_threadId);
  if (i != m_focusOfThreads.end())
  {
    fot = &(*i).second;
    if (fot->m_hwndFocus == i_hwndFocus)
      return true;
  }
  else
  {
    i = m_focusOfThreads.insert(
      FocusOfThreads::value_type(i_threadId, FocusOfThread())).first;
    fot = &(*i).second;
    fot->m_threadId = i_threadId;
  }
  fot->m_hwndFocus = i_hwndFocus;
  fot->m_isConsole = i_isConsole;
  fot->m_className = i_className;
  fot->m_titleName = i_titleName;
  
  if (m_setting)
  {
    m_setting->m_keymaps.searchWindow(&fot->m_keymaps,
				      i_className, i_titleName);
    ASSERT(0 < fot->m_keymaps.size());
  }
  else
    fot->m_keymaps.clear();
  return true;
}


// lock state
bool Engine::setLockState(bool i_isNumLockToggled,
			  bool i_isCapsLockToggled,
			  bool i_isScrollLockToggled,
			  bool i_isImeLockToggled,
			  bool i_isImeCompToggled)
{
  Acquire a(&m_cs);
  if (m_isSynchronizing)
    return false;
  m_currentLock.on(Modifier::Type_NumLock, i_isNumLockToggled);
  m_currentLock.on(Modifier::Type_CapsLock, i_isCapsLockToggled);
  m_currentLock.on(Modifier::Type_ScrollLock, i_isScrollLockToggled);
  m_currentLock.on(Modifier::Type_ImeLock, i_isImeLockToggled);
  m_currentLock.on(Modifier::Type_ImeComp, i_isImeCompToggled);
  return true;
}


// sync
bool Engine::syncNotify()
{
  Acquire a(&m_cs);
  if (!m_isSynchronizing)
    return false;
  CHECK_TRUE( SetEvent(m_eSync) );
  return true;
}


// thread detach notify
bool Engine::threadDetachNotify(DWORD i_threadId)
{
  Acquire a(&m_cs);
  m_detachedThreadIds.push_back(i_threadId);
  return true;
}


/// get help message
void Engine::getHelpMessages(tstring *o_helpMessage, tstring *o_helpTitle)
{
  Acquire a(&m_cs);
  *o_helpMessage = m_helpMessage;
  *o_helpTitle = m_helpTitle;
}


// shell execute
void Engine::shellExecute()
{
  Acquire a(&m_cs);
  const ActionFunction *af = m_afShellExecute;
  
  tstringi operation  = af->m_args[0].getString();
  tstringi file       = af->m_args[1].getString();
  tstringi parameters = af->m_args[2].getString();
  tstringi directory  = af->m_args[3].getString();
  if (operation.empty()) operation = _T("open");
  
  int r = (int)ShellExecute(NULL,
			    operation.c_str(),
			    file.empty() ? NULL : file.c_str(),
			    parameters.empty() ? NULL : parameters.c_str(),
			    directory.empty() ? NULL : directory.c_str(),
			    af->m_args[4].getData());
  if (32 < r)
    return; // success
  
  struct ErrorMessages { int m_err; _TCHAR *m_str; };
  const ErrorMessages err[] =
  {
    { 0,
      _T("The operating system is out of memory or resources.") },
    { ERROR_FILE_NOT_FOUND,
      _T("The specified file was not found.") },
    { ERROR_PATH_NOT_FOUND,
      _T("The specified path was not found.") },
    { ERROR_BAD_FORMAT,
      _T("The .exe file is invalid ")
      _T("(non-Win32R .exe or error in .exe image).") },
    { SE_ERR_ACCESSDENIED,
      _T("The operating system denied access to the specified file.") },
    { SE_ERR_ASSOCINCOMPLETE,
      _T("The file name association is incomplete or invalid.") },
    { SE_ERR_DDEBUSY,
      _T("The DDE transaction could not be completed ")
      _T("because other DDE transactions were being processed. ") },
    { SE_ERR_DDEFAIL,
      _T("The DDE transaction failed.") },
    { SE_ERR_DDETIMEOUT,
      _T("The DDE transaction could not be completed ")
      _T("because the request timed out.") },
    { SE_ERR_DLLNOTFOUND,
      _T("The specified dynamic-link library was not found.")},
    { SE_ERR_FNF,
      _T("The specified file was not found.") },
    { SE_ERR_NOASSOC,
      _T("There is no application associated ")
      _T("with the given file name extension.") },
    { SE_ERR_OOM,
      _T("There was not enough memory to complete the operation.") },
    { SE_ERR_PNF,
      _T("The specified path was not found.") },
    { SE_ERR_SHARE,
      _T("A sharing violation occurred.") },
  };

  const _TCHAR *errorMessage = _T("Unknown error.");
  for (size_t i = 0; i < NUMBER_OF(err); ++ i)
    if (r == err[i].m_err)
    {
      errorMessage = err[i].m_str;
      break;
    }
  Acquire b(&m_log, 0);
  m_log << _T("internal error: &ShellExecute(")
	<< af->m_args[0] << _T(", ")
	<< af->m_args[1] << _T(", ")
	<< af->m_args[2] << _T(", ")
	<< af->m_args[3] << _T(", ")
	<< af->m_args[4] << _T("): ") << errorMessage;
}


// command notify
void Engine::commandNotify(
  HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
  Acquire b(&m_log, 0);
  HWND hf = m_hwndFocus;
  if (!hf)
    return;

  if (GetWindowThreadProcessId(hf, NULL) == 
      GetWindowThreadProcessId(m_hwndAssocWindow, NULL))
    return;	// inhibit the investigation of MADO TSUKAI NO YUUTSU

  const _TCHAR *target = NULL;
  int number_target = 0;
  
  if (i_hwnd == hf)
    target = _T("ToItself");
  else if (i_hwnd == GetParent(hf))
    target = _T("ToParentWindow");
  else
  {
    // Function::toMainWindow
    HWND h = hf;
    while (true)
    {
      HWND p = GetParent(h);
      if (!p)
	break;
      h = p;
    }
    if (i_hwnd == h)
      target = _T("ToMainWindow");
    else
    {
      // Function::toOverlappedWindow
      HWND h = hf;
      while (h)
      {
	LONG style = GetWindowLong(h, GWL_STYLE);
	if ((style & WS_CHILD) == 0)
	  break;
	h = GetParent(h);
      }
      if (i_hwnd == h)
	target = _T("ToOverlappedWindow");
      else
      {
	// number
	HWND h = hf;
	for (number_target = 0; h; number_target ++, h = GetParent(h))
	  if (i_hwnd == h)
	    break;
	return;
      }
    }
  }

  m_log << _T("&PostMessage(");
  if (target)
    m_log << target;
  else
    m_log << number_target;
  m_log << _T(", ") << i_message
	<< _T(", 0x") << std::hex << i_wParam
	<< _T(", 0x") << i_lParam << _T(") # hwnd = ")
	<< reinterpret_cast<int>(i_hwnd) << _T(", ")
	<< _T("message = ") << std::dec;
  if (i_message == WM_COMMAND)
    m_log << _T("WM_COMMAND, ");
  else if (i_message == WM_SYSCOMMAND)
    m_log << _T("WM_SYSCOMMAND, ");
  else
    m_log << i_message << _T(", ");
  m_log << _T("wNotifyCode = ") << HIWORD(i_wParam) << _T(", ")
	<< _T("wID = ") << LOWORD(i_wParam) << _T(", ")
	<< _T("hwndCtrl = 0x") << std::hex << i_lParam << std::dec << std::endl;
}
