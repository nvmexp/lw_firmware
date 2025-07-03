/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   main.c
 * @brief  Bootstub main.
 */

#include <engine.h>
#include <lwmisc.h>
#include <lwtypes.h>
#include <riscv_csr.h>

#include <dev_fuse.h>

#include "bootstub.h"

#include "debug.h"
#include "dmem_addrs.h"
#include "load.h"
#include "mpu.h"
#include "util.h"

#ifdef IS_SSP_ENABLED
#include "ssp.h"
#endif // IS_SSP_ENABLED


#if __riscv_xlen != 64
#error "Warning: bootstub has not been tested on non-64-bit platforms!"
#endif


// Signed boot parameters embedded by rvmkfmc.pl.
typedef struct LW_RISCV_BOOTSTUB_PARAMS
{
    // Pointer to the application's MPU settings (PA).
    const LW_RISCV_MPU_INFO *pMpuInfo;

    // Application entry-point (VA).
    const void *pEntryPoint;

    // Partition-switch return address (PA).
    const void *pReturnAddress;

    // Total size of the bootstub and parameters.
    LwU64 stubFootprint;
} LW_RISCV_BOOTSTUB_PARAMS;


#ifdef IS_FUSE_CHECK_ENABLED
/*!
 * @brief Checks that privsec is enabled in production elwironments.
 *
 * @return Void on success. Does not return on failure.
 */
static void
_checkFusing(void)
{
    // Only check fuse in production elwironments.
    if (FLD_TEST_DRF_ENG(_SCP, _CTL_STAT, _DEBUG_MODE, _DISABLED,
                         csbReadPA(ENGINE_REG(_SCP_CTL_STAT))))
    {
        // Ensure that privsec is enabled.
        if (!FLD_TEST_DRF(_FUSE_OPT, _PRIV_SEC_EN, _DATA, _YES,
                          bar0ReadPA(LW_FUSE_OPT_PRIV_SEC_EN)))
        {
            // Bad fusing!
            dbgExit(BOOTSTUB_ERROR_BAD_FUSING);
        }
    }
}
#endif // IS_FUSE_CHECK_ENABLED


/*!
 * @brief Bootstub main function.
 *
 * @param[in] pStubBase     Base address of the bootstub image.
 * @param[in] pBootParams   Pointer to the boot parameters.
 *
 * @return Does not return.
 */
__attribute__((noreturn, __optimize__("no-stack-protector")))
void
bootstubMain
(
    const void *pStubBase,
    const LW_RISCV_BOOTSTUB_PARAMS *pBootParams
)
{
    LW_DBG_STATE dbgState;

    // Enable interrupts at core (we have MEMERR configured to catch ASM errors)
    csr_set(LW_RISCV_CSR_SIE, DRF_NUM64(_RISCV, _CSR_SIE, _SEIE, 1));
    csr_set(LW_RISCV_CSR_SSTATUS, DRF_DEF64(_RISCV, _CSR_SSTATUS, _SIE, _ENABLE));

#ifdef IS_SSP_ENABLED
    // Prepare stack canary before calling any other functions.
    sspCanarySetup();
#endif // IS_SSP_ENABLED

#ifdef IS_FUSE_CHECK_ENABLED
    // Check for fusing errors (security bar).
    _checkFusing();
#endif // IS_FUSE_CHECK_ENABLED

    // Startup message.
    dbgInit(&dbgState);
    dbgPuts(LEVEL_ALWAYS, "!Bootstub starting!\n");
    dbgPuts(LEVEL_INFO, "Bootstub base: 0x");
    dbgPutHex(LEVEL_INFO, 16, (LwU64)pStubBase);
    dbgPuts(LEVEL_INFO, "\nParameter base: 0x");
    dbgPutHex(LEVEL_INFO, 16, (LwU64)pBootParams);
    dbgPuts(LEVEL_INFO, "\nCarveout size: 0x");
    dbgPutHex(LEVEL_INFO, 16, (LwU64)pBootParams->stubFootprint);
    dbgPuts(LEVEL_INFO, "\nMPU info: 0x");
    dbgPutHex(LEVEL_INFO, 16, (LwU64)pBootParams->pMpuInfo);
    dbgPuts(LEVEL_INFO, "\nEntry point: 0x");
    dbgPutHex(LEVEL_INFO, 16, (LwU64)pBootParams->pEntryPoint);
    dbgPuts(LEVEL_INFO, "\nReturn address: 0x");
    dbgPutHex(LEVEL_INFO, 16, (LwU64)pBootParams->pReturnAddress);
    dbgPuts(LEVEL_INFO, "\n");

    // Sanity check to catch discrepancies between this file and rvmkfmc.pl.
    const LwU64 expectedFootprint =
        (LwUPtr)pBootParams + sizeof(*pBootParams) - (LwUPtr)pStubBase;
    if (pBootParams->stubFootprint != expectedFootprint)
    {
        dbgPuts(LEVEL_CRIT, "Error: Bootstub footprint mismatch.\n");
        dbgPuts(LEVEL_CRIT, "Expected size: 0x");
        dbgPutHex(LEVEL_CRIT, 16, expectedFootprint);
        dbgPuts(LEVEL_CRIT, "\nActual size: 0x");
        dbgPutHex(LEVEL_CRIT, 16, pBootParams->stubFootprint);
        dbgPuts(LEVEL_CRIT, "\n");

        dbgExit(BOOTSTUB_ERROR_FOOTPRINT_MISMATCH);
    }

    // Validate and apply MPU settings (if any).
    if (!mpuInit(pBootParams->pMpuInfo, pStubBase, pBootParams->stubFootprint))
    {
        dbgPuts(LEVEL_CRIT, "Error: Invalid MPU settings.\n");
        dbgExit(BOOTSTUB_ERROR_LOAD_FAILURE);
    }

    // Final status print.
    dbgPuts(LEVEL_ALWAYS, "Booting application...\n");
    dbgDisable();

    // Disable interrupts in SSTATUS and SIE, but leave MEMERR enabled.
    csr_clear(LW_RISCV_CSR_SIE, DRF_NUM64(_RISCV, _CSR_SIE, _SEIE, 1));
    csr_clear(LW_RISCV_CSR_SSTATUS, DRF_DEF64(_RISCV, _CSR_SSTATUS, _SIE, _ENABLE));

    // Scrub stack/registers and jump to the application entry-point.
    bootLoad((LwUPtr)pBootParams->pEntryPoint);

    // This should be unreachable.
    dbgExit(BOOTSTUB_ERROR_LOAD_FAILURE);
}

/*!
 * @brief Obtains the partition-switch return address for the application.
 *
 * @param[in] pBootParams   Pointer to the boot parameters.
 *
 * @return The partition-switch return address.
 *
 * This function is used to avoid hard-coding field offsets in start.S.
 */
__attribute__((__optimize__("no-stack-protector")))
const void *
bootstubGetReturnAddress
(
    const LW_RISCV_BOOTSTUB_PARAMS *pBootParams
)
{
    return pBootParams->pReturnAddress;
}
