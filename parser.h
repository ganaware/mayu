///////////////////////////////////////////////////////////////////////////////
// parser.h


#ifndef __parser_h__
#define __parser_h__


#include "misc.h"

#include "stringtool.h"

#include <vector>


using StringTool::istring;


class Token
{
public:
  enum Type { String, Number, Regexp, OpenParen, CloseParen };
  
private:
  u_char type;
  bool isValueQuoted;
  int numericValue;
  istring stringValue;
  long data;
  
public:
  Token(const Token &t);
  Token(int value, const istring &display);
  Token(const istring &value, bool isValueQuoted_, bool isRegexp = false);
  Token(Type type_);
  
  // is the value quoted ?
  bool isQuoted() const { return isValueQuoted; }

  // value type
  Type getType() const { return static_cast<Type>(type); }
  bool isString() const { return type == String; }
  bool isNumber() const { return type == Number; }
  bool isRegexp() const { return type == Regexp; }
  bool isOpenParen() const { return type == OpenParen; }
  bool isCloseParen() const { return type == CloseParen; }
  
  // get numeric value
  int getNumber() const;
  
  // get string value
  istring getString() const;
  
  // get regexp value
  istring getRegexp() const;

  // get data
  long getData() const { return data; }
  void setData(long data_) { data = data_; }
  
  // case insensitive equal
  bool operator==(const istring &str) const{ return *this == str.c_str(); }
  bool operator==(const char *str) const;
  bool operator!=(const istring &str) const{ return *this != str.c_str(); }
  bool operator!=(const char *str) const { return !(*this == str); }
  
  // paren equal c is '(' or ')'
  bool operator==(const char c) const;
  bool operator!=(const char c) const { return !(*this == c); }

  // stream output
  friend std::ostream &operator<<(std::ostream &ost, const Token &t);
};


class Parser
{
  size_t lineNumber;		// current line number
  const std::vector<istring> *prefix;
  // string that may be prefix of a token
  
  size_t internalLineNumber;	// next line number
  std::istream &ist;		// input stream

  // get a line
  bool getLine(istring *line_r);
  
public:
  Parser(std::istream &ist_);

  // get a parsed line
  // if no more lines exist, returns false
  bool getLine(std::vector<Token> *tokens_r);
  
  // get current line number
  size_t getLineNumber() const { return lineNumber; }
  
  // set string that may be prefix of a token
  // prefix_ is not copied, so it must be preserved after setPrefix()
  void setPrefix(const std::vector<istring> *prefix_);
};


#endif // __parser_h__
