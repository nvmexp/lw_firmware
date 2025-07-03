/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmerr.h>
#include <drmnonceverify.h>
#include <drmbytemanip.h>
#include <drmutilitieslite.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)
DRM_API DRM_RESULT DRM_CALL DRM_NONCE_VerifyNonce(
    __in const DRM_ID                 *f_poNonce,
    __in const DRM_LID                *f_poLID,
    __in       DRM_DWORD               f_cLicenses )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_poNonce   != NULL );
    ChkArg( f_poLID     != NULL );
    ChkArg( f_cLicenses != 0 );

    /* Check whether the passed in LID matches the internal nonce. */
    if( MEMCMP( f_poLID, f_poNonce, sizeof( DRM_ID ) ) == 0 )
    {
        /* Exact match. */
        dr = DRM_SUCCESS;
    }
    else
    {
        /* Not an exact match. Look for a relaxed match. */
        DRM_DWORD idx           = 0;
        DRM_ID    oRelaxedNonce = *f_poNonce;

        /* Fail unless we find a relaxed match. */
        dr = DRM_E_NONCE_STORE_TOKEN_NOT_FOUND;

        /*
        ** Relaxed match algorithm:
        ** Treat LID as a big-endian integer.
        ** For each big-endian integer in this set:
        **      LID+1, LID+2, ... LID+(f_cLicenses-1)
        ** If the nonce matches that big-endian integer, then it's a successful relaxed match.
        ** Otherwise, no match is found and we return DRM_E_NONCE_STORE_TOKEN_NOT_FOUND.
        ** If any value LID+x > MAX_LID (i.e. all bytes 0xFF), then the set becomes:
        **      LID+1, LID+2, ... LID+x == MAX_LID, MIN_LID, MIN_LID+1, ..., MIN_LID+(f_cLicenses-(x+1))
        **      (i.e. it "wraps" from MAX_LID (i.e. all bytes 0xFF) to MIN_LID (i.e. all bytes 0x00)
        ** Note: If f_cLicenses == 1, then only an exact match is possible!
        */

        /* For each possible relaxed match... */
        for( idx = 1; idx < f_cLicenses && DRM_FAILED( dr ); idx++ )
        {
            DRM_UTL_IncrementID( &oRelaxedNonce, FALSE );  /* Passing FALSE: wraps from all bytes 0xFF to 0x00 */

            if( MEMCMP( f_poLID, &oRelaxedNonce, sizeof( DRM_ID ) ) == 0 )
            {
                /* Found a relaxed match. */
                dr = DRM_SUCCESS;
            }

            /*
            ** If we didn't find a match, the oRelaxedNonce value intentionally doesn't get reset.
            ** We carry over the change to its value to the next time through the loop.
            */
        }
    }

    /* If we found neither an exact match nor a relaxed match, this will fail with DRM_E_NONCE_STORE_TOKEN_NOT_FOUND. */
    ChkDR( dr );

ErrorExit:
    DRMASSERT( ( dr == DRM_SUCCESS ) || ( dr == DRM_E_NONCE_STORE_TOKEN_NOT_FOUND ) );
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;
