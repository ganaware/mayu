// ////////////////////////////////////////////////////////////////////////////
// setting.cpp


#include "keyboard.h"

#include <algorithm>


using namespace std;


// ////////////////////////////////////////////////////////////////////////////
// Key


// add a name or an alias of key
void Key::addName(const istring &i_name)
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
bool Key::operator==(const istring &i_name) const
{
  return find(m_names.begin(), m_names.end(), i_name) != m_names.end();
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


/// stream output
std::ostream &operator<<(std::ostream &i_ost, const Key &i_mk)
{
  return i_ost << i_mk.getName();
}


// ////////////////////////////////////////////////////////////////////////////
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

/// stream output
std::ostream &operator<<(std::ostream &i_ost, const Modifier &i_m)
{
  struct Mods
  {
    Modifier::Type m_mt;
    const char *m_symbol;
  };
  
  const static Mods mods[] =
  {
    { Modifier::Type_Up, "U-" }, { Modifier::Type_Down, "  " },
    { Modifier::Type_Shift, "S-" }, { Modifier::Type_Alt, "A-" },
    { Modifier::Type_Control, "C-" }, { Modifier::Type_Windows, "W-" },
    { Modifier::Type_ImeLock, "IL-" }, { Modifier::Type_ImeComp, "IC-" },
    { Modifier::Type_ImeComp, "I-" }, { Modifier::Type_NumLock, "NL-" },
    { Modifier::Type_CapsLock, "CL-" }, { Modifier::Type_ScrollLock, "SL-" },
    { Modifier::Type_Mod0, "M0-" }, { Modifier::Type_Mod1, "M1-" },
    { Modifier::Type_Mod2, "M2-" }, { Modifier::Type_Mod3, "M3-" },
    { Modifier::Type_Mod4, "M4-" }, { Modifier::Type_Mod5, "M5-" },
    { Modifier::Type_Mod6, "M6-" }, { Modifier::Type_Mod7, "M7-" },
    { Modifier::Type_Mod8, "M8-" }, { Modifier::Type_Mod9, "M9-" },
    { Modifier::Type_Lock0, "L0-" }, { Modifier::Type_Lock1, "L1-" },
    { Modifier::Type_Lock2, "L2-" }, { Modifier::Type_Lock3, "L3-" },
    { Modifier::Type_Lock4, "L4-" }, { Modifier::Type_Lock5, "L5-" },
    { Modifier::Type_Lock6, "L6-" }, { Modifier::Type_Lock7, "L7-" },
    { Modifier::Type_Lock8, "L8-" }, { Modifier::Type_Lock9, "L9-" },
  };

  for (size_t i = 0; i < NUMBER_OF(mods); ++ i)
    if (!i_m.isDontcare(mods[i].m_mt) && i_m.isPressed(mods[i].m_mt))
      i_ost << mods[i].m_symbol;

  return i_ost;
}


// ////////////////////////////////////////////////////////////////////////////
// ModifiedKey


/// stream output
std::ostream &operator<<(std::ostream &i_ost, const ModifiedKey &i_mk)
{
  if (i_mk.m_key)
    i_ost << i_mk.m_modifier << *i_mk.m_key;
  return i_ost;
}


// ////////////////////////////////////////////////////////////////////////////
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


// ////////////////////////////////////////////////////////////////////////////
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
void Keyboard::addAlias(const istring &i_aliasName, Key *i_key)
{
  m_aliases.insert(Aliases::value_type(i_aliasName, i_key));
}


// add a modifier key
void Keyboard::addModifier(Modifier::Type i_mt, Key *i_key)
{
  ASSERT((int)i_mt < (int)Modifier::Type_BASIC);
  if (find(m_mods[i_mt].begin(), m_mods[i_mt].end(), i_key)
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
Key *Keyboard::searchKey(const istring &i_name)
{
  Aliases::iterator i = m_aliases.find(i_name);
  if (i != m_aliases.end())
    return (*i).second;
  return searchKeyByNonAliasName(i_name);
}


// search a key by non-alias name
Key *Keyboard::searchKeyByNonAliasName(const istring &i_name)
{
  for (int i = 0; i < HASHED_KEYS_SIZE; ++ i)
  {
    Keys &keys = m_hashedKeys[i];
    Keys::iterator i = find(keys.begin(), keys.end(), i_name);
    if (i != keys.end())
      return &*i;
  }
  return NULL;
}
