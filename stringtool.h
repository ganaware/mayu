// ////////////////////////////////////////////////////////////////////////////
// stringtool.h


#ifndef _STRINGTOOL_H
#  define _STRINGTOOL_H

#  include "misc.h"
#  include <tchar.h>
#  include <cctype>
#  include <string>
#  include <iosfwd>
#  include <boost/regex.hpp>


typedef std::basic_string<_TCHAR> tstring;
typedef std::basic_istream<_TCHAR> tistream;
typedef std::basic_ostream<_TCHAR> tostream;
typedef std::basic_streambuf<_TCHAR> tstreambuf;
typedef std::basic_stringstream<_TCHAR> tstringstream;
typedef std::basic_ifstream<_TCHAR> tifstream;
typedef boost::reg_expression<_TCHAR> tregex;
typedef boost::match_results<const _TCHAR *> tcmatch;


class tcmatch_results : public tcmatch
{
public:
  tstring str(tcmatch::size_type i_n) const
  {
    return static_cast<tstring>(
      static_cast<const tcmatch *>(this)->operator[](i_n));
  }
};



/// interpret meta characters such as \n
tstring interpretMetaCharacters(const _TCHAR *i_str, size_t i_len,
				const _TCHAR *i_quote = NULL,
				bool i_doesUseRegexpBackReference = false);

/// copy
size_t strlcpy(char *o_dest, const char *i_src, size_t i_destSize);
/// copy
size_t mbslcpy(unsigned char *o_dest, const unsigned char *i_src,
		       size_t i_destSize);
/// copy
size_t wcslcpy(wchar_t *o_dest, const wchar_t *i_src, size_t i_destSize);
/// copy
inline size_t tcslcpy(char *o_dest, const char *i_src, size_t i_destSize)
{ return strlcpy(o_dest, i_src, i_destSize); }
/// copy
inline size_t tcslcpy(unsigned char *o_dest, const unsigned char *i_src,
		      size_t i_destSize)
{ return mbslcpy(o_dest, i_src, i_destSize); }
/// copy
inline size_t tcslcpy(wchar_t *o_dest, const wchar_t *i_src, size_t i_destSize)
{ return wcslcpy(o_dest, i_src, i_destSize); }

/// converter
std::wstring to_wstring(const std::string &i_str);
/// converter
std::string to_string(const std::wstring &i_str);

  
/// case insensitive string
class tstringi : public tstring
{
public:
  ///
  tstringi() { }
  ///
  tstringi(const tstringi &i_str) : tstring(i_str) { }
  ///
  tstringi(const tstring &i_str) : tstring(i_str) { }
  ///
  tstringi(const TCHAR *i_str) : tstring(i_str) { }
  ///
  tstringi(const TCHAR *i_str, size_t i_n) : tstring(i_str, i_n) { }
  ///
  tstringi(const TCHAR *i_str, size_t i_pos, size_t i_n)
    : tstring(i_str, i_pos, i_n) { }
  ///
  tstringi(size_t i_n, TCHAR i_c) : tstring(i_n, i_c) { }
  ///
  int compare(const tstringi &i_str) const
  { return compare(i_str.c_str()); }
  ///
  int compare(const tstring &i_str) const
  { return compare(i_str.c_str()); }
  ///
  int compare(const TCHAR *i_str) const
  { return _tcsicmp(c_str(), i_str); }
  ///
  tstring &getString() { return *this; }
  ///
  const tstring &getString() const { return *this; }
};

///
inline bool operator<(const tstringi &i_wsi, const TCHAR *i_s)
{ return i_wsi.compare(i_s) < 0; }
///
inline bool operator<(const TCHAR *i_s, const tstringi &i_wsi)
{ return 0 < i_wsi.compare(i_s); }
///
inline bool operator<(const tstringi &i_wsi, const tstring &i_s)
{ return i_wsi.compare(i_s) < 0; }
///
inline bool operator<(const tstring &i_s, const tstringi &i_wsi)
{ return 0 < i_wsi.compare(i_s); }
///
inline bool operator<(const tstringi &i_wsi1, const tstringi &i_wsi2)
{ return i_wsi1.compare(i_wsi2) < 0; }

///
inline bool operator==(const TCHAR *i_s, const tstringi &i_wsi)
{ return i_wsi.compare(i_s) == 0; }
///
inline bool operator==(const tstringi &i_wsi, const TCHAR *i_s)
{ return i_wsi.compare(i_s) == 0; }
///
inline bool operator==(const tstring &i_s, const tstringi &i_wsi)
{ return i_wsi.compare(i_s) == 0; }
///
inline bool operator==(const tstringi &i_wsi, const tstring &i_s)
{ return i_wsi.compare(i_s) == 0; }
///
inline bool operator==(const tstringi &i_wsi1, const tstringi &i_wsi2)
{ return i_wsi1.compare(i_wsi2) == 0; }
  
// workarounds for Borland C++
///
inline bool operator!=(const TCHAR *i_s, const tstringi &i_wsi)
{ return i_wsi.compare(i_s) != 0; }
///
inline bool operator!=(const tstringi &i_wsi, const TCHAR *i_s)
{ return i_wsi.compare(i_s) != 0; }
///
inline bool operator!=(const tstring &i_s, const tstringi &i_wsi)
{ return i_wsi.compare(i_s) != 0; }
///
inline bool operator!=(const tstringi &i_wsi, const tstring &i_s)
{ return i_wsi.compare(i_s) != 0; }
///
inline bool operator!=(const tstringi &i_wsi1, const tstringi &i_wsi2)
{ return i_wsi1.compare(i_wsi2) != 0; }


#endif // _STRINGTOOL_H
