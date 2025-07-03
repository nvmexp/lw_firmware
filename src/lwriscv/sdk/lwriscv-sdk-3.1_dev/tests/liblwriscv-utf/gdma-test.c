/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwmisc.h>
#include <stdint.h>
#include <stdbool.h>
#include <test-macros.h>
#include <regmock.h>
#include <ut/ut.h>
#include <ut/ut_case.h>
#include <ut/ut_assert.h>
#include LWRISCV64_MANUAL_ADDRESS_MAP
#include <stddef.h>
#include <liblwriscv/gdma.h>
#include <dev_ext_devices.h>

#if LWRISCV_PLATFORM_IS_CMOD
//On cmodel, Accessing SCRATCH_GROUP_0 doesn't seem to work, but acessing LW_RISCV_AMAP_PRIV_START does
#define SCRATCH_GROUP_0(i) (i)
#elif defined LWRISCV_IS_ENGINE_gsp
#include <dev_gsp.h>
#include <dev_sec_pri.h>
#define SCRATCH_GROUP_0(i) LW_PGSP_FALCON_COMMON_SCRATCH_GROUP_0(i)
#elif defined LWRISCV_IS_ENGINE_pmu
#include <dev_pwr_pri.h>
#define SCRATCH_GROUP_0(i) LW_PPWR_FALCON_COMMON_SCRATCH_GROUP_0(i)
#elif defined LWRISCV_IS_ENGINE_sec
#include <dev_sec_pri.h>
#define SCRATCH_GROUP_0(i) LW_PSEC_FALCON_COMMON_SCRATCH_GROUP_0(i)
#elif defined LWRISCV_IS_ENGINE_fsp
#include <dev_fsp_pri.h>
#define SCRATCH_GROUP_0(i) LW_PFSP_FALCON_COMMON_SCRATCH_GROUP_0(i)
#elif defined LWRISCV_IS_ENGINE_lwdec
#include <dev_lwdec_pri.h>
#define SCRATCH_GROUP_0(i) LW_PLWDEC_FALCON_COMMON_SCRATCH_GROUP_0(i)
#endif

#define BUF_ALIGNMENT 64
#define BUF_SIZE  64

//linker symbols
extern char _imem_buffer_start[];

// buffers used for transfers: DMEM, external, and PRI
static uint8_t *dmem_buf1;
static uint8_t *dmem_buf2;
static uint8_t *imem_buf;
static volatile uint8_t *ext_buf;
static volatile uint8_t *ext_buf2;
#ifdef SCRATCH_GROUP_0
static volatile uint32_t *pri_buf;
#endif //SCRATCH_GROUP_0
#if !LWRISCV_PLATFORM_IS_CMOD
static volatile uint32_t *eeprom_buf;
#endif //!LWRISCV_PLATFORM_IS_CMOD

// a reference buffer which will be populated with the expected values. Used for comparison.
static uint8_t *buf_ref;

static void init_buffers(void)
{
    const unsigned mid = BUF_SIZE/2;

    for (unsigned i = 0; i < BUF_SIZE; i++)
    {
        dmem_buf1[i] = i % 256;
        dmem_buf2[i] = 256 - (i % 256);
        imem_buf[i] = 256 - (i % 256);
        buf_ref[i] = 0xBB;
        ext_buf[i] = 0xEE;
        ext_buf2[i] = 0xFF;
    }

#ifdef SCRATCH_GROUP_0
    for (unsigned i = 0; i < (BUF_SIZE/4); i++)
    {
        // fill it up with the same bytes as buf_ref, so that when we do a sparse transfer
        // to bug_pri (stride mode used so that not all dest bytes written), the non-written
        // bytes match those in the ref buffer
        pri_buf[i] = 0xBBBBBBBB;
    }
#endif //SCRATCH_GROUP_0
}

#ifdef SCRATCH_GROUP_0
static void copy_to_pri(uint32_t *src)
{
    for (unsigned i = 0; i < (BUF_SIZE/4); i++)
    {
        pri_buf[i] = src[i];
    }
}

static void copy_from_pri(uint32_t *dest)
{
    for (unsigned i = 0; i < (BUF_SIZE/4); i++)
    {
        dest[i] = pri_buf[i];
    }
}
#endif // SCRATCH_GROUP_0

static void create_ref_buf(gdma_addr_cfg_t *cfg, volatile uint8_t *src, volatile uint8_t *dest, uint32_t length)
{
    uint32_t stride_src;
    uint32_t stride_dest;
    uint32_t wrap_src = (cfg->src_addr_mode == GDMA_ADDR_MODE_WRAP) ? ((cfg->wrap_len+1u)*4) : 0xFFFFFFFF;
    uint32_t wrap_dest = (cfg->dest_addr_mode == GDMA_ADDR_MODE_WRAP) ? ((cfg->wrap_len+1u)*4) : 0xFFFFFFFF;

    uint32_t src_idx=0;
    uint32_t dest_idx=0;

    switch (cfg->src_addr_mode)
    {
        case GDMA_ADDR_MODE_INC:
        case GDMA_ADDR_MODE_WRAP:
            stride_src = 4;
            break;
        case GDMA_ADDR_MODE_FIX:
            stride_src = 0;
            break;
        case GDMA_ADDR_MODE_STRIDE:
            stride_src = (4u << cfg->stride_log_len);
            break;
        default:
            UT_FAIL("bad src_addr_mode\n");
            return;
    }

    switch (cfg->dest_addr_mode)
    {
        case GDMA_ADDR_MODE_INC:
        case GDMA_ADDR_MODE_WRAP:
            stride_dest = 4;
            break;
        case GDMA_ADDR_MODE_FIX:
            stride_dest = 0;
            break;
        case GDMA_ADDR_MODE_STRIDE:
            stride_dest = (4u << cfg->stride_log_len);
            break;
        default:
            UT_FAIL("bad dest_addr_mode\n");
            return;
    }

    while (length)
    {
        //transfer in 4-byte chunks
        dest[dest_idx] = src[src_idx];
        dest[dest_idx+1] = src[src_idx+1];
        dest[dest_idx+2] = src[src_idx+2];
        dest[dest_idx+3] = src[src_idx+3];

        dest_idx += stride_dest;
        src_idx  += stride_src;

        if (dest_idx == wrap_dest)
        {
            dest_idx = 0;
        }
        if (src_idx == wrap_src)
        {
            src_idx = 0;
        }

        length -= 4;
    }
}

static void gdma_test_setup(void)
{
    LWRV_STATUS ret;
    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);

    //Set Up the buffers used by the tests
    dmem_buf1 = (uint32_t *) utfMallocAligned(BUF_SIZE, BUF_ALIGNMENT);
    dmem_buf2 = (uint32_t *) utfMallocAligned(BUF_SIZE, BUF_ALIGNMENT);
    imem_buf = (uint32_t *) _imem_buffer_start;
    buf_ref = (uint32_t *) utfMallocAligned(BUF_SIZE, BUF_ALIGNMENT);

    UT_ASSERT_NOT_NULL(dmem_buf1);
    UT_ASSERT_NOT_NULL(dmem_buf2);
    UT_ASSERT_NOT_NULL(buf_ref);

    ext_buf = (uint8_t *) (LW_RISCV_AMAP_FBGPA_START);
    ext_buf2 = (uint8_t *) (LW_RISCV_AMAP_FBGPA_START + BUF_SIZE);

#ifdef SCRATCH_GROUP_0
    // 64 bytes of scratch registers
    pri_buf = (uint32_t *) (LW_RISCV_AMAP_PRIV_START + SCRATCH_GROUP_0(0));
#endif //SCRATCH_GROUP_0

#if !LWRISCV_PLATFORM_IS_CMOD
    eeprom_buf = (uint32_t *) (LW_RISCV_AMAP_PRIV_START + LW_PROM_DATA(0));
#endif //!LWRISCV_PLATFORM_IS_CMOD

    // Do some basic channel configuration
    const gdma_channel_priv_cfg_t cfg_priv = {
        .b_virtual = LW_FALSE,
        .asid = 0
    };

    ret = gdma_cfg_channel_priv(0, &cfg_priv);
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);

    const gdma_channel_cfg_t cfg = {
        .rr_weight = 0xF,
        .max_in_flight = LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG_MAX_INFLIGHT_UNLIMITED,
        .b_irq_en = LW_FALSE,
        .b_memq = LW_FALSE
    };

    ret = gdma_cfg_channel(0, &cfg);
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
}


UT_SUITE_DEFINE(GDMA,
                UT_SUITE_SET_COMPONENT("GDMA")
                UT_SUITE_SET_DESCRIPTION("gdma tests")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(gdma_test_setup)
                UT_SUITE_TEARDOWN_HOOK(NULL))

UT_CASE_DEFINE(GDMA, sync_xfer,
    UT_CASE_SET_DESCRIPTION("Synchronous transfers")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(GDMA, sync_xfer)
{
    LWRV_STATUS ret;

    //Synchronous DMEM-to-DMEM xfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)dmem_buf1, //src
        (uint64_t)dmem_buf2, //dest
        BUF_SIZE,           //length
        NULL,               //gdma_addr_cfg_t *p_addr_cfg
        0,                  //chan_idx
        0,                  //sub_chan_idx
        LW_TRUE,            //b_wait_complete
        LW_FALSE            //b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf1, dmem_buf2, BUF_SIZE), 0);

    //Synchronous DMEM-to-DMEM xfer with alignment of only 4
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)dmem_buf1+4, //src
        (uint64_t)dmem_buf2+4, //dest
        BUF_SIZE-4,           //length
        NULL,               //gdma_addr_cfg_t *p_addr_cfg
        0,                  //chan_idx
        0,                  //sub_chan_idx
        LW_TRUE,            //b_wait_complete
        LW_FALSE            //b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf1+4, dmem_buf2+4, BUF_SIZE-4), 0);

    //Synchronous DMEM-to-FB xfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)dmem_buf1, //src
        (uint64_t)ext_buf,   //dest
        BUF_SIZE,           //length
        NULL,               //gdma_addr_cfg_t *p_addr_cfg,
        0,                  //chan_idx,
        0,                  //sub_chan_idx
        LW_TRUE,            // b_wait_complete,
        LW_FALSE            // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf1, ext_buf, BUF_SIZE), 0);

    //Synchronous fb-to-dmem transfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)ext_buf,     //src
        (uint64_t)dmem_buf1,   //dest
        BUF_SIZE,     //length
        NULL,             //gdma_addr_cfg_t *p_addr_cfg,
        0,                //chan_idx,
        0,                //sub_chan_idx
        LW_TRUE,          // b_wait_complete,
        LW_FALSE          // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf1, ext_buf, BUF_SIZE), 0);

    //Synchronous fb-to-imem transfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)ext_buf,     //src
        (uint64_t)imem_buf,   //dest
        BUF_SIZE,         //length
        NULL,             //gdma_addr_cfg_t *p_addr_cfg,
        0,                //chan_idx,
        0,                //sub_chan_idx
        LW_TRUE,          // b_wait_complete,
        LW_FALSE          // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(imem_buf, ext_buf, BUF_SIZE), 0);

    //Synchronous IMEM-to-FB xfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)imem_buf, //src
        (uint64_t)ext_buf,   //dest
        BUF_SIZE,           //length
        NULL,               //gdma_addr_cfg_t *p_addr_cfg,
        0,                  //chan_idx,
        0,                  //sub_chan_idx
        LW_TRUE,            // b_wait_complete,
        LW_FALSE            // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(imem_buf, ext_buf, BUF_SIZE), 0);

    //Synchronous FB-to-FB xfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)ext_buf, //src
        (uint64_t)ext_buf2,   //dest
        BUF_SIZE,           //length
        NULL,               //gdma_addr_cfg_t *p_addr_cfg,
        0,                  //chan_idx,
        0,                  //sub_chan_idx
        LW_TRUE,            // b_wait_complete,
        LW_FALSE            // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(ext_buf, ext_buf2, BUF_SIZE), 0);

#if !LWRISCV_PLATFORM_IS_CMOD
/* Eeprom access broken on NET20
    //Synchronous EEPROM-to-DMEM xfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)eeprom_buf,//src
        (uint64_t)dmem_buf1, //dest
        BUF_SIZE,           //length
        NULL,               //gdma_addr_cfg_t *p_addr_cfg,
        0,                  //chan_idx,
        0,                  //sub_chan_idx
        LW_TRUE,            // b_wait_complete,
        LW_FALSE            // b_irq_en
    );

    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(eeprom_buf, dmem_buf1, BUF_SIZE), 0);

    //Synchronous EEPROM-to-FB xfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)eeprom_buf,//src
        (uint64_t)ext_buf,   //dest
        BUF_SIZE,           //length
        NULL,               //gdma_addr_cfg_t *p_addr_cfg,
        0,                  //chan_idx,
        0,                  //sub_chan_idx
        LW_TRUE,            // b_wait_complete,
        LW_FALSE            // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(eeprom_buf, ext_buf, BUF_SIZE), 0);
*/
#endif
}

UT_CASE_DEFINE(GDMA, async_xfer,
    UT_CASE_SET_DESCRIPTION("asynchronous transfer")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(GDMA, async_xfer)
{
    LWRV_STATUS ret;
    uint32_t req_produce;
    uint32_t req_consume;
    uint32_t req_complete;
    uint32_t reg;
    uint32_t poll_count = 10000;

    //asynchronous dmem-to-dmem transfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)dmem_buf1,
        (uint64_t)dmem_buf2,
        BUF_SIZE,
        NULL,     //gdma_addr_cfg_t *p_addr_cfg,
        0, //chan_idx,
        0,                //sub_chan_idx
        LW_TRUE, // b_wait_complete,
        LW_FALSE // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);

    //poll completion
    do {
        ret = gdma_get_channel_status(0, &req_produce, &req_consume, &req_complete);
        UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    } while(req_produce != req_complete && poll_count--);

    UT_ASSERT_NOT_EQUAL_UINT(poll_count, 0);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf1, dmem_buf2, BUF_SIZE), 0);

    //asynchronous dmem-to-dmem transfer, with interrupts
    const gdma_channel_cfg_t cfg = {
        .rr_weight = 0xF,
        .max_in_flight = LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG_MAX_INFLIGHT_UNLIMITED,
        .b_irq_en = LW_TRUE,
        .b_memq = LW_FALSE
    };

    ret = gdma_cfg_channel(0, &cfg);
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);

    UT_ASSERT_EQUAL_UINT(localRead(LW_PRGNLCL_GDMA_IRQSTAT), 0);

    //asynchronous dmem-to-dmem transfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)dmem_buf1,
        (uint64_t)dmem_buf2,
        BUF_SIZE,
        NULL,     //gdma_addr_cfg_t *p_addr_cfg,
        0,        //chan_idx,
        0,        //sub_chan_idx
        LW_TRUE,  // b_wait_complete,
        LW_FALSE  // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);

    //poll completion
    do {
        reg = localRead(LW_PRGNLCL_GDMA_IRQSTAT);
    } while((DRF_VAL(_PRGNLCL, _GDMA_IRQSTAT, _CHAN0, reg) != LW_PRGNLCL_GDMA_IRQSTAT_CHAN0_TRUE) && (poll_count--));

    UT_ASSERT_NOT_EQUAL_UINT(poll_count, 0);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf1, dmem_buf2, BUF_SIZE), 0);
}

UT_CASE_DEFINE(GDMA, bad_xfer,
    UT_CASE_SET_DESCRIPTION("bad transfer")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(GDMA, bad_xfer)
{
    LWRV_STATUS ret;
    uint8_t error_cause;
    uint32_t error_id;

    //transfer to a bad address and recover
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)0xDEADBEEFBEEFDEA0,  //src
        (uint64_t)dmem_buf1,            //dest
        BUF_SIZE,                      //length
        NULL,                          //gdma_addr_cfg_t *p_addr_cfg
        0,                             //chan_idx
        0,                             //sub_chan_idx
        LW_TRUE,                       // b_wait_complete
        LW_FALSE                       // b_irq_en
    );
    UT_ASSERT_NOT_EQUAL_UINT(ret, LWRV_OK);

    ret = gdma_get_channel_status(0, NULL, NULL, NULL);
    UT_ASSERT_NOT_EQUAL_UINT(ret, LWRV_OK);

    ret = gdma_get_error_and_reset_channel(0, &error_cause, &error_id);
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_UINT(error_cause, LW_PRGNLCL_GDMA_CHAN_STATUS_ERROR_CAUSE_PA_FAULT);
}

#if defined(SCRATCH_GROUP_0) 
#if !LWRISCV_PLATFORM_IS_CMOD && defined(LWRISCV_IS_ENGINE_gsp)
UT_CASE_DEFINE(GDMA, mmio_attribs,
    UT_CASE_SET_DESCRIPTION("check mmio_attribs")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

// note: this test will only pass once. After clearing the PLM permissions, we can not re-enable them.
UT_CASE_RUN(GDMA, mmio_attribs)
{
    LWRV_STATUS ret;

    const uint64_t plm = LW_RISCV_AMAP_PRIV_START + LW_PSEC_FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK;
    uint32_t plm_init_value = priRead(plm);
    const uint8_t pl = 3;

    utf_printf("plm_init_value %x\n", plm_init_value);
    pri_buf = (uint32_t *) (LW_RISCV_AMAP_PRIV_START + LW_PSEC_FALCON_COMMON_SCRATCH_GROUP_0(0));

    //Enable r/w access only for PL i
    priWrite(plm,
        DRF_DEF(_PSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _ALL_SOURCES_ENABLED) |
        DRF_NUM(_PSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, _READ_PROTECTION, 1<<pl) |
        DRF_NUM(_PSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, _WRITE_PROTECTION, 1<<pl));

    //Synchronous DMEM-to-DMEM xfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)dmem_buf1, //src
        (uint64_t)pri_buf,   //dest
        BUF_SIZE,           //length
        NULL,               //gdma_addr_cfg_t *p_addr_cfg
        0,                  //chan_idx
        0,                  //sub_chan_idx
        LW_TRUE,            //b_wait_complete
        LW_FALSE            //b_irq_en
    );
    //From the GDMA register perspective, it will look like this succeeded but nothing will get copied over

    //restore init value before doing any asserts
    priWrite(plm, plm_init_value);
    copy_from_pri((uint32_t *)dmem_buf2);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf1, dmem_buf2, BUF_SIZE), 0);

    //Disable for all
    priWrite(plm, DRF_DEF(_PSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _ALL_SOURCES_ENABLED));

    //Synchronous DMEM-to-DMEM xfer
    init_buffers();
    ret = gdma_xfer_reg (
        (uint64_t)dmem_buf1, //src
        (uint64_t)pri_buf, //dest
        BUF_SIZE,           //length
        NULL,               //gdma_addr_cfg_t *p_addr_cfg
        0,                  //chan_idx
        0,                  //sub_chan_idx
        LW_TRUE,            //b_wait_complete
        LW_FALSE            //b_irq_en
    );
    //From the GDMA register perspective, it will look like this succeeded but nothing will get copied over

    //restore init value before doing any asserts
    priWrite(plm, plm_init_value);
    copy_from_pri((uint32_t *)dmem_buf2);

    UT_ASSERT_NOT_EQUAL_INT(utf_memcmp(dmem_buf1, dmem_buf2, BUF_SIZE), 0);
}
#endif //!LWRISCV_PLATFORM_IS_CMOD && defined(LWRISCV_IS_ENGINE_gsp)

UT_CASE_DEFINE(GDMA, complex_addr_mode,
    UT_CASE_SET_DESCRIPTION("transfers with complex addr modes")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(GDMA, complex_addr_mode)
{
    LWRV_STATUS ret;
    //synchronous pri-to-dmem transfer, fixed address in src
    gdma_addr_cfg_t addr_cfg = {
        .src_addr_mode = GDMA_ADDR_MODE_FIX,
        .dest_addr_mode = GDMA_ADDR_MODE_INC,
        .wrap_len = 0,
        .stride_log_len = 0
    };

    init_buffers();
    copy_to_pri((uint32_t *)dmem_buf1);
    create_ref_buf(&addr_cfg, dmem_buf1, buf_ref, BUF_SIZE);
    ret = gdma_xfer_reg (
        (uint64_t)pri_buf,   //src
        (uint64_t)dmem_buf2, //dest
        BUF_SIZE,         //length
        &addr_cfg,         //gdma_addr_cfg_t *p_addr_cfg,
        0,                //chan_idx,
        0,                //sub_chan_idx
        LW_TRUE,          // b_wait_complete,
        LW_FALSE          // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf2, buf_ref, BUF_SIZE), 0);

    //synchronous pri-to-dmem transfer, wrapping address in src
    init_buffers();
    addr_cfg.src_addr_mode = GDMA_ADDR_MODE_WRAP;
    addr_cfg.wrap_len = 3; //wrap every 16 bytes
    copy_to_pri((uint32_t *)dmem_buf1);
    create_ref_buf(&addr_cfg, dmem_buf1, buf_ref, BUF_SIZE);
    ret = gdma_xfer_reg (
        (uint64_t)pri_buf,     //src
        (uint64_t)dmem_buf2,     //dest
        BUF_SIZE,     //length
        &addr_cfg,         //gdma_addr_cfg_t *p_addr_cfg,
        0,                //chan_idx,
        0,                //sub_chan_idx
        LW_TRUE,          // b_wait_complete,
        LW_FALSE          // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf2, buf_ref, BUF_SIZE), 0);

    //synchronous dmem-to-pri transfer, wrapping address in dest
    init_buffers();
    addr_cfg.src_addr_mode = GDMA_ADDR_MODE_INC;
    addr_cfg.dest_addr_mode = GDMA_ADDR_MODE_WRAP;
    addr_cfg.wrap_len = 3; //wrap every 16 bytes
    create_ref_buf(&addr_cfg, dmem_buf1, buf_ref, BUF_SIZE);

    ret = gdma_xfer_reg (
        (uint64_t)dmem_buf1,     //src
        (uint64_t)pri_buf,     //dest
        BUF_SIZE,     //length
        &addr_cfg,         //gdma_addr_cfg_t *p_addr_cfg,
        0,                //chan_idx,
        0,                //sub_chan_idx
        LW_TRUE,          // b_wait_complete,
        LW_FALSE          // b_irq_en
    );
    copy_from_pri((uint32_t *)dmem_buf2);
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf2, buf_ref, BUF_SIZE), 0);

    //synchronous pri-to-dmem transfer, stride address in src
    init_buffers();
    addr_cfg.src_addr_mode = GDMA_ADDR_MODE_STRIDE;
    addr_cfg.dest_addr_mode = GDMA_ADDR_MODE_INC;
    addr_cfg.stride_log_len = 2; //A DWORD every 16 bytes
    copy_to_pri((uint32_t *)dmem_buf1);
    create_ref_buf(&addr_cfg, dmem_buf1, buf_ref, BUF_SIZE/(1u << addr_cfg.stride_log_len));

    ret = gdma_xfer_reg (
        (uint64_t)pri_buf,     //src
        (uint64_t)dmem_buf2,     //dest
        BUF_SIZE/(1u << addr_cfg.stride_log_len),   //length
        &addr_cfg,         //gdma_addr_cfg_t *p_addr_cfg,
        0,                //chan_idx,
        0,                //sub_chan_idx
        LW_TRUE,          // b_wait_complete,
        LW_FALSE          // b_irq_en
    );
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf2, buf_ref, BUF_SIZE/(1u << addr_cfg.stride_log_len)), 0);

    //synchronous dmem-to-pri transfer, stride address in dest
    init_buffers();
    addr_cfg.src_addr_mode = GDMA_ADDR_MODE_INC;
    addr_cfg.dest_addr_mode = GDMA_ADDR_MODE_STRIDE;
    addr_cfg.stride_log_len = 2; //A DWORD every 16 bytes
    create_ref_buf(&addr_cfg, dmem_buf1, buf_ref, BUF_SIZE/(1u << addr_cfg.stride_log_len));

    ret = gdma_xfer_reg (
        (uint64_t)dmem_buf1,     //src
        (uint64_t)pri_buf,     //dest
        BUF_SIZE/(1u << addr_cfg.stride_log_len),   //length
        &addr_cfg,         //gdma_addr_cfg_t *p_addr_cfg,
        0,                //chan_idx,
        0,                //sub_chan_idx
        LW_TRUE,          // b_wait_complete,
        LW_FALSE          // b_irq_en
    );
    copy_from_pri((uint32_t *)dmem_buf2);
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);
    UT_ASSERT_EQUAL_INT(utf_memcmp(dmem_buf2, buf_ref, BUF_SIZE), 0);
}
#endif //defined(SCRATCH_GROUP_0) 
