///////////////////////////////////////////////////////////////////////////////
// parser.cpp


#include "misc.h"

#include "errormessage.h"
#include "parser.h"
#include <cassert>


using namespace std;


///////////////////////////////////////////////////////////////////////////////
// Token


Token::Token(const Token &t)
  : type(t.type),
    isValueQuoted(t.isValueQuoted),
    numericValue(t.numericValue),
    stringValue(t.stringValue),
    data(t.data)
{
}

Token::Token(int value, const istring &display)
  : type(Number),
    isValueQuoted(false),
    numericValue(value),
    stringValue(display),
    data(NULL)
{
}

Token::Token(const istring &value, bool isValueQuoted_, bool isRegexp)
  : type(isRegexp ? Regexp : String),
    isValueQuoted(isValueQuoted_),
    numericValue(0),
    stringValue(value),
    data(NULL)
{
}


Token::Token(Type type_)
  : type(type_),
    isValueQuoted(false),
    numericValue(0),
    stringValue(""),
    data(NULL)
{
  assert(type == OpenParen || type == CloseParen);
}

// get numeric value
int Token::getNumber() const
{
  if (type == Number)
    return numericValue;
  if (stringValue.empty())
    return 0;
  else
    throw ErrorMessage() << "`" << *this << "' is not a number.";
}

// get string value
istring Token::getString() const
{
  if (type == String)
    return stringValue;
  throw ErrorMessage() << "`" << *this << "' is not a string.";
}

// get regexp value
istring Token::getRegexp() const
{
  if (type == Regexp)
    return stringValue;
  throw ErrorMessage() << "`" << *this << "' is not a regexp.";
}

// case insensitive equal
bool Token::operator==(const char *str) const
{
  if (type == String)
    return stringValue == str;
  return false;
}

// paren equal
bool Token::operator==(const char c) const
{
  if (c == '(') return type == OpenParen;
  if (c == ')') return type == OpenParen;
  return false;
}

// stream output
ostream &operator<<(ostream &ost, const Token &t)
{
  switch (t.type)
  {
    case Token::String: ost << t.stringValue; break;
    case Token::Number: ost << t.stringValue; break;
    case Token::Regexp: ost << t.stringValue; break;
    case Token::OpenParen: ost << "("; break;
    case Token::CloseParen: ost << ")"; break;
  }
  return ost;
}


///////////////////////////////////////////////////////////////////////////////
// Parser


Parser::Parser(istream &ist_)
  : lineNumber(1),
    prefix(NULL),
    internalLineNumber(1),
    ist(ist_)
{
}

// set string that may be prefix of a token
// prefix_ is not copied, so it must be preserved after setPrefix()
void Parser::setPrefix(const vector<istring> *prefix_)
{
  prefix = prefix_;
}

// get a line
bool Parser::getLine(istring *line_r)
{
  line_r->resize(0);
  while (true)
  {
    char linebuf[1024] = "";
    ist.get(linebuf, sizeof(linebuf), '\n');
    if (ist.eof())
      break;
    ist.clear();
    *line_r += linebuf;
    char c;
    ist.get(c);
    if (ist.eof())
      break;
    if (c == '\n')
    {
      internalLineNumber ++;
      break;
    }
    *line_r += c;
  }
  
  return !(line_r->empty() && ist.eof());
}


// get a parsed line
// if no more lines exist, returns false
bool Parser::getLine(vector<Token> *tokens_r)
{
  tokens_r->clear();
  lineNumber = internalLineNumber;

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
	  tokens_r->push_back(Token("", false));
	isTokenExist = false;
	t ++;
	goto continue_getTokenLoop;
      }

      // paren
      if (*t == '(')
      {
	tokens_r->push_back(Token(Token::OpenParen));
	isTokenExist = false;
	t ++;
	goto continue_getTokenLoop;
      }
      if (*t == ')')
      {
	if (!isTokenExist)
	  tokens_r->push_back(Token("", false));
	isTokenExist = true;
	tokens_r->push_back(Token(Token::CloseParen));
	t ++;
	goto continue_getTokenLoop;
      }

      isTokenExist = true;
      
      // prefix
      if (prefix)
	for (size_t i = 0; i < prefix->size(); i ++)
	  if (StringTool::mbsniequal_(tokenStart, prefix->at(i).c_str(),
				      prefix->at(i).size()))
	  {
	    tokens_r->push_back(Token(prefix->at(i), false));
	    t += prefix->at(i).size();
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
	tokens_r->push_back(Token(str, true, isRegexp));
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
	  tokens_r->push_back(Token(str, false));
	}
	else
	{
	  tokens_r->push_back(
	    Token(value, istring(tokenStart, numEnd - tokenStart)));
	  t = numEnd;
	}
	goto continue_getTokenLoop;
      }
    }
    break_getTokenLoop:
    if (0 < tokens_r->size())
      break;
    lineNumber = internalLineNumber;
  }
  
  return 0 < tokens_r->size();
}
