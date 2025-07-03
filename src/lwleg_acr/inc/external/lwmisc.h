 /***************************************************************************\
|*                                                                           *|
|*       Copyright 1993-2019 LWPU, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  LWPU, Corp. of Sunnyvale,  California owns     *|
|*     copyrights, patents, and has design patents pending on the design     *|
|*     and  interface  of the LW chips.   Users and  possessors  of this     *|
|*     source code are hereby granted a nonexclusive, royalty-free copy-     *|
|*     right  and design patent license  to use this code  in individual     *|
|*     and commercial software.                                              *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user dolwmenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*     Copyright  1993-2019  LWPU,  Corporation.   LWPU  has  design     *|
|*     patents and patents pending in the U.S. and foreign countries.        *|
|*                                                                           *|
|*     LWPU, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF THIS SOURCE CODE FOR ANY PURPOSE. IT IS PROVIDED "AS IS" WITH-     *|
|*     OUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  LWPU, CORPORATION     *|
|*     DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOURCE CODE, INCLUD-     *|
|*     ING ALL IMPLIED WARRANTIES  OF MERCHANTABILITY  AND FITNESS FOR A     *|
|*     PARTICULAR  PURPOSE.  IN NO EVENT  SHALL LWPU,  CORPORATION  BE     *|
|*     LIABLE FOR ANY SPECIAL,  INDIRECT,  INCIDENTAL,  OR CONSEQUENTIAL     *|
|*     DAMAGES, OR ANY DAMAGES  WHATSOEVER  RESULTING  FROM LOSS OF USE,     *|
|*     DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR     *|
|*     OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION  WITH THE     *|
|*     USE OR PERFORMANCE OF THIS SOURCE CODE.                               *|
|*                                                                           *|
|*     RESTRICTED RIGHTS LEGEND:  Use, duplication, or disclosure by the     *|
|*     Government is subject  to restrictions  as set forth  in subpara-     *|
|*     graph (c) (1) (ii) of the Rights  in Technical Data  and Computer     *|
|*     Software  clause  at DFARS  52.227-7013 and in similar clauses in     *|
|*     the FAR and NASA FAR Supplement.                                      *|
|*                                                                           *|
 \***************************************************************************/

/*
 * lwmisc.h
 */
#if !defined(LW_MISC_H) && !defined(__LW_MISC_H)
#define LW_MISC_H

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "lwtypes.h"

#if !defined(LWIDIA_UNDEF_LEGACY_BIT_MACROS)
//
// Miscellaneous macros useful for bit field manipulations
//
// STUPID HACK FOR CL 19434692.  Will revert when fix CL is delivered bfm -> chips_a.
#ifndef BIT
#define BIT(b)                  (1U<<(b))
#endif
#ifndef BIT32
#define BIT32(b)                ((LwU32)1U<<(b))
#endif
#ifndef BIT64
#define BIT64(b)                ((LwU64)1U<<(b))
#endif

#endif

//
// It is recommended to use the following bit macros to avoid macro name
// collisions with other src code bases.
//
#ifndef LWBIT
#define LWBIT(b)                  (1U<<(b))
#endif
#ifndef LWBIT_TYPE
#define LWBIT_TYPE(b, t)          (((t)1U)<<(b))
#endif
#ifndef LWBIT32
#define LWBIT32(b)                LWBIT_TYPE(b, LwU32)
#endif
#ifndef LWBIT64
#define LWBIT64(b)                LWBIT_TYPE(b, LwU64)
#endif

// Index of the 'on' bit (assuming that there is only one).
// Even if multiple bits are 'on', result is in range of 0-31.
#define BIT_IDX_32(n)                            \
   (((((n) & 0xFFFF0000U) != 0U) ? 0x10U: 0U) |  \
    ((((n) & 0xFF00FF00U) != 0U) ? 0x08U: 0U) |  \
    ((((n) & 0xF0F0F0F0U) != 0U) ? 0x04U: 0U) |  \
    ((((n) & 0xCCCCCCCLW) != 0U) ? 0x02U: 0U) |  \
    ((((n) & 0xAAAAAAAAU) != 0U) ? 0x01U: 0U) )

// Index of the 'on' bit (assuming that there is only one).
// Even if multiple bits are 'on', result is in range of 0-63.
#define BIT_IDX_64(n)                                       \
   (((((n) & 0xFFFFFFFF00000000ULL) != 0U) ? 0x20U: 0U) |   \
    ((((n) & 0xFFFF0000FFFF0000ULL) != 0U) ? 0x10U: 0U) |   \
    ((((n) & 0xFF00FF00FF00FF00ULL) != 0U) ? 0x08U: 0U) |   \
    ((((n) & 0xF0F0F0F0F0F0F0F0ULL) != 0U) ? 0x04U: 0U) |   \
    ((((n) & 0xCCCCCCCCCCCCCCCLWLL) != 0U) ? 0x02U: 0U) |   \
    ((((n) & 0xAAAAAAAAAAAAAAAAULL) != 0U) ? 0x01U: 0U) )

// Desctructive, assumes only one bit set in n32
// Deprecated in favor of BIT_IDX_32.
#define IDX_32(n32)                     \
{                                       \
    LwU32 idx = 0;                      \
    if (((n32) & 0xFFFF0000U) != 0U)    \
    {                                   \
         idx += 16U;                    \
    }                                   \
    if (((n32) & 0xFF00FF00U) != 0U)    \
    {                                   \
         idx += 8U;                     \
    }                                   \
    if (((n32) & 0xF0F0F0F0U) != 0U)    \
    {                                   \
         idx += 4U;                     \
    }                                   \
    if (((n32) & 0xCCCCCCCLW) != 0U)    \
    {                                   \
         idx += 2U;                     \
    }                                   \
    if (((n32) & 0xAAAAAAAAU) != 0U)    \
    {                                   \
         idx += 1U;                     \
    }                                   \
    (n32) = idx;                        \
}


#define CHECK_WRAP_AND_RETURN(cond) do                                   \
{                                                                        \
    if (cond)                                                            \
    {                                                                    \
        return 0xDEADBEEFU;                                              \
    }                                                                    \
}                                                                        \
while(LW_FALSE)

/*!
 * DRF MACRO README:
 *
 * Glossary:
 *      DRF: Device, Register, Field
 *      FLD: Field
 *      REF: Reference
 *
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA                   0xDEADBEEF
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_GAMMA             27:0
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA             31:28
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_ZERO   0x00000000
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_ONE    0x00000001
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_TWO    0x00000002
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_THREE  0x00000003
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_FOUR   0x00000004
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_FIVE   0x00000005
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_SIX    0x00000006
 * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_SEVEN  0x00000007
 *
 *
 * Device = _DEVICE_OMEGA
 *   This is the common "base" that a group of registers in a manual share
 *
 * Register = _REGISTER_ALPHA
 *   Register for a given block of defines is the common root for one or more fields and constants
 *
 * Field(s) = _FIELD_GAMMA, _FIELD_ZETA
 *   These are the bit ranges for a given field within the register
 *   Fields are not required to have defined constant values (enumerations)
 *
 * Constant(s) = _ZERO, _ONE, _TWO, ...
 *   These are named values (enums) a field can contain; the width of the constants should not be larger than the field width
 *
 * MACROS:
 *
 * DRF_SHIFT:
 *      Bit index of the lower bound of a field
 *      DRF_SHIFT(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 28
 *
 * DRF_SHIFT_RT:
 *      Bit index of the higher bound of a field
 *      DRF_SHIFT_RT(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 31
 *
 * DRF_MASK:
 *      Produces a mask of 1-s equal to the width of a field
 *      DRF_MASK(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 0xF (four 1s starting at bit 0)
 *
 * DRF_SHIFTMASK:
 *      Produces a mask of 1s equal to the width of a field at the location of the field
 *      DRF_SHIFTMASK(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 0xF0000000
 *
 * DRF_DEF:
 *      Shifts a field constant's value to the correct field offset
 *      DRF_DEF(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, _THREE) == 0x30000000
 *
 * DRF_NUM:
 *      Shifts a number to the location of a particular field
 *      DRF_NUM(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, 3) == 0x30000000
 *      NOTE: If the value passed in is wider than the field, the value's high bits will be truncated
 *
 * DRF_SIZE:
 *      Provides the width of the field in bits
 *      DRF_SIZE(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 4
 *
 * DRF_VAL:
 *      Provides the value of an input within the field specified
 *      DRF_VAL(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, 0xABCD1234) == 0xA
 *      This is sort of like the ilwerse of DRF_NUM
 *
 * DRF_IDX...:
 *      These macros are similar to the above but for fields that accept an index argumment
 *
 * FLD_SET_DRF:
 *      Set the field bits in a given value with the given field constant
 *      LwU32 x = 0x00001234;
 *      x = FLD_SET_DRF(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, _THREE, x);
 *      x == 0x30001234;
 *
 * FLD_SET_DRF_NUM:
 *      Same as FLD_SET_DRF but instead of using a field constant a literal/variable is passed in
 *      LwU32 x = 0x00001234;
 *      x = FLD_SET_DRF_NUM(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, 0xF, x);
 *      x == 0xF0001234;
 *
 * FLD_IDX...:
 *      These macros are similar to the above but for fields that accept an index argumment
 *
 * FLD_TEST_DRF:
 *      Test if location specified by drf in 'v' has the same value as LW_drfc
 *      FLD_TEST_DRF(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, _THREE, 0x3000ABCD) == LW_TRUE
 *
 * FLD_TEST_DRF_NUM:
 *      Test if locations specified by drf in 'v' have the same value as n
 *      FLD_TEST_DRF_NUM(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, 0x3, 0x3000ABCD) == LW_TRUE
 *
 * REF_DEF:
 *      Like DRF_DEF but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
 *      REF_DEF(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, _THREE) == 0x30000000
 *
 * REF_VAL:
 *      Like DRF_VAL but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
 *      REF_VAL(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, 0xABCD1234) == 0xA
 *
 * REF_NUM:
 *      Like DRF_NUM but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
 *      REF_NUM(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, 0xA) == 0xA00000000
 *
 * FLD_SET_REF_NUM:
 *      Like FLD_SET_DRF_NUM but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
 *      LwU32 x = 0x00001234;
 *      x = FLD_SET_REF_NUM(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, 0xF, x);
 *      x == 0xF0001234;
 *
 * FLD_TEST_REF:
 *      Like FLD_TEST_DRF but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
 *      FLD_TEST_REF(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, _THREE, 0x3000ABCD) == LW_TRUE
 *
 * Other macros:
 *      There a plethora of other macros below that extend the above (notably Multi-Word (MW), 64-bit, and some
 *      reg read/write variations). I hope these are self explanatory. If you have a need to use them, you
 *      probably have some knowlege of how they work.
 */

// cheetah mobile uses lwmisc_macros.h and can't access lwmisc.h... and sometimes both get included.
#ifndef _LWMISC_MACROS_H
// Use Coverity Annotation to mark issues as false positives/ignore when using single bit defines.
#define DRF_ISBIT(bitval,drf)                \
        ( /* coverity[identical_branches] */ \
          bitval ? drf )
#define DEVICE_BASE(d)          (0?d)  // what's up with this name? totally non-parallel to the macros below
#define DEVICE_EXTENT(d)        (1?d)  // what's up with this name? totally non-parallel to the macros below
#ifdef LW_MISRA_COMPLIANCE_REQUIRED
#ifdef MISRA_14_3
#define DRF_BASE(drf)           (drf##_LOW_FIELD)
#define DRF_EXTENT(drf)         (drf##_HIGH_FIELD)
#define DRF_SHIFT(drf)          ((drf##_LOW_FIELD) % 32U)
#define DRF_SHIFT_RT(drf)       ((drf##_HIGH_FIELD) % 32U)
#define DRF_MASK(drf)           (0xFFFFFFFFU >> (31U - ((drf##_HIGH_FIELD) % 32U) + ((drf##_LOW_FIELD) % 32U)))
#else
#define DRF_BASE(drf)           (LW_FALSE?drf)  // much better
#define DRF_EXTENT(drf)         (LW_TRUE?drf)  // much better
#define DRF_SHIFT(drf)          (((LwU32)DRF_BASE(drf)) % 32U)
#define DRF_SHIFT_RT(drf)       (((LwU32)DRF_EXTENT(drf)) % 32U)
#define DRF_MASK(drf)           (0xFFFFFFFFU>>(31U - DRF_SHIFT_RT(drf) + DRF_SHIFT(drf)))
#endif
#define DRF_DEF(d,r,f,c)        (((LwU32)(LW ## d ## r ## f ## c))<<DRF_SHIFT(LW ## d ## r ## f))
#define DRF_NUM(d,r,f,n)        ((((LwU32)(n))&DRF_MASK(LW ## d ## r ## f))<<DRF_SHIFT(LW ## d ## r ## f))
#else
#define DRF_BASE(drf)           (0?drf)  // much better
#define DRF_EXTENT(drf)         (1?drf)  // much better
#define DRF_SHIFT(drf)          ((DRF_ISBIT(0,drf)) % 32)
#define DRF_SHIFT_RT(drf)       ((DRF_ISBIT(1,drf)) % 32)
#define DRF_MASK(drf)           (0xFFFFFFFFU>>(31-((DRF_ISBIT(1,drf)) % 32)+((DRF_ISBIT(0,drf)) % 32)))
#define DRF_DEF(d,r,f,c)        ((LW ## d ## r ## f ## c)<<DRF_SHIFT(LW ## d ## r ## f))
#define DRF_NUM(d,r,f,n)        (((n)&DRF_MASK(LW ## d ## r ## f))<<DRF_SHIFT(LW ## d ## r ## f))
#endif
#define DRF_SHIFTMASK(drf)      (DRF_MASK(drf)<<(DRF_SHIFT(drf)))
#define DRF_SIZE(drf)           (DRF_EXTENT(drf)-DRF_BASE(drf)+1)

#define DRF_VAL(d,r,f,v)        (((v)>>DRF_SHIFT(LW ## d ## r ## f))&DRF_MASK(LW ## d ## r ## f))
#endif

// Signed version of DRF_VAL, which takes care of extending sign bit.
#define DRF_VAL_SIGNED(d,r,f,v)         (((DRF_VAL(d,r,f,(v)) ^ (LWBIT(DRF_SIZE(LW ## d ## r ## f)-1)))) - (LWBIT(DRF_SIZE(LW ## d ## r ## f)-1)))
#define DRF_IDX_DEF(d,r,f,i,c)          ((LW ## d ## r ## f ## c)<<DRF_SHIFT(LW##d##r##f(i)))
#define DRF_IDX_OFFSET_DEF(d,r,f,i,o,c) ((LW ## d ## r ## f ## c)<<DRF_SHIFT(LW##d##r##f(i,o)))
#define DRF_IDX_NUM(d,r,f,i,n)          (((n)&DRF_MASK(LW##d##r##f(i)))<<DRF_SHIFT(LW##d##r##f(i)))
#define DRF_IDX_VAL(d,r,f,i,v)          (((v)>>DRF_SHIFT(LW##d##r##f(i)))&DRF_MASK(LW##d##r##f(i)))
#define DRF_IDX_OFFSET_VAL(d,r,f,i,o,v) (((v)>>DRF_SHIFT(LW##d##r##f(i,o)))&DRF_MASK(LW##d##r##f(i,o)))
// Fractional version of DRF_VAL which reads Fx.y fixed point number (x.y)*z
#define DRF_VAL_FRAC(d,r,x,y,v,z)       ((DRF_VAL(d,r,x,(v))*z) + ((DRF_VAL(d,r,y,v)*z) / (1<<DRF_SIZE(LW##d##r##y))))

//
// 64 Bit Versions
//
#define DRF_SHIFT64(drf)                ((DRF_ISBIT(0,drf)) % 64)
#define DRF_MASK64(drf)                 (LW_U64_MAX>>(63-((DRF_ISBIT(1,drf)) % 64)+((DRF_ISBIT(0,drf)) % 64)))
#define DRF_SHIFTMASK64(drf)            (DRF_MASK64(drf)<<(DRF_SHIFT64(drf)))

#define DRF_DEF64(d,r,f,c)              (((LwU64)(LW ## d ## r ## f ## c))<<DRF_SHIFT64(LW ## d ## r ## f))
#define DRF_NUM64(d,r,f,n)              ((((LwU64)(n))&DRF_MASK64(LW ## d ## r ## f))<<DRF_SHIFT64(LW ## d ## r ## f))
#define DRF_VAL64(d,r,f,v)              ((((LwU64)(v))>>DRF_SHIFT64(LW ## d ## r ## f))&DRF_MASK64(LW ## d ## r ## f))

#define DRF_VAL_SIGNED64(d,r,f,v)       (((DRF_VAL64(d,r,f,(v)) ^ (LWBIT64(DRF_SIZE(LW ## d ## r ## f)-1)))) - (LWBIT64(DRF_SIZE(LW ## d ## r ## f)-1)))
#define DRF_IDX_DEF64(d,r,f,i,c)        (((LwU64)(LW ## d ## r ## f ## c))<<DRF_SHIFT64(LW##d##r##f(i)))
#define DRF_IDX_OFFSET_DEF64(d,r,f,i,o,c) ((LwU64)(LW ## d ## r ## f ## c)<<DRF_SHIFT64(LW##d##r##f(i,o)))
#define DRF_IDX_NUM64(d,r,f,i,n)        ((((LwU64)(n))&DRF_MASK64(LW##d##r##f(i)))<<DRF_SHIFT64(LW##d##r##f(i)))
#define DRF_IDX_VAL64(d,r,f,i,v)        ((((LwU64)(v))>>DRF_SHIFT64(LW##d##r##f(i)))&DRF_MASK64(LW##d##r##f(i)))
#define DRF_IDX_OFFSET_VAL64(d,r,f,i,o,v) (((LwU64)(v)>>DRF_SHIFT64(LW##d##r##f(i,o)))&DRF_MASK64(LW##d##r##f(i,o)))

#define FLD_SET_DRF64(d,r,f,c,v)        (((LwU64)(v) & ~DRF_SHIFTMASK64(LW##d##r##f)) | DRF_DEF64(d,r,f,c))
#define FLD_SET_DRF_NUM64(d,r,f,n,v)    ((((LwU64)(v)) & ~DRF_SHIFTMASK64(LW##d##r##f)) | DRF_NUM64(d,r,f,n))
#define FLD_IDX_SET_DRF64(d,r,f,i,c,v)  (((LwU64)(v) & ~DRF_SHIFTMASK64(LW##d##r##f(i))) | DRF_IDX_DEF64(d,r,f,i,c))
#define FLD_IDX_OFFSET_SET_DRF64(d,r,f,i,o,c,v) (((LwU64)(v) & ~DRF_SHIFTMASK64(LW##d##r##f(i,o))) | DRF_IDX_OFFSET_DEF64(d,r,f,i,o,c))
#define FLD_IDX_SET_DRF_DEF64(d,r,f,i,c,v) (((LwU64)(v) & ~DRF_SHIFTMASK64(LW##d##r##f(i))) | DRF_IDX_DEF64(d,r,f,i,c))
#define FLD_IDX_SET_DRF_NUM64(d,r,f,i,n,v) (((LwU64)(v) & ~DRF_SHIFTMASK64(LW##d##r##f(i))) | DRF_IDX_NUM64(d,r,f,i,n))
#define FLD_SET_DRF_IDX64(d,r,f,c,i,v)  (((LwU64)(v) & ~DRF_SHIFTMASK64(LW##d##r##f)) | DRF_DEF64(d,r,f,c(i)))

#define FLD_TEST_DRF64(d,r,f,c,v)       (DRF_VAL64(d, r, f, (v)) == LW##d##r##f##c)
#define FLD_TEST_DRF_AND64(d,r,f,c,v)   (DRF_VAL64(d, r, f, (v)) & LW##d##r##f##c)
#define FLD_TEST_DRF_NUM64(d,r,f,n,v)   (DRF_VAL64(d, r, f, (v)) == (n))
#define FLD_IDX_TEST_DRF64(d,r,f,i,c,v) (DRF_IDX_VAL64(d, r, f, i, (v)) == LW##d##r##f##c)
#define FLD_IDX_OFFSET_TEST_DRF64(d,r,f,i,o,c,v) (DRF_IDX_OFFSET_VAL64(d, r, f, i, o, (v)) == LW##d##r##f##c)

#define REF_DEF64(drf,d)            (((drf ## d)&DRF_MASK64(drf))<<DRF_SHIFT64(drf))
#define REF_VAL64(drf,v)            (((LwU64)(v)>>DRF_SHIFT64(drf))&DRF_MASK64(drf))
#if defined(LW_MISRA_COMPLIANCE_REQUIRED) && defined(MISRA_14_3)
#define REF_NUM64(drf,n)            (((LwU64)(n)&(0xFFFFFFFFFFFFFFFFU>>(63U-((drf##_HIGH_FIELD) % 63U)+((drf##_LOW_FIELD) % 63U)))) << ((drf##_LOW_FIELD) % 63U))
#else
#define REF_NUM64(drf,n)            (((LwU64)(n)&DRF_MASK64(drf))<<DRF_SHIFT64(drf))
#endif
#define FLD_TEST_REF64(drf,c,v)     (REF_VAL64(drf, v) == drf##c)
#define FLD_TEST_REF_AND64(drf,c,v) (REF_VAL64(drf, v) & drf##c)
#define FLD_SET_REF_NUM64(drf,n,v)  (((LwU64)(v) & ~DRF_SHIFTMASK64(drf)) | REF_NUM64(drf,n))

//
// 32 Bit Versions
//

#ifdef LW_MISRA_COMPLIANCE_REQUIRED
#define FLD_SET_DRF(d,r,f,c,v)                  (((LwU32)(v) & ~DRF_SHIFTMASK(LW##d##r##f)) | DRF_DEF(d,r,f,c))
#define FLD_SET_DRF_NUM(d,r,f,n,v)              (((LwU32)(v) & ~DRF_SHIFTMASK(LW##d##r##f)) | DRF_NUM(d,r,f,n))
#define FLD_IDX_SET_DRF(d,r,f,i,c,v)            (((LwU32)(v) & ~DRF_SHIFTMASK(LW##d##r##f(i))) | DRF_IDX_DEF(d,r,f,i,c))
#define FLD_IDX_OFFSET_SET_DRF(d,r,f,i,o,c,v)   (((LwU32)(v) & ~DRF_SHIFTMASK(LW##d##r##f(i,o))) | DRF_IDX_OFFSET_DEF(d,r,f,i,o,c))
#define FLD_IDX_SET_DRF_DEF(d,r,f,i,c,v)        (((LwU32)(v) & ~DRF_SHIFTMASK(LW##d##r##f(i))) | DRF_IDX_DEF(d,r,f,i,c))
#define FLD_IDX_SET_DRF_NUM(d,r,f,i,n,v)        (((LwU32)(v) & ~DRF_SHIFTMASK(LW##d##r##f(i))) | DRF_IDX_NUM(d,r,f,i,n))
#define FLD_SET_DRF_IDX(d,r,f,c,i,v)            (((LwU32)(v) & ~DRF_SHIFTMASK(LW##d##r##f)) | DRF_DEF(d,r,f,c(i)))

#define FLD_TEST_DRF(d,r,f,c,v)                 ((DRF_VAL(d, r, f, (v)) == (LwU32)(LW##d##r##f##c)))
#define FLD_TEST_DRF_AND(d,r,f,c,v)             ((DRF_VAL(d, r, f, (v)) & (LwU32)(LW##d##r##f##c)) != 0U)
#define FLD_TEST_DRF_NUM(d,r,f,n,v)             ((DRF_VAL(d, r, f, (v)) == (LwU32)(n)))
#define FLD_IDX_TEST_DRF(d,r,f,i,c,v)           ((DRF_IDX_VAL(d, r, f, i, (v)) == (LwU32)(LW##d##r##f##c)))
#define FLD_IDX_OFFSET_TEST_DRF(d,r,f,i,o,c,v)  ((DRF_IDX_OFFSET_VAL(d, r, f, i, o, (v)) == (LwU32)(LW##d##r##f##c)))
#else
#define FLD_SET_DRF(d,r,f,c,v)                  (((v) & ~DRF_SHIFTMASK(LW##d##r##f)) | DRF_DEF(d,r,f,c))
#define FLD_SET_DRF_NUM(d,r,f,n,v)              (((v) & ~DRF_SHIFTMASK(LW##d##r##f)) | DRF_NUM(d,r,f,n))
#define FLD_IDX_SET_DRF(d,r,f,i,c,v)            (((v) & ~DRF_SHIFTMASK(LW##d##r##f(i))) | DRF_IDX_DEF(d,r,f,i,c))
#define FLD_IDX_OFFSET_SET_DRF(d,r,f,i,o,c,v)   (((v) & ~DRF_SHIFTMASK(LW##d##r##f(i,o))) | DRF_IDX_OFFSET_DEF(d,r,f,i,o,c))
#define FLD_IDX_SET_DRF_DEF(d,r,f,i,c,v)        (((v) & ~DRF_SHIFTMASK(LW##d##r##f(i))) | DRF_IDX_DEF(d,r,f,i,c))
#define FLD_IDX_SET_DRF_NUM(d,r,f,i,n,v)        (((v) & ~DRF_SHIFTMASK(LW##d##r##f(i))) | DRF_IDX_NUM(d,r,f,i,n))
#define FLD_SET_DRF_IDX(d,r,f,c,i,v)            (((v) & ~DRF_SHIFTMASK(LW##d##r##f)) | DRF_DEF(d,r,f,c(i)))

#define FLD_TEST_DRF(d,r,f,c,v)                 ((DRF_VAL(d, r, f, (v)) == LW##d##r##f##c))
#define FLD_TEST_DRF_AND(d,r,f,c,v)             ((DRF_VAL(d, r, f, (v)) & LW##d##r##f##c))
#define FLD_TEST_DRF_NUM(d,r,f,n,v)             ((DRF_VAL(d, r, f, (v)) == (n)))
#define FLD_IDX_TEST_DRF(d,r,f,i,c,v)           ((DRF_IDX_VAL(d, r, f, i, (v)) == LW##d##r##f##c))
#define FLD_IDX_OFFSET_TEST_DRF(d,r,f,i,o,c,v)  ((DRF_IDX_OFFSET_VAL(d, r, f, i, o, (v)) == LW##d##r##f##c))
#endif

#define REF_DEF(drf,d)            (((drf ## d)&DRF_MASK(drf))<<DRF_SHIFT(drf))
#define REF_VAL(drf,v)            (((v)>>DRF_SHIFT(drf))&DRF_MASK(drf))
#if defined(LW_MISRA_COMPLIANCE_REQUIRED) && defined(MISRA_14_3)
#define REF_NUM(drf,n)            (((n)&(0xFFFFFFFFU>>(31U-((drf##_HIGH_FIELD) % 32U)+((drf##_LOW_FIELD) % 32U)))) << ((drf##_LOW_FIELD) % 32U))
#else
#define REF_NUM(drf,n)            (((n)&DRF_MASK(drf))<<DRF_SHIFT(drf))
#endif
#define FLD_TEST_REF(drf,c,v)     (REF_VAL(drf, (v)) == drf##c)
#define FLD_TEST_REF_AND(drf,c,v) (REF_VAL(drf, (v)) &  drf##c)
#define FLD_SET_REF_NUM(drf,n,v)  (((v) & ~DRF_SHIFTMASK(drf)) | REF_NUM(drf,n))

#define CR_DRF_DEF(d,r,f,c)     ((CR ## d ## r ## f ## c)<<DRF_SHIFT(CR ## d ## r ## f))
#define CR_DRF_NUM(d,r,f,n)     (((n)&DRF_MASK(CR ## d ## r ## f))<<DRF_SHIFT(CR ## d ## r ## f))
#define CR_DRF_VAL(d,r,f,v)     (((v)>>DRF_SHIFT(CR ## d ## r ## f))&DRF_MASK(CR ## d ## r ## f))

// Multi-word (MW) field manipulations.  For multi-word structures (e.g., Fermi SPH),
// fields may have bit numbers beyond 32.  To avoid errors using "classic" multi-word macros,
// all the field extents are defined as "MW(X)".  For example, MW(127:96) means
// the field is in bits 0-31 of word number 3 of the structure.
//
// DRF_VAL_MW() macro is meant to be used for native endian 32-bit aligned 32-bit word data,
// not for byte stream data.
//
// DRF_VAL_BS() macro is for byte stream data used in fbQueryBIOS_XXX().
//
#define DRF_EXPAND_MW(drf)         drf                          // used to turn "MW(a:b)" into "a:b"
#define DRF_PICK_MW(drf,v)         ((v)? DRF_EXPAND_##drf)      // picks low or high bits
#define DRF_WORD_MW(drf)           (DRF_PICK_MW(drf,0)/32)      // which word in a multi-word array
#define DRF_BASE_MW(drf)           (DRF_PICK_MW(drf,0)%32)      // which start bit in the selected word?
#define DRF_EXTENT_MW(drf)         (DRF_PICK_MW(drf,1)%32)      // which end bit in the selected word
#define DRF_SHIFT_MW(drf)          (DRF_PICK_MW(drf,0)%32)
#define DRF_MASK_MW(drf)           (0xFFFFFFFFU>>((31-(DRF_EXTENT_MW(drf))+(DRF_BASE_MW(drf)))%32))
#define DRF_SHIFTMASK_MW(drf)      ((DRF_MASK_MW(drf))<<(DRF_SHIFT_MW(drf)))
#define DRF_SIZE_MW(drf)           (DRF_EXTENT_MW(drf)-DRF_BASE_MW(drf)+1)

#define DRF_DEF_MW(d,r,f,c)        ((LW##d##r##f##c) << DRF_SHIFT_MW(LW##d##r##f))
#define DRF_NUM_MW(d,r,f,n)        (((n)&DRF_MASK_MW(LW##d##r##f))<<DRF_SHIFT_MW(LW##d##r##f))
//
// DRF_VAL_MW is the ONLY multi-word macro which supports spanning. No other MW macro supports spanning lwrrently
//
#define DRF_VAL_MW_1WORD(d,r,f,v)       ((((v)[DRF_WORD_MW(LW##d##r##f)])>>DRF_SHIFT_MW(LW##d##r##f))&DRF_MASK_MW(LW##d##r##f))
#define DRF_SPANS(drf)                  ((DRF_PICK_MW(drf,0)/32) != (DRF_PICK_MW(drf,1)/32))
#define DRF_WORD_MW_LOW(drf)            (DRF_PICK_MW(drf,0)/32)
#define DRF_WORD_MW_HIGH(drf)           (DRF_PICK_MW(drf,1)/32)
#define DRF_MASK_MW_LOW(drf)            (0xFFFFFFFFU)
#define DRF_MASK_MW_HIGH(drf)           (0xFFFFFFFFU>>(31-(DRF_EXTENT_MW(drf))))
#define DRF_SHIFT_MW_LOW(drf)           (DRF_PICK_MW(drf,0)%32)
#define DRF_SHIFT_MW_HIGH(drf)          (0)
#define DRF_MERGE_SHIFT(drf)            ((32-((DRF_PICK_MW(drf,0)%32)))%32)
#define DRF_VAL_MW_2WORD(d,r,f,v)       (((((v)[DRF_WORD_MW_LOW(LW##d##r##f)])>>DRF_SHIFT_MW_LOW(LW##d##r##f))&DRF_MASK_MW_LOW(LW##d##r##f)) | \
    (((((v)[DRF_WORD_MW_HIGH(LW##d##r##f)])>>DRF_SHIFT_MW_HIGH(LW##d##r##f))&DRF_MASK_MW_HIGH(LW##d##r##f)) << DRF_MERGE_SHIFT(LW##d##r##f)))
#define DRF_VAL_MW(d,r,f,v)             ( DRF_SPANS(LW##d##r##f) ? DRF_VAL_MW_2WORD(d,r,f,v) : DRF_VAL_MW_1WORD(d,r,f,v) )

#define DRF_IDX_DEF_MW(d,r,f,i,c)  ((LW##d##r##f##c)<<DRF_SHIFT_MW(LW##d##r##f(i)))
#define DRF_IDX_NUM_MW(d,r,f,i,n)  (((n)&DRF_MASK_MW(LW##d##r##f(i)))<<DRF_SHIFT_MW(LW##d##r##f(i)))
#define DRF_IDX_VAL_MW(d,r,f,i,v)  ((((v)[DRF_WORD_MW(LW##d##r##f(i))])>>DRF_SHIFT_MW(LW##d##r##f(i)))&DRF_MASK_MW(LW##d##r##f(i)))

//
// Logically OR all DRF_DEF constants indexed from zero to s (semiinclusive).
// Caution: Target variable v must be pre-initialized.
//
#define FLD_IDX_OR_DRF_DEF(d,r,f,c,s,v)                 \
do                                                      \
{   LwU32 idx;                                          \
    for (idx = 0; idx < (LW ## d ## r ## f ## s); ++idx)\
    {                                                   \
        v |= DRF_IDX_DEF(d,r,f,idx,c);                  \
    }                                                   \
} while(0)


#define FLD_MERGE_MW(drf,n,v)               (((v)[DRF_WORD_MW(drf)] & ~DRF_SHIFTMASK_MW(drf)) | n)
#define FLD_ASSIGN_MW(drf,n,v)              ((v)[DRF_WORD_MW(drf)] = FLD_MERGE_MW(drf, n, v))
#define FLD_IDX_MERGE_MW(drf,i,n,v)         (((v)[DRF_WORD_MW(drf(i))] & ~DRF_SHIFTMASK_MW(drf(i))) | n)
#define FLD_IDX_ASSIGN_MW(drf,i,n,v)        ((v)[DRF_WORD_MW(drf(i))] = FLD_MERGE_MW(drf(i), n, v))

#define FLD_SET_DRF_MW(d,r,f,c,v)              FLD_MERGE_MW(LW##d##r##f, DRF_DEF_MW(d,r,f,c), v)
#define FLD_SET_DRF_NUM_MW(d,r,f,n,v)          FLD_ASSIGN_MW(LW##d##r##f, DRF_NUM_MW(d,r,f,n), v)
#define FLD_SET_DRF_DEF_MW(d,r,f,c,v)          FLD_ASSIGN_MW(LW##d##r##f, DRF_DEF_MW(d,r,f,c), v)
#define FLD_IDX_SET_DRF_MW(d,r,f,i,c,v)        FLD_IDX_MERGE_MW(LW##d##r##f, i, DRF_IDX_DEF_MW(d,r,f,i,c), v)
#define FLD_IDX_SET_DRF_DEF_MW(d,r,f,i,c,v)    FLD_IDX_MERGE_MW(LW##d##r##f, i, DRF_IDX_DEF_MW(d,r,f,i,c), v)
#define FLD_IDX_SET_DRF_NUM_MW(d,r,f,i,n,v)    FLD_IDX_ASSIGN_MW(LW##d##r##f, i, DRF_IDX_NUM_MW(d,r,f,i,n), v)

#define FLD_TEST_DRF_MW(d,r,f,c,v)          ((DRF_VAL_MW(d, r, f, (v)) == LW##d##r##f##c))
#define FLD_TEST_DRF_NUM_MW(d,r,f,n,v)      ((DRF_VAL_MW(d, r, f, (v)) == n))
#define FLD_IDX_TEST_DRF_MW(d,r,f,i,c,v)    ((DRF_IDX_VAL_MW(d, r, f, i, (v)) == LW##d##r##f##c))

#define DRF_VAL_BS(d,r,f,v)                 ( DRF_SPANS(LW##d##r##f) ? DRF_VAL_BS_2WORD(d,r,f,(v)) : DRF_VAL_BS_1WORD(d,r,f,(v)) )

//-------------------------------------------------------------------------//
//                                                                         //
// Common defines for engine register reference wrappers                   //
//                                                                         //
// New engine addressing can be created like:                              //
// #define ENG_REG_PMC(o,d,r)                      LW##d##r                //
// #define ENG_IDX_REG_CE(o,d,i,r)                 CE_MAP(o,r,i)           //
//                                                                         //
// See FB_FBPA* for more examples                                          //
//-------------------------------------------------------------------------//

#ifdef LW_MISRA_COMPLIANCE_REQUIRED
#define FALC_ENG_RD_REG(g,o,d,r)             GPU_REG_RD32(g, ENGN_REG##d(o,d,r))
#define FALC_ENG_WR_REG(g,o,d,r,v)           GPU_REG_WR32(g, ENGN_REG##d(o,d,r), (v))
#define FALC_ENG_RD_DRF(g,o,d,r,f)           ((GPU_REG_RD32(g, FALC_ENG_REG##d(o,d,r))>>GPU_DRF_SHIFT(LW ## d ## r ## f))&GPU_DRF_MASK(LW ## d ## r ## f))
#define FALC_ENG_WR_DRF_DEF(g,o,d,r,f,c)     GPU_REG_WR32(g, FALC_ENG_REG##d(o,d,r),(GPU_REG_RD32(g,FALC_ENG_REG##d(o,d,r))&~(GPU_DRF_MASK(LW##d##r##f)<<GPU_DRF_SHIFT(LW##d##r##f)))|GPU_DRF_DEF(d,r,f,c))
#define FALC_ENG_WR_DRF_NUM(g,o,d,r,f,n)     GPU_REG_WR32(g, FALC_ENG_REG##d(o,d,r),(GPU_REG_RD32(g,FALC_ENG_REG##d(o,d,r))&~(GPU_DRF_MASK(LW##d##r##f)<<GPU_DRF_SHIFT(LW##d##r##f)))|GPU_DRF_NUM(d,r,f,n))
#define FALC_ENG_TEST_DRF_DEF(g,o,d,r,f,c)   (FALC_ENG_RD_DRF(g, o, d, r, f) == LW##d##r##f##c)
#define FALC_ENG_RD_IDX_DRF(g,o,d,r,f,i)     ((GPU_REG_RD32(g, FLAC_ENG_REG##d(o,d,r(i)))>>GPU_DRF_SHIFT(LW ## d ## r ## f))&GPU_DRF_MASK(LW ## d ## r ## f))
#define FALC_ENG_TEST_IDX_DRF_DEF(g,o,d,r,f,c,i) (FALC_ENG_RD_IDX_DRF(g, o, d, r, f, (i)) == LW##d##r##f##c)
#define FALC_ENG_IDX_RD_REG(g,o,d,i,r)       GPU_REG_RD32(g, FALC_ENG_IDX_REG##d(o,d,i,r))
#define FALC_ENG_IDX_WR_REG(g,o,d,i,r,v)     GPU_REG_WR32(g, FALC_ENG_IDX_REG##d(o,d,i,r), (v))
#define FALC_ENG_IDX_RD_DRF(g,o,d,i,r,f)     ((GPU_REG_RD32(g, FALC_ENG_IDX_REG##d(o,d,i,r))>>GPU_DRF_SHIFT(LW ## d ## r ## f))&GPU_DRF_MASK(LW ## d ## r ## f))

#else
#define ENG_RD_REG(g,o,d,r)             GPU_REG_RD32(g, ENG_REG##d(o,d,r))
#define ENG_WR_REG(g,o,d,r,v)           GPU_REG_WR32(g, ENG_REG##d(o,d,r), (v))
#define ENG_RD_DRF(g,o,d,r,f)           ((GPU_REG_RD32(g, ENG_REG##d(o,d,r))>>GPU_DRF_SHIFT(LW ## d ## r ## f))&GPU_DRF_MASK(LW ## d ## r ## f))
#define ENG_WR_DRF_DEF(g,o,d,r,f,c)     GPU_REG_WR32(g, ENG_REG##d(o,d,r),(GPU_REG_RD32(g,ENG_REG##d(o,d,r))&~(GPU_DRF_MASK(LW##d##r##f)<<GPU_DRF_SHIFT(LW##d##r##f)))|GPU_DRF_DEF(d,r,f,c))
#define ENG_WR_DRF_NUM(g,o,d,r,f,n)     GPU_REG_WR32(g, ENG_REG##d(o,d,r),(GPU_REG_RD32(g,ENG_REG##d(o,d,r))&~(GPU_DRF_MASK(LW##d##r##f)<<GPU_DRF_SHIFT(LW##d##r##f)))|GPU_DRF_NUM(d,r,f,n))
#define ENG_TEST_DRF_DEF(g,o,d,r,f,c)   (ENG_RD_DRF(g, o, d, r, f) == LW##d##r##f##c)

#define ENG_RD_IDX_DRF(g,o,d,r,f,i)     ((GPU_REG_RD32(g, ENG_REG##d(o,d,r(i)))>>GPU_DRF_SHIFT(LW ## d ## r ## f))&GPU_DRF_MASK(LW ## d ## r ## f))
#define ENG_TEST_IDX_DRF_DEF(g,o,d,r,f,c,i) (ENG_RD_IDX_DRF(g, o, d, r, f, (i)) == LW##d##r##f##c)

#define ENG_IDX_RD_REG(g,o,d,i,r)       GPU_REG_RD32(g, ENG_IDX_REG##d(o,d,i,r))
#define ENG_IDX_WR_REG(g,o,d,i,r,v)     GPU_REG_WR32(g, ENG_IDX_REG##d(o,d,i,r), (v))

#define ENG_IDX_RD_DRF(g,o,d,i,r,f)     ((GPU_REG_RD32(g, ENG_IDX_REG##d(o,d,i,r))>>GPU_DRF_SHIFT(LW ## d ## r ## f))&GPU_DRF_MASK(LW ## d ## r ## f))
#endif

//
// DRF_READ_1WORD_BS() and DRF_READ_1WORD_BS_HIGH() do not read beyond the bytes that contain
// the requested value. Reading beyond the actual data causes a page fault panic when the
// immediately following page happened to be protected or not mapped.
//
#define DRF_VAL_BS_1WORD(d,r,f,v)           ((DRF_READ_1WORD_BS(d,r,f,v)>>DRF_SHIFT_MW(LW##d##r##f))&DRF_MASK_MW(LW##d##r##f))
#define DRF_VAL_BS_2WORD(d,r,f,v)           (((DRF_READ_4BYTE_BS(LW##d##r##f,v)>>DRF_SHIFT_MW_LOW(LW##d##r##f))&DRF_MASK_MW_LOW(LW##d##r##f)) | \
    (((DRF_READ_1WORD_BS_HIGH(d,r,f,v)>>DRF_SHIFT_MW_HIGH(LW##d##r##f))&DRF_MASK_MW_HIGH(LW##d##r##f)) << DRF_MERGE_SHIFT(LW##d##r##f)))

#define DRF_READ_1BYTE_BS(drf,v)            ((LwU32)(((const LwU8*)(v))[DRF_WORD_MW(drf)*4]))
#define DRF_READ_2BYTE_BS(drf,v)            (DRF_READ_1BYTE_BS(drf,v)| \
    ((LwU32)(((const LwU8*)(v))[DRF_WORD_MW(drf)*4+1])<<8))
#define DRF_READ_3BYTE_BS(drf,v)            (DRF_READ_2BYTE_BS(drf,v)| \
    ((LwU32)(((const LwU8*)(v))[DRF_WORD_MW(drf)*4+2])<<16))
#define DRF_READ_4BYTE_BS(drf,v)            (DRF_READ_3BYTE_BS(drf,v)| \
    ((LwU32)(((const LwU8*)(v))[DRF_WORD_MW(drf)*4+3])<<24))

#define DRF_READ_1BYTE_BS_HIGH(drf,v)       ((LwU32)(((const LwU8*)(v))[DRF_WORD_MW_HIGH(drf)*4]))
#define DRF_READ_2BYTE_BS_HIGH(drf,v)       (DRF_READ_1BYTE_BS_HIGH(drf,v)| \
    ((LwU32)(((const LwU8*)(v))[DRF_WORD_MW_HIGH(drf)*4+1])<<8))
#define DRF_READ_3BYTE_BS_HIGH(drf,v)       (DRF_READ_2BYTE_BS_HIGH(drf,v)| \
    ((LwU32)(((const LwU8*)(v))[DRF_WORD_MW_HIGH(drf)*4+2])<<16))
#define DRF_READ_4BYTE_BS_HIGH(drf,v)       (DRF_READ_3BYTE_BS_HIGH(drf,v)| \
    ((LwU32)(((const LwU8*)(v))[DRF_WORD_MW_HIGH(drf)*4+3])<<24))

// Callwlate 2^n - 1 and avoid shift counter overflow
//
// On Windows amd64, 64 << 64 => 1
//
#define LW_TWO_N_MINUS_ONE(n) (((1ULL<<(n/2))<<((n+1)/2))-1)

#define DRF_READ_1WORD_BS(d,r,f,v) \
    ((DRF_EXTENT_MW(LW##d##r##f)<8)?DRF_READ_1BYTE_BS(LW##d##r##f,(v)): \
    ((DRF_EXTENT_MW(LW##d##r##f)<16)?DRF_READ_2BYTE_BS(LW##d##r##f,(v)): \
    ((DRF_EXTENT_MW(LW##d##r##f)<24)?DRF_READ_3BYTE_BS(LW##d##r##f,(v)): \
    DRF_READ_4BYTE_BS(LW##d##r##f,(v)))))

#define DRF_READ_1WORD_BS_HIGH(d,r,f,v) \
    ((DRF_EXTENT_MW(LW##d##r##f)<8)?DRF_READ_1BYTE_BS_HIGH(LW##d##r##f,(v)): \
    ((DRF_EXTENT_MW(LW##d##r##f)<16)?DRF_READ_2BYTE_BS_HIGH(LW##d##r##f,(v)): \
    ((DRF_EXTENT_MW(LW##d##r##f)<24)?DRF_READ_3BYTE_BS_HIGH(LW##d##r##f,(v)): \
    DRF_READ_4BYTE_BS_HIGH(LW##d##r##f,(v)))))

#define BIN_2_GRAY(n)           ((n)^((n)>>1))
// operates on a 64-bit data type
#define GRAY_2_BIN_64b(n)       (n)^=(n)>>1; (n)^=(n)>>2; (n)^=(n)>>4; (n)^=(n)>>8; (n)^=(n)>>16; (n)^=(n)>>32;

#define LOWESTBIT(x)            ( (x) &  (((x)-1) ^ (x)) )
// Destructive operation on n32
#define HIGHESTBIT(n32)     \
{                           \
    HIGHESTBITIDX_32(n32);  \
    n32 = LWBIT(n32);       \
}
#define ONEBITSET(x)            ( (x) && (((x) & ((x)-1)) == 0) )

// Destructive operation on n32
#define NUMSETBITS_32(n32)                                         \
{                                                                  \
    n32 = n32 - ((n32 >> 1) & 0x55555555);                         \
    n32 = (n32 & 0x33333333) + ((n32 >> 2) & 0x33333333);          \
    n32 = (((n32 + (n32 >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;  \
}

// Destructive operation on n32
#define LOWESTBITIDX_32(n32)  \
{                             \
    n32 = LOWESTBIT(n32);     \
    IDX_32(n32);              \
}

// Destructive operation on n32
#define HIGHESTBITIDX_32(n32)   \
{                               \
    LwU32 count = 0;            \
    while (n32 >>= 1)           \
    {                           \
        count++;                \
    }                           \
    n32 = count;                \
}

// Destructive operation on n32
#define ROUNDUP_POW2(n32) \
{                         \
    n32--;                \
    n32 |= n32 >> 1;      \
    n32 |= n32 >> 2;      \
    n32 |= n32 >> 4;      \
    n32 |= n32 >> 8;      \
    n32 |= n32 >> 16;     \
    n32++;                \
}

// Destructive operation on n64
#define ROUNDUP_POW2_U64(n64) \
{                         \
    n64--;                \
    n64 |= n64 >> 1;      \
    n64 |= n64 >> 2;      \
    n64 |= n64 >> 4;      \
    n64 |= n64 >> 8;      \
    n64 |= n64 >> 16;     \
    n64 |= n64 >> 32;     \
    n64++;                \
}

#define LW_SWAP_U8(a,b) \
{                       \
    LwU8 temp;          \
    temp = a;           \
    a = b;              \
    b = temp;           \
}

#define LW_SWAP_U32(a,b)    \
{                           \
    LwU32 temp;             \
    temp = a;               \
    a = b;                  \
    b = temp;               \
}

/*!
 * @brief   Macros allowing simple iteration over bits set in a given mask.
 *
 * @param[in]       maskWidth   bit-width of the mask (allowed: 8, 16, 32, 64)
 *
 * @param[in,out]   index       lvalue that is used as a bit index in the loop
 *                              (can be declared as any LwU* or LwS* variable)
 * @param[in]       mask        expression, loop will iterate over set bits only
 */
#define FOR_EACH_INDEX_IN_MASK(maskWidth,index,mask)        \
{                                                           \
    LwU##maskWidth lclMsk = (LwU##maskWidth)(mask);         \
    for ((index) = 0U; lclMsk != 0U; (index)++, lclMsk >>= 1U)\
    {                                                       \
        if (((LwU##maskWidth)LWBIT64(0) & lclMsk) == 0U)    \
        {                                                   \
            continue;                                       \
        }
#define FOR_EACH_INDEX_IN_MASK_END                          \
    }                                                       \
}

//
// Size to use when declaring variable-sized arrays
//
#define LW_ANYSIZE_ARRAY                                                      1

//
// Returns ceil(a/b)
//
#define LW_CEIL(a,b) (((a)+(b)-1)/(b))

// Clearer name for LW_CEIL
#ifndef LW_DIV_AND_CEIL
#define LW_DIV_AND_CEIL(a, b) LW_CEIL(a,b)
#endif

#ifndef LW_MIN
#define LW_MIN(a, b)        (((a) < (b)) ? (a) : (b))
#endif

#ifndef LW_MAX
#define LW_MAX(a, b)        (((a) > (b)) ? (a) : (b))
#endif

//
// Returns absolute value of provided integer expression
//
#define LW_ABS(a) ((a)>=0?(a):(-(a)))

//
// Returns 1 if input number is positive, 0 if 0 and -1 if negative. Avoid
// macro parameter as function call which will have side effects.
//
#define LW_SIGN(s) ((LwS8)(((s) > 0) - ((s) < 0)))

//
// Returns 1 if input number is >= 0 or -1 otherwise. This assumes 0 has a
// positive sign.
//
#define LW_ZERO_SIGN(s) ((LwS8)((((s) >= 0) * 2) - 1))

// Returns the offset (in bytes) of 'member' in struct 'type'.
#ifndef LW_OFFSETOF
    #if defined(__GNUC__) && (__GNUC__ > 3)
        #define LW_OFFSETOF(type, member)   ((LwU32)__builtin_offsetof(type, member))
    #else
        #define LW_OFFSETOF(type, member)    ((LwU32)(LwU64)&(((type *)0)->member)) // shouldn't we use PtrToUlong? But will need to include windows header.
    #endif
#endif

//
// Performs a rounded division of b into a (unsigned). For SIGNED version of
// LW_ROUNDED_DIV() macro check the comments in http://lwbugs/769777.
//
#define LW_UNSIGNED_ROUNDED_DIV(a,b)    (((a) + ((b) / 2U)) / (b))

/*!
 * Performs a ceiling division of b into a (unsigned).  A "ceiling" division is
 * a division is one with rounds up result up if a % b != 0.
 *
 * @param[in] a    Numerator
 * @param[in] b    Denominator
 *
 * @return a / b + a % b != 0 ? 1 : 0.
 */
#define LW_UNSIGNED_DIV_CEIL(a, b)      (((a) + (b - 1)) / (b))

/*!
 * Performs subtraction where a negative difference is raised to zero.
 * Can be used to avoid underflowing an unsigned subtraction.
 *
 * @param[in] a    Minuend
 * @param[in] b    Subtrahend
 *
 * @return a > b ? a - b : 0.
 */
#define LW_SUBTRACT_NO_UNDERFLOW(a, b) ((a)>(b) ? (a)-(b) : 0)

/*!
 * Performs a rounded right-shift of 32-bit unsigned value "a" by "shift" bits.
 * Will round result away from zero.
 *
 * @param[in] a      32-bit unsigned value to shift.
 * @param[in] shift  Number of bits by which to shift.
 *
 * @return  Resulting shifted value rounded away from zero.
 */
#define LW_RIGHT_SHIFT_ROUNDED(a, shift)                                       \
    (((a) >> (shift)) + !!((LWBIT((shift) - 1) & (a)) == LWBIT((shift) - 1)))

//
// Power of 2 alignment.
//    (Will give unexpected results if 'gran' is not a power of 2.)
//
#ifndef LW_ALIGN_DOWN
#define LW_ALIGN_DOWN(v, gran)      ((v) & ~((gran) - 1))
#endif

#ifndef LW_ALIGN_UP
#define LW_ALIGN_UP(v, gran)        (((v) + ((gran) - 1U)) & ~((gran)-1U))
#endif

#ifndef LW_ALIGN_DOWN64
#define LW_ALIGN_DOWN64(v, gran)      ((v) & ~(((LwU64)gran) - 1))
#endif

#ifndef LW_ALIGN_UP64
#define LW_ALIGN_UP64(v, gran)        (((v) + ((gran) - 1)) & ~(((LwU64)gran)-1))
#endif

#ifndef LW_IS_ALIGNED
#define LW_IS_ALIGNED(v, gran)      (0U == ((v) & ((gran) - 1U)))
#endif

#ifndef LW_IS_ALIGNED64
#define LW_IS_ALIGNED64(v, gran)      (0U == ((v) & (((LwU64)gran) - 1U)))
#endif

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // __LW_MISC_H

