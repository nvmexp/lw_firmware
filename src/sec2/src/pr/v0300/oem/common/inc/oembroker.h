/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef OEMBROKER_H
#define OEMBROKER_H

#include <oemcryptoctx.h>
#include <oemeccp256.h>
#include <oemsha256types.h>
#include <oemaeskey.h>

ENTER_PK_NAMESPACE;

/* 
** There exist two types of OemBrokers.
** For the OemBroker that runs in the normal world, calls to OEM APIs will pass over the OEM_AES_KEY_CONTEXT as a pointer to DRM_AES_KEY
** while calls to OEM APIs of the TEE OemBroker will pass the OEM_AES_KEY_CONTEXT as an OEM_TEE_KEY.
** Respective implementation of the OemBroker shall cast the OEM_AES_KEY_CONTEXT as appropriate.
*/
typedef DRM_VOID OEM_AES_KEY_CONTEXT;

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Ignore Nonconst Param - f_pOemTeeContext and f_pBigContext are mutually exclusive and used separately in different implementations of the broker functions." );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_ECDSA_P256_Sign(
    __inout_opt                                DRM_VOID               *f_pOemTeeContext,
    __inout_opt                                struct bigctx_t        *f_pBigContext,
    __in_bcount( f_cbMessageLen )        const DRM_BYTE                f_rgbMessage[],
    __in                                       DRM_DWORD               f_cbMessageLen,
    __in                                 const PRIVKEY_P256           *f_pPrivateKey,
    __out                                      SIGNATURE_P256         *f_pbSignature );

DRM_API DRM_RESULT DRM_CALL Oem_Broker_ECDSA_P256_Verify(
    __inout_opt                                DRM_VOID               *f_pOemTeeContext,
    __inout_opt                                struct bigctx_t        *f_pBigContext,
    __in_ecount( f_cbMessageLen )       const  DRM_BYTE                f_rgbMessage[],
    __in                                const  DRM_DWORD               f_cbMessageLen,
    __in                                const  PUBKEY_P256            *f_pPubkey,
    __in                                const  SIGNATURE_P256         *f_pSignature );

DRM_API DRM_RESULT DRM_CALL Oem_Broker_Random_GetBytes(
    __inout_opt                                DRM_VOID               *f_pOemTeeContext,
    __in_opt                                   DRM_VOID               *f_pOemContext,
    __out_bcount(f_cbData)                     DRM_BYTE               *f_pbData,
    __in                                       DRM_DWORD               f_cbData );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Aes_EncryptOneBlock(
    __in_ecount( 1 )                   const OEM_AES_KEY_CONTEXT      *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE                  f_rgbData[ DRM_AES_BLOCKLEN ] );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Aes_DecryptOneBlock(
    __in_ecount( 1 )                   const OEM_AES_KEY_CONTEXT      *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE                  f_rgbData[ DRM_AES_BLOCKLEN ] );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Oem_Broker_IsTEE( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_CRITSEC_Initialize( DRM_VOID );
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Broker_CRITSEC_Uninitialize( DRM_VOID );

//
// LWE (kwilson)  In Microsoft PK code, Oem_Broker_CRITSEC_Enter returns DRM_VOID.
// Lwpu had to change the return value to DRM_RESULT in order to support a return
// code from acquiring/releasing the critical section, which may not always succeed.
//
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_CRITSEC_Enter( DRM_VOID );
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Broker_CRITSEC_Leave( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_MemAlloc(
    __in                                     DRM_DWORD                 f_cbSize,
    __out                                    DRM_VOID                **f_ppv );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Broker_MemFree(
    __inout                                  DRM_VOID                **f_ppv );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Clock_GetSystemTimeAsFileTime(
    __inout_opt                                DRM_VOID               *f_pOemTeeContextAllowNULL,
    __out                                      DRMFILETIME            *f_pFileTime );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_SHA256_CreateDigest(
    __inout_opt                                     DRM_VOID                   *f_pOemTeeContext,
    __in                                            DRM_DWORD                   f_cbBuffer,
    __in_ecount( f_cbBuffer )                 const DRM_BYTE                    f_rgbBuffer[],    
    __out                                           DRM_SHA256_Digest          *f_pDigest);


PREFAST_POP;

EXIT_PK_NAMESPACE;

#endif // OEMBROKER_H
