/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file main.c
 */

/* ------------------------- System Includes -------------------------------- */
#include <drivers/drivers.h>
#include <sbilib/sbilib.h>
#include <syslib/idle.h>
#include <sections.h>
#include <riscv_csr.h>
#include <drivers/odp.h>
#include <lwriscv/print.h>
#include <engine.h>

#include "pmusw.h"

#include "dev_pwr_falcon_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "dbgprintf.h"
#include "pmu_objpmu.h"
#include "pmu_objlsf.h"
#include "pmu_objvbios.h"

#if SANITIZER_COV_INSTRUMENT
ct_assert(PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_SANITIZER_COV));
#include "lib_sanitizercov.h"
#endif  // SANITIZER_COV_INSTRUMENT

#include "config/g_ic_private.h"
#if(IC_MOCK_FUNCTIONS_GENERATED)
#include "config/g_ic_mock.c"
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 *  Copy of RISCV bootargs
 */
static sysKERNEL_DATA RM_PMU_BOOT_PARAMS bootArgs;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Implementation --------------------------------- */
int pmuMain(LwU64 bootargsPa, RM_PMU_BOOT_PARAMS *pBootArgsUnsafe,
            LwUPtr elfAddrPhys, LwU64 elfSize, LwUPtr loadBase, LwU64 wprId)
    GCC_ATTRIB_SECTION("imem_resident", "pmuMain");

/*!
 * Main RISCV entry-point for the RTOS application.
 *
 * @param[in]  bootargsPa       The physical addr of the RISCV bootargs.
 * @param[in]  pBootArgsUnsafe  The virtual addr/pointer to the RISCV bootargs.
 * @param[in]  elfAddrPhys      The physical addr of the ELF file.
 * @param[in]  elfSize          The size in bytes of the ELF file.
 * @param[in]  loadBase         The physical addr of the load region.
 * @param[in]  wprId            The Write-Protected Region id.
 *
 * @return zero upon success; non-zero negative number upon failure
 */
GCC_ATTRIB_NO_STACK_PROTECT()
int
pmuMain
(
    LwU64               bootargsPa,
    RM_PMU_BOOT_PARAMS *pBootArgsUnsafe,
    LwUPtr              elfAddrPhys,
    LwU64               elfSize,
    LwUPtr              loadBase,
    LwU64               wprId
)
{
    FLCN_STATUS status;
    LwU64 riscvPhysAddrBase;
    VBIOS_FRTS_CONFIG frtsConfig;

    // Ensure halt interrupt is enabled to crash properly if needed
    icHaltIntrEnable_HAL();

#if SANITIZER_COV_INSTRUMENT
    sanitizerCovInit();
#endif  // SANITIZER_COV_INSTRUMENT

    // Sanity check size, this is / may be unselwred memory
    if (pBootArgsUnsafe->riscv.bl.size != sizeof(RM_PMU_BOOT_PARAMS))
    {
        appHalt();
    }

    // Worst that can happen here - we read junk or get read exception
    bootArgs = *pBootArgsUnsafe;

    // Check version
    if (bootArgs.riscv.bl.version != RM_RISCV_BOOTLDR_VERSION)
    {
        appHalt();
    }

    // Init posted writes
    csr_set(LW_RISCV_CSR_CFG, DRF_DEF64(_RISCV, _CSR_CFG, _UPOSTIO, _TRUE));

    tlsInit();

    // Ensure print interrupt is enabled for debug to work
    icRiscvSwgen1IntrEnable_HAL();

    // Ensure ICD interrupt is enabled in case we want resumable BPs early on
    icRiscvIcdIntrEnable_HAL();

    // Make sure priv lockdown is released if it hasn't been released yet
    riscvReleasePrivLockdown();

    // Init debug buffer and queue
    status = debugInit();
    if (status != FLCN_OK)
    {
        appHalt();
    }

    // Init exceptions
    status = exceptionInit();
    if (status != FLCN_OK)
    {
        appHalt();
    }

    // Disable external interrupts early on RISCV.
    lwrtosENTER_CRITICAL();

    // Read the FRTS config, so that it can be used with mmInit
    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS))
    {
        status = vbiosFrtsConfigRead_HAL(&VbiosPreInit, &frtsConfig);
        if (status != FLCN_OK)
        {
            appHalt();
        }
    }

    // Initialize memory subsystem first
    status = mmInit(loadBase, elfAddrPhys, wprId);
    if (status != FLCN_OK)
    {
        appHalt();
    }

    // Setup PMU apertures
    lsfInitApertureSettings_HAL(&(Lsf.hal));

    // Init DMA (needs to be before ODP init!)
    if ((loadBase >= LW_RISCV_AMAP_SYSGPA_START) &&
        (loadBase < LW_RISCV_AMAP_SYSGPA_END))
    {
        status = dmaInit(RM_PMU_DMAIDX_PHYS_SYS_COH);
        riscvPhysAddrBase = LW_RISCV_AMAP_SYSGPA_START;
    }
    else if ((loadBase >= LW_RISCV_AMAP_FBGPA_START) &&
             (loadBase < LW_RISCV_AMAP_FBGPA_END))
    {
        status = dmaInit(RM_PMU_DMAIDX_UCODE);
        riscvPhysAddrBase = LW_RISCV_AMAP_FBGPA_START;
    }
    else
    {
        // Bases do not match any known configuration.
        // After exceptionInit we can have proper crashes with PMU_HALT!
        PMU_HALT();
    }

    if (status != FLCN_OK)
    {
        PMU_HALT();
    }

    // Init ODP
#ifdef ON_DEMAND_PAGING_BLK
    if (!odpInit(UPROC_SECTION_dmem_osHeap_REF, riscvPhysAddrBase))
    {
        PMU_HALT();
    }
#else // ON_DEMAND_PAGING_BLK
#error "PMU RISC-V requires ODP!"
#endif // ON_DEMAND_PAGING_BLK

    //
    // At this point ODP should start working, and this string literal
    // (located in GLOBAL_RODATA) will get paged in!
    //
    dbgPrintf(LEVEL_INFO, "Initializing PMU RISC-V environment...\n");

    // Init symbol resolver with ELF data
    symbolInit(elfAddrPhys, elfSize, wprId);

    // Init trace buffer
    status = traceInit();
    if (status != FLCN_OK)
    {
        PMU_HALT();
    }

    // Initialize coredump
    coreDumpInit(bootArgs.riscv.rtos.coreDumpPhys, bootArgs.riscv.rtos.coreDumpSize);

    // Enable counters from userspace
    csr_set(LW_RISCV_CSR_SCOUNTEREN,
            DRF_NUM64(_RISCV_CSR, _SCOUNTEREN, _TM, 1) |
            DRF_NUM64(_RISCV_CSR, _SCOUNTEREN, _IR, 1));

    // Disable SLCG (WAR to fix time CSR not latched to PTIME: http://lwbugs/2543570)
#ifdef HPMCOUNTER_SLCG_WAR
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CMSDEC, _FALCON_CG2, _SLCG_FALCON_TSYNC, _DISABLED);
#endif // HPMCOUNTER_SLCG_WAR

    dbgPrintf(LEVEL_ALWAYS, "Initializing PMU application\n");

    // Verify that we can't run in level 1 or level 3
    status = pmuPrivLevelValidate_HAL();
    if (status != FLCN_OK)
    {
        PMU_HALT();
    }

    // Self bind PMU context. Moved from RM due to lockdown
    pmuCtxBind_HAL(&Pmu, bootArgs.pmu.ctxBindAddr);

    //
    // Now that ODP (and everything else) has been initialized, copy over the
    // FRTS config to the VBIOS object so that it's available for the reset of
    // pre-init
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS))
    {
        VBIOS_FRTS *pFrts;

        status = vbiosFrtsGet(&VbiosPreInit, &pFrts);
        if ((status != FLCN_OK) ||
            (pFrts == NULL))
        {
            PMU_HALT();
        }

        pFrts->config = frtsConfig;
    }

    return initPmuApp(&(bootArgs.pmu));
}

/*!
 * @brief      Initialize the IDLE Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
idlePreInitTask(void)
{
    FLCN_STATUS status;

    //
    // RISCV has a custom idle task, task_idle, defined in syslib/idle/idle.c.
    // We no longer create it in xPortInitialize(), instead we create it here.
    //
    OSTASK_CREATE(status, PMU, _IDLE);

    return status;
}
