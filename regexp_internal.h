///////////////////////////////////////////////////////////////////////////////
// regexp_internal.h


#ifdef REGEXP_SPEC

#include <string>

class RegexpSpec
{
public:
  typedef char char_t;
  typedef std::string string_t;
};

#else // if (!defined(REGEXP_SPEC))


# include "stringtool.h"

# define R_char		RegexpSpec::char_t
# define R_string	RegexpSpec::string_t
  
# define R_(x)		x
# define R_char_0	'\0'
# define sizeof_R_char	1
  
# define R_ismbblead(c)	StringTool::ismbblead_(c)
# define R_isalpha(c)	(!((c) & 0x80) && StringTool::isalpha_(c))//[A-Za-z]
# define R_isdigit(c)	(!((c) & 0x80) && StringTool::isdigit_(c))//[0-9]
# define R_isalnum(c)	(!((c) & 0x80) && StringTool::isalnum_(c))//[A-Za-z0-9]
# define R_isspace(c)	(!((c) & 0x80) && StringTool::isspace_(c))//[\t\v\f\t ]
# define R_isword(c)	(R_isalnum(c) || (c) == '_')
# define R_strinc	StringTool::mbsinc_
# define R_strdec	StringTool::mbsdec_
# define R_strchr	StringTool::mbschr_
# undef  R_strmchr			// strchr whose 2nd arg is multibyte 
# define R_strstr	StringTool::mbsstr_
# define R_strpbrk	StringTool::mbspbrk_
# define R_strspnp	StringTool::mbsspnp_
# define R_strlen	strlen		// count multibyte char as 2 chars
# define R_strncmp	strncmp		// count multibyte char as 2 chars
# define R_strncpy	strncpy		// count multibyte char as 2 chars
# undef  R_strrange
# define R_tolower	StringTool::tolower_
# define R_toupper	StringTool::toupper_


#endif // REGEXP_SPEC
