/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmversionconstants.h>
#include <drmbytemanip.h>

ENTER_PK_NAMESPACE_CODE;

#ifdef NONE
DRM_GLOBAL_CONST DRM_WCHAR g_rgwchReqTagPlayReadyClientVersionData[] = { DRM_ONE_WCHAR('3', '\0'), DRM_ONE_WCHAR('.', '\0'), DRM_ONE_WCHAR('0', '\0'), DRM_ONE_WCHAR('.', '\0'), DRM_ONE_WCHAR('0', '\0'), DRM_ONE_WCHAR('.', '\0'), DRM_ONE_WCHAR('2', '\0'), DRM_ONE_WCHAR('7', '\0'), DRM_ONE_WCHAR('5', '\0'), DRM_ONE_WCHAR('5', '\0'), DRM_ONE_WCHAR('\0', '\0') };
DRM_GLOBAL_CONST DRM_CONST_STRING g_dstrReqTagPlayReadyClientVersionData = DRM_CREATE_DRM_STRING( g_rgwchReqTagPlayReadyClientVersionData );
#endif

DRM_GLOBAL_CONST DRM_DWORD g_dwReqTagPlayReadyClientVersionData[DRM_VERSION_LEN] PR_ATTR_DATA_OVLY(_g_dwReqTagPlayReadyClientVersionData) = { 3, 0, 0, 2755 };


EXIT_PK_NAMESPACE_CODE;
