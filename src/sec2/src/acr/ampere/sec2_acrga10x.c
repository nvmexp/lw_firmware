/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_acrga10x.c
 * @brief  ACR interfaces for AMPERE and later chips.
 */

/* ------------------------ System includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_posted_write.h"
#include "lwos_dma_hs.h"

/* ------------------------ Application includes --------------------------- */
#include "sec2_objsec2.h"
#include "sec2_objacr.h"
#include "dev_sec_csb.h"
#include "dev_gc6_island.h"
#include "dev_sec_csb_addendum.h"
#include "config/g_acr_private.h"
#include "config/g_sec2_hal.h"
#include "g_acrlib_hal.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
static LwU32 xmemStore[FLCN_IMEM_BLK_SIZE_IN_BYTES/sizeof(LwU32)]
    GCC_ATTRIB_ALIGN(FLCN_IMEM_BLK_SIZE_IN_BYTES)
    GCC_ATTRIB_SECTION("dmem_acr", "xmemStore");

/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

//
// TODO-aranjan : Lwrrently we have separate APIs to update different
//    states of GPCCS bootstrapping (STARTED, DONE etc). We need this as
//    the state defines are chip specific, so can be used only inside a
//    HAL interface. We need to give more thought on how to unify them
//    with state passed as an argument (Bug 200562079 is tracking this).
//

/*!
 * @brief API to set the GPCCS bootstrap state to STARTED
 */
void
acrGpcRgGpccsBsStateUpdateStarted_GA10X(void)
{
    LwU32 val;

    // Update the GPCCS bootstrap state as STARTED
    val = REG_RD32(CSB, LW_CSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO);
    val = FLD_SET_DRF(_CSEC_GPC_RG, _GPCCS_BOOTSTRAP_INFO,
                      _STATE, _STARTED, val);
    REG_WR32_STALL(CSB, LW_CSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO, val);
}

/*!
 * @brief API to set the GPCCS bootstrap state to DONE
 */
void
acrGpcRgGpccsBsStateUpdateDone_GA10X(void)
{
    LwU32 val;

    // Update the GPCCS bootstrap state as DONE
    val = REG_RD32(CSB, LW_CSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO);
    val = FLD_SET_DRF(_CSEC_GPC_RG, _GPCCS_BOOTSTRAP_INFO,
                      _STATE, _DONE, val);
    REG_WR32_STALL(CSB, LW_CSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO, val);
}

/*!
 * @brief API to update the GPCCS bootstrap error code
 */
void
acrGpcRgGpccsBsErrorCodeUpdate_GA10X
(
    LwU32 status
)
{
    LwU32 val;

    // Update the GPCCS bootstrap error code
    val = REG_RD32(CSB, LW_CSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO);
    val = FLD_SET_DRF_NUM(_CSEC_GPC_RG, _GPCCS_BOOTSTRAP_INFO,
                          _ERR_CODE, status, val);
    REG_WR32_STALL(CSB, LW_CSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO, val);
}

/*!
 * @brief Use NON BLOCKING priv access to load target falcon memory.
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
acrPrivLoadTargetFalcon_GA10X
(
    LwU32               dstOff,
    LwU32               fbOff,
    LwU32               sizeInBytes,
    LwU32               regionID,
    LwU8                bIsDstImem,
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    FLCN_STATUS flcnStatus    = FLCN_ERROR;
    LwU32       bytesXfered   = 0;
    LwU32       tag;
    LwU32       i;

    POSTED_WRITE_INIT();

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
        acrlibFlcnRegWriteNonBlocking_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMC(0),
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_OFFS, 0) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_AINCW, 1));
    }
    else
    {
        acrlibFlcnRegWriteNonBlocking_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMC(0),
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_OFFS, 0) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_AINCW, 1));
    }

    {
        tag = 0;
        while (bytesXfered < sizeInBytes)
        {
            // Read in the next block
            CHECK_FLCN_STATUS(dmaRead_hs((void*)xmemStore, &Acr.wprRegionAddress,
                                         Acr.wprOffset + fbOff + bytesXfered,
                                         FLCN_IMEM_BLK_SIZE_IN_BYTES));

            if (bIsDstImem)
            {
                // Setup the tags for the instruction memory.
                acrlibFlcnRegWriteNonBlocking_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMT(0), REF_NUM(LW_PFALCON_FALCON_IMEMT_TAG, tag));
                for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / sizeof(LwU32); i++)
                {
                    acrlibFlcnRegWriteNonBlocking_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMD(0), xmemStore[i]);
                }
                tag++;
            }
            else
            {
                for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / sizeof(LwU32); i++)
                {
                    acrlibFlcnRegWriteNonBlocking_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMD(0), xmemStore[i]);
                }
            }
            bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;
        }
    }

    POSTED_WRITE_END(LW_CSEC_MAILBOX(RM_SEC2_MAILBOX_ID_TASK_ACR));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check if it's IFR driven GC6 exit by checking ramvalid and GC6 exit interrupt
 */
LwBool
acrIsBsiRamValid_GA10X(void)
{
    return FLD_TEST_DRF(_PGC6, _BSI_CTRL, _RAMVALID, _TRUE, REG_RD32(BAR0, LW_PGC6_BSI_CTRL));
}

