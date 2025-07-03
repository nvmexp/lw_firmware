/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_MPADDSUBCMP_C
#include "bignum.h"
#include "bigpriv.h"
#include "drmsal.h"
#include <drmlastinclude.h>

#define DRM_DWORD_LEFT_BIT 0x80000000UL

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)
/*
**          Add/subtract/compare routines.
**          These are candidates for assembly code when
**          porting this package to a new architecture.
**
**          Many of these are low-level routines.
**          Many return a carry flag (digit_t) rather than success/failure (DRM_BOOL).
**
**
**  OK = add_diff(a, lnga, b, lngb, c, pcarry)
**                                             -- c := a + b, where lnga >= lngb.
**                                                Return carry in *pcarry).
**
**  DRM_BOOL add_full(a, lng, b, lngb, c, plngc)
**                                  -- c := a + b, return length of c in *pargc,
**                                     without knowing whether a or b is longer.
**
**  carry  = add_immediate(a, iadd, lnga, b)
**                                      -- b := a + iadd, where iadd is a scalar.
**
**  OK = add_mod(a, b, c, modulus, lng);
**                                      -- c := a + b (mod modulus), all length lng
**
**  carry  = add_same(a, b, c, lng)     -- c := a + b, where all have length lng.
**
**  carry  = add_sub_same(a, b, c, d, lng)
**                                   -- d := a + b - c, where all have length lng.
**                                      N.B.  carry may be negative.
**
**  icomp = compare_diff(a, lnga, b, lngb) -- Return sign of a - b, where a and b
**                                            are allowed to have different lengths.
**
**  icomp = compare_same(a, b, lng)        -- Return sign of a - b, where a and b
**                                            both have length lng.
**                                            Declared inline in bignum.h.
**
**  icomp =  compare_sum_diff(a, lnga, b, lngb, c, lngc)
**                      -- Return sign of a + b - c, where lengths are different.
**
**  icomp =  compare_sum_same(a, b, c, lng) -- Return sign of a + b - c,
**                                            where all have length lng.
**
**  OK     = neg_mod(a, b, modulus, lng)   -- b := -a (mod modulus),
**                                                               all length lng
**
**  OK = sub_diff(a, lnga, b, lngb, c, pborrow )
**                                         -- c := a - b, where lnga >= lngb.
**                                            Return borrow in *pborrow.
**
**  borrow = sub_immediate(a, isub, lnga, b)
**                                      -- b := a - isub, where isub is a scalar.
**
**  OK = sub_mod(a, b, c, modulus, lng);
**                                         -- c := a - b (mod modulus),
**                                                        all length lng
**
**  borrow = sub_same(a, b, c, lng)        -- c := a - b, where all have length lng.
**
**  OK = validate_modular_data(data, modulus, lng) --
**                                        Validate 0 <= data < modulus, where
**                                        data and modulus have length lng.
**                                        If not, issue an error and return FALSE.
**
**          Each routine (except multiply_low) returns a carry/borrow flag.
**          Those returning TRYE/FALSE error flags have the struct bigctx_t *f_pBigCtx argument,
**          which an error handler might need.
**
**  Assembly language note:
**
**          If the architecture has fast add/subtract with carry/borrow
**          instructions (e.g., ADC/SBB on Intel x86,
**          ADDE/SUBE on PowerPC, addxcc/subxcc on 32-bit SPARCs),
**          then that is the most natural way to code these routines.
**                  The Alpha architecture has CMPULT and CMPULE
**          instructions to do unsigned compares (returning 0 or 1).
**          It also has a conditional move.  A possible inner loop is
**
**                  sum = a[i] + carry;
**                  c[i] = b[i] + sum;
**                  if (sum != 0) carry = (c[i] < sum);
**
**          for addition (c := a + b) and
**
**                  dif = a[i] - b[i];
**                  c[i] = dif - borrow;
**                  if (dif != 0) borrow = (a[i] < b[i]);
**                                              [ or   a[i] < dif  ]
**
**          for subtraction (c := a - b).  Each loop body has
**          two adds/subtracts, one compare, and one conditional move
**          (plus loads, stores, and loop control).
**
**                  The MIPS 32-bit R2000/R3000 architecture has SLTU
**          for unsigned compares but lacks conditional moves
**          (although later versions of the architecture have
**          conditional moves on zero/nonzero).
**          One can emulate the Alpha code, with explicit branches
**          when sum == 0 or dif == 0 (and with the store into c[i]
**          oclwpying the delay slot).  Or one can use SLTU twice, computing
**
**                  carry = (sum < carry) + (c[i] < sum);
**                  borrow = (dif < borrow) + (a[i] < b[i]);
**
**          (three adds or subtracts, two SLTU).
**
**                  The Pentium IV architecture has ADC and SBB, but they are
**          much slower than a regular add or subtract.  One solution inspects the
**          upper bits of a[i], b[i], a[i] + b[i] + carry (mod 2^32)
**          to identify whether a carry has oclwrred.  That is,
**
**                  ai = a[i];
**                  bi = b[i];
**                  sum = ai + bi + carry;
**                  carry = MAJORITY(ai, bi, ~sum) >> 31;
**
**          The MAJORITY function acts bitwise, returning 1 wherever
**          two (or all three) operands have a 1 bit, zero elsewhere.
**          On the Pentium, it could expand to
**
**                  (ai | bi) ^ ( sum & (ai ^ bi))
**
**          with (ai | bi) expanding as bi | (ai & bi).  Only three registers are
**          needed for ai, bi, carry (with sum oclwpying same register as carry):
**
**                   load ai and bi;
**                   carry += ai;
**                   ai ^= bi;                  i.e.  ai ^ bi
**                   carry += bi;               i.e.  sum = ai + bi + carry
**                   bi |= ai;                  i.e.  bi | (ai ^ bi) = ai | bi
**                   store low part of carry
**                   carry &= ai;               i.e.  sum & (ai ^ bi)
**                   carry ^= bi;               i.e.  (ai | bi) ^ (sum & (ai ^ bi))
**                   carry >>= 31;
**
**          (two adds, four boolean, one shift).  On Pentium IV, the
**          adds and boolean operations take 0.5 cycle each; shift takes
**          4 cycles; ADC takes 8 cycles.
**
**          Similarly, the inner loop of a subtraction a - b can have
**
**                  sum = ai - bi - borrow;
**                  borrow = MAJORITY(~ai, bi, sum) >> 31;
**
**          with MAJORITY expanding as
**
**                   ai ^ ( (ai ^ bi) | (ai ^ sum))
**
**          These methods will work in SIMD (= MMX or SSE) mode,
**          where no carry bit is available.
**          They can be used when there are more than two operands,
**          such as A + B - C.
*/

/*
**  Add c := a + b.
**  Arrays a and c have length lnga and array b has length lngb,
**  where lnga >= lngb.
**
**  *pcarry is set to 1 if there is a carry out of the left of c.
**  Otherwise *pcarry is set to 0.
**  The store is suppressed if pcarry = NULL;
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL add_diff(
    __in_ecount( lnga ) const digit_t   *a,
    __in                const DRM_DWORD  lnga,
    __in_ecount( lngb ) const digit_t   *b,
    __in                const DRM_DWORD  lngb,
    __out_ecount( lnga )      digit_t   *c,
    __out_ecount_opt( 1 )     digit_t   *pcarry )
{
    DRM_BOOL OK = TRUE;

    if( lnga < lngb
     || ( ( lnga | lngb ) & DRM_DWORD_LEFT_BIT ) )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "add_diff length conflict" );
    }
    else
    {
        digit_t carry = add_same( a, b, c, lngb );
        carry = add_immediate( a + lngb, carry, c + lngb, lnga - lngb );
        if( pcarry != NULL )
        {
            *pcarry = carry;
        }
        else if( carry != 0 )
        {
            OK = FALSE;
            SetMpErrno_clue( MP_ERRNO_OVERFLOW, "add_diff overflow" );
        }
    }
    return OK;
}

/*
** Compute c = a + b, where either a or b may be longer.
** Set *plngc to DRM_MAX(lnga, lngb, length of c).
**
** If lnga and lngb are minimal (i.e., no leading zeros in a or b),
** then *plngc will be minimal too.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL add_full(
    __in_ecount( lnga )                         const digit_t   *a,
    __in                                        const DRM_DWORD  lnga,
    __in_ecount( lngb )                         const digit_t   *b,
    __in                                        const DRM_DWORD  lngb,
    __out_ecount( 1 + DRM_MAX( lnga, lngb ) )         digit_t   *c,
    __out_ecount( 1 )                                 DRM_DWORD *plngc )
{
    digit_t carry = 0;
    DRM_DWORD lngc;
    DRM_BOOL OK = TRUE;
    *plngc = 0;

    if( lnga < lngb )
    {
        OK = OK && add_diff( b, lngb, a, lnga, c, &carry );
        lngc = lngb;
    }
    else
    {
        OK = OK && add_diff( a, lnga, b, lngb, c, &carry );
        lngc = lnga;
    }
    if( OK )
    {
        if( carry != 0 )
        {
            c[ lngc++ ] = carry;
        }
        *plngc = lngc;
    }
    return OK;
}

/*
** Compute b = a + iadd, where iadd has length 1.
** Both a and b have length lng.
** Function value is carry out of leftmost digit in b.
*/
DRM_NO_INLINE DRM_API digit_t DRM_CALL add_immediate(
    __in_ecount( lng ) const digit_t   *a,
    __in               const digit_t    iadd,
    __out_ecount( lng )      digit_t   *b,
    __in               const DRM_DWORD  lng )
{
    digit_t carry = iadd;
    DRM_DWORD i;

    for( i = 0; i != lng; i++ )
    {
        const digit_t bi = a[ i ] + carry;
        b[ i ] = bi;

        if( bi >= carry )
        {
            /* No carry propagation */
            if( a != b )
            {
                OEM_SELWRE_DIGITTCPY( b + i + 1, a + i + 1, ( lng - i - 1 ) );
            }
            return 0;
        }
        carry = 1;
    }
    return carry;
}

/*
** c = a + b (mod modulus)  where  0 <= a  &&  b < modulus
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL add_mod(
    __in_ecount( lng )   const digit_t   *a,
    __in_ecount( lng )   const digit_t   *b,
    __inout_ecount( lng )      digit_t   *c,
    __in_ecount( lng )   const digit_t   *modulus,
    __in                 const DRM_DWORD  lng )
{
    DRM_BOOL OK = TRUE;
    sdigit_t carry;

    if( lng == 0 )
    {
        OK = FALSE;
    }
    else
    {
        const digit_t alead = a[ lng - 1 ];
        const digit_t blead = b[ lng - 1 ];
        const digit_t mlead = modulus[ lng - 1 ];
        DRM_LONG itest;     /* Sign of a + b - modulus */

        if( alead >= mlead )
        {
            OK = OK && validate_modular_data( a, modulus, lng );
        }
        if( blead >= mlead )
        {
            OK = OK && validate_modular_data( b, modulus, lng );
        }

        if( OK )
        {
            /*
            ** Try to determine whether a + b >= modulus
            ** by looking only at the leading digits.
            */

            if( blead > mlead - alead )
            {
                /* implies a + b > modulus */
                itest = +1;
            }
            else if( mlead - alead - blead > 1 )
            {
                /* implies a + b < modulus */
                itest = -1;
            }
            else
            {
                /* Compare a + b to modulus */
                itest = compare_sum_same( a, b, modulus, lng );
            }

            if( itest >= 0 )
            {
                /* c = a + b - modulus */
                carry = add_sub_same( a, b, modulus, c, lng );
            }
            else
            {
                /* c = a + b */
                carry = (sdigit_t)add_same( a, b, c, lng );
            }
            if( carry != 0 )
            {
                OK = FALSE;
                SetMpErrno_clue( MP_ERRNO_INTERNAL_ERROR, "add_mod" );
            }
#if BIGNUM_EXPENSIVE_DEBUGGING
            OK = OK && validate_modular_data( c, modulus, lng );
#endif
        }
    }
    return OK;
}

/*
** c := a + b, where all operands have length lng.
**
** Function value is 1 if there is a carry out of the
** left of c.  Otherwise function value is zero.
*/
DRM_NO_INLINE DRM_API_VOID digit_t DRM_CALL add_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __out_ecount( lng )      digit_t   *c,
    __in               const DRM_DWORD  lng )
{
    digit_t carry = 0;
    DRM_DWORD i;

    for( i = 0; i < lng; i++ )
    {
        const digit_t ai = a[ i ];
        const digit_t bi = b[ i ];

        carry += ai + bi;
        c[ i ] = carry;
        carry = ( ai | bi ) ^ ( ( ai ^ bi ) & carry );  /* MAJORITY(ai, bi, ~carry) */
        carry >>= ( DRM_RADIX_BITS - 1 );
    }
    return carry;
}

/*
** d := a + b - c, where all operands have length lng.
**
** Function value (0, 1, or -1) reflects any carry or
** borrow out of the left of d.
*/
DRM_NO_INLINE DRM_API_VOID sdigit_t DRM_CALL add_sub_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __in_ecount( lng ) const digit_t   *c,
    __out_ecount( lng )      digit_t   *d,
    __in               const DRM_DWORD  lng )
{
    digit_t carry1 = 0, carry2 = 0;
    DRM_DWORD i;

    for( i = 0; i < lng; i++ )
    {
        const digit_t ai = a[ i ];
        const digit_t bi = b[ i ];
        const digit_t ci = c[ i ];
        const digit_t sum1 = ai + bi + carry1;
        const digit_t sum2 = sum1 - ci - carry2;

        d[ i ] = sum2;
        carry1 = sum1 ^ ( ( sum1 ^ ai ) | ( sum1 ^ bi ) );      /* MAJORITY(ai, bi, ~sum1) */
        carry2 = sum1 ^ ( ( sum1 ^ ci ) | ( sum1 ^ sum2 ) );    /* MAJORITY(~sum1, ci, sum2) */
        carry1 >>= ( DRM_RADIX_BITS - 1 );
        carry2 >>= ( DRM_RADIX_BITS - 1 );
    }
    return (sdigit_t)carry1 - (sdigit_t)carry2;
}

/*
** Compare two multiple precision numbers a (of length lnga)
** and b (of length lngb).  Return the sign of a - b, namely
**
**      +1  if  a > b
**       0  if  a = b  (after stripping leading zeros)
**      -1  if  a < b .
*/
DRM_NO_INLINE DRM_API DRM_LONG DRM_CALL compare_diff(
    __in_ecount( lnga ) const digit_t   *a,
    __in                const DRM_DWORD  lnga,
    __in_ecount( lngb ) const digit_t   *b,
    __in                const DRM_DWORD  lngb )
{
    DRM_DWORD la = lnga;
    DRM_DWORD lb = lngb;

    while( la > lb )
    {
        if( a[ la - 1 ] != 0 )
        {
            return +1;
        }
        la--;
    }

    while( lb > la )
    {
        if( b[ lb - 1 ] != 0 )
        {
            return -1;
        }
        lb--;
    }
    DRMASSERT( la == lb );

    while( la != 0 )
    {
        if( a[ la - 1 ] != b[ la - 1 ] )
        {
            return ( a[ la - 1 ] > b[ la - 1 ] ? +1 : -1 );
        }
        la--;
    }
    return 0;
}

/*
** Compare two multiple precision numbers a and b each of length lng.
** Function value is the sign of a - b, namely
**
**      +1 if a > b
**      0 if a = b
**      -1 if a < b
*/
DRM_NO_INLINE DRM_API DRM_LONG DRM_CALL compare_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __in               const DRM_DWORD  lng )
{
    DRM_DWORD i;
    for( i = lng; ( i-- ) != 0; /* no-op */ )
    {
        if( a[ i ] != b[ i ] )
        {
            return ( a[ i ] > b[ i ] ? +1 : -1 );
        }
    }
    return 0;
}
#endif
#if NONE
/*
** Compare multiple-precision integers a, b, c, of lengths
** lnga, lngb, lngc, respectively.  Return the sign of a + b - c, namely
**
**      +1  if  a + b > c
**       0  if  a + b = c
**      -1  if  a + b < c
*/
DRM_NO_INLINE DRM_API DRM_LONG DRM_CALL compare_sum_diff(
    __in_ecount( lnga ) const digit_t   *a,
    __in                const DRM_DWORD  lnga,
    __in_ecount( lngb ) const digit_t   *b,
    __in                const DRM_DWORD  lngb,
    __in_ecount( lngc ) const digit_t   *c,
    __in                const DRM_DWORD  lngc )
{
    const DRM_DWORD lmax = DRM_MAX( DRM_MAX( lnga, lngb ), lngc );
    DRM_DWORD i;
    DRM_LONG sum_prev = 0;

    for( i = lmax; ( i-- ) != 0; /* no-op */ )
    {
        const digit_t aval = ( i >= lnga ? 0 : a[ i ] );
        const digit_t bval = ( i >= lngb ? 0 : b[ i ] );
        const digit_t cval = ( i >= lngc ? 0 : c[ i ] );
        digit_t  sum_now = aval + bval;

        DRMASSERT( sum_prev == 0 || sum_prev == -1 );
        sum_prev += ( sum_now < aval ) - ( sum_now < cval );  /* -2, -1, 0, or +1 */
        sum_now -= cval;
        /* Exit unless sum_now = sum_prev = 0 or -1 */
        if( (digit_t)sum_prev != sum_now
         || ( ( sum_prev + 3 ) & 2 ) == 0 )
        {
            /* If -2 or 1 */
            return ( ( sum_prev + 2 ) & 2 ) - 1;  /* 1 if sum_prev > 0, else -1 */
        }
    }
    return sum_prev;     /* 0 or -1 */
}
#endif

#if defined(SEC_COMPILE)
/*
** Compare multiple-precision integers a, b, c, all of length lng.
** Return the sign of a + b - c, namely
**
**      +1  if  a + b > c
**       0  if  a + b = c
**      -1  if  a + b < c
*/
DRM_NO_INLINE DRM_API DRM_LONG DRM_CALL compare_sum_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __in_ecount( lng ) const digit_t   *c,
    __in               const DRM_DWORD  lng )
{
    DRM_DWORD i;
    DRM_LONG sum_prev = 0;

    for( i = lng; ( i-- ) != 0; /* no-op */ )
    {
        const digit_t aval = a[ i ];
        const digit_t bval = b[ i ];
        const digit_t cval = c[ i ];
        digit_t  sum_now = aval + bval;

        DRMASSERT( sum_prev == 0 || sum_prev == -1 );

        sum_prev += ( sum_now < aval ) - ( sum_now < cval );  /* -2, -1, 0, or +1 */
        sum_now -= cval;

        /* Exit unless sum_now = sum_prev = 0 or -1 */
        if( (digit_t)sum_prev != sum_now
         || ( ( sum_prev + 3 ) & 2 ) == 0 )
        {
            /* If -2 or 1 */
            return ( ( sum_prev + 2 ) & 2 ) - 1;  /* 1 if sum_prev > 0, else -1 */
        }
    }
    return sum_prev;     /* 0 or -1 */
}

/*
** b := -a (mod modulus), where 0 <= a < modulus
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL neg_mod(
    __in_ecount( lng ) const digit_t   *a,
    __out_ecount( lng )      digit_t   *b,
    __in_ecount( lng ) const digit_t   *modulus,
    __in               const DRM_DWORD  lng )
{
    DRM_BOOL OK = TRUE;
    DRM_DWORD i;
    digit_t all_zero = 0;

    /* First copy a to b */
    for( i = 0; i < lng; i++ )
    {
        all_zero |= a[ i ];
        b[ i ] = a[ i ];
    }

    /* If not identically zero, subtract from modulo->modulus */
    if( all_zero != 0 )
    {
        digit_t borrow = sub_same( modulus, b, b, lng );
        if( borrow != 0 )
        {
            OK = FALSE;
            SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "neg_mod input" );
        }
    }

#if BIGNUM_EXPENSIVE_DEBUGGING
    OK = OK && validate_modular_data( b, modulus, lng );
#endif
    return OK;
}
#endif

#ifdef NONE
/*
**  Subtract c := a - b.
**  Arrays a and c have length lnga and array b has length lngb,
**  where lnga >= lngb.
**
**  *pborrow is set to 1 if there is a borrow out of the left of a.
**  Otherwise *pborrow is set to 0.
**  This store is suppressed if pborrow = NULL.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL sub_diff(
    __in_ecount( lnga ) const digit_t   *a,
    __in                const DRM_DWORD  lnga,
    __in_ecount( lngb ) const digit_t   *b,
    __in                const DRM_DWORD  lngb,
    __out_ecount( lnga )      digit_t   *c,
    __out_ecount_opt( 1 )     digit_t   *pborrow )
{
    DRM_BOOL OK = TRUE;

    if( lnga < lngb
     || ( ( lnga | lngb ) & DRM_DWORD_LEFT_BIT ) )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "sub_diff -- length conflict" );
    }
    else
    {
        digit_t borrow = sub_same( a, b, c, lngb );
        borrow = sub_immediate( a + lngb, borrow, c + lngb, lnga - lngb );
        if( pborrow != NULL )
        {
            *pborrow = borrow;
        }
        else if( borrow != 0 )
        {
            OK = FALSE;
            SetMpErrno_clue( MP_ERRNO_OVERFLOW, "sub_diff underflow" );
        }
    }
    return OK;
}

/*
**  Compute b = a - isub, where isub has length 1.
**  Both a and b have length lng.
**  Function value is borrow out of leftmost digit in b.
*/
DRM_NO_INLINE DRM_API digit_t DRM_CALL sub_immediate(
    __in_ecount( lng ) const digit_t   *a,
    __in               const digit_t    isub,
    __out_ecount( lng )      digit_t   *b,
    __in               const DRM_DWORD  lng )
{
    digit_t borrow = isub;
    DRM_DWORD i;

    for( i = 0; i != lng; i++ )
    {
        const digit_t ai = a[ i ];

        b[ i ] = ai - borrow;
        if( ai >= borrow )
        {
            /* No carry propagation */
            if( a != b )
            {
                OEM_SELWRE_DIGITTCPY( b + i + 1, a + i + 1, lng - i - 1 );
            }
            return 0;
        }
        borrow = 1;
    }
    return borrow;
}
#endif

#if defined(SEC_COMPILE)
/*
** c := a - b (mod modulus), where 0 <= a && b < modulus
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL sub_mod(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __out_ecount( lng )      digit_t   *c,
    __in_ecount( lng ) const digit_t   *modulus,
    __in               const DRM_DWORD  lng )
{
    DRM_BOOL OK = TRUE;
    sdigit_t carry;

    if( lng == 0 )
    {
        OK = FALSE;
    }
    else
    {
        const digit_t alead = a[ lng - 1 ];
        const digit_t blead = b[ lng - 1 ];
        const digit_t mlead = modulus[ lng - 1 ];
        DRM_LONG itest;

        if( alead == blead )
        {
            itest = compare_same( a, b, lng - 1 );    /* Sign of a - b */
        }
        else
        {
            itest = ( alead < blead ? -1 : +1 );
        }

        if( itest < 0 )
        {
            /* a < b, set c = a - b + modulus */
            if( blead >= mlead && OK )
            {
                OK = validate_modular_data( b, modulus, lng );
            }
            carry = add_sub_same( a, modulus, b, c, lng );
        }
        else
        {
            /* b <= a, set c = a - b */
            if( alead >= mlead && OK )
            {
                OK = validate_modular_data( a, modulus, lng );
            }
            carry = -(sdigit_t)sub_same( a, b, c, lng );     /* c = a - b */
        }
        if( !OK )
        {
        }
        else if( carry != 0 )
        {
            OK = FALSE;
            SetMpErrno_clue( MP_ERRNO_INTERNAL_ERROR, "sub_mod" );
        }
        else
        {
#if BIGNUM_EXPENSIVE_DEBUGGING
            OK = OK && validate_modular_data( c, modulus, lng );
#endif
        }
    }
    return OK;
}

/*
** c := a - b, where all operands have length lng.
**
** Function value is 1 if there is a borrow out of the
** left of a.  Otherwise function value is zero.
*/
DRM_NO_INLINE DRM_API digit_t DRM_CALL sub_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __out_ecount( lng )      digit_t   *c,
    __in               const DRM_DWORD  lng )
{
    digit_t borrow = 0;
    DRM_DWORD i;

    for( i = 0; i < lng; i++ )
    {
        const digit_t ai = a[ i ];
        const digit_t bi = b[ i ];
        const digit_t sum = ai - bi - borrow;

        c[ i ] = sum;
        borrow = ai ^ ( ( ai ^ bi ) | ( ai ^ sum ) );  /* MAJORITY(~ai, bi, sum) */
        borrow >>= ( DRM_RADIX_BITS - 1 );
    }
    return borrow;
}

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL validate_modular_data(
    __in_ecount( lng ) const digit_t   *data,
    __in_ecount( lng ) const digit_t   *modulus,
    __in               const DRM_DWORD  lng )
{
    DRM_BOOL OK = TRUE;
    if( compare_same( data, modulus, lng ) >= 0 )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_MODULAR_TOO_BIG, "validate_modular_data" );

#if BIGNUM_EXPENSIVE_DEBUGGING
        {
            DRM_DWORD j;
            for( j = 0; j != lng; j++ )
            {
                DRM_DBG_TRACE( ( "data[%ld] = %x, modulus[%ld] = %x\n",
                    (DRM_LONG)j, data[ j ], (DRM_LONG)j, modulus[ j ] ) );
            }
        }
#endif
    }
    return OK;
}
#endif

EXIT_PK_NAMESPACE_CODE;

