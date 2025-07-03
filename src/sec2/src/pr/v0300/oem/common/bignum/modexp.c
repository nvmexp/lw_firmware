/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#define DRM_BUILDING_MODEXP_C
#include "bignum.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
/*
        File modexp.c.  Modular_exponentiation routines.

        DRM_BOOL mod_exp(base, exponent, lngexpon,
                        answer, modulo)
                     -- Compute answer = base^exponent mod modulo->modulus.
                        The 2000 version of the code stores fewer
                        Karatsuba images, to reduce memory requirements
                        and hopefully improve cache utilization.

            Function mod_exp (modular exponentiation)
        computes b^e (modulo something).
        Here b and e are multiple precision, typically
        from 512 to 2048 bits.  We strive hard to keep
        the number of multiplications low.

            For a treatise on methods of exponentiation,
        see Section 4.6.3 of Donald E. Knuth, The Art of
        Computer Programming, Volume 2, Seminumerical Algorithms,
        2nd Edition, 1981.

            The classical left-to-right binary method of exponentiation
        (Knuth, p. 441) looks at one bit of the exponent at a
        time, starting from the most significant bit.
        It requires  log2(e) + (number of 1's in e) - 1
        multiplications (or squarings), which averages about
        (3/2)*log2(e) multiplications for a random exponent
        with log2(e)+1 bits.

            When e is large, this cost can be reduced by using
        radix 2^k for suitable k (Knuth, p. 446).
        The cost is 2^k - 2 multiplications to build a table with
        b^0 = 1,  b^1 = b,  b^2,  b^3,  ... ,  b^(2^k - 1).
        After this table is built, there will be about log2(e)/k steps
        consisting of k squarings and a multiplication by a table entry.
        The final multiply can be skipped when the table entry is
        b^0 = 1, which oclwrs with probability about 2^(-k).
        The approximate cost of the radix 2^k method (Knuth, p. 451) is

                              log2(e)           log2(e)
           2^k + (k+1-2^(-k)) -------  =  2^k + -------(1 - 2^(-k)) + log2(e)
                                 k                 k

            mod_exp1995 (no longer here) usea a modified method with radix
        2^k and cost about

                         log2(e)
(*)            2^(k-1) + -------  + log2(e).
                           k+1

        It resembled that in ``Fast square-and-multiply
        exponentiation for RSA'' by L. C. K. Hui and K.-Y. Lam,
        Electronics Letters, 18 August 1994, Volume 30, No, 17,
        pp. 1396-1397, Electronics Letters Online No. 19940949.

            The upcoming presentation assumes k = 6.
        A relwrsive formulation of the mod_exp1995 algorithm resembles

                if e is small then
                    Use table look-up for b^e
                else if e is even
                    Compute  b^e  as  (b^(e/2))^2
                else
                    Compute  b^e  as  (b^j)^64 * b^i,
                    where e = 64 * j + i and 0 <= i <= 63
                end if

        Except possibly when initializing the relwrsion
        (i.e., to compute b^e where e is `small'), the algorithm needs
        only the odd powers  b^1,  b^3,  b^5,  ...,  b^63.
        It costs 32 = 2^(k-1) multiplications to compute
        b^2 and this table.

            Half of the time this value of i
        (i.e., where e = 64*j + i) will be in [0, 31].
        In those cases, we can delay the multiplication by b^i
        for one step, merely replacing x by x^2 at this time
        and awaiting another bit of the exponent.
        This explains the denominator (k+1) in (*).

            The optimal k (i.e., the one requiring the fewest
        multiplications) depends upon the exponent e.
        According to (*), the value k is better than k-1 if

                      log2(e)                           log2(e)
            2^(k-1) + -------  + log2(e)  <   2^(k-2) + ------- + log2(e) .
                        k+1                                k

                                           log2(e)
        This is equivalent to  2^(k-2)  <  -------  and to
                                           k (k+1)

        log2(e) > k (k+1) 2^(k-2)


            k         Approximate log2(e)
                      where k first beats k-1

            2            6
            3           24
            4           80
            5          240
            6          672
            7         1680
            8         4608

        For a 1024-bit exponent, this analysis suggests using k = 6
        since 672 <= 1024 <= 1680.  The estimated costs
        for a 1024-bit exponent are

            k         2^(k-1) + 1024/(k + 1) + 1024

            1         1537
            2         1367.3
            3         1284
            4         1236.8
            5         1210.7
            6         1202.3
            7         1216
            8         1265.8

        The estimated costs for k = 5, 6, 7 vary by less than 1.2%.
        The precise costs will depend upon the bit pattern in
        the exponent.  The code tries multiple values of k,
        determining the cost associated with each radix,
        and selects the one with the smallest cost.

        mod_exp uses a window of width 2^k on the exponent.
        There are bucket_max = 2^k - 1 buckets bucket[1]
        to bucket[bucket_max], all initialized to 1.
        The final result is intended to be

           answer = bucket[1]^1 * bucket[2]^2 * bucket[3]^3 * ...
                                * bucket[bucket_max]^bucket_max

        Except during final processing, all even-numbered buckets
        are empty (i.e., contain 1)


*/

#if defined (_WIN32_WCE)
         // Opt for less memory under Windows CE
#define MAX_BUCKET_WIDTH 5
#else
#define MAX_BUCKET_WIDTH 6
#endif


typedef struct {         // Temps and interface for mod_exp and helpers
        const mp_modulus_t   *modulo;
        DRM_BOOL             bucket_oclwpied[1L << MAX_BUCKET_WIDTH];
        digit_t         *bucket_location[1L << MAX_BUCKET_WIDTH];
        digit_t         *modmultemps;  // Length modmul_algorithm_temps
    } temps_t;

/***************************************************************************/

/*
      _basepower_squaring and _bucket_multiply are helpers to mod_exp.

      _bucket_multiply multiplies the contents
      of bucket[ibucket] by multiply_by.

      _basepower_squaring replaces basepower by basepower^2.

      The temps struct is used to share miscellaneous
      local and malloc-ed data with mod_exp.
*/
#if defined(SEC_COMPILE)
static DRM_BOOL DRM_CALL _basepower_squaring(
        digit_t      *basepower,               // IN/OUT
        temps_t  *temps,                   // IN/OUT
        struct bigctx_t *f_pBigCtx)
{
    DRM_BOOL OK = TRUE;
    const mp_modulus_t *modulo = temps->modulo;

    OK = OK && mod_mul(basepower, basepower, basepower,
                           modulo, temps->modmultemps, f_pBigCtx);
    return OK;
} // end _basepower_squaring



static DRM_NO_INLINE DRM_BOOL _bucket_multiply
        (const DRM_DWORD    ibucket,                // IN
         const digit_t     *multiply_by,            // IN
               temps_t     *temps,                  // IN/OUT
         struct bigctx_t   *f_pBigCtx)
{
    DRM_BOOL OK = TRUE;
    digit_t       *bloc       = temps->bucket_location[ibucket];
    const mp_modulus_t *modulo     = temps->modulo;
    const DRM_DWORD      elng       = modulo->length;

    if (temps->bucket_oclwpied[ibucket]) {

        if( !mod_mul(bloc, multiply_by, bloc, modulo,
                    temps->modmultemps, f_pBigCtx) )
        {
            OK = FALSE;
        }
    } else {
        temps->bucket_oclwpied[ibucket] = TRUE;

        OEM_SELWRE_DIGITTCPY( bloc,multiply_by,elng);
    }
    return OK;
} // end _bucket_multiply

static const unsigned short width_lwtoffs[] PR_ATTR_DATA_OVLY(_width_lwtoffs) = {6, 24, 80, 240, 672};
                                           // (k + 2)*(k + 3) * 2^k
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL mod_exp
        (const digit_t         *base,
         const digit_t         *exponent,
         const DRM_DWORD        lngexpon,
               digit_t         *answer,
         const mp_modulus_t    *modulo,
         struct bigctx_t       *f_pBigCtx)

{
    DRM_BOOL OK = TRUE;
    const DRM_DWORD elng = modulo->length;
    const DRM_DWORD exponent_bits_used
             = mp_significant_bit_count(exponent, lngexpon);

    temps_t *temps = NULL;       // malloc-ed
    digit_t *basepower = answer;           // Same array used for both
    digit_t *bucket_data = NULL;     // malloc-ed

    DRM_DWORD bucket_mask, bucket_width, carried;
    DRM_DWORD ibit, ibucket, max_bucket, ndoubling;
    DRM_BOOL base2;

    bucket_width = 1;
    while (    bucket_width < MAX_BUCKET_WIDTH
            && (DRM_DWORD)width_lwtoffs[bucket_width-1] < exponent_bits_used) {
        bucket_width++;
    }

    OK = OK && validate_modular_data(base, modulo->modulus, elng);

    bucket_mask = (DRM_DIGIT_ONE << bucket_width) - 1;
    max_bucket = bucket_mask;   // Another name for the variable

    temps = (temps_t *) bignum_alloc(sizeof(temps_t), f_pBigCtx );

    if ((elng == 0) ||
        (elng*max_bucket + modulo->modmul_algorithm_temps < elng*max_bucket)) /* check for integer underflow/overlfow */
    {
        bucket_data = NULL;
    }
    else
    {
        bucket_data = digit_allocate(elng*max_bucket + modulo->modmul_algorithm_temps,
                                     f_pBigCtx);
    }

    OK = OK && (temps != NULL) && (bucket_data != NULL);

    base2 = FALSE;
    if (OK) {
        temps->modulo = modulo;
        temps->bucket_location[0] = NULL;
        temps->modmultemps = bucket_data + elng*max_bucket;

        //  Check for base = 2.  When base = 2, use alternate algorithm.
        //  (squarings and doublings).

        OK = OK && add_mod(modulo->one, modulo->one,
                           bucket_data, modulo->modulus, modulo->length);
                                                        /* bucket_data = 2 */
        base2 = OK && compare_same(base, bucket_data, elng) == 0;
    }
    if (!OK) {
    } else if (base2 && exponent_bits_used != 0) {
        const DRM_DWORD shift_max = DRM_MIN(DRM_RADIX_BITS*elng, 1024);
        DRM_DWORD high_expon_bits = 0;
        DRM_BOOL high_bits_processed = FALSE;
        digit_t *dtemp = bucket_data; /* Some colwenient temporary */

        /*
             Start at the most significant end of the exponent.
             Accumulate the first few bits in high_expon_bits.
             As long as 2^high_expon_bits is about the
             same size as the modulus, or smaller,
             continue this aclwmulation.

             While processing the lower bits of the exponent,
             do repeated squaring, plus a doubling
             where the exponent bit is 1.

        */
        for (ibit = exponent_bits_used; OK && (ibit--) != 0; /*null*/) {
            const DRM_DWORD iexpbit = (DRM_DWORD)mp_getbit(exponent,ibit);

            if (high_bits_processed) {

                OK = OK && mod_mul(dtemp, dtemp, dtemp,
                                   modulo, temps->modmultemps,
                                   f_pBigCtx);
                if (iexpbit != 0) {
                    OK = OK && add_mod(dtemp, dtemp, dtemp,
                                       modulo->modulus, modulo->length);
                }
            } else {  /* high_bits_processed */
                high_expon_bits = 2*high_expon_bits + iexpbit;
                if (   ibit == 0
                    || 2*high_expon_bits >= shift_max) {

                    high_bits_processed = TRUE;
                    OK = OK && mod_shift(modulo->one,
                              (DRM_LONG)high_expon_bits,
                              dtemp, modulo);

                }
            } /* high_bits_processed */
        } /* for ibit */
        temps->bucket_location[1] = dtemp;  /* For copy into answer */

        if (!high_bits_processed) {
            OK = FALSE;
            SetMpErrno_clue(MP_ERRNO_INTERNAL_ERROR,
                            "mod_exp -- high_bits_processed = FALSE");
        }
    } else {  /* not base2 */

         // Partition bucket_data array into length-elng
         // pieces for individual buckets.
         // Allocate pieces for odd-numbered buckets
         // contiguously, to lessen cache conflicts.

        for (ibucket = 1; OK && ibucket <= max_bucket; ibucket++) {
            digit_t *bloc = bucket_data
                + elng*(ibucket - 1 + (DRM_IS_EVEN(ibucket) ? max_bucket : 0))/2;

            temps->bucket_location[ibucket] = bloc;
            temps->bucket_oclwpied[ibucket] = FALSE;

            OEM_SELWRE_DIGITTCPY( bloc,modulo->one,elng);
                                                      // Set bucket contents = 1
        }  // for ibucket

        if (OK) {
            OEM_SELWRE_DIGITTCPY( basepower,base,elng);    // basepower = base
        }
        carried = 0;
        ndoubling = 0;

        for (ibit = 0; OK && ibit != exponent_bits_used; ibit++) {
            const digit_t bit_now = mp_getbit(exponent, ibit);

            // Want bucket[1]^1 * ... * bucket[max_bucket]^max_bucket
            //  * basepower^ (2^ndoublings * remaining exponent bits + carried)
            // 0 <= carried <= 2^(bucket_width + 2) - 1

            if (carried >> (bucket_width + 2) != 0) {
                OK = FALSE;
                SetMpErrno_clue(MP_ERRNO_INTERNAL_ERROR,
                                "mod_exp -- carried overflow");
            }

            if (bit_now != 0) {
                while (OK && ndoubling >= bucket_width + 1) {
                    if (DRM_IS_ODD(carried)) {
                        ibucket = carried & bucket_mask;
                        carried -= ibucket;
                        OK = OK && _bucket_multiply(ibucket, basepower,
                                                   temps, f_pBigCtx);
                    }
                    OK = OK && _basepower_squaring(basepower,
                                                  temps, f_pBigCtx);
                    carried /= 2;
                    ndoubling--;
                }
                carried |= (DRM_DWORD)1 << ndoubling;
            }
            ndoubling++;
        } // for ibit

        while (OK && carried != 0) {
            DRM_BOOL square_now = FALSE;

            if (carried <= max_bucket) {
                ibucket = carried;
            } else if (DRM_IS_EVEN(carried)) {
                square_now = TRUE;
            } else if (carried <= 3*max_bucket) {
                ibucket = max_bucket;
            } else {
                ibucket = carried & bucket_mask;
            }
            if (square_now) {
                carried /= 2;
                OK = OK && _basepower_squaring(basepower,
                                              temps, f_pBigCtx);
            } else {
                carried -= ibucket;

                OK = OK && _bucket_multiply(ibucket, basepower,
                                           temps, f_pBigCtx);
            }
        } // while carried

        for (ibucket = max_bucket; OK && ibucket >= 2; ibucket--) {
            if (temps->bucket_oclwpied[ibucket]) {
                DRM_BOOL found = FALSE;
                DRM_DWORD jbucket, jbucketlargest, kbucket;
                const digit_t *bloci;

                // Try to find jbucket, kbucket, such that
                // jbucket+kbucket = ibucket, both oclwpied.

                if (DRM_IS_EVEN(ibucket)) {
                         // Defaults in case no jbucket found below
                    jbucketlargest = ibucket/2;
                } else {
                    jbucketlargest = 1;
                }
                for (jbucket = ibucket >> 1;
                    jbucket != ibucket && !found;
                    jbucket++) {

                    if (temps->bucket_oclwpied[jbucket]) {
                        jbucketlargest = jbucket;
                        found = temps->bucket_oclwpied[ibucket - jbucket];
                    }
                }
                jbucket = jbucketlargest;
                kbucket = ibucket - jbucket;

                   // Deposit ibucket contents at jbucket and kbucket.

                bloci = temps->bucket_location[ibucket];

                OK = OK && _bucket_multiply(jbucket, bloci, temps, f_pBigCtx);

                OK = OK && _bucket_multiply(kbucket, bloci, temps, f_pBigCtx);

            } // if ibucket oclwpied
        }  // for ibucket
    } // if base 2

    if (OK) {
        OEM_SELWRE_DIGITTCPY( answer,temps->bucket_location[1],elng);
    }
    if (bucket_data != NULL) {
        bignum_free(bucket_data, f_pBigCtx);
    }
    if (temps != NULL) {
        bignum_free(temps, f_pBigCtx);
    }
    return OK;
} // end mod_exp
#endif
#undef MAX_BUCKET_WIDTH

EXIT_PK_NAMESPACE_CODE;
