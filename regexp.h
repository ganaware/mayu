// ////////////////////////////////////////////////////////////////////////////
// regexp.h


#ifndef __regexp_h__
#define __regexp_h__


#define REGEXP_SPEC
#include "regexp_internal.h"
#undef REGEXP_SPEC


/// regexp utility
namespace RegexpUtil
{
  ///
  template <class T> class AutoDeletePtr
  {
  public:
    T *ptr;		///
  public:
    ///
    AutoDeletePtr(T *ptr_ = NULL) : ptr(ptr_) { }
    /// auto delete
    ~AutoDeletePtr() { delete ptr; }
    ///
    void free() { delete ptr; ptr = NULL; }
    ///
    operator T *() const { return ptr; }
    ///
    operator void *() const { return (void *)ptr; }
    ///
    operator bool() const { return !!ptr; }
    ///
    bool operator !() const { return !ptr; }
    ///
    bool operator ==(T *p) { return ptr == p; }
    ///
    size_t operator -(T *p) const { return ptr - p; }
    ///
    T &operator *() const { return *ptr; }
    ///
    T *operator =(T *p) { return ptr = p; }
    ///
    T *operator ->() const { return ptr; }
    ///
    template<class N> T *operator +(N size) const { return ptr + size; }
    ///
    template<class N> T *operator -(N size) const { return ptr - size; }
  };

  ///
  template <class T> size_t operator-(T *p, const AutoDeletePtr<T> &ptr)
  {
    return p - ptr.ptr;
  }

  ///
  template <class T> class AutoDeleteArrayPtr
  {
  public:
    T *ptr;		///
  public:
    ///
    AutoDeleteArrayPtr(T *ptr_ = NULL) : ptr(ptr_) { }
    /// auto delete
    ~AutoDeleteArrayPtr() { delete [] ptr; }
    ///
    void free() { delete [] ptr; ptr = NULL; }
    ///
    operator T *() const { return ptr; }
    ///
    operator void *() const { return (void *)ptr; }
    ///
    operator bool() const { return !!ptr; }
    ///
    bool operator !() const { return !ptr; }
    ///
    bool operator ==(T *p) { return ptr == p; }
    ///
    size_t operator -(T *p) const { return ptr - p; }
    ///
    T &operator *() const { return *ptr; }
    ///
    T *operator =(T *p) { return ptr = p; }
    ///
    template<class N> T *operator +(N size) const { return ptr + size; }
    ///
    template<class N> T *operator -(N size) const { return ptr - size; }
    ///
    template<class N> T &operator[](N index) { return ptr[index]; }
    ///
    template<class N> const T &operator[](N index) const { return ptr[index]; }
  };

  ///
  template <class T> size_t operator-(T *p, const AutoDeleteArrayPtr<T> &ptr)
  {
    return p - ptr.ptr;
  }
}


/** Note (see "regexp.html" about this library).
We should retrieve matched string (\1 \2 etc...) as soon as possible.
<pre>
Regexp re(L"e.o");
{
  MChar *text = L"hogehogetext";
  re.doesMatch(text);
  printf("%S\n", re[0].c_str());
}
printf("%S\n", re[0].c_str());
</pre>
The first printf prints "eho", but
I dont know what is printed by the second.
*/
class Regexp
{
public:
  class regexp;

  /** @name ANONYMOUS */
  enum {
    NSUBEXP = 32,		///
  };
    
  /// exception
  class InvalidRegexp : public std::invalid_argument
  {
  public:
    ///
    InvalidRegexp(const std::string &what) : std::invalid_argument(what) { }
  };
    
private:
  /// used to return substring offset only
  const RegexpSpec::char_t *target;
  ///
  RegexpUtil::AutoDeletePtr<regexp> rc;

public:
  /// constructor (MT-unsafe)
  Regexp();
  ///
  Regexp(const Regexp &o);
  ///
  Regexp(const RegexpSpec::char_t *exp, bool doesIgnoreCase = false)
    throw (InvalidRegexp);
  ///
  Regexp(const RegexpSpec::string_t &exp, bool doesIgnoreCase = false)
    throw (InvalidRegexp);

  /// destructor
  ~Regexp();

  /// compile
  void compile(const RegexpSpec::char_t *exp, bool doesIgnoreCase = false)
    throw (InvalidRegexp);
  ///
  void compile(const RegexpSpec::string_t &exp, bool doesIgnoreCase = false)
    throw (InvalidRegexp)
  { compile(exp.c_str(), doesIgnoreCase); }
    
  /// operator= (MT-unsafe)
  Regexp &operator=(const Regexp &o);
    
  /// does match ? (MT-unsafe)
  bool doesMatch(const RegexpSpec::char_t *target_) throw (InvalidRegexp);
  ///
  bool doesMatch(const RegexpSpec::string_t &target_) throw (InvalidRegexp)
  { return doesMatch(target_.c_str()); }
    
  /// number of substirngs (MT-unsafe)
  size_t getNumberOfSubStrings() const;
    
  /// get substrings (MT-unsafe)
  RegexpSpec::string_t operator[](size_t i) const throw (std::out_of_range);
  ///
  size_t subBegin(size_t i) const throw (std::out_of_range);
  ///
  size_t subEnd(size_t i) const throw (std::out_of_range);
  ///
  size_t subSize(size_t i) const throw (std::out_of_range);

  /// replace \1 \2 ... in source (MT-unsafe)
  RegexpSpec::string_t replace(const RegexpSpec::char_t *source) const;
  ///
  RegexpSpec::string_t replace(const RegexpSpec::string_t &source) const
  { return replace(source.c_str()); }
};


#endif // __regexp_h__
