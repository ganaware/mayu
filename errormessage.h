// ////////////////////////////////////////////////////////////////////////////
// errormessage.h


#ifndef _ERRORMESSAGE_H
#  define _ERRORMESSAGE_H


#  include <strstream>
#  include <string>


///
class ErrorMessage
{
  std::ostrstream m_ost;			///
  
public:
  ///
  ErrorMessage() { }
  ///
  ErrorMessage(const ErrorMessage &i_em) { m_ost << i_em.getMessage(); }

  /// get error message
  std::string getMessage() const
  {
    ErrorMessage &em = *const_cast<ErrorMessage *>(this);
    std::string msg(em.m_ost.str(), em.m_ost.pcount());
    em.m_ost.freeze(false);
    return msg;
  }
  
  /// add message
  template<class T> ErrorMessage &operator<<(const T &i_t)
  {
    m_ost << i_t;
    return *this;
  }

  /// stream output
  friend std::ostream &
  operator<<(std::ostream &i_ost, const ErrorMessage &i_em);
};


/// stream output
inline std::ostream &operator<<(std::ostream &i_ost, const ErrorMessage &i_em)
{ return i_ost << i_em.getMessage(); }


///
class WarningMessage : public ErrorMessage
{
public:
  /// add message
  template<class T> WarningMessage &operator<<(const T &i_t)
  { ErrorMessage::operator<<(i_t); return *this; }
};


#endif // _ERRORMESSAGE_H
