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

#ifndef LW_TYPES_GENERIC_ADS
#define LW_TYPES_GENERIC_ADS

#endif /* LW_TYPES_GENERIC_ADS */

#ifndef LW_TYPES_INTRINSIC_ADS
#define LW_TYPES_INTRINSIC_ADS
typedef unsigned_8 lw_types_intrinsic__lwu1_wrap;
typedef unsigned_8 lw_types_intrinsic__lwu2_wrap;
typedef unsigned_8 lw_types_intrinsic__lwu3_wrap;
typedef unsigned_8 lw_types_intrinsic__lwu4_wrap;
typedef unsigned_8 lw_types_intrinsic__lwu5_wrap;
typedef unsigned_8 lw_types_intrinsic__lwu6_wrap;
typedef unsigned_8 lw_types_intrinsic__lwu7_wrap;
typedef unsigned_8 lw_types_intrinsic__lwu8_wrap;
typedef unsigned_16 lw_types_intrinsic__lwu9_wrap;
typedef unsigned_16 lw_types_intrinsic__lwu10_wrap;
typedef unsigned_16 lw_types_intrinsic__lwu11_wrap;
typedef unsigned_16 lw_types_intrinsic__lwu12_wrap;
typedef unsigned_16 lw_types_intrinsic__lwu13_wrap;
typedef unsigned_16 lw_types_intrinsic__lwu14_wrap;
typedef unsigned_16 lw_types_intrinsic__lwu15_wrap;
typedef unsigned_16 lw_types_intrinsic__lwu16_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu17_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu18_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu19_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu20_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu21_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu22_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu23_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu24_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu25_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu26_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu27_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu28_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu29_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu30_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu31_wrap;
typedef unsigned_32 lw_types_intrinsic__lwu32_wrap;
typedef unsigned_64 lw_types_intrinsic__lwu64_wrap;
#endif /* LW_TYPES_INTRINSIC_ADS */

#ifndef LW_TYPES_ADS
#define LW_TYPES_ADS
typedef lw_types_intrinsic__lwu1_wrap lw_types__lwu1_types__base_type;
typedef lw_types_intrinsic__lwu1_wrap lw_types__lwu1_types__Tn_typeB;
typedef lw_types__lwu1_types__Tn_typeB lw_types__lwu1_types__n_type;
typedef lw_types_intrinsic__lwu2_wrap lw_types__lwu2_types__base_type;
typedef lw_types_intrinsic__lwu2_wrap lw_types__lwu2_types__Tn_typeB;
typedef lw_types__lwu2_types__Tn_typeB lw_types__lwu2_types__n_type;
typedef lw_types_intrinsic__lwu3_wrap lw_types__lwu3_types__base_type;
typedef lw_types_intrinsic__lwu3_wrap lw_types__lwu3_types__Tn_typeB;
typedef lw_types__lwu3_types__Tn_typeB lw_types__lwu3_types__n_type;
typedef lw_types_intrinsic__lwu4_wrap lw_types__lwu4_types__base_type;
typedef lw_types_intrinsic__lwu4_wrap lw_types__lwu4_types__Tn_typeB;
typedef lw_types__lwu4_types__Tn_typeB lw_types__lwu4_types__n_type;
typedef lw_types_intrinsic__lwu5_wrap lw_types__lwu5_types__base_type;
typedef lw_types_intrinsic__lwu5_wrap lw_types__lwu5_types__Tn_typeB;
typedef lw_types__lwu5_types__Tn_typeB lw_types__lwu5_types__n_type;
typedef lw_types_intrinsic__lwu6_wrap lw_types__lwu6_types__base_type;
typedef lw_types_intrinsic__lwu6_wrap lw_types__lwu6_types__Tn_typeB;
typedef lw_types__lwu6_types__Tn_typeB lw_types__lwu6_types__n_type;
typedef lw_types_intrinsic__lwu7_wrap lw_types__lwu7_types__base_type;
typedef lw_types_intrinsic__lwu7_wrap lw_types__lwu7_types__Tn_typeB;
typedef lw_types__lwu7_types__Tn_typeB lw_types__lwu7_types__n_type;
typedef lw_types_intrinsic__lwu8_wrap lw_types__lwu8_types__base_type;
typedef lw_types_intrinsic__lwu8_wrap lw_types__lwu8_types__Tn_typeB;
typedef lw_types__lwu8_types__Tn_typeB lw_types__lwu8_types__n_type;
typedef lw_types_intrinsic__lwu9_wrap lw_types__lwu9_types__base_type;
typedef lw_types_intrinsic__lwu9_wrap lw_types__lwu9_types__Tn_typeB;
typedef lw_types__lwu9_types__Tn_typeB lw_types__lwu9_types__n_type;
typedef lw_types_intrinsic__lwu10_wrap lw_types__lwu10_types__base_type;
typedef lw_types_intrinsic__lwu10_wrap lw_types__lwu10_types__Tn_typeB;
typedef lw_types__lwu10_types__Tn_typeB lw_types__lwu10_types__n_type;
typedef lw_types_intrinsic__lwu11_wrap lw_types__lwu11_types__base_type;
typedef lw_types_intrinsic__lwu11_wrap lw_types__lwu11_types__Tn_typeB;
typedef lw_types__lwu11_types__Tn_typeB lw_types__lwu11_types__n_type;
typedef lw_types_intrinsic__lwu12_wrap lw_types__lwu12_types__base_type;
typedef lw_types_intrinsic__lwu12_wrap lw_types__lwu12_types__Tn_typeB;
typedef lw_types__lwu12_types__Tn_typeB lw_types__lwu12_types__n_type;
typedef lw_types_intrinsic__lwu13_wrap lw_types__lwu13_types__base_type;
typedef lw_types_intrinsic__lwu13_wrap lw_types__lwu13_types__Tn_typeB;
typedef lw_types__lwu13_types__Tn_typeB lw_types__lwu13_types__n_type;
typedef lw_types_intrinsic__lwu14_wrap lw_types__lwu14_types__base_type;
typedef lw_types_intrinsic__lwu14_wrap lw_types__lwu14_types__Tn_typeB;
typedef lw_types__lwu14_types__Tn_typeB lw_types__lwu14_types__n_type;
typedef lw_types_intrinsic__lwu15_wrap lw_types__lwu15_types__base_type;
typedef lw_types_intrinsic__lwu15_wrap lw_types__lwu15_types__Tn_typeB;
typedef lw_types__lwu15_types__Tn_typeB lw_types__lwu15_types__n_type;
typedef lw_types_intrinsic__lwu16_wrap lw_types__lwu16_types__base_type;
typedef lw_types_intrinsic__lwu16_wrap lw_types__lwu16_types__Tn_typeB;
typedef lw_types__lwu16_types__Tn_typeB lw_types__lwu16_types__n_type;
typedef lw_types_intrinsic__lwu17_wrap lw_types__lwu17_types__base_type;
typedef lw_types_intrinsic__lwu17_wrap lw_types__lwu17_types__Tn_typeB;
typedef lw_types__lwu17_types__Tn_typeB lw_types__lwu17_types__n_type;
typedef lw_types_intrinsic__lwu18_wrap lw_types__lwu18_types__base_type;
typedef lw_types_intrinsic__lwu18_wrap lw_types__lwu18_types__Tn_typeB;
typedef lw_types__lwu18_types__Tn_typeB lw_types__lwu18_types__n_type;
typedef lw_types_intrinsic__lwu19_wrap lw_types__lwu19_types__base_type;
typedef lw_types_intrinsic__lwu19_wrap lw_types__lwu19_types__Tn_typeB;
typedef lw_types__lwu19_types__Tn_typeB lw_types__lwu19_types__n_type;
typedef lw_types_intrinsic__lwu20_wrap lw_types__lwu20_types__base_type;
typedef lw_types_intrinsic__lwu20_wrap lw_types__lwu20_types__Tn_typeB;
typedef lw_types__lwu20_types__Tn_typeB lw_types__lwu20_types__n_type;
typedef lw_types_intrinsic__lwu21_wrap lw_types__lwu21_types__base_type;
typedef lw_types_intrinsic__lwu21_wrap lw_types__lwu21_types__Tn_typeB;
typedef lw_types__lwu21_types__Tn_typeB lw_types__lwu21_types__n_type;
typedef lw_types_intrinsic__lwu22_wrap lw_types__lwu22_types__base_type;
typedef lw_types_intrinsic__lwu22_wrap lw_types__lwu22_types__Tn_typeB;
typedef lw_types__lwu22_types__Tn_typeB lw_types__lwu22_types__n_type;
typedef lw_types_intrinsic__lwu23_wrap lw_types__lwu23_types__base_type;
typedef lw_types_intrinsic__lwu23_wrap lw_types__lwu23_types__Tn_typeB;
typedef lw_types__lwu23_types__Tn_typeB lw_types__lwu23_types__n_type;
typedef lw_types_intrinsic__lwu24_wrap lw_types__lwu24_types__base_type;
typedef lw_types_intrinsic__lwu24_wrap lw_types__lwu24_types__Tn_typeB;
typedef lw_types__lwu24_types__Tn_typeB lw_types__lwu24_types__n_type;
typedef lw_types_intrinsic__lwu25_wrap lw_types__lwu25_types__base_type;
typedef lw_types_intrinsic__lwu25_wrap lw_types__lwu25_types__Tn_typeB;
typedef lw_types__lwu25_types__Tn_typeB lw_types__lwu25_types__n_type;
typedef lw_types_intrinsic__lwu26_wrap lw_types__lwu26_types__base_type;
typedef lw_types_intrinsic__lwu26_wrap lw_types__lwu26_types__Tn_typeB;
typedef lw_types__lwu26_types__Tn_typeB lw_types__lwu26_types__n_type;
typedef lw_types_intrinsic__lwu27_wrap lw_types__lwu27_types__base_type;
typedef lw_types_intrinsic__lwu27_wrap lw_types__lwu27_types__Tn_typeB;
typedef lw_types__lwu27_types__Tn_typeB lw_types__lwu27_types__n_type;
typedef lw_types_intrinsic__lwu28_wrap lw_types__lwu28_types__base_type;
typedef lw_types_intrinsic__lwu28_wrap lw_types__lwu28_types__Tn_typeB;
typedef lw_types__lwu28_types__Tn_typeB lw_types__lwu28_types__n_type;
typedef lw_types_intrinsic__lwu29_wrap lw_types__lwu29_types__base_type;
typedef lw_types_intrinsic__lwu29_wrap lw_types__lwu29_types__Tn_typeB;
typedef lw_types__lwu29_types__Tn_typeB lw_types__lwu29_types__n_type;
typedef lw_types_intrinsic__lwu30_wrap lw_types__lwu30_types__base_type;
typedef lw_types_intrinsic__lwu30_wrap lw_types__lwu30_types__Tn_typeB;
typedef lw_types__lwu30_types__Tn_typeB lw_types__lwu30_types__n_type;
typedef lw_types_intrinsic__lwu31_wrap lw_types__lwu31_types__base_type;
typedef lw_types_intrinsic__lwu31_wrap lw_types__lwu31_types__Tn_typeB;
typedef lw_types__lwu31_types__Tn_typeB lw_types__lwu31_types__n_type;
typedef lw_types_intrinsic__lwu32_wrap lw_types__lwu32_types__base_type;
typedef lw_types_intrinsic__lwu32_wrap lw_types__lwu32_types__Tn_typeB;
typedef lw_types__lwu32_types__Tn_typeB lw_types__lwu32_types__n_type;
typedef lw_types_intrinsic__lwu64_wrap lw_types__lwu64_types__base_type;
typedef lw_types_intrinsic__lwu64_wrap lw_types__lwu64_types__Tn_typeB;
typedef lw_types__lwu64_types__Tn_typeB lw_types__lwu64_types__n_type;
typedef lw_types__lwu1_types__Tn_typeB lw_types__Tlwu1B;
typedef lw_types__Tlwu1B lw_types__lwu1;
typedef lw_types__lwu2_types__Tn_typeB lw_types__Tlwu2B;
typedef lw_types__Tlwu2B lw_types__lwu2;
typedef lw_types__lwu3_types__Tn_typeB lw_types__Tlwu3B;
typedef lw_types__Tlwu3B lw_types__lwu3;
typedef lw_types__lwu4_types__Tn_typeB lw_types__Tlwu4B;
typedef lw_types__Tlwu4B lw_types__lwu4;
typedef lw_types__lwu5_types__Tn_typeB lw_types__Tlwu5B;
typedef lw_types__Tlwu5B lw_types__lwu5;
typedef lw_types__lwu6_types__Tn_typeB lw_types__Tlwu6B;
typedef lw_types__Tlwu6B lw_types__lwu6;
typedef lw_types__lwu7_types__Tn_typeB lw_types__Tlwu7B;
typedef lw_types__Tlwu7B lw_types__lwu7;
typedef lw_types__lwu8_types__Tn_typeB lw_types__Tlwu8B;
typedef lw_types__Tlwu8B lw_types__lwu8;
typedef lw_types__lwu9_types__Tn_typeB lw_types__Tlwu9B;
typedef lw_types__Tlwu9B lw_types__lwu9;
typedef lw_types__lwu10_types__Tn_typeB lw_types__Tlwu10B;
typedef lw_types__Tlwu10B lw_types__lwu10;
typedef lw_types__lwu11_types__Tn_typeB lw_types__Tlwu11B;
typedef lw_types__Tlwu11B lw_types__lwu11;
typedef lw_types__lwu12_types__Tn_typeB lw_types__Tlwu12B;
typedef lw_types__Tlwu12B lw_types__lwu12;
typedef lw_types__lwu13_types__Tn_typeB lw_types__Tlwu13B;
typedef lw_types__Tlwu13B lw_types__lwu13;
typedef lw_types__lwu14_types__Tn_typeB lw_types__Tlwu14B;
typedef lw_types__Tlwu14B lw_types__lwu14;
typedef lw_types__lwu15_types__Tn_typeB lw_types__Tlwu15B;
typedef lw_types__Tlwu15B lw_types__lwu15;
typedef lw_types__lwu16_types__Tn_typeB lw_types__Tlwu16B;
typedef lw_types__Tlwu16B lw_types__lwu16;
typedef lw_types__lwu17_types__Tn_typeB lw_types__Tlwu17B;
typedef lw_types__Tlwu17B lw_types__lwu17;
typedef lw_types__lwu18_types__Tn_typeB lw_types__Tlwu18B;
typedef lw_types__Tlwu18B lw_types__lwu18;
typedef lw_types__lwu19_types__Tn_typeB lw_types__Tlwu19B;
typedef lw_types__Tlwu19B lw_types__lwu19;
typedef lw_types__lwu20_types__Tn_typeB lw_types__Tlwu20B;
typedef lw_types__Tlwu20B lw_types__lwu20;
typedef lw_types__lwu21_types__Tn_typeB lw_types__Tlwu21B;
typedef lw_types__Tlwu21B lw_types__lwu21;
typedef lw_types__lwu22_types__Tn_typeB lw_types__Tlwu22B;
typedef lw_types__Tlwu22B lw_types__lwu22;
typedef lw_types__lwu23_types__Tn_typeB lw_types__Tlwu23B;
typedef lw_types__Tlwu23B lw_types__lwu23;
typedef lw_types__lwu24_types__Tn_typeB lw_types__Tlwu24B;
typedef lw_types__Tlwu24B lw_types__lwu24;
typedef lw_types__lwu25_types__Tn_typeB lw_types__Tlwu25B;
typedef lw_types__Tlwu25B lw_types__lwu25;
typedef lw_types__lwu26_types__Tn_typeB lw_types__Tlwu26B;
typedef lw_types__Tlwu26B lw_types__lwu26;
typedef lw_types__lwu27_types__Tn_typeB lw_types__Tlwu27B;
typedef lw_types__Tlwu27B lw_types__lwu27;
typedef lw_types__lwu28_types__Tn_typeB lw_types__Tlwu28B;
typedef lw_types__Tlwu28B lw_types__lwu28;
typedef lw_types__lwu29_types__Tn_typeB lw_types__Tlwu29B;
typedef lw_types__Tlwu29B lw_types__lwu29;
typedef lw_types__lwu30_types__Tn_typeB lw_types__Tlwu30B;
typedef lw_types__Tlwu30B lw_types__lwu30;
typedef lw_types__lwu31_types__Tn_typeB lw_types__Tlwu31B;
typedef lw_types__Tlwu31B lw_types__lwu31;
typedef lw_types__lwu32_types__Tn_typeB lw_types__Tlwu32B;
typedef lw_types__Tlwu32B lw_types__lwu32;
typedef lw_types__lwu64_types__Tn_typeB lw_types__Tlwu64B;
typedef lw_types__Tlwu64B lw_types__lwu64;

#endif /* LW_TYPES_ADS */

#ifndef LW_TYPES_ARRAY_TYPES_ADS
#define LW_TYPES_ARRAY_TYPES_ADS
typedef lw_types__lwu32 *lw_types__array_types__arr_lwu32_idx8;
typedef lw_types__lwu8 *lw_types__array_types__arr_lwu8_idx8;
typedef lw_types__lwu8 *lw_types__array_types__arr_lwu8_idx32;
typedef lw_types__lwu32 *lw_types__array_types__arr_lwu32_idx32;
#endif /* LW_TYPES_ARRAY_TYPES_ADS */

#ifndef LW_TYPES_REG_ADDR_TYPES_ADS
#define LW_TYPES_REG_ADDR_TYPES_ADS
extern const lw_types__lwu32 lw_types__reg_addr_types__max_bar0_addr;
typedef lw_types__Tlwu32B lw_types__reg_addr_types__Tbar0_addrB;
typedef lw_types__reg_addr_types__Tbar0_addrB lw_types__reg_addr_types__bar0_addr;
typedef lw_types__Tlwu32B lw_types__reg_addr_types__Tcsb_addrB;
typedef lw_types__reg_addr_types__Tcsb_addrB lw_types__reg_addr_types__csb_addr;
#endif /* LW_TYPES_REG_ADDR_TYPES_ADS */

#ifndef LW_TYPES_FALCON_TYPES_ADS
#define LW_TYPES_FALCON_TYPES_ADS
extern const lw_types__lwu32 lw_types__falcon_types__max_falcon_imem_size_bytes;
extern const lw_types__lwu32 lw_types__falcon_types__max_falcon_imem_offset_bytes_for_last_dword_datum;
extern const lw_types__lwu16 lw_types__falcon_types__max_num_of_imem_tags;
extern const lw_types__lwu16 lw_types__falcon_types__max_falcon_imem_tag_index;
extern const lw_types__lwu32 lw_types__falcon_types__max_falcon_dmem_size_bytes;
extern const lw_types__lwu32 lw_types__falcon_types__max_falcon_dmem_offset_bytes;
typedef lw_types__Tlwu16B lw_types__falcon_types__Tucode_imem_tagB;
typedef lw_types__falcon_types__Tucode_imem_tagB lw_types__falcon_types__ucode_imem_tag;
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_imem_offset_in_falcon_bytesB;
typedef lw_types__falcon_types__Tucode_imem_offset_in_falcon_bytesB lw_types__falcon_types__ucode_imem_offset_in_falcon_bytes;
typedef lw_types__falcon_types__Tucode_imem_offset_in_falcon_bytesB lw_types__falcon_types__ucode_imem_offset_in_falcon_4b_aligned_bytes;
extern boolean lw_types__falcon_types__ucode_imem_offset_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_imem_offset_in_falcon_4b_aligned_bytes I13s);
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_dmem_offset_in_falcon_bytesB;
typedef lw_types__falcon_types__Tucode_dmem_offset_in_falcon_bytesB lw_types__falcon_types__ucode_dmem_offset_in_falcon_bytes;
typedef lw_types__falcon_types__Tucode_dmem_offset_in_falcon_bytesB lw_types__falcon_types__ucode_dmem_offset_in_falcon_4b_aligned_bytes;
extern boolean lw_types__falcon_types__ucode_dmem_offset_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_dmem_offset_in_falcon_4b_aligned_bytes I15s);
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_imem_size_in_falcon_bytesB;
typedef lw_types__falcon_types__Tucode_imem_size_in_falcon_bytesB lw_types__falcon_types__ucode_imem_size_in_falcon_bytes;
typedef lw_types__falcon_types__Tucode_imem_size_in_falcon_bytesB lw_types__falcon_types__ucode_imem_size_in_falcon_4b_aligned_bytes;
extern boolean lw_types__falcon_types__ucode_imem_size_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_imem_size_in_falcon_4b_aligned_bytes I17s);
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_dmem_size_in_falcon_bytesB;
typedef lw_types__falcon_types__Tucode_dmem_size_in_falcon_bytesB lw_types__falcon_types__ucode_dmem_size_in_falcon_bytes;
typedef lw_types__falcon_types__Tucode_dmem_size_in_falcon_bytesB lw_types__falcon_types__ucode_dmem_size_in_falcon_4b_aligned_bytes;
extern boolean lw_types__falcon_types__ucode_dmem_size_in_falcon_4b_aligned_bytesPredicate(lw_types__falcon_types__ucode_dmem_size_in_falcon_4b_aligned_bytes I19s);
typedef lw_types__Tlwu32B lw_types__falcon_types__Tucode_desc_dmem_offsetB;
typedef lw_types__falcon_types__Tucode_desc_dmem_offsetB lw_types__falcon_types__ucode_desc_dmem_offset;
typedef lw_types__falcon_types__Tucode_desc_dmem_offsetB lw_types__falcon_types__ucode_desc_dmem_4b_aligned_offset;
extern boolean lw_types__falcon_types__ucode_desc_dmem_4b_aligned_offsetPredicate(lw_types__falcon_types__ucode_desc_dmem_4b_aligned_offset I21s);
#endif /* LW_TYPES_FALCON_TYPES_ADS */

#ifndef ADA_ADS
#define ADA_ADS

#endif /* ADA_ADS */

#ifndef A_UNCCON_ADS
#define A_UNCCON_ADS

#endif /* A_UNCCON_ADS */

#ifndef SYSTEM_ADS
#define SYSTEM_ADS
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

#ifndef UTILITY_ADS
#define UTILITY_ADS
typedef system__address utility__uc_address_to_lwu32GP1244__source;
typedef lw_types__Tlwu32B utility__uc_address_to_lwu32GP1244__target;
typedef lw_types__Tlwu32B utility__uc_lwu32_to_addressGP1422__source;
typedef system__address utility__uc_lwu32_to_addressGP1422__target;
extern void
  GNAT_LINKER_SECTION(".imem_pub")
__lwstom_gnat_last_chance_handler(void);
extern void
  GNAT_LINKER_SECTION(".imem_pub")
utility__initialize_to_please_flow_analysis(_fatptr_UNCarray buf_all);

#endif /* UTILITY_ADS */
