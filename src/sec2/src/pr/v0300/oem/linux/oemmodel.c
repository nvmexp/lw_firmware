/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_OEMMODEL_C
#include <drmerr.h>
#include <oemcommon.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
/*******************************************************************
**
**  Function:   Oem_Device_GetModelInfo
**
**  Synopsis:   Get the Model Info string for the Device Cert.
**
**  Arguments:  [f_pOEMContext]   -- OEM implemented context information
**              [f_pwchModelInfo] -- DRM_WCHAR pointer to hold the Model Info.
**              [f_pcchModelInfo] -- DRM_DWORD pointer to specify the size in characters
**                                   of f_pwchModelInfo.
**
**  Notes:  This string will be included in the Device Cert
**          (for WMDRM-PD and PlayReady certs).
**          f_pcchModelInfo dose not count null terminator, and function callers should NOT
**          assume that the string is null terminated.
**
**  Returns: DRM_E_BUFFERTOOSMALL   - If pdstrModelInfo->pwszString is too small to hold the string.
**           DRM_E_ILWALIDARG       - If pdstrModelInfo or pdstrModelInfo->pwszString is NULL.
**           DRM_E_NOTIMPL          - If the model info is not supported for inclusion in the device cert on the platform
**           DRM_SUCCESS            - Otherwise.
**
*******************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Device_GetModelInfo(
    __in_opt DRM_VOID   *f_pOEMContext,
    __out_ecount_opt(*f_pcchModelInfo) DRM_WCHAR *f_pwchModelInfo,
    __inout DRM_DWORD *f_pcchModelInfo )
{
    return DRM_E_NOTIMPL;
}
#endif

EXIT_PK_NAMESPACE_CODE;
