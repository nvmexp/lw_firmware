/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*
** Module Name: aes128.h
**
** Abstract:
**    This module contains the public data structures and API definitions
**  needed to utilize the low-level AES encryption routines.
**
**  Routines are implemented in aes128.c
*/


#ifndef AES128_H
#define AES128_H

#include <drmerr.h>
#include <oemaescommon.h>
#include <oemaesimpl.h>
#include <oemcommon.h>
#include <oemteecryptointernaltypes.h>

#include "playready/3.0/lw_playready.h"
#include "scp_internals.h"
#include "mem_hs.h"

ENTER_PK_NAMESPACE;

#if defined(USE_MSFT_SW_AES_IMPLEMENTATION)

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Aes_CreateKey128(
    __out_ecount( 1 )                         DRM_AESTable *f_pKeyTable,
    __in_bcount( DRM_AES_KEYSIZE_128 )  const DRM_BYTE      f_rgbKey[ DRM_AES_KEYSIZE_128 ] );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Aes_Encrypt128(
    __out_bcount( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbEncrypted[ DRM_AES_KEYSIZE_128 ],
    __in_bcount( DRM_AES_KEYSIZE_128 )  const DRM_BYTE   f_rgbClear[ DRM_AES_KEYSIZE_128 ],
    __in_ecount( DRM_AES_ROUNDS_128 + 1 ) const DRM_DWORD  f_rgbKeys[ DRM_AES_ROUNDS_128 + 1 ][ 4 ] );

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Aes_Decrypt128(
    __out_bcount( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbClear[ DRM_AES_KEYSIZE_128 ],
    __in_bcount( DRM_AES_KEYSIZE_128 )  const DRM_BYTE   f_rgbEncrypted[ DRM_AES_KEYSIZE_128 ],
    __in_ecount( DRM_AES_ROUNDS_128+1 ) const DRM_DWORD  f_rgbKeys[DRM_AES_ROUNDS_128+1][4] );

#else // else part of if defined(USE_MSFT_SW_AES_IMPLEMENTATION)

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EncryptDecryptOneBlock_LWIDIA_LS(
    __out_bcount( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbOutput[ DRM_AES_KEYSIZE_128 ],
    __in_bcount ( DRM_AES_KEYSIZE_128 ) const DRM_BYTE   f_rgbInput [ DRM_AES_KEYSIZE_128 ],
    __in_ecount ( DRM_AES_KEYSIZE_128 ) const DRM_BYTE   f_rgbKey   [ DRM_AES_KEYSIZE_128 ], 
    __in                                const DRM_DWORD  f_fCryptoMode );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EncryptOneBlockWithKeyForKDFMode_LWIDIA_HS(
    __inout_bcount( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbBuffer[ DRM_AES_KEYSIZE_128 ],
    __in                                  const DRM_DWORD  f_fCryptoMode );

#endif
EXIT_PK_NAMESPACE;

#endif // AES128_H

