/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMREVOCATIONENUM_H
#define DRMREVOCATIONENUM_H

#include <drmtypes.h>

ENTER_PK_NAMESPACE;

typedef enum _DRM_REVOCATION_TYPE_ENUM
{
    DRM_REVOCATION_TYPE_WMDRM_ND,
    DRM_REVOCATION_TYPE_WMDRM_REVINFO,
    DRM_REVOCATION_TYPE_PLAYREADY_APP,
    DRM_REVOCATION_TYPE_PLAYREADY_RUNTIME,
    DRM_REVOCATION_TYPE_PLAYREADY_REVINFO2
} DRM_REVOCATION_TYPE_ENUM;

EXIT_PK_NAMESPACE;

#endif // DRMREVOCATIONENUM_H
