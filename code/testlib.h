/* testlib.h: TEST LIBRARY INTERFACE
 *
 * $Id$
 * Copyright (c) 2001-2014 Ravenbrook Limited.  See end of file for license.
 * Portions copyright (C) 2002 Global Graphics Software.
 *
 * .purpose: A library of functions that may be of use to unit tests.
 */

#ifndef testlib_h
#define testlib_h

#include "mps.h"
#include "misc.h" /* for STR */
#include "mpstd.h"


/* Suppress Visual C warnings at warning level 4, */
/* see mail.richard.1997-09-25.13-26 and job003715. */
/* Essentially the same settings are done in config.h. */

#ifdef MPS_BUILD_MV

/* "constant conditional" (MPS_END) */
#pragma warning(disable: 4127)

#endif /* MPS_BUILD_MV */


/* Suppress Pelles C warnings at warning level 2 */
/* This is also done in config.h. */

#ifdef MPS_BUILD_PC

/* "Unreachable code" (AVER, if condition is constantly true). */
#pragma warn(disable: 2154)

#endif /* MPS_BUILD_PC */


/* Function attributes */
/* These are also defined in config.h */

#if defined(MPS_BUILD_GC) || defined(MPS_BUILD_LL)

/* GCC: <http://gcc.gnu.org/onlinedocs/gcc/Function-Attributes.html#index-Wformat-2850>
 * Clang: <http://clang.llvm.org/docs/AttributeReference.html#format-gnu-format>
 */
#define ATTRIBUTE_FORMAT(ARGLIST) __attribute__((__format__ ARGLIST))

#else

#define ATTRIBUTE_FORMAT(ARGLIST)

#endif


/* alloca -- memory allocator
 *
 * Windows calls this function _alloca() instead of alloca().
 * <http://msdn.microsoft.com/en-us/library/wb1s57t5.aspx>
 */

#if defined(MPS_OS_W3)

#define alloca _alloca

#endif


/* ulongest_t -- longest unsigned integer type
 *
 * Define a longest unsigned integer type for testing, scanning, and
 * printing.  We'd like to use C99's uintmax_t and PRIuMAX here, but
 * the MPS is in C89 and C99 isn't supported by Microsoft.
 *
 * We avoid using the types defined in mpstd.h because we want the
 * tests to root out any incompatible assumptions by breaking.
 */

#if defined(MPS_ARCH_I6)
#define PRIwWORD "16"
#elif defined(MPS_ARCH_I3)
#define PRIwWORD "8"
#else
#error "How many beans make five?"
#endif

#if defined(MPS_OS_W3) && defined(MPS_ARCH_I6)
#define PRIuLONGEST "llu"
#define SCNuLONGEST "llu"
#define SCNXLONGEST "llX"
#define PRIXLONGEST "llX"
typedef unsigned long long ulongest_t;
typedef long long longest_t;
#define MPS_WORD_CONST(n) (n##ull)
#else
#define PRIuLONGEST "lu"
#define SCNuLONGEST "lu"
#define SCNXLONGEST "lX"
#define PRIXLONGEST "lX"
typedef unsigned long ulongest_t;
typedef long longest_t;
#define MPS_WORD_CONST(n) (n##ul)
#endif


#define PRIXPTR     "0"PRIwWORD PRIXLONGEST


/* testlib_unused -- declares that a variable is unused
 *
 * It should be used to prevent compiler warnings about unused
 * variables.  Care should be exercised; the fact that a variable
 * is unused may need justification.
 */

#define testlib_unused(v) ((void)(v))


/* max -- return larger value
 * 
 * Note: evaluates its arguments twice.
 */

#undef max
#define max(a, b) (((a) > (b)) ? (a) : (b))


/* alignUp -- align word to alignment */

#define alignUp(w, a) (((w) + (a) - 1) & ~((size_t)(a) - 1))


/* die -- succeed or die
 *
 * If the first argument is not ResOK then prints the second
 * argument on stderr and exits the program.  Otherwise does nothing.
 *
 * Typical use:
 *   die(mps_ap_create(&ap, pool, mps_rank_exact()), "APCreate");
 */

extern void die(mps_res_t res, const char *s);


/* die_expect -- get expected result or die
 *
 * If the first argument is not the same as the second argument,
 * prints the third argument on stderr and exits the program. 
 * Otherwise does nothing.
 *
 * Typical use:
 *   die_expect(res, MPS_RES_COMMIT_LIMIT, "Commit limit allocation");
 */

extern void die_expect(mps_res_t res, mps_res_t expected, const char *s);


/* cdie -- succeed or die
 *
 * If the first argument is not true (non-zero) then prints the second
 * argument on stderr and exits the program.  Otherwise does nothing.
 *
 * Typical use:
 *   cdie(foo != NULL, "No foo");
 */

extern void cdie(int res, const char *s);


/* assert_die -- always die on assertion
 *
 * The MPS assertion handler may not stop in the HOT variety,
 * preventing tests from detecting defects.  This one does.
 */

void assert_die(const char *file, unsigned line, const char *condition);


/* error, verror -- die with message */

ATTRIBUTE_FORMAT((printf, 1, 2))
extern void error(const char *format, ...);
ATTRIBUTE_FORMAT((printf, 1, 0))
extern void verror(const char *format, va_list args);


/* Insist -- like assert, but even in release varieties */

#define Insist(cond) insist1(cond, #cond)

#define insist1(cond, condstring) \
  if(cond) \
    NOOP; \
  else \
    cdie(cond, condstring "\n" __FILE__ "\n" STR(__LINE__))


/* fail -- like assert, but (notionally) returns a value, so usable in an expression */

extern int fail(void);


/* rnd -- random number generator
 *
 * rnd() generates a sequence of integers in the range [1, 2^31-2].
 */

extern unsigned long rnd(void);
typedef unsigned long rnd_state_t;
extern rnd_state_t rnd_state(void);
extern void rnd_state_set(rnd_state_t state_v3);
extern void rnd_state_set_v2(rnd_state_t seed0_v2);  /* legacy */
extern rnd_state_t rnd_seed(void);


/* rnd_verify() -- checks behaviour of rnd() */
extern void rnd_verify(int depth);


/* rnd_addr -- random number generator
 *
 * rnd_addr() generates a sequence of addresses all over the address space.
 */

extern mps_addr_t rnd_addr(void);


/* rnd_double -- uniformly distributed random number between 0.0 and 1.0 */

extern double rnd_double(void);


/* randomize -- randomize the generator, or initialize to replay
 *
 * randomize(argc, argv) randomizes the rnd generator (using time(3))
 * and prints out the randomization seed, or takes a seed (as a command-
 * line argument) and initializes the generator to the same state.
 */

extern void randomize(int argc, char *argv[]);


/* testlib_init -- install assertion handler and seed RNG */

extern void testlib_init(int argc, char *argv[]);


#endif /* testlib_h */


/* C. COPYRIGHT AND LICENSE
 *
 * Copyright (C) 2001-2014 Ravenbrook Limited <http://www.ravenbrook.com/>.
 * All rights reserved.  This is an open source license.  Contact
 * Ravenbrook for commercial licensing options.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * 3. Redistributions in any form must be accompanied by information on how
 * to obtain complete source code for this software and any accompanying
 * software that uses this software.  The source code must either be
 * included in the distribution or be available for no more than the cost
 * of distribution plus a nominal fee, and must be freely redistributable
 * under reasonable conditions.  For an executable file, complete source
 * code means the source code for all modules it contains. It does not
 * include source code for modules or files that typically accompany the
 * major components of the operating system on which the executable file
 * runs.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
