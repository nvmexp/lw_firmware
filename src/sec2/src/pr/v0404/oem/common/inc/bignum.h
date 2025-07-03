/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#ifndef BIGNUM_H              /* If not previously #included */
#define BIGNUM_H 1

#include "bigdefs.h"

/*
** Expensive debugging adds some additional parameter checks,
** such as whether a point is on the lwrve as expected
** Note: This is lwrrently unused in the PK
*/
#ifndef BIGNUM_EXPENSIVE_DEBUGGING
#define BIGNUM_EXPENSIVE_DEBUGGING 0
#endif

#define MP_LONGEST_BITS 4096

/*
** Multiple precision moduli can have up to
** MP_LONGEST_BITS bits, which is
** MP_LONGEST words.  Some routines allow
** longer operands.
** 
** Older codes have used this (and MP_LONGEST, below)
** to dimension arrays.  New code should avoid
** referencing MP_LONGEST and MP_LONGEST_BITS.
*/

#define MP_LONGEST  ( MP_LONGEST_BITS / DRM_RADIX_BITS )

/*
** DOUBLE_SHIFT_LEFT(n1, n0, amt) returns
** n1 shifted left by amt bits,
** with new bits coming in from the top of n0.
**
** DOUBLE_SHIFT_RIGHT(n1, n0, amt) returns n0 shifted right
** by amt bits, with new bits coming from the bottom of n1.
**
** The shift counts must satisfy 0 <= amt <= DRM_RADIX_BITS - 1.
** The shift by    DRM_RADIX_BITS - amt   is done in two stages
** (first by 1, then by DRM_RADIX_BITS - 1 - amt),
** to avoid an illegal shift count of DRM_RADIX_BITS when amt = 0.
*/

#define DOUBLE_SHIFT_LEFT(n1, n0, amt)      ( ( ( n1 ) << ( amt ) ) | ( ( ( n0 ) >> 1 ) >> ( DRM_RADIX_BITS - 1 - ( amt ) ) ) )
#define DOUBLE_SHIFT_RIGHT(n1, n0, amt)     ( ( ( n0 ) >> ( amt ) ) | ( ( ( n1 ) << 1 ) << ( DRM_RADIX_BITS - 1 - ( amt ) ) ) )
#define MP_GCDEX_NTEMPS( lnga, lngb )       ( 8 * DRM_MAX( ( lnga ), ( lngb ) ) + 6 )
#define MP_ILWERT_NTEMPS( lng )             ( ( lng ) + MP_GCDEX_NTEMPS( ( lng ), ( lng ) ) )

#include "dblint.h"

ENTER_PK_NAMESPACE;

/*
** Some struct names are referenced in #include files before they are
** defined.  For example, there might be two struct definitions each
** containing a pointer to the other struct type.
** We list some struct names in advance here, to avoid warnings.
*/
Future_Struct(mp_modulus_t);      /* See this file */
Future_Struct(digit_tempinfo_t);  /* See bigpriv.h */


/*
** The reciprocal_1_t type is used when div21
** or divide or divide_immediate would otherwise
** divide by the same number repeatedly.  See file divide.c.
*/

typedef struct {
    digit_t    multiplier;
    DRM_DWORD  shiftamt;
} reciprocal_1_t;

/*
**     mp_modulus_t struct has modulus-dependent constants
**     used for fast reduction (typically for a fixed modulus,
**     which will be used several times, as in modular exponentiation).
**     These constants are initialized by function create_modulus:
**
**     modulus : Modulus used for computations.  Must be nonzero.
**
**     length  : Length (>= 1) of the modulus, without leading zeros.
**                Operands to mod_add, mod_mul, mod_sub, ...
**                are assumed to have this length.
**
**     reddir  : Equal to FROM_LEFT if reductions of
**                products are done from the left (traditional
**                division), and to FROM_RIGHT if reductions of
**                products are done from the right (Montgomery reduction).
**
**                When using FROM_RIGHT, the modulus must be odd.
**                Arguments to mod_mul should be pre-scaled by
**                2^scaling_power2 (mod modulus).
**                The product will be similarly scaled.
**
**     scaling_power2 :  Equal to length*DRM_RADIX_BITS when reddir = FROM_RIGHT.
**                Zero if reddir = FROM_LEFT.
**
**     one :     Constant 1 (muldiplicative identity), length length.
**                Nmerical value is 2^scaling_power2 (mod modulus).
** Denote
**
**           length = pmodulo->length
**           modulus = pmodulo->modulus
**           shiftamt = pmodulo->left_reciprocal_1.shiftamt.
**
**       Then
**
**           0 <= shiftamt < DRM_RADIX_BITS
**           RADIX^length/2 <= modulus * 2^shiftamt < RADIX^length
**           modulus < RADIX^length / 2^shiftamt <= 2*modulus
**
**       Some variants of modmul_algorithm use additional constants
**       lngred2, multiplier_first, multiplier_second.
**
**       FROM_LEFT arithmetic, these constants satisfy
**
**           modulus * (multiplier_second + RADIX^lngred2)
**         = RADIX^(length + lngred2) / 2^shiftamt + multiplier_first
**
**           0 <= multiplier_first < modulus
**           0 <= multiplier_second < RADIX^lngred2
**           lngred2 = CEIL(length/2)
**
**      For FROM_RIGHT arithmetic, these constants satisfy
**
**           multiplier_second * modulus
**         = 1 + multiplier_first * RADIX^lngred2
**
**           0 <= multipler_first < modulus
**           0 <= multiplier_second < RADIX^lngred2
**           lngred2 = CEIL(length/2)
**
**     one :     Constant 1 (multiplicative identity).
**                For FROM_LEFT arithmetic, pmodulo->one = 1.
**                For FROM_RIGHT arithmetic,
**                    pmodulo->one = (RADIX^length) % pmodulus;
**
**     left_reciprocal_1 : Reciprocal of the divisor starting at the
**                leftmost digit (i.e., modulus[length-1]);
**
**                See file divide.c for an explanation
**                about how this constant is used to get accurate
**                quotients when dividing from the left.
**
**     right_reciprocal_1 : If modulus is odd, this holds
**                1/modulus (mod RADIX), for use in mod_shift.
**                Otherwise the field is zero.
**
**       Denote
**
**           length = pmodulo->length
**           modulus = pmodulo->modulus
**           shiftamt = pmodulo->left_reciprocal_1.shiftamt.
**
**       Then
**
**           0 <= shiftamt < DRM_RADIX_BITS
**           RADIX^length/2 <= modulus * 2^shiftamt < RADIX^length
**           modulus < RADIX^length / 2^shiftamt <= 2*modulus
**
**     modmul_algorithm :
**           This library has a variety of codes for modular multiplication.
**           The mp_modulus_t struct has a pointer to the precise code
**           being used for a particular number and architecture.  A call
**
**                 (*modmul_algorithm)(a, b, c, pmodulo, temps)
**
**           is supposed to set
**
**                  c = a*b/3^(pmodulo->scaling_power2)  (mod pmodulo->modulus)
**
**           where 0 <= a, b, < pmodulo->modulus.
**           The output c may overlap a or b.
**
**           The temps array will have at least pmodulo->modmul_algorithm_temps
**           elements of type digit_t, aligned on a digit_t boundary.
**
**           The simplest modmul_algoriuthm procedures,
**           modmul_from_left_default and modmul_from_right_default,
**           work on all architectures.
**
**           In some implementations of this library, create_modulus may
**           examine the precise modulus and the precise hardware
**           we are running on, substituting another algorithm
**           or an assembly language code.
**
**           Some variants of modmul_algorithm use additional constants
**           lngred2, multiplier_first, multiplier_second.
**           In FROM_LEFT arithmetic, these constants satisfy
**
**               modulus * (multiplier_second + RADIX^lngred2 + 1))
**             = RADIX^(length + lngred2) / 2^shiftamt + multiplier_first
**
**               0 <= multiplier_first < modulus
**               0 <= multiplier_second < RADIX^lngred2
**               lngred2 = CEIL(length/2)
**
**          For FROM_RIGHT arithmetic, these constants satisfy
**
**               multiplier_second * modulus
**             = 1 + multiplier_first * RADIX^lngred2
**
**               0 <= multipler_first < modulus
**               0 <= multiplier_second < RADIX^lngred2
**               lngred2 = CEIL(length/2)
*/

typedef DRM_BOOL DRM_CALL modmul_algorithm_t(
    __in_ecount( pmodulo->length )        const digit_t             *a,
    __in_ecount( pmodulo->length )        const digit_t             *b,
    __out_ecount( pmodulo->length )             digit_t             *c,
    __in_ecount( 1 )                      const struct mp_modulus_t *pmodulo,   /* note: ansi build requires using 'struct mp_modulus_t', not just 'mp_modulus_t' */
    __inout_ecount( pmodulo->length * 2 )       digit_t             *temps );

typedef enum {FROM_LEFT, FROM_RIGHT} reddir_t;

typedef struct mp_modulus_t {
                  DRM_DWORD  length;         /* Length passed to create_modulus*/
                  DRM_DWORD  lngred2;        /* CEIL(length/2) */
                  DRM_DWORD  modmul_algorithm_temps; /* Number of digit_t temps used by modmul_algorithm */
                  DRM_LONG   scaling_power2; /* DRM_RADIX_BITS*length for FROM_RIGHT, 0 for FROM_LEFT */
                  reddir_t  reddir;         /* FROM_LEFT or FROM_RIGHT */
                  reciprocal_1_t  left_reciprocal_1;
                  digit_t   right_reciprocal_1;  /* 1/modulus[0] mod RADIX, if modulus is odd */
                  digit_t   *modulus;
                  /* interesting fact: we do not use these two multiplier variables. */
                  /* removing them, however, is very painful, */
                  /* if someone is calling something from a stublib */
                  /* with mp_modulus_t type, then removal will cause a bad break. */
                  digit_t   *multiplier_first;  /* See text */
                  digit_t   *multiplier_second; /* See text */
                  digit_t   *one;               /* Multiplicative constant 1 */
                  modmul_algorithm_t *modmul_algorithm;
                                  /* Function pointer for multiplication */
                } mp_modulus_t;


/*
** When an error is detected, the SetMpErrno_clue macro gets
** an error code (below) and an English-language string
** with more information.
** This macro will normally call an application-supplied routine.
** The application routine might print a message or set a global variable.
** 
** The library routine detecting the error will exit with return value
** FALSE, notifying its caller that something abnormal oclwrred.
** 
** Except for MP_ERRNO_NO_ERROR, the error codes are
** in alphabetical order.
*/

typedef enum
{
    MP_ERRNO_NO_ERROR = 0,          /* Success */
    MP_ERRNO_DEGREE_OVERFLOW,       /* Polynomial degree too high for allocated memory */
    MP_ERRNO_DIVIDE_ZERO,           /* Divide by zero (or by number with leading zero) */
    MP_ERRNO_ELSEWHERE,             /* Error indicator returned by some routine which may not have called SetMpErrno_clue (e.g., CRYPTAPI library, assembly codes) */
    MP_ERRNO_INTERNAL_ERROR,        /* Internal error found : please report */
    MP_ERRNO_ILWALID_DATA,          /* Invalid arguments */
    MP_ERRNO_MODULAR_TOO_BIG,       /* Modular operand >= modulus */
    MP_ERRNO_NO_MEMORY,             /* malloc failure */
    MP_ERRNO_NOT_IMPLEMENTED,       /* Case not implemented */
    MP_ERRNO_NOT_ILWERTIBLE,        /* Perhaps trying to ilwert pmodulo non-prime */
    MP_ERRNO_NOT_ON_LWRVE,          /* Point is not on elliptic lwrve */
    MP_ERRNO_NULL_POINTER,          /* NULL argument where valid argument expected */
    MP_ERRNO_OVERFLOW,              /* Integer overflow (or unexpectedly negative) */
    MP_ERRNO_OVERLAPPING_ARGS,      /* Overlapping (i.e., duplicate) arguments where they are disallowed */
    MP_ERRNO_TOO_MANY_ITERATIONS,   /* e.g., unable to find large prime */
    MP_ERRNO_ZERO_OPERAND,          /* Zero operand(s) where nonzero expected */
    MP_ERRNO_COUNT                  /* Number of entries above */
} mp_errno_t;

#define SetMpErrno_clue(errcode, debug_info) DRM_DO { DRMCASSERT( errcode != 0 ); DRM_DBG_TRACE(( "Bignum Error %u  Clue \"%s\"", errcode, debug_info )); } DRM_WHILE_FALSE

DRM_API digit_t DRM_CALL accumulate(
    __in_ecount( lng )    const digit_t   *a,
    __in                  const digit_t    mult,
    __inout_ecount( lng )       digit_t   *b,
    __in                  const DRM_DWORD  lng );

DRM_API DRM_BOOL DRM_CALL add_diff(
    __in_ecount( lnga ) const digit_t   *a,
    __in                const DRM_DWORD  lnga,
    __in_ecount( lngb ) const digit_t   *b,
    __in                const DRM_DWORD  lngb,
    __out_ecount( lnga )      digit_t   *c,
    __out_ecount_opt( 1 )     digit_t   *pcarry );

DRM_API DRM_BOOL DRM_CALL add_full(
    __in_ecount( lnga )                         const digit_t   *a,
    __in                                        const DRM_DWORD  lnga,
    __in_ecount( lngb )                         const digit_t   *b,
    __in                                        const DRM_DWORD  lngb,
    __out_ecount( 1 + DRM_MAX( lnga, lngb ) )         digit_t   *c,
    __out_ecount( 1 )                                 DRM_DWORD *plngc );

DRM_API digit_t DRM_CALL add_immediate(
    __in_ecount( lng ) const digit_t   *a,
    __in               const digit_t    iadd,
    __out_ecount( lng )      digit_t   *b,
    __in               const DRM_DWORD  lng );

DRM_API DRM_BOOL DRM_CALL add_mod(
    __in_ecount( lng )   const digit_t   *a,
    __in_ecount( lng )   const digit_t   *b,
    __inout_ecount( lng )      digit_t   *c,
    __in_ecount( lng )   const digit_t   *modulus,
    __in                 const DRM_DWORD  lng );

DRM_API_VOID digit_t DRM_CALL add_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __out_ecount( lng )      digit_t   *c,
    __in               const DRM_DWORD  lng );

DRM_API_VOID sdigit_t DRM_CALL add_sub_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __in_ecount( lng ) const digit_t   *c,
    __out_ecount( lng )      digit_t   *d,
    __in               const DRM_DWORD  lng );

DRM_API DRM_LONG DRM_CALL compare_diff(
    __in_ecount( lnga ) const digit_t   *a,
    __in                const DRM_DWORD  lnga,
    __in_ecount( lngb ) const digit_t   *b,
    __in                const DRM_DWORD  lngb );

DRM_API DRM_LONG DRM_CALL compare_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __in               const DRM_DWORD  lng );

DRM_API DRM_LONG DRM_CALL compare_sum_diff(
    __in_ecount( lnga ) const digit_t   *a,
    __in                const DRM_DWORD  lnga,
    __in_ecount( lngb ) const digit_t   *b,
    __in                const DRM_DWORD  lngb,
    __in_ecount( lngc ) const digit_t   *c,
    __in                const DRM_DWORD  lngc );

DRM_API DRM_LONG DRM_CALL compare_sum_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __in_ecount( lng ) const digit_t   *c,
    __in               const DRM_DWORD  lng );

DRM_API DRM_BOOL DRM_CALL create_modulus(
    __in_ecount( lnga ) const digit_t           *a,
    __in                const DRM_DWORD          lnga,
    __in                const reddir_t           reddir,
    __out_ecount( 1 )         mp_modulus_t      *pmodulo,
    __inout                   struct bigctx_t   *f_pBigCtx,
    __inout                   struct bigctx_t   *pbigctxGlobal );

DRM_API digit_t DRM_CALL delwmulate(
    __in_ecount( lng )    const digit_t  *a,
    __in                  const digit_t   mult,
    __inout_ecount( lng )       digit_t  *b,
    __in                  const DRM_DWORD lng );

DRM_API digit_t* DRM_CALL digit_allocate(
    __in           const DRM_DWORD   nelmt,
    __inout struct bigctx_t         *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL digit_ogcd(
    __in              const digit_t  d1,
    __in              const digit_t  d2,
    __out_ecount( 1 )       digit_t *pgcd );

DRM_API DRM_BOOL DRM_CALL div21(
    __in              const DRM_UINT64   db,
    __in              const digit_t      d,
    __out_ecount( 1 )       digit_t     *quot,
    __out_ecount( 1 )       digit_t     *rem );

DRM_EXTERN_INLINE DRM_BOOL DRM_CALL div21_fast(
    __in                const DRM_UINT64         db,
    __in                const digit_t            d,
    __in_ecount( 1 )    const reciprocal_1_t    *recip,
    __out_ecount( 1 )         digit_t           *quot,
    __out_ecount( 1 )         digit_t           *rem );


DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL divide(
    __in_ecount( lnum )                                                         const digit_t         *numer,
    __in                                                                        const DRM_DWORD        lnum,
    __in_ecount( lden )                                                         const digit_t         *denom,
    __in                                                                        const DRM_DWORD        lden,
    __in_ecount_opt( 1 )                                                        const reciprocal_1_t  *supplied_reciprocal,
    __out_ecount_opt( DRM_MAX( ( (DRM_LONG)lnum - (DRM_LONG)lden ) + 1, 0 ) )         digit_t         *quot,
    __out_ecount( lden )                                                              digit_t         *rem );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL divide_precondition_1(
    __in_ecount( lden ) const digit_t         *denom,
    __in                const DRM_DWORD        lden,
    __inout_ecount( 1 )       reciprocal_1_t  *recip );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL divide_immediate(
    __in_ecount( lng )       const digit_t         *numer,
    __in                     const digit_t          den,
    __in_ecount_opt( 1 )     const reciprocal_1_t  *supplied_reciprocal,
    __out_ecount_opt( lng )        digit_t         *quot,
    __in                     const DRM_DWORD        lng,
    __out_ecount( 1 )              digit_t         *prem );

DRM_EXTERN_INLINE digit_t DRM_CALL estimated_quotient_1(
    __in             const digit_t         n2,
    __in             const digit_t         n1,
    __in             const digit_t         n0,
    __in_ecount( 1 ) const reciprocal_1_t *recip );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL from_modular(
    __in_ecount( pmodulo->length )  const digit_t       *a,
    __out_ecount( pmodulo->length )       digit_t       *b,
    __in_ecount( 1 )                const mp_modulus_t  *pmodulo );

DRM_API digit_t* DRM_CALL low_prime_prod_construction(
    __inout           struct bigctx_t   *f_pBigCtx,
    __out_ecount( 1 ) DRM_DWORD         *pclowprods );

DRM_API_VOID DRM_VOID DRM_CALL low_prime_prod_destruction(
    __in    digit_t         *lowprods,
    __inout struct bigctx_t *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL low_prime_divisibility(
    __in_ecount( lng )          const digit_t           *array,
    __in                        const DRM_DWORD          lng,
    __in_ecount( clowprods )    const digit_t           *lowprods,
    __in                        const DRM_DWORD          clowprods,
    __out_ecount( 1 )                 digit_t           *pdivisor,
    __inout                           struct bigctx_t   *f_pBigCtx );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL mod_exp(
    __in_ecount( modulo->length )   const digit_t           *base,
    __in_ecount( modulo->length )   const digit_t           *exponent,
    __in                            const DRM_DWORD          lngexpon,
    __out_ecount( modulo->length )        digit_t           *answer,
    __in_ecount( 1 )                const mp_modulus_t      *modulo,
    __inout                               struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL mod_mul(
    __in_ecount( pmodulo->length )                           const digit_t           *a,
    __in_ecount( pmodulo->length )                           const digit_t           *b,
    __out_ecount( pmodulo->length )                                digit_t           *c,
    __in_ecount( 1 )                                         const mp_modulus_t      *pmodulo,
    __inout_ecount_opt( pmodulo->modmul_algorithm_temps )          digit_t           *supplied_temps,
    __inout                                                        struct bigctx_t   *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL mod_shift(
    __in_ecount( pmodulo->length )  const digit_t       *a,
    __in                            const DRM_LONG       shiftamt,
    __out_ecount( pmodulo->length )       digit_t       *b,
    __in_ecount( 1 )                const mp_modulus_t  *pmodulo );

DRM_API DRM_BOOL DRM_CALL mod_sqrt(
    __in_ecount( modulo->length )    const digit_t           *base,
    __inout_ecount( modulo->length )       digit_t           *answer,
    __in_ecount( 1 )                 const mp_modulus_t      *modulo,
    __out_ecount( 1 )                      DRM_BOOL          *pperfect_square,
    __inout                                struct bigctx_t   *f_pBigCtx );

#define Allocate_Temporaries(typename, ptr, ctx) \
        Allocate_Temporaries_Multiple(1, typename, ptr, ctx)

#define Allocate_Temporaries_Multiple(nelmt, typename, ptr, ctx) \
               ptr = (typename*)bignum_alloc((nelmt)*sizeof(typename), f_pBigCtx)

#define Free_Temporaries( ptr, ctx ) bignum_free( ptr, ctx )

DRM_API DRM_BOOL DRM_CALL mp_gcdex(
    __in_ecount( lnga )                             const digit_t           *a,
    __in                                            const DRM_DWORD          lnga,
    __in_ecount( lngb )                             const digit_t           *b,
    __in                                            const DRM_DWORD          lngb,
    __out_ecount( lngb )                                  digit_t           *ailwmodb,
    __out_ecount_opt( lnga )                              digit_t           *bilwmoda,
    __out_ecount( DRM_MIN( lnga, lngb ) )                 digit_t           *gcd,
    __out_ecount_opt( lnga + lngb )                       digit_t           *lcm,
    __out_ecount( 1 )                                     DRM_DWORD         *plgcd,
    __inout_ecount_opt( MP_GCDEX_NTEMPS( lnga, lngb ) )   digit_t           *supplied_temps,
    __inout                                               struct bigctx_t   *f_pBigCtx );

DRM_EXTERN_INLINE DRM_DWORD DRM_CALL mp_gcdex_ntemps(
    __in const DRM_DWORD lnga,
    __in const DRM_DWORD lngb );
           /* Temporary count required by last argument to mp_gcdex */

DRM_API DRM_BOOL DRM_CALL mp_initialization(
    __inout struct bigctx_t    *f_pBigCtx );

DRM_EXTERN_INLINE DRM_DWORD DRM_CALL mp_ilwert_ntemps( __in const DRM_DWORD );
           /* Temporary count required by last argument to mp_ilwert */

DRM_API DRM_BOOL DRM_CALL mp_ilwert(
    __in_ecount( lng )                            const digit_t            *denom,
    __in_ecount( lng )                            const digit_t            *modulus,
    __in                                          const DRM_DWORD           lng,
    __out_ecount( lng )                                 digit_t            *result,
    __in_z_opt                                    const DRM_CHAR           *caller,
    __inout_ecount_opt( MP_ILWERT_NTEMPS( lng ) )       digit_t            *supplied_temps,
    __inout                                             struct bigctx_t    *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL mp_shift(
    __in_ecount( lng )    const digit_t  *a,
    __in                  const DRM_LONG  ishift,
    __inout_ecount( lng )       digit_t  *b,
    __in                  const DRM_DWORD lng );

DRM_API DRM_BOOL DRM_CALL mp_shift_lost(
    __in_ecount( lng )  const digit_t   *a,
    __in                const DRM_LONG   shift_amt,
    __out_ecount( lng )       digit_t   *b,
    __in                const DRM_DWORD  lng,
    __out_ecount( 1 )         digit_t   *plost );

DRM_API digit_t DRM_CALL multiply_immediate(
    __in_ecount( lng )  const digit_t    *a,
    __in                const digit_t     mult,
    __out_ecount( lng )       digit_t    *b,
    __in                const DRM_DWORD   lng );

DRM_API DRM_BOOL DRM_CALL neg_mod(
    __in_ecount( lng ) const digit_t   *a,
    __out_ecount( lng )      digit_t   *b,
    __in_ecount( lng ) const digit_t   *modulus,
    __in               const DRM_DWORD  lng );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL set_immediate(
    __out_ecount( lnga )       digit_t            *a,
    __in                 const digit_t             ivalue,
    __in                 const DRM_DWORD           lnga,
    __inout                    struct bigctx_t    *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL validate_modular_data(
    __in_ecount( lng ) const digit_t   *data,
    __in_ecount( lng ) const digit_t   *modulus,
    __in               const DRM_DWORD  lng );

#define MULTIPLY_DRMBIGNUM_IMPL() DRM_DO {                          \
    DRM_BOOL OK = TRUE;                                             \
    const digit_t *p1, *p2;                                         \
    DRM_DWORD i, lng1, lng2;                                        \
                                                                    \
    if (lnga > lngb) /* Put longer operand in p1 */                 \
    {                                                               \
        p1 = a; p2 = b; lng1 = lnga; lng2 = lngb;                   \
    }                                                               \
    else                                                            \
    {                                                               \
        p2 = a; p1 = b; lng2 = lnga; lng1 = lngb;                   \
    }                                                               \
                                                                    \
    if (!OK)                                                        \
    {                                                               \
        /* no-op */                                                 \
    }                                                               \
    else if (a == c || b == c)                                      \
    {                                                               \
        OK = FALSE;                                                 \
    }                                                               \
    else if (lng2 == 0)                                             \
    {       /* If an operand has length zero */                     \
        OEM_SELWRE_ZERO_MEMORY( c, (lng1)*sizeof( digit_t ) );      \
    }                                                               \
    else                                                            \
    {                                                               \
        c[lng1] = multiply_immediate( p1, p2[0], c, lng1 );         \
        for( i = 1; i != lng2; i++ )                                \
        {                                                           \
            c[i + lng1] = accumulate( p1, p2[i], &c[i], lng1 );     \
        }                                                           \
    }                                                               \
    return OK;                                                      \
} DRM_WHILE_FALSE

#if !DRM_INLINING_SUPPORTED

DRM_API DRM_BOOL DRM_CALL multiply(
    __in_ecount( lnga )             const digit_t   *a,
    __in                            const DRM_DWORD  lnga,
    __in_ecount( lngb )             const digit_t   *b,
    __in                            const DRM_DWORD  lngb,
    __inout_ecount( lnga + lngb )         digit_t   *c );

#else  /* !DRM_INLINING_SUPPORTED */

/****************************************************************************/

/*
** Multiply a (length lnga) times b (length lngb),
** getting a product c (length lnga + lngb).
** The output should not overlap the inputs.
*/
DRM_ALWAYS_INLINE DRM_BOOL DRM_CALL multiply(
    __in_ecount( lnga )             const digit_t   *a,
    __in                            const DRM_DWORD  lnga,
    __in_ecount( lngb )             const digit_t   *b,
    __in                            const DRM_DWORD  lngb,
    __inout_ecount( lnga + lngb )         digit_t   *c )
{
    MULTIPLY_DRMBIGNUM_IMPL();
} /* multiply */

#endif  /* !DRM_INLINING_SUPPORTED */

#if DRM_INLINING_SUPPORTED && defined(_M_IX86) && DRM_SUPPORT_ASSEMBLY
    #define significant_bit_count significant_bit_count_ix86
    #define UNIFORM_SIGNIFICANT_BIT_COUNT 1
    PRAGMA_WARNING_PUSH_WARN( 4035 )   /* No return value */
    static DRM_ALWAYS_INLINE DRM_DWORD significant_bit_count(const digit_t pattern)
    {
    _asm {
            mov  eax,pattern        ; Nonzero pattern
            bsr  eax,eax            ; eax = index of leftmost nonzero bit
            inc  eax                ; Add one to get significant bit count
         }
    }
    PRAGMA_WARNING_POP
#else /* DRM_INLINING_SUPPORTED && defined(_M_IX86) && DRM_SUPPORT_ASSEMBLY */
    #define UNIFORM_SIGNIFICANT_BIT_COUNT 0
           /* Algorithm faster for larger inputs.  See mpmisc.c */
#endif /* DRM_INLINING_SUPPORTED && defined(_M_IX86) && DRM_SUPPORT_ASSEMBLY */

DRM_API DRM_BOOL DRM_CALL sub_diff(
    __in_ecount( lnga ) const digit_t   *a,
    __in                const DRM_DWORD  lnga,
    __in_ecount( lngb ) const digit_t   *b,
    __in                const DRM_DWORD  lngb,
    __out_ecount( lnga )      digit_t   *c,
    __out_ecount_opt( 1 )     digit_t   *pborrow );

DRM_API digit_t DRM_CALL sub_immediate(
    __in_ecount( lng ) const digit_t   *a,
    __in               const digit_t    isub,
    __out_ecount( lng )      digit_t   *b,
    __in               const DRM_DWORD  lng );

DRM_API DRM_BOOL DRM_CALL sub_mod(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __out_ecount( lng )      digit_t   *c,
    __in_ecount( lng ) const digit_t   *modulus,
    __in               const DRM_DWORD  lng );

DRM_API digit_t DRM_CALL sub_same(
    __in_ecount( lng ) const digit_t   *a,
    __in_ecount( lng ) const digit_t   *b,
    __out_ecount( lng )      digit_t   *c,
    __in               const DRM_DWORD  lng );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL to_modular(
    __in_ecount( lnga )             const digit_t            *a,
    __in                            const DRM_DWORD           lnga,
    __out_ecount( pmodulo->length )       digit_t            *b,
    __in_ecount( 1 )                const mp_modulus_t       *pmodulo,
    __inout_opt                           struct bigctx_t    *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL two_adic_ilwerse(
    __in                const digit_t  d,
    __out_ecount( 1 )         digit_t *pdilw );

DRM_API_VOID DRM_VOID DRM_CALL uncreate_modulus(
    __inout mp_modulus_t    *pmodulo,
    __inout struct bigctx_t *f_pBigCtx );

DRM_API DRM_LONG DRM_CALL compare_immediate(
    __in_ecount( lng ) const digit_t     *a,
    __in               const digit_t      ivalue,
    __in               const DRM_DWORD    lng );

EXIT_PK_NAMESPACE;

#include <bigdecls.h>

#endif /* BIGNUM_H */
