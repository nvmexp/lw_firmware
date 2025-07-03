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
#include <ut/ut.h>
#include <ut/ut_case.h>
#include <ut/ut_assert.h>
#include "status-shim.h"
#include LWRISCV64_MANUAL_ADDRESS_MAP

// Get the MPU functions either from liblwriscv or libfsp.
// they are named the same in both places, but have different return types,
// hence this if/else. Once we drop support for liblwriscv, remove this.
#if defined(UTF_USE_LIBLWRISCV)
    #include <lwmisc.h>                // Non-safety DRF macros
    #include <liblwriscv/memutils.h>
#elif defined(UTF_USE_LIBFSP)
    #include <misc/lwmisc_drf.h>       // safety macros, needed for libfsp
    #include <fbdma/memutils.h>
#else
    #error UTF_USE_LIBLWRISCV or UTF_USE_LIBFSP must be defined
#endif

/** \file
 * @testgroup{MEMUTILS}
 * @testgrouppurpose{Testgroup purpose is to test the MEMUTILS portion of the FBDMA driver in memutils.c}
 */
UT_SUITE_DEFINE(MEMUTILS,
                UT_SUITE_SET_COMPONENT("MEMUTILS")
                UT_SUITE_SET_DESCRIPTION("MEMUTILS tests")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))

UT_CASE_DEFINE(MEMUTILS, memutils_riscv_pa_to_target_offset,
    UT_CASE_SET_DESCRIPTION("Test memutils_riscv_pa_to_target_offset")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542998"))

/**
 * @testcasetitle{MEMUTILS_memutils_riscv_pa_to_target_offset}
 * @testdependencies{}
 * @testdesign{
 *   1) Call memutils_riscv_pa_to_target_offset with an IMEM address
 *   2) Verify that it returns the RISCV_MEM_TARGET_IMEM target with the correct offset.
 *   3) repeat the above steps for DMEM, FB and SYSMEM
 *   4) Call memutils_riscv_pa_to_target_offset with a 0 address and verify that it returns an error.
 *   5) Call memutils_riscv_pa_to_target_offset with NULL p_target and p_offset aruments and verify that it returns success.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MEMUTILS, memutils_riscv_pa_to_target_offset)
{
    RET_TYPE ret;
    uint16_t i;

    riscv_mem_target_t target;
    uint64_t offset;
    const uint64_t offset_ev = 1234;

    ret = memutils_riscv_pa_to_target_offset(LW_RISCV_AMAP_IMEM_START + offset_ev, &target, &offset);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(target, RISCV_MEM_TARGET_IMEM);
    UT_ASSERT_EQUAL_UINT(offset, offset_ev);

    ret = memutils_riscv_pa_to_target_offset(LW_RISCV_AMAP_DMEM_START + offset_ev, &target, &offset);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(target, RISCV_MEM_TARGET_DMEM);
    UT_ASSERT_EQUAL_UINT(offset, offset_ev);

    ret = memutils_riscv_pa_to_target_offset(LW_RISCV_AMAP_FBGPA_START + offset_ev, &target, &offset);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(target, RISCV_MEM_TARGET_FBGPA);
    UT_ASSERT_EQUAL_UINT(offset, offset_ev);

#ifdef LW_RISCV_AMAP_SYSGPA_START
    ret = memutils_riscv_pa_to_target_offset(LW_RISCV_AMAP_SYSGPA_START + offset_ev, &target, &offset);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(target, RISCV_MEM_TARGET_SYSGPA);
    UT_ASSERT_EQUAL_UINT(offset, offset_ev);
#endif

    ret = memutils_riscv_pa_to_target_offset(0, &target, &offset);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = memutils_riscv_pa_to_target_offset(LW_RISCV_AMAP_IMEM_START + offset_ev, NULL, NULL);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
}

#if LWRISCV_HAS_FBIF
UT_CASE_DEFINE(MEMUTILS, memutils_riscv_pa_to_fbif_aperture,
    UT_CASE_SET_DESCRIPTION("Test memutils_riscv_pa_to_fbif_aperture()")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542999"))

/**
 * @testcasetitle{MEMUTILS_memutils_riscv_pa_to_fbif_aperture}
 * @testdependencies{}
 * @testdesign{
 *   1) Call memutils_riscv_pa_to_fbif_aperture with a FB address
 *   2) Verify that it returns the correct offset of this address withing the FB aperture.
 *   3) Call memutils_riscv_pa_to_fbif_aperture with a SYSMEM address
 *   4) Verify that it returns the correct offset of this address withing the SYSMEM aperture.
 *   5) Call memutils_riscv_pa_to_fbif_aperture with a 0 address. Verify that it returns an error.
 *   6) Call memutils_riscv_pa_to_fbif_aperture with an IMEM address. Verify that it returns an error.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MEMUTILS, memutils_riscv_pa_to_fbif_aperture)
{
    RET_TYPE ret;
    uint16_t i;

    fbif_transcfg_target_t target;
    uint64_t offset;
    const uint64_t offset_ev = 1234;

    ret = memutils_riscv_pa_to_fbif_aperture(LW_RISCV_AMAP_FBGPA_START + offset_ev, &target, &offset);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(target, FBIF_TRANSCFG_TARGET_LOCAL_FB);
    UT_ASSERT_EQUAL_UINT(offset, offset_ev);

#ifdef LW_RISCV_AMAP_SYSGPA_START
    ret = memutils_riscv_pa_to_fbif_aperture(LW_RISCV_AMAP_SYSGPA_START + offset_ev, &target, &offset);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(target, FBIF_TRANSCFG_TARGET_COHERENT_SYSTEM);
    UT_ASSERT_EQUAL_UINT(offset, offset_ev);
#endif

    ret = memutils_riscv_pa_to_fbif_aperture(0, &target, &offset);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = memutils_riscv_pa_to_fbif_aperture(LW_RISCV_AMAP_IMEM_START, &target, &offset);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
}
#endif
