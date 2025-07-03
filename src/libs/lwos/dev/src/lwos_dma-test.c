/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file lwos_dma-test.c
 *
 * @details    This file contains all the unit tests for the dma unit
 *
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include "regmock.h"
#include "ucode-lib.h"
#include <ut/ut.h>
#include "ut/ut_suite.h"
#include "ut/ut_case.h"
#include "ut/ut_assert.h"

/* ------------------------ Application includes ---------------------------- */
#include "lwos_dma.h"
#include "lwrtos.h"
#include "lwmisc.h"
#include "dmemovl.h"
#include "lwos_ovl.h"

/* ------------------------- Defines ---------------------------------------- */
// Utility macros for checking the value of the CTX SPR
#define ASSERT_EQUAL_CTX_DMREAD_DMAIDX(ctx1, dmaIdx) \
    UT_ASSERT_EQUAL_UINT((ctx1) & (0x7 << 8), (dmaIdx) << 8)

#define ASSERT_EQUAL_CTX_DMWRITE_DMAIDX(ctx1, dmaIdx) \
    UT_ASSERT_EQUAL_UINT((ctx1) & (0x7 << 12), (dmaIdx) << 12)

//Uncomment this for debugging purposes
//#define PRINT_DMREAD_DMWRITE 1

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
UPROC_STATIC volatile LwU32 LwosDmaLock;

/* ------------------------ Mocked Function Prototypes ---------------------- */

/* ------------------------ Unit-Under-Test --------------------------------- */

/* ------------------------ Local Variables --------------------------------- */

/* -------------------------------------------------------------------------- */

/*!
 * Temporary stub to allow us to compile lwos_dma.c. Eventually
 * PMU will be an NS falcon, and DMA_REGION_CHECK will be false
 * and vApplicationIsDmaIdxPermitted won't be called, and we 
 * won't need this stub anymore.
 * TODO: Remove this stub when that happens.
 *
 * @param[in]  dmaIdx    DMA index to check
 *
 * @return     LwBool    LW_TRUE
 */
LwBool
lsfIsDmaIdxPermitted_GM20X
(
    LwU8 dmaIdx
)
{
    return LW_TRUE;
}

typedef struct {
    LwU32 numDmreadCalls;
    LwU32 numDmwriteCalls;
    LwU32 numDmwaitCalls;

    //dummy buffer for DMA transfers
    LwU8* buf;

    LwU32 surfaceAddrHi;
    LwU32 surfaceAddrLo;
    LwU32 surfaceSize;
    LwU32 offset;      //within the surface
    LwU32 numBytes;    //to transfer
    LwU32 bytesTransferred;
    LwU8 dmaIdx;
} DmaTestState;

DmaTestState g_dmaTestState;

// Some generic handlers for counting the number of calls
void dmreadHandler(LwU32 dmaOffset, LwU32 args)
{
    LwU16 pa;
    LwU16 transferSize;
    LwU8 encsize;
    LwU32 surfaceStartAddrHi = g_dmaTestState.surfaceAddrHi;
    const LwU32 surfaceStartAddrLo = g_dmaTestState.surfaceAddrLo + g_dmaTestState.offset;

    if (surfaceStartAddrLo < g_dmaTestState.surfaceAddrLo) {
        //overflow, carry the bit
        surfaceStartAddrHi++;
    }

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB(),
        (surfaceStartAddrHi << 24) | (surfaceStartAddrLo >> 8));
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB1(),
        (surfaceStartAddrHi >> 8) & 0xFFFF);
    ASSERT_EQUAL_CTX_DMREAD_DMAIDX(UTF_INTRINSICS_MOCK_GET_CTX(), g_dmaTestState.dmaIdx);

    pa = DRF_VAL(_FLCN, _DMREAD, _PHYSICAL_ADDRESS, args);
    encsize = DRF_VAL(_FLCN, _DMREAD, _ENC_SIZE, args);
    transferSize = DMA_XFER_SIZE_BYTES(encsize);

#if defined(PRINT_DMREAD_DMWRITE)
    utf_printf("dmreadHandler dmaOffset:0x%x pa:0x%x transferSize:0x%x\n",
        dmaOffset, pa, transferSize);
#endif

    //assert that we are transferring from where the last block left off
    UT_ASSERT_EQUAL_UINT((surfaceStartAddrLo & 0xFF) + g_dmaTestState.bytesTransferred, dmaOffset);
    UT_ASSERT_EQUAL_UINT((LwU16)(g_dmaTestState.buf + g_dmaTestState.bytesTransferred), pa);

    //Callwlate the number of bytes transferred after this dmread call
    g_dmaTestState.bytesTransferred += transferSize;
    //Assert that we haven't transferred more than the requested size
    UT_ASSERT_IN_RANGE_UINT(g_dmaTestState.bytesTransferred, 0, g_dmaTestState.numBytes);

    //Assert that our transfer size does not exceed the alignment of the dmaOffset or pa
    UT_ASSERT(VAL_IS_ALIGNED(dmaOffset, transferSize));
    UT_ASSERT(VAL_IS_ALIGNED(pa, transferSize));

    //Assert that we are not attempting to exceed the transfer limit
    UT_ASSERT_IN_RANGE_UINT(transferSize, 0, DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MAX_READ));

    g_dmaTestState.numDmreadCalls++;
}

void dmwriteHandler(LwU32 dmaOffset, LwU32 args)
{
    LwU16 pa;
    LwU16 transferSize;
    LwU8 encsize;
    LwU32 surfaceStartAddrHi = g_dmaTestState.surfaceAddrHi;
    const LwU32 surfaceStartAddrLo = g_dmaTestState.surfaceAddrLo + g_dmaTestState.offset;

    if (surfaceStartAddrLo < g_dmaTestState.surfaceAddrLo) {
        //overflow, carry the bit
        surfaceStartAddrHi++;
    }

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB(),
        (surfaceStartAddrHi << 24) | (surfaceStartAddrLo >> 8));
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB1(),
        (surfaceStartAddrHi >> 8) & 0xFFFF);
    ASSERT_EQUAL_CTX_DMREAD_DMAIDX(UTF_INTRINSICS_MOCK_GET_CTX(), g_dmaTestState.dmaIdx);

    pa = DRF_VAL(_FLCN, _DMWRITE, _PHYSICAL_ADDRESS, args);
    encsize = DRF_VAL(_FLCN, _DMWRITE, _ENC_SIZE, args);
    transferSize = DMA_XFER_SIZE_BYTES(encsize);

#if defined(PRINT_DMREAD_DMWRITE)
    utf_printf("dmwriteHandler dmaOffset:0x%x pa:0x%x transferSize:0x%x\n",
        dmaOffset, pa, transferSize);
#endif

    //assert that we are transferring from where the last block left off
    UT_ASSERT_EQUAL_UINT((surfaceStartAddrLo & 0xFF) + g_dmaTestState.bytesTransferred, dmaOffset);
    UT_ASSERT_EQUAL_UINT((LwU16)(g_dmaTestState.buf + g_dmaTestState.bytesTransferred), pa);

    //Callwlate the number of bytes transferred after this dmread call
    g_dmaTestState.bytesTransferred += transferSize;
    //Assert that we haven't transferred more than the requested size
    UT_ASSERT_IN_RANGE_UINT(g_dmaTestState.bytesTransferred, 0, g_dmaTestState.numBytes);

    //Assert that our transfer size does not exceed the alignment of the dmaOffset or pa
    UT_ASSERT(VAL_IS_ALIGNED(dmaOffset, transferSize));
    UT_ASSERT(VAL_IS_ALIGNED(pa, transferSize));

    //Assert that we are not attempting to exceed the transfer limit
    UT_ASSERT_IN_RANGE_UINT(transferSize, 0, DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MAX_WRITE));

    g_dmaTestState.numDmwriteCalls++;
}

void dmwaitHandler()
{
    g_dmaTestState.numDmwaitCalls++;
}

/*!
 * Helper function to malloc a buffer with an "exact" alignment. Guarentees that
 * returned ptr will not be aligned to any value higher than the requested alignment.
 *
 * @param[in]  size      The number of bytes to allocate
 * @param[in]  alignment The requested alignement in bytes
 *
 * @return     void *    Pointer to the allocated buffer
 */
void *lwosMallocExactAlignment
(
    LwU32 size,
    LwU32 alignment
)
{
    void *ret = lwosMallocAligned_IMPL(OVL_INDEX_OS_HEAP, size + alignment, alignment);

    if (((LwU32)ret) & alignment)
    {
        return ret;
    }
    else
    {
        return ((LwU8 *)ret) + alignment;
    }
}

UT_SUITE_DEFINE(DMA,
    UT_SUITE_SET_COMPONENT("LWOS Command Management")
    UT_SUITE_SET_DESCRIPTION("Unit tests for the dma functions")
    UT_SUITE_SET_OWNER("ashenfield")
    UT_SUITE_SETUP_HOOK(NULL)
    UT_SUITE_TEARDOWN_HOOK(NULL))


UT_CASE_DEFINE(DMA, dmaRead,
    UT_CASE_SET_DESCRIPTION("test the dmaRead function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMA_dmaRead]"))

/*!
 * @brief      Tests the dmaRead function
 *
 * @details    Calls the dmaRead function multiple times, requesting DMA transfers
 * of various sizes into buffers of various alignments and offsets. Uses the
 * mock function dmreadHandler to track the state of the transfer and assert
 * that dmread has been called correctly.
 *
 * This test depends on the Malloc unit to allocate an aligned buffer for DMA transfers.
 */
UT_CASE_RUN(DMA, dmaRead)
{
    RM_FLCN_MEM_DESC memDesc;
    const LwU32 initialDMB = 0xA1A1A1A1;
    const LwU16 initialDMB1 = 0xB2B2;
    const LwU16 initialCTX = 0xC3C3;

    LwU16 alignment;
    LwU32 offset;
    LwU32 numBytes;

    UTF_INTRINSICS_MOCK_SET_DMB(initialDMB);
    UTF_INTRINSICS_MOCK_SET_DMB1(initialDMB1);
    UTF_INTRINSICS_MOCK_SET_CTX(initialCTX);

    UTF_INTRINSICS_MOCK_REGISTER_DMREAD_HANDLER(dmreadHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWRITE_HANDLER(dmwriteHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWAIT_HANDLER(dmwaitHandler);

    for (alignment = DMA_MIN_READ_ALIGNMENT; alignment <= 0x100; alignment <<= 1)
    {
        for (offset = DMA_MIN_READ_ALIGNMENT; offset <= 0x100; offset <<= 1)
        {
            for (numBytes = DMA_MIN_READ_ALIGNMENT; numBytes <= 0x400; numBytes <<= 1)
            {
                // utf_printf("dmaRead: alignment:0x%x offset:0x%x numBytes:0x%x\n",
                //    alignment, offset, numBytes);
                utfResetHeap();

                memset(&g_dmaTestState, 0, sizeof(DmaTestState));
                //top byte of surfaceAddrHi is empty because DMB1 only supports 16 bits.
                g_dmaTestState.surfaceAddrHi = 0x00DCBA98;
                g_dmaTestState.surfaceAddrLo = 0x76543200;
                g_dmaTestState.surfaceSize = 4096;
                g_dmaTestState.dmaIdx = 3;
                g_dmaTestState.offset = offset;
                g_dmaTestState.numBytes = numBytes;
                g_dmaTestState.buf = lwosMallocExactAlignment(numBytes, alignment);

                memDesc.address.hi = g_dmaTestState.surfaceAddrHi;
                memDesc.address.lo = g_dmaTestState.surfaceAddrLo;
                memDesc.params = DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, g_dmaTestState.dmaIdx) |
                    DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, g_dmaTestState.surfaceSize);

                dmaRead_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes);

                UT_ASSERT_NOT_EQUAL_UINT(g_dmaTestState.numDmreadCalls, 0);
                UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmwriteCalls, 0);
                UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmwaitCalls, 1);

                //Assert that the SPRS have been returned back to their initial state
                UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB(), initialDMB);
                UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB1(), initialDMB1);
                UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_CTX(), initialCTX);
            }
        }

    }
}

UT_CASE_DEFINE(DMA, dmaWrite,
    UT_CASE_SET_DESCRIPTION("test the dmaWrite function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMA_dmaWrite]"))

/*!
 * @brief      Tests the dmaWrite function
 *
 * @details    Calls the dmaWrite function multiple times, requesting DMA transfers
 * of various sizes into buffers of various alignments and offsets. Uses the
 * mock function dmreadHandler to track the state of the transfer and assert
 * that dmread has been called correctly.
 *
 * This test depends on the Malloc unit to allocate an aligned buffer for DMA transfers.
 */
UT_CASE_RUN(DMA, dmaWrite)
{
    RM_FLCN_MEM_DESC memDesc;
    const LwU32 initialDMB = 0xA1A1A1A1;
    const LwU16 initialDMB1 = 0xB2B2;
    const LwU16 initialCTX = 0xC3C3;

    LwU16 alignment;
    LwU32 offset;
    LwU32 numBytes;

    UTF_INTRINSICS_MOCK_SET_DMB(initialDMB);
    UTF_INTRINSICS_MOCK_SET_DMB1(initialDMB1);
    UTF_INTRINSICS_MOCK_SET_CTX(initialCTX);

    UTF_INTRINSICS_MOCK_REGISTER_DMREAD_HANDLER(dmreadHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWRITE_HANDLER(dmwriteHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWAIT_HANDLER(dmwaitHandler);

    for (alignment = DMA_MIN_READ_ALIGNMENT; alignment <= 0x100; alignment <<= 1)
    {
        for (offset = DMA_MIN_READ_ALIGNMENT; offset <= 0x100; offset <<= 1)
        {
            for (numBytes = DMA_MIN_READ_ALIGNMENT; numBytes <= 0x400; numBytes <<= 1)
            {
                // utf_printf("dmaRead: alignment:0x%x offset:0x%x numBytes:0x%x\n",
                //    alignment, offset, numBytes);
                utfResetHeap();

                memset(&g_dmaTestState, 0, sizeof(DmaTestState));
                //top byte of surfaceAddrHi is empty because DMB1 only supports 16 bits.
                g_dmaTestState.surfaceAddrHi = 0x00DCBA98;
                g_dmaTestState.surfaceAddrLo = 0x76543200;
                g_dmaTestState.surfaceSize = 4096;
                g_dmaTestState.dmaIdx = 3;
                g_dmaTestState.offset = offset;
                g_dmaTestState.numBytes = numBytes;
                g_dmaTestState.buf = lwosMallocExactAlignment(numBytes, alignment);

                memDesc.address.hi = g_dmaTestState.surfaceAddrHi;
                memDesc.address.lo = g_dmaTestState.surfaceAddrLo;
                memDesc.params = DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, g_dmaTestState.dmaIdx) |
                    DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, g_dmaTestState.surfaceSize);

                dmaWrite_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes);

                UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmreadCalls, 0);
                UT_ASSERT_NOT_EQUAL_UINT(g_dmaTestState.numDmwriteCalls, 0);
                UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmwaitCalls, 1);

                //Assert that the SPRS have been returned back to their initial state
                UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB(), initialDMB);
                UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB1(), initialDMB1);
                UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_CTX(), initialCTX);
            }
        }

    }
}

UT_CASE_DEFINE(DMA, dmaReadMisaligned,
    UT_CASE_SET_DESCRIPTION("test the dmaRead function with misaligned inputs")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMA_dmaReadMisaligned]"))

/*!
 * @brief      Tests the dmaRead function with misaligned buffers
 *
 * @details    Calls the dmaRead function multiple times, requesting DMA transfers
 * to buffers which do not have correct alignment for the buffer address, offset, and size.
 * Ensure that the misalignement is caught and dmaRead returns FLCN_ERR_DMA_ALIGN
 *
 * This test depends on the Malloc unit to allocate an aligned buffer for DMA transfers.
 */
UT_CASE_RUN(DMA, dmaReadMisaligned)
{
    RM_FLCN_MEM_DESC memDesc;
    const LwU32 initialDMB = 0xA1A1A1A1;
    const LwU16 initialDMB1 = 0xB2B2;
    const LwU16 initialCTX = 0xC3C3;

    UTF_INTRINSICS_MOCK_SET_DMB(initialDMB);
    UTF_INTRINSICS_MOCK_SET_DMB1(initialDMB1);
    UTF_INTRINSICS_MOCK_SET_CTX(initialCTX);

    UTF_INTRINSICS_MOCK_REGISTER_DMREAD_HANDLER(dmreadHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWRITE_HANDLER(dmwriteHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWAIT_HANDLER(dmwaitHandler);

    //call dmaRead with a misaligned offset. Minimum supported alignment is 4.
    memset(&g_dmaTestState, 0, sizeof(DmaTestState));
    //top byte of surfaceAddrHi is empty because DMB1 only supports 16 bits.
    g_dmaTestState.surfaceAddrHi = 0x00DCBA98;
    g_dmaTestState.surfaceAddrLo = 0x76543200;
    g_dmaTestState.surfaceSize = 4096;
    g_dmaTestState.dmaIdx = 3;
    g_dmaTestState.offset = 2;
    g_dmaTestState.numBytes = 8;
    g_dmaTestState.buf = lwosMallocExactAlignment(g_dmaTestState.numBytes, 16);

    memDesc.address.hi = g_dmaTestState.surfaceAddrHi;
    memDesc.address.lo = g_dmaTestState.surfaceAddrLo;
    memDesc.params = DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, g_dmaTestState.dmaIdx) |
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, g_dmaTestState.surfaceSize);

    UT_ASSERT_EQUAL_UINT(
        dmaRead_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes),
        FLCN_ERR_DMA_ALIGN);

    // Call it again, This time, with a misaligned buf.
    g_dmaTestState.offset = 0;
    g_dmaTestState.buf = lwosMallocExactAlignment(g_dmaTestState.numBytes, 2);

    UT_ASSERT_EQUAL_UINT(
        dmaRead_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes),
        FLCN_ERR_DMA_ALIGN);

    // Call it again, This time, with a misaligned numBytes.
    g_dmaTestState.offset = 0;
    g_dmaTestState.numBytes = 10;
    g_dmaTestState.buf = lwosMallocExactAlignment(g_dmaTestState.numBytes, 0);

    UT_ASSERT_EQUAL_UINT(
        dmaRead_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes),
        FLCN_ERR_DMA_ALIGN);

    UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmreadCalls, 0);
    UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmwriteCalls, 0);
    UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmwaitCalls, 0);

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB(), initialDMB);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB1(), initialDMB1);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_CTX(), initialCTX);
}

UT_CASE_DEFINE(DMA, dmaWriteMisaligned,
    UT_CASE_SET_DESCRIPTION("test the dmaWrite function with misaligned inputs")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMA_dmaWriteMisaligned]"))

/*!
 * @brief      Tests the dmaWrite function with misaligned buffers
 *
 * @details    Calls the dmaWrite function multiple times, requesting DMA transfers
 * to buffers which do not have correct alignment for the buffer address, offset, and size.
 * Ensure that the misalignement is caught and dmaWrite returns FLCN_ERR_DMA_ALIGN
 *
 * This test depends on the Malloc unit to allocate an aligned buffer for DMA transfers.
 */
UT_CASE_RUN(DMA, dmaWriteMisaligned)
{
    RM_FLCN_MEM_DESC memDesc;
    const LwU32 initialDMB = 0xA1A1A1A1;
    const LwU16 initialDMB1 = 0xB2B2;
    const LwU16 initialCTX = 0xC3C3;

    UTF_INTRINSICS_MOCK_SET_DMB(initialDMB);
    UTF_INTRINSICS_MOCK_SET_DMB1(initialDMB1);
    UTF_INTRINSICS_MOCK_SET_CTX(initialCTX);

    UTF_INTRINSICS_MOCK_REGISTER_DMREAD_HANDLER(dmreadHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWRITE_HANDLER(dmwriteHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWAIT_HANDLER(dmwaitHandler);

    //call dmaWrite with a misaligned offset. Minimum supported alignment is 4.
    memset(&g_dmaTestState, 0, sizeof(DmaTestState));
    //top byte of surfaceAddrHi is empty because DMB1 only supports 16 bits.
    g_dmaTestState.surfaceAddrHi = 0x00DCBA98;
    g_dmaTestState.surfaceAddrLo = 0x76543200;
    g_dmaTestState.surfaceSize = 4096;
    g_dmaTestState.dmaIdx = 3;
    g_dmaTestState.offset = 2;
    g_dmaTestState.numBytes = 8;
    g_dmaTestState.buf = lwosMallocExactAlignment(g_dmaTestState.numBytes, 16);

    memDesc.address.hi = g_dmaTestState.surfaceAddrHi;
    memDesc.address.lo = g_dmaTestState.surfaceAddrLo;
    memDesc.params = DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, g_dmaTestState.dmaIdx) |
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, g_dmaTestState.surfaceSize);

    UT_ASSERT_EQUAL_UINT(
        dmaWrite_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes),
        FLCN_ERR_DMA_ALIGN);

    // Call it again, This time, with a misaligned buf.
    g_dmaTestState.offset = 0;
    g_dmaTestState.buf = lwosMallocExactAlignment(g_dmaTestState.numBytes, 2);

    UT_ASSERT_EQUAL_UINT(
        dmaWrite_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes),
        FLCN_ERR_DMA_ALIGN);

    // Call it again, This time, with a misaligned numBytes.
    g_dmaTestState.offset = 0;
    g_dmaTestState.numBytes = 10;
    g_dmaTestState.buf = lwosMallocExactAlignment(g_dmaTestState.numBytes, 0);

    UT_ASSERT_EQUAL_UINT(
        dmaWrite_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes),
        FLCN_ERR_DMA_ALIGN);

    UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmreadCalls, 0);
    UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmwriteCalls, 0);
    UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmwaitCalls, 0);

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB(), initialDMB);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB1(), initialDMB1);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_CTX(), initialCTX);
}

UT_CASE_DEFINE(DMA, dmaWriteSurfaceAddrOverflow,
    UT_CASE_SET_DESCRIPTION("test the dmaWrite function with overflowing surfaceAddr")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMA_dmaWriteSurfaceAddrOverflow]"))

/*!
 * @brief      Tests the dmaWrite function with an address overflow
 *
 * @details    Calls the dmaWrite function multiple times, with arguments such that:
 *             - offset overflows surfaceAddrLo
 *             - offset overflows surfaceAddrLo and surfaceAddrHi
 * 
 *             In the first case, the transfer should behandled correctly.
 *             In the second, we expect dmaWrite to return an error.
 *
 * This test depends on the Malloc unit to allocate an aligned buffer for DMA transfers.
 */
UT_CASE_RUN(DMA, dmaWriteSurfaceAddrOverflow)
{
    RM_FLCN_MEM_DESC memDesc;
    const LwU32 initialDMB = 0xA1A1A1A1;
    const LwU16 initialDMB1 = 0xB2B2;
    const LwU16 initialCTX = 0xC3C3;

    UTF_INTRINSICS_MOCK_SET_DMB(initialDMB);
    UTF_INTRINSICS_MOCK_SET_DMB1(initialDMB1);
    UTF_INTRINSICS_MOCK_SET_CTX(initialCTX);

    UTF_INTRINSICS_MOCK_REGISTER_DMREAD_HANDLER(dmreadHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWRITE_HANDLER(dmwriteHandler);
    UTF_INTRINSICS_MOCK_REGISTER_DMWAIT_HANDLER(dmwaitHandler);

    //call dmaWrite with an offset that overflows surfaceAddrLo
    memset(&g_dmaTestState, 0, sizeof(DmaTestState));
    //top byte of surfaceAddrHi is empty because DMB1 only supports 16 bits.
    g_dmaTestState.surfaceAddrHi = 0x00DCBA98;
    g_dmaTestState.surfaceAddrLo = 0xFFFFFF00;
    g_dmaTestState.surfaceSize = 4096;
    g_dmaTestState.dmaIdx = 3;
    g_dmaTestState.offset = 0x100;
    g_dmaTestState.numBytes = 8;
    g_dmaTestState.buf = lwosMallocExactAlignment(g_dmaTestState.numBytes, 16);

    memDesc.address.hi = g_dmaTestState.surfaceAddrHi;
    memDesc.address.lo = g_dmaTestState.surfaceAddrLo;
    memDesc.params = DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, g_dmaTestState.dmaIdx) |
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, g_dmaTestState.surfaceSize);

    UT_ASSERT_EQUAL_UINT(
        dmaWrite_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes),
        FLCN_OK);

    UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmreadCalls, 0);
    UT_ASSERT_NOT_EQUAL_UINT(g_dmaTestState.numDmwriteCalls, 0);
    UT_ASSERT_EQUAL_UINT(g_dmaTestState.numDmwaitCalls, 1);

    //call dmaWrite with an offset that overflows surfaceAddrLo and surfaceAddrHi
    memset(&g_dmaTestState, 0, sizeof(DmaTestState));
    g_dmaTestState.surfaceAddrHi = 0xFFFFFFFF;
    g_dmaTestState.surfaceAddrLo = 0xFFFFFF00;
    g_dmaTestState.surfaceSize = 4096;
    g_dmaTestState.dmaIdx = 3;
    g_dmaTestState.offset = 0x100;
    g_dmaTestState.numBytes = 8;
    g_dmaTestState.buf = lwosMallocExactAlignment(g_dmaTestState.numBytes, 16);

    memDesc.address.hi = g_dmaTestState.surfaceAddrHi;
    memDesc.address.lo = g_dmaTestState.surfaceAddrLo;
    memDesc.params = DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, g_dmaTestState.dmaIdx) |
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, g_dmaTestState.surfaceSize);

    UT_ASSERT_EQUAL_UINT(
        dmaWrite_IMPL(g_dmaTestState.buf, &memDesc, g_dmaTestState.offset, g_dmaTestState.numBytes),
        FLCN_ERR_ILWALID_ARGUMENT);

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB(), initialDMB);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_DMB1(), initialDMB1);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_CTX(), initialCTX);
}

UT_CASE_DEFINE(DMA, lwosDmaLock,
    UT_CASE_SET_DESCRIPTION("test the lwosDmaLockAcquire and lwosDmaLockRelease functions")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMA_lwosDmaLock]"))

/*!
 * @brief      Tests the lwosDmaLockAcquire and lwosDmaLockRelease functions
 *
 * @details    - Aquire and release a lock 5 times. We expect no errors here.
 *             - Attempt to release again: lwosDmaLockRelease should halt.
 *             - Set the lock count to LW_U32_MAX and attempt to aquire: lwosDmaLockRelease should halt.
 *
 * This test has no external dependencies.
 */
UT_CASE_RUN(DMA, lwosDmaLock)
{
    LwU32 initHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();
    LwU32 i;
    for (i=0; i < 5; i++)
        lwosDmaLockAcquire_IMPL();

    for (i=0; i < 5; i++)
        lwosDmaLockRelease_IMPL();

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), initHaltCount);

    lwosDmaLockRelease_IMPL();

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), initHaltCount + 1);

    LwosDmaLock = LW_U32_MAX; 

    lwosDmaLockAcquire_IMPL();

    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), initHaltCount + 2);
}
