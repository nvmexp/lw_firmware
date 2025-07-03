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
    #include <liblwriscv/tfbif.h>
    #include <liblwriscv/io.h>
#elif defined(UTF_USE_LIBFSP)
    #include <misc/lwmisc_drf.h>       // safety macros, needed for libfsp
    #include <fbdma/tfbif.h>
    #include <cpu/io_local.h>
#else
    #error UTF_USE_LIBLWRISCV or UTF_USE_LIBFSP must be defined
#endif

/** \file
 * @testgroup{TFBIF}
 * @testgrouppurpose{Testgroup purpose is to test the TFBIF portion of the FBDMA driver in tfbif.c}
 */
UT_SUITE_DEFINE(TFBIF,
                UT_SUITE_SET_COMPONENT("TFBIF")
                UT_SUITE_SET_DESCRIPTION("TFBIF tests")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))

UT_CASE_DEFINE(TFBIF, tfbif_configure_aperture,
    UT_CASE_SET_DESCRIPTION("Test tfbif_configure_aperture()")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542997"))

/**
 * @testcasetitle{TFBIF_tfbif_configure_aperture}
 * @testdependencies{}
 * @testdesign{
 *   1) call tfbif_configure_aperture
 *   2) Verify that the correct values are written to the TFBIF registers
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(TFBIF, tfbif_configure_aperture)
{
    RET_TYPE ret;
    uint16_t i;

    const tfbif_aperture_cfg_t cfgs []=
    {
        {
            .aperture_idx = 0, // Must match the values in the ev callwlation below
            .swid = 1,
            .b_vpr = false,
            .aperture_id = 2,
        },
        {
            .aperture_idx = 7, // Must match the values in the ev callwlation below
            .swid = 2,
            .b_vpr = true,
            .aperture_id = 3,
        },
    };

    const uint32_t transcfg_ev =
        DRF_NUM(_PRGNLCL, _TFBIF_TRANSCFG, _ATT0_SWID, cfgs[0].swid) |
        DRF_NUM(_PRGNLCL, _TFBIF_TRANSCFG, _ATT7_SWID, cfgs[1].swid);
    const uint32_t regioncfg_ev =
        DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T0_VPR, cfgs[0].b_vpr) |
        DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T7_VPR, cfgs[1].b_vpr);
    const uint32_t regioncfg1_ev =
        DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG1, _T0_APERT_ID, cfgs[0].aperture_id); // aperture_idx <= 3
    const uint32_t regioncfg2_ev =
        DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG2, _T7_APERT_ID, cfgs[1].aperture_id); // aperture_idx > 3

    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    //seed the register with values before calling tfbif_configure_aperture
    localWrite(LW_PRGNLCL_TFBIF_TRANSCFG, 0);
    localWrite(LW_PRGNLCL_TFBIF_REGIONCFG, 0);
    localWrite(LW_PRGNLCL_TFBIF_REGIONCFG1, 0);
    localWrite(LW_PRGNLCL_TFBIF_REGIONCFG2, 0);

    ret = tfbif_configure_aperture(cfgs, 2);

    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    UT_ASSERT_EQUAL_INT(localRead(LW_PRGNLCL_TFBIF_TRANSCFG), transcfg_ev);
    UT_ASSERT_EQUAL_INT(localRead(LW_PRGNLCL_TFBIF_REGIONCFG), regioncfg_ev);
    UT_ASSERT_EQUAL_INT(localRead(LW_PRGNLCL_TFBIF_REGIONCFG1), regioncfg1_ev);
    UT_ASSERT_EQUAL_INT(localRead(LW_PRGNLCL_TFBIF_REGIONCFG2), regioncfg2_ev);
}

UT_CASE_DEFINE(TFBIF, tfbif_configure_aperture_badargs,
    UT_CASE_SET_DESCRIPTION("Test tfbif_configure_aperture() with bad args")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542997"))

/**
 * @testcasetitle{TFBIF_tfbif_configure_aperture_badargs}
 * @testdependencies{}
 * @testdesign{
 *   1) call tfbif_configure_aperture with an invalid aperture_idx. Verify that it returns an error.
 *   2) call tfbif_configure_aperture with an invalid swid. Verify that it returns an error.
 *   3) call tfbif_configure_aperture with an invalid aperture_id. Verify that it returns an error.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(TFBIF, tfbif_configure_aperture_badargs)
{
    RET_TYPE ret;
    uint16_t i;

    const tfbif_aperture_cfg_t cfgs_bad_aperture_idx []=
    {
        {
            .aperture_idx = 10,
            .swid = 1,
            .b_vpr = false,
            .aperture_id = 2,
        },
    };

    const tfbif_aperture_cfg_t cfgs_bad_swid []=
    {
        {
            .aperture_idx = 0,
            .swid = 255,
            .b_vpr = false,
            .aperture_id = 2,
        },
    };

    const tfbif_aperture_cfg_t cfgs_bad_aperture_id []=
    {
        {
            .aperture_idx = 0,
            .swid = 255,
            .b_vpr = false,
            .aperture_id = 2,
        },
    };

    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    //seed the register with values before calling tfbif_configure_aperture
    localWrite(LW_PRGNLCL_TFBIF_TRANSCFG, 0);
    localWrite(LW_PRGNLCL_TFBIF_REGIONCFG, 0);
    localWrite(LW_PRGNLCL_TFBIF_REGIONCFG1, 0);
    localWrite(LW_PRGNLCL_TFBIF_REGIONCFG2, 0);

    ret = tfbif_configure_aperture(NULL, 2);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = tfbif_configure_aperture(cfgs_bad_aperture_idx, 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = tfbif_configure_aperture(cfgs_bad_swid, 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = tfbif_configure_aperture(cfgs_bad_aperture_id, 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
}
