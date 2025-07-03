/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _DRMTEELICPREPINTERNAL_H_
#define _DRMTEELICPREPINTERNAL_H_ 1

#include <drmteetypes.h>
#include <oemteetypes.h>
#include <drmxmrformat_generated.h>

ENTER_PK_NAMESPACE;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_LICPREP_DecryptScalableLicenseKeyFromKeyMaterial(
    __inout                     DRM_TEE_CONTEXT                         *f_pContext,
    __in                  const DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER    *f_pXmrContainerKeyLeaf,
    __in                  const DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER    *f_pXmrContainerKeyRoot,
    __in_ecount( 2 )      const DRM_TEE_KEY                             *f_pEscrowedKeys,
    __out_ecount( 2 )           DRM_TEE_KEY                             *f_pLeafKeys );

EXIT_PK_NAMESPACE;

#endif /* _DRMTEELICPREPINTERNAL_H_ */

