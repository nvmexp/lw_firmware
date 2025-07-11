/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMRIVCRLPARSER_H__
#define __DRMRIVCRLPARSER_H__

#include <drmbcrltypes.h>

ENTER_PK_NAMESPACE;

#define SELWRE_COPY_FROMBUFFER(to, from, index, size, buffersize) DRM_DO {  \
    DRM_DWORD __dwSpaceRequired=0;                                          \
    ChkDR(DRM_DWordAdd(index,size,&__dwSpaceRequired));                     \
    ChkBOOL(__dwSpaceRequired<=(buffersize),DRM_E_BUFFERTOOSMALL);          \
    OEM_SELWRE_MEMCPY((DRM_BYTE*)(to),&(from[(index)]),(size));             \
    (index)=(__dwSpaceRequired);                                            \
} DRM_WHILE_FALSE

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_RVK_FindEntryInRevInfo(
    __in                     const DRM_RLVI    *f_prlvi,
    __in_bcount(f_cbRevInfo) const DRM_BYTE    *f_pbRevInfo,
    __in                           DRM_DWORD    f_cbRevInfo,
    __in                     const DRM_GUID    *f_pguidEntry,
    __out                          DRM_DWORD   *f_pdwVersion );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_RVK_ParseRevocationInfoHeader(
    __in_bcount( f_cbRevInfo ) const DRM_BYTE           *f_pbRevInfo,
    __in                             DRM_DWORD           f_cbRevInfo,
    __out                            DRM_RLVI           *f_pRLVI,
    __inout                          DRM_DWORD          *f_pidxRevInfo );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_RVK_ParseRevocationInfo(
    __in_bcount( f_cbRevInfo ) const DRM_BYTE           *f_pbRevInfo,
    __in                             DRM_DWORD           f_cbRevInfo,
    __out_ecount(1)                  DRM_RLVI           *f_pRLVI,
    __out_ecount_opt(1)              DRM_DWORD          *f_pcbSignedBytes );

DRM_API DRM_RESULT DRM_CALL DRM_RVK_VerifyRevocationInfoV2Only(
    __inout_opt                      DRM_VOID               *f_pvOemCtx,
    __inout_opt                      DRM_CRYPTO_CONTEXT     *f_pCryptContext,
    __in_bcount( f_cbRevInfo ) const DRM_BYTE               *f_pbRevInfo,
    __in                             DRM_DWORD               f_cbRevInfo,
    __out                            DRM_RLVI               *f_pRLVI );

/*********************************************************************
**
**  Parses the unsigned portion of Playready/Silverlight CRL from
**  binary to data structure.
**
**  NOTE: The parser does not make copies of the DRM_RevocationEntry
**        data, it just points to them in the f_pbCrlData buffer so
**        you cannot free the f_pbCrlData and still have a valid f_poCrl
**        data structure.
**
*********************************************************************/

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCrl_ParseUnsignedCrl(
    __in_bcount(f_cbCrlData) const DRM_BYTE       *f_pbCrlData,
    __in                     const DRM_DWORD       f_cbCrlData,
    __out                    DRM_DWORD            *f_pcbSignedMessageLength,
    __out                    DRM_BCRL             *f_poCrl );

/*********************************************************************
**
**  Parses the entire Playready/Silverlight CRL from
**  binary to data structure.
**
**  NOTE: The parser does not make copies of the DRM_RevocationEntry
**        data, it just points to them in the f_pbCrlData buffer so
**        you cannot free the f_pbCrlData and still have a valid f_poCrl
**        data structure.
**
*********************************************************************/

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCrl_ParseSignedCrl(
    __in_bcount(f_cbCrlData) const DRM_BYTE        *f_pbCrlData,
    __in                     const DRM_DWORD        f_cbCrlData,
    __out_ecount_opt(1)            DRM_DWORD       *f_pcbSignedMessageLength,
    __out                          DRM_BCRL_Signed *f_poCrl );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCrl_VerifySignature(
    __inout_opt                             DRM_VOID               *f_pOemTeeContextAllowNULL,
    __inout_opt                             DRM_CRYPTO_CONTEXT     *f_pCryptoContext,
    __in_bcount(f_cbSignedBytes)      const DRM_BYTE               *f_pbSignedBytes,
    __in                                    DRM_DWORD               f_cbSignedBytes,
    __in_bcount(f_cbSignature)        const DRM_BYTE               *f_pbSignature,
    __in                                    DRM_DWORD               f_cbSignature,
    __in_bcount(f_cbCertificateChain) const DRM_BYTE               *f_pbCertificateChain,
    __in                                    DRM_DWORD               f_cbCertificateChain,
    __in_ecount_opt(1)                const PUBKEY_P256            *f_pRootPubkey );

EXIT_PK_NAMESPACE;

#endif /* __DRMRIVCRLPARSER_H__ */

