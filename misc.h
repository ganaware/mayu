// ////////////////////////////////////////////////////////////////////////////
// misc.h


#ifndef _MISC_H
#  define _MISC_H

#  include "compiler_specific.h"

#  include <windows.h>
#  include <assert.h>


typedef unsigned char u_char;		/// unsigned char
typedef unsigned short u_short;		/// unsigned short
typedef unsigned long u_long;		/// unsigned long

typedef char int8;
typedef short int16;
typedef long int32;
typedef unsigned char u_int8;
typedef unsigned short u_int16;
typedef unsigned long u_int32;

#  ifdef NDEBUG
#    define ASSERT(exp)		exp
#    define CHECK(cond, exp)	exp
#    define CHECK_TRUE(exp)	exp
#    define CHECK_FALSE(exp)	exp
#  else // NDEBUG
#    define ASSERT(exp)		assert(exp)
#    define CHECK(cond, exp)	assert(cond (exp))
#    define CHECK_TRUE(exp)	assert(!!(exp))
#    define CHECK_FALSE(exp)	assert(!(exp))
#  endif // NDEBUG


/// get number of array elements
#  define NUMBER_OF(a) (sizeof(a) / sizeof((a)[0]))

/// max path length
#  define GANA_MAX_PATH		(MAX_PATH * 4)

/// max length of global atom
#  define GANA_MAX_ATOM_LENGTH	256

/// max
#  undef MAX
#  define MAX(a, b)	(((b) < (a)) ? (a) : (b))

/// min
#  undef MIN
#  define MIN(a, b)	(((a) < (b)) ? (a) : (b))


#endif // _MISC_H
