// ////////////////////////////////////////////////////////////////////////////
// errormessage.h


#ifndef __errormessage_h__
#define __errormessage_h__


#include <strstream>
#include <string>


///
class ErrorMessage
{
  std::ostrstream ost;		///
  
public:
  ///
  ErrorMessage() { }
  ///
  ErrorMessage(const ErrorMessage &em) { ost << em.getMessage(); }

  /// get error message
  std::string getMessage() const
  {
    ErrorMessage &em = *const_cast<ErrorMessage *>(this);
    std::string msg(em.ost.str(), em.ost.pcount());
    em.ost.freeze(false);
    return msg;
  }
  
  /// add message
  template<class T> ErrorMessage &operator<<(const T &t)
  { ost << t; return *this; }
  

  /// stream output
  friend std::ostream &operator<<(std::ostream &ost, const ErrorMessage &em);
};


/// stream output
inline std::ostream &operator<<(std::ostream &ost, const ErrorMessage &em)
{ return ost << em.getMessage(); }


///
class WarningMessage : public ErrorMessage
{
public:
  /// add message
  template<class T> WarningMessage &operator<<(const T &t)
  { ErrorMessage::operator<<(t); return *this; }
};


#endif // __errormessage_h__

