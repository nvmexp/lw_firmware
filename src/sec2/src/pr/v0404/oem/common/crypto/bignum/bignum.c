/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*
**  This file has extern definitions of functions
**  which are ordinarily inlined.  In particular, it exists to
**  support compilers which don't support inlining.
**  It also has simple routines defined nowhere else.
*/
#define DRM_BUILDING_BIGNUM_C
#include "bignum.h"
#include "drmsal.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#ifdef NONE
/*
**  Find *pdilw so that (*pdilw)*d == 1 (mod RADIX)
**  d must be odd
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL two_adic_ilwerse(
    __in                const digit_t  d,
    __out_ecount( 1 )         digit_t *pdilw )
{
    DRM_BOOL OK = TRUE;

    if( DRM_IS_EVEN( d ) )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "two_adic_ilwerse" );
    }
    else
    {
        digit_t dilw = ( 3 * d ) ^ 2;     /* 5-bit  2-adic ilwerse approximation */
        digit_t err = 1 - d * dilw;
        DRM_DWORD nbits;   /* Bits of accuracy so far */

        DRMASSERT( ( err & 31 ) == 0 );
        for( nbits = 5; nbits < DRM_RADIX_BITS / 2; nbits *= 2 )
        {
            dilw += dilw * err;
            err = err * err;
            DRMASSERT( err == (digit_t)( 1 - d * dilw ) );
        }
        *pdilw = dilw * err + dilw;
    }
    return OK;
}
#endif

#if defined(SEC_COMPILE)
/*
**  Compute b = b + mult*a, where a and b have length lng.
**  Function value is carry out of leftmost digit.
*/
DRM_NO_INLINE DRM_API digit_t DRM_CALL accumulate(
    __in_ecount( lng )    const digit_t   *a,
    __in                  const digit_t    mult,
    __inout_ecount( lng )       digit_t   *b,
    __in                  const DRM_DWORD  lng )
{
    digit_t carry = 0;
    DRM_DWORD i;

    for( i = 0; i < lng; i++ )
    {
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_READ_OVERRUN_6385, "Annotations are correct and code guarantees a and b are not overrun" )
        const DRM_UINT64 dtemp = MULTIPLY_ADD2( mult, a[ i ], b[ i ], carry );
PREFAST_POP /* __WARNING_READ_OVERRUN_6385 */
        b[ i ] = DRM_UI64Low32( dtemp );
        carry = DRM_UI64High32( dtemp );
    }
    return carry;
}

/*
**  Compute b = b - mult*a, where a and b have length lng.
**  Function value is borrow out of leftmost digit.
*/
DRM_NO_INLINE DRM_API digit_t DRM_CALL delwmulate(
    __in_ecount( lng )    const digit_t  *a,
    __in                  const digit_t   mult,
    __inout_ecount( lng )       digit_t  *b,
    __in                  const DRM_DWORD lng )
{
    digit_t borrow = 0;
    DRM_DWORD i;

    for( i = 0; i < lng; i++ )
    {
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_READ_OVERRUN_6385, "Annotations are correct and code guarantees a and b are not overrun" )
        const DRM_UINT64 dtemp = DRM_UI64Sub( DRM_UI64Sub( DRM_UI64( b[ i ] ), DRM_UI64( borrow ) ), DPRODUU( mult, a[ i ] ) );
PREFAST_POP /* __WARNING_READ_OVERRUN_6385 */
        b[ i ] = DRM_UI64Low32( dtemp );
        borrow = 0 - DRM_UI64High32( dtemp );
    }
    return borrow;
}

/*
**  Compute  b = a << ishift     (if ishift >= 0)
**       or  b = a >> (-ishift)  (if ishift < 0).
**
**  Both input and output are length lng.
**  Unlike mp_shift_lost, the shift count may
**  exceed DRM_RADIX_BITS bits (either direction).
**  It may even exceed lng*DRM_RADIX_BITS.
**  Bits shifted past either end are lost.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL mp_shift(
    __in_ecount( lng )    const digit_t  *a,
    __in                  const DRM_LONG  ishift,
    __inout_ecount( lng )       digit_t  *b,
    __in                  const DRM_DWORD lng )
{
    /* Round quotient ishift/DRM_RADIX_BITS towards zero */
    const DRM_LONG itranslate = ( ishift >= 0 ? (DRM_LONG)(DRM_DWORD)ishift / DRM_RADIX_BITS : -(DRM_LONG)( (DRM_DWORD)( -ishift ) / DRM_RADIX_BITS ) );
    DRM_DWORD i;
    DRM_BOOL OK = TRUE;
    digit_t lost = 0;

    OK = OK && mp_shift_lost( a, ishift - DRM_RADIX_BITS * itranslate, b, lng, &lost );

    if( !OK )
    {
    }
    else if( itranslate < 0 )
    {
        /* Right shift, multiple words */
        const DRM_DWORD dtranslate = (DRM_DWORD)( -itranslate );
        for( i = 0; i < lng; i++ )
        {
            DRM_DWORD j = i + dtranslate;
            b[ i ] = 0;

            /* Skip the following assignment if integer overflow oclwrrs. */
            if( j >= i && j >= dtranslate && j < lng )
            {
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_READ_OVERRUN_6385, "Annotations are correct and code guarantees b is not overrun" )
                b[ i ] = b[ j ];
PREFAST_POP /* __WARNING_READ_OVERRUN_6385 */
            }
        }
    }
    else if( itranslate > 0 )
    {
        /* Left shift, multiple words */
        const DRM_DWORD dtranslate = (DRM_DWORD)itranslate;
        for( i = lng; ( i-- ) > 0; /* no-op */ )
        {
            b[ i ] = ( i >= dtranslate ? b[ i - dtranslate ] : 0 );
        }
    }
    return OK;
}

/*
**  Compute lower lng words of b = a*2^shift_amt.
**  Require -DRM_RADIX_BITS <= shift_amt <= DRM_RADIX_BITS.
**  Function value reflects bits shifted off the
**  right or off the left.
**  *plost will receive bits
**  lost due to overflow/underflow.
**
**  The arrays a and b should be identical, or not overlap.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL mp_shift_lost(
    __in_ecount( lng )  const digit_t   *a,
    __in                const DRM_LONG   shift_amt,
    __out_ecount( lng )       digit_t   *b,
    __in                const DRM_DWORD  lng,
    __out_ecount( 1 )         digit_t   *plost )
{
    DRM_DWORD i;
    DRM_BOOL OK = TRUE;
    digit_t bits_lost = 0;

    if( ( shift_amt >= 0 ? shift_amt : -shift_amt ) > DRM_RADIX_BITS )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "mp_shift_lost -- bad shift count" );
    }
    else if( lng == 0 )
    {
    }
    else if( shift_amt > 0 )
    {
        /* left shift */
        if( shift_amt == DRM_RADIX_BITS )
        {
            bits_lost = a[ lng - 1 ];
            for( i = lng - 1; i != 0; i-- ) b[ i ] = a[ i - 1 ];
            b[ 0 ] = 0;
        }
        else
        {
            for( i = 0; i != lng; i++ )
            {
                const digit_t bnew = ( a[ i ] << shift_amt ) | bits_lost;

                bits_lost = a[ i ] >> ( DRM_RADIX_BITS - shift_amt );
                b[ i ] = bnew;
            }
        }
    }
    else if( shift_amt < 0 )
    {
        if( shift_amt == -DRM_RADIX_BITS )
        {
            bits_lost = a[ 0 ];
            for( i = 1; i != lng; i++ ) b[ i - 1 ] = a[ i ];
            b[ lng - 1 ] = 0;
        }
        else
        {
            for( i = lng; ( i-- ) > 0; /* no-op */ )
            {
                const digit_t bnew = ( a[ i ] >> ( -shift_amt ) ) | bits_lost;
                bits_lost = a[ i ] << ( DRM_RADIX_BITS + shift_amt );
                b[ i ] = bnew;
            }
            bits_lost >>= ( DRM_RADIX_BITS + shift_amt ); /* Move to bottom of word */
        }
    }
    else
    {
        OEM_SELWRE_DIGITTCPY( b, a, lng );
    }
    if( OK )
    {
        *plost = bits_lost;
    }
    return OK;
}

/*
**  Compute b = mult*a, where a and b have length lng.
**  Function value is carry out of leftmost digit.
*/
DRM_NO_INLINE DRM_API digit_t DRM_CALL multiply_immediate(
    __in_ecount( lng )  const digit_t    *a,
    __in                const digit_t     mult,
    __out_ecount( lng )       digit_t    *b,
    __in                const DRM_DWORD   lng )
{
    digit_t carry = 0;
    DRM_DWORD i;

    for( i = 0; i < lng; i++ )
    {
        const DRM_UINT64 dtemp = MULTIPLY_ADD1( mult, a[ i ], carry );
        b[ i ] = DRM_UI64Low32( dtemp );
        carry = DRM_UI64High32( dtemp );
    }
    return carry;
}
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL set_immediate(
    __out_ecount( lnga )       digit_t            *a,
    __in                 const digit_t             ivalue,
    __in                 const DRM_DWORD           lnga,
    __inout                    struct bigctx_t    *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    if( lnga == 0 )
    {
        if( ivalue != 0 )
        {
            OK = FALSE;
            SetMpErrno_clue( MP_ERRNO_OVERFLOW, "set_immediate" );
        }
    }
    else
    {
        a[ 0 ] = ivalue;
        OEM_SELWRE_ZERO_MEMORY( a + 1, ( lnga - 1 ) * sizeof( digit_t ) );
    }
    return OK;
}


/*
** This is pretty much a no-op function that is needed for prebuilt
** stublibs for testing.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL mp_initialization(
    __inout struct bigctx_t    *f_pBigCtx )
{
    return TRUE;
}
#endif

#if defined(SEC_COMPILE)
/*
**  Compare a multiple-precision number to a scalar.
*/
DRM_API DRM_LONG DRM_CALL compare_immediate(
    __in_ecount( lng ) const digit_t     *a,
    __in               const digit_t      ivalue,
    __in               const DRM_DWORD    lng )
{
    return compare_diff( a, lng, &ivalue, 1 );
}
#endif
EXIT_PK_NAMESPACE_CODE;

