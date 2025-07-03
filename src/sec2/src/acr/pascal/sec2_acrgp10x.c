/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_acrgp10x.c
 * @brief  Get WPR and VPR region details by reading MMU registers and
 *         programs/clears the CBC base register.
 */

/* ------------------------ System includes -------------------------------- */
#include "sec2sw.h"
#include "lwos_dma_hs.h"

/* ------------------------ Application includes --------------------------- */
#include "sec2_objsec2.h"
#include "acr.h"
#include "sec2_objacr.h"
#include "sec2_bar0_hs.h"
#include "lib_lw64.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "config/g_acr_private.h"
#include "g_acrlib_hal.h"

#include "dev_pwr_pri.h"

#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

/* ------------------------ Macros ------------------------------ */

// Below are temporary defines for enabling SEC2 ACRLIB
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_SEC2_ACRLIB                           LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_RESERVED /*23:23*/
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_SEC2_ACRLIB_ENABLE                    0x00000001
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_SEC2_ACRLIB_DISABLE                   0x00000000

/* ------------------------ Global variables ------------------------------- */
static LwU32 xmemStore[FLCN_IMEM_BLK_SIZE_IN_BYTES/sizeof(LwU32)]
    GCC_ATTRIB_ALIGN(FLCN_IMEM_BLK_SIZE_IN_BYTES)
    GCC_ATTRIB_SECTION("dmem_acr", "xmemStore");

/*!
 * Read SCI SW Interrupt register to check, if GPU is in GC6 exit
 */
LwBool
acrGetGpuGc6ExitStatus_GP10X(void)
{
    return FLD_TEST_DRF(_PGC6_SCI, _SW_INTR0_STATUS_LWRR, _GC6_EXIT, _PENDING,
                        (REG_RD32(BAR0, LW_PGC6_SCI_SW_INTR0_STATUS_LWRR)));
}

/*!
 * Write PMU BSI TDY4TRANS register to trigger next SCI phase
 */
void
acrTriggerNextSCIPhase_GP10X(void)
{
    REG_WR32(BAR0, LW_PPWR_PMU_PMU2BSI_RDY4TRANS, 0x0);
}

/*!
 * @brief Use priv access to load target falcon memory.
 *        Supports loading into both IMEM and DMEM of target falcon from FB.
 *        Expects size to be 256 multiple.
 *
 * @param[in] dstOff       Destination offset in either target falcon DMEM or IMEM
 * @param[in] fbOff        Offset from fbBase
 * @param[in] sizeInBytes  Number of bytes to be transferred
 * @param[in] regionID     ACR region ID to be used for this transfer
 * @param[in] bIsDstImem   TRUE if destination is IMEM
 * @param[in] pFlcnCfg     Falcon config
 *
 */
FLCN_STATUS
acrPrivLoadTargetFalcon_GP10X
(
    LwU32               dstOff,
    LwU32               fbOff,
    LwU32               sizeInBytes,
    LwU32               regionID,
    LwU8                bIsDstImem,
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    FLCN_STATUS status        = FLCN_ERROR;
    LwU32       bytesXfered   = 0;
    LwU32       tag;
    LwU32       i;

    //
    // Sanity checks
    // Only does 256B transfers
    //
    if ((!VAL_IS_ALIGNED(sizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES)) ||
        (!VAL_IS_ALIGNED(dstOff, FLCN_IMEM_BLK_SIZE_IN_BYTES))      ||
        (!VAL_IS_ALIGNED(fbOff, FLCN_IMEM_BLK_SIZE_IN_BYTES)))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Setup the IMEMC/DMEMC registers
    if (bIsDstImem)
    {
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMC(0),
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_OFFS, 0) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_AINCW, 1));
    }
    else
    {
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMC(0),
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_OFFS, 0) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_AINCW, 1));
    }

    {
        tag = 0;
        while (bytesXfered < sizeInBytes)
        {
            // Read in the next block
            status = dmaRead_hs((void*)xmemStore, &Acr.wprRegionAddress,
                                         Acr.wprOffset + fbOff + bytesXfered,
                                         FLCN_IMEM_BLK_SIZE_IN_BYTES);
            if (FLCN_OK != status)
            {
                return status;
            }

            if (bIsDstImem)
            {
                // Setup the tags for the instruction memory.
                acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMT(0), REF_NUM(LW_PFALCON_FALCON_IMEMT_TAG, tag));
                for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / sizeof(LwU32); i++)
                {
                    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMD(0), xmemStore[i]);
                }
                tag++;
            }
            else
            {
                for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / sizeof(LwU32); i++)
                {
                    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMD(0), xmemStore[i]);
                }
            }
            bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;
        }
    }

    return status;
}

