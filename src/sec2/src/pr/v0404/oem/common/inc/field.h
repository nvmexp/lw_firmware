/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#ifndef __FIELD_H
#define __FIELD_H 1
#include <bignum.h>

ENTER_PK_NAMESPACE;

/*
**      This file defines field_desc_t, a struct representing a field.
**      The data structures allow
**      GF(2^m) or GF(q) (with multiple precision q).
**      finite-degree extensions.
**
**      The letter `K' is often used in mathematics
**      to denote a field.  We use names like Kadd
**      for field addition, since the name `Fadd'
**      suggests a floating point addition routine.
**
**      A field element is an array of type digit_t.
**      The elng element of the field_sesc_t struct tells its length
**
**  Arithmetic routines:
**
**      Kadd(f1, f2, f3, &fdesc) :: f3 = f1 + f2
**      Kdiv(f1, f2, f3, &fdesc, ftemps) :: f3 = f1 / f2
**                          Array of fdesc->ndigtemps_arith temps supplied
**      Kequal (f1, f2,  &fdesc) :: Is f1 == f2?
**      Kimmediate(scalar, f1, &fdesc) :: f1 = scalar (a signed digit_t)
**      Kilwert(f1, f2,  &fdesc, ftemps) :: f2 = 1/f1
**                          Array of fdesc->ndigtemps_ilwert1 supplied
**      Kiszero(f1,      &fdesc) :: Is f1 == 0?
**      Kmulpower2(f1, ishift, f3, fdesc) :: f3 = f1 * 2^ishift
**      Kmul(f1, f2, f3, &fdesc) :: f3 = f1 * f2
**      Kmul(f1, f2, f3, &fdesc, ftemps) :: f3 = f1*f2,
**                      Array of fdesc->ndigtemps_mul temps supplied
**      Kmuladd(f1, f2, f3, f4, &fdesc, ftemps) :: f4 = f1 * f2 + f3
**                          Array of fdesc->ndigtemps_arith temps supplied
**      Knegate(f1, f2,  &fdesc) :: f2 = -f1
**      Ksqrt(f1, f2,    &fdesc) :: f2 = sqrt(f1) (either root)
**      Ksub(f1, f2, f3, &fdesc) :: f3 = f1 - f2
**
**  Miscellaneous routines:
**
**      Kclear_many(f1, nelmt,    &fdesc)   :: Set nelmt elements to zero.
**      Kfree  (&fdesc)                     :: Free any memory malloc-ed when field was initialized.
**      Kinitalize_normal2(m, T, &fdesc)    :: Initialize for normal basis.
**
**      Kinitialize_prime(&modulus, &fdesc) :: Initialize field with prime modulus.
*/

typedef enum {FIELD_TYPE_ILWALID = 0,
              FIELD_Q_MP,          /* Field mod multiple-precision prime q */
              FIELD_2_NORMAL,      /* Field GF(2^m) with normal basis over GF(2) */
              FIELD_2_POLYNOMIAL}  /* Field GF(2^m) with polynomial basis over GF(2) */
           field_type_t;

#define CHARACTERISTIC_2(fdesc) ((fdesc)->ftype >= FIELD_2_NORMAL)      /* Test for characteristic 2 field. */

typedef digit_t flongest_t[MP_LONGEST];

/* Special GF(2^m) fields defined by sparse polynomials. */

Future_Struct(field_desc_t);

#define Kadd_many(f1, f2, f3, nelmt, fdesc, ctx) \
        (Kprime_adder ((f1), (f2), (f3), (nelmt), (fdesc), ctx))
#define Kclear_many(      f3, nelmt, fdesc, ctx) \
        (Kzeroizer_default  ((f3), (nelmt), (fdesc), ctx))
#define Kequal_many(f1, f2,   nelmt, fdesc, ctx) \
        (Kequaler_default   ((f1), (f2), (nelmt), (fdesc), ctx))
#define Kimmediate_many(sarray, f3, nelmt, fdesc, ctx) \
        (Kprime_immediater((sarray), (f3), (nelmt), (fdesc), ctx))
#define Kiszero_many(f1,      nelmt, fdesc, ctx) \
        (Kiszeroer_default  ((f1), (nelmt), (fdesc), ctx))
#define Kmulpower2_many(f1, ishift, f3, nelmt, fdesc, ctx) \
        (Kprime_mulpower2er ( (f1), (ishift), (f3), (nelmt), (fdesc), ctx))
#define Knegate_many(f1,  f3, nelmt, fdesc, ctx) \
        (Kprime_negater   ((f1), (f3), (nelmt), (fdesc), ctx))
#define Ksqrt_many(f1,    f3, nelmt, fdesc, are_squares, ctx) \
        (Kprime_sqrter((f1), (f3), (nelmt), (fdesc), (are_squares), ctx))
#define Ksub_many(f1, f2, f3, nelmt, fdesc, ctx) \
        (Kprime_subtracter ((f1), (f2), (f3), (nelmt), (fdesc), ctx))


#define Kadd(f1, f2, f3, fdesc, ctx)   \
        Kadd_many(   (f1), (f2), (f3), 1, (fdesc), ctx)
#define Kequal(f1, f2,   fdesc, ctx)   \
        Kequal_many( (f1), (f2),       1, (fdesc), ctx)

#define Kiszero(f1,      fdesc, ctx)   \
        Kiszero_many((f1),             1, (fdesc), ctx)
#define Kmulpower2(f1, ishift, f3, fdesc, ctx)\
        Kmulpower2_many((f1), (ishift), (f3), 1, (fdesc), ctx)
#define Knegate(f1,  f3, fdesc, ctx)   \
        Knegate_many((f1),       (f3), 1, (fdesc), ctx)
#define Ksqrt(f1,    f3, fdesc, is_square, ctx)  \
        Ksqrt_many(  (f1),       (f3), 1, (fdesc), (is_square), ctx)
#define Ksub(f1, f2, f3, fdesc, ctx)   \
        Ksub_many(   (f1), (f2), (f3), 1, (fdesc), ctx)


typedef struct field_desc_t {
          DRM_DWORD     elng;   /* Length of each field element, in digit_t's*/
          DRM_DWORD     degree; /* Extension degree m if GF(2^m)*/
                               /* Also used for extension fields*/
          DRM_DWORD     ndigtemps_arith;  /* Number of digit_t temporaries*/
                                         /* adequate for any of the following:*/

                                         /*   (use subfield->ndigtemps_arith)*/
                                         /* Kdiv*/
                                         /* Kilwert*/
                                         /* Kilwert_many*/
                                         /* Kmul*/
                                         /* Kmuladd*/
          DRM_DWORD     ndigtemps_mul;
                               /* Number of digit_t temporaries*/
                               /* needed for a multiplication.*/
          DRM_DWORD     ndigtemps_ilwert1;
                               /* Number of digit_t temporaries*/
                               /* needed for an ilwersion.*/
          field_type_t ftype;  /* Type of base field*/
          DRM_BOOL         free_modulus;   /* If TRUE, Kfree frees*/
                                       /* the modulo field.*/
                                       /* Can be set by application.*/
          digit_t      *one;   /* Constant 1*/
          digit_t      *deallocate;

/* Next items apply only if ftype = FIELD_Q_MP*/
          const mp_modulus_t *modulo;      /* Information about q*/
          digit_t *ilwerse_adjustment;
                                      /* Multiplier to adjust reciprocal*/
                                      /* for FROM_RIGHT arithmetic*/

} field_desc_t;


DRM_API DRM_BOOL DRM_CALL Kdiv(
    __in_ecount( fdesc->elng )               const digit_t          *f1,
    __in_ecount( fdesc->elng )               const digit_t          *f2,
    __out_ecount( fdesc->elng )                    digit_t          *f3,
    __in_ecount( 1 )                         const field_desc_t     *fdesc,
    __inout_ecount( fdesc->ndigtemps_arith )       digit_t          *supplied_temps,
    __inout                                        struct bigctx_t  *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kfree(
    __inout_ecount( 1 ) field_desc_t    *fdesc,
    __inout             struct bigctx_t *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kimmediate(
    __in                        const sdigit_t           scalar,
    __out_ecount( fdesc->elng )       digit_t           *f3,
    __in_ecount( 1 )            const field_desc_t      *fdesc,
    __inout                           struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kinitialize_prime(
    __in_ecount( 1 )  const mp_modulus_t    *modulo,
    __out_ecount( 1 )       field_desc_t    *fdesc,
    __inout                 struct bigctx_t *pbigctxTemp,
    __inout                 struct bigctx_t *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kilwert(
    __in_ecount( fdesc->elng )                  const digit_t           *f1,
    __out_ecount( fdesc->elng )                       digit_t           *f3,
    __in_ecount( 1 )                            const field_desc_t      *fdesc,
    __inout_ecount( fdesc->ndigtemps_ilwert1 )        digit_t           *supplied_temps,
    __inout                                           struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kilwert_many(
    __in_ecount( nelmt * fdesc->elng )       const digit_t          *f1,
    __out_ecount( nelmt * fdesc->elng )            digit_t          *f3,
    __in                                     const DRM_DWORD         nelmt,
    __in_ecount( 1 )                         const field_desc_t     *fdesc,
    __inout_ecount( fdesc->ndigtemps_arith )       digit_t          *supplied_temps,
    __inout                                        struct bigctx_t  *f_pBigCtx );

#define Kmul(f1, f2, f3, fdesc, temps, ctx) Kmul_many((f1), (f2), (f3), 1, (fdesc), (temps), ctx)

DRM_API DRM_BOOL DRM_CALL Kmul_many(
    __in_ecount( nelmt * fdesc->elng )                          const digit_t           *f1,
    __in_ecount( nelmt * fdesc->elng )                          const digit_t           *f2,
    __out_ecount( nelmt * fdesc->elng )                               digit_t           *f3,
    __in                                                        const DRM_DWORD          nelmt,
    __in_ecount( 1 )                                            const field_desc_t      *fdesc,
    __inout_ecount_opt( fdesc->ndigtemps_mul )                        digit_t           *supplied_temps,
    __inout                                                           struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kmuladd(
    __in_ecount( fdesc->elng )                               const digit_t          *f1,
    __in_ecount( fdesc->elng )                               const digit_t          *f2,
    __in_ecount( fdesc->elng )                               const digit_t          *f3,
    __out_ecount( fdesc->elng )                                    digit_t          *f4,
    __in_ecount( 1 )                                         const field_desc_t     *fdesc,
    __inout_ecount_opt( fdesc->elng + fdesc->ndigtemps_mul )       digit_t          *supplied_temps,
    __inout                                                        struct bigctx_t  *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kprime_sqrter(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __out_ecount( 1 )                         DRM_BOOL          *psquares,
    __inout                                   struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_adder(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f2,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_freer(
    __inout_ecount( 1 ) field_desc_t    *fdesc,
    __inout             struct bigctx_t *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_immediater(
    __in_ecount( nelmt )                const sdigit_t          *scalars,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_ilwerter1(
    __in_ecount( fdesc->elng )  const digit_t           *f1,
    __out_ecount( fdesc->elng )       digit_t           *f3,
    __in_ecount( 1 )            const field_desc_t      *fdesc,
    __in_ecount( 1 )            const digit_tempinfo_t  *tempinfo,
    __inout                           struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_mulpower2er(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __in                                const DRM_LONG           ishift,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_multiplier1(
    __in_ecount( fdesc->modulo->length )                        const digit_t           *f1,
    __in_ecount( fdesc->modulo->length )                        const digit_t           *f2,
    __out_ecount( fdesc->modulo->length )                             digit_t           *f3,
    __in_ecount( 1 )                                            const field_desc_t      *fdesc,
    __inout_ecount_opt( fdesc->modulo->modmul_algorithm_temps )       digit_t           *temps,
    __inout                                                           struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_negater(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_sizer(
    __out_ecount( fdesc->elng + 1 )           digit_t           *size,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_subtracter(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f2,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kequaler_default(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f2,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout_opt                               struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kiszeroer_default(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout_opt                               struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL Kzeroizer_default(
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout_opt                               struct bigctx_t   *f_pBigCtx );

EXIT_PK_NAMESPACE;

#endif /* __FIELD_H */
