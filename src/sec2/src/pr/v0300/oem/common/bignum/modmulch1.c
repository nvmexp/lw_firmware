/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#define DRM_BUILDING_MODMULCH1_C
#include "bignum.h"
#include "bigpriv.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#ifdef NONE
/*
       File modmulch1.c.   Version 1 October, 2002.

            This is the first of several modmulchx files with algorithms for
        modular multiplication, both FROM_LEFT and FROM_RIGHT.
        All procedures have an argument list

            (const digit_t *a, *b,           // Two numbers to multiply
                                        // 0 <= a, b < modulus
             digit_t  *c                // Product.  May overlap a or b.
             const mp_modulus_t *pmodulo,    // Struct with information about modulus
             digit_t  *temps,           // Temporaries (length
                                        //     pmodulo->modmul_algorithm_temps)
             struct bigctx_t *f_pBigCtx)

            FROM_LEFT codes return

                c == (a*b) mod modulus

            FROM_RIGHT codes return the Montgomery product

               c == a*b / RADIX^lng  (mod pmodulo->modulus).

        where lng = pmodulo->length.

            This file contains

                modmul_from_left_default
                modmul_from_right_default

        which work on all architectures and lengths.
*/
/******************************************************************************/
DRM_NO_INLINE DRM_BOOL DRM_CALL modmul_from_left_default
        (const digit_t *a,              /* IN */
         const digit_t *b,              /* IN */
         digit_t  *c,                   /* OUT */
         const mp_modulus_t *pmodulo,   /* IN */
         digit_t  *temps )              /* TEMPORARIES, at least 2*lng */
/*
        This implements ordinary modular multiplication.
    Given inputs a, b with 0 <= a, b < modulus < RADIX^lng,
    and where lng > 0, we form

           c == (a*b) mod modulus.
*/
{
    DRM_BOOL OK = TRUE;
    const DRM_DWORD lng = pmodulo->length;
    digit_t *temp1 = temps;     /* Length 2*lng */

    DRMASSERT (pmodulo->modmul_algorithm_temps == 2*lng);
    OK = OK && multiply(a, lng, b, lng, temp1);     /* Double-length product */
    OK = OK && divide(temp1, 2*lng, pmodulo->modulus, lng,
                    &pmodulo->left_reciprocal_1, NULL, c);
    return OK;
}  /* modmul_from_left_default */
/******************************************************************************/


/*
**  General case for mutiplication of arbirtrary long integers
*/



DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL modmul_from_right_default(
    __in     const digit_t      *a,
    __in     const digit_t      *b,
    __out          digit_t      *c,
    __in     const mp_modulus_t *pmodulo,
    __inout        digit_t      *temps )    /* TEMPORARIES, at least 2*lng */
/*
        This implements Montgomery (FROM_RIGHT) multiplication.
    Let lng = pmodulo->length > 0.
    Given inputs a, b with 0 <= a, b < pmodulo->modulus < RADIX^lng, we form

           c == a*b / RADIX^lng  (mod pmodulo->modulus).

        At the start of the loop on i, there exists templow
    (formed by the discarded values of
    DRM_UI64Low32(prod1) = DRM_UI64Low32(prod2)) such that

               0 <= temp1, temp2 < modulus
               temp1*RADIX^j + templow = b[0:j-1] * a
               temp2*RADIX^j + templow == 0 (mod modulus)

    When j = lng, we exit with c == temp1 - temp2 (mod modulus)
*/
{
    DRM_BOOL OK = TRUE;
    const DRM_DWORD lng = pmodulo->length;
    digit_t *temp1 = temps, *temp2 = temps + lng;   /* Both length lng */
    const digit_t *modulus = pmodulo->modulus;
    const digit_t milw = pmodulo->right_reciprocal_1;
    const digit_t milwa0 = milw*a[0];    /* mod RADIX */
    DRM_DWORD i, j;
    digit_t carry1, carry2, mul1, mul2;
    DRM_UINT64 carryfull;
    DRM_UINT64 prod1, prod2;

    DRMASSERT (pmodulo->modmul_algorithm_temps == 2*lng);
    /* Case j = 0 of main loop, with temp1 = temp2 = 0 beforehand. */
    mul1 = b[0];
    mul2 = milwa0*mul1;   /* Modulo RADIX; pmodulo->right_reciprocal_1*a[0]*b[0] */

    carryfull = DPRODUU(mul1, a[0]);  /* a[0] * b[0], 64 bit */
    carry1 = DRM_UI64High32(carryfull); /* >>32 */

    carryfull = DPRODUU(mul2, modulus[0]);
    carry2 = DRM_UI64High32( carryfull );

    DRMASSERT (mul1*a[0] == mul2*modulus[0]);     /* mod RADIX, since 1/modulus[0] mod RADIX == right_reciprocal, */

    for (i = 1; i != lng; i++) {
        prod1 = MULTIPLY_ADD1(mul1,       a[i], carry1);
        prod2 = MULTIPLY_ADD1(mul2, modulus[i], carry2);
        temp1[i-1] = DRM_UI64Low32(prod1);
        temp2[i-1] = DRM_UI64Low32(prod2);
        carry1 = DRM_UI64High32(prod1);
        carry2 = DRM_UI64High32(prod2);
    }
    temp1[lng-1] = carry1;
    temp2[lng-1] = carry2;

    for (j = 1; j != lng; j++) {
        mul1 = b[j];
        mul2 = milwa0*mul1 + milw*(temp1[0] - temp2[0]);  /* Modulo RADIX */
        prod1 = MULTIPLY_ADD1(mul1, a[0], temp1[0]);
        prod2 = MULTIPLY_ADD1(mul2, modulus[0], temp2[0]);

        /* Replace temp1 by (temp1 + b[j]*a - DRM_UI64Low32(prod1))/RADIX */
        /* Replace temp2 by (temp2 + mul2*modulus - DRM_UI64Low32(prod2))/RADIX */

        DRMASSERT (DRM_UI64Low32(prod1) == DRM_UI64Low32(prod2));

        carry1 = DRM_UI64High32(prod1);
        carry2 = DRM_UI64High32(prod2);

        for (i = 1; i != lng; i++) {
            prod1 = MULTIPLY_ADD2(mul1,       a[i], temp1[i], carry1);
            prod2 = MULTIPLY_ADD2(mul2, modulus[i], temp2[i], carry2);
            temp1[i-1] = DRM_UI64Low32(prod1);
            temp2[i-1] = DRM_UI64Low32(prod2);
            carry1 = DRM_UI64High32(prod1);
            carry2 = DRM_UI64High32(prod2);
        }
        temp1[lng-1] = carry1;
        temp2[lng-1] = carry2;
    }
    OK = OK && sub_mod(temp1, temp2, c, modulus, lng);
    return OK;
} /* modmul_from_right_default; */
#endif

#if defined (SEC_COMPILE)
/**********************************************************************
** modmul_from_right_default_modulo8 when the size of big integer is 256 bits
** Performance improved due to access to local temporary array
** and loop unrolling.
***********************************************************************/

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL modmul_from_right_default_modulo8_P256(
    __in_ecount(LNGQ_MODULO_8)      const digit_t      *a,
    __in_ecount(LNGQ_MODULO_8)      const digit_t      *b,
    __out_ecount(LNGQ_MODULO_8)           digit_t      *c,
    __in                            const mp_modulus_t *pmodulo,
    __inout                               digit_t      *temps )
{
    DRM_BOOL OK = TRUE;
    digit_t temp1[LNGQ_MODULO_8], temp2[LNGQ_MODULO_8];   /* Both length 8 */
    const digit_t milwa0 = -a[0];    /* mod RADIX */
    DRM_UINT64 mul2ffs;
    DRM_DWORD j;
    digit_t mul1, mul2;
    DRM_UINT64 prod1;

    DRMASSERT (pmodulo->modmul_algorithm_temps == 2*LNGQ_MODULO_8);

    /* Case j = 0 of main loop, with temp1 = temp2 = 0 beforehand.*/
    mul1 = b[0];

    prod1 = DPRODUU(mul1, a[0]      );
    prod1 = MULTIPLY_ADD1(mul1,       a[1], DRM_UI64High32(prod1));
    temp1[0] = DRM_UI64Low32(prod1);
    prod1 = MULTIPLY_ADD1(mul1,       a[2], DRM_UI64High32(prod1));
    temp1[1] = DRM_UI64Low32(prod1);
    prod1 = MULTIPLY_ADD1(mul1,       a[3], DRM_UI64High32(prod1));
    temp1[2] = DRM_UI64Low32(prod1);
    prod1 = MULTIPLY_ADD1(mul1,       a[4], DRM_UI64High32(prod1));
    temp1[3] = DRM_UI64Low32(prod1);
    prod1 = MULTIPLY_ADD1(mul1,       a[5], DRM_UI64High32(prod1));
    temp1[4] = DRM_UI64Low32(prod1);
    prod1 = MULTIPLY_ADD1(mul1,       a[6], DRM_UI64High32(prod1));
    temp1[5] = DRM_UI64Low32(prod1);
    prod1 = MULTIPLY_ADD1(mul1,       a[7], DRM_UI64High32(prod1));
    temp1[6] = DRM_UI64Low32(prod1);
    temp1[7] = DRM_UI64High32(prod1);

    mul2 = milwa0*mul1;   /* Modulo RADIX */
    mul2ffs = DPRODUU( mul2, 0xffffffff );

    DRMASSERT (mul1*a[0] == mul2*0xffffffff);     /* mod RADIX */
    DRMASSERT ( pmodulo->length == LNGQ_MODULO_8 );
    DRMASSERT( DRM_UI64High32(mul2ffs) == (DRM_DWORD)(mul2 - 1) || mul2 == 0);

    prod1 = DRM_UI64Add(mul2ffs, DRM_UI64(DRM_UI64High32(mul2ffs)));
    temp2[0] = DRM_UI64Low32(prod1);
    prod1 = DRM_UI64Add(mul2ffs, DRM_UI64( DRM_UI64High32(prod1)) );
    temp2[1] = DRM_UI64Low32(prod1);
    temp2[2] = DRM_UI64High32(prod1);
    temp2[3] = 0;
    temp2[4] = 0;
    temp2[5] = mul2;
    temp2[6] = DRM_UI64Low32(mul2ffs);
    temp2[7] = DRM_UI64High32(mul2ffs);
    for (j = 1; j != LNGQ_MODULO_8; j++) {
        mul1 = b[j];
        mul2 = milwa0*mul1 + (temp2[0]-temp1[0]);  /* Modulo RADIX */
    
        mul2ffs = DPRODUU( mul2, 0xffffffff );
        prod1 = MULTIPLY_ADD1(mul1, a[0], temp1[0]);

        /*
        ** Replace temp1 by (temp1 + b[j]*a - DRM_UI64Low32(prod1))/RADIX
        ** Replace temp2 by (temp2 + mul2*modulus - DRM_UI64Low32(prod1))/RADIX
        */

        prod1 = MULTIPLY_ADD2(mul1,       a[1], temp1[1], DRM_UI64High32(prod1));
        temp1[0] = DRM_UI64Low32(prod1);
        prod1 = MULTIPLY_ADD2(mul1,       a[2], temp1[2], DRM_UI64High32(prod1));
        temp1[1] = DRM_UI64Low32(prod1);
        prod1 = MULTIPLY_ADD2(mul1,       a[3], temp1[3], DRM_UI64High32(prod1));
        temp1[2] = DRM_UI64Low32(prod1);
        prod1 = MULTIPLY_ADD2(mul1,       a[4], temp1[4], DRM_UI64High32(prod1));
        temp1[3] = DRM_UI64Low32(prod1);
        prod1 = MULTIPLY_ADD2(mul1,       a[5], temp1[5], DRM_UI64High32(prod1));
        temp1[4] = DRM_UI64Low32(prod1);
        prod1 = MULTIPLY_ADD2(mul1,       a[6], temp1[6], DRM_UI64High32(prod1));
        temp1[5] = DRM_UI64Low32(prod1);
        prod1 = MULTIPLY_ADD2(mul1,       a[7], temp1[7], DRM_UI64High32(prod1));
        temp1[6] = DRM_UI64Low32(prod1);
        temp1[7] = DRM_UI64High32(prod1);

        prod1 = DRM_UI64Add(mul2ffs, DRM_UI64(temp2[0]));
        prod1 = DRM_UI64Add(DRM_UI64Add(mul2ffs, DRM_UI64(temp2[1])), DRM_UI64(DRM_UI64High32(prod1)));
        temp2[0] = DRM_UI64Low32(prod1);
        prod1 = DRM_UI64Add(DRM_UI64Add(mul2ffs, DRM_UI64(temp2[2])), DRM_UI64(DRM_UI64High32(prod1)));
        temp2[1] = DRM_UI64Low32(prod1);
        prod1 = DRM_UI64Add(DRM_UI64(temp2[3]), DRM_UI64(DRM_UI64High32(prod1)));
        temp2[2] = DRM_UI64Low32(prod1);
        prod1 = DRM_UI64Add(DRM_UI64(temp2[4]), DRM_UI64(DRM_UI64High32(prod1)));
        temp2[3] = DRM_UI64Low32(prod1);
        prod1 = DRM_UI64Add(DRM_UI64(temp2[5]), DRM_UI64(DRM_UI64High32(prod1)));
        temp2[4] = DRM_UI64Low32(prod1);
        prod1 = DRM_UI64Add(DRM_UI64Add(DRM_UI64(mul2), DRM_UI64(temp2[6])), DRM_UI64(DRM_UI64High32(prod1)));
        temp2[5] = DRM_UI64Low32(prod1);
        prod1 = DRM_UI64Add(DRM_UI64Add(mul2ffs, DRM_UI64(temp2[7])), DRM_UI64(DRM_UI64High32(prod1)));
        temp2[6] = DRM_UI64Low32(prod1);
        temp2[7] = DRM_UI64High32(prod1);
    }
    OK = OK && sub_mod(temp1, temp2, c, pmodulo->modulus, 8);
    return OK;
}
#endif
#ifdef NONE
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL modmul_from_right_default_modulo8(
    __in      const digit_t      *a,
    __in      const digit_t      *b,
    __out           digit_t      *c,
    __in      const mp_modulus_t *pmodulo,
    __inout         digit_t      *temps )    
/*
**     This is exactly the same as modmul_from_right_default
**     in the case of pmodulo->length equal to 8
*/
{
    DRM_BOOL OK = TRUE;
    digit_t temp1[LNGQ_MODULO_8], temp2[LNGQ_MODULO_8];   /* Both length 8 */
    const digit_t *modulus = pmodulo->modulus;
    const digit_t milw = pmodulo->right_reciprocal_1;
    const digit_t milwa0 = milw*a[0];    /* mod RADIX */
    DRM_DWORD j;
    DRM_UINT64 carry1full, carry2full;
    digit_t mul1, mul2;
    DRM_UINT64 prod1, prod2;

    DRMASSERT (pmodulo->modmul_algorithm_temps == 2*LNGQ_MODULO_8);

    /* Case j = 0 of main loop, with temp1 = temp2 = 0 beforehand. */
    mul1 = b[0];
    mul2 = milwa0*mul1;   /* Modulo RADIX */

    carry1full = DPRODUU(mul1, a[0]      );
    carry2full = DPRODUU(mul2, modulus[0]);

    DRMASSERT (mul1*a[0] == mul2*modulus[0]);     /* mod RADIX */
    DRMASSERT ( pmodulo->length == LNGQ_MODULO_8 );

    prod1 = MULTIPLY_ADD1(mul1,       a[1], DRM_UI64High32(carry1full));
    prod2 = MULTIPLY_ADD1(mul2, modulus[1], DRM_UI64High32(carry2full));
    temp1[0] = DRM_UI64Low32(prod1);
    temp2[0] = DRM_UI64Low32(prod2);

    prod1 = MULTIPLY_ADD1(mul1,       a[2], DRM_UI64High32(prod1));
    prod2 = MULTIPLY_ADD1(mul2, modulus[2], DRM_UI64High32(prod2));
    temp1[1] = DRM_UI64Low32(prod1);
    temp2[1] = DRM_UI64Low32(prod2);

    prod1 = MULTIPLY_ADD1(mul1,       a[3], DRM_UI64High32(prod1));
    prod2 = MULTIPLY_ADD1(mul2, modulus[3], DRM_UI64High32(prod2));
    temp1[2] = DRM_UI64Low32(prod1);
    temp2[2] = DRM_UI64Low32(prod2);

    prod1 = MULTIPLY_ADD1(mul1,       a[4], DRM_UI64High32(prod1));
    prod2 = MULTIPLY_ADD1(mul2, modulus[4], DRM_UI64High32(prod2));
    temp1[3] = DRM_UI64Low32(prod1);
    temp2[3] = DRM_UI64Low32(prod2);

    prod1 = MULTIPLY_ADD1(mul1,       a[5], DRM_UI64High32(prod1));
    prod2 = MULTIPLY_ADD1(mul2, modulus[5], DRM_UI64High32(prod2));
    temp1[4] = DRM_UI64Low32(prod1);
    temp2[4] = DRM_UI64Low32(prod2);

    prod1 = MULTIPLY_ADD1(mul1,       a[6], DRM_UI64High32(prod1));
    prod2 = MULTIPLY_ADD1(mul2, modulus[6], DRM_UI64High32(prod2));
    temp1[5] = DRM_UI64Low32(prod1);
    temp2[5] = DRM_UI64Low32(prod2);

    prod1 = MULTIPLY_ADD1(mul1,       a[7], DRM_UI64High32(prod1));
    prod2 = MULTIPLY_ADD1(mul2, modulus[7], DRM_UI64High32(prod2));
    temp1[6] = DRM_UI64Low32(prod1);
    temp2[6] = DRM_UI64Low32(prod2);


    temp1[7] = DRM_UI64High32(prod1);
    temp2[7] = DRM_UI64High32(prod2);

    for (j = 1; j != LNGQ_MODULO_8; j++) {
        mul1 = b[j];
        mul2 = milwa0*mul1 + milw*(temp1[0] - temp2[0]);  /* Modulo RADIX */
        prod1 = MULTIPLY_ADD1(mul1, a[0], temp1[0]);
        prod2 = MULTIPLY_ADD1(mul2, modulus[0], temp2[0]);

        /* 
        ** Replace temp1 by (temp1 + b[j]*a - DRM_UI64Low32(prod1))/RADIX
        ** Replace temp2 by (temp2 + mul2*modulus - DRM_UI64Low32(prod2))/RADIX
        */

        DRMASSERT (DRM_UI64Low32(prod1) == DRM_UI64Low32(prod2));

        prod1 = MULTIPLY_ADD2(mul1,       a[1], temp1[1], DRM_UI64High32(prod1));
        prod2 = MULTIPLY_ADD2(mul2, modulus[1], temp2[1], DRM_UI64High32(prod2));
        temp1[0] = DRM_UI64Low32(prod1);
        temp2[0] = DRM_UI64Low32(prod2);

        prod1 = MULTIPLY_ADD2(mul1,       a[2], temp1[2], DRM_UI64High32(prod1));
        prod2 = MULTIPLY_ADD2(mul2, modulus[2], temp2[2], DRM_UI64High32(prod2));
        temp1[1] = DRM_UI64Low32(prod1);
        temp2[1] = DRM_UI64Low32(prod2);

        prod1 = MULTIPLY_ADD2(mul1,       a[3], temp1[3], DRM_UI64High32(prod1));
        prod2 = MULTIPLY_ADD2(mul2, modulus[3], temp2[3], DRM_UI64High32(prod2));
        temp1[2] = DRM_UI64Low32(prod1);
        temp2[2] = DRM_UI64Low32(prod2);

        prod1 = MULTIPLY_ADD2(mul1,       a[4], temp1[4], DRM_UI64High32(prod1));
        prod2 = MULTIPLY_ADD2(mul2, modulus[4], temp2[4], DRM_UI64High32(prod2));
        temp1[3] = DRM_UI64Low32(prod1);
        temp2[3] = DRM_UI64Low32(prod2);


        prod1 = MULTIPLY_ADD2(mul1,       a[5], temp1[5], DRM_UI64High32(prod1));
        prod2 = MULTIPLY_ADD2(mul2, modulus[5], temp2[5], DRM_UI64High32(prod2));
        temp1[4] = DRM_UI64Low32(prod1);
        temp2[4] = DRM_UI64Low32(prod2);

        prod1 = MULTIPLY_ADD2(mul1,       a[6], temp1[6], DRM_UI64High32(prod1));
        prod2 = MULTIPLY_ADD2(mul2, modulus[6], temp2[6], DRM_UI64High32(prod2));
        temp1[5] = DRM_UI64Low32(prod1);
        temp2[5] = DRM_UI64Low32(prod2);

        prod1 = MULTIPLY_ADD2(mul1,       a[7], temp1[7], DRM_UI64High32(prod1));
        prod2 = MULTIPLY_ADD2(mul2, modulus[7], temp2[7], DRM_UI64High32(prod2));
        temp1[6] = DRM_UI64Low32(prod1);
        temp2[6] = DRM_UI64Low32(prod2);


        temp1[7] = DRM_UI64High32(prod1);
        temp2[7] = DRM_UI64High32(prod2);

    }
    OK = OK && sub_mod(temp1, temp2, c, modulus, 8);
    return OK;
} /* modmul_from_right_default_modulo8; */

/******************************************************************************/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL modmul_choices1
        (mp_modulus_t *pmodulo,       /* IN/OUT */
         DRM_LONG         *pindex )   /* IN/OUT */
/*
            This procedure (and any other modmul_choicesX ones it calls)
        looks for a suitable modmul_algorithm.  The selection algorithm
        can look at:

              1)  The target architecture (if there is a known target with assembly available)
              2)  Instruction-availability flags
              3)  Fields in the pmodulo struct, such as the length and the
                  FROM_LEFT/FROM_RIGHT flag.  A Karatsuba routine might
                  accept only moduli beyond a specific length.
                  Another routine might accept only Mersenne prime moduli.

            Each modmul_algorithm should be able to do both squarings and
        general multiplications.  It might branch to separate squaring code,
        but that check will be made when the routine is called, not now.

            If *pindex = 5 (say) on entry, then the fifth qualifying routine
        will be used.  During testing, the test routine calls this once
        with *pindex = 0, ending with *pindex = -k where k is the number
        of routines accepting this modulus.  Then it makes one call each with
        *pindex = 1, 2, ..., k, checking that all k procedures
        give consistent results (and perhaps comparing their timings).
        During production, the second call uses *pindex = k, and the
        final qualifying one is chosen.  For this reason, faster codes
        (typically assembly language versions) should appear last.

            Each routine accepting the modulus should save its address,
        its name, and the number of digit_t temporaries it will need.
*/
{
    DRM_BOOL OK = TRUE;
    DRM_LONG index = *pindex;
    const DRM_DWORD lng = pmodulo->length;

    if (pmodulo->reddir == FROM_LEFT) {
        index--;
        if (index >= 0) {
            pmodulo->modmul_algorithm = modmul_from_left_default;
            pmodulo->modmul_algorithm_temps = 2*lng;
        }
    } else if (pmodulo->reddir == FROM_RIGHT) {
        index--;
        if (index >= 0)
        { /*  If the lenght of the modulo is 5, then we have special
          **  function that optimized for 5.
          **  It is used in ECC 160 algorithms.
          **  RSA uses lenght of 16, 17 and 32.
          **  Generic implementation for these cases is modmul_from_right_default
          */

            /* LNGQ_MODULO_5 is an unlikely length to be used */
            if ( pmodulo->length == LNGQ_MODULO_8 )
            {
                pmodulo->modmul_algorithm = modmul_from_right_default_modulo8;
            }
            else
            {
                pmodulo->modmul_algorithm = modmul_from_right_default;
            }
            pmodulo->modmul_algorithm_temps = 2*lng;
        }
    }
    *pindex = index;
    return OK;
} /* end modmul_choices1 */
#endif
EXIT_PK_NAMESPACE_CODE;
