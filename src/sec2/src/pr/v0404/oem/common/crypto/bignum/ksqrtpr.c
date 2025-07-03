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
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kprime_sqrter(
    __in_ecount( nelmt * fdesc->elng )  const digit_t           *f1,
    __out_ecount( nelmt * fdesc->elng )       digit_t           *f3,
    __in                                const DRM_DWORD          nelmt,
    __in_ecount( 1 )                    const field_desc_t      *fdesc,
    __out_ecount( 1 )                         DRM_BOOL          *psquares,
    __inout                                   struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    DRM_DWORD i;
    DRM_DWORD elng = fdesc->elng;
    DRM_BOOL all_squares = TRUE;
    *psquares = FALSE;

    OK = OK && ( fdesc->modulo->length <= elng );     /* Buffer overflow check */
    for( i = 0; OK && i != nelmt * elng; i += elng )
    {
        DRM_BOOL square_now = FALSE;
        OK = OK && mod_sqrt( f1 + i, f3 + i, fdesc->modulo, &square_now, f_pBigCtx );
        all_squares = all_squares && square_now;
    }
    if( OK )
    {
        *psquares = all_squares;
    }
    return OK;
}
#endif

EXIT_PK_NAMESPACE_CODE;

