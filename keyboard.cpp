//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting.cpp


#include "keyboard.h"

#include <algorithm>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Key


// add a name or an alias of key
void Key::addName(const tstringi &i_name)
{
  m_names.push_back(i_name);
}


// add a scan code
void Key::addScanCode(const ScanCode &i_sc)
{
  ASSERT(m_scanCodesSize < MAX_SCAN_CODES_SIZE);
  m_scanCodes[m_scanCodesSize ++] = i_sc;
}


// initializer
Key &Key::initialize()
{
  m_names.clear();
  m_isPressed = false;
  m_isPressedOnWin32 = false;
  m_isPressedByAssign = false;
  m_scanCodesSize = 0;
  return *this;
}


// equation by name
bool Key::operator==(const tstringi &i_name) const
{
  return std::find(m_names.begin(), m_names.end(), i_name) != m_names.end();
}

  
// is the scan code of this key ?
bool Key::isSameScanCode(const Key &i_key) const
{
  if (m_scanCodesSize != i_key.m_scanCodesSize)
    return false;
  return isPrefixScanCode(i_key);
}


// is the key's scan code the prefix of this key's scan code ?
bool Key::isPrefixScanCode(const Key &i_key) const
{
  for (size_t i = 0; i < i_key.m_scanCodesSize; ++ i)
    if (m_scanCodes[i] != i_key.m_scanCodes[i])
      return false;
  return true;
}


// stream output
tostream &operator<<(tostream &i_ost, const Key &i_mk)
{
  return i_ost << i_mk.getName();
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Modifier


Modifier::Modifier()
  : m_modifiers(0),
    m_dontcares(0)
{
  ASSERT(Type_end <= (sizeof(MODIFIERS) * 8));
  static const Type defaultDontCare[] =
  {
    Type_Up, Type_Down,
    Type_ImeLock, Type_ImeComp, Type_NumLock, Type_CapsLock, Type_ScrollLock,
    Type_Lock0, Type_Lock1, Type_Lock2, Type_Lock3, Type_Lock4,
    Type_Lock5, Type_Lock6, Type_Lock7, Type_Lock8, Type_Lock9,
  };
  for (size_t i = 0; i < NUMBER_OF(defaultDontCare); ++ i)
    dontcare(defaultDontCare[i]);
}


// add m's modifiers where this dontcare
void Modifier::add(const Modifier &i_m)
{
  for (int i = 0; i < Type_end; ++ i)
  {
    if (isDontcare(static_cast<Modifier::Type>(i)))
      if (!i_m.isDontcare(static_cast<Modifier::Type>(i)))
	if (i_m.isPressed(static_cast<Modifier::Type>(i)))
	  press(static_cast<Modifier::Type>(i));
	else
	  release(static_cast<Modifier::Type>(i));
  }
}

// stream output
tostream &operator<<(tostream &i_ost, const Modifier &i_m)
{
  struct Mods
  {
    Modifier::Type m_mt;
    const _TCHAR *m_symbol;
  };
  
  const static Mods mods[] =
  {
    { Modifier::Type_Up, _T("U-") }, { Modifier::Type_Down, _T("  ") },
    { Modifier::Type_Shift, _T("S-") }, { Modifier::Type_Alt, _T("A-") },
    { Modifier::Type_Control, _T("C-") }, { Modifier::Type_Windows, _T("W-") },
    { Modifier::Type_ImeLock, _T("IL-") },
    { Modifier::Type_ImeComp, _T("IC-") },
    { Modifier::Type_ImeComp, _T("I-") },
    { Modifier::Type_NumLock, _T("NL-") },
    { Modifier::Type_CapsLock, _T("CL-") },
    { Modifier::Type_ScrollLock, _T("SL-") },
    { Modifier::Type_Mod0, _T("M0-") }, { Modifier::Type_Mod1, _T("M1-") },
    { Modifier::Type_Mod2, _T("M2-") }, { Modifier::Type_Mod3, _T("M3-") },
    { Modifier::Type_Mod4, _T("M4-") }, { Modifier::Type_Mod5, _T("M5-") },
    { Modifier::Type_Mod6, _T("M6-") }, { Modifier::Type_Mod7, _T("M7-") },
    { Modifier::Type_Mod8, _T("M8-") }, { Modifier::Type_Mod9, _T("M9-") },
    { Modifier::Type_Lock0, _T("L0-") }, { Modifier::Type_Lock1, _T("L1-") },
    { Modifier::Type_Lock2, _T("L2-") }, { Modifier::Type_Lock3, _T("L3-") },
    { Modifier::Type_Lock4, _T("L4-") }, { Modifier::Type_Lock5, _T("L5-") },
    { Modifier::Type_Lock6, _T("L6-") }, { Modifier::Type_Lock7, _T("L7-") },
    { Modifier::Type_Lock8, _T("L8-") }, { Modifier::Type_Lock9, _T("L9-") },
  };

  for (size_t i = 0; i < NUMBER_OF(mods); ++ i)
    if (!i_m.isDontcare(mods[i].m_mt) && i_m.isPressed(mods[i].m_mt))
      i_ost << mods[i].m_symbol;

  return i_ost;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ModifiedKey


// stream output
tostream &operator<<(tostream &i_ost, const ModifiedKey &i_mk)
{
  if (i_mk.m_key)
    i_ost << i_mk.m_modifier << *i_mk.m_key;
  return i_ost;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Keyboard::KeyIterator


Keyboard::KeyIterator::KeyIterator(Keys *i_hashedKeys, size_t i_hashedKeysSize)
  : m_hashedKeys(i_hashedKeys),
    m_hashedKeysSize(i_hashedKeysSize),
    m_i((*m_hashedKeys).begin())
{
  if ((*m_hashedKeys).empty())
    next();
}


void Keyboard::KeyIterator::next()
{
  if (m_hashedKeysSize == 0)
    return;
  ++ m_i;
  if (m_i == (*m_hashedKeys).end())
  {
    do
    {
      -- m_hashedKeysSize;
      ++ m_hashedKeys;
    } while (0 < m_hashedKeysSize && (*m_hashedKeys).empty());
    if (0 < m_hashedKeysSize)
      m_i = (*m_hashedKeys).begin();
  }
}


Key *Keyboard::KeyIterator::operator *()
{
  if (m_hashedKeysSize == 0)
    return NULL;
  return &*m_i;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Keyboard


Keyboard::Keys &Keyboard::getKeys(const Key &i_key)
{
  ASSERT(1 <= i_key.getScanCodesSize());
  return m_hashedKeys[i_key.getScanCodes()->m_scan % HASHED_KEYS_SIZE];
}


// add a key
void Keyboard::addKey(const Key &i_key)
{
  getKeys(i_key).push_front(i_key);
}


// add a key name alias
void Keyboard::addAlias(const tstringi &i_aliasName, Key *i_key)
{
  m_aliases.insert(Aliases::value_type(i_aliasName, i_key));
}


// add a modifier key
void Keyboard::addModifier(Modifier::Type i_mt, Key *i_key)
{
  ASSERT((int)i_mt < (int)Modifier::Type_BASIC);
  if (std::find(m_mods[i_mt].begin(), m_mods[i_mt].end(), i_key)
      != m_mods[i_mt].end())
    return; // already added
  m_mods[i_mt].push_back(i_key);
}


// search a key
Key *Keyboard::searchKey(const Key &i_key)
{
  Keys &keys = getKeys(i_key);
  for (Keys::iterator i = keys.begin(); i != keys.end(); ++ i)
    if ((*i).isSameScanCode(i_key))
      return &*i;
  return NULL;
}


// search a key (of which the key's scan code is the prefix)
Key *Keyboard::searchPrefixKey(const Key &i_key)
{
  Keys &keys = getKeys(i_key);
  for (Keys::iterator i = keys.begin(); i != keys.end(); ++ i)
    if ((*i).isPrefixScanCode(i_key))
      return &*i;
  return NULL;
}

  
// search a key by name
Key *Keyboard::searchKey(const tstringi &i_name)
{
  Aliases::iterator i = m_aliases.find(i_name);
  if (i != m_aliases.end())
    return (*i).second;
  return searchKeyByNonAliasName(i_name);
}


// search a key by non-alias name
Key *Keyboard::searchKeyByNonAliasName(const tstringi &i_name)
{
  for (int i = 0; i < HASHED_KEYS_SIZE; ++ i)
  {
    Keys &keys = m_hashedKeys[i];
    Keys::iterator i = std::find(keys.begin(), keys.end(), i_name);
    if (i != keys.end())
      return &*i;
  }
  return NULL;
}
