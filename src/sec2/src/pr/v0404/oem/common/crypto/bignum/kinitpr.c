/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_KINITPR_C
#include "bignum.h"
#include "fieldpriv.h"
#include "mprand.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)
DRM_API DRM_NO_INLINE DRM_BOOL DRM_CALL Kprime_adder(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f2,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;

    if( OK )
    {
        DRM_DWORD i;
        const DRM_DWORD elng = fdesc->elng;

        OK = OK && ( fdesc->modulo->length <= elng );     /* Buffer overflow check */
        for( i = 0; OK && i != nelmt * elng; i += elng )
        {
            OK = OK && add_mod( f1 + i, f2 + i, f3 + i, fdesc->modulo->modulus, fdesc->modulo->length );
        }
    }
    return OK;
}
#endif

#ifdef NONE
DRM_API DRM_NO_INLINE DRM_BOOL DRM_CALL Kprime_freer(
    __inout_ecount( 1 ) field_desc_t    *fdesc,
    __inout             struct bigctx_t *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;

    if( OK
     && fdesc->free_modulus
     && fdesc->modulo != NULL )
    {
        uncreate_modulus( (mp_modulus_t*)fdesc->modulo, f_pBigCtx );
        Free_Temporaries( (mp_modulus_t*)fdesc->modulo, f_pBigCtx );
        fdesc->free_modulus = FALSE;
        fdesc->modulo = NULL;
    }
    return OK;
}

/*
**  Colwert a signed digit_t value
**  (usually a 32-bit signed integer)
**  to a field element.
**
**  N.B.  fdesc->one is available to colwert a 1.
*/
DRM_API DRM_NO_INLINE DRM_BOOL DRM_CALL Kprime_immediater(
    __in_ecount( nelmt )                const sdigit_t          *scalars,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    DRM_DWORD i;
    const DRM_DWORD elng = fdesc->elng;

    OK = OK && ( fdesc->modulo->length <= elng );     /* Buffer overflow check */
    for( i = 0; OK && i != nelmt; i++ )
    {
        const sdigit_t scalar = scalars[ i ];
        const digit_t abssc = (digit_t)( scalar >= 0 ? scalar : -scalar );
        digit_t* f3addr = f3 + i * elng;

        OK = OK && to_modular( &abssc, 1, f3addr, fdesc->modulo, f_pBigCtx );
        if( scalar < 0 )
        {
            OK = OK && neg_mod( f3addr, f3addr, fdesc->modulo->modulus, fdesc->modulo->length );
        }
    }
    return OK;
}
#endif

#if defined(SEC_COMPILE)
/*
**  Ilwert one field element f3 = 1/f1.
**  f1 is guaranteed nonzero.
*/
DRM_API DRM_NO_INLINE DRM_BOOL DRM_CALL Kprime_ilwerter1(
    __in_ecount( fdesc->elng )  const digit_t           *f1,
    __out_ecount( fdesc->elng )       digit_t           *f3,
    __in_ecount( 1 )            const field_desc_t      *fdesc,
    __in_ecount( 1 )            const digit_tempinfo_t  *tempinfo,
    __inout                           struct bigctx_t   *f_pBigCtx )
{
    const DRM_DWORD elng = fdesc->elng;
    digit_t *ftemp = tempinfo->address;     /* Length elng */
    digit_t *ftemps = ftemp + elng;         /* Length DRM_MAX(fdesc->ndigtemps_mul, mp_ilwert_ntemps(elng)) */
    const digit_t *finp = f1;               /* Tentative input location */
    const mp_modulus_t *modulo = fdesc->modulo;
    DRM_BOOL OK = TRUE;

    DRMASSERT( tempinfo->address + tempinfo->nelmt
            == ftemps + DRM_MAX( fdesc->ndigtemps_mul, mp_ilwert_ntemps( elng ) ) );

    if( OK && modulo->reddir == FROM_RIGHT )
    {
        OK = OK && Kmul( finp, fdesc->ilwerse_adjustment, ftemp, fdesc, ftemps, f_pBigCtx );
        finp = ftemp;
    }
    OK = OK && mp_ilwert( finp, modulo->modulus, elng, f3, "", ftemps, f_pBigCtx );
    return OK;
}
#endif

#ifdef NONE
/*
**  Multiply field elements in f1 by 2^ishift,
**  store results in f3.
*/
DRM_API DRM_NO_INLINE DRM_BOOL DRM_CALL Kprime_mulpower2er(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __in                                const DRM_LONG           ishift,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    const DRM_DWORD elng = fdesc->elng;
    DRM_DWORD i;

    OK = OK && ( fdesc->modulo->length <= elng );     /* Buffer overflow check */
    for( i = 0; i != nelmt * elng; i += elng )
    {
        OK = OK && mod_shift( f1 + i, ishift, f3 + i, fdesc->modulo );
    }
    return OK;
}
#endif

#if defined(SEC_COMPILE)
DRM_API DRM_NO_INLINE DRM_BOOL DRM_CALL Kprime_multiplier1(
    __in_ecount( fdesc->modulo->length )                        const digit_t           *f1,
    __in_ecount( fdesc->modulo->length )                        const digit_t           *f2,
    __out_ecount( fdesc->modulo->length )                             digit_t           *f3,
    __in_ecount( 1 )                                            const field_desc_t      *fdesc,
    __inout_ecount_opt( fdesc->modulo->modmul_algorithm_temps )       digit_t           *temps,
    __inout                                                           struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;

    OK = OK && mod_mul( f1, f2, f3, fdesc->modulo, temps, f_pBigCtx );
    return OK;
}


DRM_API DRM_NO_INLINE DRM_BOOL DRM_CALL Kprime_negater(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;

    if( OK )
    {
        DRM_DWORD i;
        const DRM_DWORD elng = fdesc->elng;

        OK = OK && ( fdesc->modulo->length <= elng );     /* Buffer overflow check */
        for( i = 0; OK && i != nelmt * elng; i += elng )
        {
            OK = OK && neg_mod( f1 + i, f3 + i, fdesc->modulo->modulus, fdesc->modulo->length );
        }
    }
    return OK;
}
#endif


#ifdef NONE
/*
**  Return field size (length elng+1).
**  For prime fields, this is the same as its characteristic.
*/
DRM_API DRM_NO_INLINE DRM_BOOL DRM_CALL Kprime_sizer(
    __out_ecount( fdesc->elng + 1 )           digit_t           *size,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx )
{
    const DRM_DWORD elng = fdesc->elng;
    DRM_BOOL OK = TRUE;

    DRMASSERT( elng == fdesc->modulo->length );

    mp_extend( fdesc->modulo->modulus, elng, size, elng + 1 );

    return OK;
}
#endif

#if defined(SEC_COMPILE)
DRM_API DRM_NO_INLINE DRM_BOOL DRM_CALL Kprime_subtracter(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f2,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __inout                                   struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    DRM_DWORD i;
    const DRM_DWORD elng = fdesc->elng;

    OK = OK && ( fdesc->modulo->length <= elng );     /* Buffer overflow check */
    for( i = 0; OK && i != nelmt * elng; i += elng )
    {
        OK = OK && sub_mod( f1 + i, f2 + i, f3 + i, fdesc->modulo->modulus, fdesc->modulo->length );
    }
    return OK;
}
#endif

#ifdef NONE
/*
**  Initialize a field descriptor for a prime field.
**
**  CAUTION -- The modulo struct must remain accessible
**  (i.e., in scope) as long as the field is being accessed.
**  That is, don't use an automatic variable in a function
**  for *modulo if the field will be accessed after exiting the function.
**
**  CAUTION -- modulo->modulus should be prime, but we don't check that.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kinitialize_prime(
    __in_ecount( 1 )  const mp_modulus_t    *modulo,
    __out_ecount( 1 )       field_desc_t    *fdesc,
    __inout                 struct bigctx_t *pbigctxTemp,
    __inout                 struct bigctx_t *f_pBigCtx )
{
    const DRM_DWORD elng = modulo->length;
    const DRM_DWORD nilw_temps = mp_ilwert_ntemps( elng );
    DRM_BOOL OK = TRUE;

    fdesc->deallocate = NULL;
    OK = OK && Kfdesc_initialize( fdesc, 0, pbigctxTemp );

    fdesc->degree = 1;
    fdesc->elng = elng;
    fdesc->ftype = FIELD_Q_MP;
    fdesc->modulo = modulo;
    fdesc->ndigtemps_mul = modulo->modmul_algorithm_temps;
    fdesc->ndigtemps_ilwert1 = elng + DRM_MAX( fdesc->ndigtemps_mul, nilw_temps );
    fdesc->ndigtemps_arith = fdesc->ndigtemps_ilwert1 + elng;
    fdesc->one = (digit_t*)modulo->one;

    /*
    ** Set up multiplier for Kilwert.
    ** This is 1 for FROM_LEFT arithmetic.
    ** For FROM_RIGHT arithmetic, we twice unscale the
    ** constant fdesc->one.
    */
    if( !OK )
    {
    }
    else if( modulo->reddir == FROM_LEFT )
    {
        fdesc->ilwerse_adjustment = fdesc->one;
    }
    else
    {
        digit_t *ilwadj = digit_allocate( elng, f_pBigCtx );
        if( ilwadj == NULL )
        {
            OK = FALSE;
        }
        fdesc->deallocate = ilwadj;
        fdesc->ilwerse_adjustment = ilwadj;
        OK = OK && mod_shift( fdesc->one, -2 * modulo->scaling_power2, ilwadj, modulo );
        if( !OK && ilwadj != NULL )
        {
            Free_Temporaries( ilwadj, f_pBigCtx );
        }
    }
    return OK;
}
#endif

EXIT_PK_NAMESPACE_CODE;

