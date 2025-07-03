/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RMT_ECCFBFULLSCANTEST_H
#define RMT_ECCFBFULLSCANTEST_H

#define HI_32_MASK      0xFFFFFFFF00000000
#define LO_32_MASK      0x00000000FFFFFFFF
#define SEMA_CODE       0xA1B2C3D4 // magic # that sema is to return in notifier
#define VIRTUAL         0x00000000
#define PHYSICAL        0x00000001
#define SBE             0x00000001
#define DBE             0x10000001
#define MAX_BLOCK_SIZE  0x00080000

#endif
