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


using namespace std;


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
      Acquire a(&cs);
      if (currentFocusOfThread &&
	  currentFocusOfThread->threadId == threadId &&
	  currentFocusOfThread->hwndFocus == hwndFocus)
	return;

      emacsEditKillLine.reset();
      
      // erase dead thread
      if (!detachedThreadIds.empty())
      {
	for (DetachedThreadIds::iterator i = detachedThreadIds.begin();
	     i != detachedThreadIds.end(); i ++)
	{
	  FocusOfThreads::iterator j = focusOfThreads.find((*i));
	  if (j != focusOfThreads.end())
	  {
	    FocusOfThread *fot = &((*j).second);
	    Acquire a(&log, 1);
	    log << "RemoveThread" << endl;
	    log << "\tHWND:\t" << hex << (int)fot->hwndFocus
		<< dec << endl;
	    log << "\tTHREADID:" << fot->threadId << endl;
	    log << "\tCLASS:\t" << fot->className << endl;
	    log << "\tTITLE:\t" << fot->titleName << endl;
	    log << endl;
	    focusOfThreads.erase(j);
	  }
	}
	detachedThreadIds.erase
	  (detachedThreadIds.begin(), detachedThreadIds.end());
      }
      
      FocusOfThreads::iterator i = focusOfThreads.find(threadId);
      if (i != focusOfThreads.end())
      {
	currentFocusOfThread = &((*i).second);
	if (!currentFocusOfThread->isConsole || 2 <= count)
	{
	  if (currentFocusOfThread->keymaps.empty())
	    currentKeymap = NULL;
	  else
	    currentKeymap = *currentFocusOfThread->keymaps.begin();
	  hwndFocus = currentFocusOfThread->hwndFocus;
	
	  Acquire a(&log, 1);
	  log << "FocusChanged" << endl;
	  log << "\tHWND:\t" << hex << (int)currentFocusOfThread->hwndFocus
	      << dec << endl;
	  log << "\tTHREADID:" << currentFocusOfThread->threadId << endl;
	  log << "\tCLASS:\t" << currentFocusOfThread->className << endl;
	  log << "\tTITLE:\t" << currentFocusOfThread->titleName << endl;
	  log << endl;
	  return;
	}
      }
    }
    
    char className[GANA_MAX_ATOM_LENGTH];
    if (GetClassName(hwndFore, className, sizeof(className)))
    {
      if (StringTool::mbsiequal_(className, "ConsoleWindowClass"))
      {
	char titleName[1024];
	if (GetWindowText(hwndFore, titleName, sizeof(titleName)) == 0)
	  titleName[0] = '\0';
	setFocus(hwndFore, threadId, className, titleName, true);
	Acquire a(&log, 1);
	log << "HWND:\t" << hex << (int)hwndFore << dec << endl;
	log << "THREADID:" << threadId << endl;
	log << "CLASS:\t" << className << endl;
	log << "TITLE:\t" << titleName << endl << endl;
	goto restart;
      }
    }
  }
  
  Acquire a(&cs);
  if (globalFocus.keymaps.empty())
  {
    Acquire a(&log, 1);
    log << "NO GLOBAL FOCUS" << endl;
    currentFocusOfThread = NULL;
    currentKeymap = NULL;
  }
  else
  {
    Acquire a(&log, 1);
    log << "GLOBAL FOCUS" << endl;
    currentFocusOfThread = &globalFocus;
    currentKeymap = globalFocus.keymaps.front();
  }
  hwndFocus = NULL;
}



// is modifier pressed ?
bool Engine::isPressed(Modifier::Type mt)
{
  const Keymap::ModAssignments &ma = currentKeymap->getModAssignments(mt);
  for (Keymap::ModAssignments::const_iterator i = ma.begin();
       i != ma.end(); i++)
    if ((*i).key->isPressed)
      return true;
  return false;
}


// fix modifier key (if fixed, return true)
bool Engine::fixModifierKey(ModifiedKey *mkey_r, Keymap::AssignMode *am_r)
{
  for (int i = Modifier::begin; i != Modifier::end; i ++)
  {
    const Keymap::ModAssignments &ma =
      currentKeymap->getModAssignments((Modifier::Type)i);
    for (Keymap::ModAssignments::const_iterator j = ma.begin();
	 j != ma.end(); j ++)
      if (mkey_r->key == (*j).key)
      {
	{
	  Acquire a(&log, 1);
	  log << "* Modifier Key" << endl;
	}
	mkey_r->modifier.dontcare((Modifier::Type)i);
	*am_r = (*j).assignMode;
	return true;
      }
  }
  *am_r = Keymap::AMnotModifier;
  return false;
}


// output to log
void Engine::outputToLog(const Key *key, const ModifiedKey &mkey,
			 int debugLevel)
{
  size_t i;
  Acquire a(&log, debugLevel);

  // output scan codes
  for (i = 0; i < key->getLengthof_scanCodes(); i++)
  {
    if (key->getScanCodes()[i].flags & ScanCode::E0) log << "E0-";
    if (key->getScanCodes()[i].flags & ScanCode::E1) log << "E1-";
    if (!(key->getScanCodes()[i].flags & ScanCode::E0E1)) log << "   ";
    log << "0x" << hex << setw(2) << setfill('0')
	<< (int)key->getScanCodes()[i].scan << dec << " ";
  }
  
  if (!mkey.key) // key corresponds to no phisical key
  {
    log << endl;
    return;
  }
  
  log << "  " << mkey << endl;
}


// describe bindings
void Engine::describeBindings()
{
  Acquire a(&log, 0);

  Keymap::DescribedKeys dk;
  
  for (std::list<Keymap *>::iterator i = currentFocusOfThread->keymaps.begin();
       i != currentFocusOfThread->keymaps.end(); i ++)
    (*i)->describe(log, &dk);
  log << endl;
}


// get current modifiers
Modifier Engine::getCurrentModifiers(bool isPressed_)
{
  Modifier cmods;
  cmods += currentLock;

  cmods.press(Modifier::Shift  , isPressed(Modifier::Shift  ));
  cmods.press(Modifier::Alt    , isPressed(Modifier::Alt    ));
  cmods.press(Modifier::Control, isPressed(Modifier::Control));
  cmods.press(Modifier::Windows, isPressed(Modifier::Windows));
  cmods.press(Modifier::Up     , !isPressed_);
  cmods.press(Modifier::Down   , isPressed_);

  for (int i = Modifier::Mod0; i <= Modifier::Mod9; i++)
    cmods.press((Modifier::Type)i, isPressed((Modifier::Type)i));
  
  return cmods;
}


// generate keyboard event for a key
void Engine::generateKeyEvent(Key *key, bool doPress, bool isByAssign)
{
  // check if key is event
  bool isEvent = false;
  for (Key **e = Event::events; *e; e ++)
    if (*e == key)
    {
      isEvent = true;
      break;
    }

  if (!isEvent)
  {
    if (doPress &&!key->isPressedOnWin32)
      currentKeyPressCountOnWin32 ++;
    else if (!doPress && key->isPressedOnWin32)
      currentKeyPressCountOnWin32 --;
    key->isPressedOnWin32 = doPress;
    
    if (isByAssign)
      key->isPressedByAssign = doPress;
    
    KEYBOARD_INPUT_DATA kid = { 0, 0, 0, 0, 0 };
    const ScanCode *sc = key->getScanCodes();
    for (size_t i = 0; i < key->getLengthof_scanCodes(); i++)
    {
      kid.MakeCode = sc[i].scan;
      kid.Flags = sc[i].flags;
      if (!doPress)
	kid.Flags |= KEYBOARD_INPUT_DATA::BREAK;
      DWORD len;
      _true( WriteFile(device, &kid, sizeof(kid), &len, NULL) );
    }
    
    lastPressedKey = doPress ? key : NULL;
  }
  
  {
    Acquire a(&log, 1);
    log << "\t\t    =>\t";
  }
  ModifiedKey mkey(key);
  mkey.modifier.on(Modifier::Up, !doPress);
  mkey.modifier.on(Modifier::Down, doPress);
  outputToLog(key, mkey, 1);
}


// genete event
void Engine::generateEvents(Current i_c, Keymap *i_keymap, Key *i_event)
{
  // generate
  i_c.keymap = i_keymap;
  i_c.mkey.key = i_event;
  if (const Keymap::KeyAssignment *keyAssign =
      i_c.keymap->searchAssignment(i_c.mkey))
  {
    {
      Acquire a(&log, 1);
      log << endl << "           " << i_event->getName() << endl;
    }
    generateKeySeqEvents(i_c, keyAssign->keySeq, PartAll);
  }
}


// genete modifier events
void Engine::generateModifierEvents(const Modifier &mod)
{
  {
    Acquire a(&log, 1);
    log << "* Gen Modifiers\t{" << endl;
  }

  for (int i = Modifier::begin; i < Modifier::BASIC; i++)
  {
    Keyboard::Mods &mods = setting->keyboard.getModifiers((Modifier::Type)i);

    if (mod.isDontcare((Modifier::Type)i))
      // no need to process
	;
    else if (mod.isPressed((Modifier::Type)i))
      // we have to press this modifier
    {
      bool noneIsPressed = true;
      bool noneIsPressedByAssign = true;
      for (Keyboard::Mods::iterator i = mods.begin(); i != mods.end(); i++)
      {
	if ((*i)->isPressedOnWin32)
	  noneIsPressed = false;
	if ((*i)->isPressedByAssign)
	  noneIsPressedByAssign = false;
      }
      if (noneIsPressed)
      {
	if (noneIsPressedByAssign)
	  generateKeyEvent(mods.front(), true, false);
	else
	  for (Keyboard::Mods::iterator i = mods.begin(); i != mods.end(); i++)
	    if ((*i)->isPressedByAssign)
	      generateKeyEvent((*i), true, false);
      }
    }

    else
      // we have to release this modifier
    {
      // avoid such sequences as  "Alt U-ALt" or "Windows U-Windows"
      if (i == Modifier::Alt || i == Modifier::Windows)
      {
	for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); j ++)
	  if ((*j) == lastPressedKey)
	  {
	    Keyboard::Mods *mods =
	      &setting->keyboard.getModifiers(Modifier::Shift);
	    if (mods->size() == 0)
	      mods = &setting->keyboard.getModifiers(Modifier::Control);
	    if (0 < mods->size())
	    {
	      generateKeyEvent(mods->front(), true, false);
	      generateKeyEvent(mods->front(), false, false);
	    }
	    break;
	  }
      }
      
      for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); j ++)
      {
	if ((*j)->isPressedOnWin32)
	  generateKeyEvent((*j), false, false);
      }
    }
  }
  
  {
    Acquire a(&log, 1);
    log << "\t\t}" << endl;
  }
}


// generate keyboard events for action
void Engine::generateActionEvents(const Current &c, const Action *a,
				  bool doPress)
{
  Current cnew = c;
  cnew.mkey.modifier.on(Modifier::Up, !doPress);
  cnew.mkey.modifier.on(Modifier::Down, doPress);

  switch (a->type)
  {
    // key
    case Action::ActKey:
    {
      const ModifiedKey &mkey = ((ActionKey *)a)->modifiedKey;

      // release
      if (!doPress &&
	  (mkey.modifier.isOn(Modifier::Up) ||
	   mkey.modifier.isDontcare(Modifier::Up)))
	generateKeyEvent(mkey.key, false, true);

      // press
      else if (doPress &&
	       (mkey.modifier.isOn(Modifier::Down) ||
		mkey.modifier.isDontcare(Modifier::Down)))
      {
	Modifier modifier = mkey.modifier;
	modifier += c.mkey.modifier;
	generateModifierEvents(modifier);
	generateKeyEvent(mkey.key, true, true);
      }
      break;
    }

    // keyseq
    case Action::ActKeySeq:
    {
      ActionKeySeq *aks = (ActionKeySeq *)a;
      generateKeySeqEvents(c, aks->keySeq, doPress ? PartDown : PartUp);
      break;
    }

    // function
    case Action::ActFunction:
    {
      ActionFunction *af = (ActionFunction *)a;
      bool is_up = (!doPress &&
		    (af->modifier.isOn(Modifier::Up) ||
		     af->modifier.isDontcare(Modifier::Up)));
      bool is_down = (doPress &&
		      (af->modifier.isOn(Modifier::Down) ||
		       af->modifier.isDontcare(Modifier::Down)));
      if (!is_down && !is_up)
	break;
      try
      {
	bool doesNeedEndl = true;
	{
	  Acquire a(&log, 1);
	  log << "\t\t     >\t&" << af->function->name;
	}
	switch (af->function->id)
	{
	  Default:
	  case Function::Default:
	  {
	    {
	      Acquire a(&log, 1);
	      log << endl;
	      doesNeedEndl = false;
	    }
	    if (is_down)
	      generateModifierEvents(cnew.mkey.modifier);
	    generateKeyEvent(cnew.mkey.key, doPress, true);
	    break;
	  }
	  
	  case Function::KeymapParent:
	  {
	    cnew.keymap = c.keymap->getParentKeymap();
	    if (!cnew.keymap)
	      goto Default;
	    {
	      Acquire a(&log, 1);
	      log << "(" << cnew.keymap->getName() << ")" << endl;
	      doesNeedEndl = false;
	    }
	    generateKeyboardEvents(cnew);
	    break;
	  }

	  case Function::KeymapWindow:
	  {
	    cnew.keymap = currentFocusOfThread->keymaps.front();
	    cnew.i = currentFocusOfThread->keymaps.begin();
	    generateKeyboardEvents(cnew);
	    break;
	  }
	  
	  case Function::OtherWindowClass:
	  {
	    cnew.i ++;
	    if (cnew.i == currentFocusOfThread->keymaps.end())
	      goto Default;
	    cnew.keymap = (*cnew.i);
	    {
	      Acquire a(&log, 1);
	      log << "(" << cnew.keymap->getName() << ")" << endl;
	      doesNeedEndl = false;
	    }
	    generateKeyboardEvents(cnew);
	    break;
	  }
	  
	  case Function::Prefix:
	  {
	    if (doPress)
	    {
	      Keymap *keymap = (Keymap *)af->args[0].getData();
	      assert( keymap );
	      currentKeymap = keymap;

	      // generate prefixed event
	      generateEvents(cnew, currentKeymap, &Event::prefixed);
		
	      isPrefix = true;
	      doesEditNextModifier = false;
	      doesIgnoreModifierForPrefix = true;
	      if (af->args.size() == 2)	// optional function argument
		doesIgnoreModifierForPrefix = !!af->args[1].getData();
	      Acquire a(&log, 1);
	      log << "(" << keymap->getName() << ", " <<
		  (doesIgnoreModifierForPrefix ? "true" : "false") << ")";
	    }
	    break;
	  }
	  
	  case Function::Keymap:
	  {
	    cnew.keymap = (Keymap *)af->args[0].getData();
	    assert( cnew.keymap );
	    {
	      Acquire a(&log, 1);
	      log << "(" << cnew.keymap->getName() << ")" << endl;
	      doesNeedEndl = false;
	    }
	    generateKeyboardEvents(cnew);
	    break;
	  }
	  
	  case Function::Sync:
	  {
	    if (is_down)
	      generateModifierEvents(af->modifier);
	    if (!doPress || currentFocusOfThread->isConsole)
	      break;
	    
	    Key *sync = setting->keyboard.getSyncKey();
	    if (sync->getLengthof_scanCodes() == 0)
	      break;
	    const ScanCode *sc = sync->getScanCodes();

	    // set variables exported from mayu.dll
	    hookData->syncKey = sc->scan;
	    hookData->syncKeyIsExtended = !!(sc->flags & ScanCode::E0E1);
	    isSynchronizing = true;
	    generateKeyEvent(sync, false, false);
	    
	    cs.release();
	    DWORD r = WaitForSingleObject(eSync, 5000);
	    if (r == WAIT_TIMEOUT)
	    {
	      Acquire a(&log, 0);
	      log << " *FAILED*";
	    }
	    cs.acquire();
	    isSynchronizing = false;
	    break;
	  }
	  
	  case Function::Toggle:
	  {
	    if (!doPress)
	    {
	      Modifier::Type mt = (Modifier::Type)(int)af->args[0].getData();
	      currentLock.press(mt, !currentLock.isPressed(mt));
	    }
	    break;
	  }

	  case Function::EditNextModifier:
	  {
	    if (doPress)
	    {
	      isPrefix = true;
	      doesEditNextModifier = true;
	      doesIgnoreModifierForPrefix = true;
	      modifierForNextKey = *(Modifier *)af->args[0].getData();
	    }
	    break;
	  }

	  case Function::Variable:
	  {
	    if (doPress)
	    {
	      m_variable *= af->args[0].getNumber();
	      m_variable += af->args[1].getNumber();
	    }
	    break;
	  }

	  case Function::Repeat:
	  {
	    KeySeq *keySeq = (KeySeq *)af->args[0].getData();
	    if (doPress)
	    {
	      int end = m_variable;
	      if (af->args.size() == 2)
		end = MIN(end, af->args[1].getNumber());
	      else
		end = MIN(end, 10);

	      for (int i = 0; i < end - 1; ++ i)
		generateKeySeqEvents(c, keySeq, PartAll);
	      if (0 < end)
		generateKeySeqEvents(c, keySeq, PartDown);
	    }
	    else
	      generateKeySeqEvents(c, keySeq, PartUp);
	    break;
	  }
	  
	  case Function::Ignore:
	    break;
	    
	  case Function::EmacsEditKillLineFunc:
	  {
	    if (!is_down)
	      break;
	    emacsEditKillLine.func();
	    emacsEditKillLine.doForceReset = false;
	    break;
	  }
	  
	  case Function::EmacsEditKillLinePred:
	  {
	    emacsEditKillLine.doForceReset = false;
	    if (!is_down)
	      break;
	    int r = emacsEditKillLine.pred();
	    KeySeq *keySeq;
	    if (r == 1)
	      keySeq = (KeySeq *)af->args[0].getData();
	    else if (r == 2)
	      keySeq = (KeySeq *)af->args[1].getData();
	    else // r == 0
	      break;
	    assert(keySeq);
	    generateKeySeqEvents(c, keySeq, PartAll);
	    break;
	  }

	  case Function::ShellExecute:
	  {
	    if (!is_down)
	      break;
	    afShellExecute = af;
	    PostMessage(hwndAssocWindow,
			WM_engineNotify, engineNotify_shellExecute, 0);
	    break;
	  }
	  
	  case Function::LoadSetting:
	  {
	    if (!is_down)
	      break;
	    PostMessage(hwndAssocWindow,
			WM_engineNotify, engineNotify_loadSetting, 0);
	    break;
	  }
	  
	  case Function::Wait:
	  {
	    if (!is_down)
	      break;
	    isSynchronizing = true;
	    int ms = af->args[0].getNumber();
	    if (ms < 0 || 5000 < ms)	// too long wait
	      break;
	    cs.release();
	    Sleep(ms);
	    cs.acquire();
	    isSynchronizing = false;
	    break;
	  }

	  case Function::DescribeBindings:
	  {
	    if (!is_down)
	      break;
	    {
	      Acquire a(&log, 1);
	      log << endl;
	    }
	    describeBindings();
	    break;
	  }
	  
	  default:
	  {
	    if (af->function->func)
	    {
	      Function::FuncData fd(af->args, *this);
	      fd.id = af->function->id;
	      fd.hwnd = currentFocusOfThread->hwndFocus;
	      fd.isPressed = doPress;
	      af->function->func(fd);
	    }
	    break;
	  }
	}
	
	if (doesNeedEndl)
	{
	  Acquire a(&log, 1);
	  log << endl;
	}
      }
      catch (ErrorMessage &e)
      {
	Acquire a(&log);
	log << "&" << af->function->name << ": invalid arguments: "
	    << e << endl;
      }
      break;
    }
  }
}


// generate keyboard events for keySeq
void Engine::generateKeySeqEvents(const Current &c, const KeySeq *keySeq,
				  Part part)
{
  const vector<Action *> &actions = keySeq->getActions();
  if (actions.empty())
    return;
  if (part == PartUp)
    generateActionEvents(c, actions[actions.size() - 1], false);
  else
  {
    size_t i;
    for (i = 0 ; i < actions.size() - 1; i ++)
    {
      generateActionEvents(c, actions[i], true);
      generateActionEvents(c, actions[i], false);
    }
    generateActionEvents(c, actions[i], true);
    if (part == PartAll)
      generateActionEvents(c, actions[i], false);
  }
}


// generate keyboard events for current key
void Engine::generateKeyboardEvents(const Current &c)
{
  if (++ generateKeyboardEventsRecursionGuard ==
      maxGenerateKeyboardEventsRecursion)
  {
    Acquire a(&log);
    log << "error: too deep keymap recursion.  there may be a loop." << endl;
    return;
  }
  
  const Keymap::KeyAssignment *keyAssign = c.keymap->searchAssignment(c.mkey);
  if (!keyAssign)
  {
    const KeySeq *keySeq = c.keymap->getDefaultKeySeq();
    assert( keySeq );
    generateKeySeqEvents(c, keySeq, c.isPressed() ? PartDown : PartUp);
  }
  else
  {
    if (keyAssign->modifiedKey.modifier.isOn(Modifier::Up) ||
	keyAssign->modifiedKey.modifier.isOn(Modifier::Down))
      generateKeySeqEvents(c, keyAssign->keySeq, PartAll);
    else
      generateKeySeqEvents(c, keyAssign->keySeq,
			   c.isPressed() ? PartDown : PartUp);
  }
  generateKeyboardEventsRecursionGuard --;
}


// generate keyboard events for current key
void Engine::generateKeyboardEvents(const Current &c, bool isModifier)
{
  //           (1)           (2)           (3)  (4)   (1)
  // up/down:  D-            U-            D-   U-    D-
  // keymap:   currentKeymap currentKeymap X    X     currentKeymap
  // memo:     &Prefix(X)    ...           ...  ...   ...
  // isPrefix: false         true          true false false
  
  bool isPhysicallyPressed = c.mkey.modifier.isPressed(Modifier::Down);
  
  // for prefix key
  Keymap *tmpKeymap = currentKeymap;
  if (isModifier || !isPrefix) ; 
  else if (isPhysicallyPressed)			// when (3)
    isPrefix = false;
  else if (!isPhysicallyPressed)		// when (2)
    currentKeymap = currentFocusOfThread->keymaps.front();
  
  // for EmacsEditKillLine function
  emacsEditKillLine.doForceReset = !isModifier;

  // generate key evend !
  generateKeyboardEventsRecursionGuard = 0;
  if (isPhysicallyPressed)
    generateEvents(c, c.keymap, &Event::before_key_down);
  generateKeyboardEvents(c);
  if (!isPhysicallyPressed)
    generateEvents(c, c.keymap, &Event::after_key_up);
      
  // for EmacsEditKillLine function
  if (emacsEditKillLine.doForceReset)
    emacsEditKillLine.reset();

  // for prefix key
  if (isModifier) ;
  else if (!isPrefix)				// when (1), (4)
    currentKeymap = currentFocusOfThread->keymaps.front();
  else if (!isPhysicallyPressed)		// when (2)
    currentKeymap = tmpKeymap;
}


// pop all pressed key on win32
void Engine::keyboardResetOnWin32()
{
  for (Keyboard::KeyIterator i = setting->keyboard.getKeyIterator();  *i; i ++)
  {
    if ((*i)->isPressedOnWin32)
      generateKeyEvent((*i), false, true);
  }
}


// keyboard handler thread
void Engine::keyboardHandler(void *This)
{
  ((Engine *)This)->keyboardHandler();
}
void Engine::keyboardHandler()
{
  // initialize ok
  _true( SetEvent(eEvent) );
    
  // loop
  Key key;
  while (!doForceTerminate)
  {
    KEYBOARD_INPUT_DATA kid;
    
    DWORD len;
    if (!ReadFile(device, &kid, sizeof(kid), &len, NULL))
      continue; // TODO

    checkFocusWindow();

    if (!setting ||	// setting has not been loaded
	!isEnabled)	// disabled
    {
      WriteFile(device, &kid, sizeof(kid), &len, NULL);
      continue;
    }
    
    Acquire a(&cs);

    if (!currentFocusOfThread ||
	!currentKeymap)
    {
      WriteFile(device, &kid, sizeof(kid), &len, NULL);
      Acquire a(&log, 0);
      if (!currentFocusOfThread)
	log << "internal error: currentFocusOfThread == NULL" << endl;
      if (!currentKeymap)
	log << "internal error: currentKeymap == NULL" << endl;
      continue;
    }
    
    Current c;
    c.keymap = currentKeymap;
    c.i = currentFocusOfThread->keymaps.begin();
    
    // search key
    key.addScanCode(ScanCode(kid.MakeCode, kid.Flags));
    c.mkey = setting->keyboard.searchKey(key);
    if (!c.mkey.key)
    {
      c.mkey.key = setting->keyboard.searchPrefixKey(key);
      if (c.mkey.key)
	continue;
    }

    // press the key and update counter
    bool isPhysicallyPressed = !(key.getScanCodes()[0].flags&ScanCode::BREAK);
    if (c.mkey.key)
    {
      if (!c.mkey.key->isPressed && isPhysicallyPressed)
	currentKeyPressCount ++;
      else if (c.mkey.key->isPressed && !isPhysicallyPressed)
	currentKeyPressCount --;
      c.mkey.key->isPressed = isPhysicallyPressed;
    }
    
    // create modifiers
    c.mkey.modifier = getCurrentModifiers(isPhysicallyPressed);
    Keymap::AssignMode am;
    bool isModifier = fixModifierKey(&c.mkey, &am);
    if (isPrefix)
    {
      if (isModifier && doesIgnoreModifierForPrefix)
	am = Keymap::AMtrue;
      if (doesEditNextModifier)
      {
	Modifier modifier = modifierForNextKey;
	modifier += c.mkey.modifier;
	c.mkey.modifier = modifier;
      }
    }
    
    if (isLogMode)
      outputToLog(&key, c.mkey, 0);
    else if (am == Keymap::AMtrue)
    {
      {
	Acquire a(&log, 1);
	log << "* true modifier" << endl;
      }
      // true modifier doesn't generate scan code
      outputToLog(&key, c.mkey, 1);
    }
    else if (am == Keymap::AMoneShot)
    {
      {
	Acquire a(&log, 1);
	log << "* one shot modifier" << endl;
      }
      // oneShot modifier doesn't generate scan code
      outputToLog(&key, c.mkey, 1);
      if (isPhysicallyPressed)
	oneShotKey = c.mkey.key;
      else
      {
	if (oneShotKey)
	{
	  Current cnew = c;
	  cnew.mkey.modifier.off(Modifier::Up);
	  cnew.mkey.modifier.on(Modifier::Down);
	  generateKeyboardEvents(cnew, true);
	  
	  cnew = c;
	  cnew.mkey.modifier.on(Modifier::Up);
	  cnew.mkey.modifier.off(Modifier::Down);
	  generateKeyboardEvents(cnew, true);
	}
	oneShotKey = NULL;
      }
    }
    else if (c.mkey.key)
      // normal key
    {
      outputToLog(&key, c.mkey, 1);
      oneShotKey = NULL;
      generateKeyboardEvents(c, isModifier);
    }
    
    // if counter is zero, reset modifiers and keys on win32
    if (currentKeyPressCount <= 0)
    {
      {
	Acquire a(&log, 1);
	log << "* No key is pressed" << endl;
      }
      generateModifierEvents(Modifier());
      if (0 < currentKeyPressCountOnWin32)
	keyboardResetOnWin32();
      currentKeyPressCount = 0;
      currentKeyPressCountOnWin32 = 0;
      oneShotKey = NULL;
    }
    
    key.initialize();
  }
  _true( SetEvent(eEvent) );
}
  

Engine::Engine(omsgstream &log_)
  : hwndAssocWindow(NULL),
    setting(NULL),
    device(NULL),
    didMayuStartDevice(false),
    eEvent(NULL),
    doForceTerminate(false),
    isLogMode(false),
    isEnabled(true),
    isSynchronizing(false),
    eSync(NULL),
    generateKeyboardEventsRecursionGuard(0),
    currentKeyPressCount(0),
    currentKeyPressCountOnWin32(0),
    lastPressedKey(NULL),
    oneShotKey(NULL),
    isPrefix(false),
    currentKeymap(NULL),
    currentFocusOfThread(NULL),
    hwndFocus(NULL),
    afShellExecute(NULL),
    m_variable(0),
    log(log_)
{
  int i;
  // set default lock state
  for (i = 0; i < Modifier::end; i++)
    currentLock.dontcare((Modifier::Type)i);
  for (i = Modifier::Lock0; i <= Modifier::Lock9; i++)
    currentLock.release((Modifier::Type)i);
  
  // open mayu device
  device = CreateFile(MAYU_DEVICE_FILE_NAME, GENERIC_READ | GENERIC_WRITE,
		      0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (device == INVALID_HANDLE_VALUE)
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
	didMayuStartDevice = true;
      }
      CloseServiceHandle(hscm);
    }
    
    // open mayu device
    device = CreateFile(MAYU_DEVICE_FILE_NAME, GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (device == INVALID_HANDLE_VALUE)
      throw ErrorMessage() << loadString(IDS_driverNotInstalled);
//    throw ErrorMessage() << loadString(IDS_cannotOpenDevice);
  }

  // create event for sync
  _true( eSync = CreateEvent(NULL, FALSE, FALSE, NULL) );
}


// start keyboard handler thread
void Engine::start()
{
  _true( eEvent = CreateEvent(NULL, FALSE, FALSE, NULL) );
  _true( 0 <= _beginthread(keyboardHandler, 0, this) );
  _must_be( WaitForSingleObject(eEvent, INFINITE), ==, WAIT_OBJECT_0 );
}


// stop keyboard handler thread
void Engine::stop()
{
  if (eEvent)
  {
    doForceTerminate = true;
    do
    {
      // cancel device ... TODO: this does not work on W2k
      CancelIo(device);
      //DWORD buf;
      //DeviceIoControl(device, IOCTL_MAYU_DETOUR_CANCEL,
      //                &buf, sizeof(buf), &buf, sizeof(buf), &buf, NULL);
      
      // wait for message handler thread terminate
    } while (WaitForSingleObject(eEvent, 100) != WAIT_OBJECT_0);
    _true( CloseHandle(eEvent) );
    eEvent = NULL;

    // stop mayud
    if (didMayuStartDevice)
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
  _true( CloseHandle(eSync) );
  
  // close device
  _true( CloseHandle(device) );
}


// set setting
bool Engine::setSetting(Setting *setting_)
{
  Acquire a(&cs);
  if (isSynchronizing)
    return false;

  if (setting)
    for (Keyboard::KeyIterator i = setting->keyboard.getKeyIterator();
	 *i; i ++)
    {
      Key *key = setting_->keyboard.searchKey(*(*i));
      if (key)
      {
	key->isPressed = (*i)->isPressed;
	key->isPressedOnWin32 = (*i)->isPressedOnWin32;
	key->isPressedByAssign = (*i)->isPressedByAssign;
      }
    }
  
  setting = setting_;
  if (currentFocusOfThread)
  {
    for (FocusOfThreads::iterator i = focusOfThreads.begin();
	 i != focusOfThreads.end(); i ++)
    {
      FocusOfThread *fot = &(*i).second;
      setting->keymaps.searchWindow(&fot->keymaps,
				    fot->className, fot->titleName);
    }
  }
  setting->keymaps.searchWindow(&globalFocus.keymaps, "", "");
  if (globalFocus.keymaps.empty())
  {
    Acquire a(&log, 0);
    log << "internal error: globalFocus.keymap is empty" << endl;
  }
  currentFocusOfThread = &globalFocus;
  currentKeymap = globalFocus.keymaps.front();
  hwndFocus = NULL;
  return true;
}


// focus
bool Engine::setFocus(HWND hwndFocus_, DWORD threadId, 
		      const char *className, const char *titleName,
		      bool isConsole)
{
  Acquire a(&cs);
  if (isSynchronizing)
    return false;
  if (hwndFocus_ == NULL)
    return true;

  // remove newly created thread's id from detachedThreadIds
  if (!detachedThreadIds.empty())
  {
    DetachedThreadIds::iterator i;
    do
    {
      for (i = detachedThreadIds.begin(); i != detachedThreadIds.end(); i ++)
	if (*i == threadId)
	{
	  detachedThreadIds.erase(i);
	  break;
	}
    } while (i != detachedThreadIds.end());
  }
  
  FocusOfThread *fot;
  FocusOfThreads::iterator i = focusOfThreads.find(threadId);
  if (i != focusOfThreads.end())
  {
    fot = &(*i).second;
    if (fot->hwndFocus == hwndFocus_)
      return true;
  }
  else
  {
    i = focusOfThreads.insert(
      FocusOfThreads::value_type(threadId, FocusOfThread())).first;
    fot = &(*i).second;
    fot->threadId = threadId;
  }
  fot->hwndFocus = hwndFocus_;
  fot->isConsole = isConsole;
  fot->className = className;
  fot->titleName = titleName;
  
  if (setting)
  {
    setting->keymaps.searchWindow(&fot->keymaps, className, titleName);
    assert(0 < fot->keymaps.size());
  }
  else
    fot->keymaps.clear();
  return true;
}


// lock state
bool Engine::setLockState(bool isNumLockToggled,
			  bool isCapsLockToggled,
			  bool isScrollLockToggled,
			  bool isImeLockToggled,
			  bool isImeCompToggled)
{
  Acquire a(&cs);
  if (isSynchronizing)
    return false;
  currentLock.on(Modifier::NumLock, isNumLockToggled);
  currentLock.on(Modifier::CapsLock, isCapsLockToggled);
  currentLock.on(Modifier::ScrollLock, isScrollLockToggled);
  currentLock.on(Modifier::ImeLock, isImeLockToggled);
  currentLock.on(Modifier::ImeComp, isImeCompToggled);
  return true;
}


// sync
bool Engine::syncNotify()
{
  Acquire a(&cs);
  if (!isSynchronizing)
    return false;
  _true( SetEvent(eSync) );
  return true;
}


// thread detach notify
bool Engine::threadDetachNotify(DWORD threadId)
{
  Acquire a(&cs);
  detachedThreadIds.push_back(threadId);
  return true;
}


/// get help message
void Engine::getHelpMessages(std::string *o_helpMessage,
			     std::string *o_helpTitle)
{
  Acquire a(&cs);
  *o_helpMessage = m_helpMessage;
  *o_helpTitle = m_helpTitle;
}


// shell execute
void Engine::shellExecute()
{
  Acquire a(&cs);
  ActionFunction *af = afShellExecute;
  
  istring operation  = af->args[0].getString();
  istring file       = af->args[1].getString();
  istring parameters = af->args[2].getString();
  istring directory  = af->args[3].getString();
  if (operation.empty()) operation = "open";
  
  int r = (int)ShellExecute(NULL,
			    operation.c_str(),
			    file.empty() ? NULL : file.c_str(),
			    parameters.empty() ? NULL : parameters.c_str(),
			    directory.empty() ? NULL : directory.c_str(),
			    af->args[4].getData());
  if (32 < r)
    return; // success
  
  const struct { int err; char *str; } err[] =
  {
    { 0, "The operating system is out of memory or resources." },
    { ERROR_FILE_NOT_FOUND, "The specified file was not found." },
    { ERROR_PATH_NOT_FOUND, "The specified path was not found." },
    { ERROR_BAD_FORMAT,
      "The .exe file is invalid (non-Win32R .exe or error in .exe image)." },
    { SE_ERR_ACCESSDENIED,
      "The operating system denied access to the specified file." },
    { SE_ERR_ASSOCINCOMPLETE,
      "The file name association is incomplete or invalid." },
    { SE_ERR_DDEBUSY, "The DDE transaction could not be completed "
      "because other DDE transactions were being processed. " },
    { SE_ERR_DDEFAIL, "The DDE transaction failed." },
    { SE_ERR_DDETIMEOUT, "The DDE transaction could not be completed "
      "because the request timed out." },
    { SE_ERR_DLLNOTFOUND, "The specified dynamic-link library was not found."},
    { SE_ERR_FNF, "The specified file was not found." },
    { SE_ERR_NOASSOC, "There is no application associated "
      "with the given file name extension." },
    { SE_ERR_OOM, "There was not enough memory to complete the operation." },
    { SE_ERR_PNF, "The specified path was not found." },
    { SE_ERR_SHARE, "A sharing violation occurred." },
  };

  const char *errorMessage = "Unknown error.";
  for (int i = 0; i < lengthof(err); i++)
    if (r == err[i].err)
    {
      errorMessage = err[i].str;
      break;
    }
  Acquire b(&log, 0);
  log <<"internal error: &ShellExecute("
      << af->args[0] << ", "
      << af->args[1] << ", "
      << af->args[2] << ", "
      << af->args[3] << ", "
      << af->args[4] << "): " << errorMessage;
}


// command notify
void Engine::commandNotify(
  HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  Acquire b(&log, 0);
  HWND hf = hwndFocus;
  if (!hf)
    return;

  if (GetWindowThreadProcessId(hf, NULL) == 
      GetWindowThreadProcessId(hwndAssocWindow, NULL))
    return;	// inhibit the investigation of MADO TSUKAI NO YUUTSU

  const char *target = NULL;
  int number_target = 0;
  
  if (hwnd == hf)
    target = "ToItself";
  else if (hwnd == GetParent(hf))
    target = "ToParentWindow";
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
    if (hwnd == h)
      target = "ToMainWindow";
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
      if (hwnd == h)
	target = "ToOverlappedWindow";
      else
      {
	// number
	HWND h = hf;
	for (number_target = 0; h; number_target ++, h = GetParent(h))
	  if (hwnd == h)
	    break;
	return;
      }
    }
  }

  log << "&PostMessage(";
  if (target)
    log << target;
  else
    log << number_target;
  log << ", " << message
      << ", 0x" << hex << wParam
      << ", 0x" << lParam << ") # hwnd = "
      << (int)hwnd << ", "
      << "message = " << dec;
  if (message == WM_COMMAND)
    log << "WM_COMMAND, ";
  else if (message == WM_SYSCOMMAND)
    log << "WM_SYSCOMMAND, ";
  else
    log << message << ", ";
  log << "wNotifyCode = " << HIWORD(wParam) << ", "
      << "wID = " << LOWORD(wParam) << ", "
      << "hwndCtrl = 0x" << hex << lParam << dec << endl;
}
