/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgislandgm10x.c
 * @brief  HAL-interface for the GM20X PGISLAND
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pwr_pri.h"
#include "rmbsiscratch.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objpg.h"
#include "pmu_didle.h"

#include "config/g_pg_private.h"

/* ------------------------- Public Functions ------------------------------- */

/* ------------------------- Macros and Defines ----------------------------- */
#define LW_SIZE_OF_BSI_DESCRIPTOR 5 // size of a descriptor in dwords
#define LW_BSI_DMEM0_OFFSET       2 // offset in dwords for DMEM0 in a phase descriptor

/*!
 * @brief Configures BSI RAM
 *
 * Writes to LW_PGC6_BSI_BOOTPHASES and LW_PGC6_BSI_CTRL as RM cannot access
 * these any more because of priv security.
 *
 * @param[in]  phasePointers     Configure phases in LW_PGC6_BSI_BOOTPHASES.
 */
void
pgIslandBsiRamConfig_GM20X
(
    LwU32 bsiBootPhaseInfo
)
{
    LwU32 val = 0;

    REG_WR32(FECS, LW_PGC6_BSI_BOOTPHASES, bsiBootPhaseInfo);

    //
    // This was being set initially in pgislandBSIRamValid_HAL,
    // but after GM20x for priv security, only PMU can write to
    // LW_PGC6_BSI_CTRL.
    //
    val = REG_RD32(FECS, LW_PGC6_BSI_CTRL);
    val = FLD_SET_DRF(_PGC6, _BSI_CTRL, _RAMVALID, _TRUE, val);
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, val);
}

/*!
 * @brief Read/write content of BSI RAM from offset
 *
 * @param[in/out] pBuf          Depends on read or write
 * @param[in]     sizeDwords    size to read from BSI ram
 * @param[in]     offset        offset from we want to read
 * @param[in]     bRead         direction of the transfer
 *
 * @return        FLCN_OK   : If read is possible from correct offset
 * @return        FLCN_ERROR : Not able to set correct offset
 */
FLCN_STATUS
pgIslandBsiRamRW_GM20X
(
    LwU32  *pBuf,
    LwU32  sizeDwords,
    LwU32  offset,
    LwBool bRead
)
{
    LwU32 i;
    LwU32 ramCtrl = 0;

    if ((offset + (sizeDwords * sizeof(LwU32)) - 1) >=
                       (pgIslandGetBsiRamSize_HAL(&Pg)))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, offset, ramCtrl);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _RAUTOINCR, _ENABLE, ramCtrl);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _ENABLE, ramCtrl);
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);

    for (i = 0; i < sizeDwords; i++)
    {
        if (bRead)
        {
            pBuf[i] = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
        }
        else
        {
            REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, pBuf[i]);
        }
    }

    return FLCN_OK;
}

/*!
 * @brief Get BSI RAM descriptor info of particular phase
 *
 * @param[in]  phaseId      Indicates the phase
 * @param[out] pPhaseDesc   Contains desc info about particular phase
 */
FLCN_STATUS
pgIslandBsiRamDescGet_GM20X
(
    LwU32 phaseId,
    RM_PMU_PG_ISLAND_PHASE_DESCRIPTOR *pPhaseDesc
)
{
    LwU32 offset;
    LwU32 index = 0;
    LwU32 buf[PGISLAND_DWORDS_PER_PHASE_DESC];

    offset = phaseId * sizeof(RM_PMU_PG_ISLAND_PHASE_DESCRIPTOR);

    if (pgIslandBsiRamRW_HAL(&Pg, buf, PGISLAND_DWORDS_PER_PHASE_DESC, offset, LW_TRUE) != FLCN_OK)
    {
        return FLCN_ERROR;
    }

    pPhaseDesc->imemSrcAddr   = DRF_VAL(_PGC6, _BSI_RAMDATA, _IMEM0_SRCADDR,  buf[index]);
    pPhaseDesc->imemSize      = DRF_VAL(_PGC6, _BSI_RAMDATA, _IMEM0_SIZE,     buf[index]);
    index++;

    pPhaseDesc->imemDestAddr  = DRF_VAL(_PGC6, _BSI_RAMDATA, _IMEM1_DESTADDR, buf[index]);
    pPhaseDesc->imemTag       = DRF_VAL(_PGC6, _BSI_RAMDATA, _IMEM1_TAG,      buf[index]);
    index++;

    pPhaseDesc->dmemSrcAddr   = DRF_VAL(_PGC6, _BSI_RAMDATA, _DMEM0_SRCADDR,  buf[index]);
    pPhaseDesc->dmemSize      = DRF_VAL(_PGC6, _BSI_RAMDATA, _DMEM0_SIZE,     buf[index]);
    index++;

    pPhaseDesc->dmemDestAddr  = DRF_VAL(_PGC6, _BSI_RAMDATA, _DMEM1_DESTADDR, buf[index]);
    index++;

    pPhaseDesc->privSrcAddr   = DRF_VAL(_PGC6, _BSI_RAMDATA, _PRIV_SRCADDR,   buf[index]);
    pPhaseDesc->numPrivWrites = DRF_VAL(_PGC6, _BSI_RAMDATA, _PRIV_NUMWRITES, buf[index]);
    index++;

    if (index != PGISLAND_DWORDS_PER_PHASE_DESC)
    {
        return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Check and patch descriptors if BRSS is present
 */
void
pgIslandPatchForBrss_GM20X
(
    LwU32 bsiBootPhaseInfo
)
{
    LwU32               ramCtrl     = 0;
    LwU32               val         = 0;
    LwU32               addr        = 0;
    LwU32               imemSrc     = 0;
    LwU32               scriptPhase = LW_PGC6_BSI_PHASE_OFFSET + LW_PGC6_BSI_PHASE_SCRIPT;
    // first check if we have UDE
    if (REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6STOP, bsiBootPhaseInfo) < scriptPhase)
    {
        return;
    }

    // Check the last dword in the BSI to see if BRSS is present
    addr = pgIslandGetBsiRamSize_HAL(&Pg) - sizeof(LwU32);
    ramCtrl = REG_RD32(FECS, LW_PGC6_BSI_RAMCTRL);
    val = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _DISABLE, val);
    val = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, addr, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, val);

    //
    // If the last dword matches the identifier, then we need to adjust
    // the devinit script phase descriptor because compaction size does not
    // account for the space.
    //
    if (REG_RD32(FECS, LW_PGC6_BSI_RAMDATA) == LW_BRSS_IDENTIFIER)
    {
        //
        // Each descriptor is 5 dwords. We want to adjust the dmem src address,
        // which is 15:0 in the third dword of the descriptor.
        // See manuals for more info on descriptor layout.
        //

        addr = ((scriptPhase * LW_SIZE_OF_BSI_DESCRIPTOR) + LW_BSI_DMEM0_OFFSET) * sizeof(LwU32);
        val = 0;
        val = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _DISABLE, val);
        val = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, addr, val);
        REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, val);

        // Parse the value
        val = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
        imemSrc = DRF_VAL(_PGC6, _BSI_RAMDATA, _DMEM0_SRCADDR, val);

        //
        // Subtract the size of the structure (because the ucode grows up)
        // TODO - check version
        //
        imemSrc -= sizeof(BSI_RAM_SELWRE_SCRATCH_V1);
        val = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMDATA, _DMEM0_SRCADDR, imemSrc, val);
        REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    }

    // Restore RAMCTRL
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);
}

/*!
 * @brief Reset pertinent BSI registers before setting it up for GCX
 */
void
pgIslandResetBsiSettings_GM20X()
{
    LwU32 val = 0;

    // Set RAMVALID to false to ilwalidate RAM contents
    val = REG_RD32(FECS, LW_PGC6_BSI_CTRL);
    val = FLD_SET_DRF(_PGC6, _BSI_CTRL, _RAMVALID, _FALSE, val);
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, val);

    // Reset bootphases
    REG_WR32(FECS, LW_PGC6_BSI_BOOTPHASES, 0);
}
