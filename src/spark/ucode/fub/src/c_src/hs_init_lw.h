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

#ifndef HS_INIT_COMMON_TU104_ADS
#define HS_INIT_COMMON_TU104_ADS
extern void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_common_tu104__read_fpf_fuse_pub_rev(lw_types__lwu8 *data);
extern void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_common_tu104__read_chip_id(lw_types__lwu9 *chipid);
extern void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_common_tu104__read_fuse_opt(lw_types__lwu32 *data);
#endif /* HS_INIT_COMMON_TU104_ADS */

#ifndef LW_TYPES_REG_ADDR_TYPES_ADS
#define LW_TYPES_REG_ADDR_TYPES_ADS
extern const lw_types__lwu32 lw_types__reg_addr_types__max_bar0_addr;
typedef lw_types__Tlwu32B lw_types__reg_addr_types__Tbar0_addrB;
typedef lw_types__reg_addr_types__Tbar0_addrB lw_types__reg_addr_types__bar0_addr;
typedef lw_types__Tlwu32B lw_types__reg_addr_types__Tcsb_addrB;
typedef lw_types__reg_addr_types__Tcsb_addrB lw_types__reg_addr_types__csb_addr;
#endif /* LW_TYPES_REG_ADDR_TYPES_ADS */

#ifndef DEV_MASTER_ADS
#define DEV_MASTER_ADS
typedef struct _dev_master__lw_pmc_boot_42_register {
  unsigned_32 _pad1 : 8;
  unsigned_32 minor_extended_revision : 4;
  unsigned_32 minor_revision : 4;
  unsigned_32 major_revision : 4;
  unsigned_32 chip_id : 9;
  unsigned_32 rsvd : 3;
} GNAT_PACKED dev_master__lw_pmc_boot_42_register;
extern const lw_types__reg_addr_types__bar0_addr dev_master__lw_pmc_boot_42_addr;
#endif /* DEV_MASTER_ADS */

#ifndef UCODE_POST_CODES_ADS
#define UCODE_POST_CODES_ADS
typedef integer_32 ucode_post_codes__lw_ucode_unified_error_type;
enum {ucode_post_codes__ucode_err_code_noerror=0, ucode_post_codes__ucode_descriptor_ilwalid=1, ucode_post_codes__xtal4x_clk_not_good=
  2, ucode_post_codes__ilwalid_falcon_rev=3, ucode_post_codes__chip_id_ilwalid=4, ucode_post_codes__ilwalid_ucode_revision=
  5, ucode_post_codes__unexpected_falcon_engid=6, ucode_post_codes__bad_csberrorstate=7, ucode_post_codes__sebus_get_data_timeout=
  8, ucode_post_codes__sebus_get_data_bus_request_fail=9, ucode_post_codes__sebus_send_request_timeout_channel_empty=
  10, ucode_post_codes__sebus_send_request_timeout_write_complete=11, ucode_post_codes__sebus_default_timeout_ilwalid=
  12, ucode_post_codes__sebus_err_mutex_acquire=13, ucode_post_codes__sebus_vhr_check_failed=14, ucode_post_codes__flcn_error_reg_access=
  15, ucode_post_codes__debug1=16, ucode_post_codes__debug2=17, ucode_post_codes__debug3=18, ucode_post_codes__debug4=
  19, ucode_post_codes__debug5=20};
typedef character ucode_post_codes__Tlw_ucode_unified_error_typeSS[419];
typedef character ucode_post_codes__T1s[419];
typedef integer ucode_post_codes__TTlw_ucode_unified_error_typeSSP1;
extern const ucode_post_codes__Tlw_ucode_unified_error_typeSS ucode_post_codes__lw_ucode_unified_error_typeS;
typedef integer_16 *ucode_post_codes__Tlw_ucode_unified_error_typeNB;
typedef integer_16 ucode_post_codes__T5s[22];
typedef integer_16 ucode_post_codes__Tlw_ucode_unified_error_typeNT[22];
typedef integer ucode_post_codes__Tlw_ucode_unified_error_typeND1;
extern const ucode_post_codes__Tlw_ucode_unified_error_typeNT ucode_post_codes__lw_ucode_unified_error_typeN;
#endif /* UCODE_POST_CODES_ADS */

#ifndef COMPATIBILITY_ADS
#define COMPATIBILITY_ADS

#endif /* COMPATIBILITY_ADS */

#ifndef HS_INIT_LW_ADS
#define HS_INIT_LW_ADS
extern ucode_post_codes__lw_ucode_unified_error_type
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_lw__is_valid_ucode_rev(void);
extern void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_lw__get_ucode_rev_fpf_val(lw_types__lwu32 *rev);
extern void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_lw__get_ucode_rev_chip_option_fuse_val(lw_types__lwu32 *revlock_fuse_val);
extern void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_lw__get_ucode_rev_val(lw_types__lwu32 *rev, ucode_post_codes__lw_ucode_unified_error_type *status);
#endif /* HS_INIT_LW_ADS */

#ifndef INTERFAC_ADS
#define INTERFAC_ADS
typedef short_short_integer interfaces__Tinteger_8B;
typedef interfaces__Tinteger_8B interfaces__integer_8;
typedef short_integer interfaces__Tinteger_16B;
typedef interfaces__Tinteger_16B interfaces__integer_16;
typedef integer interfaces__Tinteger_32B;
typedef interfaces__Tinteger_32B interfaces__integer_32;
typedef long_long_integer interfaces__Tinteger_64B;
typedef interfaces__Tinteger_64B interfaces__integer_64;
typedef unsigned_8 interfaces__unsigned_8;
typedef unsigned_16 interfaces__unsigned_16;
typedef unsigned_32 interfaces__unsigned_24;
typedef unsigned_32 interfaces__unsigned_32;
typedef unsigned_64 interfaces__unsigned_64;
typedef short_float interfaces__Tieee_float_32B;
typedef interfaces__Tieee_float_32B interfaces__ieee_float_32;
typedef long_float interfaces__Tieee_float_64B;
typedef interfaces__Tieee_float_64B interfaces__ieee_float_64;
typedef long_long_float interfaces__Tieee_extended_floatB;
typedef interfaces__Tieee_extended_floatB interfaces__ieee_extended_float;
#endif /* INTERFAC_ADS */
