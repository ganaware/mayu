// ////////////////////////////////////////////////////////////////////////////
// stringtool.cpp


#include "stringtool.h"
#include <locale>
#include <malloc.h>


std::string StringTool::interpretMetaCharacters(const char *str, size_t len,
						const char *quote)
{
  char *result = (char *)_alloca(len + 1);
  char *d = result;
  const char *end = str + len;
  
  while (str < end && *str)
  {
    if (*str != '\\')
    {
      if (_ismbblead(*str) && *(str + 1) && str + 1 < end)
	*d++ = *str++;
      *d++ = *str++;
    }
    else if (*(str + 1) != '\0')
    {
      str ++;
      if (quote && strchr(quote, *str))
	*d++ = *str++;
      else
	switch (*str)
	{
	  case 'a': *d++ = '\x07'; str ++; break;
	    //case 'b': *d++ = '\b'; str ++; break;
	  case 'e': *d++ = '\x1b'; str ++; break;
	  case 'f': *d++ = '\f'; str ++; break;
	  case 'n': *d++ = '\n'; str ++; break;
	  case 'r': *d++ = '\r'; str ++; break;
	  case 't': *d++ = '\t'; str ++; break;
	    //case 'v': *d++ = '\v'; str ++; break;
	    //case '?': *d++ = '\x7f'; str ++; break;
	    //case '_': *d++ = ' '; str ++; break;
	  case '\\': *d++ = '\\'; str ++; break;
	  case '\'': *d++ = '\''; str ++; break;
	  case '"': *d++ = '"'; str ++; break;
	  case 'c': // control code, for example '\c[' is escape: '\x1b'
	    str ++;
	    if (str < end && *str)
	    {
	      static const char *ctrlchar =
		"@ABCDEFGHIJKLMNO"
		"PQRSTUVWXYZ[\\]^_"
		"@abcdefghijklmno"
		"pqrstuvwxyz@@@@?";
	      static const char *ctrlcode =
		"\00\01\02\03\04\05\06\07\10\11\12\13\14\15\16\17"
		"\20\21\22\23\24\25\26\27\30\31\32\33\34\35\36\37"
		"\00\01\02\03\04\05\06\07\10\11\12\13\14\15\16\17"
		"\20\21\22\23\24\25\26\27\30\31\32\00\00\00\00\177";
	      if (const char *c = strchr(ctrlchar, *str))
		*d++ = ctrlcode[c - ctrlchar], str ++;
	    }
	    break;
	  case 'x': case 'X':
	  {
	    str ++;
	    static const char *hexchar = "0123456789ABCDEFabcdef";
	    static int hexvalue[] = { 0, 1, 2, 3, 4, 5 ,6, 7, 8, 9,
				      10, 11, 12, 13, 14, 15,
				      10, 11, 12, 13, 14, 15, };
	    int n = 0;
	    for (const char *e = min(end, str + 2);
		 str < e && *str; str ++)
	      if (const char *c = strchr(hexchar, *str))
		n = n * 16 + hexvalue[c - hexchar];
	      else
		break;
	    if (0 < n)
	      *d++ = (char)n;
	    break;
	  }
	  case '0': case '1': case '2': case '3':
	  case '4': case '5': case '6': case '7':
	  {
	    static const char *octalchar = "01234567";
	    static int octalvalue[] = { 0, 1, 2, 3, 4, 5 ,6, 7, };
	    int n = 0;
	    for (const char *e = min(end, str + 3);
		 str < e && *str; str ++)
	      if (const char *c = strchr(octalchar, *str))
		n = n * 8 + octalvalue[c - octalchar];
	      else
		break;
	    if (0 < n)
	      *d++ = (char)n;
	    break;
	  }
	  default:
	    *d++ = '\\';
	    if (_ismbblead(*str) && *(str + 1) && str + 1 < end)
	      *d++ = *str++;
	    *d++ = *str++;
	    break;
	}
    }
  }
  *d ='\0';
  return result;
}


// converter
std::wstring StringTool::cast_wstring(const std::string &text)
{
  size_t size = ::mbstowcs(NULL, text.c_str(), text.size() + 1);
  if (size == (size_t)-1)
    return std::wstring();
  wchar_t *w_text = (wchar_t *)_alloca((size + 1) * sizeof(wchar_t));
  ::mbstowcs(w_text, text.c_str(), text.size() + 1);
  return std::wstring(w_text);
}


// converter
std::string StringTool::cast_string(const std::wstring &w_text)
{
  size_t size = ::wcstombs(NULL, w_text.c_str(), w_text.size() + 1);
  if (size == (size_t)-1)
    return std::string();
  char *text = (char *)_alloca((size + 1) * sizeof(char));
  ::wcstombs(text, w_text.c_str(), w_text.size() + 1);
  return std::string(text);
}
