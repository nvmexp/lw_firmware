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
#include LWRISCV64_MANUAL_ADDRESS_MAP
#include <lwriscv/status.h>
#include <lwriscv/fence.h>
#include <lwriscv/cache.h>
#include <liblwriscv/io_dio.h>
#include <dev_se_seb.h>

#if LWRISCV_PLATFORM_IS_CMOD
/* These 2 will be undefined because LWRISCV_HAS_PRI=n
 * give them dummy values to keep compiler from complaining when we include io_pri.h.
 * We won't be calling the functions that use them.
 */
#define FALCON_BASE 0
#define RISCV_BASE 0
#endif // LWRISCV_PLATFORM_IS_CMOD
#include <liblwriscv/io_pri.h>

#if LWRISCV_PLATFORM_IS_CMOD
//On cmodel, Accessing SCRATCH_GROUP_0 doesn't seem to work, but acessing LW_RISCV_AMAP_PRIV_START does
#define SCRATCH_GROUP_0(i) (i)
#elif defined LWRISCV_IS_ENGINE_gsp
#include <dev_gsp.h>
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
#define SCRATCH_GROUP_0(i) LW_PLWDEC_FALCON_COMMON_SCRATCH_GROUP_0(0, i)
#endif

//TODO: Some engines don't have a dcache (FSP). Make this a liblwriscv feature.
#define DCACHE_TEST_ENABLED 0

#ifdef LW_RISCV_AMAP_FBGPA_START
static void dummyFbAccess(void)
{
    volatile uint64_t * const fbStart = LW_RISCV_AMAP_FBGPA_START;
    uint64_t val = *fbStart;
    *fbStart = val;
}
#endif

UT_SUITE_DEFINE(CPL,
                UT_SUITE_SET_COMPONENT("CPL")
                UT_SUITE_SET_DESCRIPTION("CPU primitives layer tests")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))

UT_CASE_DEFINE(CPL, cplFence,
    UT_CASE_SET_DESCRIPTION("Test fences")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CPL, cplFence)
{
    bool ret;
    uint16_t i;

    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);

    /*
     * We have no way of confirming that the fences actually worked,
     * But at least we can confirm that they have not hung the core
     */
#ifdef SCRATCH_GROUP_0
    priWrite(SCRATCH_GROUP_0(0), 0);
#endif //SCRATCH_GROUP_0
    riscvLwfenceIO();

#ifdef LW_RISCV_AMAP_FBGPA_START
    dummyFbAccess();
#endif
    riscvLwfenceRW();

#ifdef SCRATCH_GROUP_0
    priWrite(SCRATCH_GROUP_0(0), 0);
#endif //SCRATCH_GROUP_0
#ifdef LW_RISCV_AMAP_FBGPA_START
    dummyFbAccess();
#endif
    riscvLwfenceRWIO();

#ifdef SCRATCH_GROUP_0
    priWrite(SCRATCH_GROUP_0(0), 0);
#endif //SCRATCH_GROUP_0
    riscvFenceIO();

#ifdef LW_RISCV_AMAP_FBGPA_START
    dummyFbAccess();
#endif
    riscvFenceRW();

#ifdef SCRATCH_GROUP_0
    priWrite(SCRATCH_GROUP_0(0), 0);
#endif //SCRATCH_GROUP_0
#ifdef LW_RISCV_AMAP_FBGPA_START
    dummyFbAccess();
#endif
    riscvFenceRWIO();
}

#if DCACHE_TEST_ENABLED
UT_CASE_DEFINE(CPL, cplCache,
    UT_CASE_SET_DESCRIPTION("Test cache ilwalidation")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CPL, cplCache)
{
    /*
     * We have no way of confirming that the cache ilwalidation actually worked,
     * But at least we can confirm that they have not hung the core
     */

    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);

    dcacheIlwalidatePaRange(LW_RISCV_AMAP_DMEM_START, LW_RISCV_AMAP_DMEM_START + 0x1000);

    dcacheIlwalidateVaRange(LW_RISCV_AMAP_DMEM_START, LW_RISCV_AMAP_DMEM_START + 0x1000);

    dcacheIlwalidate();

    icacheIlwalidate();  
}
#endif

#if LWRISCV_FEATURE_DIO && defined(LW_SSE_LWPKA_CORE_LOR_K0)
UT_CASE_DEFINE(CPL, dioTest,
    UT_CASE_SET_DESCRIPTION("Test DIO")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CPL, dioTest)
{
    LWRV_STATUS ret;
    DIO_PORT dioPort;
    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;
    uint32_t val = 0xDEADBEEF;
    uint32_t valRb = 0;

    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);

    ret = dioReadWrite(dioPort, DIO_OPERATION_WR, (uint32_t)LW_SSE_LWPKA_CORE_LOR_K0(0), &val);
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);

    ret = dioReadWrite(dioPort, DIO_OPERATION_RD, (uint32_t)LW_SSE_LWPKA_CORE_LOR_K0(0), &valRb);
    UT_ASSERT_EQUAL_UINT(ret, LWRV_OK);

    UT_ASSERT_EQUAL_UINT(val, valRb);
}
#endif // LWRISCV_HAS_DIO_SE
