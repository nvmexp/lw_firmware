/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

// This file is shared between ucode and RM test.
#ifndef DMVA_COMMON_H
#define DMVA_COMMON_H

#define DMEMVA_NUM_TESTS 33

// In fmodel, a bug was introduced sometime around 9/14 which made a block that's pending (and
// therefore invalid) to be marked dirty.  Uncomment this when this gets fixed (if this is even a
// bug).
#define IGNORE_DIRTY_ON_ILWALID

// Uncomment this in emulation if dmlvl exception and dmem exception tests are slow
// This is necessary in fmodel if we load tests above ID 31 (for some reason).
#define NO_FB_VERIFY

// Comment this out if we ever test priv security
#define NO_PRIV_SEC

// remove this in fmodel until it gets support for SEC.disable_exceptions
// This is untested, so you might want to leave this uncommented.
#define NO_SEC_DISABLE_EXCEPTIONS

// Comment this out in emulation always, and in fmodel when it gets fixed
// #define FMODEL_DMA2_FIX

// Comment this out in emulation always, and in fmodel when it gets fixed
// #define FMODEL_DMA2_INTERRUPT_FIX

// Comment this out in emulation always.  It is slow and unnecessary in fmodel.
// #define DONT_RESET_ON_EXCEPTION

typedef enum
{
    DMVA_OK = 0x7fcd18cb,
    DMVA_FAIL = 0xbbecb4f7,
    DMVA_NULL = 0xacf668b4,
    DMVA_FORWARD_PROGRESS = 0x59322d50,
    DMVA_ASSERT_FAIL = 0x11dfc079,
    DMVA_BAD_TAG = 0x89a17bdb,
    DMVA_BAD_STATUS = 0xeec538bc,
    DMVA_BAD_MASK = 0x90b7f54f,
    DMVA_BAD_DATA = 0x6cc8903f,
    DMVA_DMEMC_BAD_MISS = 0x89944796,
    DMVA_DMEMC_BAD_DATA_READ = 0x2f948995,
    DMVA_DMEMC_BAD_DATA_WRITTEN = 0x4a64e02c,
    DMVA_DMEMC_BAD_LVLERR = 0x0c6f0d9d,
    DMVA_DMLVL_SHOULD_EXCEPT = 0xbe3f8350,
    DMVA_UNEXPECTED_EXCEPTION = 0x5cc1c153,
    DMVA_PLATFORM_NOT_SUPPORTED = 0x77a9e230,
    DMVA_EMEM_BAD_DATA = 0x24669945,
    DMVA_REQUEST_PRIV_RW_FROM_RM = 0x2ef1c9a2,
    DMVA_BAD_VALID = 0x749968ad,
    DMVA_BAD_PENDING = 0xb998d7fd,
    DMVA_BAD_DIRTY = 0xbe27bc20,
    DMVA_BAD_ZEROS = 0xfedb6911,
    DMVA_DMA_TAG_BAD_NEXT_TAG = 0x466ea8ac,
    DMVA_DMA_TAG_HANDLER_WAS_NOT_CALLED = 0x75c17ba9,
} DMVA_RC;

// Block Status is here instead of dmvamem so rmtest can decode DMVA_BAD_STATUS
typedef enum
{
    INVALID = 0,
    VALID_AND_CLEAN,
    VALID_AND_DIRTY,
    PENDING,
    N_BLOCK_STATUS,
} BlockStatus;

#define DMA_BUFFER_DMEM_NBLKS (1 << 16)
#define DMA_BUFFER_IMEM_NBLKS (1 << 16)
#define DMA_BUFFER_DMEM_SIZE (256 * (DMA_BUFFER_DMEM_NBLKS))
#define DMA_BUFFER_IMEM_SIZE (256 * (DMA_BUFFER_IMEM_NBLKS))

#define REG_TEST_MAILBOX0 0xab6cee87
#define REG_TEST_MAILBOX1 0x50bb2486

#endif
