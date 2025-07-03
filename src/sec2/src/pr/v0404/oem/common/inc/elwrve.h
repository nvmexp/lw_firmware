/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#ifndef ELWRVE_H
#define ELWRVE_H 1
#include <bignum.h>
#include <bigpriv.h>
#include <field.h>

ENTER_PK_NAMESPACE;

Future_Struct(elwrve_t);

#define DRM_ELWRVE_TABLE_SPACING_MAX 7

typedef struct elwrve_t {
             const field_desc_t     *fdesc;
             digit_t           *a;
             digit_t           *b;
             digit_t        *generator;  /* Generator of cyclic group */
                                            /* (affine form, x and y) */
             digit_t           *gorder;     /* Order of cyclic group */
             digit_t           *deallocate;
             DRM_BOOL               free_field; /* Flag telling ec_free to call */
                                            /* Kfree.  For system use only. */
             DRM_BOOL               biszero;    /* Is b == 0? */
             DRM_DWORD           ndigtemps;  /* Number of digit_t temporaries */
                                            /* needed by EC */
                                            /* addition/subtraction routines: */
                                            /* */
                                            /*     ecaffine_addition */
                                            /*     ecaffine_addition_subtraction */
                                            /*     ecaffine_on_lwrve */
                                            /*     ecaffine_PPQ */
                                            /*     ecaffine_random */
                                            /* */
                                            /* Includes enough for a */
                                            /* multiplication or ilwersion */
                                            /* (or Kmuladd, etc.) */
                                            /* in the base field. */
} elwrve_t;

#define ECPROJ5_TEMPS_COUNT 10

/*
** This number (MAX_ECTEMPS) was 5, but has been increased to 8
** because we have gone from 160bit to 256 bit encryption
** and this assumes that the minimum size of digit_t/digit_t
** types is a 32 bit type
**
** Maximum number of field temporaries needed
** by any EC addition/subtraction/doubling routine,
** including
**
**     ecaffine_addition,
**     ecaffine_addition_subtraction
**     ecaffine_on_lwrve
**     ecaffine_PPQ
**     ecaffine_random
*/
#define MAX_ECTEMPS 8

DRM_API DRM_BOOL DRM_CALL ecaffine_addition(
    __in_ecount( 2 * E->fdesc->elng )                                    const digit_t           *p1,
    __in_ecount( 2 * E->fdesc->elng )                                    const digit_t           *p2,
    __out_ecount( 2 * E->fdesc->elng )                                         digit_t           *p3,
    __in                                                                 const DRM_LONG           addsub,
    __in_ecount( 1 )                                                     const elwrve_t          *E,
    __inout_ecount_opt( 3 * E->fdesc->elng
                        + DRM_MAX( E->fdesc->ndigtemps_ilwert1,
                                   E->fdesc->ndigtemps_mul ) )                 digit_t           *supplied_temps,
    __inout                                                                    struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL ecaffine_addition_subtraction(
    __in_ecount( 2 * E->fdesc->elng )                              const digit_t           *p1,
    __in_ecount( 2 * E->fdesc->elng )                              const digit_t           *p2,
    __out_ecount( 2 * E->fdesc->elng )                                   digit_t           *psum,
    __out_ecount( 2 * E->fdesc->elng )                                   digit_t           *pdif,
    __in_ecount( 1 )                                               const elwrve_t          *E,
    __inout_ecount( 5 * E->fdesc->elng
                  + DRM_MAX( DRM_MAX( E->fdesc->ndigtemps_ilwert1,
                                      E->fdesc->ndigtemps_mul ),
                             E->ndigtemps ) )                            digit_t           *supplied_temps,
    __inout                                                              struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_exponentiation(
    __in_ecount( 2 * E->fdesc->elng )       const digit_t           *P0,
    __in_ecount( lng_exponent )             const digit_t           *exponent,
    __in                                    const DRM_DWORD          lng_exponent,
    __out_ecount( 2 * E->fdesc->elng )            digit_t           *Presult,
    __in_ecount( 1 )                        const elwrve_t          *E,
    __inout                                       struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL ecaffine_is_infinite(
    __in_ecount( 2 * E->fdesc->elng ) const digit_t           *p1,
    __in_ecount( 1 )                  const elwrve_t          *E,
    __inout_opt                             struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL ecaffine_multiply_pm1(
    __in_ecount( 2 * E->fdesc->elng )  const digit_t           *p1,
    __out_ecount( 2 * E->fdesc->elng )       digit_t           *p2,
    __in                               const DRM_LONG           negate_flag,
    __in_ecount( 1 )                   const elwrve_t          *E,
    __inout                                  struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL ecaffine_negate(
    __in_ecount( 2 * E->fdesc->elng )  const digit_t           *p1,
    __out_ecount( 2 * E->fdesc->elng )       digit_t           *p2,
    __in_ecount( 1 )                   const elwrve_t          *E,
    __inout                                  struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL ecaffine_on_lwrve(
    __in_ecount( 2 * E->fdesc->elng )  const digit_t           *p1,
    __in_ecount( 1 )                   const elwrve_t          *E,
    __in_z_opt                         const DRM_CHAR          *pdebug_info,
    __inout_ecount_opt( E->ndigtemps )       digit_t           *supplied_temps,
    __inout                                  struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL ecaffine_PPQ(
    __in_ecount( 2 * E->fdesc->elng )                                                   const digit_t           *P,
    __in_ecount( 2 * E->fdesc->elng )                                                   const digit_t           *Q,
    __out_ecount( 2 * E->fdesc->elng )                                                        digit_t           *PPQ,
    __in                                                                                const DRM_LONG           pm1,     /* +- 1 */
    __in_ecount( 1 )                                                                    const elwrve_t          *E,
    __inout_ecount( DRM_MAX( 3 * E->fdesc->elng + E->ndigtemps,
                             4 * E->fdesc->elng + DRM_MAX( E->fdesc->ndigtemps_ilwert1,
                                                           E->fdesc->ndigtemps_mul ) ) )      digit_t           *supplied_temps,
    __inout                                                                                   struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL ecaffine_set_infinite(
    __out_ecount( 2 * E->fdesc->elng )        digit_t           *p1,
    __in_ecount( 1 )                    const elwrve_t          *E,
    __inout                                   struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL ec_initialize(
    __in_ecount( fdesc->elng )  const digit_t       *a,
    __in_ecount( fdesc->elng )  const digit_t       *b,
    __in_ecount( 1 )            const field_desc_t  *fdesc,
    __out_ecount( 1 )                 elwrve_t      *E,
    __inout                    struct bigctx_t      *f_pBigCtx,
    __inout                    struct bigctx_t      *pbigctxGlobal );

DRM_API DRM_BOOL DRM_CALL ec_free(
    __inout elwrve_t        *E,
    __inout struct bigctx_t *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_exponentiation_tabular(
    __in_ecount( lexpon * DRM_RADIX_BITS )  const digit_t           *Pbase_powers,
    __in                                    const DRM_DWORD          table_spacing,
    __in                                    const DRM_DWORD          table_last,
    __in_ecount( lexpon )                   const digit_t           *exponent,
    __in                                    const DRM_DWORD          lexpon,
    __out_ecount( 2 * elwrve->fdesc->elng )       digit_t           *result,
    __in_ecount( 1 )                        const elwrve_t          *elwrve,
    __inout                                       struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_table_construction(
    __in_ecount( 2 * elwrve->fdesc->elng )                          const digit_t           *p0,
    __in                                                            const DRM_DWORD          table_spacing,
    __in                                                            const DRM_DWORD          table_last,
    __out_ecount( 2 * elwrve->fdesc->elng * ( table_last + 1 ) )          digit_t           *table,
    __in_ecount( 1 )                                                const elwrve_t          *elwrve,
    __inout                                                               struct bigctx_t   *f_pBigCtx );

DRM_BOOL DRM_CALL ecaffine_to_ecproj5(
    __in_ecount( 2 * elwrve->fdesc->elng )  const digit_t           *p1,
    __out_ecount( 5 * elwrve->fdesc->elng )       digit_t           *p2,
    __in_ecount( 1 )                        const elwrve_t          *elwrve,
    __inout                                       digit_t           *p5temps,
    __inout                                       struct bigctx_t   *f_pBigCtx );

DRM_BOOL DRM_CALL ecproj5_add_ecproj5(
    __in_ecount( 5 * elwrve->fdesc->elng )                      const digit_t           *p1,
    __in_ecount( 5 * elwrve->fdesc->elng )                      const digit_t           *p2,
    __out_ecount( 5 * elwrve->fdesc->elng )                           digit_t           *psum,
    __in_ecount( 1 )                                            const elwrve_t          *elwrve,
    __inout_ecount( ECPROJ5_TEMPS_COUNT * elwrve->fdesc->elng
                  + elwrve->fdesc->ndigtemps_mul )                    digit_t           *p5temps,
    __inout                                                           struct bigctx_t   *f_pBigCtx );

DRM_BOOL DRM_CALL ecproj5_to_ecaffine(
    __in_ecount( 5 * elwrve->fdesc->elng )                      const digit_t           *p1,
    __out_ecount( 2 * elwrve->fdesc->elng )                           digit_t           *p2,
    __in_ecount( 1 )                                            const elwrve_t          *elwrve,
    __inout_ecount( ECPROJ5_TEMPS_COUNT * elwrve->fdesc->elng
                  + DRM_MAX( elwrve->fdesc->ndigtemps_ilwert1,
                             elwrve->fdesc->ndigtemps_mul ) )         digit_t           *p5temps,
    __inout                                                           struct bigctx_t   *f_pBigCtx );

EXIT_PK_NAMESPACE;

#endif /* ELWRVE_H */
