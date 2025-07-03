/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
/*

       File bignum.c
       Version 20 February 2005

       This file has extern definitions of functions
       which are ordinarily inlined.  In particular, it exists to
       support compilers which don't support inlining.
       It also has simple routines defined nowhere else.
*/
#define DRM_BUILDING_BIGNUM_C
#include "bignum.h"
#include "drmsal.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#ifdef NONE
/*
        DRM_DWORD mp_significant_digit_count(a, lng) --  Count the number
                                            of significant bits in a.
                                            This is one more than the
                                            truncated base 2 logarithm of a.

        DRM_DWORD significant_bit_count(d) -- Compute the number of
                                          significant bits in d (d != 0).
                                          This is one more than the
                                          truncated base 2 logarithm of d.

        DRM_BOOL two_adic_ilwerse(d, &dilw, &ctx) -- Returns dilw so that
                                           d*dilw == 1 (mod RADIX).
                                           d must be odd.

*/
/****************************************************************************/

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL two_adic_ilwerse( const digit_t d, digit_t *pdilw )
/*
      Find *pdilw so that (*pdilw)*d == 1 (mod RADIX)
*/
{
    DRM_BOOL OK = TRUE;

    if (DRM_IS_EVEN(d)) {
        OK = FALSE;
        SetMpErrno_clue(MP_ERRNO_ILWALID_DATA,
                        "two_adic_ilwerse");
    } else {
        digit_t dilw = (3*d)^2;     /* 5-bit  2-adic ilwerse approximation */
        digit_t err = 1 - d*dilw;
        DRM_DWORD nbits;   /* Bits of accuracy so far */

        DRMASSERT((err & 31) == 0);
        for (nbits = 5; nbits < DRM_RADIX_BITS/2; nbits *= 2) {
            dilw += dilw*err;
            err = err*err;
            DRMASSERT(err == (digit_t)(1 - d*dilw));
        }
        *pdilw = dilw*err + dilw;
    }
    return OK;
} /* two_adic_ilwerse */

/****************************************************************************/


/*
        Here are some simple routines which fit nowhere else.

      carry  = accumulate(a, mult, b, lng)   -- b := b + mult*a, where a, b
                                                           have length lng


borrow = delwmulate(a, mult, b, lng)   -- b := b - mult*a, where a, b
                                                           have length lng
carry  = multiply_immediate(a, mult, b, lng) -- b := mult*a, where a, b
                                                            have length lng

        DRM_BOOL mp_shift(a, amt, b, lng)    -- Compute lower lng words
                                            of b = a*2^amt, where
                                            amt can be arbitrarily large.
                                            Overflow/underflow is lost.
                                            Does left shift if amt >= 0,
                                            right shift if amt < 0.

        digit_t mp_shift_lost(a, amt, b, lng, plost)
                                         -- Compute lower lng words
                                            of b = a*2^amt, where
                                            |amt| <= DRM_RADIX_BITS.
                                            *plost will receive bits
                                            lost due to overflow/underflow.

        DRM_BOOL multiply(a, lnga, b, lngb, c) - Compute c = a * b
                                             (classical algorithm).

        DRM_BOOL set_immediate(a, ivalue, lngs) -- Set  a = ivalue (a scalar)

         multiply_low(a, b, c, lng)    -- c := a * b (mod RADIX^lng)
         (see file multlow.c)
*/
#endif
/***************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API digit_t DRM_CALL accumulate(
                        const digit_t  *a,
                        const digit_t   mult,
    __inout_ecount(lng)       digit_t  *b,
                        const DRM_DWORD lng)
/*
        Compute b = b + mult*a, where a and b have length lng.
        Function value is carry out of leftmost digit.
*/
{
    digit_t carry = 0;
    DRM_DWORD i;

    for (i = 0; i != lng; i++) {
        const DRM_UINT64 dtemp = MULTIPLY_ADD2(mult, a[i], b[i], carry);
        b[i] = DRM_UI64Low32(dtemp);
        carry = DRM_UI64High32(dtemp);
    }
    return carry;
}  /* accumulate */
#endif
/****************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API digit_t DRM_CALL delwmulate(
    __in_ecount(lng)    const digit_t  *a,
    __in                const digit_t   mult,
    __inout_ecount(lng)       digit_t  *b,
    __in                const DRM_DWORD lng )
/*
        Compute b = b - mult*a, where a and b have length lng.
        Function value is borrow out of leftmost digit.
*/
{
    digit_t borrow = 0;
    DRM_DWORD i;

    for (i = 0; i != lng; i++) {
        const DRM_UINT64 dtemp = DRM_UI64Sub(DRM_UI64Sub(DRM_UI64(b[i]),
                                                DRM_UI64(borrow)),
                                     DPRODUU(mult, a[i]));
        b[i] = DRM_UI64Low32(dtemp);
        borrow = 0 - DRM_UI64High32(dtemp);
    }
    return borrow;
}  /* delwmulate */
#endif


/***************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL mp_shift(
    __in_ecount(lng)    const digit_t  *a,
                        const DRM_LONG  ishift,
    __inout_ecount(lng)       digit_t  *b,
                        const DRM_DWORD lng)
/*
**              Compute  b = a << ishift     (if ishift >= 0)
**                   or  b = a >> (-ishift)  (if ishift < 0).
**
**              Both input and output are length lng.
**              Unlike mp_shift, the shift count may
**              exceed DRM_RADIX_BITS bits (either direction).
**              It may even exceed lng*DRM_RADIX_BITS.
**              Bits shifted past either end are lost.
*/
{
    const DRM_LONG itranslate = (ishift >= 0 ?   (DRM_LONG) (DRM_DWORD)ishift/DRM_RADIX_BITS
                                   : - (DRM_LONG)((DRM_DWORD)(-ishift)/DRM_RADIX_BITS));
                   /* Round quotient ishift/DRM_RADIX_BITS towards zero */
    DRM_DWORD i;
    DRM_BOOL OK = TRUE;
    digit_t lost = 0;

    OK = OK && mp_shift_lost(a, ishift - DRM_RADIX_BITS*itranslate, b,
                             lng, &lost);

    if (!OK) {
    } else if (itranslate < 0) {               /* Right shift, multiple words */
        const DRM_DWORD dtranslate = (DRM_DWORD)(-itranslate);
        for (i = 0; i < lng; i++) {
            DRM_DWORD j = i + dtranslate;
            b[i] = 0;

            /* Skip the following assignment if integer overflow oclwrrs. */
            if ( j >= i && j >= dtranslate && j < lng )
            {
                b[i] = b[j];
            }
        }
    } else if (itranslate > 0) {        /* Left shift, multiple words */
        const DRM_DWORD dtranslate = (DRM_DWORD)itranslate;
        for (i = lng; (i--) > 0; /*null*/) {
            b[i] = (i >= dtranslate ? b[i - dtranslate] : 0);
        }
    }
    return OK;
} /* mp_shift */
#endif
/****************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL mp_shift_lost(
                        const digit_t   *a,
                        const DRM_LONG   shift_amt,
    __inout_ecount(lng)       digit_t   *b,
                        const DRM_DWORD  lng,
                              digit_t   *plost)
/*
**      Compute b = a*2^shift_amt.
**      Require -DRM_RADIX_BITS <= shift_amt <= DRM_RADIX_BITS.
**      Function value reflects bits shifted off the
**      right or off the left.
**
**      The arrays a and b should be identical, or not overlap.
*/
{
    DRM_DWORD i;
    DRM_BOOL OK = TRUE;
    digit_t bits_lost = 0;

    if( ( shift_amt >= 0 ? shift_amt : -shift_amt ) > DRM_RADIX_BITS ) {
        OK = FALSE;
        SetMpErrno_clue(MP_ERRNO_ILWALID_DATA,
                        "mp_shift_lost -- bad shift count");
    } else if (lng == 0) {
    } else if (shift_amt > 0) {           /* left shift */
        if (shift_amt == DRM_RADIX_BITS) {
            bits_lost = a[lng-1];
            for (i = lng-1; i != 0; i--) b[i] = a[i-1];
            b[0] = 0;
        } else {
            for (i = 0; i != lng; i++) {
                const digit_t bnew = (a[i] << shift_amt) | bits_lost;

                bits_lost = a[i] >> (DRM_RADIX_BITS - shift_amt);
                b[i] = bnew;
            }
        }
    } else if (shift_amt < 0) {
        if (shift_amt == -DRM_RADIX_BITS) {
            bits_lost = a[0];
            for (i = 1; i != lng; i++) b[i-1] = a[i];
            b[lng-1] = 0;
        } else {

            for (i = lng; (i--) > 0; /*null*/) {
                const digit_t bnew = (a[i] >> (-shift_amt) ) | bits_lost;
                bits_lost = a[i] << (DRM_RADIX_BITS + shift_amt);
                b[i] = bnew;
            }
            bits_lost >>= (DRM_RADIX_BITS + shift_amt); /* Move to bottom of word */
        }
    } else {
        OEM_SELWRE_DIGITTCPY( b,a,lng );
    }
    if (OK) *plost = bits_lost;
    return OK;
} /* mp_shift_lost */
#endif


/****************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API digit_t DRM_CALL multiply_immediate(
                        const digit_t    *a,
                        const digit_t     mult,
    __out_ecount(lng)         digit_t    *b,
                        const DRM_DWORD   lng )
/*
        Compute b = mult*a, where a and b have length lng.
        Function value is carry out of leftmost digit.
*/
{
    digit_t carry = 0;
    DRM_DWORD i;

    for (i = 0; i < lng; i++) {
        const DRM_UINT64 dtemp = MULTIPLY_ADD1(mult, a[i], carry);
        b[i] = DRM_UI64Low32(dtemp);
        carry = DRM_UI64High32(dtemp);
    }
    return carry;
} /* multiply_immediate */
#endif

#ifdef NONE
/****************************************************************************/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL set_immediate(
    __out_ecount(lnga)         digit_t            *a,
    __in                 const digit_t             ivalue,
    __in                 const DRM_DWORD           lnga,
    __inout              struct bigctx_t          *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    if (lnga == 0) {
        if (ivalue != 0) {
            OK = FALSE;
            SetMpErrno_clue(MP_ERRNO_OVERFLOW,
                            "set_immediate");
        }
    } else {
        a[0] = ivalue;
        OEM_SELWRE_ZERO_MEMORY(a + 1,( lnga - 1)*sizeof( digit_t ));
    }
    return OK;
} /* end set_immediate */
/****************************************************************************/


/*
** This is pretty much a no-op function that is needed for prebuilt
** stublibs for testing.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL mp_initialization( struct bigctx_t *f_pBigCtx )
{
    return TRUE;
}
#endif

/***************************************************************************/
#if defined(SEC_COMPILE)
DRM_LONG DRM_CALL compare_immediate
        (const digit_t     *a,
         const digit_t      ivalue,
         const DRM_DWORD    lng)
/*
        Compare a multiple-precision number to a scalar.
*/
{
    return compare_diff(a, lng, &ivalue, 1);
} /* compare_immediate */
#endif
EXIT_PK_NAMESPACE_CODE;
