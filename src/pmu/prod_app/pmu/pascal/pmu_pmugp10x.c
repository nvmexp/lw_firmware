/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_pmugp10x.c
 *          PMU HAL Functions for GP10x and later GPUs
 *
 * PMU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmusw.h"
#include "lwos_ovl_priv.h"
#include "linkersymbols.h"
#include "main.h"
#include "pmu_objpmu.h"
#include "dev_pwr_csb.h"

#include "config/g_pmu_private.h"

#ifdef UPROC_FALCON

/* ------------------------- Macros and Defines ----------------------------- */
#define PMU_NULL_SECTION_SIZE_IN_BLOCKS    1U

// NJ-TODO: Move to a manual addendum.
#define LW_CPWR_FALCON_DMVACTL_BOUND_ZERO  0x00000000U

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static LwU8 PmuNullSection_GP10X[DMEM_BLOCK_SIZE * PMU_NULL_SECTION_SIZE_IN_BLOCKS]
    GCC_ATTRIB_SECTION("nullSection", "PmuNullSection_GP10X");

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_pmuDmemVaGc6Copy_GP10X(void);

#endif // UPROC_FALCON

/* ------------------------- Public Functions ------------------------------- */
/*
 * @brief Initialize the DMEM_VA_BOUND
 *
 * @return      FLCN_OK     On success
 * @return      Others      Error return value from failing child function
 */
FLCN_STATUS
pmuDmemVaInit_GP10X
(
    LwU32 boundInBlocks
)
{
    FLCN_STATUS status = FLCN_OK;
#ifdef UPROC_FALCON
    LwU32 addr;
    LwU32 block;

    // Assure that the PmuNullSection_GP10X does not get eliminated by linker.
    PmuNullSection_GP10X[0]++;

    for (block = 0; block < boundInBlocks; block++)
    {
        addr = DMEM_IDX_TO_ADDR(block);
        falc_setdtag(addr, block);
    }

    // Entire DMEM must be pageable.
    REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FALCON_DMVACTL, _BOUND, _ZERO);

    // Enable SP to take values above 64kB
    REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FALCON_DMCYA, _LDSTVA_DIS, _FALSE);

    for (block = 0; block < PMU_NULL_SECTION_SIZE_IN_BLOCKS; block++)
    {
        falc_dmilw(block);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_GC6_DMEM_VA_COPY))
    {
        status = s_pmuDmemVaGc6Copy_GP10X();
    }
#endif // UPROC_FALCON

    return status;
}

/*
 * @brief Raise priv level masks so that RM doesn't interfere with PMU
 * operation until they are lowered again.
 */
void
pmuPreInitRaisePrivLevelMasks_GP10X(void)
{
    if (Pmu.bLSMode)
    {
        //
        // Only raise priv level masks when we're in booted in LS mode,
        // else we'll take away our own ability to reset the masks when we
        // unload.
        // Check if Priv Level 2 has write access before updating.
        //
        if (REG_FLD_TEST_DRF_DEF(CSB, _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK,
                         _WRITE_PROTECTION_LEVEL2, _ENABLE))
        {
            REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK,
                       _WRITE_PROTECTION_LEVEL0, _DISABLE);
        }

        REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FALCON_IRQTMR_PRIV_LEVEL_MASK,
                   _WRITE_PROTECTION_LEVEL0, _DISABLE);
    }
}

/*!
 * @brief Lower priv level masks when we unload
 */
void
pmuLowerPrivLevelMasks_GP10X(void)
{
    // Check if Priv Level 2 has write access before updating reset PLM.
    if (REG_FLD_TEST_DRF_DEF(CSB, _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK,
                     _WRITE_PROTECTION_LEVEL2, _ENABLE))
    {
        // NJ-TODO: Contains redundant read
        REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK,
                   _WRITE_PROTECTION_LEVEL0, _ENABLE);
    }

    REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FALCON_IRQTMR_PRIV_LEVEL_MASK,
               _WRITE_PROTECTION_LEVEL0, _ENABLE);
}

/* ------------------------- Private Functions ------------------------------ */
#ifdef UPROC_FALCON
/*!
 * @brief   Save/restore the original DMEM-VA surface across GC6 cycles.
 *
 * @note    Current implementation performs save/restore only on the statically
 *          allocated structures to minimize DMA traffic reducing exit penalty.
 *
 * @return  FLCN_OK     On success
 * @return  FLCN_ERROR  If gc6DmemCopy overlay size is insufficient
 */
static FLCN_STATUS
s_pmuDmemVaGc6Copy_GP10X(void)
{
    FLCN_STATUS status     = FLCN_OK;
    LwU32       copyOffset = _dmem_ovl_start_address[OVL_INDEX_DMEM(gc6DmemCopy)];
    LwU32       dmemBlock  = (LwU32)_dmem_va_bound;
    LwU32       dmemAddr   = DMEM_IDX_TO_ADDR(dmemBlock);
    LwU32       ovlSize;
    LwU32       ovlIdx;
    LwU32       vaSrc;
    LwU32       vaDst;

    // Save/restore not required for resident data (never corrupted).
    for (ovlIdx = (OVL_INDEX_OS_HEAP + 1);
         ovlIdx < OVL_INDEX_DMEM(gc6DmemCopy);
         ovlIdx++)
    {
        // Perform save/restore only if overlay contains static allocations.
        ovlSize = _dmem_ovl_size_lwrrent[ovlIdx];
        if (0U == ovlSize)
        {
            continue;
        }

        // Check if this is GC6 resume in which case we need to restore.
        if (DMA_ADDR_IS_ZERO(&PmuInitArgs.gc6Ctx))
        {
            // Save...
            vaSrc = _dmem_ovl_start_address[ovlIdx];
            vaDst = copyOffset;
        }
        else
        {
            // Restore...
            vaSrc = copyOffset;
            vaDst = _dmem_ovl_start_address[ovlIdx];
        }

        ovlSize     = LWUPROC_ALIGN_UP(ovlSize, DMEM_BLOCK_SIZE);
        copyOffset += ovlSize;

        // HALT implies that gc6DmemCopy overlay size has to be increased.
        if (copyOffset >
            (_dmem_ovl_start_address[OVL_INDEX_DMEM(gc6DmemCopy)] +
             _dmem_ovl_size_max[OVL_INDEX_DMEM(gc6DmemCopy)]))
        {
            status = FLCN_ERROR;
            PMU_BREAKPOINT();
            goto s_pmuDmemVaGc6Copy_GP10X_exit;
        }

        while (ovlSize > 0U)
        {
            ovlSize -= DMEM_BLOCK_SIZE;

            falc_dmread(vaSrc + ovlSize,
                DRF_NUM(_FLCN, _DMREAD, _PHYSICAL_ADDRESS, dmemAddr)   |
                DRF_DEF(_FLCN, _DMREAD, _ENC_SIZE, _256)               |
                DRF_DEF(_FLCN, _DMREAD, _SET_TAG, _YES));

            falc_dmwrite(vaDst + ovlSize,
                DRF_NUM(_FLCN, _DMWRITE, _PHYSICAL_ADDRESS, dmemAddr) |
                DRF_DEF(_FLCN, _DMWRITE, _ENC_SIZE, _256));
        }
    }

    falc_dmwait();
    falc_dmilw(dmemBlock);

s_pmuDmemVaGc6Copy_GP10X_exit:
    return status;
}
#endif // UPROC_FALCON
