/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef OEMTEEINTERNAL_H
#define OEMTEEINTERNAL_H 1

#include <oemteetypes.h>
#include <oemaescommon.h>
#include <oemaeskeywrap.h>

ENTER_PK_NAMESPACE;

//
// Code reviewers @ Microsoft: We noticed that this variable is unused.
// However, to make the diff easier, we are leaving it as is.
//
extern const DRM_ID                   g_idCTK;

// LWE (nkuo): TKs are generated on demand in production code. So we don't need these globals/struct to be compiled in.
#ifdef SINGLE_METHOD_ID_REPLAY
#define OEM_TEE_TK_HISTORY_KEY_COUNT  ((DRM_DWORD)1)

typedef struct __tag_OEM_TEE_TK_HISTORY {
    DRM_DWORD           coKeys; /* coKeys == 0 indicates not initialized state */
    OEM_TEE_KEY         rgoKeys[ OEM_TEE_TK_HISTORY_KEY_COUNT ];
} OEM_TEE_TK_HISTORY;
#endif

/*
** WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
**
** This assert requires you to NOT place any data inside OEM_TEE_CONTEXT.
** Refer to the comments at the declaration of OEM_TEE_CONTEXT.
** If you acknowledge the risks ilwolved with using the OEM_TEE_CONTEXT
** and decide to use it anyway, you may modify this assert.
**
** WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
*/
#define ASSERT_TEE_CONTEXT_HAS_NO_DATA( pCtx )  DRMASSERT( ( pCtx )->cbUnprotectedOEMData == 0 && ( ( pCtx )->pbUnprotectedOEMData == NULL ) )

#define ASSERT_TEE_CONTEXT_ALLOW_NULL( pCtx ) do {                                                              \
    if( ( pCtx ) != NULL )                                                                                      \
    {                                                                                                           \
        DRMASSERT( ( ( pCtx )->cbUnprotectedOEMData == 0 ) == ( ( pCtx )->pbUnprotectedOEMData == NULL ) );     \
        ASSERT_TEE_CONTEXT_HAS_NO_DATA( pCtx );                                                                 \
    }                                                                                                           \
} while(FALSE)

#define ASSERT_TEE_CONTEXT_REQUIRED( pCtx ) do {    \
    DRMASSERT( ( pCtx ) != NULL );                  \
    ASSERT_TEE_CONTEXT_ALLOW_NULL( pCtx );          \
} while(FALSE)

typedef struct __tagOEM_TEE_MEM_ALLOC_INFO
{
    DRM_DWORD dwAllocatedSize;
    DRM_DWORD dwPadding;
} OEM_TEE_MEM_ALLOC_INFO;

EXIT_PK_NAMESPACE;

#endif // OEMTEEINTERNAL_H

