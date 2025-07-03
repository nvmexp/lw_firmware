/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RMT_ECCISRORDERTEST_H
#define RMT_ECCISRORDERTEST_H

#define FB                          0
#define L2_CACHE                    1
#define L1_CACHE                    2
#define SM                          3
#define NUM_LOCATIONS               2

#define SBE                         0
#define DBE                         1
#define NUM_EVENT_TYPES             2

#define BLOCK_SIZE                  0x00100000
#define SBE_DATA                    0x00000001
#define DBE_DATA                    0x10000001
#define TEST_ITERATIONS             1000
#define FB_FBPA_INTR_SAMPLING_INTERVAL_MS 0xFA

#endif
