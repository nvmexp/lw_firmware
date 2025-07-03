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

#ifndef RMT_ECCSHMFAULT_H
#define RMT_ECCSHMFAULT_H

#define LRF                             0
#define SHM                             1
#define L1DATA                          2
#define L1TAG                           3
#define CBU_PAR                         4
#define NUM_ECC_LOCATIONS               5

#define SBE                             0
#define DBE                             1
#define NUM_ECC_TYPES                   2

#define BLOCK_SIZE                  0x00100000
#define SBE_DATA                    0x00000001
#define DBE_DATA                    0x10000001
#define TEST_ITERATIONS             1000
#define FB_FBPA_INTR_SAMPLING_INTERVAL_MS 0xFA

#endif
