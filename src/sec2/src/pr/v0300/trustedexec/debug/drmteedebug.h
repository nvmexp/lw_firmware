/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMTEEBASEDEBUG_H
#define DRMTEEBASEDEBUG_H 1

#include <drmteetypes.h>

ENTER_PK_NAMESPACE;

#if DRM_DBG

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_IMPL_DEBUG_Init( DRM_VOID ) DRM_NO_INLINE_ATTRIBUTE;
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_IMPL_DEBUG_DeInit( DRM_VOID ) DRM_NO_INLINE_ATTRIBUTE;

#else   /* DRM_DBG */

#define DRM_TEE_BASE_IMPL_DEBUG_Init()
#define DRM_TEE_BASE_IMPL_DEBUG_DeInit()

#endif /* DRM_DBG */

EXIT_PK_NAMESPACE;

#endif // DRMTEEBASEDEBUG_H

