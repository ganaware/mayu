// ////////////////////////////////////////////////////////////////////////////
// parser.cpp


#include "misc.h"

#include "errormessage.h"
#include "parser.h"
#include <cassert>


using namespace std;


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

Token::Token(int i_value, const istring &i_display)
  : m_type(Type_number),
    m_isValueQuoted(false),
    m_numericValue(i_value),
    m_stringValue(i_display),
    m_data(NULL)
{
}

Token::Token(const istring &i_value, bool i_isValueQuoted, bool i_isRegexp)
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
    m_stringValue(""),
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
    throw ErrorMessage() << "`" << *this << "' is not a Type_number.";
}

// get string value
istring Token::getString() const
{
  if (m_type == Type_string)
    return m_stringValue;
  throw ErrorMessage() << "`" << *this << "' is not a string.";
}

// get regexp value
istring Token::getRegexp() const
{
  if (m_type == Type_regexp)
    return m_stringValue;
  throw ErrorMessage() << "`" << *this << "' is not a regexp.";
}

// case insensitive equal
bool Token::operator==(const char *i_str) const
{
  if (m_type == Type_string)
    return m_stringValue == i_str;
  return false;
}

// paren equal
bool Token::operator==(const char i_c) const
{
  if (i_c == '(') return m_type == Type_openParen;
  if (i_c == ')') return m_type == Type_openParen;
  return false;
}

// stream output
ostream &operator<<(ostream &i_ost, const Token &i_token)
{
  switch (i_token.m_type)
  {
    case Token::Type_string: i_ost << i_token.m_stringValue; break;
    case Token::Type_number: i_ost << i_token.m_stringValue; break;
    case Token::Type_regexp: i_ost << i_token.m_stringValue; break;
    case Token::Type_openParen: i_ost << "("; break;
    case Token::Type_closeParen: i_ost << ")"; break;
  }
  return i_ost;
}


// ////////////////////////////////////////////////////////////////////////////
// Parser


Parser::Parser(istream &i_ist)
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
bool Parser::getLine(istring *o_line)
{
  o_line->resize(0);
  while (true)
  {
    char linebuf[1024] = "";
    m_ist.get(linebuf, sizeof(linebuf), '\n');
    if (m_ist.eof())
      break;
    m_ist.clear();
    *o_line += linebuf;
    char c;
    m_ist.get(c);
    if (m_ist.eof())
      break;
    if (c == '\n')
    {
      m_internalLineNumber ++;
      break;
    }
    *o_line += c;
  }
  
  return !(o_line->empty() && m_ist.eof());
}


// get a parsed line.
// if no more lines exist, returns false
bool Parser::getLine(vector<Token> *o_tokens)
{
  o_tokens->clear();
  m_lineNumber = m_internalLineNumber;

  istring line;
  continue_getLineLoop:
  while (getLine(&line))
  {
    const char *t = line.c_str();
    bool isTokenExist = false;

    continue_getTokenLoop:
    while (true)
    {
      // skip white space
      while (*t != '\0' && StringTool::isspace_(*t))
	t ++;
      if (*t == '\0' || *t == '#')
	goto break_getTokenLoop; // no more tokens exist
      if (*t == '\\' && *(t + 1) == '\0')
	goto continue_getLineLoop; // continue to next line
      
      const char *tokenStart = t;
      
      // empty token
      if (*t == ',')
      {
	if (!isTokenExist)
	  o_tokens->push_back(Token("", false));
	isTokenExist = false;
	t ++;
	goto continue_getTokenLoop;
      }

      // paren
      if (*t == '(')
      {
	o_tokens->push_back(Token(Token::Type_openParen));
	isTokenExist = false;
	t ++;
	goto continue_getTokenLoop;
      }
      if (*t == ')')
      {
	if (!isTokenExist)
	  o_tokens->push_back(Token("", false));
	isTokenExist = true;
	o_tokens->push_back(Token(Token::Type_closeParen));
	t ++;
	goto continue_getTokenLoop;
      }

      isTokenExist = true;
      
      // prefix
      if (m_prefixes)
	for (size_t i = 0; i < m_prefixes->size(); i ++)
	  if (StringTool::mbsniequal_(tokenStart, m_prefixes->at(i).c_str(),
				      m_prefixes->at(i).size()))
	  {
	    o_tokens->push_back(Token(m_prefixes->at(i), false));
	    t += m_prefixes->at(i).size();
	    goto continue_getTokenLoop;
	  }

      // quoted or regexp
      if (*t == '"' || *t == '\'' ||
	  *t == '/' || (*t == '\\' && *(t + 1) == 'm' && *(t + 2) != '\0'))
      {
	bool isRegexp = !(*t == '"' || *t == '\'');
	char q[2] = { *t++, '\0' }; // quote character
	if (q[0] == '\\')
	{
	  t++;
	  q[0] = *t++;
	}
	tokenStart = t;
	
	while (*t != '\0' && *t != q[0])
	{
	  if (*t == '\\' && *(t + 1))
	    t ++;
	  if (StringTool::ismbblead_(*t) && *(t + 1))
	    t ++;
	  t ++;
	}
	
	string str =
	  StringTool::interpretMetaCharacters(tokenStart, t - tokenStart, q);
	o_tokens->push_back(Token(str, true, isRegexp));
	if (*t != '\0')
	  t ++;
	goto continue_getTokenLoop;
      }

      // not quoted
      {
	while (*t != '\0' &&
	       (StringTool::ismbblead_(*t) ||
		StringTool::isalpha_(*t) ||
		StringTool::isdigit_(*t) ||
		StringTool::strchr_("-+/?_\\", *t)))
	{
	  if (*t == '\\')
	    if (*(t + 1))
	      t ++;
	    else
	      break;
	  if (StringTool::ismbblead_(*t) && *(t + 1))
	    t ++;
	  t ++;
	}
	if (t == tokenStart)
	{
	  ErrorMessage e;
	  e << "invalid character \\x" << hex << (int)(u_char)*t << dec;
	  if (StringTool::isprint_(*t))
	    e << "(" << *t << ")";
	  throw e;
	}
	
	char *numEnd = NULL;
	long value = strtol(tokenStart, &numEnd, 0);
	if (tokenStart == numEnd)
	{
	  string str =
	    StringTool::interpretMetaCharacters(tokenStart, t - tokenStart);
	  o_tokens->push_back(Token(str, false));
	}
	else
	{
	  o_tokens->push_back(
	    Token(value, istring(tokenStart, numEnd - tokenStart)));
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
