/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMSELWRETIMETYPES_H__
#define __DRMSELWRETIMETYPES_H__

ENTER_PK_NAMESPACE;

typedef enum _DRM_SELWRETIME_CLOCK_TYPE
{
    DRM_SELWRETIME_CLOCK_TYPE_ILWALID = 0,
    DRM_SELWRETIME_CLOCK_TYPE_TEE     = 1,    /* SelwreTime - TEE clock           */
    DRM_SELWRETIME_CLOCK_TYPE_UM      = 2,    /* SelwreTime - User-mode clock     */
} DRM_SELWRETIME_CLOCK_TYPE;

EXIT_PK_NAMESPACE;

#endif /*__DRMSELWRETIMETYPES_H__ */
