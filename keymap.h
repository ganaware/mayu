// ////////////////////////////////////////////////////////////////////////////
// keymap.h


#ifndef __keymap_h__
#define __keymap_h__


#include "keyboard.h"
#include "regexp.h"
#include "function.h"


#include <vector>


///
class Action
{
public:
  ///
  enum Type {
    ActKey,		///
    ActKeySeq,		///
    ActFunction,	///
  };
  ///
  const Type type;
  ///
  Action(Type type_) : type(type_) { }
};


///
class ActionKey : public Action
{
public:
  ///
  const ModifiedKey modifiedKey;
  ///
  ActionKey(const ModifiedKey &mk_)
    : Action(Action::ActKey), modifiedKey(mk_) { }
};


class KeySeq;
///
class ActionKeySeq : public Action
{
public:
  KeySeq * const keySeq;		///
  ///
  ActionKeySeq(KeySeq *keySeq_) : Action(Action::ActKeySeq), keySeq(keySeq_) {}
};


///
class ActionFunction : public Action
{
public:
  Modifier modifier;		/// modifier for &Sync
  Function const *function;	/// function
  typedef std::vector<Token> Args;
  Args args;	/// arguments
  ///
  ActionFunction() : Action(Action::ActFunction), function(NULL) { }
};


///
class KeySeq
{
  std::vector<Action *> actions;		///
  istring m_name;				/// 
  
  ///
  void copy();
  ///
  void clear();
  
public:
  ///
  KeySeq(const istring &i_name);
  ///
  KeySeq(const KeySeq &ks);
  ///
  ~KeySeq();
  
  ///
  const std::vector<Action *> &getActions() const { return actions; }
  
  ///
  KeySeq &operator=(const KeySeq &ks);
  
  /// add
  KeySeq &add(const ActionKey &ak);
  ///
  KeySeq &add(const ActionKeySeq &aks);
  ///
  KeySeq &add(const ActionFunction &af);

  ///
  const istring &getName() const { return m_name; }
  
  /// stream output
  friend std::ostream &
  operator<<(std::ostream &i_ost, const KeySeq &i_ks);
};


///
class Keymap
{
public:
  ///
  enum Type
  {
    TypeKeymap,		/// this is keymap
    TypeWindowAnd,	/// this is window &amp;&amp;
    TypeWindowOr,	/// this is window ||
  };
  ///
  enum AssignOperator
  {
    AOnew,		/// =
    AOadd,		/// +=
    AOsub,		/// -=
    AOoverwrite,	/// !, !!
  };
  ///
  enum AssignMode
  {
    AMnotModifier,	///    not modifier
    AMnormal,		///    normal modifier
    AMtrue,		/// !  true modifier(doesn't generate scan code)
    AMoneShot,		/// !! one shot modifier
  };
  
  /// key assignment
  class KeyAssignment
  {
  public:
    ModifiedKey modifiedKey;	///
    KeySeq *keySeq;		///
    ///
    KeyAssignment(const ModifiedKey &mk, KeySeq *keySeq_)
      : modifiedKey(mk), keySeq(keySeq_) { }
    ///
    KeyAssignment(const KeyAssignment &i_o)
      : modifiedKey(i_o.modifiedKey), keySeq(i_o.keySeq) { }

    bool operator<(const KeyAssignment &i_o) const
    { return modifiedKey < i_o.modifiedKey; }
  };

  /// modifier assignments
  class ModAssignment
  {
  public:
    AssignOperator assignOperator;	///
    AssignMode assignMode;		///
    Key *key;				///
  };
  typedef std::list<ModAssignment> ModAssignments;	///

private:
  /// key assignments (hashed by first scan code)
  typedef std::list<KeyAssignment> KeyAssignments;
  /** @name ANONYMOUS */
  enum {
    lengthof_hashedKeyAssignment = 32,	///
  };
  KeyAssignments hashedKeyAssignments[lengthof_hashedKeyAssignment];	///
  
  // lock assignments
//    class LockAssignment
//    {
//    public:
//      AssignOperator assignOperator;
//      AssignMode assignMode;
//      ModifiedKey mkey;
//    };
//    std::list<LockAssignment> lockAssignments;
  
  ModAssignments modAssignments[Modifier::ASSIGN]; /// modifier assignments

  Type type;			/// type
  istring name;			/// keymap name
  Regexp windowClass;		/// window class name regexp
  Regexp windowTitle;		/// window title name regexp

  KeySeq *defaultKeySeq;	/// default keySeq
  Keymap *parentKeymap;		/// parent keymap
  
private:
  ///
  KeyAssignments &getKeyAssignments(const ModifiedKey &mk);
  
public:
  ///
  Keymap(Type type_,
	 const istring &name_,
	 const istring &windowClass_,
	 const istring &windowTitle_,
	 KeySeq *defaultKeySeq_,
	 Keymap *parentKeymap_);
  
  /// add a key assignment;
  void addAssignment(const ModifiedKey &mk, KeySeq *keySeq);

  /// add modifier
  void addModifier(Modifier::Type mt, AssignOperator ao,
		   AssignMode am, Key *key);
  
  /// search
  const KeyAssignment *searchAssignment(const ModifiedKey &mk);

  /// get
  const KeySeq *getDefaultKeySeq() const { return defaultKeySeq; }
  ///
  Keymap *getParentKeymap() const { return parentKeymap; }
  ///
  const istring &getName() const { return name; }

  /// does same window
  bool doesSameWindow(const istring className, const istring &titleName);
  
  /// adjust modifier
  void adjustModifier(Keyboard &keyboard);

  /// get modAssignments
  const ModAssignments &getModAssignments(Modifier::Type mt) const
  { return modAssignments[mt]; }

  /// describe
  typedef std::list<ModifiedKey> DescribedKeys;
  void describe(std::ostream &i_ost, DescribedKeys *o_mk) const;
};


///
class Keymaps
{
  std::list<Keymap> keymaps;		/// pointer into keymaps may exist
  
public:
  ///
  Keymaps();
  
  /// search by name
  Keymap *searchByName(const istring &name);

  /// search window
  void searchWindow(std::list<Keymap *> *keymaps_r,
		    const istring className,
		    const istring &titleName);
  
  /// add keymap
  Keymap *add(const Keymap &keymap);
  
  /// adjust modifier
  void adjustModifier(Keyboard &keyboard);
};


///
class KeySeqs
{
#if 0
  ///
  class NamedKeySeq
  {
  public:
    const istring name;		///
    KeySeq keySeq;		///
    ///
    NamedKeySeq(const istring &name_, const KeySeq &keySeq_)
      : name(name_), keySeq(keySeq_) { }
  };
  typedef std::list<NamedKeySeq> NamedKeySeqs;	///
  NamedKeySeqs namedKeySeqs;			///
#endif
  typedef std::list<KeySeq> KeySeqList;		///
  KeySeqList keySeqList;			///
  
public:
  /// add a named keyseq (name can be empty)
  KeySeq *add(const KeySeq &keySeq);
  
  /// search by name
  KeySeq *searchByName(const istring &name);
};


#endif // __keymap_h__
