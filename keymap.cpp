///////////////////////////////////////////////////////////////////////////////
// setting.cpp


#include "keymap.h"
#include "errormessage.h"
#include "stringtool.h"


using namespace std;


///////////////////////////////////////////////////////////////////////////////
// KeySeq


void KeySeq::copy()
{
  for (vector<Action *>::iterator i = actions.begin();
       i != actions.end(); i++)
    switch ((*i)->type)
    {
      case Action::ActKey:
	(*i) = new ActionKey(*(ActionKey *)(*i)); break;
      case Action::ActKeySeq:
	(*i) = new ActionKeySeq(*(ActionKeySeq *)(*i)); break;
      case Action::ActFunction:
	(*i) = new ActionFunction(*(ActionFunction *)(*i)); break;
    }
}


void KeySeq::clear()
{
  for (vector<Action *>::iterator i = actions.begin();
       i != actions.end(); i++)
    switch ((*i)->type)
    {
      case Action::ActKey: delete (ActionKey *)(*i); break;
      case Action::ActKeySeq: delete (ActionKeySeq *)(*i); break;
      case Action::ActFunction: delete (ActionFunction *)(*i); break;
    }
}


KeySeq::KeySeq()
{
}


KeySeq::KeySeq(const KeySeq &ks)
  : actions(ks.actions)
{
  copy();
}


KeySeq::~KeySeq()
{
  clear();
}


KeySeq &KeySeq::operator=(const KeySeq &ks)
{
  if (this != &ks)
  {
    clear();
    actions = ks.actions;
    copy();
  }
  return *this;
}


KeySeq &KeySeq::add(const ActionKey &ak)
{
  actions.push_back(new ActionKey(ak));
  return *this;
}


KeySeq &KeySeq::add(const ActionKeySeq &aks)
{
  actions.push_back(new ActionKeySeq(aks));
  return *this;
}


KeySeq &KeySeq::add(const ActionFunction &af)
{
  actions.push_back(new ActionFunction(af));
  return *this;
}


///////////////////////////////////////////////////////////////////////////////
// Keymap


Keymap::KeyAssignments &Keymap::getKeyAssignments(const ModifiedKey &mk)
{
  assert(1 <= mk.key->getLengthof_scanCodes());
  return hashedKeyAssignments[mk.key->getScanCodes()->scan %
			     lengthof_hashedKeyAssignment];
}
  

Keymap::Keymap(Type type_,
	       const istring &name_,
	       const istring &windowClass_,
	       const istring &windowTitle_,
	       KeySeq *defaultKeySeq_,
	       Keymap *parentKeymap_)
  : type(type_),
    name(name_),
    defaultKeySeq(defaultKeySeq_),
    parentKeymap(parentKeymap_)
{
  if (type == TypeWindowAnd || type == TypeWindowOr)
    try
    {
      windowClass.compile(windowClass_, true);
      windowTitle.compile(windowTitle_, true);
    }
    catch (Regexp::InvalidRegexp &e)
    {
      throw ErrorMessage() << e.what();
    }
}


// add a key assignment;
void Keymap::addAssignment(const ModifiedKey &mk, KeySeq *keySeq)
{
  KeyAssignments &ka = getKeyAssignments(mk);
  for (KeyAssignments::iterator i = ka.begin(); i != ka.end(); i++)
    if ((*i).modifiedKey == mk)
    {
      (*i).keySeq = keySeq;
      return;
    }
  ka.push_front(KeyAssignment(mk, keySeq));
}


// add modifier
void Keymap::addModifier(Modifier::Type mt, AssignOperator ao,
			 AssignMode am, Key *key)
{
  if (ao == AOnew)
    modAssignments[mt].clear();
  else
  {
    for (ModAssignments::iterator i = modAssignments[mt].begin();
	 i != modAssignments[mt].end(); i++)
      if ((*i).key == key)
      {
	(*i).assignOperator = ao;
	(*i).assignMode = am;
	return;
      }
  }
  ModAssignment ma;
  ma.assignOperator = ao;
  ma.assignMode = am;
  ma.key = key;
  modAssignments[mt].push_back(ma);
}

  
// search
const Keymap::KeyAssignment *
Keymap::searchAssignment(const ModifiedKey &mk)
{
  KeyAssignments &ka = getKeyAssignments(mk);
  for (KeyAssignments::iterator i = ka.begin(); i != ka.end(); i++)
  {
    try
    {
      if ((*i).modifiedKey.key == mk.key &&
	  (*i).modifiedKey.modifier.doesMatch(mk.modifier))
	return &(*i);
    }
    catch (Regexp::InvalidRegexp &e)
    {
      printf("error1: %s\n", e.what());
    }
  }
  return NULL;
}


// does same window
bool Keymap::doesSameWindow(const istring className, const istring &titleName)
{
  if (type == TypeKeymap)
    return false;
  try
  {
    if (windowClass.doesMatch(className))
    {
      if (type == TypeWindowAnd)
	return windowTitle.doesMatch(titleName);
      else // type == TypeWindowOr
	return true;
    }
    else
    {
      if (type == TypeWindowAnd)
	return false;
      else // type == TypeWindowOr
	return windowTitle.doesMatch(titleName);
    }
  }
  catch (Regexp::InvalidRegexp &e)
  {
    printf("error2: %s\n", e.what());
    return false;
  }
}


// adjust modifier
void Keymap::adjustModifier(Keyboard &keyboard)
{
  for (int i = 0; i < Modifier::end; i++)
  {
    ModAssignments mos;
    if (parentKeymap)
      mos = parentKeymap->modAssignments[i];
    else
    {
      // set default modifiers
      if (i < Modifier::BASIC)
      {
	Keyboard::Mods mods = keyboard.getModifiers((Modifier::Type)i);
	for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); j++)
	{
	  ModAssignment ma;
	  ma.assignOperator = AOadd;
	  ma.assignMode = AMnormal;
	  ma.key = *j;
	  mos.push_back(ma);
	}
      }
    }
    
    // mod adjust
    for (list<ModAssignment>::iterator mai = modAssignments[i].begin();
	 mai != modAssignments[i].end(); mai++)
    {
      ModAssignment ma = *mai;
      ma.assignOperator = AOnew;
      switch ((*mai).assignOperator)
      {
	case AOnew:
	{
	  mos.clear();
	  mos.push_back(ma);
	  break;
	}
	case AOadd:
	{
	  mos.push_back(ma);
	  break;
	}
	case AOsub:
	{
	  for (list<ModAssignment>::iterator j = mos.begin();
	       j != mos.end(); j++)
	    if ((*j).key == ma.key)
	    {
	      mos.erase(j);
	      goto break_for;
	    }
	  break;
	}
	case AOoverwrite:
	{
	  for (list<ModAssignment>::iterator j = mos.begin();
	       j != mos.end(); j ++)
	    (*j).assignMode = (*mai).assignMode;
	  break;
	}
      }
    }
    break_for:

    // erase redundant modifiers
    for (list<ModAssignment>::iterator j = mos.begin(); j != mos.end(); j ++)
    {
      list<ModAssignment>::iterator k;
      for (k = j, k ++; k != mos.end(); k ++)
	if ((*k).key == (*j).key)
	  break;
      if (k != mos.end())
      {
	k = j;
	j ++;
	mos.erase(k);
	break;
      }
    }
    
    modAssignments[i] = mos;
  }
}


///////////////////////////////////////////////////////////////////////////////
// Keymaps


Keymaps::Keymaps()
{
}


// search by name
Keymap *Keymaps::searchByName(const istring &name)
{
  for (list<Keymap>::iterator i = keymaps.begin(); i != keymaps.end(); i++)
    if ((*i).getName() == name)
      return &*i;
  return NULL;
}


// search window
void Keymaps::searchWindow(list<Keymap *> *keymaps_r,
			   const istring className,
			   const istring &titleName)
{
  keymaps_r->clear();
  for (list<Keymap>::iterator i = keymaps.begin(); i != keymaps.end(); i++)
    if ((*i).doesSameWindow(className, titleName))
      keymaps_r->push_back(&(*i));
}


// add keymap
Keymap *Keymaps::add(const Keymap &keymap)
{
  Keymap *k;
  k = searchByName(keymap.getName());
  if (k)
    return k;
  keymaps.push_front(keymap);
  return &keymaps.front();
}


// adjust modifier
void Keymaps::adjustModifier(Keyboard &keyboard)
{
  for (list<Keymap>::reverse_iterator i = keymaps.rbegin();
       i != keymaps.rend(); i++)
    (*i).adjustModifier(keyboard);
}


///////////////////////////////////////////////////////////////////////////////
// KeySeqs


// add a named keyseq (name can be empty)
KeySeq *KeySeqs::add(const istring &name, const KeySeq &keySeq)
{
  if (!name.empty())
  {
    KeySeq *ks = searchByName(name);
    if (ks)
      return &(*ks = keySeq);
  }
  namedKeySeqs.push_front(NamedKeySeq(name, keySeq));
  return &namedKeySeqs.front().keySeq;
}


// search by name
KeySeq *KeySeqs::searchByName(const istring &name)
{
  for (NamedKeySeqs::iterator i = namedKeySeqs.begin();
       i != namedKeySeqs.end(); i++)
    if ((*i).name == name)
      return &(*i).keySeq;
  return NULL;
}
