/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#ifndef OEMPROFILE_H
#define OEMPROFILE_H

#include <drmprofiletypes.h>

ENTER_PK_NAMESPACE;

#if DRM_SUPPORT_PROFILING

DRM_API PERF_GLOBAL_CONTEXT * DRM_CALL Oem_Profiling_GetProfileContext( DRM_VOID );

#endif /* DRM_SUPPORT_PROFILING */

EXIT_PK_NAMESPACE;

#endif // OEMPROFILE_H

