/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_BIGNUMNOINLINE_C
#include "bignum.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if !DRM_INLINING_SUPPORTED

/******************************************************************************
**
** bigdefs.h
**
******************************************************************************/
#if defined(SEC_COMPILE)
/*
**  Compute the number of
**  significant bits in d (d != 0).
**  This is one more than the
**  truncated base 2 logarithm of d.
*/
DRM_API DRM_DWORD DRM_CALL significant_bit_count( __in const digit_t d )
{
    SIGNIFICANT_BIT_COUNT_DRMBIGNUM_IMPL();
}


/*
**  Count the number
**  of significant bits in a.
**  This is one more than the
**  truncated base 2 logarithm of a.
*/
DRM_API DRM_DWORD DRM_CALL mp_significant_bit_count(
    __in_ecount( lnga ) const digit_t   *a,
    __in                const DRM_DWORD  lnga )
{
    MP_SIGNIFICANT_BIT_COUNT_DRMBIGNUM_IMPL();
}
#endif
#ifdef NONE
DRM_API DRM_BOOL DRM_CALL digits_to_dwords(
    __in_ecount( lng_dwords )                                                    const digit_t  *pdigit,
    __inout_ecount_full( lng_dwords * sizeof( digit_t ) / DRM_DWORDS_PER_DIGIT )       DRM_BYTE *pbyte,
    __in                                                                         const DRM_DWORD lng_dwords )
{
    DIGITS_TO_DWORDS_DRMBIGNUM_IMPL();
}

DRM_API DRM_BOOL DRM_CALL dwords_to_digits(
    __in_ecount( lng_dwords )    const DRM_DWORD  *pdword,
    __inout_ecount( lng_dwords )       digit_t    *pdigit,
    __in                         const DRM_DWORD   lng_dwords )
{
    DWORDS_TO_DIGITS_DRMBIGNUM_IMPL();
}
#endif

#if defined(SEC_COMPILE)
DRM_API_VOID DRM_VOID DRM_CALL mp_extend(
    __in_ecount( lnga )    const digit_t  *a,
    __in                   const DRM_DWORD lnga,
    __inout_ecount( lngb )       digit_t  *b,
    __in                   const DRM_DWORD lngb )
{
    MP_EXTEND_DRMBIGNUM_IMPL();
}

DRM_API digit_t DRM_CALL mp_getbit(
    __in_ecount( ( ibit / DRM_RADIX_BITS ) + 1 ) const digit_t      *a,
    __in                                         const DRM_DWORD     ibit )
{
    MP_GETBIT_DRMBIGNUM_IMPL();
}

DRM_API_VOID DRM_VOID DRM_CALL mp_setbit(
    __inout_ecount( lnga )       digit_t     *a,
    __in                   const DRM_DWORD    lnga,
    __in                   const DRM_DWORD    ibit,
    __in                   const digit_t      new_value )
{
    MP_SETBIT_DRMBIGNUM_IMPL();
}

_Post_satisfies_(return <= lng) DRM_API DRM_DWORD DRM_CALL significant_digit_count(
    __in_ecount(lng) const digit_t  *a,
    __in             const DRM_DWORD lng )
{
    SIGNIFICANT_DIGIT_COUNT_DRMBIGNUM_IMPL();
}

/******************************************************************************
**
** bignum.h
**
******************************************************************************/

DRM_API DRM_BOOL DRM_CALL multiply(
    __in_ecount( lnga )             const digit_t   *a,
    __in                            const DRM_DWORD  lnga,
    __in_ecount( lngb )             const digit_t   *b,
    __in                            const DRM_DWORD  lngb,
    __inout_ecount( lnga + lngb )         digit_t   *c )
{
    MULTIPLY_DRMBIGNUM_IMPL();
}

#endif
#endif /* !DRM_INLINING_SUPPORTED */

EXIT_PK_NAMESPACE_CODE;

