///////////////////////////////////////////////////////////////////////////////
// setting.cpp


#include "keyboard.h"

#include <algorithm>


using namespace std;


///////////////////////////////////////////////////////////////////////////////
// Key


// add a name or an alias of key
void Key::addName(const istring &name)
{
  names.push_back(name);
}


// add a scan code
void Key::addScanCode(const ScanCode &sc)
{
  assert(lengthof_scanCodes < maxlengthof_scanCodes);
  scanCodes[lengthof_scanCodes ++] = sc;
}


// initializer
Key &Key::initialize()
{
  names.clear();
  isPressed = false;
  isPressedOnWin32 = false;
  isPressedByAssign = false;
  lengthof_scanCodes = 0;
  return *this;
}


// equation by name
bool Key::operator==(const istring &name) const
{
  return find(names.begin(), names.end(), name) != names.end();
}

  
// is the scan code of this key ?
bool Key::isSameScanCode(const Key &key) const
{
  if (lengthof_scanCodes != key.lengthof_scanCodes)
    return false;
  return isPrefixScanCode(key);
}


// is the key's scan code the prefix of this key's scan code ?
bool Key::isPrefixScanCode(const Key &key) const
{
  for (size_t i = 0; i < key.lengthof_scanCodes; i++)
    if (scanCodes[i] != key.scanCodes[i])
      return false;
  return true;
}


///////////////////////////////////////////////////////////////////////////////
// Modifier


Modifier::Modifier()
  : modifiers(0),
    dontcares(0)
{
  assert(end <= (sizeof(MODIFIERS) * 8));
  static const Type defaultDontCare[] =
  {
    Up, Down,
    ImeLock, ImeComp, NumLock, CapsLock, ScrollLock,
    Lock0, Lock1, Lock2, Lock3, Lock4, Lock5, Lock6, Lock7, Lock8, Lock9,
  };
  for (int i = 0; i < lengthof(defaultDontCare); i++)
    dontcare(defaultDontCare[i]);
}


// add m's modifiers where this dontcare
Modifier &Modifier::operator+=(const Modifier &m)
{
  for (int i = 0; i < end; i++)
  {
    if (isDontcare((Modifier::Type)i))
      if (!m.isDontcare((Modifier::Type)i))
	if (m.isPressed((Modifier::Type)i))
	  press((Modifier::Type)i);
	else
	  release((Modifier::Type)i);
  }
  return *this;
}


///////////////////////////////////////////////////////////////////////////////
// Keyboard::KeyIterator


Keyboard::KeyIterator::KeyIterator
(Keys *hashedKeys_, size_t lengthof_hashedKeys_)
  : hashedKeys(hashedKeys_),
    lengthof_hashedKeys(lengthof_hashedKeys_),
    i((*hashedKeys).begin())
{
  if ((*hashedKeys).empty())
    next();
}


void Keyboard::KeyIterator::next()
{
  if (lengthof_hashedKeys == 0)
    return;
  i ++;
  if (i == (*hashedKeys).end())
  {
    do
    {
      lengthof_hashedKeys --;
      hashedKeys ++;
    } while (0 < lengthof_hashedKeys && (*hashedKeys).empty());
    if (0 < lengthof_hashedKeys)
      i = (*hashedKeys).begin();
  }
}


Key *Keyboard::KeyIterator::operator *()
{
  if (lengthof_hashedKeys == 0)
    return NULL;
  return &*i;
}


///////////////////////////////////////////////////////////////////////////////
// Keyboard


Keyboard::Keys &Keyboard::getKeys(const Key &key)
{
  assert(1 <= key.getLengthof_scanCodes());
  return hashedKeys[key.getScanCodes()->scan % lengthof_hashedKeys];
}


// add a key
void Keyboard::addKey(const Key &key)
{
  getKeys(key).push_front(key);
}


// add a key name alias
void Keyboard::addAlias(const istring &aliasName, Key *key)
{
  aliases.insert(Aliases::value_type(aliasName, key));
}


// add a modifier key
void Keyboard::addModifier(Modifier::Type mt, Key * key)
{
  assert((int)mt < (int)Modifier::BASIC);
  if (find(mods[mt].begin(), mods[mt].end(), key) != mods[mt].end())
    return; // already added
  mods[mt].push_back(key);
}


// search a key
Key *Keyboard::searchKey(const Key &key)
{
  Keys &keys = getKeys(key);
  for (Keys::iterator i = keys.begin(); i != keys.end(); i ++)
    if ((*i).isSameScanCode(key))
      return &*i;
  return NULL;
}


// search a key (of which the key's scan code is the prefix)
Key *Keyboard::searchPrefixKey(const Key &key)
{
  Keys &keys = getKeys(key);
  for (Keys::iterator i = keys.begin(); i != keys.end(); i ++)
    if ((*i).isPrefixScanCode(key))
      return &*i;
  return NULL;
}

  
// search a key by name
Key *Keyboard::searchKey(const istring &name)
{
  Aliases::iterator i = aliases.find(name);
  if (i != aliases.end())
    return (*i).second;
  return searchKeyByNonAliasName(name);
}


// search a key by non-alias name
Key *Keyboard::searchKeyByNonAliasName(const istring &name)
{
  for (int i = 0; i < lengthof_hashedKeys; i++)
  {
    Keys &keys = hashedKeys[i];
    Keys::iterator i = find(keys.begin(), keys.end(), name);
    if (i != keys.end())
      return &*i;
  }
  return NULL;
}
