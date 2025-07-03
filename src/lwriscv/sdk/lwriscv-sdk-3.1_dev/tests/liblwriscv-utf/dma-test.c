/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdint.h>
#include <stdbool.h>
#include <test-macros.h>
#include <regmock.h>
#include <ut/ut.h>
#include <ut/ut_case.h>
#include <ut/ut_assert.h>
#include "status-shim.h"

#if defined(UTF_USE_LIBLWRISCV)
    #include <lwmisc.h>
    #include <liblwriscv/dma.h>
    #include <liblwriscv/io.h>
    #include <liblwriscv/memutils.h>
    #if LWRISCV_HAS_FBIF
    #include <liblwriscv/fbif.h>
    #elif LWRISCV_HAS_TFBIF
    #include <liblwriscv/tfbif.h>
    #endif
#elif defined(UTF_USE_LIBFSP)
    #include <misc/lwmisc_drf.h>
    #include <fbdma/fbdma.h>
    #include <cpu/io_local.h>
    #include <fbdma/memutils.h>
    #if LWRISCV_HAS_FBIF
    #include <fbdma/fbif.h>
    #elif LWRISCV_HAS_TFBIF
    #include <fbdma/tfbif.h>
    #endif
#else
    #error UTF_USE_LIBLWRISCV or UTF_USE_LIBFSP must be defined
#endif

#define DMEM_BUF_SIZE  0x100
#define DMEM_BUF_ALIGN 0x100
// Use any part of FB except where the non resident code and data are
#define FB_ADDR        0x1000ull

extern char _imem_buffer_start[];
extern char _imem_buffer_end[];

static unsigned char *dmem_buf;
static volatile uint8_t *p_fb_buffer = (volatile uint8_t *)(LW_RISCV_AMAP_FBGPA_START + FB_ADDR);

// o be used by tests which mock the hardware
static uint32_t expected_dmatrfcmd;
static uint32_t total_xfer_size;
static uint32_t num_dmatrfcmd_full_reads;
static uint32_t num_dmatrfcmd_idle_false_reads;
static uint32_t num_dmatrfcmd_error_reads;

static void *dma_test_setup(void)
{
    // Reset variables used by mocks for tests that have mocking enabled
    expected_dmatrfcmd = 0;
    total_xfer_size = 0;
    num_dmatrfcmd_full_reads = 0;
    num_dmatrfcmd_idle_false_reads = 0;
    num_dmatrfcmd_error_reads = 0;

    // Disable mocks by default. Tests that need them can enable
    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);

    dmem_buf = (unsigned char *) utfMallocAligned(DMEM_BUF_SIZE, DMEM_BUF_ALIGN);
    UT_ASSERT_NOT_NULL(dmem_buf);

#if LWRISCV_HAS_FBIF
    fbif_aperture_cfg_t cfgs []=
    {
        {
            .aperture_idx = 0,
            .target = FBIF_TRANSCFG_TARGET_LOCAL_FB,
            .b_target_va = LW_FALSE,
            .l2c_wr = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
            .l2c_rd = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
            .b_fbif_transcfg_wachk0_enable = LW_FALSE,
            .b_fbif_transcfg_wachk1_enable = LW_FALSE,
            .b_fbif_transcfg_rachk0_enable = LW_FALSE,
            .b_fbif_transcfg_rachk1_enable = LW_FALSE,
            .region_id = 0
        }
    };

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(fbif_configure_aperture(cfgs, 1)), E_SUCCESS);

#elif LWRISCV_HAS_TFBIF
    #error DMA tests not yet compatible with TFBIF engines
#endif
}

/** \file
 * @testgroup{DMA}
 * @testgrouppurpose{Testgroup purpose is to test the FBDMA driver in dma.c}
 */
UT_SUITE_DEFINE(DMA,
                UT_SUITE_SET_COMPONENT("DMA")
                UT_SUITE_SET_DESCRIPTION("dma tests")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(dma_test_setup)
                UT_SUITE_TEARDOWN_HOOK(NULL))

/*
 * Right now, we only have tests for transfers to FB. We cannot test SYSMEM because
 * This would require code on the CPU side which allocates a buffer and passes that
 * address to the ucode. Testing FB transfers shuold be sufficient for SW verification.
 */
UT_CASE_DEFINE(DMA, dma_fb_to_dtcm,
    UT_CASE_SET_DESCRIPTION("Test fbdma_pa() FB->DTCM")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542992"))

/**
 * @testcasetitle{DMA_dma_fb_to_dtcm}
 * @testdependencies{}
 * @testdesign{
 *   1) Call fbdma_pa to transfer from FB to DTCM
 *   2) compare the source and detination buffers and verify that they contain the same data
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, dma_fb_to_dtcm)
{
    RET_TYPE ret;
    uint16_t i;

    for (i = 0; i < DMEM_BUF_SIZE; i++)
    {
        p_fb_buffer[i] = (uint8_t)(DMEM_BUF_SIZE-i-1);
    }

    ret = fbdma_pa((uint64_t)dmem_buf,
        FB_ADDR,
        DMEM_BUF_SIZE - 4,  //misaligned size to make it interesting
        0,
        LW_TRUE
    );

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_INT(retval_to_error_t(utf_memcmp(dmem_buf, (void *) p_fb_buffer, DMEM_BUF_SIZE - 4)), 0);
}

UT_CASE_DEFINE(DMA, dma_dtcm_to_fb,
    UT_CASE_SET_DESCRIPTION("Test fbdma_pa() DTCM->FB")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542992"))

/**
 * @testcasetitle{DMA_dma_dtcm_to_fb}
 * @testdependencies{}
 * @testdesign{
 *   1) Call fbdma_pa to transfer from DTCM to FB
 *   2) compare the source and detination buffers and verify that they contain the same data
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, dma_dtcm_to_fb)
{
    RET_TYPE ret;
    uint16_t i;

    for (i = 0; i < DMEM_BUF_SIZE; i++)
    {
        dmem_buf[i] = (uint8_t)i;
    }

    ret = fbdma_pa((uint64_t)(dmem_buf + 4),   // misaligned to make it interesting
        FB_ADDR,
        DMEM_BUF_SIZE - 4,
        0,
        LW_FALSE
    );

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_INT(retval_to_error_t(utf_memcmp((void *)(dmem_buf + 4), (void *) p_fb_buffer, DMEM_BUF_SIZE - 4)), 0);
}

UT_CASE_DEFINE(DMA, dma_itcm_to_fb,
    UT_CASE_SET_DESCRIPTION("Test fbdma_pa() ITCM->FB")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542992"))

/**
 * @testcasetitle{DMA_dma_itcm_to_fb}
 * @testdependencies{}
 * @testdesign{
 *   1) Call fbdma_pa to transfer from ITCM to FB
 *   2) compare the source and detination buffers and verify that they contain the same data
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, dma_itcm_to_fb)
{
    RET_TYPE ret;
    uint16_t i;

    // We can't write a pattern into our imem buffer since it is RO due to our MPU setup.
    // Copy data from the start of IMEM since this will have some non-zero bytes.
    //
    // This only seems to work when itcm address is 256 byte aligned.
    // Otherwise it gets dma'd to FB at an offset corresponding to the misalignment.
    // This only happens on itcm, not dtcm.
    uint64_t p_itcm = LW_RISCV_AMAP_IMEM_START;

    for (i = 0; i < 256; i++)
    {
        p_fb_buffer[i]=0;
    }

    ret = fbdma_pa((uint64_t)p_itcm,
        FB_ADDR,
        256,
        0,
        LW_FALSE
    );

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_INT(retval_to_error_t(memcmp((void *)p_itcm, (void *) p_fb_buffer, 256)), 0);
}

UT_CASE_DEFINE(DMA, dma_fbto_itcm,
    UT_CASE_SET_DESCRIPTION("Test fbdma_pa() FB->ITCM")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542992"))

/**
 * @testcasetitle{DMA_dma_fb_to_itcm}
 * @testdependencies{}
 * @testdesign{
 *   1) Call fbdma_pa to transfer from FB to ITCM
 *   2) compare the source and detination buffers and verify that they contain the same data
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, dma_fbto_itcm)
{
    RET_TYPE ret;
    uint16_t i;
    uint64_t imem_buf_size = _imem_buffer_end - _imem_buffer_start;

    for (i = 0; i < imem_buf_size; i++)
    {
        p_fb_buffer[i] = i;
    }

    ret = fbdma_pa((uint64_t)_imem_buffer_start,
        FB_ADDR,
        imem_buf_size,
        0,
        LW_TRUE
    );

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_INT(retval_to_error_t(utf_memcmp((void *)_imem_buffer_start, (void *) p_fb_buffer, imem_buf_size)), 0);
}

UT_CASE_DEFINE(DMA, dma_break_into_chunks,
    UT_CASE_SET_DESCRIPTION("Test breaking dma buffer into chunks")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542994"))

/**
 * @testcasetitle{DMA_dma_itcm_to_fb}
 * @testdependencies{}
 * @testdesign{
 *   1) Call fbdma_pa to transfer from FB to DTCM. Use as size that forces the driver to break the transfer into chunks.
 *   2) compare the source and detination buffers and verify that they contain the same data
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, dma_break_into_chunks)
{
    RET_TYPE ret;
    uint16_t i;
    //Pick a size that forces us to break it into all th different size chunks
    const uint64_t sizebytes = 256 + 128 + 64 + 32 + 16 + 8 + 4;

    for (i = 0; i < DMEM_BUF_SIZE; i++)
    {
        p_fb_buffer[i] = (uint8_t)(DMEM_BUF_SIZE-i-1);
    }

    // FB --> DTCM transfer
    ret = fbdma_pa((uint64_t)dmem_buf,
        FB_ADDR,
        sizebytes,
        0,
        LW_TRUE
    );

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_INT(retval_to_error_t(utf_memcmp(dmem_buf, (void *) p_fb_buffer, sizebytes)), 0);
}

/*
 * Callback functions for tests which use HW mocking.
 */
static void dmatrfcmd_mock_write(uint32_t addr, uint8_t size, uint32_t value)
{
    uint32_t xfer_size = DRF_VAL(_PRGNLCL, _FALCON_DMATRFCMD, _SIZE, value);
    total_xfer_size += (1 << xfer_size) * DMA_BLOCK_SIZE_MIN;

    UTF_IO_CACHE_WRITE(addr, size, value);

    // Zero out fields we don't want to check in the comparison below
    value = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _SIZE, 0, value);
    value = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _IDLE, 0, value);
    value = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _ERROR, 0, value);
    value = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _FULL, 0, value);
    value = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _SET_DMTAG, _FALSE, value);

    UT_ASSERT_EQUAL_UINT(value, expected_dmatrfcmd);
}

static uint32_t dmatrfcmd_mock_read(uint32_t addr, uint8_t size)
{
    uint32_t value = UTF_IO_CACHE_READ(addr, size);

    if (num_dmatrfcmd_idle_false_reads)
    {
        value = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IDLE, _FALSE, value);
        num_dmatrfcmd_idle_false_reads--;
    }
    else
    {
        value = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IDLE, _TRUE, value);
    }

    if (num_dmatrfcmd_error_reads)
    {
        value = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _ERROR, _TRUE, value);
        num_dmatrfcmd_error_reads--;
    }
    else
    {
        value = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _ERROR, _FALSE, value);
    }

    if (num_dmatrfcmd_full_reads)
    {
        value = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _FULL, _TRUE, value);
        num_dmatrfcmd_full_reads--;
    }
    else
    {
        value = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _FULL, _FALSE, value);
    }

    return value;
}

UT_CASE_DEFINE(DMA, dma_full_queue,
    UT_CASE_SET_DESCRIPTION("Test fbdma_pa with a full Queue condition")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5629072"))
/**
 * @testcasetitle{DMA_dma_full_queue}
 * @testdependencies{}
 * @testdesign{
 *   1) Set up Register Mocking to simulate a scenario when the DMA Request Queue is full.
 *   2) Call the driver to kick off a DMA transfer.
 *   3) Verify that the driver does not attempt to enqueue the transfer until the DMA Request Queue is non-full.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, dma_full_queue)
{
    RET_TYPE ret;
    const uint8_t dma_idx = 0;

    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    // seed some registers with values that the DMA code will read
    localWrite(LW_PRGNLCL_FALCON_DMAINFO_CTL,
        DRF_DEF(_PRGNLCL, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION, _FALSE));

    localWrite(LW_PRGNLCL_FALCON_DMATRFCMD, 0);

    UTF_IO_MOCK(LW_PRGNLCL_FALCON_DMATRFCMD,
                LW_PRGNLCL_FALCON_DMATRFCMD,
                dmatrfcmd_mock_read,
                dmatrfcmd_mock_write);

    // Force a read of the FULL bit before it shows up as empty,
    // and a read of the IDLE bit as FALSE before it shows up as TRUE
    num_dmatrfcmd_full_reads = 1;
    num_dmatrfcmd_idle_false_reads = 3;

    expected_dmatrfcmd = DRF_DEF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE) |
                DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dma_idx) |
                DRF_DEF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _FALSE);

    ret = fbdma_pa((uint64_t)dmem_buf,
        FB_ADDR,
        DMEM_BUF_SIZE,
        0,
        LW_FALSE
    );

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(total_xfer_size, DMEM_BUF_SIZE);
}

UT_CASE_DEFINE(DMA, dma_error,
    UT_CASE_SET_DESCRIPTION("Test fbdma_pa with an error condition")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5629073"))

/**
 * @testcasetitle{DMA_dma_error}
 * @testdependencies{}
 * @testdesign{
 *   1) Set up register mocking to simulate a condition where the DMA Controller reports an error state.
 *   2) Call the DMA Driver API to kick off a transfer and verify that it returns an error.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, dma_error)
{
    RET_TYPE ret;
    const uint8_t dma_idx = 0;

    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    // seed some registers with values that the DMA code will read
    localWrite(LW_PRGNLCL_FALCON_DMAINFO_CTL,
        DRF_DEF(_PRGNLCL, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION, _FALSE));

    localWrite(LW_PRGNLCL_FALCON_DMATRFCMD, 0);

    UTF_IO_MOCK(LW_PRGNLCL_FALCON_DMATRFCMD,
                LW_PRGNLCL_FALCON_DMATRFCMD,
                dmatrfcmd_mock_read,
                dmatrfcmd_mock_write);

    // Force a read of the ERROR bit as true
    num_dmatrfcmd_error_reads = 2;

    expected_dmatrfcmd = DRF_DEF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE) |
                DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dma_idx) |
                DRF_DEF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _FALSE);

    ret = fbdma_pa((uint64_t)dmem_buf,
        FB_ADDR,
        DMEM_BUF_SIZE,
        0,
        LW_FALSE
    );

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_FAULT);

    // Force a read of INTR_ERR_COMPLETION (NACK) from HW
    localWrite(LW_PRGNLCL_FALCON_DMAINFO_CTL,
        DRF_DEF(_PRGNLCL, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION, _TRUE));

    ret = fbdma_pa((uint64_t)dmem_buf,
        FB_ADDR,
        DMEM_BUF_SIZE,
        0,
        LW_FALSE
    );

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_BUSY);
}

UT_CASE_DEFINE(DMA, dma_bad_args,
    UT_CASE_SET_DESCRIPTION("Test fbdma_pa with bad args")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542993")
    UT_CASE_LINK_SPECS(""))

/**
 * @testcasetitle{DMA_dma_bad_args}
 * @testdependencies{}
 * @testdesign{
 *   1) Call fbdma_pa with arguments such that tcm_pa + size_bytes overflows. Verify that an error is returned.
 *   2) Call fbdma_pa with a tcm_pa outside of IMEM or DMEM. Verify that an error is returned.
 *   3) Call fbdma_pa with a size_bytes that is not a multiple of DMA_BLOCK_SIZE_MIN. Verify that an error is returned.
 *   4) Call fbdma_pa with size_bytes=0. Verify that a success is returned.
 *   5) Call fbdma_pa with an invalid dma_idx. Verify that an error is returned.
 *   6) Call fbdma_pa with arguments such that size_bytes + fb_offset overflows. Verify that an error is returned.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, dma_bad_args)
{
    RET_TYPE ret;

    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);
    // seed some registers with values that the DMA code will read
    localWrite(LW_PRGNLCL_FALCON_DMAINFO_CTL,
        DRF_DEF(_PRGNLCL, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION, _FALSE));

    localWrite(LW_PRGNLCL_FALCON_DMATRFCMD, 0);

    UTF_IO_MOCK(LW_PRGNLCL_FALCON_DMATRFCMD,
                LW_PRGNLCL_FALCON_DMATRFCMD,
                dmatrfcmd_mock_read,
                NULL);

    // tcm_pa + size_bytes overflow
    ret = fbdma_pa(0xFFFFFFFFFFFFFF00ULL,
        FB_ADDR,
        0x100ULL,
        0,
        LW_FALSE
    );
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    // tcm_pa outside of IMEM or DMEM
    ret = fbdma_pa(0x0ULL,
        FB_ADDR,
        DMEM_BUF_SIZE,
        0,
        LW_FALSE
    );
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    // misaligned sizebytes
    ret = fbdma_pa((uint64_t)dmem_buf,
        FB_ADDR,
        0xFF,
        0,
        LW_FALSE
    );
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    // size_bytes = 0
    ret = fbdma_pa((uint64_t)dmem_buf,
        FB_ADDR,
        0,
        0,
        LW_FALSE
    );
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // bad dma_idx
    ret = fbdma_pa((uint64_t)dmem_buf,
        FB_ADDR,
        DMEM_BUF_SIZE,
        20,
        LW_FALSE
    );
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

#if LWRISCV_FEATURE_SCP || TEST_SCPTOEXTMEM
    // size_bytes + fb_offset overflow in dma_xfer
    ret = fbdma_scp_to_extmem(
        0xF0,
        0xFFFFFFFFFFFFFFF0ULL,
        0
    );
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
#endif
}

#if LWRISCV_FEATURE_SCP || TEST_SCPTOEXTMEM
UT_CASE_DEFINE(DMA, fbdma_scp_to_extmem,
    UT_CASE_SET_DESCRIPTION("Test fbdma_scp_to_extmem")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542995")
    UT_CASE_LINK_SPECS(""))

/**
 * @testcasetitle{DMA_fbdma_scp_to_extmem}
 * @testdependencies{}
 * @testdesign{
 *   1) Call fbdma_scp_to_extmem
 *   2) Verify that the correct registers are written to facilitate an SCP-to-exmem transfer
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, fbdma_scp_to_extmem)
{
    RET_TYPE ret;
    const uint8_t dma_idx = 0;
    const uint64_t size_bytes = 256ULL;

    /*
     * Using the real SCP Bypass path requires first calling into the SCP driver to configure
     * SCPDMA for the transfer. In order to reduce dependencies on SCP driver and outside-of-peregrine
     * blocks, test this with mocked registers
     */
    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    // seed some registers with values that the DMA code will read
    localWrite(LW_PRGNLCL_FALCON_DMAINFO_CTL,
        DRF_DEF(_PRGNLCL, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION, _FALSE));

    localWrite(LW_PRGNLCL_FALCON_DMATRFCMD, 0);

    UTF_IO_MOCK(LW_PRGNLCL_FALCON_DMATRFCMD,
                LW_PRGNLCL_FALCON_DMATRFCMD,
                dmatrfcmd_mock_read,
                dmatrfcmd_mock_write);

    total_xfer_size = 0;
    expected_dmatrfcmd = DRF_DEF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE) |
                DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _SEC, 2) |
                DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dma_idx) |
                DRF_DEF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _FALSE);

    ret = fbdma_scp_to_extmem(
        FB_ADDR,
        size_bytes,
        dma_idx
    );

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(total_xfer_size, size_bytes);
}

UT_CASE_DEFINE(DMA, fbdma_scp_to_extmem_bad_args,
    UT_CASE_SET_DESCRIPTION("Test fbdma_scp_to_extmem with bad args")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542995"))

/**
 * @testcasetitle{DMA_fbdma_scp_to_extmem_bad_args}
 * @testdependencies{}
 * @testdesign{
 *   1) Call fbdma_scp_to_extmem with size_bytes=0. Verify that it returns success.
 *   2) Call fbdma_scp_to_extmem with a size_bytes that is not a multiple of DMA_BLOCK_SIZE_MIN. Verify that an error is returned.
 *   3) Call fbdma_scp_to_extmem with an invalid dma_idx. Verify that an error is returned.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(DMA, fbdma_scp_to_extmem_bad_args)
{
    RET_TYPE ret;
    const uint8_t dma_idx = 0;

    /*
     * Using the real SCP Bypass path requires first calling into the SCP driver to configure
     * SCPDMA for the transfer. In order to reduce dependencies on SCP driver and outside-of-peregrine
     * blocks, test this with mocked registers
     */
    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    //test with size_bytes = 0
    ret = fbdma_scp_to_extmem(
        FB_ADDR,
        0,
        dma_idx
    );
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    //Misaligned sizebytes
    ret = fbdma_scp_to_extmem(
        FB_ADDR,
        1,
        dma_idx
    );
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    //bad dma_idx
    ret = fbdma_scp_to_extmem(
        FB_ADDR,
        256,
        20
    );
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
}
#endif
