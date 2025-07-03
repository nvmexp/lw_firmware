/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _DRMTEEXB_H_
#define _DRMTEEXB_H_ 1

#include <drmteetypes.h>
#include <drmteebase.h>
#include <drmxbbuilder.h>
#include <drmteexbformat_generated.h>
#include <drmteekbcryptodata.h>
#include <oemteetypes.h>

ENTER_PK_NAMESPACE;

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_KB_Build(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eKBType,
    __in                                                  DRM_DWORD                   f_cbKBCryptoData,
    __in_bcount( f_cbKBCryptoData )                 const DRM_BYTE                   *f_pbKBCryptoData,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pKB )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_KB_ParseAndVerifyWithReconstitution(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pKB,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType,
    __in_opt                                        const DRM_BOOL                   *f_pfReconstituted,
    __out                                                 DRM_DWORD                  *f_pcbCryptoDataWeakRef,
    __deref_out_bcount( *f_pcbCryptoDataWeakRef )         DRM_BYTE                  **f_ppbCryptoDataWeakRef )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_KB_ParseAndVerify(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pKB,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType,
    __out                                                 DRM_DWORD                  *f_pcbCryptoDataWeakRef,
    __deref_out_bcount( *f_pcbCryptoDataWeakRef )         DRM_BYTE                  **f_ppbCryptoDataWeakRef );

EXIT_PK_NAMESPACE;

#endif /* _DRMTEEXB_H_ */

