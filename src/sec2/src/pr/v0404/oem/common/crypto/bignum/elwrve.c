/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#define DRM_BUILDING_ELWRVE_C
#include "bignum.h"
#include "elwrve.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)
/*
** This file (elwrve.c) has definitions for
** elliptic lwrve operations.
**
**
** Over a field GF(q), where q > 3 is prime,
** an elliptic lwrve is all points (x, y) in GF(q) x GF(q)
** satisfying the Weierstrass equation
**
** y^2 = x^3 + ax + b,          (4a^3 + 27 b^2 != 0)
**
** plus a point at infinity.  The points form an abelian group
** under a well-known addition rule.
**
** Over GF(2^m), the lwrve is instead
**
** y^2 + xy = x^3 + ax^2 + b   (b != 0)
**
** Fields a and b of the struct should have length
** elng = fdesc->elng.
** generator should have length 2*elng or be NULL.
** gorder, the order of generator, should be have length
** elng+1 or be NULL.
**
** Field deallocate is the address to pass to free
** (unless it is NULL) when the lwrve becomes inactive.
**
** Points are usually in affine form, with only x and y.
** When b <> 0,  the point at infinity has x = y = 0.
** When b = 0, the point at infinity has x = 0 and y = 1.
** fdesc.elng digit_t words are used for each of x and y,
** with no gap between them.
*/

#ifndef MAX_ECTEMPS
    #error -- MAX_ECTEMPS undefined
#endif




/*
**   An elliptic lwrve over GF(q) is all points (x, y) satisfying
**   y^2 = x^3 + ax + b mod p, plus a point at infinity.
**   The elwrve_t struct has a and b.
**
**   Points on an elliptic lwrve may be represented in affine
**   or projective form.  Affine form has simply x and y.
**   If b = 0, the point at infinity has x = 0 and y = 1.
**   If b <> 0, the point at infinity has x = 0 and y = 0.
**
**   Projective coordinates use a format suggested in
**   the February 6, 1997 IEEE P1363 Working Draft.
**   We represent (x, y) by (X, Y, Z) where x = X/Z^2 and y = Y/Z^2.
**   Then we need Y^2 = X^3 + aX Z^2 + bZ^4.
**   The point at infinity is (lambda^2, lambda^3, 0) where lambda != 0.
**
**
**   Over GF(2^m) we use the lwrves y^2 + x*y = x^3 + a*x^2 + b
**   where b <> 0.  The affine representation of the point at infinity
**   has x = y = 0.  The negative of a point (x, y) is (x, x+y).
*/

/*
** Compute p3 = p1 + p2 on an elliptic lwrve (addsub = +1)
**      or p3 = p1 = p2 (addsub = -1)
** Any or all of p1, p2, p3 can overlap.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_addition(
    __in_ecount( 2 * E->fdesc->elng )                                    const digit_t           *p1,
    __in_ecount( 2 * E->fdesc->elng )                                    const digit_t           *p2,
    __out_ecount( 2 * E->fdesc->elng )                                         digit_t           *p3,
    __in                                                                 const DRM_LONG           addsub,
    __in_ecount( 1 )                                                     const elwrve_t          *E,
    __inout_ecount_opt( 3 * E->fdesc->elng
                        + DRM_MAX( E->fdesc->ndigtemps_ilwert1,
                                   E->fdesc->ndigtemps_mul ) )                 digit_t           *supplied_temps,
    __inout                                                                    struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL simple_case = FALSE;
    DRM_BOOL OK = TRUE;
    const field_desc_t *fdesc = E->fdesc;
    DRM_BOOL fFree = FALSE;
    const DRM_DWORD elng = fdesc->elng;
    const digit_t *x1 = p1;
    const digit_t *y1 = p1 + elng;
    const digit_t *x2 = p2;
    const digit_t *y2 = p2 + elng;
    digit_t *x3 = p3;
    digit_t *y3 = p3 + elng;
    digit_t *t1 = NULL;    /* Each length elng */
    digit_t *t2 = NULL;
    digit_t *t3 = NULL;
    digit_t *ftemps = NULL;

    if( CHARACTERISTIC_2( fdesc ) )
    {
        return FALSE;
    }

    if( supplied_temps == NULL )
    {
        /*
        ** Check for integer underflow/overflow
        */
        if( 2 * elng < elng
         || 3 * elng < elng
         || 3 * elng + E->ndigtemps < E->ndigtemps
         || 3 * elng + E->ndigtemps < 3 * elng
         || 3 * elng + E->ndigtemps * sizeof( digit_t ) < 3 * elng + E->ndigtemps )
        {
            return FALSE;
        }

        Allocate_Temporaries_Multiple( 3 * elng + E->ndigtemps, digit_t, t1, f_pBigCtx );
        if( t1 == NULL )
        {
            return FALSE;
        }
        fFree = TRUE;
        t2 = t1 + elng;
        t3 = t2 + elng;
        ftemps = t3 + elng;
    }
    else
    {
        t1 = supplied_temps;
        t2 = t1 + elng;
        t3 = t2 + elng;
        ftemps = t3 + elng;
    }

    if( ( fdesc->ftype != FIELD_Q_MP )
     || ( addsub != 1 && addsub != -1 ) )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "ecaffine_addition" );
    }

#if MAX_ECTEMPS < 3
#error -- Need more EC temporaries
#endif

    if( ecaffine_is_infinite( p2, E, f_pBigCtx ) )
    {
        OEM_SELWRE_DIGITTCPY( p3, p1, 2 * E->fdesc->elng );
        simple_case = TRUE;
    }
    else if( ecaffine_is_infinite( p1, E, f_pBigCtx ) )
    {
        if( addsub == +1 )
        {
            OEM_SELWRE_DIGITTCPY( p3, p2, 2 * E->fdesc->elng );
        }
        else
        {
            OK = OK && ecaffine_negate( p2, p3, E, f_pBigCtx );
        }
        simple_case = TRUE;
    }
    else
    {
        /* t2 = y coordinate of -addsub * (x2, y2) */
        if( addsub == -1 )
        {
            OEM_SELWRE_DIGITTCPY( t2, y2, fdesc->elng );  /* t2 = y2 */
        }
        else
        {
            OK = OK && Knegate( y2, t2, fdesc, f_pBigCtx );   /* t2 = -y2 */
        }
        /* Plan to do (x1, y1) - (x2, t2) */
    }

    if( simple_case || !OK )
    {
        /* Done */
    }
    else if( Kequal( x1, x2, fdesc, f_pBigCtx ) )
    {
        if( Kequal( y1, t2, fdesc, f_pBigCtx ) )
        {
            /* Subtracting equal points */
            OK = OK && ecaffine_set_infinite( p3, E, f_pBigCtx );
            simple_case = TRUE;
        }
        else
        {
            /* Doubling over GF(q) */
            OK = OK && Kadd( y1, y1, t2, fdesc, f_pBigCtx );            /* t2 = 2*y */
            OK = OK && Kmul( x1, x1, t1, fdesc, ftemps, f_pBigCtx );    /* t1 = x^2 */
            OK = OK && Kadd( t1, E->a, t3, fdesc, f_pBigCtx );          /* t3 = x^2 + a */
            OK = OK && Kadd( t1, t3, t3, fdesc, f_pBigCtx );            /* t3 = 2x^2 + a */
            OK = OK && Kadd( t1, t3, t3, fdesc, f_pBigCtx );            /* t3 = 3x^2 + a */
            OK = OK && Kilwert( t2, t1, fdesc, ftemps, f_pBigCtx );     /* t1 = 1/(2*y) */
            OK = OK && Kmul( t3, t1, t1, fdesc, ftemps, f_pBigCtx );    /* t1 = m = (3x^2 + a)/(2*y) */
        }
    }
    else
    {
        /* Distinct x coordinates */
        OK = OK && ( fdesc->modulo->length <= elng );     /* Buffer overflow check */
        OK = OK && Kadd( y1, t2, t3, fdesc, f_pBigCtx );            /* t3 = y1 + t2 */
        OK = OK && Ksub( x1, x2, t2, fdesc, f_pBigCtx );            /* t2 = x1 - x2 */
        OK = OK && Kilwert( t2, t1, fdesc, ftemps, f_pBigCtx );     /* t1 = 1/t2 */
        OK = OK && Kmul( t3, t1, t1, fdesc, ftemps, f_pBigCtx );    /* t1 = m = t3/t2 */
    }

    if( simple_case || !OK )
    {
        /* Do nothing */
    }
    else
    {
        OK = OK && Kmul( t1, t1, t2, fdesc, ftemps, f_pBigCtx );  /* t2 = m^2 */
        OK = OK && Ksub( t2, x1, t2, fdesc, f_pBigCtx );          /* m^2 - x1 */
        OK = OK && Ksub( t2, x2, t2, fdesc, f_pBigCtx );          /* t2 = x3 = m^2 - x1 - x2; */
        OK = OK && Ksub( x1, t2, t3, fdesc, f_pBigCtx );          /* t3 = x1 - x3 */
        OEM_SELWRE_DIGITTCPY( x3, t2, fdesc->elng );
        OK = OK && Kmul( t1, t3, t2, fdesc, ftemps, f_pBigCtx );  /* t2 = m*(x1 - x3) */
        OK = OK && Ksub( t2, y1, y3, fdesc, f_pBigCtx );          /* y3 = m*(x1 - x3) - y1 */
    }

    if( fFree )
    {
        Free_Temporaries( t1, f_pBigCtx );
    }
    return OK;
}


/*
** Compute psum = p1 + p2 and pdif = p1 - p2
** on an elliptic lwrve.
** Do this in a way which uses only one ilwersion.
** Outputs may overlap inputs.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_addition_subtraction(
    __in_ecount( 2 * E->fdesc->elng )                              const digit_t           *p1,
    __in_ecount( 2 * E->fdesc->elng )                              const digit_t           *p2,
    __out_ecount( 2 * E->fdesc->elng )                                   digit_t           *psum,
    __out_ecount( 2 * E->fdesc->elng )                                   digit_t           *pdif,
    __in_ecount( 1 )                                               const elwrve_t          *E,
    __inout_ecount( 5 * E->fdesc->elng
                    + DRM_MAX( DRM_MAX( E->fdesc->ndigtemps_ilwert1,
                                        E->fdesc->ndigtemps_mul ),
                               E->ndigtemps ) )                          digit_t           *supplied_temps,     /* Refer to comments near usage for how SAL was determined */
    __inout                                                              struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL special_case = FALSE;
    DRM_BOOL OK = TRUE;
    const field_desc_t *fdesc = E->fdesc;
    const DRM_DWORD elng = fdesc->elng;
    const digit_t *x1 = p1;
    const digit_t *y1 = p1 + elng;
    const digit_t *x2 = p2;
    const digit_t *y2 = p2 + elng;

    DRM_DWORD i;
    if( CHARACTERISTIC_2( fdesc ) )
    {
        return FALSE;
    }

    if( fdesc->ftype != FIELD_Q_MP )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "ecaffine_addition_subtraction" );
    }
    else if( supplied_temps == NULL )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_NULL_POINTER, "ecaffine_addition_subtraction" );
    }

    if( OK )
    {
#if !defined(MAX_ECTEMPS) || MAX_ECTEMPS < 5
#error -- Increase MAX_ECTEMPS
#endif

        /* WARNING -- ps must come first for special_case code to work. */
        /* This also requires that ecaffine_addition use only three field temps. */

        /*
        ** SAL annotation math for supplied_temps:
        ** ps: passed to ecaffine_addition                  (requires size 2 * E->fdesc->elng)
        ** pd: passed to ecaffine_addition                  (requires size 2 * E->fdesc->elng)
        ** t0: passed to Kilwert param 2                    (requires size 1 * E->fdesc->elng)
        ** ftemps: passed to Kilwert as param 4             (requires size E->fdesc->ndigtemps_ilwert1)
        **               and Kmul->Kmul_many as param 6     (requires size E->fdesc->ndigtemps_mul)
        ** special_case_temps: [points to pd, and if pd, t0, and ftemps have enough size, it already has size: 3 * E->fdesc->elng + max( E->fdesc->ndigtemps_ilwert1, E->fdesc->ndigtemps_mul ) ]
        **                     passed to ecaffine_addition as param 6  (requires size 3 * E->fdesc->elng + E->ndigtemps)
        **                     The 3 * E->fdesc->elng overlaps with ftemps, so it needs at least E->ndigtemps as opposed to max( ndigtemps_ilwert1, ndigtemps_mul )
        **                     This results in the portion: max( max( ndigtemps_ilwert1, ndigtemps_mul ), ndigtemps_mul )
        */
        digit_t *ps = supplied_temps;
        digit_t *pd = ps + 2 * elng;
        digit_t *t0 = pd + 2 * elng;
        digit_t *ftemps = t0 + elng;
        digit_t *special_case_temps = pd;

        /*
        ** If either input or output is
        ** the point at infinity, use special code.
        */

        if( ecaffine_is_infinite( p1, E, f_pBigCtx )
         || ecaffine_is_infinite( p2, E, f_pBigCtx )
         || Kequal( x1, x2, fdesc, f_pBigCtx ) )
        {

            special_case = TRUE;
            OK = OK && ecaffine_addition( p1, p2, ps, +1, E, special_case_temps, f_pBigCtx );
            OK = OK && ecaffine_addition( p1, p2, pdif, -1, E, special_case_temps, f_pBigCtx );
            OEM_SELWRE_DIGITTCPY( psum, ps, 2 * E->fdesc->elng );
        }

        if( OK && !special_case )
        {
            digit_t *psx = ps, *psy = ps + elng;
            digit_t *pdx = pd, *pdy = pd + elng;

            OK = OK && ( fdesc->modulo->length <= elng );     /* Buffer overflow check */
            OK = OK && Ksub( x1, x2, psx, fdesc, f_pBigCtx );
            OK = OK && Kilwert( psx, t0, fdesc, ftemps, f_pBigCtx );        /* t0 = 1/(x1 - x2) */
            {
                OK = OK && Ksub( y1, y2, psy, fdesc, f_pBigCtx );           /* y1 - y2 */
                OK = OK && Kadd( y1, y2, pdy, fdesc, f_pBigCtx );           /* y1 - (-y2) */
            }
            OK = OK && Kmul( t0, psy, psy, fdesc, ftemps, f_pBigCtx );      /* ms = (y1 - y2)/(x1 - x2) */
            OK = OK && Kmul( t0, pdy, pdy, fdesc, ftemps, f_pBigCtx );      /* md = (y1 + y2)/(x1 - x2)   or   (y1 + y2 + x2)/(x1 - x2) */
            OK = OK && Kmul( psy, psy, psx, fdesc, ftemps, f_pBigCtx );     /* ms^2 */
            OK = OK && Kmul( pdy, pdy, pdx, fdesc, ftemps, f_pBigCtx );     /* md^2 */
            {
                OK = OK && Kadd( x1, x2, t0, fdesc, f_pBigCtx );            /* x1 + x2 */
                OK = OK && Ksub( psx, t0, psx, fdesc, f_pBigCtx );          /* ms^2 - x1 - x2 */
                OK = OK && Ksub( pdx, t0, pdx, fdesc, f_pBigCtx );          /* md^2 - x1 - x2 */
            }

            /*
            ** Now psx has x(P1 + P2) and pdx has x(P1 - P2)
            ** Compute y(P1 + P2) = ms*(x1 - psx) - y1
            **     (subtract another psx in characteristic 2)
            ** Likewise y(P1 - P2) = md*(x1 - pdx) - y1
            */

            OK = OK && Ksub( x1, psx, t0, fdesc, f_pBigCtx );
            OK = OK && Kmul( psy, t0, psy, fdesc, ftemps, f_pBigCtx );
            OK = OK && Ksub( x1, pdx, t0, fdesc, f_pBigCtx );
            OK = OK && Kmul( pdy, t0, pdy, fdesc, ftemps, f_pBigCtx );
            {
                OK = OK && Ksub( psy, y1, psy, fdesc, f_pBigCtx );
                OK = OK && Ksub( pdy, y1, pdy, fdesc, f_pBigCtx );

                for( i = 0; OK && i < elng; i++ )
                {
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_READ_OVERRUN_6385, "Annotations are correct and code guarantees psx, psy, pdx, and pdy are not overrun" )
                    const digit_t xsumi = psx[ i ];
                    const digit_t ysumi = psy[ i ];
                    const digit_t xdifi = pdx[ i ];
                    const digit_t ydifi = pdy[ i ];
PREFAST_POP /* __WARNING_READ_OVERRUN_6385 */

                    psum[ i ]        = xsumi;
                    psum[ i + elng ] = ysumi;
                    pdif[ i ]        = xdifi;
                    pdif[ i + elng ] = ydifi;
                }
            }
        }
    }
    if( !OK )
    {
        DRM_DBG_TRACE( ( "ecaffine_addition_subtraction exiting abnormally\n" ) );
    }
    return OK;
}

/*
** Check for point at infinity,
** If b = 0, check whether x = 0 and y = 1.
** If b <> 0, check whether x = y = 0
**
**    If an error oclwrs, this routine returns FALSE.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_is_infinite(
    __in_ecount( 2 * E->fdesc->elng ) const digit_t           *p1,
    __in_ecount( 1 )                  const elwrve_t          *E,
    __inout_opt                             struct bigctx_t   *f_pBigCtx )
{
    const field_desc_t *fdesc = E->fdesc;
    const DRM_DWORD elng = fdesc->elng;
    DRM_BOOL OK = TRUE;
    DRM_BOOL infinite;

    if( Kiszero( p1, fdesc, f_pBigCtx ) )
    {
        /* If x = 0 */
        if( E->biszero )
        {
            infinite = Kequal( p1 + elng, fdesc->one, fdesc, f_pBigCtx );
        }
        else
        {
            infinite = Kiszero( p1 + elng, fdesc, f_pBigCtx );
        }
    }
    else
    {
        infinite = FALSE;
    }
    return OK && infinite;
}

/*
** Set p2 =  p1 if negate_flag = +1
** Set p2 = -p1 if negate_flag = -1
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_multiply_pm1(
    __in_ecount( 2 * E->fdesc->elng )  const digit_t           *p1,
    __out_ecount( 2 * E->fdesc->elng )       digit_t           *p2,
    __in                               const DRM_LONG           negate_flag,
    __in_ecount( 1 )                   const elwrve_t          *E,
    __inout                                  struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    if( negate_flag == +1 )
    {
        OEM_SELWRE_DIGITTCPY( p2, p1, 2 * E->fdesc->elng );
    }
    else if( negate_flag == -1 )
    {
        OK = OK && ecaffine_negate( p1, p2, E, f_pBigCtx );
    }
    else
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "ecaffine_multiply_pm1" );
    }
    return OK;
}

/*
** Compute p2 = -p1 with elliptic lwrve arithmetic.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_negate(
    __in_ecount( 2 * E->fdesc->elng )  const digit_t           *p1,
    __out_ecount( 2 * E->fdesc->elng )       digit_t           *p2,
    __in_ecount( 1 )                   const elwrve_t          *E,
    __inout                                  struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    const field_desc_t *fdesc = E->fdesc;
    const DRM_DWORD elng = fdesc->elng;

    if( ecaffine_is_infinite( p1, E, f_pBigCtx ) )
    {
        OEM_SELWRE_DIGITTCPY( p2 + elng, p1 + elng, fdesc->elng );    /* y2 = y1 */
    }
    else if( CHARACTERISTIC_2( fdesc ) )
    {
        return FALSE;
    }
    else
    {
        OK = Knegate( p1 + elng, p2 + elng, fdesc, f_pBigCtx );  /* y2 = -y1 */
    }
    OEM_SELWRE_DIGITTCPY( p2, p1, fdesc->elng );                 /* x2 = x1 */
    return OK;
}

/*
** Test whether p1 = (x1, y1) is on the lwrve.
** In GF(q) case, check whether y1^2 = x1 * (x1^2 + a) + b.
** In GF(2^m) case, check whether y1*(x1 + y1) = x1^2*(x1 + a) + b.
**
** When the point is not on the lwrve, we call SetMpErrno_clue,
** using *pdebug_info as a hint
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_on_lwrve(
    __in_ecount( 2 * E->fdesc->elng )  const digit_t           *p1,
    __in_ecount( 1 )                   const elwrve_t          *E,
    __in_z_opt                         const DRM_CHAR          *pdebug_info,
    __inout_ecount_opt( E->ndigtemps )       digit_t           *supplied_temps,
    __inout                                  struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    const field_desc_t *fdesc = E->fdesc;
    const DRM_DWORD elng = fdesc->elng;
    const digit_t *x1 = p1;
    const digit_t *y1 = p1 + elng;
    digit_t *t1 = NULL;
    digit_t *t2 = NULL;
    digit_t *ftemps = NULL;
    digit_tempinfo_t tempinfo;

    if( ecaffine_is_infinite( p1, E, f_pBigCtx ) )
    {
        return TRUE;
    }

    tempinfo.address = supplied_temps;
    tempinfo.need_to_free = FALSE;
    tempinfo.nelmt = E->ndigtemps;
    OK = OK && possible_digit_allocate( &tempinfo, f_pBigCtx );

    if( OK )
    {
        t1 = tempinfo.address;
        t2 = t1 + elng;
        ftemps = t2 + elng;
#if MAX_ECTEMPS < 2
#error -- "Increase MAX_ECTEMPS"
#endif
    }

    OK = OK && Kmul( x1, x1, t1, fdesc, ftemps, f_pBigCtx );      /* x^2 */

    if( CHARACTERISTIC_2( fdesc ) )
    {
        /* Characteristic 2 */
        return FALSE;
    }
    else
    {
        /* Odd characteristic */
        OK = OK && Kadd( t1, E->a, t1, fdesc, f_pBigCtx );        /* t1 = x^2 + a */
        OK = OK && Kmul( x1, t1, t1, fdesc, ftemps, f_pBigCtx );  /* t1 = x*(x^2 + a) */
        OK = OK && Kadd( t1, E->b, t1, fdesc, f_pBigCtx );        /* t1 = x*(x^2 + a) + b */
        OK = OK && Kmul( y1, y1, t2, fdesc, ftemps, f_pBigCtx );  /* t2 = y^2 */
    }
    if( OK && !Kequal( t1, t2, fdesc, f_pBigCtx ) )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_NOT_ON_LWRVE, pdebug_info );
    }
    if( tempinfo.need_to_free )
    {
        Free_Temporaries( tempinfo.address, f_pBigCtx );
    }
    return OK;
}


/*
** Set p1 to identity (point at infinity).
** If b = 0, set x = 0 and y = 1.
** If b <> 0. set x = y = 0.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_set_infinite(
    __out_ecount( 2 * E->fdesc->elng )        digit_t           *p1,
    __in_ecount( 1 )                    const elwrve_t          *E,
    __inout                                   struct bigctx_t   *f_pBigCtx )
{
    const field_desc_t *fdesc = E->fdesc;
    const DRM_DWORD elng = fdesc->elng;
    DRM_BOOL OK = TRUE;

    OK = OK && Kclear_many( p1, 2, fdesc, f_pBigCtx );
    if( E->biszero )
    {
        OEM_SELWRE_DIGITTCPY( p1 + elng, fdesc->one, fdesc->elng );
    }
    return OK;
}
#endif

#ifdef NONE
/*
** Free the temporaries associated with an elliptic lwrve.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ec_free(
    __inout elwrve_t        *E,
    __inout struct bigctx_t *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    if( E->free_field && E->fdesc != NULL )
    {
        OK = OK && Kfree( (field_desc_t*)E->fdesc, f_pBigCtx );

        Free_Temporaries( (field_desc_t*)E->fdesc, f_pBigCtx );
    }
    if( E->deallocate != NULL )
    {
        Free_Temporaries( E->deallocate, f_pBigCtx );
    }
    E->deallocate = NULL;
    E->generator = NULL;
    E->a = NULL;
    E->b = NULL;
    E->gorder = NULL;
    E->fdesc = NULL;
    return OK;
}


/*
** Initialize struct for elliptic lwrve with parameters a and b
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ec_initialize(
    __in_ecount( fdesc->elng )  const digit_t       *a,
    __in_ecount( fdesc->elng )  const digit_t       *b,
    __in_ecount( 1 )            const field_desc_t  *fdesc,
    __out_ecount( 1 )                 elwrve_t      *E,
    __inout                    struct bigctx_t      *f_pBigCtx,
    __inout                    struct bigctx_t      *pbigctxGlobal )
{
    const DRM_DWORD elng = fdesc->elng;
    DRM_BOOL OK = TRUE;
    digit_t *fields = digit_allocate( 5 * elng + 1, pbigctxGlobal );
    E->deallocate = fields;
    E->free_field = FALSE;
    E->ndigtemps = fdesc->ndigtemps_arith + MAX_ECTEMPS * elng;
    E->biszero = Kiszero( b, fdesc, f_pBigCtx );

    if( fields == NULL )
    {
        OK = FALSE;
    }
    else
    {
        E->a = fields;  fields += elng;
        E->b = fields;  fields += elng;
        E->generator = fields;  fields += 2 * elng;
        E->gorder = fields; fields += ( elng + 1 );
    }

    if( CHARACTERISTIC_2( fdesc ) )
    {
        OK = FALSE;
    }
    else
    {
        digit_t *ftemps = NULL;
        OK = OK
            && Kimmediate( 27, E->b, fdesc, f_pBigCtx )
            && Kmul( E->b, b, E->b, fdesc, ftemps, f_pBigCtx )             /* 27*b */
            && Kmul( E->b, b, E->b, fdesc, ftemps, f_pBigCtx )             /* 27*b^2 */
            && Kadd( a, a, E->a, fdesc, f_pBigCtx )                        /* 2*a */
            && Kmul( E->a, E->a, E->a, fdesc, ftemps, f_pBigCtx )          /* 4*a^2 */
            && Kmuladd( a, E->a, E->b, E->a, fdesc, ftemps, f_pBigCtx )    /* 4*a^3 + 27*b^2 */
            && !Kiszero( E->a, fdesc, f_pBigCtx );                         /* Ensure discriminant nonzero */
    }

    if( OK )
    {
        E->fdesc = fdesc;
        OEM_SELWRE_DIGITTCPY( E->a, a, fdesc->elng );
        OEM_SELWRE_DIGITTCPY( E->b, b, fdesc->elng );

        /*
        ** Initialize generator to point at infinity
        ** and order to 1.  Application can change these.
        */

        OK = OK && ecaffine_set_infinite( E->generator, E, f_pBigCtx );
        OK = OK && set_immediate( E->gorder, 1, elng + 1, f_pBigCtx );
    }

    if( !OK )
    {
        (DRM_VOID)ec_free( E, f_pBigCtx );
    }
    return OK;
}
#endif

EXIT_PK_NAMESPACE_CODE;
