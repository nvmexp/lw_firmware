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
DRM_GLOBAL_CONST DRM_WCHAR g_rgwchReqTagPlayReadyClientVersionData[] = { DRM_ONE_WCHAR('4', '\0'), DRM_ONE_WCHAR('.', '\0'), DRM_ONE_WCHAR('4', '\0'), DRM_ONE_WCHAR('.', '\0'), DRM_ONE_WCHAR('0', '\0'), DRM_ONE_WCHAR('.', '\0'), DRM_ONE_WCHAR('6', '\0'), DRM_ONE_WCHAR('2', '\0'), DRM_ONE_WCHAR('1', '\0'), DRM_ONE_WCHAR('1', '\0'), DRM_ONE_WCHAR('\0', '\0') };
DRM_GLOBAL_CONST DRM_CONST_STRING g_dstrReqTagPlayReadyClientVersionData = DRM_CREATE_DRM_STRING( g_rgwchReqTagPlayReadyClientVersionData );
#endif

#if defined (SEC_COMPILE)
DRM_GLOBAL_CONST DRM_DWORD g_dwReqTagPlayReadyClientVersionData[DRM_VERSION_LEN] PR_ATTR_DATA_OVLY(_g_dwReqTagPlayReadyClientVersionData) = { 4, 4, 0, 6211 };
#endif

EXIT_PK_NAMESPACE_CODE;
