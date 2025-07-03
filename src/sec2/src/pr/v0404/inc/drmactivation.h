/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRM_ACTIVATION__
#define __DRM_ACTIVATION__

#include <drmtypes.h>

ENTER_PK_NAMESPACE;

DRM_API DRM_BOOL DRM_CALL DRM_ACTIVATION_IsActivationSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_ACTIVATION_IsActivationUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_ACTIVATION_GetReeFeatureXml( DRM_VOID );


EXIT_PK_NAMESPACE;

#endif /* __DRM_ACTIVATION__ */

