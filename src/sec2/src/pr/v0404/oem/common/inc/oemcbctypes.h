/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __OEMCBCTYPES_H__
#define __OEMCBCTYPES_H__

#include <drmtypes.h>

ENTER_PK_NAMESPACE;

typedef struct _CBCKey
{
    DRM_DWORD a1, b1, c1, d1, e1, f1, a2, b2, c2, d2, e2, f2;
} DRM_CBCKey;

typedef struct __tagCBCState
{
    DRM_DWORD sum,t;
    DRM_BYTE  buf[ 8 ];
    DRM_DWORD dwBufLen;
} DRM_CBCState;

EXIT_PK_NAMESPACE;

#endif /* __OEMCBCTYPES_H__ */

