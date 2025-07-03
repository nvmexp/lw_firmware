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
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_sqrter
        (const digit_t *f1,        // IN
         digit_t       *f3,        // OUT
         const DRM_DWORD      nelmt,     // IN
         const field_desc_t *fdesc,     // IN
         DRM_BOOL      *psquares,  // OUT
         struct bigctx_t *f_pBigCtx)
{
    DRM_BOOL OK = TRUE;
    DRM_DWORD i;
    DRM_DWORD elng = fdesc->elng;
    DRM_BOOL all_squares = TRUE;

    for (i = 0; OK && i != nelmt*elng; i += elng) {
        DRM_BOOL square_now = FALSE;
        OK = OK && mod_sqrt(f1 + i, f3 + i,
                            fdesc->modulo, &square_now, f_pBigCtx);
        all_squares = all_squares && square_now;
    }
    if (OK) *psquares = all_squares;
    return OK;
}  // Kprime_sqrter
#endif
EXIT_PK_NAMESPACE_CODE;

