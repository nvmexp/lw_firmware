/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010,2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_ECCDEFS_H
#define INCLUDED_ECCDEFS_H

// These opcodes were chosen to all have checkbits of 0x0 in L1 & SM.
// That gets around some issues with reading/writing these values
// while checkbits are being enabled and disabled.
// See GK110GpuSubdevice::ForceL1EccCheckbits for checkbit algorithm.
// Other values that produce zero checkbits are:
// 0x0000060f,0x000006d1,0x00000732,0x000007ec,0x0000086b,0x000008b5,0x00000956,0x00000988
// ,0x00000a11,0x00000acf,0x00000b2c,0x00000bf2,0x00000c1e,0x00000cc0,0x00000d23,0x00000dfd
// ,0x00000e64,0x00000eba,0x00000f59,0x00000f87
//
#define HOST_STORE_THREAD_ID    0x000000de
#define HOST_STORE_THREAD_LOOP  0x0000013d
#define HOST_STORE_ERR_TYPE     0x000001e3
#define HOST_STORE_VAL          0x0000027a
#define HOST_DISABLE_CHECKBITS  0x000002a4
#define HOST_ENABLE_CHECKBITS   0x00000347
#define HOST_CHECK_VAL          0x00000399
#define HOST_CHECK_COUNTS       0x00000475
#define GPU_DONE                0x000004ab
#define HOST_PRINT              0x00000548
#define HOST_DONE               0x00000596

#define MAX_SMS 32
#define WARP_SIZE 32

#define COMMAND_OFFSET 0
#define REPLY_OFFSET   0
#define ARG_OFFSET     1
#define COMMAND_SIZE   8

#endif // INCLUDED_ECCDEFS_H
