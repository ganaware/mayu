// ////////////////////////////////////////////////////////////////////////////
// stringtool.cpp


#include "stringtool.h"
#include <locale>
#include <malloc.h>


/// copy 
char *StringTool::mbsfill(char *o_dest,  const char *i_src, size_t i_destSize)
{
  const char *next;
  const char *i;
  for (i = i_src; *i; i = next)
  {
    next = mbsinc_(i);
    if (i_destSize <= next - i_src)
      break;
  }
  memcpy(o_dest, i_src, i - i_src);
  o_dest[i - i_src] = '\0';
  return o_dest;
}


std::string StringTool::interpretMetaCharacters(
  const char *i_str, size_t i_len, const char *i_quote)
{
  char *result = (char *)_alloca(i_len + 1);
  char *d = result;
  const char *end = i_str + i_len;
  
  while (i_str < end && *i_str)
  {
    if (*i_str != '\\')
    {
      if (_ismbblead(*i_str) && *(i_str + 1) && i_str + 1 < end)
	*d++ = *i_str++;
      *d++ = *i_str++;
    }
    else if (*(i_str + 1) != '\0')
    {
      i_str ++;
      if (i_quote && strchr(i_quote, *i_str))
	*d++ = *i_str++;
      else
	switch (*i_str)
	{
	  case 'a': *d++ = '\x07'; i_str ++; break;
	    //case 'b': *d++ = '\b'; i_str ++; break;
	  case 'e': *d++ = '\x1b'; i_str ++; break;
	  case 'f': *d++ = '\f'; i_str ++; break;
	  case 'n': *d++ = '\n'; i_str ++; break;
	  case 'r': *d++ = '\r'; i_str ++; break;
	  case 't': *d++ = '\t'; i_str ++; break;
	    //case 'v': *d++ = '\v'; i_str ++; break;
	    //case '?': *d++ = '\x7f'; i_str ++; break;
	    //case '_': *d++ = ' '; i_str ++; break;
	  case '\\': *d++ = '\\'; i_str ++; break;
	  case '\'': *d++ = '\''; i_str ++; break;
	  case '"': *d++ = '"'; i_str ++; break;
	  case 'c': // control code, for example '\c[' is escape: '\x1b'
	    i_str ++;
	    if (i_str < end && *i_str)
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
	      if (const char *c = strchr(ctrlchar, *i_str))
		*d++ = ctrlcode[c - ctrlchar], i_str ++;
	    }
	    break;
	  case 'x': case 'X':
	  {
	    i_str ++;
	    static const char *hexchar = "0123456789ABCDEFabcdef";
	    static int hexvalue[] = { 0, 1, 2, 3, 4, 5 ,6, 7, 8, 9,
				      10, 11, 12, 13, 14, 15,
				      10, 11, 12, 13, 14, 15, };
	    int n = 0;
	    for (const char *e = min(end, i_str + 2);
		 i_str < e && *i_str; i_str ++)
	      if (const char *c = strchr(hexchar, *i_str))
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
	    for (const char *e = min(end, i_str + 3);
		 i_str < e && *i_str; i_str ++)
	      if (const char *c = strchr(octalchar, *i_str))
		n = n * 8 + octalvalue[c - octalchar];
	      else
		break;
	    if (0 < n)
	      *d++ = (char)n;
	    break;
	  }
	  default:
	    *d++ = '\\';
	    if (_ismbblead(*i_str) && *(i_str + 1) && i_str + 1 < end)
	      *d++ = *i_str++;
	    *d++ = *i_str++;
	    break;
	}
    }
  }
  *d ='\0';
  return result;
}


// converter
std::wstring StringTool::cast_wstring(const std::string &i_text)
{
  size_t size = ::mbstowcs(NULL, i_text.c_str(), i_text.size() + 1);
  if (size == (size_t)-1)
    return std::wstring();
  wchar_t *w_text = (wchar_t *)_alloca((size + 1) * sizeof(wchar_t));
  ::mbstowcs(w_text, i_text.c_str(), i_text.size() + 1);
  return std::wstring(w_text);
}


// converter
std::string StringTool::cast_string(const std::wstring &i_wText)
{
  size_t size = ::wcstombs(NULL, i_wText.c_str(), i_wText.size() + 1);
  if (size == (size_t)-1)
    return std::string();
  char *text = (char *)_alloca((size + 1) * sizeof(char));
  ::wcstombs(text, i_wText.c_str(), i_wText.size() + 1);
  return std::string(text);
}
