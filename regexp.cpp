// ///////////////////////////////////////////////////////////////////////////
// regexp.cpp

// modified by TAGA Nayuta <nayuta@is.s.u-tokyo.ac.jp>
//
// * more precise general text routine mapping (R_char etc.)
// * added some perl-like regexp symbol (\b \B \w \W \s \S \d \D)
// * errors are reported by InvalidRegexp exception
// * support for multi byte string
// * some bug fix
//   - maches empty string incorrectly

// Win32 porting notes.
//
// #if defined( _MBCS )
// #pragma message( __FILEINFO__ "This code is broken under _MBCS, " \ 
//                 "see the comments at the top of this file." )
// #endif //_MBCS
//
//
// In  case  this isn't  obvious from  the later  comments this  is an
// ALTERED version of the software.  If you like my changes then cool,
// but  nearly all  of the  functionality here  is derived  from Henry
// Spencer's original work.
//
// This  code should work correctly  under both _SBCS  and _UNICODE, I
// did start working on making it  work with _MBCS but gave up after a
// while since I don't need this particular port and it's not going to
// be as straight forward as the other two.
//
// The problem stems from the compiled program being stored as TCHARS,
// the  individual  items  need to  be  wide enough  to hold  whatever
// character is thrown at them,  but currently they are accessed as an
// array of  whatever size integral type is  appropriate.  _MBCS would
// cause  this to be char,  but at times it  would need to  be larger. 
// This would require making the program be an array of short with the
// appropriate  conversions used  everywhere.  Certainly  it's doable,
// but it's  a pain.   What's  worse  is  that the  current code  will
// compile  and  run  under _MBCS,  only  breaking when  it gets  wide
// characters thrown against it.
//
// I've marked at least one bit of code with #pragma messages, I may
// not get all of them, but they should be a start
//
// Guy Gascoigne - Piggford (ggp@bigfoot.com) Friday, February 27, 1998

// regcomp and regexec -- regsub and regerror are elsewhere
// @(#)regexp.c 1.3 of 18 April 87
//
//  Copyright (c) 1986 by University of Toronto.
//  Written by Henry Spencer.  Not derived from licensed software.
//
//  Permission is  granted to  anyone to  use this  software  for any
//  purpose  on any  computer system, and  to redistribute  it freely,
//  subject to the following restrictions:
//
//  1. The author is not responsible for the consequences of use of
//      this software, no matter how awful, even if they arise
//      from defects in it.
//
//  2. The origin of this software must not be misrepresented, either
//      by explicit claim or by omission.
//
//  3. Altered versions must be plainly marked as such, and must not
//      be misrepresented as being the original software.
//
// *** THIS IS AN ALTERED VERSION.  It was altered by John Gilmore,
// ***   hoptoad!gnu, on 27 Dec 1986, to add \< and \> for word-matching
// ***   as in BSD grep and ex.
// *** THIS IS AN ALTERED VERSION.  It was altered by John Gilmore,
// ***   hoptoad!gnu, on 28 Dec 1986, to optimize characters quoted with \.
// *** THIS IS AN ALTERED VERSION.  It was altered by James A. Woods,
// ***   ames!jaw, on 19 June 1987, to quash a regcomp() redundancy.
// *** THIS IS AN ALTERED VERSION.  It was altered by Geoffrey Noer,
// *** THIS IS AN ALTERED VERSION.  It was altered by Guy Gascoigne - Piggford
// ***   guy@wyrdrune.com, on 15 March 1998, porting it to C++ and converting
// ***   it to be the engine for the Regexp class
// *** THIS IS AN ALTERED VERSION.  It was altered by TAGA Nayuta
// ***   support perl-like regexp etc.
//
// Beware that  some of this code is subtly aware  of the way operator
// precedence is structured in regular expressions.  Serious changes in
// regular-expression syntax might require a total rethink.

#include "regexp.h"
#include <malloc.h>
#include <stdio.h>
#include "regexp_internal.h"


// The  first byte of the  regexp internal "program"  is actually this
// magic number; the start node begins in the second byte.

const R_char MAGIC = (R_char)'\234';

// The "internal use only" fields in regexp.h are present to pass info
// from compile to execute that  permits the execute phase to run lots
// faster on simple cases.  They are:
//
// regstart	char that must begin a match; '\0' if none obvious
// reganch	is the match anchored (at beginning-of-line only)?
// regmust	string (pointer into program) that match must include, or NULL
// regmlen	length of regmust string
//
// Regstart  and  reganch  permit  very  fast  decisions  on  suitable
// starting points for a match,  cutting down the work a lot.  Regmust
// permits  fast rejection of  lines that cannot possibly  match.  The
// regmust tests  are costly enough that regcomp()  supplies a regmust
// only if  the  r.e. contains  something  potentially  expensive  (at
// present, the only such thing detected is * or + at the start of the
// r.e., which can  involve a  lot of  backup).  Regmlen  is  supplied
// because  the test in regexec() needs it and regcomp() is  computing
// it anyway.

// Structure for  regexp  "program".  This  is  essentially  a  linear
// encoding of  a nondeterministic  finite-state machine  (aka  syntax
// charts or "railroad normal form" in parsing technology).  Each node
//  is an  opcode plus  a "next"  pointer, possibly  plus an  operand. 
// "Next" pointers of all nodes except BRANCH implement concatenation;
// a "next" pointer with a BRANCH on both ends of it is connecting two
// alternatives.  (Here we have one of the subtle syntax dependencies:
// an individual BRANCH (as opposed  to a collection of them) is never
// concatenated  with anything  because of operator  precedence.)  The
// operand of  some types of node is a literal  string; for others, it
// is a node leading into  a sub-FSM.  In particular, the operand of a
// BRANCH node is  the first node of the branch.  (NB  this is *not* a
//  tree structure:  the  tail of  the  branch connects  to the  thing
// following the set of BRANCHes.)  The opcodes are:

enum
{
//definition	// opnd?	meaning
  END     = 0,	// no	\0	End of program.
  BOL     = 1,	// no	^	Match beginning of line.
  EOL     = 2,	// no	$	Match end of line.
  ANY     = 3,	// no	.	Match any character.
  ANYOF   = 4,	// str	[...]	Match any of these.
  ANYBUT  = 5,	// str	[^...]	Match any but one of these.
  BRANCH  = 6,	// node	|	Match this, or the next..\&.
  BACK    = 7,	// no		"next" ptr points backward.
  EXACTLY = 8,	// str		Match this string.
  NOTHING = 9,	// no		Match empty string.
  STAR    = 10,	// node	*	Match this 0 or more times.
  PLUS    = 11,	// node	+	Match this 1 or more times.
//WORDA   = 12,	// no	\<	Match "" at wordchar, where prev is nonword
//WORDZ   = 13,	// no	\>	Match "" at nonwordchar, where prev is word
  WORDC   = 14,	// no	\w	[a-zA-Z0-9_]	word character
  NONWORDC= 15,	// no	\W	[^a-zA-Z0-9_]	non-(word character)
  WS      = 16,	// no	\s	[\t\n\v\f\n ]	whitespace
  NONWS   = 17,	// no	\S	[^\t\n\v\f\n ]	non-whitespace
  DIGIT   = 18,	// no	\d	[0-9]		digit
  NONDIGIT= 19,	// no	\D	[^0-9]		non-digit
  WORDB   = 20,	// no	\b	Match a word boundary
  NONWORDB= 21,	// no	\B	Match a non-(word boundary)
  ANYOFC  = 22,	// str	[...]	Match any of these. (complex)
  ANYBUTC = 23,	// str	[^...]	Match any but one of these. (complex)
  
  OPEN    = 64,	// no	(	Sub-RE starts here. OPEN+1 is number 1, etc.
  CLOSE   = 96,	// no	)	Analogous to OPEN.
};

// Opcode notes:
//
// BRANCH
//	The set  of branches constituting  a single choice  are hooked
//	together with their "next" pointers, since precedence prevents
//	anything  being concatenated  to any  individual  branch.  The
//	"next" pointer  of the last BRANCH  in a choice  points to the
//	thing  following the  whole choice.   This is  also  where the
//	final "next"  pointer of  each individual branch  points; each
//	branch starts with the operand node of a BRANCH node.
//
// BACK	Normal  "next" pointers  all implicitly  point  forward;  BACK
//	exists to make loop structures possible.
//
// STAR, PLUS
//	'?',  and complex  '*' and  '+', are  implemented  as circular     
//	BRANCH  structures using  BACK.  Simple  cases  (one character
//	per  match)  are implemented  with  STAR  and  PLUS for  speed
//	and to minimize recursive plunges.
//
// OPEN, CLOSE	...are numbered at compile time.
//
// ANYOF, ANYBUT
//	For example,  [abc-f#] (where # is multibyte char) is compiled
//	to:
//		ANYOF 'abcdef#' 0
//
// ANYOFC, ANYBUTC
//	For example, [ab#A-G$-%] (where #$% are multibyte char,
//	256 <= (int)G - (int)A) is compiled to:
//		ANYOFC 'ab#' 0 'AG$%' 0

// A  node  is one  char of  opcode followed  by two  chars  of "next"
// pointer.  "Next"  pointers  are  stored as  two 8-bit  pieces, high
// order first.  The value is a positive offset from the opcode of the
// node containing  it.  An operand, if any, simply  follows the node. 
// (Note  that much of the  code generation knows  about this implicit
// relationship.)
//
// Using  two bytes for the  "next" pointer is vast  overkill for most
// things, but allows patterns to get big without disasters.


// Flags to be passed up and down.

enum
{
  WORST    = 0,		// Worst case.
  HASWIDTH = 1 << 0,	// Known never to match null string.
  SIMPLE   = 1 << 1,	// Simple enough to be STAR/PLUS operand.
  SPSTART  = 1 << 2,	// Starts with * or +.
};

enum
{
#if sizeof_R_char == 1
  lengthof_OP = 3,
#else
  lengthof_OP = 2,
#endif
};


// ////////////////////////////////////////////////////////////////////////////
// classes


class CRegExecutor;
class CRegCompilerBase;
class CRegValidator;
class CRegCompiler;
typedef R_char PROGRAM;


// ////////////////////////////////////////////////////////////////////////////
// All of the functions required to directly access the 'program'


#if sizeof_R_char == 1
static inline short short_le(PROGRAM *p)
{
  return (short)(((short)p[0] & 0x00ff) |
		 ((short)p[1] << 8));
}
#endif
static inline PROGRAM OP(PROGRAM *p) { return *p; }
static inline PROGRAM *OPERAND(PROGRAM *p) { return p + lengthof_OP; }
static inline PROGRAM *regnext(PROGRAM *p)
{
#if sizeof_R_char == 1
  const short offset = short_le(p + 1);
#else
  const PROGRAM offset = *(p + 1);
#endif
  if (offset == 0)
    return NULL;
  return (OP(p) == BACK) ? p - offset : p + offset;
}


// ////////////////////////////////////////////////////////////////////////////
// function


// for debugging
void dump(const R_char *exp, PROGRAM *program, size_t lengthof_program)
{
  if (!*exp)
    return;
  
  printf("regexp: %s\n", exp);
  
  PROGRAM *p = program;
  PROGRAM *end = program + lengthof_program - 1;

  while (p < end)
  {
#if sizeof_R_char == 1
    const short offset = short_le(p + 1);
#else
    const PROGRAM offset = *(p + 1);
#endif

    printf("\t%d:\t", (p - program));
    printf("(%d)", (p - program) + ((OP(p) == BACK) ? -offset : offset));
    
    switch (*p)
    {
      case END: printf("END"); p += lengthof_OP; break;
      case BOL: printf("BOL"); p += lengthof_OP; break;
      case EOL: printf("EOL"); p += lengthof_OP; break;
      case ANY: printf("ANY"); p += lengthof_OP; break;
      case ANYOF:
	printf("ANYOF %s", OPERAND(p));
	p = OPERAND(p) + R_strlen(OPERAND(p)) + 1;
	break;
      case ANYBUT:
	printf("ANYBUT %s", OPERAND(p));
	p = OPERAND(p) + R_strlen(OPERAND(p)) + 1;
	break;
      case BRANCH: printf("BRANCH"); p += lengthof_OP; break;
      case BACK: printf("BACK"); p += lengthof_OP; break;
      case EXACTLY:
	printf("EXACTLY %s", OPERAND(p));
	p = OPERAND(p) + R_strlen(OPERAND(p)) + 1;
	break;
      case NOTHING: printf("NOTHING"); p += lengthof_OP; break;
      case STAR: printf("STAR"); p += lengthof_OP; break;
      case PLUS: printf("PLUS"); p += lengthof_OP; break;
      case WORDC: printf("WORDC"); p += lengthof_OP; break;
      case NONWORDC: printf("NONWORDC"); p += lengthof_OP; break;
      case WS: printf("WS"); p += lengthof_OP; break;
      case NONWS: printf("NONWS"); p += lengthof_OP; break;
      case DIGIT: printf("DIGIT"); p += lengthof_OP; break;
      case NONDIGIT: printf("NONDIGIT"); p += lengthof_OP; break;
      case WORDB: printf("WORDB"); p += lengthof_OP; break;
      case NONWORDB: printf("NONWORDB"); p += lengthof_OP; break;
      case ANYOFC:
	printf("ANYOFC %s ", OPERAND(p));
	p = OPERAND(p) + R_strlen(OPERAND(p)) + 1;
	printf("%s", p);
	p += R_strlen(p) + 1;
	break;
      case ANYBUTC:
	printf("ANYBUT %s ", OPERAND(p));
	p = OPERAND(p) + R_strlen(OPERAND(p)) + 1;
	printf("%s", p);
	p += R_strlen(p) + 1;
	break;
      default:
	if (OPEN <= *p && *p <= CLOSE)
	{
	  printf("OPEN+%d", *p - OPEN);
	  p += lengthof_OP;
	}
	else if (CLOSE <= *p)
	{
	  printf("CLOSE+%d", *p - CLOSE);
	  p += lengthof_OP;
	}
	else
	{
	  printf("error");
	  p = end;
	}
	break;
    }

    printf("\n");
  }
}


#ifndef R_strrange
// Is c in the range ?
bool R_strrange(const R_char *range, const R_char *c)
  // range is "ABCDEF" if /[A-BC-DE-F]/
{
  const R_char *next = R_strinc(c);
  if (!next)
    return false;
  size_t clen = next - c;
  
  for (; *range; range = next)
  {
    const R_char *from = range;
    const R_char *to = R_strinc(from);
    if (!to || *to == R_char_0)
      return false;
    next = R_strinc(to);
    if (!next)
      return false;

    size_t fromlen = to - from;
    size_t tolen = next - to;

    if (clen == 1 && fromlen == 1 && tolen == 1)
    {
      if (*from <= *c && *c <= *to)
	return true;
    }
    else
    {
      if (fromlen < clen && clen < tolen)
	return true;
      
      int f = R_strncmp(from, c, clen);
      int t = R_strncmp(c, to, clen);
      
      if (fromlen == clen && clen == tolen && f <= 0 && t <= 0)
	return true;
      
      if (fromlen < clen &&
	  ((fromlen == clen && f <= 0) || (tolen == clen && t <= 0)))
	return true;
    }
  }
  return false;
}
#endif // R_strrange


#ifndef R_strmchr
// strchr (multi byte)
R_char *R_strmchr(R_char *s, const R_char *c)
  // 'c' need not be null terminated
{
  if (!R_ismbblead(*c))
    return R_strchr(s, *c);
  
  const R_char *next = R_strinc(c);
  if (*c == R_char_0 || !next)
    return NULL;
  size_t len = next - c;
  
  while (*s)
  {
    if (R_ismbblead(*s))
    {
      R_char *next = R_strinc(s);
      if (!next)
	return NULL;
      if ((size_t)(next - s) == len)
      {
	size_t i;
	for (i = 0; i < len; i ++)
	  if (s[i] != c[i])
	    break;
	if (i == len)
	  return s;
      }
      s = next;
    }
    else
      s ++;
  }
  return NULL;
}
#endif R_strmchr


// ////////////////////////////////////////////////////////////////////////////
// The internal interface to the regexp,  wrapping the compilation as
// well as the execution of the regexp (matching)


class Regexp::regexp
{
  friend class ::CRegExecutor;

  // substring
  const R_char *beginp[Regexp::NSUBEXP];
  const R_char *endp[Regexp::NSUBEXP];
  
  // Internal use only
  R_char regstart;
  R_char reganch;
  R_char *regmust;
  size_t regmlen;

  // program
  RegexpUtil::AutoDeleteArrayPtr<PROGRAM> program;
  size_t lengthof_program;
  
  int refcount;	// used by Regexp to manage the reference counting of regexps
  size_t numSubs;

private:
  void ignoreCase(const R_char *in, R_char *out);// ignore case (MT-unsafe)
  void regcomp(const R_char *exp) throw (InvalidRegexp); // compile (MT-unsafe)
  
  // check substring index
  void checkSubStringIndex(size_t i) const throw (std::out_of_range)
  {
    if (numSubs < i)
      throw std::out_of_range("no more substrings");
  }
  
public:
  // constructor (MT-unsafe)
  regexp(const R_char *exp, bool doesIgnoreCase)  throw (InvalidRegexp);
  regexp(const regexp &o);
  
  // execute (MT-unsafe)
  bool regexec(const R_char *target) throw (InvalidRegexp);
  
  // replace \1 \2 ... in source (MT-unsafe)
  R_string replace(const R_char *source) const;

  // make clone of this object (MT-unsafe)
  regexp *clone() { return new regexp(*this); }

  // reference count
  int unrefer() { return -- refcount; }
  void refer() { ++ refcount; }
  int getRefcount() const { return refcount; }

  // substring
  const R_char *getBeginp(size_t i) const throw (std::out_of_range)
  { checkSubStringIndex(i); return beginp[i]; }
  const R_char *getEndp(size_t i) const throw (std::out_of_range)
  { checkSubStringIndex(i); return endp[i]; }
  size_t getNumSubs() const { return numSubs - 1; }
};


// ////////////////////////////////////////////////////////////////////////////
// Compile / Validate the regular expression - ADT


class CRegCompilerBase
{
protected:
  const R_char *regparse;	// Input-scan pointer. 
  int regnpar;			// () count. 

  R_char *regbranch(int *flags_r) throw (Regexp::InvalidRegexp);
  R_char *regpiece(int *flags_r) throw (Regexp::InvalidRegexp);
  R_char *regatom(int *flags_r) throw (Regexp::InvalidRegexp);
  bool regany(int mode) throw (Regexp::InvalidRegexp);
  
  virtual void regc(PROGRAM c) = 0;
  virtual void regc(const PROGRAM *begin, const PROGRAM *end) = 0;
  virtual PROGRAM *regnode(int op) = 0;
  virtual void reginsert(int op, PROGRAM *opnd) = 0;
  virtual void regtail(PROGRAM *p, const PROGRAM *val) = 0;
  virtual void regoptail(PROGRAM *p, const PROGRAM *val) = 0;
public:
  // constructor
  CRegCompilerBase(const R_char *parse)
    : regparse(parse), regnpar(1) { }
  
  // destructor
  virtual ~CRegCompilerBase() { }
  
  PROGRAM *reg(bool isInParen, int *flags_r) throw (Regexp::InvalidRegexp);
};


// ////////////////////////////////////////////////////////////////////////////
// First pass over the expression, testing for validity and returning the 
// program size


class CRegValidator : public CRegCompilerBase
{
  size_t regsize;		// Code size. 
  R_char regdummy[lengthof_OP];	// NOTHING, 0 next-ptr
  virtual void regc(PROGRAM) { regsize ++; }
  virtual void regc(const PROGRAM *begin, const PROGRAM *end)
  { regsize += end - begin; }
  virtual PROGRAM *regnode(int) { regsize += lengthof_OP; return regdummy; }
  virtual void reginsert(int, PROGRAM *) { regsize += lengthof_OP; }
  virtual void regtail(PROGRAM *, const PROGRAM *) { }
  virtual void regoptail(PROGRAM *, const PROGRAM *) { }

public:
  // constructor
  CRegValidator(const R_char *parse)
    : CRegCompilerBase(parse), regsize(0)
  {
    regc(MAGIC);
    regdummy[0] = NOTHING;
    regdummy[1] = 0;
#if sizeof_R_char == 1
    regdummy[2] = 0;
#endif
  }

  // destructor
  virtual ~CRegValidator() { }

  // get size
  size_t getSize() const { return regsize; }
};


// ////////////////////////////////////////////////////////////////////////////
// Second pass, actually generating the program


class CRegCompiler : public CRegCompilerBase
{
  PROGRAM *regcode;
  
protected:
  // regc - emit (if appropriate) a byte of code
  virtual void regc(PROGRAM c) { *regcode ++ = c; }
  virtual void regc(const PROGRAM *begin, const PROGRAM *end);
  virtual PROGRAM *regnode(int op);
  virtual void reginsert(int op, PROGRAM *opnd);
  virtual void regtail(PROGRAM *p, const PROGRAM *val);
  virtual void regoptail(PROGRAM *p, const PROGRAM *val);

public:
  CRegCompiler(const R_char *parse, PROGRAM *prog)
    : CRegCompilerBase(parse), regcode(prog)
  {
    regc(MAGIC);
  }
  
  // destructor
  virtual ~CRegCompiler() { }
};

// regc - emit (if appropriate) a byte of code
void CRegCompiler::regc(const PROGRAM *begin, const PROGRAM *end)
{
  for (; begin != end; begin ++)
    *regcode ++ = *begin;
}

// regnode - emit a node
PROGRAM *CRegCompiler::regnode(int op)
{
  PROGRAM *r = regcode;
  *regcode ++ = (R_char)op;
  *regcode ++ = R_char_0;	// Null next pointer.
#if sizeof_R_char == 1
  *regcode ++ = R_char_0;
#endif
  return r;
}

// reginsert - insert an operator in front of already-emitted operand 
//             Means relocating the operand.
void CRegCompiler::reginsert(int op, PROGRAM *opnd)
{
  memmove(opnd + lengthof_OP, opnd, (regcode - opnd) * sizeof(R_char));
  regcode += lengthof_OP;
  
  *opnd ++ = (PROGRAM)op;
  *opnd ++ = R_char_0;
#if sizeof_R_char == 1
  *opnd ++ = R_char_0;
#endif
}

// regtail - set the next-pointer at the end of a node chain
void CRegCompiler::regtail(PROGRAM *p, const PROGRAM *val)
{
  PROGRAM *scan;
  PROGRAM *temp;

  // Find last node. 
  for (scan = p; (temp = regnext(scan)) != NULL; scan = temp)
    continue;

#if sizeof_R_char == 1
  short offset = (short)((OP(scan) == BACK) ? scan - val : val - scan);
  assert(0 <= offset);
  scan[1] = (PROGRAM)offset;
  scan[2] = (PROGRAM)(offset >> 8);
#else
  PROGRAM offset = (PROGRAM)((OP(scan) == BACK) ? scan - val : val - scan);
  *(scan + 1) = offset;
#endif
}

// regoptail - regtail on operand of first argument; nop if operandless
void CRegCompiler::regoptail(PROGRAM *p, const PROGRAM *val)
{
  // "Operandless" and "op != BRANCH" are synonymous in practice. 
  if (OP(p) == BRANCH)
    regtail(OPERAND(p), val);
}


// ////////////////////////////////////////////////////////////////////////////
// Regexp::regexp


// constructor
Regexp::regexp::regexp(const R_char *exp, bool doesIgnoreCase)
  throw (Regexp::InvalidRegexp)
  : lengthof_program(0),
    regstart(0),
    reganch(0),
    regmust(0),
    regmlen(0),
    program(0),
    refcount(1),
    numSubs(1)
{
  assert(exp);
  
  if (doesIgnoreCase)
  {
    // regcomp throws InvalidRegexp, so we must ensure 'delete[] out'
    RegexpUtil::AutoDeleteArrayPtr<R_char>
      out(new R_char[(R_strlen(exp) * 4) + 1]);
    ignoreCase(exp, out);
    regcomp(out);
  }
  else
    regcomp(exp);
#if 0
  dump(exp, program + 1, lengthof_program);
#endif
}

// copy constructor
Regexp::regexp::regexp(const regexp &o)
  : lengthof_program(o.lengthof_program),
    regstart(o.regstart),
    reganch(o.reganch),
    regmust(0),
    regmlen(o.regmlen),
    refcount(1),
    numSubs(o.numSubs)
{
  if (o.program)
  {
    program = new R_char[lengthof_program];
    memcpy(program, o.program, lengthof_program * sizeof(R_char));
  }
  if (o.regmust)
    regmust = program + (o.regmust - o.program);

  for (int i = Regexp::NSUBEXP; i > 0; i--)
  {
    beginp[i] = o.beginp[i];
    endp[i] = o.endp[i];
  }
}


// regcomp - compile a regular expression into internal code
//
// We  can't allocate space  until we know  how big the  compiled form
// will  be, but we  can't compile  it (and thus  know how big  it is)
// until we've got  a place to put the code.  So  we cheat: we compile
// it  twice, once with code  generation turned off  and size counting
// turned  on, and  once "for  real".  This also  means that  we don't
// allocate space until we are sure that the thing really will compile
// successfully,  and  we  never  have  to  move  the  code  and  thus
// invalidate pointers into it.  (Note  that it has to be in one piece
// because free() must be able to free it all.)
//
// Beware  that the optimization-preparation code in  here knows about
// some of the structure of the compiled regexp.
void Regexp::regexp::regcomp(const R_char *exp)
  throw (Regexp::InvalidRegexp)
{
  assert(exp);
  
  int flags;
  
  // First pass: determine size, legality.
  CRegValidator tester(exp);
  _true( tester.reg(false, &flags) );

  // Small enough for pointer-storage convention?
  if (
#if sizeof_R_char == 1
    0x7fffL  // Probably could be 0xffffL.
#else
    (((R_char)1 << (sizeof(R_char) * 8 - 1)) - 1)
#endif
    <= tester.getSize())
    throw InvalidRegexp("regexp too big");

  // Allocate space.
  lengthof_program = tester.getSize();
  program = new R_char[lengthof_program];
#ifndef NDEBUG
  memset(program, 0, sizeof(R_char) * lengthof_program);
#endif
  
  // Second pass: emit code. 
  CRegCompiler comp(exp, program);
  _true( comp.reg(false, &flags) );

  PROGRAM *scan = program + 1;		// First BRANCH.
  assert(OP(scan) == BRANCH);
  if (OP(regnext(scan)) == END)		// Only one top-level choice. 
  {
    scan = OPERAND(scan);
    
    // Starting-point info. 
    if (OP(scan) == EXACTLY)
    {
      regstart = (R_char)*OPERAND(scan);
      if (R_ismbblead(regstart))
	regstart = R_char_0;
    }
    else if (OP(scan) == BOL)
      reganch = 1;
    
    // If there's  something expensive in  the r.e., find  the longest   
    // literal  string that  must  appear  and make  it  the regmust.  
    // Resolve  ties in  favor of  later strings,  since  the regstart
    // check  works  with the  beginning  of  the  r.e.  and  avoiding
    // duplication  strengthens checking.   Not a  strong  reason, but
    // sufficient in the absence of others.
         
    if (flags & SPSTART)
    {
      regmust = NULL;
      regmlen = 0;

      for (; scan != NULL; scan = regnext(scan))
	if (OP(scan) == EXACTLY)
	{
	  size_t len = R_strlen((R_char *)OPERAND(scan));
	  if (regmlen <= len)
	  {
	    regmust = (R_char *)OPERAND(scan);
	    regmlen = len;
	  }
	}
    }
  }
}

// reg - regular expression, i.e. main body or parenthesized thing
//
// Caller must absorb opening parenthesis.
//
// Combining  parenthesis  handling  with the  base  level of  regular
// expression is a trifle forced, but the need to tie the tails of the
// branches to what follows makes it hard to avoid.
R_char *CRegCompilerBase::reg(bool isInParen, int *flags_r)
  throw (Regexp::InvalidRegexp)
{
  R_char *ret = NULL;
  int parno = 0;
  
  *flags_r = HASWIDTH;  // Tentatively. 
  
  if (isInParen)
  {
    // Make an OPEN node. 
    if (Regexp::NSUBEXP <= regnpar)
      throw Regexp::InvalidRegexp("too many ()");
    parno = regnpar;
    regnpar ++;
    ret = regnode(OPEN + parno);
  }

  // Pick up the branches, linking them together.
  int flags;
  R_char *br = regbranch(&flags);
  assert(br);
  if (isInParen)
    regtail(ret, br);			// OPEN -> first. 
  else
    ret = br;
  *flags_r &= ~(~flags & HASWIDTH);	// Clear bit if bit 0.
  *flags_r |= flags & SPSTART;
  while (*regparse == R_('|'))
  {
    regparse ++;
    br = regbranch(&flags);
    assert(br);
    regtail(ret, br);			// BRANCH -> BRANCH. 
    *flags_r &= ~(~flags & HASWIDTH);
    *flags_r |= flags & SPSTART;
  }

  // Make a closing node, and hook it on the end. 
  R_char *ender = regnode(isInParen ? CLOSE + parno : END);
  regtail(ret, ender);

  // Hook the tails of the branches to the closing node. 
  for (br = ret; br != NULL; br = regnext(br))
    regoptail(br, ender);

  // Check for proper termination. 
  if (isInParen && *regparse ++ != R_(')'))
    throw Regexp::InvalidRegexp("unterminated ()");
  else if (!isInParen && *regparse != R_char_0)
  {
    assert(*regparse == R_(')'));
    throw Regexp::InvalidRegexp("unmatched ()");
  }
  
  return ret;
}

// regbranch - one alternative of an | operator
//             Implements the concatenation operator.
R_char *CRegCompilerBase::regbranch(int *flags_r)
  throw (Regexp::InvalidRegexp)
{
  *flags_r = WORST;	// Tentatively. 

  R_char *ret = regnode(BRANCH);
  R_char *chain = NULL;
  
  R_char c;
  while ((c = *regparse) != R_char_0 && c != R_('|') && c != R_(')'))
  {
    int flags;
    R_char *latest = regpiece(&flags);
    assert(latest);
    *flags_r |= flags & HASWIDTH;
    if (chain == NULL)		// First piece. 
      *flags_r |= flags & SPSTART;
    else
      regtail(chain, latest);
    chain = latest;
  }
  if (chain == NULL)		// Loop ran zero times. 
    regnode(NOTHING);
  
  return ret;
}

// regpiece - something followed by possible [*+?]
//
// Note that  the branching code sequences used for  ? and the general
// cases of * and +  are somewhat optimized: they use the same NOTHING
// node  as both the endmarker for  their branch list and  the body of
// the last  branch.  It might seem that this  node could be dispensed
// with entirely, but the endmarker role is not redundant.
R_char *CRegCompilerBase::regpiece(int *flags_r)
  throw (Regexp::InvalidRegexp)
{
  int flags;
  R_char *ret = regatom(&flags);
  assert(ret);

  R_char op = *regparse;
  if (!(op == R_('*') || op == R_('+') || op == R_('?')))
  {
    *flags_r = flags;
    return ret;
  }

  if (!(flags & HASWIDTH) && op != R_('?'))
    throw Regexp::InvalidRegexp("*+ operand could be empty");
  switch (op)
  {
    case R_('*'): *flags_r = WORST | SPSTART; break;
    case R_('+'): *flags_r = WORST | SPSTART | HASWIDTH; break;
    case R_('?'): *flags_r = WORST; break;
  }

  if (op == R_('*') && (flags & SIMPLE))
    reginsert(STAR, ret);
  else if (op == R_('*'))
  {
    // Emit x* as (x&|), where & means "self". 
    reginsert(BRANCH, ret);		// Either x 
    regoptail(ret, regnode(BACK));	// and loop 
    regoptail(ret, ret);		// back 
    regtail(ret, regnode(BRANCH));	// or 
    regtail(ret, regnode(NOTHING));	// null. 
  }
  else if (op == R_('+') && (flags & SIMPLE))
    reginsert(PLUS, ret);
  else if (op == R_('+'))
  {
    // Emit x+ as x(&|), where & means "self". 
    PROGRAM *next = regnode(BRANCH);	// Either 
    regtail(ret, next);
    regtail(regnode(BACK), ret);	// loop back 
    regtail(next, regnode(BRANCH));	// or 
    regtail(ret, regnode(NOTHING));	// null. 
  }
  else if (op == R_('?'))
  {
    // Emit x? as (x|) 
    reginsert(BRANCH, ret);		// Either x 
    regtail(ret, regnode(BRANCH));	// or 
    R_char *next = regnode(NOTHING);	// null. 
    regtail(ret, next);
    regoptail(ret, next);
  }
  regparse ++;
  op = *regparse;
  if (op == R_('*') || op == R_('+') || op == R_('?'))
    throw Regexp::InvalidRegexp("nested *?+");
  
  return ret;
}

// returns true if ANYOFC or ANYBUTC
bool CRegCompilerBase::regany(int mode)
  throw (Regexp::InvalidRegexp)
{
  // mode: 0: is complex ?
  //       1: non-complex chars
  //       2: complex ranges

  const R_char *&p = regparse;
  const R_char *next;

  goto start;
  
  for (; *p != R_(']'); p = next)
  {
    start:
    
    if (*p == R_char_0)
      goto unmatched;

    if (*p == R_('\\'))
      p ++;
    next = R_strinc(p);
    if (!next)
      goto invalid_string;

    if (*next == R_('-')) // range
    {
      const R_char *from = p;
      size_t fromlen = next - from;
      const R_char *to = R_strinc(next);
      if (!to)
	goto invalid_string;
      next = R_strinc(to);
      if (!next)
	goto invalid_string;
      
	
      if (*to != R_(']'))	// [...X-] is not range
      {
	if (*to == R_('\\'))	// escape
	{
	  to = next;
	  next = R_strinc(next);
	  if (!next)
	    goto invalid_string;
	}
	size_t tolen = next - to;
	
	if (*to == R_char_0)
	  goto unmatched;
	if (R_ismbblead(*from) || R_ismbblead(*to) ||
#if 1 < sizeof_R_char
	    (*from <= *to && 256 <= *to - *from)
#else
	    false
#endif
	  )
	{
	  if (tolen < fromlen)
	    goto invalid_range;
	  if (tolen == fromlen)
	    if (0 <= R_strncmp(from, to, tolen))
	      goto invalid_range;
	  if (mode == 0)
	    return true;	// complex
	  if (mode == 2)
	  {
	    regc(from, from + fromlen);
	    regc(to, to + tolen);
	  }
	}
	else
	{
	  if (*to < *from)
	    goto invalid_range;
	  if (mode == 1)
	    for (R_char i = *from; i <= *to; i ++)
	      regc(i);
	}
	continue;
      }
    }
    
    // a char
    if (mode == 1)
      regc(p, next);
  }
  if (mode == 1 || mode == 2)
    regc(R_char_0);
  p ++;
  return false;	// not complex

  invalid_string:
  throw Regexp::InvalidRegexp("invalid string(1)");
  invalid_range:
  throw Regexp::InvalidRegexp("invalid [] range");
  unmatched:
  throw Regexp::InvalidRegexp("unmatched []");
}

// regatom - the lowest level
//
// Optimization: gobbles an  entire sequence of ordinary characters so
// that it can turn them into a single node, which is smaller to store
// and  faster to  run.  Backslashed  characters are  exceptions, each
// becoming a separate node; the code is simpler that way and it's not
// worth fixing.
R_char *CRegCompilerBase::regatom(int *flags_r)
  throw (Regexp::InvalidRegexp)
{
  R_char *ret;

  *flags_r = WORST;	// Tentatively.
  
  switch (*regparse ++)
  {
    // FIXME: these chars only have meaning at beg/end of pat?
    case R_('^'):
      ret = regnode(BOL);
      break;
    case R_('$'):
      ret = regnode(EOL);
      break;
    case R_('.'):
      ret = regnode(ANY);
      *flags_r |= HASWIDTH | SIMPLE;
      break;
    case R_('['):
    {
      int op = ANYOF;
      if (*regparse == R_('^'))	// Complement of range. 
      {
	op = ANYBUT;
	regparse ++;
      }
      const R_char *start = regparse;
      bool isComplex = regany(0);
      if (isComplex)
	// complex
	op = (op == ANYOF) ? ANYOFC : ANYBUTC;
      ret = regnode(op);
      regparse = start;
      regany(1);
      if (isComplex)
      {
	regparse = start;
	regany(2);
      }
      *flags_r |= HASWIDTH | SIMPLE;
      break;
    }
    case R_('('):
    {
      int flags;
      ret = reg(true, &flags);
      assert(ret);
      *flags_r |= flags & (HASWIDTH | SPSTART);
      break;
    }
    case R_char_0: // '\0'
    case R_('|'):
    case R_(')'):
      // supposed to be caught earlier
      throw Regexp::InvalidRegexp("internal error: \\0|) unexpected");
    case R_('?'):
    case R_('+'):
    case R_('*'):
      throw Regexp::InvalidRegexp("?+* follows nothing");
    case R_('\\'):
      switch (*regparse ++)
      {
	case R_char_0:
	  throw Regexp::InvalidRegexp("trailing \\");
//	case R_('<'):
//	  ret = regnode(WORDA);
//	  break;
//	case R_('>'):
//	  ret = regnode(WORDZ);
//	  break;
	case R_('b'):
	  ret = regnode(WORDB);
	  break;
	case R_('B'):
	  ret = regnode(NONWORDB);
	  break;
	case R_('w'):
	  ret = regnode(WORDC); *flags_r |= HASWIDTH | SIMPLE; break;
	case R_('W'):
	  ret = regnode(NONWORDC); *flags_r |= HASWIDTH | SIMPLE; break;
	case R_('s'):
	  ret = regnode(WS); *flags_r |= HASWIDTH | SIMPLE; break;
	case R_('S'):
	  ret = regnode(NONWS); *flags_r |= HASWIDTH | SIMPLE; break;
	case R_('d'):
	  ret = regnode(DIGIT); *flags_r |= HASWIDTH | SIMPLE; break;
	case R_('D'):
	  ret = regnode(NONDIGIT); *flags_r |= HASWIDTH | SIMPLE; break;
	  // FIXME: Someday handle \1, \2, ...
	default:
	  // Handle general quoted chars in exact-match routine
	  goto de_fault;
      }
      break;
    de_fault:
    default:
      // Encode a string of characters to be matched exactly.
      //
      // This is a bit tricky due to quoted chars and due to '*', '+',
      // and '?' taking the SINGLE char previous as their operand.
      //
      // On entry,  the char at regparse[-1]  is going to  go into the
      // string, no matter what it is.   (It could be following a \ if
      // we are entered from the '\' case.)
      //
      // Basic idea  is to pick up a  good char in ch  and examine the
      // next char.  If  it's *+? then we twiddle.  If  it's \ then we
      // frozzle.  If it's  other magic char we push  ch and terminate
      // the string.  If  none of the above, we push  ch on the string
      // and go around again.
      //
      // regprev  is  used  to   remember  where  "the  current  char"
      // starts  in the  string, if  due  to a  *+?  we  need to  back
      // up and  put the current char  in a separate,  1-char, string.
      // When  regprev  is  NULL,  ch  is  the  only  char  in  the
      // string;  this  is  used  in  *+?  handling,  and  in  setting
      // flags |= SIMPLE at the end.
    {
      const R_char *regprev;

      regparse --;			// Look at cur char
      ret = regnode(EXACTLY);
      for (regprev = NULL; ; )
      {
	const R_char *p = regparse;
	R_char ch = *regparse;		// get current char
	regparse = R_strinc(regparse);
	if (!regparse)
	  throw Regexp::InvalidRegexp("invalid string(2)");
	switch (*regparse)		// look at next one
	{
	  default:
	    regc(p, regparse);		// add current char to string
	    break;
	  case R_('.'): case R_('['): case R_('('):
	  case R_(')'): case R_('|'): // case R_('\n'):
	  case R_('$'): case R_('^'):
	  case R_char_0:
	  {
	    // FIXME, $ and ^ should not always be magic
	    magic:
	    regc(p, regparse);	// dump cur char
	    goto done;		// and we are done
	  }
	  
	  case R_('?'): case R_('+'): case R_('*'):
	    if (!regprev)	// If just ch in str,
	      goto magic;	// use it
	    // End multi-char string one early
	    regparse = regprev;	// Back up parse
	    goto done;

	  case R_('\\'):
	    regc(p, regparse);		// Cur char OK
	    switch (regparse[1])	// Look after \ 
	    {
	      case R_char_0:
//	      case R_('<'): case R_('>'):
	      case R_('b'): case R_('B'):
	      case R_('w'): case R_('W'):
	      case R_('s'): case R_('S'):
	      case R_('d'): case R_('D'):
		// FIXME: Someday handle \1, \2, ...
		goto done;	// Not quoted
	      default:
		// Backup point is \, scan point is after it.
		regprev = regparse;
		regparse ++;
		continue;	// NOT break;
	    }
	}
	regprev = regparse;	// Set backup point
      }
      done:
      regc(R_char_0);
      *flags_r |= HASWIDTH;
      if (!regprev)		// One char?
	*flags_r |= SIMPLE;
      break;
    }
  }

  return ret;
}

// ////////////////////////////////////////////////////////////////////////////
// regexec and friends

// Work-variable struct for regexec().

class CRegExecutor
{
  friend bool Regexp::regexp::regexec(const R_char *str);

  const R_char *reginput;	// String-input pointer. 
  const R_char *regbol;		// Beginning of input, for ^ check. 
  const R_char **regbeginp;	// Pointer to beginp array. 
  const R_char **regendp;	// Ditto for endp.

  Regexp::regexp *prog;
public:
  CRegExecutor(Regexp::regexp *prog, R_char *target);
protected:
  bool regtry(R_char *target) throw (Regexp::InvalidRegexp);
  bool regmatch(PROGRAM *prog) throw (Regexp::InvalidRegexp);
  const R_char *regrepeat(PROGRAM *node) throw (Regexp::InvalidRegexp);
};

::CRegExecutor::CRegExecutor(Regexp::regexp *p, R_char *target)
  : regbol(target),
    regbeginp(p->beginp),
    regendp(p->endp),
    prog(p)
{
}

// regexec - match a regexp against a string
bool Regexp::regexp::regexec(const R_char *target)
  throw (Regexp::InvalidRegexp)
{
  // Check validity of program.
  assert(*program == MAGIC);
  assert(target);
  
  R_char *t = (R_char *)target;    // avert const poisoning

  numSubs = 1;
  
  // If there is a "must appear" string, look for it. 
  if (regmust != NULL && R_strstr(t, regmust) == NULL)
    return false;

  CRegExecutor executor(this, t);

  // Simplest case:  anchored match need be tried only once. 
  if (reganch)
    return executor.regtry(t);

  // Messy cases:  unanchored match. 
  if (regstart != R_char_0)
  {
    // We know what R_char it must start with. 
    for (R_char *
	   s = R_strchr(t, regstart); s != NULL; s = R_strchr(s , regstart))
    {
      if (executor.regtry(s))
	return true;
      s = R_strinc(s);
      if (!s)
	goto invalid_string;
    }
    return false;
  }
  else
  {
    // We don't -- general case.
    R_char *s;
    for (s = t; s; s = R_strinc(s))
    {
      if (executor.regtry(s))
	return true;
      if (*s == R_char_0)
	return false;
    }
    if (s)
      return false;
    else
      goto invalid_string;
  }
  invalid_string:
  throw Regexp::InvalidRegexp("invalid string(3)");
}

// regtry - try match at specific point
bool CRegExecutor::regtry(R_char *target)
  throw (Regexp::InvalidRegexp)
{
  reginput = target;
  prog->numSubs = 1;

  size_t i;
  for (i = 0; i < Regexp::NSUBEXP; i ++)
  {
    prog->beginp[i] = NULL;
    prog->endp[i] = NULL;
  }
  if (regmatch(prog->program + 1))
  {
    prog->beginp[0] = target;
    prog->endp[0] = reginput;
    
    for (prog->numSubs = 0; prog->numSubs < Regexp::NSUBEXP; prog->numSubs ++)
      if (!prog->beginp[i])
	break;
    return true;
  }
  else
    return false;
}

// regmatch - main matching routine
//
// Conceptually  the  strategy is  simple:  check to  see whether  the
// current node matches, call self recursively to see whether the rest
// matches, and then act accordingly.  In practice we make some effort
// to avoid recursion, in particular by going through "ordinary" nodes
// (that don't need to know whether the rest of the match failed) by a
// loop instead of by recursion.
bool CRegExecutor::regmatch(R_char *prog)
  throw (Regexp::InvalidRegexp)
{
  const R_char *prevc = reginput;	// previous char
  PROGRAM *next;			// next node.
  for (PROGRAM *scan = prog; scan != NULL; scan = next)
  {
    if (!reginput)
      throw Regexp::InvalidRegexp("invalid string(4)");
    
    next = regnext(scan);	// next node.

    switch (OP(scan))
    {
      case BOL:
	if (reginput != regbol)
	  return false;
	break;
      case EOL:
	if (*reginput != R_char_0)
	  return false;
	break;
//        case WORDA:
//  	// Must be looking at a letter, digit, or _
//  	// Prev must be BOL or nonword
//  	if (!(R_isword(*reginput) &&
//  	      (reginput == regbol || !R_isword(*prevc))))
//  	  return false;
//  	break;
//        case WORDZ:
//  	// Must be looking at non letter, digit, or _
//  	if (R_isword(*reginput))
//  	  return false;
//  	// We don't care what the previous char was
//	break;
      case ANY:
	if (*reginput == R_char_0)
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      case WORDB:
	if (R_isword(*reginput))	// \W\w, ^\w
	{
	  if (!(regbol == reginput || !R_isword(*prevc)))
	    return false;
	}
	else	// \w\W, \w$
	{
	  if (!(regbol < reginput && R_isword(*prevc)))
	    return false;
	}
	break;
      case WORDC:
	if (!R_isword(*reginput) || *reginput == R_char_0)
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      case NONWORDC:
	if (R_isword(*reginput) ||  *reginput == R_char_0)
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      case WS:
	if (!R_isspace(*reginput) || *reginput == R_char_0)
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      case NONWS:
	if (R_isspace(*reginput) || *reginput == R_char_0)
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      case DIGIT:
	if (!R_isdigit(*reginput) || *reginput == R_char_0)
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      case NONDIGIT:
	if (R_isdigit(*reginput) || *reginput == R_char_0)
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      case EXACTLY:
      {
	size_t len;
	R_char * const opnd = OPERAND(scan);
	// Inline the first character, for speed. 
	if (*opnd != *reginput)
	  return false;
	len = R_strlen(opnd);
	if (1 < len && R_strncmp(opnd, reginput, len) != 0)
	  return false;
	reginput += len;
	prevc = R_strdec(regbol, reginput);
	break;
      }
      
      case ANYOF:
	if (*reginput == R_char_0)
	  return false;
	if (!R_strmchr(OPERAND(scan), reginput))
	  // multi byte R_strchr(R_char *, const R_char *)
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      case ANYBUT:
	if (*reginput == R_char_0)
	  return false;
	if (R_strmchr(OPERAND(scan), reginput))
	  // multi byte R_strchr(R_char *, const R_char *)
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      
      case ANYOFC:
      {
	if (*reginput == R_char_0)
	  return false;
	R_char *opnd = (R_char *)OPERAND(scan);
	if (!R_strmchr(opnd, reginput))
	  // multi byte R_strchr(R_char *, const R_char *)
	{
	  R_char *r = opnd + R_strlen(opnd) + 1;
	  if (!R_strrange(r, reginput))
	    return false;
	}
	reginput = R_strinc(prevc = reginput);
	break;
      }
      
      case ANYBUTC:
      {
	if (*reginput == R_char_0)
	  return false;
	R_char *opnd = (R_char *)OPERAND(scan);
	if (R_strmchr(opnd, reginput))
	  // multi byte R_strchr(R_char *, const R_char *)
	  return false;
	R_char *r = opnd + R_strlen(opnd) + 1;
	if (R_strrange(r, reginput))
	  return false;
	reginput = R_strinc(prevc = reginput);
	break;
      }
      
      case NOTHING:
	break;
      case BACK:
	break;

      case BRANCH:
      {
	const R_char *save = reginput;
	if (OP(next) == BRANCH)
	{
	  while (OP(scan) == BRANCH)
	  {
	    if (regmatch(OPERAND(scan)))
	      return true;
	    reginput = save;
	    scan = regnext(scan);
	  }
	  return false;
	}
	next = OPERAND(scan);		// Avoid recursion. 
	break;
      }
      case STAR: case PLUS:
      {
	R_char nextch = (OP(next) == EXACTLY) ? *OPERAND(next) : R_char_0;
	const R_char *save = reginput;
	const R_char *min = (OP(scan) == STAR) ? reginput : R_strinc(reginput);
	if (!min)
	  throw Regexp::InvalidRegexp("invalid string(5)");

	const R_char *i;
	for (i = regrepeat(OPERAND(scan)); min <= i; i = R_strdec(min, i))
	{
	  reginput = i;
	  // If it could work, try it. 
	  if (nextch == R_char_0 || *reginput == nextch)
	    if (regmatch(next))
	      return true;
	  if (min == i)
	    break;
	}
	if (!i)
	  throw Regexp::InvalidRegexp(R_string("invalid string(6)"));
	return false;
      }
      case END:
	return true;	// Success!
      default:
      {
	R_char op = OP(scan);
	if (OPEN + 1 <= op && op < OPEN + Regexp::NSUBEXP)
	{
	  const R_char *input = reginput;
	  int no = OP(scan) - OPEN;
	  if (regmatch(next))
	  {
	    // Don't set beginp if some later invocation of the
	    // same parentheses already has.
	    if (regbeginp[no] == NULL)
	      regbeginp[no] = input;
	    return true;
	  }
	  return false;
	}
	else if (CLOSE + 1 <= op && op <= CLOSE + Regexp::NSUBEXP - 1)
	{
	  const R_char *input = reginput;
	  int no = OP(scan) - CLOSE;
	  if (regmatch(next))
	  {
	    // Don't set endp if some later invocation of the
	    // same parentheses already has.
	    if (regendp[no] == NULL)
	      regendp[no] = input;
	    return true;
	  }
	  return false;
	}
	else
	{
	  assert(!"regexp corruption");
	  return false;
	}
      }
    }
  }

  // We get here only if there's trouble -- normally "case END"
  // is the terminating point.
  
  assert(!"corrupted pointers");
  return false;
}

// regrepeat - report how many times something simple would match
const R_char *CRegExecutor::regrepeat(PROGRAM *node)
  throw (Regexp::InvalidRegexp)
{
  const R_char *scan = reginput;
  switch (OP(node))
  {
    case ANY:
      return R_strlen(reginput) + reginput;
    case WORDC:
      while (*scan != R_char_0 && R_isword(*scan))
	scan ++;
      return scan;
    case NONWORDC:
      while (*scan != R_char_0 && !R_isword(*scan))
	scan ++;
      return scan;
    case WS:
      while (*scan != R_char_0 && R_isspace(*scan))
	scan ++;
      return scan;
    case NONWS:
      while (*scan != R_char_0 && !R_isspace(*scan))
	scan ++;
      return scan;
    case DIGIT:
      while (*scan != R_char_0 && R_isdigit(*scan))
	scan ++;
      return scan;
    case NONDIGIT:
      while (*scan != R_char_0 && !R_isdigit(*scan))
	scan ++;
      return scan;
    case EXACTLY:
    {
      R_char c = *OPERAND(node);
      if (R_ismbblead(c))
      {
	size_t len = R_strlen(OPERAND(node));
	while (*scan != R_char_0 && R_strncmp(scan, OPERAND(node), len) == 0)
	  scan += len;
      }
      else
      {
	while (*scan != R_char_0 && *scan == c)
	  scan ++;
      }
      return scan;
    }
    case ANYOF:
    {
      R_char *p = R_strspnp(reginput, (R_char *)OPERAND(node));
      return p ? p : reginput + R_strlen(reginput);
    }
    case ANYBUT:
    {
      R_char *p = R_strpbrk(reginput, (R_char *)OPERAND(node));
      return p ? p : reginput + R_strlen(reginput);
    }
    case ANYOFC:
    {
      R_char *opnd = OPERAND(node);
      R_char *range = opnd + R_strlen(opnd) + 1;
      for (; scan; scan = R_strinc(scan))
      {
	R_char *p = R_strspnp(scan, opnd);
	scan = p ? p : scan + R_strlen(scan);
	if (*scan == R_char_0 || !R_strrange(range, scan))
	  return scan;
      }
      throw Regexp::InvalidRegexp("invalid string(7)");
    }
    case ANYBUTC:
    {
      R_char *opnd = OPERAND(node);
      R_char *range = opnd + R_strlen(opnd) + 1;
      for (; scan; scan = R_strinc(scan))
      {
	R_char *p = R_strpbrk(scan, opnd);
	scan = p ? p : scan + R_strlen(scan);
	if (*scan == R_char_0 || R_strrange(range, scan))
	  return scan;
      }
      throw Regexp::InvalidRegexp("invalid string(8)");
    }
    default:	// Oh dear.  Called inappropriately.
      assert(!"internal error: bad call of regrepeat");
      throw Regexp::InvalidRegexp("internal error: bad call of regrepeat");
  }
}


// ////////////////////////////////////////////////////////////////////////////
// Regexp::Regexp


// constructor
Regexp::Regexp()
  : target(NULL), rc(NULL)
{
}

// constructor
Regexp::Regexp(const Regexp &o)
  : target(o.target), rc(o.rc)
{
  if (rc)
    rc->refer();
}

// constructor
Regexp::Regexp(const R_char *exp, bool doesIgnoreCase)
  throw (Regexp::InvalidRegexp)
  : target(NULL), rc(new regexp(exp, doesIgnoreCase))
{
}

// constructor
Regexp::Regexp(const R_string &exp, bool doesIgnoreCase)
  throw (Regexp::InvalidRegexp)
  : target(NULL), rc(new regexp(exp.c_str(), doesIgnoreCase))
{
}

// destructor
Regexp::~Regexp()
{
  if (rc.ptr && rc->unrefer() == 0)
    rc.free();
  else
    rc = NULL;
}

// compile
void Regexp::compile(const R_char *exp, bool doesIgnoreCase)
  throw (Regexp::InvalidRegexp)
{
  if (rc.ptr && rc->unrefer() == 0)
    rc.free();
  rc = new regexp(exp, doesIgnoreCase);
}

// operator=
Regexp &Regexp::operator=(const Regexp &o)
{
  if (this != &o)
  {
    if (rc.ptr && rc->unrefer() == 0)
      rc.free();
    
    rc = o.rc;
    if (rc)
      rc->refer();
    
    target = o.target;
  }
  return *this;
}

// does match ?
bool Regexp::doesMatch(const R_char *target_)
  throw (Regexp::InvalidRegexp)
{
  target = target_;
  bool r = false;
  assert(rc.ptr);
  assert(target);
  
  if (1 < rc->getRefcount()) // copy on write !
  {
    rc->unrefer();
    rc = rc->clone();
  }

#if 0
  try
  {
#endif
    r = rc->regexec(target);
#if 0
  }
  catch (InvalidRegexp &e)
  {
    printf("regexp execute error: %s: %s\n", e.what(), target_);
    throw;
  }
#endif
  return r;
}

// get number of sub strings
size_t Regexp::getNumberOfSubStrings() const
{
  assert(rc.ptr);
  return rc->getNumSubs();
}

// operator[]
R_string Regexp::operator[](size_t i) const
  throw (std::out_of_range)
{
  return R_string(rc->getBeginp(i), subSize(i));
}

// get beginning of substring indexed by i
size_t Regexp::subBegin(size_t i) const
  throw (std::out_of_range)
{
  assert(rc.ptr);
  return rc->getBeginp(i) - target;
}

// get end of substring indexed by i
size_t Regexp::subEnd(size_t i) const
  throw (std::out_of_range)
{
  assert(rc.ptr);
  return rc->getEndp(i) - target;
}

// get size of substring indexed by i
size_t Regexp::subSize(size_t i) const
  throw (std::out_of_range)
{
  assert(rc.ptr);
  return rc->getEndp(i) - rc->getBeginp(i);
}

// replace \1 \2 ... in source
R_string Regexp::replace(const R_char *source) const
{
  assert(rc.ptr);
  return rc->replace(source);
}

void Regexp::regexp::ignoreCase(const R_char *in, R_char *out)
{
  // copy in to out making every top level character a [Aa] set
  bool inRange = false;
  while (*in)
  {
    if (!inRange && R_isalpha(*in))
    {
      *out ++ = R_('[');
      *out ++ = (R_char)R_toupper(*in);
      *out ++ = (R_char)R_tolower(*in);
      *out ++ = R_(']');
      in ++;
    }
    else
    {
      if (*in == R_('['))
	inRange = true;
      else if (*in == R_(']'))
	inRange = false;
      if (R_ismbblead(*in))
	*out ++ = *in ++;
      *out ++ = *in ++;
    }
  }
  *out = R_char_0;
}

// replace - Converts a replace expression to a string
//                  - perform substitutions after a regexp match
// Returns          - The resultant string
R_string Regexp::regexp::replace(const R_char *sReplaceExp) const
{
  assert(sReplaceExp);
  
  R_char null = R_char_0;
  R_string szEmpty(&null);

  R_char *src = (R_char *)sReplaceExp;
  R_char *buf;
  R_char c;
  int no;
  size_t len;
  
  if (*program != MAGIC)
  {
    assert(!"damaged regexp fed to regsub");
    return szEmpty;
  }

  // First compute the length of the string
  int replacelen = 0;
  while ((c = *src++) != R_char_0) 
  {
    if (c == R_('&'))
      no = 0;
    else if (c == R_('\\') && R_isdigit(*src))
      no = *src++ - R_('0');
    else
      no = -1;

    if (no < 0) 
    {   
      // Ordinary character. 
      if (c == R_('\\') && (*src == R_('\\') || *src == R_('&')))
	c = *src++;
      replacelen++;
    } 
    else if (beginp[no] != NULL && endp[no] != NULL &&
	     endp[no] > beginp[no]) 
    {
      // Get tagged expression
      len = endp[no] - beginp[no];
      replacelen += len;
    }
  }

  R_string szReplace;

  buf = (R_char *)alloca((replacelen + 1) * sizeof(R_char));
  buf[0] = R_char_0;
  
  // Now we can create the string
  src = (R_char *)sReplaceExp;
  while ((c = *src++) != R_char_0) 
  {
    if (c == R_('&'))
      no = 0;
    else if (c == R_('\\') && R_isdigit(*src))
      no = *src++ - R_('0');
    else
      no = -1;

    if (no < 0) 
    {   
      // Ordinary character. 
      if (c == R_('\\') && (*src == R_('\\') || *src == R_('&')))
	c = *src++;
      *buf++ = c;
    } 
    else if (beginp[no] != NULL && endp[no] != NULL &&
	     endp[no] > beginp[no]) 
    {
      // Get tagged expression
      len = endp[no] - beginp[no];
      R_strncpy(buf, beginp[no], len);
      buf += len;
      if (len != 0 && *(buf-1) == R_char_0)
      {		// strncpy hit NUL.
	assert(!"damaged match string");
	return szEmpty;
      }
    }
  }

  buf[replacelen] = R_char_0;
  return R_string(buf);
}
