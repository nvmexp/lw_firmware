/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMKEYFILECERTTYPES_H
#define DRMKEYFILECERTTYPES_H

#include <oemcommon.h>

ENTER_PK_NAMESPACE;

typedef enum DRM_KF_CERT_TYPE
{
    eKF_CERT_TYPE_ILWALID,
    eKF_CERT_TYPE_NDT,
    eKF_CERT_TYPE_WMDRM,
    eKF_CERT_TYPE_PLAYREADY,
} DRM_KF_CERT_TYPE;

EXIT_PK_NAMESPACE;

#endif // DRMKEYFILECERTTYPES_H

