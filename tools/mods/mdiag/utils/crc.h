/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _CRC_H_
#define _CRC_H_

#include "types.h"

// helper functions for callwlating CRCs and stuff

// CRC32 for a memory region
UINT32 BufCRC32(uintptr_t start, UINT32 size);

// Callwlate CRC32 for non-continous buffer
UINT32 BufCRC32Append(uintptr_t start, UINT32 size, UINT32 prevCrc);

// CRC32 generator with more knobs
UINT32 BufCrcDim32(uintptr_t addr, UINT32 pitch,
                   UINT32 width_pixels, UINT32 height_pixels,
                   UINT32 bytes_per_pixel);

#endif
