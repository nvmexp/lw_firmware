/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef OEMRC4TYPES_H
#define OEMRC4TYPES_H

#include <drmerr.h>

#define RC4_TABLESIZE 256

ENTER_EXTERN_C_NAMESPACE;

/* Key structure */
typedef struct __tagRC4_KEYSTRUCT
{
    DRM_BYTE S[ RC4_TABLESIZE ];     /* State table */
    DRM_BYTE i, j;        /* Indices */
} RC4_KEYSTRUCT;

EXIT_EXTERN_C_NAMESPACE;

#endif // OEMRC4TYPES_H

