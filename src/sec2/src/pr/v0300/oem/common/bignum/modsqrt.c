/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#define DRM_BUILDING_MODSQRT_C
#include "bignum.h"
#include "mprand.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
/*
//      DRM_BOOL mod_sqrt(base, answer, modulo, psquare) --
//              Compute a square root
//              answer = +- sqrt(base) mod modulo->modulus.
//              modulo->modulus should be a prime.
//              Set *psquare = TRUE if successful, FALSE if unsuccessful
*/
/***************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL mod_sqrt
        (const digit_t       *base,             // IN
               digit_t       *answer,           // OUT
         const mp_modulus_t  *modulo,           // IN
               DRM_BOOL      *pperfect_square,  // OUT
         struct bigctx_t     *f_pBigCtx)
/*
        Compute a square root answer = +- sqrt(base) mod modulo->modulus.
        modulo->modulus should be prime.
        Set *perfect_square = TRUE is successful, FALSE if not.

        base and answer must not overlap.
*/
{
    DRM_BOOL        OK             = TRUE;
    DRM_BOOL        perfect_square = TRUE;
    const DRM_DWORD   elng           = modulo->length;
    digit_t    *dtemps         = NULL;

    if ((elng > 0) &&
        (6*elng + modulo->modmul_algorithm_temps > 6*elng))  /* check for integer underflow/overflow */
    {
        dtemps = digit_allocate(2*elng + modulo->modmul_algorithm_temps, f_pBigCtx);
    }

    if (dtemps == NULL) {
        OK = FALSE;
    } else if (base == NULL || answer == NULL) {
        OK = FALSE;
        SetMpErrno_clue(MP_ERRNO_NULL_POINTER, "mod_sqrt");
    } else if (base == answer) {
        OK = FALSE;
        SetMpErrno_clue(MP_ERRNO_OVERLAPPING_ARGS, "mod_sqrt");
    } else if (significant_digit_count(base,  elng) == 0) {
        perfect_square = TRUE;
        OEM_SELWRE_DIGITTCPY( answer,base, elng );      // sqrt(0) = 0
    } else {
        DRM_BOOL verified = FALSE;
        digit_t *exponent = dtemps + elng;
        digit_t *temp2 = dtemps;
        digit_t *modmultemps = dtemps + 2*elng;

        switch (modulo->modulus[0] & 7) {
        case 3:
        case 7:
                  // p == 3 mod 4
                  // Let answer == base^((p+1)/4)
                  // Then answer^2 == base*((p+1)/2) == base
                  //      if base is a square

            OK = OK && mp_shift(modulo->modulus, -2, exponent,
                                elng);// (p-3)/4
            if (OK) (DRM_VOID)add_immediate(exponent, 1, exponent, elng);      // (p+1)/4
            OK = OK && mod_exp(base, exponent, elng,
                               answer, modulo, f_pBigCtx);
            break;
        default:
            /* PR code never uses other prime numbers for this function, so we don't implement the other cases */
            DRMASSERT( FALSE );
            OK = FALSE;
            break;
        } /* switch */

        if (!verified) {
            OK = OK && mod_mul(answer, answer, temp2, modulo,
                               modmultemps, f_pBigCtx);
            if (OK && compare_same(temp2, base, elng) != 0) {
                perfect_square = FALSE;
            }
        }
    }
    if (dtemps != NULL) {
        Free_Temporaries(dtemps, f_pBigCtx);
    }
    if (OK) *pperfect_square = perfect_square;
    return OK;
} /* mod_sqrt */
#endif
EXIT_PK_NAMESPACE_CODE;
