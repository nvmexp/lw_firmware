/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmteesupported.h>
#include <drmteesecurestop2internal.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_SECURESTOP2_IsSECURESTOP2Supported( DRM_VOID )
{
    return FALSE;
}

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_SECURESTOP2_InitializeNotThreadSafe( DRM_VOID )
{
    /* Nothing to initialize if feature is not supported */
    return;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_SECURESTOP2_StartDecryption(
    __inout       OEM_TEE_CONTEXT       *f_pOemTeeContext,
    __in    const DRM_ID                *f_pLID,
    __in    const DRM_ID                *f_pidNonceSecureStopSession )
{
    /* Decryption is always enabled if feature is not supported */
    UNREFERENCED_PARAMETER( f_pOemTeeContext );
    UNREFERENCED_PARAMETER( f_pLID );
    UNREFERENCED_PARAMETER( f_pidNonceSecureStopSession );
    return DRM_SUCCESS;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_SECURESTOP2_VerifyDecryptionNotStopped(
    __inout       OEM_TEE_CONTEXT       *f_pOemTeeContext,
    __in    const DRM_TEE_POLICY_INFO   *f_pPolicy )
{
    /* Decryption is always enabled if feature is not supported */
    UNREFERENCED_PARAMETER( f_pOemTeeContext );
    UNREFERENCED_PARAMETER( f_pPolicy );
    return DRM_SUCCESS;
}
#endif
EXIT_PK_NAMESPACE_CODE;

