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
#include LWRISCV64_MANUAL_LOCAL_IO

// Get the MPU functions either from liblwriscv or libfsp.
// they are named the same in both places, but have different return types,
// hence this if/else. Once we drop support for liblwriscv, remove this.
#if defined(UTF_USE_LIBLWRISCV)
    #include <lwmisc.h>                // Non-safety DRF macros
    #include <liblwriscv/fbif.h>
    #include <liblwriscv/io.h>
#elif defined(UTF_USE_LIBFSP)
    #include <misc/lwmisc_drf.h>       // safety macros, needed for libfsp
    #include <fbdma/fbif.h>
    #include <cpu/io_local.h>
#else
    #error UTF_USE_LIBLWRISCV or UTF_USE_LIBFSP must be defined
#endif

/** \file
 * @testgroup{FBIF}
 * @testgrouppurpose{Testgroup purpose is to test the FBIF portion of the FBDMA driver in fbif.c}
 */
UT_SUITE_DEFINE(FBIF,
                UT_SUITE_SET_COMPONENT("FBIF")
                UT_SUITE_SET_DESCRIPTION("FBIF tests")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))

UT_CASE_DEFINE(FBIF, fbif_configure_aperture,
    UT_CASE_SET_DESCRIPTION("Test fbif_configure_aperture()")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542997"))

/**
 * @testcasetitle{FBIF_fbif_configure_aperture}
 * @testdependencies{}
 * @testdesign{
 *   1) call fbif_configure_aperture
 *   2) Verify that it writes the correct values into the registers
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(FBIF, fbif_configure_aperture)
{
    RET_TYPE ret;
    uint16_t i;

    const fbif_aperture_cfg_t cfgs []=
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
        },
        {
            .aperture_idx = 1,
            .target = LW_PRGNLCL_FBIF_TRANSCFG_TARGET_COHERENT_SYSMEM,
            .b_target_va = LW_TRUE,
            .l2c_wr = FBIF_TRANSCFG_L2C_L2_EVICT_FIRST,
            .l2c_rd = FBIF_TRANSCFG_L2C_L2_EVICT_LAST,
            .b_fbif_transcfg_wachk0_enable = LW_TRUE,
            .b_fbif_transcfg_wachk1_enable = LW_TRUE,
            .b_fbif_transcfg_rachk0_enable = LW_TRUE,
            .b_fbif_transcfg_rachk1_enable = LW_TRUE,
            .region_id = 2
        }
    };

    const uint32_t transcfg0_ev =
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _TARGET, cfgs[0].target) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE,
                ((cfgs[0].b_target_va) ?
                LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_VIRTUAL : LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_PHYSICAL)) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _L2C_WR, cfgs[0].l2c_wr) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _L2C_RD, cfgs[0].l2c_rd) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _WACHK0, cfgs[0].b_fbif_transcfg_wachk0_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _WACHK1, cfgs[0].b_fbif_transcfg_wachk1_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _RACHK0, cfgs[0].b_fbif_transcfg_rachk0_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _RACHK1, cfgs[0].b_fbif_transcfg_rachk1_enable ? 1 : 0);

    const uint32_t transcfg1_ev =
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _TARGET, cfgs[1].target) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE,
                ((cfgs[1].b_target_va) ?
                LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_VIRTUAL : LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_PHYSICAL)) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _L2C_WR, cfgs[1].l2c_wr) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _L2C_RD, cfgs[1].l2c_rd) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _WACHK0, cfgs[1].b_fbif_transcfg_wachk0_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _WACHK1, cfgs[1].b_fbif_transcfg_wachk1_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _RACHK0, cfgs[1].b_fbif_transcfg_rachk0_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _RACHK1, cfgs[1].b_fbif_transcfg_rachk1_enable ? 1 : 0);

    const uint32_t regioncfg_ev =
        DRF_NUM(_PRGNLCL, _FBIF_REGIONCFG, _T0, cfgs[0].region_id) |
        DRF_NUM(_PRGNLCL, _FBIF_REGIONCFG, _T1, cfgs[1].region_id);


    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    //seed the register with values before calling fbif_configure_aperture
    localWrite(LW_PRGNLCL_FBIF_REGIONCFG, 0);

    ret = fbif_configure_aperture(cfgs, 2);

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    UT_ASSERT_EQUAL_INT(localRead(LW_PRGNLCL_FBIF_TRANSCFG(cfgs[0].aperture_idx)), transcfg0_ev);
    UT_ASSERT_EQUAL_INT(localRead(LW_PRGNLCL_FBIF_TRANSCFG(cfgs[1].aperture_idx)), transcfg1_ev);
    UT_ASSERT_EQUAL_INT(localRead(LW_PRGNLCL_FBIF_REGIONCFG), regioncfg_ev);
}

UT_CASE_DEFINE(FBIF, fbif_configure_aperture_badargs,
    UT_CASE_SET_DESCRIPTION("Test fbif_configure_aperture() with bad args")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542997"))

/**
 * @testcasetitle{FBIF_fbif_configure_aperture_badargs}
 * @testdependencies{}
 * @testdesign{
 *   1) call fbif_configure_aperture with an invalid aperture_idx. Verify that it returns an error.
 *   2) call fbif_configure_aperture with an invalid target. Verify that it returns an error.
 *   3) call fbif_configure_aperture with an invalid l2c_wr. Verify that it returns an error.
 *   4) call fbif_configure_aperture with an invalid l2c_rd. Verify that it returns an error.
 *   5) call fbif_configure_aperture with an invalid Region_id. Verify that it returns an error.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(FBIF, fbif_configure_aperture_badargs)
{
    RET_TYPE ret;
    uint16_t i;

    const fbif_aperture_cfg_t cfgs_bad_aperture_idx []=
    {
        {
            .aperture_idx = 10,
            .target = FBIF_TRANSCFG_TARGET_LOCAL_FB,
            .b_target_va = LW_FALSE,
            .l2c_wr = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
            .l2c_rd = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
            .b_fbif_transcfg_wachk0_enable = LW_FALSE,
            .b_fbif_transcfg_wachk1_enable = LW_FALSE,
            .b_fbif_transcfg_rachk0_enable = LW_FALSE,
            .b_fbif_transcfg_rachk1_enable = LW_FALSE,
            .region_id = 0
        },
    };

    const fbif_aperture_cfg_t cfgs_bad_target []=
    {
        {
            .aperture_idx = 0,
            .target = FBIF_TRANSCFG_TARGET_COUNT,
            .b_target_va = LW_FALSE,
            .l2c_wr = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
            .l2c_rd = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
            .b_fbif_transcfg_wachk0_enable = LW_FALSE,
            .b_fbif_transcfg_wachk1_enable = LW_FALSE,
            .b_fbif_transcfg_rachk0_enable = LW_FALSE,
            .b_fbif_transcfg_rachk1_enable = LW_FALSE,
            .region_id = 0
        },
    };

    const fbif_aperture_cfg_t cfgs_bad_l2c_wr []=
    {
        {
            .aperture_idx = 0,
            .target = FBIF_TRANSCFG_TARGET_LOCAL_FB,
            .b_target_va = LW_FALSE,
            .l2c_wr = FBIF_TRANSCFG_L2C_COUNT,
            .l2c_rd = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
            .b_fbif_transcfg_wachk0_enable = LW_FALSE,
            .b_fbif_transcfg_wachk1_enable = LW_FALSE,
            .b_fbif_transcfg_rachk0_enable = LW_FALSE,
            .b_fbif_transcfg_rachk1_enable = LW_FALSE,
            .region_id = 0
        },
    };

    const fbif_aperture_cfg_t cfgs_bad_l2c_rd []=
    {
        {
            .aperture_idx = 0,
            .target = FBIF_TRANSCFG_TARGET_LOCAL_FB,
            .b_target_va = LW_FALSE,
            .l2c_wr = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
            .l2c_rd = FBIF_TRANSCFG_L2C_COUNT,
            .b_fbif_transcfg_wachk0_enable = LW_FALSE,
            .b_fbif_transcfg_wachk1_enable = LW_FALSE,
            .b_fbif_transcfg_rachk0_enable = LW_FALSE,
            .b_fbif_transcfg_rachk1_enable = LW_FALSE,
            .region_id = 0
        },
    };

    const fbif_aperture_cfg_t cfgs_bad_Region_id []=
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
            .region_id = 255
        },
    };

    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    //seed the register with values before calling fbif_configure_aperture
    localWrite(LW_PRGNLCL_FBIF_REGIONCFG, 0);

    ret = fbif_configure_aperture(NULL, 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = fbif_configure_aperture(cfgs_bad_aperture_idx, 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = fbif_configure_aperture(cfgs_bad_target, 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = fbif_configure_aperture(cfgs_bad_l2c_wr, 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = fbif_configure_aperture(cfgs_bad_l2c_rd, 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = fbif_configure_aperture(cfgs_bad_Region_id, 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
}
