///////////////////////////////////////////////////////////////////////////////
// stringtool.h


#ifndef __stringtool_h__
#define __stringtool_h__


#include "misc.h"

#include <string>

#include <ctype.h>
#include <mbstring.h>


namespace StringTool
{
  using std::string;

#define S_	const char *s
#define SC_	const char *s, char c
#define SN_	const char *s, size_t n
#define SS_	const char *s1, const char *s2
#define SSN_	const char *s1, const char *s2, size_t n
#define SCN_	const char *s, char *c, size_t n
#define _S	(u_char *)s
#define _SC	(u_char *)s, (int)(u_char)c
#define _SN	(u_char *)s, n
#define _SS	(u_char *)s1, (u_char *)s2
#define _SSN	(u_char *)s1, (u_char *)s2, n
#define _SCN	(u_char *)s, (int)(u_char)c, n
  
  // multi byte functions
  
  // ++, --
  inline char * mbsinc_    (S_  ) { return (char *)_mbsinc    (_S  ); }
  inline char * mbsninc_   (SN_ ) { return (char *)_mbsninc   (_SN ); }
  inline char * mbsdec_    (SS_ ) { return (char *)_mbsdec    (_SS ); }
  // compare
  inline int    mbscmp_    (SS_ ) { return         _mbscmp    (_SS ); }
  inline int    mbsncmp_   (SSN_) { return         _mbsncmp   (_SSN); }
  inline int    mbsicmp_   (SS_ ) { return         _mbsicmp   (_SS ); }
  inline int    mbsnicmp_  (SSN_) { return         _mbsnicmp  (_SSN); }
  inline int    mbscoll_   (SS_ ) { return         _mbscoll   (_SS ); }
  inline int    mbsncoll_  (SSN_) { return         _mbsncoll  (_SSN); }
  inline int    mbsicoll_  (SS_ ) { return         _mbsicoll  (_SS ); }
  inline int    mbsnicoll_ (SSN_) { return         _mbsnicoll (_SSN); }
  inline int    mbsnbcmp_  (SSN_) { return         _mbsnbcmp  (_SSN); }
  inline int    mbsnbicmp_ (SSN_) { return         _mbsnbicmp (_SSN); }
  inline bool   mbsiequal_ (SS_ ) { return 0 ==    _mbsicmp   (_SS ); }
  inline bool   mbsniequal_(SSN_) { return 0 ==    _mbsnicmp  (_SSN); }
  // char search
  inline char * mbschr_    (SC_ ) { return (char *)_mbschr    (_SC ); }
  inline char * mbsrchr_   (SC_ ) { return (char *)_mbsrchr   (_SC ); }
  // char set search
  inline size_t mbsspn_    (SS_ ) { return         _mbsspn    (_SS ); }
  inline size_t mbscspn_   (SS_ ) { return         _mbscspn   (_SS ); }
  inline char * mbsspnp_   (SS_ ) { return (char *)_mbsspnp   (_SS ); }
  inline char * mbspbrk_   (SS_ ) { return (char *)_mbspbrk   (_SS ); }
  // string search
  inline char * mbsstr_    (SS_ ) { return (char *)_mbsstr    (_SS ); }
  // concatination
  inline char * mbscat_    (SS_ ) { return (char *)_mbscat    (_SS ); }
  inline char * mbsncat_   (SSN_) { return (char *)_mbsncat   (_SSN); }
  inline char * mbsnbcat_  (SSN_) { return (char *)_mbsnbcat  (_SSN); }
  // copy
  inline char * mbscpy_    (SS_ ) { return (char *)_mbscpy    (_SS ); }
  inline char * mbsncpy_   (SSN_) { return (char *)_mbsncpy   (_SSN); }
  inline char * mbsnbcpy_  (SSN_) { return (char *)_mbsnbcpy  (_SSN); }
  // set
  inline char * mbsset_    (SC_ ) { return (char *)_mbsset    (_SC ); }
  inline char * mbsnset_   (SCN_) { return (char *)_mbsnset   (_SCN); }
  inline char * mbsnbset_  (SCN_) { return (char *)_mbsnbset  (_SCN); }
  // length
  inline size_t mbslen_    (S_  ) { return         _mbslen    (_S  ); }
  //inline size_t mbstrlen_(S_  ) { return         _mbstrlen  (s   ); }
  // convertor
  inline char * mbslwr_    (S_  ) { return (char *)_mbslwr    (_S  ); }
  inline char * mbsupr_    (S_  ) { return (char *)_mbsupr    (_S  ); }
  inline char * mbsrev_    (S_  ) { return (char *)_mbsrev    (_S  ); }
  // count
  inline size_t mbsnccnt_  (SN_ ) { return         _mbsnccnt  (_SN ); }
  inline size_t mbsnbcnt_  (SN_ ) { return         _mbsnbcnt  (_SN ); }
  // next char
  inline int    mbsnextc_  (S_  ) { return         _mbsnextc  (_S  ); }
  // token
  inline char * mbstok_    (SS_ ) { return (char *)_mbstok    (_SS ); }

#undef S_
#undef SC_
#undef SN_
#undef SS_
#undef SSN_
#undef SCN_
#undef _S
#undef _SC
#undef _SN
#undef _SS
#undef _SSN
#undef _SCN

  // 
  inline char *strchr_(const char *s, char c)
  { return strchr((char *)s, (int)(u_char)c); }

  // character test
  //inline bool __isascii(char c) { return !!__isascii  ((u_char)c); }
  //inline bool __iscsym (char c) { return !!__iscsym   ((u_char)c); }
  inline bool isalnum_   (char c) { return !!isalnum    ((u_char)c); }
  inline bool isalpha_	 (char c) { return !!isalpha    ((u_char)c); }
  inline bool iscntrl_	 (char c) { return !!iscntrl    ((u_char)c); }
  inline bool isdigit_	 (char c) { return !!isdigit    ((u_char)c); }
  inline bool isgraph_	 (char c) { return !!isgraph    ((u_char)c); }
  inline bool islower_	 (char c) { return !!islower    ((u_char)c); }
  inline bool isprint_	 (char c) { return !!isprint    ((u_char)c); }
  inline bool ispunct_	 (char c) { return !!ispunct    ((u_char)c); }
  inline bool isspace_	 (char c) { return !!isspace    ((u_char)c); }
  inline bool isupper_	 (char c) { return !!isupper    ((u_char)c); }
  inline bool isxdigit_  (char c) { return !!isxdigit   ((u_char)c); }

  inline bool ismbblead_ (char c) { return !!_ismbblead ((u_char)c); }
  inline bool ismbbtrail_(char c) { return !!_ismbbtrail((u_char)c); }

  inline char toupper_   (char c) { return (char)toupper((u_char)c); }
  inline char tolower_   (char c) { return (char)tolower((u_char)c); }

  
  // interpret meta characters such as \n
  string interpretMetaCharacters(const char *str, size_t len,
				 const char *quote = NULL);
  
  // converter
  std::wstring cast_wstring(const string &text);
  string cast_string(const std::wstring &w_text);

  // case insensitive string
  class istring : public string
  {
  public:
    istring() { }
    istring(const istring &s) : string(s) { }
    istring(const string &s) : string(s) { }
    istring(const char *s) : string(s) { }
    istring(const char *s, size_t n) : string(s, n) { }
    istring(const char *s, size_t pos, size_t n) : string(s, pos, n) { }
    istring(size_t n, char c) : string(n, c) { }
    
    int compare(const istring &i) const { return compare(i.c_str()); }
    int compare(const string &s) const { return compare(s.c_str()); }
    int compare(const char *s) const  { return mbsicmp_(c_str(), s); }
    string &getString() { return *this; }
  };

#define CC_	const char
#define CIS_	const istring
#define CS_	const string
  
  inline bool operator<(CIS_ &i , CC_  *s )  { return i.compare(s) < 0; }
  inline bool operator<(CC_  *s , CIS_ &i )  { return 0 < i.compare(s); }
  inline bool operator<(CIS_ &i , CS_  &s )  { return i.compare(s) < 0; }
  inline bool operator<(CS_  &s , CIS_ &i )  { return 0 < i.compare(s); }
  inline bool operator<(CIS_ &i1, CIS_ &i2)  { return i1.compare(i2) < 0; }

  inline bool operator==(CC_  *s , CIS_ &i )  { return i.compare(s) == 0; }
  inline bool operator==(CIS_ &i , CC_  *s )  { return i.compare(s) == 0; }
  inline bool operator==(CS_  &s , CIS_ &i )  { return i.compare(s) == 0; }
  inline bool operator==(CIS_ &i , CS_  &s )  { return i.compare(s) == 0; }
  inline bool operator==(CIS_ &i1, CIS_ &i2)  { return i1.compare(i2) == 0; }

  // workarounds for Borland C++
  inline bool operator!=(CC_  *s , CIS_ &i )  { return i.compare(s) != 0; }
  inline bool operator!=(CIS_ &i , CC_  *s )  { return i.compare(s) != 0; }
  inline bool operator!=(CS_  &s , CIS_ &i )  { return i.compare(s) != 0; }
  inline bool operator!=(CIS_ &i , CS_  &s )  { return i.compare(s) != 0; }
  inline bool operator!=(CIS_ &i1, CIS_ &i2)  { return i1.compare(i2) != 0; }

#undef CC_
#undef CIS_
#undef CS_
}


#endif // __stringtool_h__
