/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMNONCEVERIFY_H
#define DRMNONCEVERIFY_H 1

#include <drmtypes.h>

ENTER_PK_NAMESPACE;

DRM_API DRM_RESULT DRM_CALL DRM_NONCE_VerifyNonce(
    __in const DRM_ID                 *f_poNonce,
    __in const DRM_LID                *f_poLID,
    __in       DRM_DWORD               f_cLicenses );

EXIT_PK_NAMESPACE;

#endif // DRMNONCEVERIFY_H
