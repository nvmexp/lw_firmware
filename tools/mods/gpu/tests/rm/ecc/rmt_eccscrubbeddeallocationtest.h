/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef ECC_SCRUBBED_DEALLOCATION_TEST
#define ECC_SCRUBBED_DEALLOCATION_TEST

#define FILL_PATTERN    0xB2
#define ERASE_PATTERN   0x00 // should get this value from RM but is lwrrently hard-coded
#define SURFACE_PITCH   0x200
#define MAX_BLOCK_SIZE  0x00002000
#define VIRTUAL         0x00000000
#define PHYSICAL        0x00000001
#define SEMA_CODE       0xA1B2C3D4 // magic # that sema is to return in notifier

#define FB                          0
#define L2_CACHE                    1

#define SBE                         0
#define DBE                         1
#define NUM_EVENT_TYPES             2

#define BLOCK_SIZE                  0x00100000
#define SBE_DATA                    0x00000001
#define DBE_DATA                    0x10000001
#define TEST_ITERATIONS             50

#endif

