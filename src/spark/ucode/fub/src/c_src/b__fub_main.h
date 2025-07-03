#ifndef STANDARD_H
#define STANDARD_H

#ifdef __vxworks
#include "vxWorks.h"
#endif

/* stdint.h is only guaranteed to be there starting with C99 */
#if __STDC_VERSION__ >= 199901L
#include <stdint.h>   /* for [u]int*_t */
#endif

#include <string.h>
/* for memcpy(), memcmp(), used to implement assignments and comparisons
   between composite objects.  */
#include <math.h>
/* for fabs*() - Ada abs operator on floating point
   isfinite() - 'Valid on floating point
   powl() - "**" operator on floating point
   Ada.Numerics package hierarchy.  */

#ifndef __USE_ISOC99
#define __USE_ISOC99
/* for enabling llabs() - implementation of abs operator on long long int */
#endif
#include <stdlib.h>
/* for *abs() - abs operator on integers
   malloc()  - Dynamic memory allocation via "new".  */

/* Definition of Standard.Boolean type and True/False values, as well as
   definition of the GNAT_INLINE qualifier */

#if __STDC_VERSION__ >= 199901L
#include <stdbool.h>

#define GNAT_INLINE inline
typedef bool boolean;

#else

#define GNAT_INLINE
typedef unsigned char boolean;

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif
#endif

/* Definition of various C extensions needed to support alignment and packed
   clauses, as well as pragma Linker_Section and Machine_Attribute.
   These are supported out of the box with a GNU C compatible compiler, and
   are defined by default as no-op otherwise.  */

#ifdef __GNUC__
#define GNAT_ALIGN(ALIGNMENT) __attribute__((aligned(ALIGNMENT)))
#define GNAT_PACKED __attribute__ ((packed))
#define GNAT_LINKER_SECTION(SECTION) __attribute__((section(SECTION)))
#define GNAT_ATTRIBUTE(ATTR) __attribute__((ATTR))
#define GNAT_ATTRIBUTE_INFO(ATTR,INFO) __attribute__((ATTR(INFO)))
#else
#define GNAT_ALIGN(ALIGNMENT)
#define GNAT_PACKED
#define GNAT_LINKER_SECTION(SECTION)
#define GNAT_ATTRIBUTE(ATTR)
#define GNAT_ATTRIBUTE_INFO(ATTR,INFO)
#endif

/* Definition of entities needed by the code generator to define base types */

#ifdef INT8_MIN  /* stdint.h supported */
typedef uint8_t unsigned_8;
typedef uint16_t unsigned_16;
typedef uint32_t unsigned_32;
typedef uint64_t unsigned_64;

typedef int8_t integer_8;
typedef int16_t integer_16;
typedef int32_t integer_32;
typedef int64_t integer_64;

typedef intptr_t integer_ptr_t;
#else
typedef unsigned char unsigned_8;
typedef unsigned short unsigned_16;
typedef unsigned unsigned_32;
typedef unsigned long long unsigned_64;

typedef signed char integer_8;
typedef short integer_16;
typedef int integer_32;
typedef long long integer_64;

typedef long integer_ptr_t;
#endif

/* Definition of entities defined in package Standard */

typedef int integer;

typedef int natural;
typedef int positive;

typedef signed char short_short_integer;
typedef short short_integer;
typedef long long_integer;
typedef long long long_long_integer;
typedef long long universal_integer;

typedef float short_float;
typedef double long_float;
typedef long double long_long_float;
typedef long double universal_real;

typedef unsigned char character;
typedef unsigned_16 wide_character;
typedef unsigned_32 wide_wide_character;

typedef character *access_character;
typedef character *string;
typedef wide_character *wide_string;
typedef wide_wide_character *wide_wide_string;

typedef integer_32 duration;

/* Definitions needed by exception handling */

typedef void *_void_ptr;
extern void __gnat_last_chance_handler (const _void_ptr file, integer line);

#ifndef GNAT_LAST_CHANCE_HANDLER
#define GNAT_LAST_CHANCE_HANDLER(FILE,LINE) \
	__gnat_last_chance_handler(FILE, LINE)
#endif

/* Fat pointer of unidimensional unconstrained arrays */

typedef struct {
  void *all;
  integer_ptr_t first;
  integer_ptr_t last;
} _fatptr_UNCarray;

static GNAT_INLINE _fatptr_UNCarray
_fatptr_UNCarray_CONS (void *all, integer_ptr_t first, integer_ptr_t last)
{
  _fatptr_UNCarray fatptr;
  fatptr.all   = all;
  fatptr.first = first;
  fatptr.last  = last;
  return fatptr;
}

/* Support for 'Max and 'Min attributes on all integer sizes */

static GNAT_INLINE integer_8
_imax8(const integer_8 a, integer_8 b)
{
   return (b > a) ? b : a;
}

static GNAT_INLINE integer_16
_imax16(const integer_16 a, integer_16 b)
{
   return (b > a) ? b : a;
}

static GNAT_INLINE integer_32
_imax32(const integer_32 a, integer_32 b)
{
   return (b > a) ? b : a;
}

static GNAT_INLINE integer_64
_imax64(const integer_64 a, const integer_64 b)
{
   return (b > a) ? b : a;
}

static GNAT_INLINE integer_8
_imin8(const integer_8 a, const integer_8 b)
{
   return (b < a) ? b : a;
}

static GNAT_INLINE integer_16
_imin16(const integer_16 a, const integer_16 b)
{
   return (b < a) ? b : a;
}

static GNAT_INLINE integer_32
_imin32(const integer_32 a, const integer_32 b)
{
   return (b < a) ? b : a;
}

static GNAT_INLINE integer_64
_imin64(const integer_64 a, const integer_64 b)
{
   return (b < a) ? b : a;
}

static GNAT_INLINE unsigned_8
_umax8(const unsigned_8 a, unsigned_8 b)
{
   return (b > a) ? b : a;
}

static GNAT_INLINE unsigned_16
_umax16(const unsigned_16 a, unsigned_16 b)
{
   return (b > a) ? b : a;
}

static GNAT_INLINE unsigned_32
_umax32(const unsigned_32 a, unsigned_32 b)
{
   return (b > a) ? b : a;
}

static GNAT_INLINE unsigned_64
_umax64(const unsigned_64 a, const unsigned_64 b)
{
   return (b > a) ? b : a;
}

static GNAT_INLINE unsigned_8
_umin8(const unsigned_8 a, const unsigned_8 b)
{
   return (b < a) ? b : a;
}

static GNAT_INLINE unsigned_16
_umin16(const unsigned_16 a, const unsigned_16 b)
{
   return (b < a) ? b : a;
}

static GNAT_INLINE unsigned_32
_umin32(const unsigned_32 a, const unsigned_32 b)
{
   return (b < a) ? b : a;
}

static GNAT_INLINE unsigned_64
_umin64(const unsigned_64 a, const unsigned_64 b)
{
   return (b < a) ? b : a;
}

#endif /* STANDARD_H */

#ifndef SYSTEM_ADS
#define SYSTEM_ADS
extern short_integer system_E;
typedef integer_8 system__name;
enum {system__system_name_gnat=0};
typedef character system__TnameSS[16];
typedef character system__T1s[16];
typedef integer system__TTnameSSP1;
extern const system__TnameSS system__nameS;
typedef integer_8 *system__TnameNB;
typedef integer_8 system__T5s[2];
typedef integer_8 system__TnameNT[2];
typedef integer system__TnameND1;
extern const system__TnameNT system__nameN;
extern const system__name system__system_name;
typedef void* system__address;
extern const system__address system__null_address;
typedef integer_8 system__bit_order;
enum {system__high_order_first=0, system__low_order_first=1};
typedef character system__Tbit_orderSS[31];
typedef character system__T6s[31];
typedef integer system__TTbit_orderSSP1;
extern const system__Tbit_orderSS system__bit_orderS;
typedef integer_8 *system__Tbit_orderNB;
typedef integer_8 system__T10s[3];
typedef integer_8 system__Tbit_orderNT[3];
typedef integer system__Tbit_orderND1;
extern const system__Tbit_orderNT system__bit_orderN;
extern const system__bit_order system__default_bit_order;
extern const positive system__max_priority;
extern const positive system__max_interrupt_priority;
typedef integer system__any_priority;
typedef integer system__priority;
typedef integer system__interrupt_priority;
extern const system__priority system__default_priority;
extern const boolean system__backend_divide_checks;
extern const boolean system__backend_overflow_checks;
extern const boolean system__command_line_args;
extern const boolean system__configurable_run_time;
extern const boolean system__denorm;
extern const boolean system__duration_32_bits;
extern const boolean system__exit_status_supported;
extern const boolean system__fractional_fixed_ops;
extern const boolean system__frontend_layout;
extern const boolean system__machine_overflows;
extern const boolean system__machine_rounds;
extern const boolean system__preallocated_stacks;
extern const boolean system__signed_zeros;
extern const boolean system__stack_check_default;
extern const boolean system__stack_check_probes;
extern const boolean system__stack_check_limits;
extern const boolean system__support_aggregates;
extern const boolean system__support_composite_assign;
extern const boolean system__support_composite_compare;
extern const boolean system__support_long_shifts;
extern const boolean system__always_compatible_rep;
extern const boolean system__suppress_standard_library;
extern const boolean system__use_ada_main_program_name;
extern const boolean system__frontend_exceptions;
extern const boolean system__zcx_by_default;
#endif /* SYSTEM_ADS */

#ifndef B__FUB_MAIN_ADS
#define B__FUB_MAIN_ADS
extern short_integer ada_main_E;
#ifdef IN_ADA_BODY
short_integer ada_main_E = 0;
#endif /* IN_ADA_BODY */
extern integer ada_main__gnat_exit_status;
#ifdef IN_ADA_BODY
integer ada_main__gnat_exit_status = 0;
#endif /* IN_ADA_BODY */
typedef character ada_main__Tgnat_versionS[38];
typedef character ada_main__T1s[38];
typedef integer ada_main__TTgnat_versionSP1;
extern const ada_main__Tgnat_versionS __gnat_version;
#ifdef IN_ADA_BODY
const ada_main__Tgnat_versionS __gnat_version = "GNAT Version: Pro 20.0w (20190611-83)\x00";
#endif /* IN_ADA_BODY */
typedef character ada_main__Tada_main_program_nameS[14];
typedef character ada_main__T3s[14];
typedef integer ada_main__TTada_main_program_nameSP1;
extern const ada_main__Tada_main_program_nameS __gnat_ada_main_program_name;
#ifdef IN_ADA_BODY
const ada_main__Tada_main_program_nameS __gnat_ada_main_program_name = "_ada_fub_main\x00";
#endif /* IN_ADA_BODY */
extern void adainit(void);
extern integer main(void);
#endif /* B__FUB_MAIN_ADS */
