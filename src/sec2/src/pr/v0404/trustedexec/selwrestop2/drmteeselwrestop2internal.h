/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _DRMTEESELWRESTOP2INTERNAL_H_
#define _DRMTEESELWRESTOP2INTERNAL_H_ 1

#include <drmteetypes.h>
#include <oemteetypes.h>
#include <drmteebase.h>

ENTER_PK_NAMESPACE;

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_SELWRESTOP2_InitializeNotThreadSafe( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_SELWRESTOP2_StartDecryption(
    __inout       OEM_TEE_CONTEXT       *f_pOemTeeContext,
    __in    const DRM_ID                *f_pLID,
    __in    const DRM_ID                *f_pidNonceSelwreStopSession );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_SELWRESTOP2_VerifyDecryptionNotStopped(
    __inout       OEM_TEE_CONTEXT       *f_pOemTeeContext,
    __in    const DRM_TEE_POLICY_INFO   *f_pPolicy );

EXIT_PK_NAMESPACE;

#endif /* _DRMTEESELWRESTOP2INTERNAL_H_ */

