/*
 * Copyright (c) 2007-2021, LWPU CORPORATION.  All rights reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_LWRM_DRF_H
#define INCLUDED_LWRM_DRF_H

/**
 *  @defgroup lwrm_drf RM DRF Macros
 *
 *  @ingroup lwddk_rm
 *
 * The following suite of macros are used for generating values to write into
 * hardware registers, or for extracting fields from read registers.  The
 * hardware headers have a RANGE define for each field in the register in the
 * form of x:y, 'x' being the high bit, 'y' the lower.  Through a clever use
 * of the C ternary operator, x:y may be passed into the macros below to
 * geneate masks, shift values, etc.
 *
 * There are two basic flavors of DRF macros, the first is used to define
 * a new register value from 0, the other is modifiying a field given a
 * register value.  An example of the first:
 *
 * reg = LW_DRF_DEF( HW, REGISTER0, FIELD0, VALUE0 )
 *     | LW_DRF_DEF( HW, REGISTER0, FIELD3, VALUE2 );
 *
 * To modify 'reg' from the previous example:
 *
 * reg = LW_FLD_SET_DRF_DEF( HW, REGISTER0, FIELD2, VALUE1, reg );
 *
 * To pass in numeric values instead of defined values from the header:
 *
 * reg = LW_DRF_NUM( HW, REGISTER3, FIELD2, 1024 );
 *
 * To read a value from a register:
 *
 * val = LW_DRF_VAL( HW, REGISTER3, FIELD2, reg );
 *
 * Some registers have non-zero reset values which may be extracted from the
 * hardware headers via LW_RESETVAL.
 *
 * This originated from lwrm_drf.h. Copy from bootrom and modified by bootrom team,
 * modified for CCC (not significantly).
 */

/**
 * Use the precomputed shift and field values instead of callwlating them from
 * the _RANGE parameter.  MISRA throws seveeral violations getting the low and
 * high bits from _RANGE, as well as falsely reporting the +1 in LW_FIELD_SIZE()
 * is a boolean.
**/
#define USE_PRECOMPUTED_NOT_DRF 1 /* set as default for CCC */

/*
 * The LW_FIELD_* macros are helper macros for the public LW_DRF_* macros.
 */
#define LW_FIELD_LOWBIT(x)      ((const uint32_t)(false?x))
#define LW_FIELD_HIGHBIT(x)     ((const uint32_t)(true?x))
#define LW_FIELD_SIZE(x)        ((const uint32_t)(LW_FIELD_HIGHBIT(x)-LW_FIELD_LOWBIT(x)+(uint32_t)1U))
#define LW_FIELD_SHIFT(x)       ((const uint32_t)(LW_FIELD_LOWBIT(x)%32U))
#define LW_FIELD_MASK(x)        ((const uint32_t)(0xFFFFFFFFU>>(31U-(LW_FIELD_HIGHBIT(x)%32U)+(LW_FIELD_LOWBIT(x)%32U))))
#define LW_FIELD_BITS(val, x)   ((uint32_t)((((uint32_t)(val)) & LW_FIELD_MASK(x)) << LW_FIELD_SHIFT(x)))
#define LW_FIELD_SHIFTMASK(x)   ((const uint32_t)(LW_FIELD_MASK(x) << (LW_FIELD_SHIFT(x))))


/*
 * The LW_FIELD64_* macros are helper macros for the public LW_DRF64_* macros.
 */
#define LW_FIELD64_SHIFT(x)     ((const uint64_t)(LW_FIELD_LOWBIT(x)%64U))
#define LW_FIELD64_MASK(x)      ((const uint64_t)(0xFFFFFFFFFFFFFFFFULL>>(63U-(LW_FIELD_HIGHBIT(x)%64U)+(LW_FIELD_LOWBIT(x)%64U))))


/** LW_DRF_DEF - define a new register value.

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param c defined value for the field
 */
#ifdef USE_PRECOMPUTED_NOT_DRF
#define LW_DRF_DEF(d,r,f,c) \
    (uint32_t)((uint32_t)(d##_##r##_0_##f##_##c) << (d##_##r##_0_##f##_SHIFT))
#else
#define LW_DRF_DEF(d,r,f,c) \
    ((const uint32_t)(d##_##r##_0_##f##_##c) << LW_FIELD_SHIFT(d##_##r##_0_##f##_RANGE))
#endif

/** LW_DRF_NUM - define a new register value.

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param n numeric value for the field
 */
#ifdef USE_PRECOMPUTED_NOT_DRF
#define LW_DRF_NUM(d, r, f, n)                                    \
    ((((uint32_t)(n)) & (((uint32_t)(d##_##r##_0_##f##_FIELD)) >> \
                         ((uint32_t)(d##_##r##_0_##f##_SHIFT))))  \
     << ((uint32_t)(d##_##r##_0_##f##_SHIFT)))
#else
#define LW_DRF_NUM(d,r,f,n) \
    ((((uint32_t)(n)) & LW_FIELD_MASK(d##_##r##_0_##f##_RANGE)) << \
        LW_FIELD_SHIFT(d##_##r##_0_##f##_RANGE))
#endif

/** LW_DRF_VAL - read a field from a register value.

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param v register value
 */
#ifdef USE_PRECOMPUTED_NOT_DRF
#define LW_DRF_VAL(d, r, f, v)                                    \
    ((((uint32_t)(v)) >> ((uint32_t)(d##_##r##_0_##f##_SHIFT))) & \
     (((uint32_t)(d##_##r##_0_##f##_FIELD)) >>                    \
      ((uint32_t)(d##_##r##_0_##f##_SHIFT))))
#else
#define LW_DRF_VAL(d,r,f,v) \
    ((((uint32_t)(v)) >> LW_FIELD_SHIFT(d##_##r##_0_##f##_RANGE)) & \
        LW_FIELD_MASK(d##_##r##_0_##f##_RANGE))
#endif

/** LW_FLD_SET_DRF_NUM - modify a register field.

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param n numeric field value
    @param v register value
 */
#ifdef USE_PRECOMPUTED_NOT_DRF
#define LW_FLD_SET_DRF_NUM(d,r,f,n,v) \
    ((((uint32_t)(v)) & (~(uint32_t)(d##_##r##_0_##f##_FIELD))) | LW_DRF_NUM(d,r,f,n))
#else
#define LW_FLD_SET_DRF_NUM(d,r,f,n,v) \
    ((((uint32_t)(v)) & (uint32_t)~LW_FIELD_SHIFTMASK(d##_##r##_0_##f##_RANGE)) | LW_DRF_NUM(d,r,f,n))
#endif

/** LW_FLD_SET_DRF_DEF - modify a register field.

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param c defined field value
    @param v register value
 */
#ifdef USE_PRECOMPUTED_NOT_DRF
#define LW_FLD_SET_DRF_DEF(d,r,f,c,v) \
    ((((uint32_t)(v)) & (~(uint32_t)(d##_##r##_0_##f##_FIELD))) | \
        LW_DRF_DEF(d,r,f,c))
#else
#define LW_FLD_SET_DRF_DEF(d,r,f,c,v) \
    ((((uint32_t)(v)) & ((uint32_t)~LW_FIELD_SHIFTMASK(d##_##r##_0_##f##_RANGE))) | \
        LW_DRF_DEF(d,r,f,c))
#endif

/** LW_RESETVAL - get the reset value for a register.

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
 */
#define LW_RESETVAL(d,r)    (d##_##r##_0_RESET_VAL)

/** LW_DRF64_NUM - define a new 64-bit register value.

    @ingroup lwrm_drf

    @param d register domain
    @param r register name
    @param f register field
    @param n numeric value for the field
 */
#define LW_DRF64_NUM(d,r,f,n) \
    ((((uint64_t)(n)) & LW_FIELD64_MASK(d##_##r##_0_##f##_RANGE)) << \
        LW_FIELD64_SHIFT(d##_##r##_0_##f##_RANGE))

/** LW_FLD_TEST_DRF_DEF - test a field from a register

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param c defined value for the field
    @param v register value
 */
#define LW_FLD_TEST_DRF_DEF(d,r,f,c,v) \
    (LW_DRF_VAL(d, r, f, ((uint32_t)(v))) == (d##_##r##_0_##f##_##c))

/** LW_FLD_TEST_DRF_NUM - test a field from a register

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param n numeric value for the field
    @param v register value
 */
#define LW_FLD_TEST_DRF_NUM(d,r,f,n,v) \
    (LW_DRF_VAL(d, r, f, ((uint32_t)(v))) == \
     (((uint32_t)(n)) & (((uint32_t)(d##_##r##_0_##f##_FIELD)) >> \
                          ((uint32_t)(d##_##r##_0_##f##_SHIFT)))))

/** LW_DRF_IDX_VAL - read a field from an indexed register value.

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param i register index
    @param v register value
 */
#ifdef USE_PRECOMPUTED_NOT_DRF
#define LW_DRF_IDX_VAL(d,r,f,i,v) \
    ((((uint32_t)(v)) >> (d##_##r##_0_##f##_SHIFT(((uint32_t)i)))) & \
        ((d##_##r##_0_##f##_FIELD(((uint32_t)(i))))>>(d##_##r##_0_##f##_SHIFT(((uint32_t)(i))))))
#else
#define LW_DRF_IDX_VAL(d,r,f,i,v) \
    ((((uint32_t)(v)) >> LW_FIELD_SHIFT(d##_##r##_0_##f##_RANGE(((uint32_t)i)))) & \
        LW_FIELD_MASK(d##_##r##_0_##f##_RANGE(((uint32_t)(i)))))
#endif

/** LW_FLD_IDX_TEST_DRF - test a field from an indexed register value.

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param i register index
    @param c defined value for the field
    @param v register value
 */
#define LW_FLD_IDX_TEST_DRF(d,r,f,i,c,v) \
    ((LW_DRF_IDX_VAL(d, r, f, ((uint32_t)(i)), ((uint32_t)(v))) == (d##_##r##_0_##f##_##c)))

/** LW_REG_FIELD_SHIFTMASK - return register field shifted  mask.

    @ingroup lwrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
 */
#define LW_REG_FIELD_SHIFTMASK(d,r,f) \
    (d##_##r##_0_##f##_FIELD)

#if !defined(CCC_DRF_MK_CONST_MISRA_COMPATIBLE)
/**
 * At the end of every HW header file these are defined as just "drf_constant"
 * We need to override these to our platform and compiler
 *
 * If the headers are already misra compatible, do not do this (default now).
 * [ Using "undef" is in itself a misra violation. ]
 *
 * If HW header files are not misra compatile, do not define
 * CCC_DRF_MK_CONST_MISRA_COMPATIBLE (This can help if the numeric constants
 * are signed values in the included HW header). The CCC redefined macros
 * additionally CAST the values to (const uint32_t) which is acceptable
 * for coverity and these will not generate misra conditions.
 */
#ifdef MK_SHIFT_CONST
  #undef MK_SHIFT_CONST
#endif
#define MK_SHIFT_CONST(drf_constant) ((const uint32_t)(drf_constant))

#ifdef MK_MASK_CONST
  #undef MK_MASK_CONST
#endif
#define MK_MASK_CONST(drf_constant) ((const uint32_t)(drf_constant ## U))

#ifdef MK_ENUM_CONST
  #undef MK_ENUM_CONST
#endif
#define MK_ENUM_CONST(drf_constant) ((const uint32_t)(drf_constant ## U))

#ifdef MK_ADDR_CONST
  #undef MK_ADDR_CONST
#endif
#define MK_ADDR_CONST(drf_constant) ((const uint32_t)(drf_constant ## U))

#ifdef MK_FIELD_CONST
  #undef MK_FIELD_CONST
#endif
#define MK_FIELD_CONST(drf_mask, drf_shift) (MK_MASK_CONST(drf_mask) << MK_SHIFT_CONST(drf_shift))
#endif /* !defined(CCC_DRF_MK_CONST_MISRA_COMPATIBLE) */

#endif /* INCLUDED_LWRM_DRF_H */
