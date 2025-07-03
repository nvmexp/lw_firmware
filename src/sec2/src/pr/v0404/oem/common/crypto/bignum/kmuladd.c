/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_KMULADD_C
#include "bignum.h"
#include "fieldpriv.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kmuladd(
    __in_ecount( fdesc->elng )                               const digit_t          *f1,
    __in_ecount( fdesc->elng )                               const digit_t          *f2,
    __in_ecount( fdesc->elng )                               const digit_t          *f3,
    __out_ecount( fdesc->elng )                                    digit_t          *f4,
    __in_ecount( 1 )                                         const field_desc_t     *fdesc,
    __inout_ecount_opt( fdesc->elng + fdesc->ndigtemps_mul )       digit_t          *supplied_temps,
    __inout                                                        struct bigctx_t  *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    const DRM_DWORD elng = fdesc->elng;
    digit_tempinfo_t tempinfo;

    tempinfo.address = supplied_temps;
    tempinfo.nelmt = elng + fdesc->ndigtemps_mul;
    tempinfo.need_to_free = FALSE;

    OK = OK && possible_digit_allocate( &tempinfo, f_pBigCtx );

    if( OK )
    {
        digit_t *prod = tempinfo.address;
        digit_t *ftemps = prod + elng;

        OK = OK && Kmul( f1, f2, prod, fdesc, ftemps, f_pBigCtx );
        OK = OK && Kadd( prod, f3, f4, fdesc, f_pBigCtx );
    }
    if( tempinfo.need_to_free )
    {
        Free_Temporaries( tempinfo.address, f_pBigCtx );
    }
    return OK;
}
#endif
EXIT_PK_NAMESPACE_CODE;

