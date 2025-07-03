 /***************************************************************************\
|*                                                                           *|
|*       Copyright 1993-2019 LWPU, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user dolwmenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 1993-2019 LWPU, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     LWPU, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  LWPU, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     LWPU, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  8 C.F.R. 2.101 (OCT 1995),      *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/


 /***************************************************************************\
|*                                                                           *|
|*                         LW Architecture Interface                         *|
|*                                                                           *|
|*  <lwtypes.h> defines common widths used to access hardware in of LWPU's *|
|*  Unified Media Architecture (TM).                                         *|
|*                                                                           *|
 \***************************************************************************/


#ifndef LWTYPES_INCLUDED
#define LWTYPES_INCLUDED

/* XAPIGEN - this file is not suitable for (nor needed by) xapigen.         */
/*           Rather than #ifdef out every such include in every sdk         */
/*           file, punt here.                                               */
#if !defined(XAPIGEN)        /* rest of file */

#ifdef __cplusplus
extern "C" {
#endif

#include "cpuopsys.h"
#include "xapi-sdk.h"       /* XAPIGEN sdk macros for C */

#define LWRM_64 1
#if defined(LW_64_BITS)
#define LWRM_TRUE64 1
#endif

 /***************************************************************************\
|*                                 Typedefs                                  *|
 \***************************************************************************/

#ifdef LW_MISRA_COMPLIANCE_REQUIRED
//Typedefs for MISRA COMPLIANCE
typedef unsigned long long   UInt64;
typedef   signed long long    Int64;
typedef unsigned int         UInt32;
typedef   signed int          Int32;
typedef unsigned short       UInt16;
typedef   signed short        Int16;
typedef unsigned char        UInt8 ;
typedef   signed char         Int8 ;

typedef          void          Void;
typedef          float    float32_t;
typedef          double   float64_t;
#endif


// Floating point types
#ifdef LW_MISRA_COMPLIANCE_REQUIRED
typedef float32_t          LwF32; /* IEEE Single Precision (S1E8M23)         */
typedef float64_t          LwF64; /* IEEE Double Precision (S1E11M52)        */
#else
typedef float              LwF32; /* IEEE Single Precision (S1E8M23)         */
typedef double             LwF64; /* IEEE Double Precision (S1E11M52)        */
#endif


// 8-bit: 'char' is the only 8-bit in the C89 standard and after.
#ifdef LW_MISRA_COMPLIANCE_REQUIRED
typedef UInt8              LwV8; /* "void": enumerated or multiple fields    */
typedef UInt8              LwU8; /* 0 to 255                                 */
typedef  Int8              LwS8; /* -128 to 127                              */
#else
typedef unsigned char      LwV8; /* "void": enumerated or multiple fields    */
typedef unsigned char      LwU8; /* 0 to 255                                 */
typedef   signed char      LwS8; /* -128 to 127                              */
#endif


// 16-bit: If the compiler tells us what we can use, then use it.
#ifdef __INT16_TYPE__
typedef unsigned __INT16_TYPE__ LwV16; /* "void": enumerated or multiple fields */
typedef unsigned __INT16_TYPE__ LwU16; /* 0 to 65535                            */
typedef   signed __INT16_TYPE__ LwS16; /* -32768 to 32767                       */

// The minimal standard for C89 and after
#else       // __INT16_TYPE__
#ifdef LW_MISRA_COMPLIANCE_REQUIRED
typedef UInt16             LwV16; /* "void": enumerated or multiple fields   */
typedef UInt16             LwU16; /* 0 to 65535                              */
typedef  Int16             LwS16; /* -32768 to 32767                         */
#else
typedef unsigned short     LwV16; /* "void": enumerated or multiple fields   */
typedef unsigned short     LwU16; /* 0 to 65535                              */
typedef   signed short     LwS16; /* -32768 to 32767                         */
#endif
#endif      // __INT16_TYPE__


// Macros to get the MSB and LSB of a 16 bit unsigned number
#define LwU16_HI08(n) ((LwU8)(((LwU16)(n)) >> 8))
#define LwU16_LO08(n) ((LwU8)((LwU16)(n)))

// Macro to build a LwU16 from msb and lsb bytes.
#define LwU16_BUILD(msb, lsb)  (((msb) << 8)|(lsb))

#if defined(macosx) || defined(MACOS) || defined(LW_MACINTOSH) || \
    defined(LW_MACINTOSH_64) || defined(LWCPU_AARCH64)
typedef char*              LWREGSTR;
#else
typedef LwU8*              LWREGSTR;
#endif


// 32-bit: If the compiler tells us what we can use, then use it.
#ifdef __INT32_TYPE__
typedef unsigned __INT32_TYPE__ LwV32; /* "void": enumerated or multiple fields */
typedef unsigned __INT32_TYPE__ LwU32; /* 0 to 4294967295                       */
typedef   signed __INT32_TYPE__ LwS32; /* -2147483648 to 2147483647             */

// Older compilers
#else       // __INT32_TYPE__

// For historical reasons, LwU32/LwV32 are defined to different base intrinsic
// types than LwS32 on some platforms.
// Mainly for 64-bit linux, where long is 64 bits and win9x, where int is 16 bit.
#if (defined(LW_UNIX) || defined(vxworks) || defined(LW_WINDOWS_CE) ||  \
     defined(__arm) || defined(__IAR_SYSTEMS_ICC__) || defined(LW_QNX) || \
     defined(LW_INTEGRITY) || defined(LW_HOS) || defined(LW_MODS) || \
     defined(__GNUC__) || defined(__clang__) || defined(LW_MACINTOSH_64)) && \
    (!defined(LW_MACINTOSH) || defined(LW_MACINTOSH_64))
#ifdef LW_MISRA_COMPLIANCE_REQUIRED
typedef UInt32             LwV32; /* "void": enumerated or multiple fields   */
typedef UInt32             LwU32; /* 0 to 4294967295                         */
#else
typedef unsigned int       LwV32; /* "void": enumerated or multiple fields   */
typedef unsigned int       LwU32; /* 0 to 4294967295                         */
#endif

// The minimal standard for C89 and after
#else       // (defined(LW_UNIX) || defined(vxworks) || ...
typedef unsigned long      LwV32; /* "void": enumerated or multiple fields   */
typedef unsigned long      LwU32; /* 0 to 4294967295                         */
#endif      // (defined(LW_UNIX) || defined(vxworks) || ...

// Mac OS 32-bit still needs this
#if defined(LW_MACINTOSH) && !defined(LW_MACINTOSH_64)
typedef   signed long      LwS32; /* -2147483648 to 2147483647               */
#else
#ifdef LW_MISRA_COMPLIANCE_REQUIRED
typedef   Int32            LwS32; /* -2147483648 to 2147483647               */
#else
typedef   signed int       LwS32; /* -2147483648 to 2147483647               */
#endif
#endif      // defined(LW_MACINTOSH) && !defined(LW_MACINTOSH_64)
#endif      // __INT32_TYPE__



// 64-bit types for compilers that support them, plus some obsolete variants
#if defined(__GNUC__) || defined(__clang__) || defined(__arm) || \
    defined(__IAR_SYSTEMS_ICC__) || defined(__ghs__) || defined(_WIN64) || \
    defined(__SUNPRO_C) || defined(__SUNPRO_CC) || defined (__xlC__)
#ifdef LW_MISRA_COMPLIANCE_REQUIRED
typedef UInt64             LwU64; /* 0 to 18446744073709551615                      */
typedef  Int64             LwS64; /* -9223372036854775808 to 9223372036854775807    */
#else
typedef unsigned long long LwU64; /* 0 to 18446744073709551615                      */
typedef          long long LwS64; /* -9223372036854775808 to 9223372036854775807    */
#endif

#define LwU64_fmtX "llX"
#define LwU64_fmtx "llx"
#define LwU64_fmtu "llu"
#define LwU64_fmto "llo"
#define LwS64_fmtd "lld"
#define LwS64_fmti "lli"

// Microsoft since 2003 -- https://msdn.microsoft.com/en-us/library/29dh1w7z.aspx
#else
typedef unsigned __int64   LwU64; /* 0 to 18446744073709551615                      */
typedef          __int64   LwS64; /* -9223372036854775808 to 9223372036854775807    */

#define LwU64_fmtX "I64X"
#define LwU64_fmtx "I64x"
#define LwU64_fmtu "I64u"
#define LwU64_fmto "I64o"
#define LwS64_fmtd "I64d"
#define LwS64_fmti "I64i"

#endif

#ifdef LW_TYPESAFE_HANDLES
/*
 * Can't use opaque pointer as clients might be compiled with mismatched
 * pointer sizes. TYPESAFE check will eventually be removed once all clients
 * have transistioned safely to LwHandle.
 * The plan is to then eventually scale up the handle to be 64-bits.
 */
typedef struct
{
    LwU32 val;
} LwHandle;
#else
/*
 * For compatibility with modules that haven't moved typesafe handles.
 */
typedef LwU32 LwHandle;
#endif // LW_TYPESAFE_HANDLES

/* Boolean type */
typedef LwU8 LwBool;
#define LW_TRUE           ((LwBool)(0 == 0))
#define LW_FALSE          ((LwBool)(0 != 0))

/* Tristate type: LW_TRISTATE_FALSE, LW_TRISTATE_TRUE, LW_TRISTATE_INDETERMINATE */
typedef LwU8 LwTristate;
#define LW_TRISTATE_FALSE           ((LwTristate) 0)
#define LW_TRISTATE_TRUE            ((LwTristate) 1)
#define LW_TRISTATE_INDETERMINATE   ((LwTristate) 2)

/* Macros to extract the low and high parts of a 64-bit unsigned integer */
/* Also designed to work if someone happens to pass in a 32-bit integer */
#ifdef LW_MISRA_COMPLIANCE_REQUIRED
#define LwU64_HI32(n)     ((LwU32)((((LwU64)(n)) >> 32) & 0xffffffffU))
#define LwU64_LO32(n)     ((LwU32)(( (LwU64)(n))        & 0xffffffffU))
#else
#define LwU64_HI32(n)     ((LwU32)((((LwU64)(n)) >> 32) & 0xffffffff))
#define LwU64_LO32(n)     ((LwU32)(( (LwU64)(n))        & 0xffffffff))
#endif
#define LwU40_HI32(n)     ((LwU32)((((LwU64)(n)) >>  8) & 0xffffffffU))
#define LwU40_HI24of32(n) ((LwU32)(  (LwU64)(n)         & 0xffffff00U))

/* Macros to get the MSB and LSB of a 32 bit unsigned number */
#define LwU32_HI16(n)     ((LwU16)((((LwU32)(n)) >> 16) & 0xffffU))
#define LwU32_LO16(n)     ((LwU16)(( (LwU32)(n))        & 0xffffU))

 /***************************************************************************\
|*                                                                           *|
|*  64 bit type definitions for use in interface structures.                 *|
|*                                                                           *|
 \***************************************************************************/

#if defined(LW_64_BITS)

typedef void*              LwP64; /* 64 bit void pointer                     */
typedef LwU64             LwUPtr; /* pointer sized unsigned int              */
typedef LwS64             LwSPtr; /* pointer sized signed int                */
typedef LwU64           LwLength; /* length to agree with sizeof             */

#define LwP64_VALUE(n)        (n)
#define LwP64_fmt "%p"

#define KERNEL_POINTER_FROM_LwP64(p,v) ((p)(v))
#define LwP64_PLUS_OFFSET(p,o) (LwP64)((LwU64)(p) + (LwU64)(o))

#define LwUPtr_fmtX LwU64_fmtX
#define LwUPtr_fmtx LwU64_fmtx
#define LwUPtr_fmtu LwU64_fmtu
#define LwUPtr_fmto LwU64_fmto
#define LwSPtr_fmtd LwS64_fmtd
#define LwSPtr_fmti LwS64_fmti

#else

typedef LwU64              LwP64; /* 64 bit void pointer                     */
typedef LwU32             LwUPtr; /* pointer sized unsigned int              */
typedef LwS32             LwSPtr; /* pointer sized signed int                */
typedef LwU32           LwLength; /* length to agree with sizeof             */

#define LwP64_VALUE(n)        ((void *)(LwUPtr)(n))
#define LwP64_fmt "0x%llx"

#define KERNEL_POINTER_FROM_LwP64(p,v) ((p)(LwUPtr)(v))
#define LwP64_PLUS_OFFSET(p,o) ((p) + (LwU64)(o))

#define LwUPtr_fmtX "X"
#define LwUPtr_fmtx "x"
#define LwUPtr_fmtu "u"
#define LwUPtr_fmto "o"
#define LwSPtr_fmtd "d"
#define LwSPtr_fmti "i"

#endif

#define LwP64_NULL       (LwP64)0

/*!
 * Helper macro to pack an @ref LwU64_ALIGN32 structure from a @ref LwU64.
 *
 * @param[out] pDst   Pointer to LwU64_ALIGN32 structure to pack
 * @param[in]  pSrc   Pointer to LwU64 with which to pack
 */
#define LwU64_ALIGN32_PACK(pDst, pSrc)                                         \
do {                                                                           \
    (pDst)->lo = LwU64_LO32(*(pSrc));                                          \
    (pDst)->hi = LwU64_HI32(*(pSrc));                                          \
} while (LW_FALSE)

/*!
 * Helper macro to unpack a @ref LwU64_ALIGN32 structure into a @ref LwU64.
 *
 * @param[out] pDst   Pointer to LwU64 in which to unpack
 * @param[in]  pSrc   Pointer to LwU64_ALIGN32 structure from which to unpack
 */
#define LwU64_ALIGN32_UNPACK(pDst, pSrc)                                       \
do {                                                                           \
    (*(pDst)) = LwU64_ALIGN32_VAL(pSrc);                                       \
} while (LW_FALSE)

/*!
 * Helper macro to unpack a @ref LwU64_ALIGN32 structure as a @ref LwU64.
 *
 * @param[in]  pSrc   Pointer to LwU64_ALIGN32 structure to unpack
 */
#define LwU64_ALIGN32_VAL(pSrc)                                                \
    ((LwU64) ((LwU64)((pSrc)->lo) | (((LwU64)(pSrc)->hi) << 32U)))

/*!
 * Helper macro to check whether the 32 bit aligned 64 bit number is zero.
 *
 * @param[in]  _pU64   Pointer to LwU64_ALIGN32 structure.
 *
 * @return
 *  LW_TRUE     _pU64 is zero.
 *  LW_FALSE    otherwise.
 */
#define LwU64_ALIGN32_IS_ZERO(_pU64)                                          \
    (((_pU64)->lo == 0U) && ((_pU64)->hi == 0U))

/*!
 * Helper macro to sub two 32 aligned 64 bit numbers on 64 bit processor.
 *
 * @param[in]       pSrc1   Pointer to LwU64_ALIGN32 scource 1 structure.
 * @param[in]       pSrc2   Pointer to LwU64_ALIGN32 scource 2 structure.
 * @param[in/out]   pDst    Pointer to LwU64_ALIGN32 dest. structure.
 */
#define LwU64_ALIGN32_ADD(pDst, pSrc1, pSrc2)                                 \
do {                                                                          \
    LwU64 __dst, __src1, __scr2;                                              \
                                                                              \
    LwU64_ALIGN32_UNPACK(&__src1, (pSrc1));                                   \
    LwU64_ALIGN32_UNPACK(&__scr2, (pSrc2));                                   \
    __dst = __src1 + __scr2;                                                  \
    LwU64_ALIGN32_PACK((pDst), &__dst);                                       \
} while (LW_FALSE)

/*!
 * Helper macro to sub two 32 aligned 64 bit numbers on 64 bit processor.
 *
 * @param[in]       pSrc1   Pointer to LwU64_ALIGN32 scource 1 structure.
 * @param[in]       pSrc2   Pointer to LwU64_ALIGN32 scource 2 structure.
 * @param[in/out]   pDst    Pointer to LwU64_ALIGN32 dest. structure.
 */
#define LwU64_ALIGN32_SUB(pDst, pSrc1, pSrc2)                                  \
do {                                                                           \
    LwU64 __dst, __src1, __scr2;                                               \
                                                                               \
    LwU64_ALIGN32_UNPACK(&__src1, (pSrc1));                                    \
    LwU64_ALIGN32_UNPACK(&__scr2, (pSrc2));                                    \
    __dst = __src1 + __scr2;                                                   \
    LwU64_ALIGN32_PACK((pDst), &__dst);                                        \
} while (LW_FALSE)

/*!
 * Structure for representing 32 bit aligned LwU64 (64-bit unsigned integer)
 * structures. This structure must be used because the 32 bit processor and
 * 64 bit processor compilers will pack/align LwU64 differently.
 *
 * One use case is RM being 64 bit proc whereas PMU being 32 bit proc, this
 * alignment difference will result in corrupted transactions between the RM
 * and PMU.
 *
 * See the @ref LwU64_ALIGN32_PACK and @ref LwU64_ALIGN32_UNPACK macros for
 * packing and unpacking these structures.
 *
 * @note The intention of this structure is to provide a datatype which will
 *       packed/aligned consistently and efficiently across all platforms.
 *       We don't want to use "LW_DECLARE_ALIGNED(LwU64, 8)" because that
 *       leads to memory waste on our 32-bit uprocessors (e.g. FALCONs) where
 *       DMEM efficiency is vital.
 */
typedef struct
{
    /*!
     * Low 32 bits.
     */
    LwU32 lo;
    /*!
     * High 32 bits.
     */
    LwU32 hi;
} LwU64_ALIGN32;

// XXX Obsolete -- get rid of me...
typedef LwP64 LwP64_VALUE_T;
#define LwP64_LVALUE(n)   (n)
#define LwP64_SELECTOR(n) (0)

/* Useful macro to hide required double cast */
#define LW_PTR_TO_LwP64(n) (LwP64)(LwUPtr)(n)
#define LW_SIGN_EXT_PTR_TO_LwP64(p) ((LwP64)(LwS64)(LwSPtr)(p))
#define KERNEL_POINTER_TO_LwP64(p) ((LwP64)(uintptr_t)(p))

/* obsolete stuff  */
/* MODS needs to be able to build without these definitions because they collide
   with some definitions used in mdiag. */
#ifndef DONT_DEFINE_U032
typedef LwV8  V008;
typedef LwV16 V016;
typedef LwV32 V032;
typedef LwU8  U008;
typedef LwU16 U016;
typedef LwU32 U032;
typedef LwS8  S008;
typedef LwS16 S016;
typedef LwS32 S032;
#endif
#if defined(MACOS) || defined(macintosh) || defined(__APPLE_CC__) || defined(LW_MODS) || defined(MINIRM) || defined (LW_QNX) || defined(LW_INTEGRITY) || defined(LW_HOS)
/* more obsolete stuff */
/* need to provide these on macos9 and macosX */
#if defined(__APPLE_CC__)  /* gross but Apple osX already claims ULONG */
#undef ULONG    // just in case
#define ULONG unsigned long
#else
typedef unsigned long  ULONG;
#endif
typedef unsigned char *PUCHAR;
#endif

 /***************************************************************************\
|*                                                                           *|
|*  Limits for common types.                                                 *|
|*                                                                           *|
 \***************************************************************************/

/* Explanation of the current form of these limits:
 *
 * - Decimal is used, as hex values are by default positive.
 * - Casts are not used, as usage in the preprocessor itself (#if) ends poorly.
 * - The subtraction of 1 for some MIN values is used to get around the fact
 *   that the C syntax actually treats -x as NEGATE(x) instead of a distinct
 *   number.  Since 214748648 isn't a valid positive 32-bit signed value, we
 *   take the largest valid positive signed number, negate it, and subtract 1.
 */
#define LW_S8_MIN       (-128)
#define LW_S8_MAX       (+127)
#define LW_U8_MIN       (0U)
#define LW_U8_MAX       (+255U)
#define LW_S16_MIN      (-32768)
#define LW_S16_MAX      (+32767)
#define LW_U16_MIN      (0U)
#define LW_U16_MAX      (+65535U)
#define LW_S32_MIN      (-2147483647 - 1)
#define LW_S32_MAX      (+2147483647)
#define LW_U32_MIN      (0U)
#define LW_U32_MAX      (+4294967295U)
#define LW_S64_MIN      (-9223372036854775807LL - 1LL)
#define LW_S64_MAX      (+9223372036854775807LL)
#define LW_U64_MIN      (0ULL)
#define LW_U64_MAX      (+18446744073709551615ULL)

#if !defined(LW_PTR)
#define LW_PTR
#define CAST_LW_PTR(p)     p
#endif

/* Aligns fields in structs  so they match up between 32 and 64 bit builds */
#if defined(__GNUC__) || defined(__clang__) || defined(LW_QNX) || defined(LW_HOS)
#define LW_ALIGN_BYTES(size) __attribute__ ((aligned (size)))
#elif defined(__arm)
#define LW_ALIGN_BYTES(size) __align(ALIGN)
#else
// XXX This is dangerously nonportable!  We really shouldn't provide a default
// version of this that doesn't do anything.
#define LW_ALIGN_BYTES(size)
#endif

// LW_DECLARE_ALIGNED() can be used on all platforms.
// This macro form accounts for the fact that __declspec on Windows is required
// before the variable type,
// and LW_ALIGN_BYTES is required after the variable name.
#if defined(__GNUC__) || defined(__clang__) || defined(LW_QNX) || defined(LW_HOS)
#define LW_DECLARE_ALIGNED(TYPE_VAR, ALIGN) TYPE_VAR __attribute__ ((aligned (ALIGN)))
#elif defined(_MSC_VER)
#define LW_DECLARE_ALIGNED(TYPE_VAR, ALIGN) __declspec(align(ALIGN)) TYPE_VAR
#elif defined(__arm)
#define LW_DECLARE_ALIGNED(TYPE_VAR, ALIGN) __align(ALIGN) TYPE_VAR
#endif

// LWRM_IMPORT is defined on windows for lwrm4x build (lwrm4x.lib).
#if (defined(_MSC_VER) && defined(LWRM4X_BUILD))
#define LWRM_IMPORT __declspec(dllimport)
#else
#define LWRM_IMPORT
#endif

// Check for typeof support. For now restricting to GNUC compilers.
#if defined(__GNUC__)
#define LW_TYPEOF_SUPPORTED 1
#else
#define LW_TYPEOF_SUPPORTED 0
#endif

 /***************************************************************************\
|*                       Function Declaration Types                          *|
 \***************************************************************************/

// stretching the meaning of "lwtypes", but this seems to least offensive
// place to re-locate these from lwos.h which cannot be included by a number
// of builds that need them

#if defined(_MSC_VER)

    #if _MSC_VER >= 1310
    #define LW_NOINLINE __declspec(noinline)
    #else
    #define LW_NOINLINE
    #endif

    #define LW_INLINE __inline

    #if _MSC_VER >= 1200
    #define LW_FORCEINLINE __forceinline
    #else
    #define LW_FORCEINLINE __inline
    #endif

    #define LW_APIENTRY  __stdcall
    #define LW_FASTCALL  __fastcall
    #define LW_CDECLCALL __cdecl
    #define LW_STDCALL   __stdcall

    #define LW_FORCERESULTCHECK

    #define LW_ATTRIBUTE_UNUSED

#else // ! defined(_MSC_VER)

    #if defined(__GNUC__)
        #if (__GNUC__ > 3) || \
            ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1) && (__GNUC_PATCHLEVEL__ >= 1))
            #define LW_NOINLINE __attribute__((__noinline__))
        #endif
    #elif defined(__clang__)
        #if __has_attribute(noinline)
        #define LW_NOINLINE __attribute__((__noinline__))
        #endif
    #elif defined(__arm) && (__ARMCC_VERSION >= 300000)
        #define LW_NOINLINE __attribute__((__noinline__))
    #elif (defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)) ||\
            (defined(__SUNPRO_CC) && (__SUNPRO_CC >= 0x590))
        #define LW_NOINLINE __attribute__((__noinline__))
    #elif defined (__INTEL_COMPILER)
        #define LW_NOINLINE __attribute__((__noinline__))
    #endif

    #if !defined(LW_NOINLINE)
    #define LW_NOINLINE
    #endif

    /* GreenHills compiler defines __GNUC__, but doesn't support
     * __inline__ keyword. */
    #if defined(__ghs__)
    #define LW_INLINE inline
    #elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
    #define LW_INLINE __inline__
    #elif defined (macintosh) || defined(__SUNPRO_C) || defined(__SUNPRO_CC)
    #define LW_INLINE inline
    #elif defined(__arm)
    #define LW_INLINE __inline
    #else
    #define LW_INLINE
    #endif

    /* Don't force inline on DEBUG builds -- it's annoying for debuggers. */
    #if !defined(DEBUG)
        /* GreenHills compiler defines __GNUC__, but doesn't support
         * __attribute__ or __inline__ keyword. */
        #if defined(__ghs__)
            #define LW_FORCEINLINE inline
        #elif defined(__GNUC__)
            // GCC 3.1 and beyond support the always_inline function attribute.
            #if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
            #define LW_FORCEINLINE __attribute__((__always_inline__)) __inline__
            #else
            #define LW_FORCEINLINE __inline__
            #endif
        #elif defined(__clang__)
            #if __has_attribute(always_inline)
            #define LW_FORCEINLINE __attribute__((__always_inline__)) __inline__
            #else
            #define LW_FORCEINLINE __inline__
            #endif
        #elif defined(__arm) && (__ARMCC_VERSION >= 220000)
            // RVDS 2.2 also supports forceinline, but ADS 1.2 does not
            #define LW_FORCEINLINE __forceinline
        #else /* defined(__GNUC__) */
            #define LW_FORCEINLINE LW_INLINE
        #endif
    #else
        #define LW_FORCEINLINE LW_INLINE
    #endif

    #define LW_APIENTRY
    #define LW_FASTCALL
    #define LW_CDECLCALL
    #define LW_STDCALL

    /*
     * The 'warn_unused_result' function attribute prompts GCC to issue a
     * warning if the result of a function tagged with this attribute
     * is ignored by a caller.  In combination with '-Werror', it can be
     * used to enforce result checking in RM code; at this point, this
     * is only done on UNIX.
     */
    #if defined(__GNUC__) && defined(LW_UNIX)
        #if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4))
        #define LW_FORCERESULTCHECK __attribute__((__warn_unused_result__))
        #else
        #define LW_FORCERESULTCHECK
        #endif
    #elif defined(__clang__)
        #if __has_attribute(warn_unused_result)
        #define LW_FORCERESULTCHECK __attribute__((__warn_unused_result__))
        #else
        #define LW_FORCERESULTCHECK
        #endif
    #else /* defined(__GNUC__) */
        #define LW_FORCERESULTCHECK
    #endif

    #if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
        #define LW_ATTRIBUTE_UNUSED __attribute__((__unused__))
    #else
        #define LW_ATTRIBUTE_UNUSED
    #endif

#endif  // defined(_MSC_VER)

/*!
 * Fixed-point master data types.
 *
 * These are master-types represent the total number of bits contained within
 * the FXP type.  All FXP types below should be based on one of these master
 * types.
 */
typedef LwS16                                                         LwSFXP16;
typedef LwS32                                                         LwSFXP32;
typedef LwU16                                                         LwUFXP16;
typedef LwU32                                                         LwUFXP32;
typedef LwU64                                                         LwUFXP64;


/*!
 * Fixed-point data types.
 *
 * These are all integer types with precision indicated in the naming of the
 * form: Lw<sign>FXP<num_bits_above_radix>_<num bits below radix>.  The actual
 * size of the data type is callwlated as num_bits_above_radix +
 * num_bit_below_radix.
 *
 * All of these FXP types should be based on one of the master types above.
 */
typedef LwSFXP16                                                    LwSFXP11_5;
typedef LwSFXP16                                                    LwSFXP4_12;
typedef LwSFXP16                                                     LwSFXP8_8;
typedef LwSFXP32                                                    LwSFXP8_24;
typedef LwSFXP32                                                   LwSFXP10_22;
typedef LwSFXP32                                                   LwSFXP16_16;
typedef LwSFXP32                                                   LwSFXP18_14;
typedef LwSFXP32                                                   LwSFXP20_12;
typedef LwSFXP32                                                    LwSFXP24_8;
typedef LwSFXP32                                                    LwSFXP27_5;
typedef LwSFXP32                                                    LwSFXP28_4;
typedef LwSFXP32                                                    LwSFXP29_3;
typedef LwSFXP32                                                    LwSFXP31_1;

typedef LwUFXP16                                                    LwUFXP0_16;
typedef LwUFXP16                                                    LwUFXP4_12;
typedef LwUFXP16                                                     LwUFXP8_8;
typedef LwUFXP32                                                    LwUFXP3_29;
typedef LwUFXP32                                                    LwUFXP4_28;
typedef LwUFXP32                                                    LwUFXP8_24;
typedef LwUFXP32                                                    LwUFXP9_23;
typedef LwUFXP32                                                   LwUFXP10_22;
typedef LwUFXP32                                                   LwUFXP16_16;
typedef LwUFXP32                                                   LwUFXP20_12;
typedef LwUFXP32                                                    LwUFXP24_8;
typedef LwUFXP32                                                    LwUFXP25_7;
typedef LwUFXP32                                                    LwUFXP28_4;

typedef LwUFXP64                                                   LwUFXP40_24;
typedef LwUFXP64                                                   LwUFXP48_16;
typedef LwUFXP64                                                   LwUFXP52_12;

/*!
 * Utility macros used in colwerting between signed integers and fixed-point
 * notation.
 *
 * - COMMON - These are used by both signed and unsigned.
 */
#define LW_TYPES_FXP_INTEGER(x, y)                              ((x)+(y)-1):(y)
#define LW_TYPES_FXP_FRACTIONAL(x, y)                                 ((y)-1):0
#define LW_TYPES_FXP_FRACTIONAL_MSB(x, y)                       ((y)-1):((y)-1)
#define LW_TYPES_FXP_FRACTIONAL_MSB_ONE                              0x00000001
#define LW_TYPES_FXP_FRACTIONAL_MSB_ZERO                             0x00000000
#define LW_TYPES_FXP_ZERO                                                   (0)

/*!
 * - UNSIGNED - These are only used for unsigned.
 */
#define LW_TYPES_UFXP_INTEGER_MAX(x, y)                      (~(LWBIT((y))-1U))
#define LW_TYPES_UFXP_INTEGER_MIN(x, y)                                    (0U)

/*!
 * - SIGNED - These are only used for signed.
 */
#define LW_TYPES_SFXP_INTEGER_SIGN(x, y)                ((x)+(y)-1):((x)+(y)-1)
#define LW_TYPES_SFXP_INTEGER_SIGN_NEGATIVE                          0x00000001
#define LW_TYPES_SFXP_INTEGER_SIGN_POSITIVE                          0x00000000
#define LW_TYPES_SFXP_S32_SIGN_EXTENSION(x, y)                           31:(x)
#define LW_TYPES_SFXP_S32_SIGN_EXTENSION_POSITIVE(x, y)              0x00000000
#define LW_TYPES_SFXP_S32_SIGN_EXTENSION_NEGATIVE(x, y)      (LWBIT(32-(x))-1U)
#define LW_TYPES_SFXP_INTEGER_MAX(x, y)                         (LWBIT((x))-1U)
#define LW_TYPES_SFXP_INTEGER_MIN(x, y)                      (~(LWBIT((x))-1U))

/*!
 * Colwersion macros used for colwerting between integer and fixed point
 * representations.  Both signed and unsigned variants.
 *
 * Warning:
 * Note that most of the macros below can overflow if applied on values that can
 * not fit the destination type.  It's caller responsibility to ensure that such
 * situations will not occur.
 *
 * Some colwersions perform some commonly preformed tasks other than just
 * bit-shifting:
 *
 * - _SCALED:
 *   For integer -> fixed-point we add handling divisors to represent
 *   non-integer values.
 *
 * - _ROUNDED:
 *   For fixed-point -> integer we add rounding to integer values.
 */

// 32-bit Unsigned FXP:
#define LW_TYPES_U32_TO_UFXP_X_Y(x, y, integer)                               \
    ((LwUFXP##x##_##y) (((LwU32) (integer)) <<                                \
                        DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))))

#define LW_TYPES_U32_TO_UFXP_X_Y_SCALED(x, y, integer, scale)                 \
    ((LwUFXP##x##_##y) ((((((LwU32) (integer)) <<                             \
                        DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y))))) /         \
                            (scale)) +                                        \
                        ((((((LwU32) (integer)) <<                            \
                            DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))) %      \
                                (scale)) > ((scale) >> 1)) ? 1U : 0U)))

#define LW_TYPES_UFXP_X_Y_TO_U32(x, y, fxp)                                   \
    ((LwU32) (DRF_VAL(_TYPES, _FXP, _INTEGER((x), (y)),                       \
                    ((LwUFXP##x##_##y) (fxp)))))

#define LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(x, y, fxp)                           \
    (LW_TYPES_UFXP_X_Y_TO_U32(x, y, (fxp)) +                                  \
        !!DRF_VAL(_TYPES, _FXP, _FRACTIONAL_MSB((x), (y)),                    \
            ((LwUFXP##x##_##y) (fxp))))

// 64-bit Unsigned FXP
#define LW_TYPES_U64_TO_UFXP_X_Y(x, y, integer)                               \
    ((LwUFXP##x##_##y) (((LwU64) (integer)) <<                                \
                        DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))))

#define LW_TYPES_U64_TO_UFXP_X_Y_SCALED(x, y, integer, scale)                 \
    ((LwUFXP##x##_##y) (((((LwU64) (integer)) <<                              \
                             DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))) +     \
                         ((scale) >> 1)) /                                    \
                        (scale)))

#define LW_TYPES_UFXP_X_Y_TO_U64(x, y, fxp)                                   \
    ((LwU64) (DRF_VAL(_TYPES, _FXP, _INTEGER((x), (y)),                       \
                    ((LwUFXP##x##_##y) (fxp)))))

#define LW_TYPES_UFXP_X_Y_TO_U64_ROUNDED(x, y, fxp)                           \
    (LW_TYPES_UFXP_X_Y_TO_U64(x, y, (fxp)) +                                  \
        !!DRF_VAL(_TYPES, _FXP, _FRACTIONAL_MSB((x), (y)),                    \
            ((LwUFXP##x##_##y) (fxp))))

//
// 32-bit Signed FXP:
// Some compilers do not support left shift negative values
// so typecast integer to LwU32 instead of LwS32
//
#define LW_TYPES_S32_TO_SFXP_X_Y(x, y, integer)                               \
    ((LwSFXP##x##_##y) (((LwU32) (integer)) <<                                \
                        DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))))

#define LW_TYPES_S32_TO_SFXP_X_Y_SCALED(x, y, integer, scale)                 \
    ((LwSFXP##x##_##y) (((((LwS32) (integer)) <<                              \
                             DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))) +     \
                         ((scale) >> 1)) /                                    \
                        (scale)))

#define LW_TYPES_SFXP_X_Y_TO_S32(x, y, fxp)                                   \
    ((LwS32) ((DRF_VAL(_TYPES, _FXP, _INTEGER((x), (y)),                      \
                    ((LwSFXP##x##_##y) (fxp)))) |                             \
              ((DRF_VAL(_TYPES, _SFXP, _INTEGER_SIGN((x), (y)), (fxp)) ==     \
                    LW_TYPES_SFXP_INTEGER_SIGN_NEGATIVE) ?                    \
                DRF_NUM(_TYPES, _SFXP, _S32_SIGN_EXTENSION((x), (y)),         \
                    LW_TYPES_SFXP_S32_SIGN_EXTENSION_NEGATIVE((x), (y))) :    \
                DRF_NUM(_TYPES, _SFXP, _S32_SIGN_EXTENSION((x), (y)),         \
                    LW_TYPES_SFXP_S32_SIGN_EXTENSION_POSITIVE((x), (y))))))

#define LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(x, y, fxp)                           \
    (LW_TYPES_SFXP_X_Y_TO_S32(x, y, (fxp)) +                                  \
        !!DRF_VAL(_TYPES, _FXP, _FRACTIONAL_MSB((x), (y)),                    \
            ((LwSFXP##x##_##y) (fxp))))


/*!
 * Macros representing the single-precision IEEE 754 floating point format for
 * "binary32", also known as "single" and "float".
 *
 * Single precision floating point format wiki [1]
 *
 * _SIGN
 *     Single bit representing the sign of the number.
 * _EXPONENT
 *     Unsigned 8-bit number representing the exponent value by which to scale
 *     the mantissa.
 *     _BIAS - The value by which to offset the exponent to account for sign.
 * _MANTISSA
 *     Explicit 23-bit significand of the value.  When exponent != 0, this is an
 *     implicitly 24-bit number with a leading 1 prepended.  This 24-bit number
 *     can be conceptualized as FXP 9.23.
 *
 * With these definitions, the value of a floating point number can be
 * callwlated as:
 *     (-1)^(_SIGN) *
 *         2^(_EXPONENT - _EXPONENT_BIAS) *
 *         (1 + _MANTISSA / (1 << 23))
 */
// [1] : http://en.wikipedia.org/wiki/Single_precision_floating-point_format
#define LW_TYPES_SINGLE_SIGN                                               31:31
#define LW_TYPES_SINGLE_SIGN_POSITIVE                                 0x00000000
#define LW_TYPES_SINGLE_SIGN_NEGATIVE                                 0x00000001
#define LW_TYPES_SINGLE_EXPONENT                                           30:23
#define LW_TYPES_SINGLE_EXPONENT_ZERO                                 0x00000000
#define LW_TYPES_SINGLE_EXPONENT_BIAS                                 0x0000007F
#define LW_TYPES_SINGLE_MANTISSA                                            22:0


/*!
 * Helper macro to return a IEEE 754 single-precision value's mantissa as an
 * unsigned FXP 9.23 value.
 *
 * @param[in] single   IEEE 754 single-precision value to manipulate.
 *
 * @return IEEE 754 single-precision values mantissa represented as an unsigned
 *     FXP 9.23 value.
 */
#define LW_TYPES_SINGLE_MANTISSA_TO_UFXP9_23(single)                           \
    ((LwUFXP9_23)(FLD_TEST_DRF(_TYPES, _SINGLE, _EXPONENT, _ZERO, single) ?    \
                    LW_TYPES_U32_TO_UFXP_X_Y(9, 23, 0) :                       \
                    (LW_TYPES_U32_TO_UFXP_X_Y(9, 23, 1) +                      \
                        DRF_VAL(_TYPES, _SINGLE, _MANTISSA, single))))

/*!
 * Helper macro to return an IEEE 754 single-precision value's exponent,
 * including the bias.
 *
 * @param[in] single   IEEE 754 single-precision value to manipulate.
 *
 * @return Signed exponent value for IEEE 754 single-precision.
 */
#define LW_TYPES_SINGLE_EXPONENT_BIASED(single)                                \
    ((LwS32)(DRF_VAL(_TYPES, _SINGLE, _EXPONENT, single) -                     \
        LW_TYPES_SINGLE_EXPONENT_BIAS))

/*!
 * LwTemp - temperature data type introduced to avoid bugs in colwersion between
 * various existing notations.
 */
typedef LwSFXP24_8              LwTemp;

/*!
 * Macros for LwType <-> Celsius temperature colwersion.
 */
#define LW_TYPES_CELSIUS_TO_LW_TEMP(cel)                                      \
                                LW_TYPES_S32_TO_SFXP_X_Y(24,8,(cel))
#define LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED(lwt)                              \
                                LW_TYPES_SFXP_X_Y_TO_S32(24,8,(lwt))
#define LW_TYPES_LW_TEMP_TO_CELSIUS_ROUNDED(lwt)                              \
                                LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(24,8,(lwt))

/*!
 * Macro for LwType -> number of bits colwersion
 */
#define LW_NBITS_IN_TYPE(type) (8 * sizeof(type))

/*!
 * Macro to colwert SFXP 11.5 to LwTemp.
 */
#define LW_TYPES_LWSFXP11_5_TO_LW_TEMP(x) ((LwTemp)(x) << 3)

/*!
 * Macro to colwert UFXP11.5 Watts to LwU32 milli-Watts.
 */
#define LW_TYPES_LWUFXP11_5_WATTS_TO_LWU32_MILLI_WATTS(x) ((((LwU32)(x)) * ((LwU32)1000)) >> 5)

#ifdef __cplusplus
}
#endif

#endif /* ! XAPIGEN */

#endif /* LWTYPES_INCLUDED */
