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

#ifndef RMT_ECCASYNCSCRUBSTRESSTEST_H
#define RMT_ECCASYNCSCRUBSTRESSTEST_H

#define NUM_TESTS       5000
#define FB              0
#define L2_CACHE        1
#define MIN_BLOCK_SIZE  0x001000
#define MAX_BLOCK_SIZE  0x800000
#define MAX_NUM_BLOCKS  1000
#define PAGE_SIZE_KB    64
#define ALIGNMENT       64

//
// the block indecies that bound the region of FB that is safe to scrub (i.e. our
// driver isn't using memory in this area). these are experimentally determined
// or are "just known".
//
#define START_OF_SAFE_REGION    0x200
#define END_OF_SAFE_REGION      0x1590

#endif
