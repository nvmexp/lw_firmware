/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMSELWRESTORETYPES_H
#define DRMSELWRESTORETYPES_H

#include <drmtoken.h>
#include <drmdatastoretypes.h>
#include <drmpragmas.h>
#include <oemsha1.h>

ENTER_PK_NAMESPACE;

#define DRM_SEC_STORE_MAX_SLOT_SIZE       (DRM_DWORD)( 1024 + 2 * sizeof(DRM_DWORD) + DRM_SHA1_DIGEST_LEN )
#define DRM_MAX_ATTRIBUTES_PER_SST_KEY    25
#define DRM_MAX_ATTRIBUTE_STRING_LENGTH   25

/*
    Secure store API users.  Please note that there are 3 modes these APIs open the secure store in.
    The standard DRM_SST_OpenKeyTokens API will open and cache a secure store entry.  This data will
    be flushed to disk on a call to DRM_SST_CloseKey.  No file I/O is done in the calls to
    DRM_SST_GetTokelwalue and DRM_SST_SetTokelwalue in between.

    Using the enumerator APIs requries that the DST context the enum is given be kept open the whole
    time.

    And finally the DRM_SST_GetData & DRM_SST_SetData are direct read/write tunnels to the secure data in
    the secure store.  The secure store opens and closes the DST during this call so no caching in on.
    If the Token APIs are too restrictive going directly to these raw read/write may be the best option; but these
    should be used with caution.
*/


typedef enum
{
    SELWRE_STORE_UNDEFINED_DATA           = 0x0,    /* Added for a PC client */
    SELWRE_STORE_LICENSE_DATA             = 0x1,
    SELWRE_STORE_GLOBAL_DATA              = 0x2,
    SELWRE_STORE_REVOCATION_DATA          = 0x3,
    SELWRE_STORE_METERING_DATA            = 0x4,
    SELWRE_STORE_RESERVED_5               = 0x5,    /* Reserved - Not Supported. */
    SELWRE_STORE_DEVICE_REGISTRATION_DATA = 0x6,
    SELWRE_STORE_CACHED_BLOB_DATA         = 0x7,
    SELWRE_STORE_TRANSIENT_DATA           = 0x8,
    SELWRE_STORE_SELWRE_STOP_DATA         = 0x9,
} eDRM_SELWRE_STORE_TYPE;

#define DRM_SELWRE_STORE_NUM_CHILD_NODES 0x10

/* In the slot meta data if the high bit is set it is raw data.  Otherwise it is TOKEN data */
#define DRM_SST_RAW_DATA                ((DRM_DWORD)(0x80000000))
#define DRM_SST_SLOT_SIZE_MASK          ((DRM_DWORD)(0x3FFF0000))
#define DRM_SST_SLOT_SIZE_VALID_MASK    ((DRM_DWORD)(0x40000000))
#define DRM_SST_DEVREG_PREALLOCATE_SIZE ((DRM_DWORD)30000)
#define DRM_SST_PREALLOCATE_SIZE        ((DRM_DWORD)500)
#define DRM_SST_SLOT_DATA_MASK          ((DRM_DWORD)(0x7FFFFF00))
#define DRM_SST_SLOT_VERSION_MASK       ((DRM_DWORD)(0x0000FF00))
#define DRM_SST_SLOT_VERSION            ((DRM_DWORD)(0x00000100))

#define DRM_SST_SLOT_V0_HASH_OFFSET  sizeof(DRM_DWORD)
#define DRM_SST_SLOT_V0_DATA_OFFSET (DRM_SST_SLOT_V0_HASH_OFFSET + DRM_SHA1_DIGEST_LEN)
#define DRM_SST_SLOT_V0_HEADER_SIZE  DRM_SST_SLOT_V0_DATA_OFFSET

#define DRM_SST_SLOT_METADATA_SIZE     sizeof(DRM_DWORD)
#define DRM_SST_SLOT_SIZEDATA_SIZE     sizeof(DRM_DWORD)
#define DRM_SST_SLOT_HASH_OFFSET  DRM_SST_SLOT_METADATA_SIZE + DRM_SST_SLOT_SIZEDATA_SIZE
#define DRM_SST_SLOT_DATA_OFFSET (DRM_SST_SLOT_HASH_OFFSET + DRM_SHA1_DIGEST_LEN)
#define DRM_SST_SLOT_HEADER_SIZE  DRM_SST_SLOT_DATA_OFFSET

#define DRM_SST_SET_SLOT_METADATA( dwMetaData ) do {    \
    dwMetaData &= ~DRM_SST_SLOT_DATA_MASK;              \
    dwMetaData |= DRM_SST_SLOT_VERSION                  \
               |  DRM_SST_SLOT_SIZE_VALID_MASK;         \
    } while( FALSE )

#define DRM_SST_GET_SLOT_SIZE_FROM_METADATA( cbSlotData, dwMetaData ) cbSlotData = (dwMetaData & DRM_SST_SLOT_SIZE_MASK) >> 16;

/* These are methods to resolve conflicts when data is written to the secure store but some other
** instance already wrote their changes to the secure store.
*/
#define DRM_TOKEN_RESOLVE_BITMASK    7
#define DRM_TOKEN_RESOLVE_CALLBACK   0 /* The supplied callback function will resolve the conflict */
#define DRM_TOKEN_RESOLVE_FIRST_WINS 1 /* The first data to be Committed will be persisted */
#define DRM_TOKEN_RESOLVE_LAST_WINS  2 /* The last data to be Commited will be persisted */
#define DRM_TOKEN_RESOLVE_DELTA      3 /* Available for TOKEN_LONG datatype or other numeric types only */

#define DRM_TOKEN_FLAG_DIRTY         16 /* The token has been changed since being loaded */

typedef struct __tagCachedAttribute
{
    PERSISTEDTOKEN    TokenDelta;
    DRM_CONST_STRING  dstrAttribute;

    /*
    ** First 2 bits of dwFlags is for DRM_TOKEN_RESOLVE_*
    */
    DRM_DWORD         dwFlags;

    DRM_BYTE         *pTokelwalue;  /* Never assume this is aligned before accessing it!!*/
} CachedAttribute;

typedef struct __tagSEC_STORE_CONTEXT
{
    DRM_DST_NAMESPACE_CONTEXT oNsContext;
    DRM_DST_SLOT_CONTEXT      oSlotContext;

    CachedAttribute rgAttributes [DRM_MAX_ATTRIBUTES_PER_SST_KEY];
    DRM_BYTE        rgbSlotData  [DRM_SEC_STORE_MAX_SLOT_SIZE];
    DRM_BYTE        rgbPassword  [DRM_SHA1_DIGEST_LEN];

    DRM_DST_KEY     rgbKey1;
    DRM_DST_KEY     rgbKey2;

    const DRM_DST_NAMESPACE *pNamespaceId;
    eDRM_SELWRE_STORE_TYPE   eType;

    DRM_WORD wNumAttributes;
    DRM_WORD wNumOriginalAttributes;
    DRM_DWORD cbSlotData;
    DRM_DWORD cbSlot;
    DRM_BOOL fInited;
    DRM_DST  *pDatastore;
    DRM_BOOL fOpened;
    DRM_BOOL fLocked;
    DRM_BOOL fDirty;
    DRM_DWORD dwSlotVersion;
    DRM_BOOL  fNoPassword;
    DRM_DST_SLOT_HINT slotHint;
} DRM_SECSTORE_CONTEXT;

EXIT_PK_NAMESPACE;

#endif // DRMSELWRESTORETYPES_H

