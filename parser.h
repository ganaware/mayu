// ////////////////////////////////////////////////////////////////////////////
// parser.h


#ifndef _PARSER_H
#  define _PARSER_H

#  include "misc.h"
#  include "stringtool.h"
#  include <vector>


using StringTool::istring;


///
class Token
{
public:
  ///
  enum Type
  {
    Type_string,				///
    Type_number,				///
    Type_regexp,				///
    Type_openParen,				///
    Type_closeParen,				///
  };
  
private:
  u_char m_type;				///
  bool m_isValueQuoted;				///
  int m_numericValue;				///
  istring m_stringValue;			///
  long m_data;					///
  
public:
  ///
  Token(const Token &i_token);
  ///
  Token(int i_value, const istring &i_display);
  ///
  Token(const istring &i_value, bool i_isValueQuoted, bool i_isRegexp = false);
  ///
  Token(Type i_type);
  
  /// is the value quoted ?
  bool isQuoted() const { return m_isValueQuoted; }

  /// value type
  Type getType() const { return static_cast<Type>(m_type); }
  ///
  bool isString() const { return m_type == Type_string; }
  ///
  bool isNumber() const { return m_type == Type_number; }
  ///
  bool isRegexp() const { return m_type == Type_regexp; }
  ///
  bool isOpenParen() const { return m_type == Type_openParen; }
  ///
  bool isCloseParen() const { return m_type == Type_closeParen; }
  
  /// get numeric value
  int getNumber() const;
  
  /// get string value
  istring getString() const;
  
  /// get regexp value
  istring getRegexp() const;

  /// get data
  long getData() const { return m_data; }
  ///
  void setData(long i_data) { m_data = i_data; }
  
  /// case insensitive equal
  bool operator==(const istring &i_str) const
  { return *this == i_str.c_str(); }
  ///
  bool operator==(const char *i_str) const;
  ///
  bool operator!=(const istring &i_str) const{ return *this != i_str.c_str(); }
  ///
  bool operator!=(const char *i_str) const { return !(*this == i_str); }
  
  /// paren equal c is '<code>(</code>' or '<code>)</code>'
  bool operator==(const char i_c) const;
  ///
  bool operator!=(const char i_c) const { return !(*this == i_c); }

  /// stream output
  friend std::ostream &operator<<(std::ostream &i_ost, const Token &i_token);
};


///
class Parser
{
public:
  typedef std::vector<Token> Tokens;
  
private:
  typedef std::vector<istring> Prefixes;
  
private:
  size_t m_lineNumber;				/// current line number
  const Prefixes *m_prefixes;			/** string that may be prefix
                                                    of a token */
  
  size_t m_internalLineNumber;			/// next line number
  std::istream &m_ist;				/// input stream

private:
  /// get a line
  bool getLine(istring *o_line);
  
public:
  ///
  Parser(std::istream &i_ist);

  /** get a parsed line.  if no more lines exist, returns false */
  bool getLine(Tokens *o_tokens);
  
  /// get current line number
  size_t getLineNumber() const { return m_lineNumber; }
  
  /** set string that may be prefix of a token.  prefix_ is not
      copied, so it must be preserved after setPrefix() */
  void setPrefixes(const Prefixes *m_prefixes);
};


#endif // _PARSER_H
