/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*
**  oemaes.h
**
**  Contains the key structure for AES
**
*/

#ifndef OEMAESKEY_H
#define OEMAESKEY_H

#include <oemaesimpl.h>

ENTER_PK_NAMESPACE;

/*
** AES secret key
**
** The actual contents of the structure are opaque.
**
** The opaque data is represented as a byte array below.
** Do not access the array directly.
*/
typedef struct __tagDRM_AES_KEY
{
    DRM_BYTE rgbOpaque[ DRM_AES_KEYSTRUCTSIZE ];
} DRM_AES_KEY;

EXIT_PK_NAMESPACE;

#endif // OEMAESKEY_H

