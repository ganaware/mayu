///////////////////////////////////////////////////////////////////////////////
// misc.h


#ifndef __misc_h__
#define __misc_h__


#include "compiler_specific.h"

#include <windows.h>
#include <assert.h>


typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;

#define lengthof(a) (sizeof(a) / sizeof((a)[0]))

#ifdef NDEBUG
#define _must_be(exp, op, v) exp
#else // NDEBUG
#define _must_be(exp, op, v) assert((exp) op (v))
#endif // NDEBUG

#define _true(exp)  _must_be(!!(exp), ==, true)
#define _false(exp) _true(!(exp))
#define _NULL(exp)  _false(exp)
#define _not_NULL(exp)  _true(exp)

// misc max length

#define GANA_MAX_PATH		(MAX_PATH * 4)	// path
#define GANA_MAX_ATOM_LENGTH	256		// global atom

// max/min

#define MAX(a, b)	(((b) < (a)) ? (a) : (b))
#define MIN(a, b)	(((a) < (b)) ? (a) : (b))


#endif // __misc_h__
