/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#define DRM_BUILDING_DIVIDE_C
#include "bignum.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;


/*
**          This file has five routines:
**
**          DRM_BOOL div21(db, d, &q, &r) --
**                                          Divide DRM_UINT64 db by d,
**                                          where db < d*RADIX (so that
**                                          0 <= quotient < RADIX).
**                                          The quotient is stored in q,
**                                          and the remainder in r.
**
**          DRM_BOOL div21_fast(db, d, &recip, &q, &r)--
**                                          Similar to div21,
**                                          but useful when dividing by
**                                          the same denominator
**                                          repeatedly.  The recip
**                                          argument (of type reciprocal_1_t)
**                                          must have been initialized
**                                          by divide_precondition_1.
**                                          The algorithm uses two
**                                          multiplications
**                                          (top and bottom halves of each)
**                                          and several additions/subtractions.
**
**          DRM_BOOL divide(numer, lnum, denom, lden, &recip,
**                      quot, rem, plrem, struct bigctx_t *f_pBigCtx) --
**                                          Divide numer (of length lnum)
**                                          by denom (of length lden).
**                                          Quotient
**                                          (of length DRM_MAX(lnum - lden + 1 ,0))
**                                          goes to quot and
**                                          remainder (length lden) to rem.
**                                          recip can be a reciprocal_1_t
**                                          struct for the denominator,
**                                          or NULL.
**
**          DRM_BOOL divide_immediate(numer, den, &recip,
**                                quot, lng, prem) --
**                                          Divide numer (of length lng)
**                                          by den (of length 1);
**                                          quotient goes to quot
**                                          and remainder to *prem.
**                                          recip can be the reciprocal_1_t
**                                          struct for the denominator,
**                                          or NULL.
**
**          DRM_BOOL divide_precondition_1(denom, lng, &reciprocal)
**                                          Initialize a reciprocal_1_t
**                                          struct for div21_fast or
**                                          divide or divide_immediate.
**
**          The div21 code is slow but assumes only ANSI C.
**
**
**                  Some architectures (e.g., Intel 386, Power PC,
**          Motorola 68020, 32-bit SPARC version 8)
**          have a hardware instruction to divide a double-length integer
**          by a single-length integer.  On such machines,
**          the translation of div21 into assembly code
**          is straightforward (although we may need to compute
**          the remainder via a multiplication and subtraction).
**          Other architectures (e.g., MIPS, 64-bit SPARC version 9)
**          have an integer division instruction but allow only
**          single-length numerator and denominator.
**          Still other architectures (e.g., Alpha, IA-64)
**          lack an integer divide instruction.
**          On the latter architectures, the div21_fast algorithm is attractive.
**          Even when there is a hardware instruction which functions
**          like div21, the alternative code is attractive if
**          division is much slower than multiplication.
**
**                  The div21_fast algorithm resembles that on p. 68 of
**
**                          Torjborn Granlund and Peter L. Montgomery,
**                          Division by Ilwariant Integers using Multiplication,
**                          pp. 61-72 of Proceedings of SIGPLAN '94 Conference on
**                          Programming Language Design and Implementation (PLDI),
**                          ACM Press, 1994.
**
**                  Let DBOUND = RADIX be a strict upper bound on the divisor.
**          and QBOUND = RADIX be a strict upper bound on the quotient.
**          These should be powers of 2, with QBOUND dividing DBOUND.
**          (The multiple-precision modular reduction
**          analogue of this algorithm uses DBOUND = QBOUND^2.)
**          Suppose we want to divide repeatedly by d, where 0 < d < DBOUND.
**          Start by computing three constants dependent only on d:
**
**                  shiftamt = LOG2(DBOUND) - 1 - FLOOR(LOG2(d))
**                  dnorm = d * 2^shiftamt                                              (Normalized divisor)
**                  multiplier = FLOOR((DBOUND*QBOUND - 1)/dnorm) - QBOUND
**
**          Also define k = QBOUND*DBOUND - (multiplier + QBOUND)*dnorm.
**          These satisfy
**
**                  DBOUND/2 <= dnorm < DBOUND
**                  0 <= multiplier < QBOUND
**                  0 < k <= dnorm
**
**                  Later, given n with 0 <= n <= d*QBOUND - 1,
**          do the following computations:
**
**                  qprod, nshifted hold values in [0, QBOUND*DBOUND - 1]
**                  remest holds values in [1-DBOUND, DBOUND-1]
**                  qest, nshiftedhi hold values in [0, QBOUND - 1]
**
**                  nshifted   = n * 2^shiftamt;
**                  nshiftedhi = FLOOR(nshifted/DBOUND);
**                  nshiftedlo = (nshifted mod DBOUND);
**
**                  adjust = (top bit of nshiftedlo)*FLOOR(multiplier/2);
**                  qprod =   nshifted
**                          + (multiplier*nshiftedhi + adjust)*DBOUND/QBOUND;
**                  qest = FLOOR(qprod/DBOUND);
**                  remest = n - qest*d - d;
**                  if (remest < 0) then
**                      quotient is qest, remainder is remest + d
**                  else
**                      quotient is qest+1, remainder is remest
**                  end if
**
**          We claim that the correct quotient is either qest or qest + 1.
**          We achieve this by proving
**
**  (*)            0 <= nshifted/dnorm - qprod/DBOUND < 1.
**
**          Add this to qest <= qprod/DBOUND < qest + 1
**          and use nshifted/dnorm = n/d to prove
**
**                  qest <= n/d < qest + 2.
**
**          Therefore FLOOR(n/d) is either qest or qest + 1.
**
**                  Inequality (a) will be useful later:
**
**    (a)     0 <= multiplier*nshiftedlo/DBOUND - adjust
**              <  (multiplier + 1)/2
**               = (QBOUND*DBOUND - k - QBOUND*dnorm + dnorm)/(2*dnorm)
**
**          The proof of (a) has two cases:
**
**                  Case 1:  0 <= nshiftedlo < DBOUND/2 and adjust = 0;
**
**                  Case 2:  DBOUND/2 <= nshiftedlo < DBOUND
**                           and adjust = FLOOR(multiplier/2).
**
**          Each case is straightforward.
**
**                  Next check that
**
**                 qprod*QBOUND
**               = nshifted*QBOUND + (multiplier*nshiftedhi + adjust)*DBOUND
**               =   nshifted*QBOUND
**                 + multiplier*(nshifted - nshiftedlo) + adjust*DBOUND
**               =   nshifted * (multiplier + QBOUND)
**                 - (multiplier*nshiftedlo - adjust*DBOUND)
**               =   nshifted * (QBOUND * DBOUND - k)/dnorm
**                 - (multiplier*nshiftedlo - adjust*DBOUND)   .
**
**          Multiply this by dnorm/DBOUND to get
**
**                   QBOUND*(nshifted - qprod*dnorm/DBOUND)
**               =   QBOUND*nshifted
**                 - nshifted*(QBOUND*DBOUND - k)/DBOUND
**                 + dnorm*(multiplier*nshiftedlo - adjust*DBOUND)/DBOUND
**               =   nshifted*k/DBOUND
**                 + dnorm*(multiplier*nshiftedlo/DBOUND - adjust) .
**
**          By (a), the right side above is nonnegative,
**          which proves the left inequality in (*).
**          Using nshifted < QBOUND*dnorm and (a),
**          we can bound the right side above by
**
**          QBOUND*(nshifted - qprod*dnorm/DBOUND)
**        = nshifted*k/DBOUND + dnorm*(multiplier*nshiftedlo/DBOUND - adjust)
**       <  QBOUND*dnorm*k/DBOUND + (QBOUND*DBOUND - k - QBOUND*dnorm + dnorm)/2
**        = k*(QBOUND*dnorm/DBOUND - 1/2) + dnorm/2 + QBOUND*(DBOUND - dnorm)/2
**       <=   dnorm*(QBOUND*dnorm/DBOUND - 1/2)
**          + dnorm/2 + QBOUND*(DBOUND - dnorm)/2
**        = QBOUND*dnorm - QBOUND*(DBOUND - dnorm)*(dnorm/DBOUND - 1/2)
**       <= QBOUND*dnorm.
**
**          (we used QBOUND*dnorm/DBOUND >= QBOUND/2 >= 1/2 above).
**          This proves the right inequality in (*).
*/

/*
**  This routine estimates a quotient digit given
**  the three most significant digits of the unshifted
**  numerator and given the multiplier from
**  divide_precondition_1.
**
**  The quotient returned will be in the interval
**  [0, RADIX - 1] and will be correct or one too low.
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE digit_t DRM_CALL estimated_quotient_1(
    __in             const digit_t         n2,
    __in             const digit_t         n1,
    __in             const digit_t         n0,
    __in_ecount( 1 ) const reciprocal_1_t *recip )
{
    DRM_UINT64 qprod;
    const digit_t nshiftedhi = DOUBLE_SHIFT_LEFT( n2, n1, recip->shiftamt );
    const digit_t nshiftedlo = DOUBLE_SHIFT_LEFT( n1, n0, recip->shiftamt );

    qprod = DRM_UI64Add( DRM_UI64HL( nshiftedhi, nshiftedlo ),
                       DPRODUU( nshiftedhi, recip->multiplier ) );

    if( nshiftedlo & DRM_RADIX_HALF )
    {
        qprod = DRM_UI64Add( qprod, DRM_UI64( recip->multiplier >> 1 ) );
    }
    return DRM_UI64High32( qprod );
}

/*
**  This routine divides a double-length
**  dividend (db = nhigh * RADIX + nlow) by a single-length divisor (d).
**  All arguments are unsigned.
**  We require db < d*RADIX, to ensure the quotient will be <= RADIX - 1.
**  The quotient *quot and the remainder *rem satisfy the usual rules:
**
**          0 <= *rem < d
**          db = d * (*quot) + (*rem)
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL div21(
    __in              const DRM_UINT64   db,         /* Dividend */
    __in              const digit_t      d,          /* Divisor */
    __out_ecount( 1 )       digit_t     *quot,       /* Quotient (output) */
    __out_ecount( 1 )       digit_t     *rem )       /* Remainder (output) */
{
    DRM_BOOL OK = TRUE;
    const digit_t nhigh = DRM_UI64High32( db );
    const digit_t nlow = DRM_UI64Low32( db );

    if( !OK )
    {
    }
    else if( nhigh >= d )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "div21" );
    }
    else
    {
        /* nhigh < d */

        /*
        **  Construct quotient one bit at a time.
        */
        digit_t rhigh = nhigh;
        digit_t rlow = nlow;
        digit_t q = 0;
        digit_t ipow2;

        for( ipow2 = DRM_RADIX_HALF; ipow2 != 0; ipow2 >>= 1 )
        {
            /*
            **    At this point,
            **
            **    nhigh * RADIX + nlow
            **  = q*d + rhigh*(2*ipow2) + rlow*(2*ipow2/RADIX)
            **
            **    0 <= rhigh < d
            **
            **    If 2*rhigh + 2*rlow/RADIX >= d, increase q by ipow2.
            **    Avoid integer overflow during the test.
            */
            digit_t rlow_hibit = rlow >> ( DRM_RADIX_BITS - 1 );

            if( rhigh + rlow_hibit >= d - rhigh )
            {
                q += ipow2;
                rlow_hibit -= d;
            }
            rhigh = 2 * rhigh + rlow_hibit;
            rlow <<= 1;
        }

        /*
        **  Store quotient and remainder.  rlow is zero now.
        */
        *quot = q;
        *rem = rhigh;
    }
    return OK;
}

/*
**  See start of file for explanation of algorithm.
*/
DRM_EXPORTED_INLINE DRM_BOOL DRM_CALL div21_fast(
    __in                const DRM_UINT64         db,
    __in                const digit_t            d,
    __in_ecount( 1 )    const reciprocal_1_t    *recip,
    __out_ecount( 1 )         digit_t           *quot,
    __out_ecount( 1 )         digit_t           *rem )
{
    const digit_t n1 = DRM_UI64High32( db );
    const digit_t n0 = DRM_UI64Low32( db );
    const digit_t qestcomp = DRM_RADIXM1 - estimated_quotient_1( n1, n0, 0, recip ); /* RADIX - 1 - qest */

    const DRM_UINT64 remest = DRM_UI64Add( DRM_UI64HL( n1 - d, n0 ), DPRODUU( qestcomp, d ) );

    /*
    **  remest = (n1 - d)*RADIX + n0 + (RADIX - 1 - qest)*d
    **         = n1*RADIX + n0 - (qest + 1)*d
    **  is the remainder using qest+1 as estimated quotient.
    **  Be careful since qest+1 may equal RADIX.
    **  remest is in [-d, d - 1]
    **
    **  If DRM_UI64High32(remest) = 0, then remest is nonnegative;
    **  set the quotient to qest + 1 = RADIX - qestcomp
    **  and the remainder to DRM_UI64Low32(remest).
    **  Otherwise DRM_UI64High32(remest) = DRM_RADIXM1;
    **  set the quotient to qest = DRM_RADIXM1 - qestcomp
    **  and the remainder to DRM_UI64Low32(remest) + d - RADIX.
    */
    *quot = DRM_UI64High32( remest ) - qestcomp;
    *rem  = DRM_UI64Low32( remest ) + ( d & DRM_UI64High32( remest ) );
    return TRUE;
}

/*
**  Divide numer (of length lnum) by denom (of length lden).
**  Fifth argument can be the reciprocal_1_t struct
**  from divide_precondition_1 if same denominator is used
**  repeatedly, or can be NULL if
**  this denominator is new.
**
**  Quotient (of length DRM_MAX(lnum - lden + 1, 0)) is put in quot.
**  However, if quot == NULL, no quotient is returned.
**
**  Remainder (of length lden) is put in rem.
**  There is no option to suppress the remainder.
**
**  Function value is the number of significant digits
**  in the remainder.  The function value is zero precisely
**  when the remainder is zero.
**
**  Quotient and remainder should not overlap other arguments.
**  The leading digit of the denominator should be nonzero.
*/
GCC_ATTRIB_NO_STACK_PROTECT()
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL divide(
    __in_ecount( lnum )                                                         const digit_t         *numer,
    __in                                                                        const DRM_DWORD        lnum,
    __in_ecount( lden )                                                         const digit_t         *denom,
    __in                                                                        const DRM_DWORD        lden,
    __in_ecount_opt( 1 )                                                        const reciprocal_1_t  *supplied_reciprocal,     /* reciprocal_t struct for this denominator, or NULL if not previously precomputed */
    __out_ecount_opt( DRM_MAX( ( (DRM_LONG)lnum - (DRM_LONG)lden ) + 1, 0 ) )         digit_t         *quot,                    /* Quotient -- length DRM_MAX(lnum - lden + 1, 0) */
    __out_ecount( lden )                                                              digit_t         *rem )                    /* Remainder -- length lden  */
{
    DRM_BOOL OK = TRUE;
    digit_t dlead;

    if( lden == 0 )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_DIVIDE_ZERO, "divide" );
    }
    else if( numer == NULL || denom == NULL || rem == NULL )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_NULL_POINTER, "divide" );
    }
    else if( numer == quot || numer == rem || denom == quot || denom == rem )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_OVERLAPPING_ARGS, "divide" );
    }
    if( OK )
    {
        dlead = denom[ lden - 1 ];
        if( dlead == 0 )
        {
            OK = FALSE;
            SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "divide -- leading zero" );
        }
        else if( lnum < lden )
        {
            mp_extend( numer, lnum, rem, lden );  /* Quotient length zero */
        }
        else if( lden == 1 )
        {
            OK = OK && divide_immediate( numer, dlead, supplied_reciprocal, quot, lnum, &rem[ 0 ] );
        }
        else
        {
            DRM_DWORD iq, i;
            reciprocal_1_t computed_reciprocal;
            const reciprocal_1_t *used_reciprocal;

            used_reciprocal = supplied_reciprocal;
            if( used_reciprocal == NULL )
            {
                if( !divide_precondition_1( denom, lden, &computed_reciprocal ) )
                {
                    return FALSE;
                }
                used_reciprocal = &computed_reciprocal;
            }

            /*
            ** Copy top lden-1 words of numerator to remainder.
            ** Zero most significant word of remainder.
            */
            rem[ lden - 1 ] = 0;
            OEM_SELWRE_DIGITTCPY( rem, &numer[ lnum - lden + 1 ], lden - 1 );

            for( iq = lnum - lden + 1; OK && ( iq-- ) != 0; /* no-op */ )
            {
                const digit_t remtop = rem[ lden - 1 ];
                digit_t qest;
                /*
                ** Multiply old remainder by RADIX.  Add numer[iq].
                */
                for( i = lden - 1; i != 0; i-- )
                {
                    rem[ i ] = rem[ i - 1 ];
                }
                rem[ 0 ] = numer[ iq ];

                /*
                ** Check for zero digit in quotient.
                ** This is especially likely to happen on the
                ** first iteration of the iq loop.
                */
                if( remtop == 0 && compare_same( rem, denom, lden ) < 0 )
                {
                    qest = 0;
                }
                else
                {
                    digit_t borrow;
                    qest = estimated_quotient_1( remtop, rem[ lden - 1 ], rem[ lden - 2 ], used_reciprocal );
                    /* qest is correct or one too low */
                    qest += ( qest < DRM_RADIXM1 );
                    /* Now qest is correct or one too high */
                    borrow = delwmulate( denom, qest, rem, lden );
                    /* Subtract qest*denom from rem */
                    if( borrow > remtop )
                    {
                        /* If estimated quotient is too high */
                        qest--;
                        borrow -= add_same( rem, denom, rem, lden );
                    }

                    if( borrow != remtop )
                    {
                        OK = FALSE;
                        SetMpErrno_clue( MP_ERRNO_INTERNAL_ERROR, "divide - Quotient estimation error.\n" );
                    }
                }

                if( quot != NULL )
                {
                    quot[ iq ] = qest;
                }
            }
        }
    }
    return OK;
}

/*
**  Divide numer (length lng) by den (length 1).
**  Quotient (length lng) is written to quot
**  (or can be suppressed if quot = NULL).
**  Remainder is returned in *prem.
**
**  supplied_reciprocal is the output of
**  divide_precondition_1 for this denominator,
**  or can be NULL if reciprocal was not
**  previously computed.
*/
GCC_ATTRIB_NO_STACK_PROTECT()
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL divide_immediate(
    __in_ecount( lng )       const digit_t         *numer,
    __in                     const digit_t          den,
    __in_ecount_opt( 1 )     const reciprocal_1_t  *supplied_reciprocal,
    __out_ecount_opt( lng )        digit_t         *quot,
    __in                     const DRM_DWORD        lng,
    __out_ecount( 1 )              digit_t         *prem )
{
    DRM_BOOL OK = TRUE;
    digit_t carry = 0;
    DRM_DWORD i;
    DRM_DWORD lngleft = lng;
    *prem = 0;
    if( quot != NULL )
    {
        *quot = 0;
    }

    if( lngleft > 0 && numer[ lngleft - 1 ] < den )
    {
        lngleft--;
        carry = numer[ lngleft ];
        if( quot != NULL )
        {
            quot[ lngleft ] = 0;
        }
    }

    if( supplied_reciprocal == NULL && lngleft < 2 )
    {
        for( i = lngleft; ( i-- ) != 0; /* no-op */ )
        {
            digit_t qest = 0;
            OK = OK && div21( DRM_UI64HL( carry, numer[ i ] ), den, &qest, &carry );
            if( quot != NULL )
            {
                quot[ i ] = qest;
            }
        }
    }
    else
    {
        reciprocal_1_t computed_reciprocal;
        const reciprocal_1_t *used_reciprocal = supplied_reciprocal;

        if( used_reciprocal == NULL )
        {
            OK = OK && divide_precondition_1( &den, 1, &computed_reciprocal );
            used_reciprocal = &computed_reciprocal;
        }

        for( i = lngleft; OK && ( i-- ) != 0; /* no-op */ )
        {
            digit_t qest = 0;
            OK = OK && div21_fast( DRM_UI64HL( carry, numer[ i ] ), den, used_reciprocal, &qest, &carry );
            if( quot != NULL )
            {
                quot[ i ] = qest;
            }
        }
    }
    if( OK )
    {
        *prem = carry;
    }
    return OK;
}

/*
**  This routine computes the reciprocal_1_t structure
**  for the denominator denom, of length lden.
**  The leading digit denom[lden-1] must be nonzero.
**  It computes a multiplier accurate enough for estimated_quotient_1
**  to predict one digit of a quotient (with an error at most 1).
**  This is the case QBOUND = RADIX, DBOUND = RADIX^lden
**  of the theory atop this file.
*/
GCC_ATTRIB_NO_STACK_PROTECT()
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL divide_precondition_1(
    __in_ecount( lden ) const digit_t         *denom,
    __in                const DRM_DWORD        lden,
    __inout_ecount( 1 )       reciprocal_1_t  *recip )
{
    DRM_BOOL OK = TRUE;

    if( denom == NULL || recip == NULL )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_NULL_POINTER, "divide_precondition_1" );
    }
    else if( lden == 0 || denom[ lden - 1 ] == 0 )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "divide_precondition_1" );
    }
    else
    {
        DRM_DWORD iden, recip_shift;
        digit_t recip_multiplier = 0;
        digit_t dlead[ 3 ];
        digit_t dshiftedhi;
        digit_t dshiftedlo;
        digit_t rem = 0;

        recip_shift = DRM_RADIX_BITS - significant_bit_count( denom[ lden - 1 ] );
        dlead[ 2 ] = denom[ lden - 1 ];
        dlead[ 1 ] = ( lden >= 2 ? denom[ lden - 2 ] : 0 );
        dlead[ 0 ] = ( lden >= 3 ? denom[ lden - 3 ] : 0 );

        dshiftedhi = DOUBLE_SHIFT_LEFT( dlead[ 2 ], dlead[ 1 ], recip_shift );
        dshiftedlo = DOUBLE_SHIFT_LEFT( dlead[ 1 ], dlead[ 0 ], recip_shift );

        /*
        **       We want our RADIX + multiplier to be the integer part of
        **
        **                         RADIX^lden - 1
        **       ---------------------------------------------------------
        **       dshiftedhi*RADIX^(lden-2) + dshiftedlo*RADIX^(lden-3) + ...
        **
        **
        **  The leading digit of this quotient is 1*RADIX^1
        **  since dshiftedhi >= RADIX/2.
        **  After subtracting RADIX*denominator from the numerator,
        **  we get the next digit of the quotient by approximating
        **  the denominator by dshiftedhi*RADIX^(lden-1). Standard theory
        **  (see, e.g., Knuth, Seminumerical Algorithms, 1981,
        **  Theorem B, p. 257) says the so-estimated quotient
        **  differs from the real quotient by at most 2,
        **  and the so-estimated quotient is never smaller than the real
        **  quotient.  When we use the two leading digits from the
        **  divisor, the error in the estimate can be at most one.
        **  We allow one correction while looking at
        **  the dshiftedlo term and one more later on.
        */

        OK = OK && div21( DRM_UI64HL( DRM_RADIXM1 - dshiftedhi, DRM_RADIXM1 - dshiftedlo ), dshiftedhi, &recip_multiplier, &rem );

        /*
        ** Quick adjustment.  Check the sign of
        **
        **        RADIX^3 - 1 - (RADIX + multiplier)*(dshiftedhi*RADIX + dshiftedlo)
        **       =    RADIX*(RADIX^2 - 1 - dshiftedhi*RADIX - dshliftedlo)
        **         - multiplier*(dshiftedhi*RADIX + dshiftedlo)
        **         + RADIX - 1
        **      = RADIX*(rem + multiplier*dshiftedhi) + RADIX - 1
        **         - multiplier*(dshiftedhi*RADIX + dshiftedlo)
        **      = rem*RADIX + RADIX - 1 - multiplier*dshiftedlo
        */
        if( OK )
        {
            DRM_UINT64 tempRem = DPRODUU( recip_multiplier, dshiftedlo );

            if( DRM_UI64High32( tempRem ) > rem )
            {
                recip_multiplier--;
            }

            /*
            **  Fine adjustment.  Check the sign of
            **
            **        RADIX^(lden+1)/2^shiftamt - 1
            **      - (RADIX+multiplier)*denom[lden-1:0].
            **
            **  If this is negative, then the multiplier is too large.
            **  If this is nonnegative, then the multiplier is correct.
            */
            rem = ( DRM_RADIXM1 >> recip_shift ) - denom[ lden - 1 ];
            /* RADIX/2^shiftamt - 1 - den[lden-1] */
            /*
            **  Repeatedly replace rem by
            **
            **      rem*RADIX + RADIX - 1 - denom[iden-1] - multiplier*denom[iden]
            **
            **  until it is known whether rem >= multiplier or rem < 0.
            **  Once one of these happens, the sign of rem won't change.
            */
            for( iden = lden; ( iden-- ) != 0 && rem < recip_multiplier; /* no-op */ )
            {
                /* CAUTION -- loop may exit early */

                DRM_UINT64  test1 = DRM_UI64HL( rem, DRM_RADIXM1 - ( iden > 0 ? denom[ iden - 1 ] : 0 ) );
                const DRM_UINT64 test2 = DPRODUU( recip_multiplier, denom[ iden ] );
                if( !DRM_UI64Les( test2, test1 ) )
                {
                    recip_multiplier--;
                    break;
                }
                test1 = DRM_UI64Sub( test1, test2 );
                rem = DRM_UI64Low32( test1 );
                if( DRM_UI64High32( test1 ) != 0 )
                {
                    break;
                }
            }
            recip->shiftamt   = recip_shift;
            recip->multiplier = recip_multiplier;
        }
    }
    return OK;
}
#endif
EXIT_PK_NAMESPACE_CODE;

