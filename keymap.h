//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// keymap.h


#ifndef _KEYMAP_H
#  define _KEYMAP_H

#  include "keyboard.h"
#  include "function.h"
#  include <vector>


///
class Action
{
public:
  ///
  enum Type
  {
    Type_key,					///
    Type_keySeq,				///
    Type_function,				///
  };

public:
  ///
  const Type m_type;

public:
  ///
  Action(Type i_type) : m_type(i_type) { }
};


///
class ActionKey : public Action
{
public:
  ///
  const ModifiedKey m_modifiedKey;

public:
  ///
  ActionKey(const ModifiedKey &i_mk)
    : Action(Action::Type_key), m_modifiedKey(i_mk) { }
};


class KeySeq;
///
class ActionKeySeq : public Action
{
public:
  KeySeq * const m_keySeq;			///

public:
  ///
  ActionKeySeq(KeySeq *i_keySeq)
    : Action(Action::Type_keySeq), m_keySeq(i_keySeq) { }
};


///
class ActionFunction : public Action
{
public:
  Modifier m_modifier;				/// modifier for &Sync
  FunctionData *m_functionData;			/// function data

public:
  ///
  ActionFunction() : Action(Action::Type_function), m_functionData(NULL) { }
};


///
class KeySeq
{
public:
  typedef std::vector<Action *> Actions;	/// 

private:
  Actions m_actions;				///
  tstringi m_name;				/// 

private:
  ///
  void copy();
  ///
  void clear();
  
public:
  ///
  KeySeq(const tstringi &i_name);
  ///
  KeySeq(const KeySeq &i_ks);
  ///
  ~KeySeq();
  
  ///
  const Actions &getActions() const { return m_actions; }
  
  ///
  KeySeq &operator=(const KeySeq &i_ks);
  
  /// add
  KeySeq &add(const ActionKey &i_ak);
  ///
  KeySeq &add(const ActionKeySeq &i_aks);
  ///
  KeySeq &add(const ActionFunction &i_af);

  ///
  const tstringi &getName() const { return m_name; }
  
  /// stream output
  friend tostream &operator<<(tostream &i_ost, const KeySeq &i_ks);
};


///
class Keymap
{
public:
  ///
  enum Type
  {
    Type_keymap,				/// this is keymap
    Type_windowAnd,				/// this is window &amp;&amp;
    Type_windowOr,				/// this is window ||
  };
  ///
  enum AssignOperator
  {
    AO_new,					/// =
    AO_add,					/// +=
    AO_sub,					/// -=
    AO_overwrite,				/// !, !!
  };
  ///
  enum AssignMode
  {
    AM_notModifier,				///    not modifier
    AM_normal,					///    normal modifier
    AM_true,					/** !  true modifier(doesn't
                                                    generate scan code) */
    AM_oneShot,					/// !! one shot modifier
  };
  
  /// key assignment
  class KeyAssignment
  {
  public:
    ModifiedKey m_modifiedKey;	///
    KeySeq *m_keySeq;		///

  public:
    ///
    KeyAssignment(const ModifiedKey &i_modifiedKey, KeySeq *i_keySeq)
      : m_modifiedKey(i_modifiedKey), m_keySeq(i_keySeq) { }
    ///
    KeyAssignment(const KeyAssignment &i_o)
      : m_modifiedKey(i_o.m_modifiedKey), m_keySeq(i_o.m_keySeq) { }
    ///
    bool operator<(const KeyAssignment &i_o) const
    { return m_modifiedKey < i_o.m_modifiedKey; }
  };

  /// modifier assignments
  class ModAssignment
  {
  public:
    AssignOperator m_assignOperator;	///
    AssignMode m_assignMode;		///
    Key *m_key;				///
  };
  typedef std::list<ModAssignment> ModAssignments; ///

  typedef std::list<ModifiedKey> DescribedKeys;	/// 
  
private:
  /// key assignments (hashed by first scan code)
  typedef std::list<KeyAssignment> KeyAssignments;
  enum {
    HASHED_KEY_ASSIGNMENT_SIZE = 32,	///
  };

private:
  KeyAssignments m_hashedKeyAssignments[HASHED_KEY_ASSIGNMENT_SIZE];	///
  
  /// modifier assignments
  ModAssignments m_modAssignments[Modifier::Type_ASSIGN];

  Type m_type;					/// type
  tstringi m_name;				/// keymap name
  tregex m_windowClass;				/// window class name regexp
  tregex m_windowTitle;				/// window title name regexp

  KeySeq *m_defaultKeySeq;			/// default keySeq
  Keymap *m_parentKeymap;			/// parent keymap
  
private:
  ///
  KeyAssignments &getKeyAssignments(const ModifiedKey &i_mk);
  ///
  const KeyAssignments &getKeyAssignments(const ModifiedKey &i_mk) const;
  
public:
  ///
  Keymap(Type i_type,
	 const tstringi &i_name,
	 const tstringi &i_windowClass,
	 const tstringi &i_windowTitle,
	 KeySeq *i_defaultKeySeq,
	 Keymap *i_parentKeymap);
  
  /// add a key assignment;
  void addAssignment(const ModifiedKey &i_mk, KeySeq *i_keySeq);

  /// add modifier
  void addModifier(Modifier::Type i_mt, AssignOperator i_ao,
		   AssignMode i_am, Key *i_key);
  
  /// search
  const KeyAssignment *searchAssignment(const ModifiedKey &i_mk) const;

  /// get
  const KeySeq *getDefaultKeySeq() const { return m_defaultKeySeq; }
  ///
  Keymap *getParentKeymap() const { return m_parentKeymap; }
  ///
  const tstringi &getName() const { return m_name; }

  /// does same window
  bool doesSameWindow(const tstringi i_className,
		      const tstringi &i_titleName);
  
  /// adjust modifier
  void adjustModifier(Keyboard &i_keyboard);

  /// get modAssignments
  const ModAssignments &getModAssignments(Modifier::Type i_mt) const
  { return m_modAssignments[i_mt]; }

  /// describe
  void describe(tostream &i_ost, DescribedKeys *o_mk) const;
};


///
class Keymaps
{
public:
  typedef std::list<Keymap *> KeymapPtrList;	/// 
  
private:
  typedef std::list<Keymap> KeymapList;		/// 

private:
  KeymapList m_keymapList;			/** pointer into keymaps may
                                                    exist */
  
public:
  ///
  Keymaps();
  
  /// search by name
  Keymap *searchByName(const tstringi &i_name);

  /// search window
  void searchWindow(KeymapPtrList *o_keymapPtrList,
		    const tstringi &i_className,
		    const tstringi &i_titleName);
  
  /// add keymap
  Keymap *add(const Keymap &i_keymap);
  
  /// adjust modifier
  void adjustModifier(Keyboard &i_keyboard);
};


///
class KeySeqs
{
private:
  typedef std::list<KeySeq> KeySeqList;		///

private:
  KeySeqList m_keySeqList;			///
  
public:
  /// add a named keyseq (name can be empty)
  KeySeq *add(const KeySeq &i_keySeq);
  
  /// search by name
  KeySeq *searchByName(const tstringi &i_name);
};


#endif // !_KEYMAP_H
