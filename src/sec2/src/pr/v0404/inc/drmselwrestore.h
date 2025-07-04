/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMSELWRESTORE_H__
#define __DRMSELWRESTORE_H__

#include <drmselwrestoretypes.h>
#include <drmcallbacks.h>

ENTER_PK_NAMESPACE;

typedef enum _tag_EnumModeSST
{
    eSSTEnumNone = 0,     /* no secure store entries found */
    eSSTEnumFiltered,     /* enum for secure store entries matching given ID */
    eSSTEnumNatural       /* enum all secure store entries */
} _EnumModeSST;

typedef struct _tag_DRM_SECSTOREENUM_CONTEXT
{
    DRM_DST_ENUM_CONTEXT      oDstEnumContext;
    DRM_DST_NAMESPACE_CONTEXT oNsContext;
    DRM_DST_SLOT_CONTEXT      oSlotContext;
    DRM_BOOL                  fInited;
    _EnumModeSST              eMode;
    eDRM_SELWRE_STORE_TYPE    eType;
    DRM_BOOL                  fLwrrIsValid;
    DRM_KID                   oLwrrKID;
    DRM_LID                   oLwrrLID;
    DRM_DST                  *pDatastore;
} DRM_SECSTOREENUM_CONTEXT;


/* Flags for DRM_SST_OpenKeyTokens */
#define DRM_SELWRE_STORE_CREATE_NEW             0x1
#define DRM_SELWRE_STORE_OPEN_ALWAYS            0x2
#define DRM_SELWRE_STORE_OPEN_EXISTING          0x4

#define DRM_SELWRE_STORE_VALID_FLAGS    (DRM_SELWRE_STORE_CREATE_NEW | DRM_SELWRE_STORE_OPEN_ALWAYS | DRM_SELWRE_STORE_OPEN_EXISTING)

DRM_API DRM_RESULT DRM_CALL DRM_SST_LoadKeyTokens(
    __inout    DRM_SECSTORE_CONTEXT   *pcontextSST,
    __in       DRM_DWORD               dwFlags,
    __in       DRM_DST                *pDatastore );

DRM_API DRM_RESULT DRM_CALL DRM_SST_OpenKeyTokens(
    __inout                                      DRM_SECSTORE_CONTEXT   *pcontextSST,
    __in                                   const DRM_ID                 *pKey1,
    __in_opt                               const DRM_ID                 *pKey2,
    __in_bcount_opt( OEM_SHA1_DIGEST_LEN ) const DRM_BYTE                rgbPassword [ OEM_SHA1_DIGEST_LEN ],
    __in                                         DRM_DWORD               dwFlags,
    __in                                         eDRM_SELWRE_STORE_TYPE  eType,
    __in                                         DRM_DST                *pDatastore );

DRM_API DRM_RESULT DRM_CALL DRM_SST_GetTokelwalue(
    __in       DRM_SECSTORE_CONTEXT *pcontextSST,
    __in const DRM_CONST_STRING     *pdstrAttribute,
    __out      TOKEN                *pToken );

DRM_API DRM_RESULT DRM_CALL DRM_SST_SetTokelwalue(
    __in       DRM_SECSTORE_CONTEXT *pcontextSST,
    __in const DRM_CONST_STRING     *pdstrAttribute,
    __in const TOKEN                *pToken );

DRM_API DRM_RESULT DRM_CALL DRM_SST_SetExplicitResolutionTokelwalue(
    __in       DRM_SECSTORE_CONTEXT *pcontextSST,
    __in const DRM_CONST_STRING     *pdstrAttribute,
    __in const TOKEN                *pToken,
    __in       DRM_DWORD             dwFlags );

DRM_API DRM_RESULT DRM_CALL DRM_SST_CloseKey(
    __in           DRM_SECSTORE_CONTEXT *pcontextSST,
    __in           DRM_DST              *pDatastore,
    __in_opt       DRMPFNPOLICYCALLBACK  pfnMergeCallback,
    __in_opt const DRM_VOID             *pvCallbackData );

DRM_API DRM_RESULT DRM_CALL DRM_SST_OpenAndLockSlot(
    __in                                     DRM_DST                *f_pDatastore,
    __in                                     eDRM_SELWRE_STORE_TYPE  f_eType,
    __in                               const DRM_ID                 *f_pKey1,
    __in_opt                           const DRM_ID                 *f_pKey2,
    __in_bcount( OEM_SHA1_DIGEST_LEN ) const DRM_BYTE                f_rgbPassword[ OEM_SHA1_DIGEST_LEN ],
    __in                                     DRM_DWORD               f_dwFlags,
    __out                                    DRM_SECSTORE_CONTEXT   *f_pcontextSST,
    __inout_opt                              DRM_DWORD              *f_pcbData );

DRM_API DRM_RESULT DRM_CALL DRM_SST_GetLockedData(
    __in                           DRM_SECSTORE_CONTEXT   *f_pcontextSST,
    __out_bcount_opt( *f_pcbData ) DRM_BYTE               *f_pbData,
    __inout                        DRM_DWORD              *f_pcbData );

DRM_API DRM_RESULT DRM_CALL DRM_SST_SetLockedData(
    __in                          DRM_SECSTORE_CONTEXT   *f_pcontextSST,
    __in                          DRM_DWORD               f_cbData,
    __in_bcount( f_cbData ) const DRM_BYTE               *f_pbData );

DRM_API DRM_RESULT DRM_CALL DRM_SST_CloseLockedSlot(
    __in  DRM_SECSTORE_CONTEXT *f_pcontextSST );

DRM_API DRM_RESULT DRM_CALL DRM_SST_GetData(
    __in                                     DRM_SECSTORE_CONTEXT   *f_pcontextSST,
    __in                               const DRM_ID                 *f_pKey1,
    __in_opt                           const DRM_ID                 *f_pKey2,
    __in_bcount( OEM_SHA1_DIGEST_LEN ) const DRM_BYTE                f_rgbPassword[ OEM_SHA1_DIGEST_LEN ],
    __in                                     eDRM_SELWRE_STORE_TYPE  f_eType,
    __in                                     DRM_DST                *f_pDatastore,
    __out_bcount_opt( *f_pcbData )           DRM_BYTE               *f_pbData,
    __inout                                  DRM_DWORD              *f_pcbData );

DRM_API DRM_RESULT DRM_CALL DRM_SST_SetData(
    __in                                     DRM_SECSTORE_CONTEXT   *f_pcontextSST,
    __in                               const DRM_ID                 *f_pKey1,
    __in_opt                           const DRM_ID                 *f_pKey2,
    __in_bcount( OEM_SHA1_DIGEST_LEN ) const DRM_BYTE                f_rgbPassword[ OEM_SHA1_DIGEST_LEN ],
    __in                                     eDRM_SELWRE_STORE_TYPE  f_eType,
    __in                                     DRM_DST                *f_pDatastore,
    __in_bcount( f_cbData )            const DRM_BYTE               *f_pbData,
    __in                                     DRM_DWORD               f_cbData );


/*
** Delete an entry from the secure store.  Be careful when this happens -- it could open up the system for
** replay attacks
*/
DRM_API DRM_RESULT DRM_CALL DRM_SST_DeleteKey(
    __in           DRM_SECSTORE_CONTEXT   *f_pcontextSST,
    __in           eDRM_SELWRE_STORE_TYPE  f_typeSST,
    __in     const DRM_ID                 *f_pid1,
    __in_opt const DRM_ID                 *f_pid2,
    __in           DRM_DST                *f_pDatastore );

DRM_API DRM_RESULT DRM_CALL DRM_SST_GetAllData(
    __in                                                        DRM_SECSTORE_CONTEXT *pcontextSST,
    __out_bcount_opt( *pcbData )                                DRM_BYTE             *pbData,
    __inout _Post_satisfies_( *pcbData <= _Old_( *pcbData ) )   DRM_DWORD            *pcbData );

/*
** The enumeration APIs work on 2 keys or IDs.  When opening an enumerator the caller should pass in key1 (the first key used
** calls to DRM_SST_SetData and DRM_SST_OpenKeyTokens).  All entries with this value as the first key will be listed in the
** enumeration.  On calls to DRM_SST_EnumNext the value of key2 will be returned so that the caller can use the combination of
** key1 and key2 to open a unique SST entry.
** If the caller does not specify key1, then all values are returned and both key1 and key2 can be obtained from DRM_SST_EnumNext.
*/

DRM_API DRM_RESULT DRM_CALL DRM_SST_OpenEnumerator(
    __in           eDRM_SELWRE_STORE_TYPE    eType,
    __in_opt const DRM_ID                   *pKey1,
    __out          DRM_SECSTOREENUM_CONTEXT *pcontextSSTEnum,
    __in           DRM_DST                  *pDatastore,
    __in           DRM_BOOL                  fExclusiveLock );

DRM_API DRM_RESULT DRM_CALL DRM_SST_EnumNext(
    __in                DRM_SECSTOREENUM_CONTEXT *pcontextSSTEnum,
    __out_ecount_opt(1) DRM_ID                   *pKey1,
    __out_ecount_opt(1) DRM_ID                   *pKey2,
    __out_ecount(1)     DRM_DWORD                *pcbData );

DRM_API DRM_RESULT DRM_CALL DRM_SST_EnumLoadLwrrent(
    __in                                     DRM_SECSTOREENUM_CONTEXT *pcontextSSTEnum,
    __inout                                  DRM_SECSTORE_CONTEXT     *pcontextSST,
    __in_bcount( OEM_SHA1_DIGEST_LEN ) const DRM_BYTE                  rgbPassword[ OEM_SHA1_DIGEST_LEN ],
    __out                                    DRM_ID                   *pKey1,
    __out_opt                                DRM_ID                   *pKey2,
    __out                                    DRM_DWORD                *pcbData );

DRM_API DRM_RESULT DRM_CALL DRM_SST_EnumDeleteLwrrent(
    __in       DRM_SECSTOREENUM_CONTEXT *pcontextSSTEnum,
    __inout    DRM_SECSTORE_CONTEXT     *pcontextSST );

EXIT_PK_NAMESPACE;

#endif /* __DRMSELWRESTORE_H__ */
