// ////////////////////////////////////////////////////////////////////////////
// setting.cpp


#include "keymap.h"
#include "errormessage.h"
#include "stringtool.h"
#include <algorithm>


using namespace std;


// ////////////////////////////////////////////////////////////////////////////
// KeySeq


void KeySeq::copy()
{
  for (Actions::iterator i = m_actions.begin();
       i != m_actions.end(); ++ i)
    switch ((*i)->m_type)
    {
      case Action::Type_key:
	(*i) = new ActionKey(*reinterpret_cast<ActionKey *>(*i)); break;
      case Action::Type_keySeq:
	(*i) = new ActionKeySeq(*reinterpret_cast<ActionKeySeq *>(*i)); break;
      case Action::Type_function:
	(*i) = new ActionFunction(*reinterpret_cast<ActionFunction *>(*i));
	break;
    }
}


void KeySeq::clear()
{
  for (Actions::iterator i = m_actions.begin();
       i != m_actions.end(); ++ i)
    switch ((*i)->m_type)
    {
      case Action::Type_key:
	delete reinterpret_cast<ActionKey *>(*i); break;
      case Action::Type_keySeq:
	delete reinterpret_cast<ActionKeySeq *>(*i); break;
      case Action::Type_function:
	delete reinterpret_cast<ActionFunction *>(*i); break;
    }
}


KeySeq::KeySeq(const istring &i_name)
  : m_name(i_name)
{
}


KeySeq::KeySeq(const KeySeq &i_ks)
  : m_actions(i_ks.m_actions),
    m_name(i_ks.m_name)
{
  copy();
}


KeySeq::~KeySeq()
{
  clear();
}


KeySeq &KeySeq::operator=(const KeySeq &i_ks)
{
  if (this != &i_ks)
  {
    clear();
    m_actions = i_ks.m_actions;
    copy();
  }
  return *this;
}


KeySeq &KeySeq::add(const ActionKey &i_ak)
{
  m_actions.push_back(new ActionKey(i_ak));
  return *this;
}


KeySeq &KeySeq::add(const ActionKeySeq &i_aks)
{
  m_actions.push_back(new ActionKeySeq(i_aks));
  return *this;
}


KeySeq &KeySeq::add(const ActionFunction &i_af)
{
  m_actions.push_back(new ActionFunction(i_af));
  return *this;
}


// stream output
std::ostream &
operator<<(std::ostream &i_ost, const KeySeq &i_ks)
{
  for (KeySeq::Actions::const_iterator
	 i = i_ks.m_actions.begin(); i != i_ks.m_actions.end(); ++ i)
  {
    switch ((*i)->m_type)
    {
      case Action::Type_key:
	i_ost << reinterpret_cast<const ActionKey *>(*i)->m_modifiedKey;
	break;
      case Action::Type_keySeq:
	i_ost << "$"
	      << reinterpret_cast<const ActionKeySeq *>(*i)->m_keySeq->m_name;
	break;
      case Action::Type_function:
      {
	const ActionFunction *af =
	  reinterpret_cast<const ActionFunction *>(*i);
	i_ost << af->m_modifier << *af->m_function;
	if (af->m_args.size())
	{
	  i_ost << "(";
	  for (ActionFunction::Args::const_iterator
		 i = af->m_args.begin(); i != af->m_args.end(); ++ i)
	  {
	    if (i != af->m_args.begin())
	      i_ost << ", ";
	    i_ost << *i;
	  }
	  i_ost << ")";
	}
	break;
      }
    }
    i_ost << " ";
  }
  return i_ost;
}


// ////////////////////////////////////////////////////////////////////////////
// Keymap


Keymap::KeyAssignments &Keymap::getKeyAssignments(const ModifiedKey &i_mk)
{
  ASSERT(1 <= i_mk.m_key->getScanCodesSize());
  return m_hashedKeyAssignments[i_mk.m_key->getScanCodes()->m_scan %
			       HASHED_KEY_ASSIGNMENT_SIZE];
}
  

Keymap::Keymap(Type i_type,
	       const istring &i_name,
	       const istring &i_windowClass,
	       const istring &i_windowTitle,
	       KeySeq *i_defaultKeySeq,
	       Keymap *i_parentKeymap)
  : m_type(i_type),
    m_name(i_name),
    m_defaultKeySeq(i_defaultKeySeq),
    m_parentKeymap(i_parentKeymap)
{
  if (i_type == Type_windowAnd || i_type == Type_windowOr)
    try
    {
      m_windowClass.compile(i_windowClass, true);
      m_windowTitle.compile(i_windowTitle, true);
    }
    catch (Regexp::InvalidRegexp &i_e)
    {
      throw ErrorMessage() << i_e.what();
    }
}


// add a key assignment;
void Keymap::addAssignment(const ModifiedKey &i_mk, KeySeq *i_keySeq)
{
  KeyAssignments &ka = getKeyAssignments(i_mk);
  for (KeyAssignments::iterator i = ka.begin(); i != ka.end(); ++ i)
    if ((*i).m_modifiedKey == i_mk)
    {
      (*i).m_keySeq = i_keySeq;
      return;
    }
  ka.push_front(KeyAssignment(i_mk, i_keySeq));
}


// add modifier
void Keymap::addModifier(Modifier::Type i_mt, AssignOperator i_ao,
			 AssignMode i_am, Key *i_key)
{
  if (i_ao == AO_new)
    m_modAssignments[i_mt].clear();
  else
  {
    for (ModAssignments::iterator i = m_modAssignments[i_mt].begin();
	 i != m_modAssignments[i_mt].end(); ++ i)
      if ((*i).m_key == i_key)
      {
	(*i).m_assignOperator = i_ao;
	(*i).m_assignMode = i_am;
	return;
      }
  }
  ModAssignment ma;
  ma.m_assignOperator = i_ao;
  ma.m_assignMode = i_am;
  ma.m_key = i_key;
  m_modAssignments[i_mt].push_back(ma);
}

  
// search
const Keymap::KeyAssignment *
Keymap::searchAssignment(const ModifiedKey &i_mk)
{
  KeyAssignments &ka = getKeyAssignments(i_mk);
  for (KeyAssignments::iterator i = ka.begin(); i != ka.end(); ++ i)
  {
    try
    {
      if ((*i).m_modifiedKey.m_key == i_mk.m_key &&
	  (*i).m_modifiedKey.m_modifier.doesMatch(i_mk.m_modifier))
	return &(*i);
    }
    catch (Regexp::InvalidRegexp &i_e)
    {
      printf("error1: %s\n", i_e.what());
    }
  }
  return NULL;
}


// does same window
bool Keymap::doesSameWindow(const istring i_className,
			    const istring &i_titleName)
{
  if (m_type == Type_keymap)
    return false;
  try
  {
    if (m_windowClass.doesMatch(i_className))
    {
      if (m_type == Type_windowAnd)
	return m_windowTitle.doesMatch(i_titleName);
      else // type == TypeWindowOr
	return true;
    }
    else
    {
      if (m_type == Type_windowAnd)
	return false;
      else // type == TypeWindowOr
	return m_windowTitle.doesMatch(i_titleName);
    }
  }
  catch (Regexp::InvalidRegexp &i_e)
  {
    printf("error2: %s\n", i_e.what());
    return false;
  }
}


// adjust modifier
void Keymap::adjustModifier(Keyboard &i_keyboard)
{
  for (int i = 0; i < Modifier::Type_end; ++ i)
  {
    ModAssignments mos;
    if (m_parentKeymap)
      mos = m_parentKeymap->m_modAssignments[i];
    else
    {
      // set default modifiers
      if (i < Modifier::Type_BASIC)
      {
	Keyboard::Mods mods =
	  i_keyboard.getModifiers(static_cast<Modifier::Type>(i));
	for (Keyboard::Mods::iterator j = mods.begin(); j != mods.end(); ++ j)
	{
	  ModAssignment ma;
	  ma.m_assignOperator = AO_add;
	  ma.m_assignMode = AM_normal;
	  ma.m_key = *j;
	  mos.push_back(ma);
	}
      }
    }
    
    // mod adjust
    for (ModAssignments::iterator mai = m_modAssignments[i].begin();
	 mai != m_modAssignments[i].end(); ++ mai)
    {
      ModAssignment ma = *mai;
      ma.m_assignOperator = AO_new;
      switch ((*mai).m_assignOperator)
      {
	case AO_new:
	{
	  mos.clear();
	  mos.push_back(ma);
	  break;
	}
	case AO_add:
	{
	  mos.push_back(ma);
	  break;
	}
	case AO_sub:
	{
	  for (ModAssignments::iterator j = mos.begin();
	       j != mos.end(); ++ j)
	    if ((*j).m_key == ma.m_key)
	    {
	      mos.erase(j);
	      goto break_for;
	    }
	  break;
	}
	case AO_overwrite:
	{
	  for (ModAssignments::iterator j = mos.begin();
	       j != mos.end(); ++ j)
	    (*j).m_assignMode = (*mai).m_assignMode;
	  break;
	}
      }
    }
    break_for:

    // erase redundant modifiers
    for (ModAssignments::iterator j = mos.begin(); j != mos.end(); ++ j)
    {
      ModAssignments::iterator k;
      for (k = j, ++ k; k != mos.end(); ++ k)
	if ((*k).m_key == (*j).m_key)
	  break;
      if (k != mos.end())
      {
	k = j;
	++ j;
	mos.erase(k);
	break;
      }
    }
    
    m_modAssignments[i] = mos;
  }
}

/// describe
void Keymap::describe(std::ostream &i_ost, DescribedKeys *o_dk) const
{
  switch (m_type)
  {
    case Type_keymap: i_ost << "keymap "; break;
    case Type_windowAnd: i_ost << "window "; break;
    case Type_windowOr: i_ost << "window "; break;
  }
  i_ost << m_name << " /.../";
  if (m_parentKeymap)
    i_ost << " : " << m_parentKeymap->m_name;
  i_ost << " => " << *m_defaultKeySeq << endl;

  typedef std::vector<KeyAssignment> SortedKeyAssignments;
  SortedKeyAssignments ska;
  for (size_t i = 0; i < HASHED_KEY_ASSIGNMENT_SIZE; ++ i)
  {
    const KeyAssignments &ka = m_hashedKeyAssignments[i];
    for (KeyAssignments::const_iterator j = ka.begin(); j != ka.end(); ++ j)
      ska.push_back(*j);
  }
  std::sort(ska.begin(), ska.end());
  for (SortedKeyAssignments::iterator i = ska.begin(); i != ska.end(); ++ i)
  {
    if (o_dk)
    {
      DescribedKeys::iterator
	j = find(o_dk->begin(), o_dk->end(), i->m_modifiedKey);
      if (j != o_dk->end())
	continue;
    }
    i_ost << " key " << i->m_modifiedKey << "\t=> " << *i->m_keySeq << endl;
    if (o_dk)
      o_dk->push_back(i->m_modifiedKey);
  }
  i_ost << endl;

  if (m_parentKeymap)
    m_parentKeymap->describe(i_ost, o_dk);
}



// ////////////////////////////////////////////////////////////////////////////
// Keymaps


Keymaps::Keymaps()
{
}


// search by name
Keymap *Keymaps::searchByName(const istring &i_name)
{
  for (KeymapList::iterator
	 i = m_keymapList.begin(); i != m_keymapList.end(); ++ i)
    if ((*i).getName() == i_name)
      return &*i;
  return NULL;
}


// search window
void Keymaps::searchWindow(KeymapPtrList *o_keymapPtrList,
			   const istring &i_className,
			   const istring &i_titleName)
{
  o_keymapPtrList->clear();
  for (KeymapList::iterator
	 i = m_keymapList.begin(); i != m_keymapList.end(); ++ i)
    if ((*i).doesSameWindow(i_className, i_titleName))
      o_keymapPtrList->push_back(&(*i));
}


// add keymap
Keymap *Keymaps::add(const Keymap &i_keymap)
{
  Keymap *k;
  k = searchByName(i_keymap.getName());
  if (k)
    return k;
  m_keymapList.push_front(i_keymap);
  return &m_keymapList.front();
}


// adjust modifier
void Keymaps::adjustModifier(Keyboard &i_keyboard)
{
  for (KeymapList::reverse_iterator i = m_keymapList.rbegin();
       i != m_keymapList.rend(); ++ i)
    (*i).adjustModifier(i_keyboard);
}


// ////////////////////////////////////////////////////////////////////////////
// KeySeqs


// add a named keyseq (name can be empty)
KeySeq *KeySeqs::add(const KeySeq &i_keySeq)
{
  if (!i_keySeq.getName().empty())
  {
    KeySeq *ks = searchByName(i_keySeq.getName());
    if (ks)
      return &(*ks = i_keySeq);
  }
  m_keySeqList.push_front(i_keySeq);
  return &m_keySeqList.front();
}


// search by name
KeySeq *KeySeqs::searchByName(const istring &i_name)
{
  for (KeySeqList::iterator
	 i = m_keySeqList.begin(); i != m_keySeqList.end(); ++ i)
    if ((*i).getName() == i_name)
      return &(*i);
  return NULL;
}
