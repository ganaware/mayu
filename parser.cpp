// ////////////////////////////////////////////////////////////////////////////
// parser.cpp


#include "misc.h"

#include "errormessage.h"
#include "parser.h"
#include <cassert>


// ////////////////////////////////////////////////////////////////////////////
// Token


Token::Token(const Token &i_token)
  : m_type(i_token.m_type),
    m_isValueQuoted(i_token.m_isValueQuoted),
    m_numericValue(i_token.m_numericValue),
    m_stringValue(i_token.m_stringValue),
    m_data(i_token.m_data)
{
}

Token::Token(int i_value, const tstringi &i_display)
  : m_type(Type_number),
    m_isValueQuoted(false),
    m_numericValue(i_value),
    m_stringValue(i_display),
    m_data(NULL)
{
}

Token::Token(const tstringi &i_value, bool i_isValueQuoted, bool i_isRegexp)
  : m_type(i_isRegexp ? Type_regexp : Type_string),
    m_isValueQuoted(i_isValueQuoted),
    m_numericValue(0),
    m_stringValue(i_value),
    m_data(NULL)
{
}

Token::Token(Type i_m_type)
  : m_type(i_m_type),
    m_isValueQuoted(false),
    m_numericValue(0),
    m_stringValue(_T("")),
    m_data(NULL)
{
  ASSERT(m_type == Type_openParen || m_type == Type_closeParen);
}

// get numeric value
int Token::getNumber() const
{
  if (m_type == Type_number)
    return m_numericValue;
  if (m_stringValue.empty())
    return 0;
  else
    throw ErrorMessage() << _T("`") << *this << _T("' is not a Type_number.");
}

// get string value
tstringi Token::getString() const
{
  if (m_type == Type_string)
    return m_stringValue;
  throw ErrorMessage() << _T("`") << *this << _T("' is not a string.");
}

// get regexp value
tstringi Token::getRegexp() const
{
  if (m_type == Type_regexp)
    return m_stringValue;
  throw ErrorMessage() << _T("`") << *this << _T("' is not a regexp.");
}

// case insensitive equal
bool Token::operator==(const _TCHAR *i_str) const
{
  if (m_type == Type_string)
    return m_stringValue == i_str;
  return false;
}

// paren equal
bool Token::operator==(const _TCHAR i_c) const
{
  if (i_c == _T('(')) return m_type == Type_openParen;
  if (i_c == _T(')')) return m_type == Type_openParen;
  return false;
}

// stream output
tostream &operator<<(tostream &i_ost, const Token &i_token)
{
  switch (i_token.m_type)
  {
    case Token::Type_string: i_ost << i_token.m_stringValue; break;
    case Token::Type_number: i_ost << i_token.m_stringValue; break;
    case Token::Type_regexp: i_ost << i_token.m_stringValue; break;
    case Token::Type_openParen: i_ost << _T("("); break;
    case Token::Type_closeParen: i_ost << _T(")"); break;
  }
  return i_ost;
}


// ////////////////////////////////////////////////////////////////////////////
// Parser


Parser::Parser(tistream &i_ist)
  : m_lineNumber(1),
    m_prefixes(NULL),
    m_internalLineNumber(1),
    m_ist(i_ist)
{
}

// set string that may be prefix of a token.
// prefix_ is not copied, so it must be preserved after setPrefix()
void Parser::setPrefixes(const Prefixes *i_prefixes)
{
  m_prefixes = i_prefixes;
}

// get a line
bool Parser::getLine(tstringi *o_line)
{
  o_line->resize(0);
  while (true)
  {
    _TCHAR linebuf[1024] = _T("");
    m_ist.get(linebuf, NUMBER_OF(linebuf), _T('\n'));
    if (m_ist.eof())
      break;
    m_ist.clear();
    *o_line += linebuf;
    _TCHAR c;
    m_ist.get(c);
    if (m_ist.eof())
      break;
    if (c == _T('\n'))
    {
      m_internalLineNumber ++;
      break;
    }
    *o_line += c;
  }
  
  return !(o_line->empty() && m_ist.eof());
}

// symbol test
static bool isSymbolChar(_TCHAR i_c)
{
  if (i_c == _T('\0'))
    return false;
  if (_istlead(i_c) ||
      _istalpha(i_c) ||
      _istdigit(i_c) ||
      _istlead(i_c))
    return true;

#ifdef UNICODE
  if (0x80 <= i_c && _istgraph(i_c))
    return true;
#endif // UNICODE

  if (_istpunct(i_c))
    return !!_tcschr(_T("-+/?_\\"), i_c);

  return _istgraph(i_c);
}


// get a parsed line.
// if no more lines exist, returns false
bool Parser::getLine(std::vector<Token> *o_tokens)
{
  o_tokens->clear();
  m_lineNumber = m_internalLineNumber;

  tstringi line;
  continue_getLineLoop:
  while (getLine(&line))
  {
    const _TCHAR *t = line.c_str();
    bool isTokenExist = false;

    continue_getTokenLoop:
    while (true)
    {
      // skip white space
      while (*t != _T('\0') && _istspace(*t))
	t ++;
      if (*t == _T('\0') || *t == _T('#'))
	goto break_getTokenLoop; // no more tokens exist
      if (*t == _T('\\') && *(t + 1) == _T('\0'))
	goto continue_getLineLoop; // continue to next line
      
      const _TCHAR *tokenStart = t;
      
      // empty token
      if (*t == _T(','))
      {
	if (!isTokenExist)
	  o_tokens->push_back(Token(_T(""), false));
	isTokenExist = false;
	t ++;
	goto continue_getTokenLoop;
      }

      // paren
      if (*t == _T('('))
      {
	o_tokens->push_back(Token(Token::Type_openParen));
	isTokenExist = false;
	t ++;
	goto continue_getTokenLoop;
      }
      if (*t == _T(')'))
      {
	if (!isTokenExist)
	  o_tokens->push_back(Token(_T(""), false));
	isTokenExist = true;
	o_tokens->push_back(Token(Token::Type_closeParen));
	t ++;
	goto continue_getTokenLoop;
      }

      isTokenExist = true;
      
      // prefix
      if (m_prefixes)
	for (size_t i = 0; i < m_prefixes->size(); i ++)
	  if (_tcsnicmp(tokenStart, m_prefixes->at(i).c_str(),
			m_prefixes->at(i).size()) == 0)
	  {
	    o_tokens->push_back(Token(m_prefixes->at(i), false));
	    t += m_prefixes->at(i).size();
	    goto continue_getTokenLoop;
	  }

      // quoted or regexp
      if (*t == _T('"') || *t == _T('\'') ||
	  *t == _T('/') || (*t == _T('\\') && *(t + 1) == _T('m') &&
			    *(t + 2) != _T('\0')))
      {
	bool isRegexp = !(*t == _T('"') || *t == _T('\''));
	_TCHAR q[2] = { *t++, _T('\0') }; // quote character
	if (q[0] == _T('\\'))
	{
	  t++;
	  q[0] = *t++;
	}
	tokenStart = t;
	
	while (*t != _T('\0') && *t != q[0])
	{
	  if (*t == _T('\\') && *(t + 1))
	    t ++;
	  if (_istlead(*t) && *(t + 1))
	    t ++;
	  t ++;
	}
	
	tstring str =
	  interpretMetaCharacters(tokenStart, t - tokenStart, q, isRegexp);
	o_tokens->push_back(Token(str, true, isRegexp));
	if (*t != _T('\0'))
	  t ++;
	goto continue_getTokenLoop;
      }

      // not quoted
      {
	while (isSymbolChar(*t))
	{
	  if (*t == _T('\\'))
	    if (*(t + 1))
	      t ++;
	    else
	      break;
	  if (_istlead(*t) && *(t + 1))
	    t ++;
	  t ++;
	}
	if (t == tokenStart)
	{
	  ErrorMessage e;
	  e << _T("invalid character \\x") << std::hex;
#ifdef UNICODE
	  e << (int)(wchar_t)*t;
#else
	  e << (int)(u_char)*t;
#endif
	  e << std::dec;
	  if (_istprint(*t))
	    e << _T("(") << *t << _T(")");
	  throw e;
	}
	
	_TCHAR *numEnd = NULL;
	long value = _tcstol(tokenStart, &numEnd, 0);
	if (tokenStart == numEnd)
	{
	  tstring str = interpretMetaCharacters(tokenStart, t - tokenStart);
	  o_tokens->push_back(Token(str, false));
	}
	else
	{
	  o_tokens->push_back(
	    Token(value, tstringi(tokenStart, numEnd - tokenStart)));
	  t = numEnd;
	}
	goto continue_getTokenLoop;
      }
    }
    break_getTokenLoop:
    if (0 < o_tokens->size())
      break;
    m_lineNumber = m_internalLineNumber;
  }
  
  return 0 < o_tokens->size();
}
