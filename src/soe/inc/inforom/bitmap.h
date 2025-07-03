/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef IFS_BITMAP_H
#define IFS_BITMAP_H

#include <lwtypes.h>

// Bitmap access functions
void ifsBitmapSet(LwU64 *pBitmap, LwU32 start, LwU32 count);
void ifsBitmapClear(LwU64 *pBitmap, LwU32 start, LwU32 count);
LwBool ifsBitmapIsSet(LwU64 *pBitmap, LwU32 index);

#endif // IFS_BITMAP_H
