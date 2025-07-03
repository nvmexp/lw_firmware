/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
/*
       File kmuladd.c. Version  10 April 2002
*/
#define DRM_BUILDING_KMULADD_C
#include "bignum.h"
#include "fieldpriv.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kmuladd
        (const digit_t *f1,     // Set f4 = f1 * f2 + f3
         const digit_t *f2,
         const digit_t *f3,
         digit_t  *f4,
    const field_desc_t *fdesc,
         digit_t  *supplied_temps,
         struct bigctx_t *f_pBigCtx)
{
    DRM_BOOL OK = TRUE;
    const DRM_DWORD elng = fdesc->elng;
    digit_tempinfo_t tempinfo;

    tempinfo.address = supplied_temps;
    tempinfo.nelmt = elng + fdesc->ndigtemps_mul;
    tempinfo.need_to_free = FALSE;

    OK = OK && possible_digit_allocate(&tempinfo, f_pBigCtx);

    if (OK) {
        digit_t *prod = tempinfo.address;
        digit_t *ftemps = prod + elng;

        OK = OK && Kmul(f1, f2, prod, fdesc, ftemps, f_pBigCtx);
        OK = OK && Kadd(prod, f3, f4, fdesc, f_pBigCtx);
    }
    if (tempinfo.need_to_free) {
        Free_Temporaries(tempinfo.address, f_pBigCtx);
    }
    return OK;
} /* Kmuladd */
#endif

EXIT_PK_NAMESPACE_CODE;
