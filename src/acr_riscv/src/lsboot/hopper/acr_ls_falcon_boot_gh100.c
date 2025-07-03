/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_ls_falcon_boot_gh100.c
 */
/* ------------------------ System Includes -------------------------------- */
#include "acr.h"
#include "acrdrfmacros.h"

/* ------------------------ Application Includes --------------------------- */
#include "bootrom_pkc_parameters.h"
#include "dev_sec_pri.h"
#include "dev_falcon_second_pri.h"
#include "dev_falcon_v4.h"
#include "dev_falcon_v4_addendum.h"
#include "dev_falcon_second_pri.h"
#include "dev_fbif_v4.h"
#include "dev_sec_pri.h"
#include "dev_lwdec_pri.h"
#include "dev_gsp.h"
#include "dev_top.h"
#include "dev_master.h"
#include "dev_pwr_pri.h"
#include "dev_fbfalcon_pri.h"
#include "hwproject.h"
#include "dev_smcarb_addendum.h"
#include "dev_gsp_addendum.h"
#include "dev_graphics_nobundle.h"
#include "dev_gc6_island.h"
#include "dev_riscv_pri.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_gpc_addendum.h"
#include "mmu/mmucmn.h"
#include <liblwriscv/print.h>
#include "config/g_acr_private.h"
#include "dev_sec_addendum.h"
#include "dev_gsp_addendum.h"
#include "dev_lwdec_addendum.h"

#ifdef ACR_RISCV_LS
#include "dev_riscv_pri.h"
#endif



/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */

// Starting from Ampere, LW_PMC_ENABLE is getting deprecated and move to LW_PMC_DEVICE_ENABLE(i) to disable/enable Falcon device.
#ifdef LW_PMC_DEVICE_ENABLE
    #define READ_DEVICE_ENABLE_REG()  ACR_REG_RD32(BAR0, LW_PMC_DEVICE_ENABLE(pFlcnCfg->pmcEnableRegIdx))
    #define WRITE_DEVICE_ENABLE_REG() ACR_REG_WR32(BAR0, LW_PMC_DEVICE_ENABLE(pFlcnCfg->pmcEnableRegIdx), data);
#else
    #define READ_DEVICE_ENABLE_REG()  ACR_REG_RD32(BAR0, LW_PMC_ENABLE)
    #define WRITE_DEVICE_ENABLE_REG() ACR_REG_WR32(BAR0, LW_PMC_ENABLE, data);
#endif

// Max time to wait for RISCV BR return code update
#define ACR_TIMEOUT_OF_150US_FOR_RISCV_BR_RETCODE_UPDATE        (150*1000)

#define ACR_TIMEOUT_FOR_RESET (10 * 1000) // 10 us

// Global Variables
#define DIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))

/* ------------------------ External Definitions --------------------------- */
extern ACR_DMA_PROP  g_dmaProp;
extern LwU8  g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
extern LwU8  g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE];
extern ACR_DMA_PROP     g_dmaProp;

/*!
 * @brief Program Dmem range registers
 */
static ACR_STATUS
_acrProgramDmemRange
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;
    LwU32      rdata  = 0;

    //
    // Setup DMEM carveouts
    // ((endblock<<16)|startblock)
    // Just opening up one last block
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFindTotalDmemBlocks_HAL(pAcr, pFlcnCfg, &data));
    data = data - 1;
    data = (data<<16)|(data);
    if (pFlcnCfg->bIsBoOwner)
    {
        ACR_REG_WR32(PRGNLCL, pFlcnCfg->range0Addr, data);
        rdata = ACR_REG_RD32(PRGNLCL, pFlcnCfg->range0Addr);
    }
    else
    {
        ACR_REG_WR32(BAR0, pFlcnCfg->range0Addr, data);
        rdata = ACR_REG_RD32(BAR0, pFlcnCfg->range0Addr);
    }

    //
    // This is to make sure RANGE0 is programmable..
    // If it is not, open up DMEM to make sure we dont break
    // the existing SW.. TODO: Remove once RANGE0 becomes
    // programmable..
    //
    if (rdata != data)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK,
                                   ACR_PLMASK_READ_L0_WRITE_L0));
    }
    
    return status;
}



/*!
 * @brief Set up SEC2-specific registers
 */
void
acrSetupSec2Registers_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32 plmValue = 0;
    ACR_REG_WR32(BAR0, LW_PSEC_FBIF_CTL2_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    ACR_REG_WR32(BAR0, LW_PSEC_BLOCKER_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);

    // Setting LW_PSEC_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK to ACR_PLMASK_READ_L0_WRITE_L2
    plmValue = ACR_REG_RD32(BAR0, LW_PSEC_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PSEC, _CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,
                                           _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PSEC, _CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                                           _DISABLE, plmValue);
    ACR_REG_WR32(BAR0, LW_PSEC_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, plmValue);

    // Setting LW_PSEC_CMD_INTREN_PRIV_LEVEL_MASK to ACR_PLMASK_READ_L0_WRITE_L2
    plmValue = ACR_REG_RD32(BAR0, LW_PSEC_CMD_INTREN_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PSEC, _CMD_INTREN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,
                                           _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PSEC, _CMD_INTREN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                                           _DISABLE, plmValue);
    ACR_REG_WR32(BAR0, LW_PSEC_CMD_INTREN_PRIV_LEVEL_MASK, plmValue);

    // Setting LW_PSEC_TIMER_0_INTERVAL_PRIV_LEVEL_MASK to ACR_PLMASK_READ_L0_WRITE_L2
    plmValue = ACR_REG_RD32(BAR0, LW_PSEC_TIMER_0_INTERVAL_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PSEC, _TIMER_0_INTERVAL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,
                                           _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PSEC, _TIMER_0_INTERVAL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                                           _DISABLE, plmValue);
    ACR_REG_WR32(BAR0, LW_PSEC_TIMER_0_INTERVAL_PRIV_LEVEL_MASK, plmValue);

    // Setting LW_PSEC_TIMER_CTRL_PRIV_LEVEL_MASK to ACR_PLMASK_READ_L0_WRITE_L2
    plmValue = ACR_REG_RD32(BAR0, LW_PSEC_TIMER_CTRL_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PSEC, _TIMER_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,
                                           _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PSEC, _TIMER_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                                           _DISABLE, plmValue);
    ACR_REG_WR32(BAR0, LW_PSEC_TIMER_CTRL_PRIV_LEVEL_MASK, plmValue);
}

/*!
 * @brief Setup PLMs of target falcons.
 *        PLMs raised per falcon are
 *        For all falcons setting FALCON_PRIVSTATE_PRIV_LEVEL_MASK to Level0 Read and Level3 Write
 *        GSP      -
 *                     LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK,               Level0 Read and Level2 Write
 *                     LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK,              Level0 Read and Level2 Write
 *                     LW_PGSP_IRQTMR_PRIV_LEVEL_MASK                      Level0 Read and Level2 Write
 *
 *        LWDEC    -   LW_PLWDEC_FALCON_DIODT_PRIV_LEVEL_MASK              Level0 Read and Level2 Write
 *
 *        PMU      -   LW_PPWR_FALCON_DIODT_PRIV_LEVEL_MASK                Level0 Read and Level2 Write
 *
 *        SEC2     -
 *                     LW_PSEC_FALCON_DIODT_PRIV_LEVEL_MASK                Level0 Read and Level2 Write
 *                     LW_PSEC_FALCON_DIODTA_PRIV_LEVEL_MASK               Level0 Read and Level2 Write
 *
 *        FBFALCON -   LW_PFBFALCON_FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK,  Level0 Read and Level2 Write
 *
 *
 * @param[out] pFlcnCfg   Structure to hold falcon config
 */
ACR_STATUS
acrSetupTargetFalconPlms_GH100
(
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      plmVal = 0;
    //
    // TODO: For ACR_PLMASK_READ_L0_WRITE_L3, either
    // 1. Use DRF_DEF in this RegWrite code  OR
    // 2. Add lots of ct_asserts + build that define using one of the registers using DRF_DEF
    // This is to be done at all paces where this define is used
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_PRIVSTATE_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L3));

    switch(pFlcnCfg->falconId)
    {
        case LSF_FALCON_ID_LWDEC:
#ifndef BOOT_FROM_HS_BUILD
            plmVal = ACR_REG_RD32(BAR0, LW_PLWDEC_FALCON_DIODT_PRIV_LEVEL_MASK(0));
            plmVal = FLD_SET_DRF(_PLWDEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PLWDEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PLWDEC_FALCON_DIODT_PRIV_LEVEL_MASK(0), plmVal);
#endif // ifndef BOOT_FROM_HS_BUILD
            break;

        case LSF_FALCON_ID_GSP_RISCV:
#ifndef BOOT_FROM_HS_BUILD
            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK, plmVal);
#endif // ifndef BOOT_FROM_HS_BUILD

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_CMD_INTREN_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _CMD_INTREN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _CMD_INTREN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_CMD_INTREN_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_TIMER_0_INTERVAL_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _TIMER_0_INTERVAL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _TIMER_0_INTERVAL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_TIMER_0_INTERVAL_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_TIMER_CTRL_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _TIMER_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _TIMER_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_TIMER_CTRL_PRIV_LEVEL_MASK, plmVal);
            break;

        case LSF_FALCON_ID_SEC2:
#ifndef BOOT_FROM_HS_BUILD
            plmVal = ACR_REG_RD32(BAR0, LW_PSEC_FALCON_DIODT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PSEC_FALCON_DIODT_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PSEC_FALCON_DIODTA_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PSEC_FALCON_DIODTA_PRIV_LEVEL_MASK, plmVal);
#endif // ifndef BOOT_FROM_HS_BUILD
            break;

        case LSF_FALCON_ID_FBFALCON:
            plmVal = ACR_REG_RD32(BAR0, LW_PFBFALCON_FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PFBFALCON, _FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PFBFALCON, _FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PFBFALCON_FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK, plmVal);
            break;
    }

    return status;
}

/*!
 * @brief Check if falconInstance is valid
 */
LwBool
acrIsFalconInstanceValid_GH100
(
    LwU32 falconId,
    LwU32 falconInstance
)
{
    switch (falconId)
    {
        case LSF_FALCON_ID_FECS:
        case LSF_FALCON_ID_GPCCS:
            if (falconInstance < LW_SCAL_LITTER_MAX_NUM_SMC_ENGINES)
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break;
#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
            if (falconInstance < (LW_PLWDEC__SIZE_1 - 1))
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break;
#endif
#ifdef LWJPG_PRESENT
         case LSF_FALCON_ID_LWJPG:
            return acrValidateLwjpgId_HAL(pAcr, falconId, falconInstance);
#endif
        default:
            if ((falconId < LSF_FALCON_ID_END) && (falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0))
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break;
    }
}


/*!
 * @brief Check if count if GPC's is valid in case of Unicast boot scheme
 */
LwBool
acrIsFalconIndexMaskValid_GH100
(
    LwU32 falconId,
    LwU32 falconIndexMask
)
{
    switch (falconId)
    {
        case LSF_FALCON_ID_GPCCS:
            if (falconIndexMask <= (BIT(LSF_FALCON_INSTANCE_FECS_GPCCS_MAX)-1))
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break;
        default:
            if (falconIndexMask == LSF_FALCON_INDEX_MASK_DEFAULT_0)
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break;
    }
}

/*!
 * @brief Program DMA base register
 *
 * @param[in] pFlcnCfg   Structure to hold falcon config
 * @param[in] fbBase     Base address of fb region to be copied
 */
ACR_STATUS
acrProgramDmaBase_GH100
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    LwU64               fbBase
)
{
   ACR_STATUS status = ACR_OK;
   
   CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE, (LwU32)LwU64_LO32(fbBase)));
   CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE1, (LwU32)LwU64_HI32(fbBase)));

   return status;
}

/*!
 * @brief: Program CORE_SELECT based on the UPROC_TYPE
 *
 * @param[in] pFlcnCfg          FALCONF CONFIG of target engine
 *
 */
ACR_STATUS
acrCoreSelect_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data32 = 0;

    if(!pFlcnCfg)
    {
        return ACR_ERROR_ILWALID_ARGUMENT ;
    }

    // RISCV is not enabled, no need to program core select
    if(!pFlcnCfg->riscvRegisterBase)
    {
        return status;
    }

    switch(pFlcnCfg->uprocType)
    {
        case ACR_TARGET_ENGINE_CORE_RISCV:
            data32 = DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _RISCV) |
                     DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _BRFETCH,     _TRUE);

            if (pFlcnCfg->bFmcFlushShadow == LW_TRUE)
            {
                data32 = FLD_SET_DRF(_PRISCV, _RISCV_BCR_CTRL, _RTS_FLUSH_SHADOW,  _TRUE, data32);
            }
        break;

        case ACR_TARGET_ENGINE_CORE_RISCV_EB:
            data32 = DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _RISCV) |
                     DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _BRFETCH,     _TRUE);
        break;

        case ACR_TARGET_ENGINE_CORE_FALCON:
            data32 = DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _FALCON);
        break;

        default:
            return ACR_ERROR_UNEXPECTED_ARGS;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_RISCV, LW_PRISCV_RISCV_BCR_CTRL, data32));

    return status;
}

/*!
 * @brief Poll for IMEM and DMEM scrubbing to complete
 *
 * @param[in] pFlcnCfg Falcon Config
 */
ACR_STATUS
acrPollForScrubbing_GH100
(
    PACR_FLCN_CONFIG  pFlcnCfg
)
{
    ACR_STATUS    status          = ACR_OK;
    LwU32         data            = 0;
    ACR_TIMESTAMP startTimeNs;
    LwS32         timeoutLeftNs;

    if(!pFlcnCfg)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Poll for SRESET to complete
    acrGetLwrrentTimeNs_HAL(pAcr, &startTimeNs);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_HWCFG2, &data));
    while(FLD_TEST_DRF(_PFALCON, _FALCON_HWCFG2, _MEM_SCRUBBING, _PENDING, data))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckTimeout_HAL(pAcr, (LwU32)ACR_DEFAULT_TIMEOUT_NS,
                                                            startTimeNs, &timeoutLeftNs));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_HWCFG2, &data));
    }

    return status;
}


/*
 * @brief Get common scratch group register allocation for particular falcon IMB/IMB1/DMB/DMB1.
 */
ACR_STATUS
acrGetFalconScratchGroupAllocationsForCodeDataBase_GH100
(
    LwU32 falconId,
    LwU32 *pIMBRegister,
    LwU32 *pIMB1Register,
    LwU32 *pDMBRegister,
    LwU32 *pDMB1Register,
    LwU32 *pPLMRegister
)
{
    if ((pIMBRegister == NULL) || (pIMB1Register == NULL) || (pDMBRegister == NULL) || (pDMB1Register == NULL))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (falconId)
    {
        case LSF_FALCON_ID_SEC2:
                *pIMBRegister  = LW_PSEC_FALCON_IMB_BASE_COMMON_SCRATCH;
                *pIMB1Register = LW_PSEC_FALCON_IMB1_BASE_COMMON_SCRATCH;
                *pDMBRegister  = LW_PSEC_FALCON_DMB_BASE_COMMON_SCRATCH;
                *pDMB1Register = LW_PSEC_FALCON_DMB1_BASE_COMMON_SCRATCH;
                *pPLMRegister  = LW_PSEC_FALCON_IMB_DMB_COMMON_SCRATCH_PLM;
            break;
        case LSF_FALCON_ID_GSP_RISCV:
                *pIMBRegister  = LW_PGSP_FALCON_IMB_BASE_COMMON_SCRATCH;
                *pIMB1Register = LW_PGSP_FALCON_IMB1_BASE_COMMON_SCRATCH;
                *pDMBRegister  = LW_PGSP_FALCON_DMB_BASE_COMMON_SCRATCH;
                *pDMB1Register = LW_PGSP_FALCON_DMB1_BASE_COMMON_SCRATCH;
                *pPLMRegister  = LW_PGSP_FALCON_IMB_DMB_COMMON_SCRATCH_PLM;
            break;
#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
#endif
        case LSF_FALCON_ID_LWDEC:
                *pIMBRegister  = LW_PLWDEC_FALCON_IMB_BASE_COMMON_SCRATCH;
                *pIMB1Register = LW_PLWDEC_FALCON_IMB1_BASE_COMMON_SCRATCH;
                *pDMBRegister  = LW_PLWDEC_FALCON_DMB_BASE_COMMON_SCRATCH;
                *pDMB1Register = LW_PLWDEC_FALCON_DMB1_BASE_COMMON_SCRATCH;
                *pPLMRegister  = LW_PLWDEC_FALCON_IMB_DMB_COMMON_SCRATCH_PLM;
            break;
        default:
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    return ACR_OK;
}

/*!
 * @brief Writes the value for target falcon's IMB, IMB1, DMB, DMB1 register in
 * the allocated common scratch group register.
 *
 * @param[in] wprBase           WPR regiosn base address
 * @param[in] pLsbHeaderWrapper Target falcon LSB header
 * @param[in] pFlcnCfg          Falcon Config
 *
 * @param[out] ACR_STATUS
 */
ACR_STATUS
acrProgramFalconCodeDataBaseInScratchGroup_GH100
(
    LwU64 wprBase,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper,
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;
    LwU32 ucodeOffset = 0;
    LwU32 blCodeSize = 0;
    LwU32 appDataOffset = 0;
    LwU32 val = 0;
    LwU64 IMBBase = 0;
    LwU64 DMBBase = 0;
    LwU32 IMBRegister;
    LwU32 IMB1Register;
    LwU32 DMBRegister;
    LwU32 DMB1Register;
    LwU32 PLMRegister;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET, pLsbHeaderWrapper, &ucodeOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE, pLsbHeaderWrapper, &blCodeSize));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET, pLsbHeaderWrapper, &appDataOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconScratchGroupAllocationsForCodeDataBase_HAL(pAcr, pFlcnCfg->falconId,
                                      &IMBRegister, &IMB1Register, &DMBRegister, &DMB1Register, &PLMRegister));

    blCodeSize = LW_ALIGN_UP(blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);
    IMBBase = (wprBase << 8) + ucodeOffset + blCodeSize;
    DMBBase = (wprBase << 8) + ucodeOffset + appDataOffset;

    // IMB Programming
    val = (LwU32)LwU64_LO32(IMBBase) >> 8 | (LwU32)LwU64_HI32(IMBBase) << 24;
    ACR_REG_WR32(BAR0, IMBRegister, val);

    // IMB1 Programming
    val = (LwU32)LwU64_HI32(IMBBase) >> 8;
    ACR_REG_WR32(BAR0, IMB1Register, val);

    // DMB Programming
    val = (LwU32)LwU64_LO32(DMBBase) >> 8 | (LwU32)LwU64_HI32(DMBBase) << 24;
    ACR_REG_WR32(BAR0, DMBRegister, val);

    // DMB1 Programming
    val = (LwU32)LwU64_HI32(DMBBase) >> 8;
    ACR_REG_WR32(BAR0, DMB1Register, val);

    // PLM Programming
    ACR_REG_WR32(BAR0, PLMRegister, ACR_PLMASK_READ_L0_WRITE_L3);

    return status;
}

/*
 * @brief: Use Pri Source Isolation to lock the falcon register space to engine
 *         on which ACR is running
 * @param[in] sourceId               : Source Falcon Id
 * @param[in] pTargetFlcnCfg         : Target Falcon config
 * @param[in] setTrap                : If True  - Lock falcon register space
 *                                     If False - Restore TARGET_MASK to original value
 * @param[in] pTargetMaskPlmOldValue : Used for restoring TARGET_MASK register PLM value.
 * @param[in] pTargetMaskOldValue    : Used for restoring TARGET_MASK register value.
 * @param[out] ACR_STATUS
 */
ACR_STATUS
acrLockFalconRegSpace_GH100
(
    LwU32 sourceId,
    PACR_FLCN_CONFIG pTargetFlcnCfg,
    LwBool setTrap,
    LwU32 *pTargetMaskPlmOldValue,
    LwU32 *pTargetMaskOldValue
)
{
    ACR_STATUS  status                  = ACR_OK;
    LwU32       targetMaskValData       = 0;
    LwU32       targetMaskPlm           = 0;
    LwU32       targetMaskIndexData     = 0;
    LwU32       sourceMask              = 0;
    LwU32       targetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
    LwU32       targetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
    LwU32       targetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;

    if ((pTargetFlcnCfg == NULL) || (pTargetMaskPlmOldValue == NULL) || (pTargetMaskOldValue == NULL))
    {
        status = ACR_ERROR_ILWALID_ARGUMENT;
        goto Cleanup;
    }

#if defined(LWDEC_RISCV_EB_PRESENT) || defined(LWJPG_PRESENT)
    //
    // On GH100 Fmodel, SYS_B LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK is undefined,
    // and initial value is zero. This will cause register lock function corrupted.
    // LWDEC_RISCV_EB (2 ~ 6) and LWJPG(3 ~ 7) is controlled under SYS_B cluster. So we just return
    // ACR_OK to avoid test failed on Fmodel.
    //
#ifdef ACR_FMODEL_BUILD
    if (pTargetFlcnCfg->falconId == LSF_FALCON_ID_LWDEC_RISCV_EB ||
        pTargetFlcnCfg->falconId == LSF_FALCON_ID_LWJPG)
    {
        return ACR_OK;
    }
#endif // ACR_FMODEL_BUILD

#endif // defined(LWDEC_RISCV_EB_PRESENT) || defined(LWJPG_PRESENT)

    //
    // HW has kept flexibility of SYSB/SYSC cluster for now. This is handled with the HAL call below.
    // NOTE: We are not changing the field names of LW_PPRIV_SYS*_TARGET_MASK* with the assumption
    // that even if the cluster has changed for FbFalcon/LWDEC, the arrangement of fields inside the register is not changed.
    // Thus we can keep using the LW_PPRIV_SYS_* field names.
    //

    switch (pTargetFlcnCfg->falconId)
    {
        case LSF_FALCON_ID_FBFALCON:
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrGetFbFalconTargetMaskRegisterDetails_HAL(pAcr,
                                            &targetMaskRegister, &targetMaskPlmRegister, &targetMaskIndexRegister));
        break;

#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
#endif
        case LSF_FALCON_ID_LWDEC:
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrGetLwdecTargetMaskRegisterDetails_HAL(pAcr, pTargetFlcnCfg,
                                            &targetMaskRegister, &targetMaskPlmRegister, &targetMaskIndexRegister));
        break;

        case LSF_FALCON_ID_GPCCS:
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrGetGpccsTargetMaskRegisterDetails_HAL(pAcr, pTargetFlcnCfg->falconInstance, &targetMaskRegister, &targetMaskPlmRegister, &targetMaskIndexRegister));
        break;

#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrGetLwjpgTargetMaskRegisterDetails_HAL(pAcr, pTargetFlcnCfg, &targetMaskRegister, &targetMaskPlmRegister, &targetMaskIndexRegister));
        break;
#endif
    }

    // Lock falcon
    if (setTrap == LW_TRUE)
    {
        if (sourceId == pTargetFlcnCfg->falconId)
        {
            status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
            goto Cleanup;
        }

        // Step 1: Read, store and program TARGET_MASK PLM register.
        targetMaskPlm = ACR_REG_RD32(BAR0, targetMaskPlmRegister);

        // Save plm value to be restored during unlock
        *pTargetMaskPlmOldValue = targetMaskPlm;

        switch (sourceId)
        {
            case LSF_FALCON_ID_GSP_RISCV:
                sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP;
                break;
            case LSF_FALCON_ID_SEC2:
                sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2;
                break;

            default:
                status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
                goto Cleanup;
        }

        // Verify if PLM of TARGET_MASK does not allow programming from GSP or SEC2
        if (!(DRF_VAL(_PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, targetMaskPlm) & BIT(sourceMask)))
        {
            status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
            goto Cleanup;
        }

        //
        // Program PLM of TARGET_MASK registers to be accessible only from GSP or SEC2.
        // And Block accesses from other sources
        //
        targetMaskPlm = FLD_SET_DRF_NUM( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(sourceMask), targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCK, targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCK, targetMaskPlm);
        ACR_REG_WR32(BAR0, targetMaskPlmRegister, targetMaskPlm);

        // Step 2: Read and program TARGET_MASK_INDEX register.
        targetMaskIndexData = ACR_REG_RD32(BAR0, targetMaskIndexRegister);

        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, pTargetFlcnCfg->targetMaskIndex, targetMaskIndexData);
        ACR_REG_WR32(BAR0, targetMaskIndexRegister, targetMaskIndexData);

        // Step 3: Read, store and program TARGET_MASK register.
        targetMaskValData  = ACR_REG_RD32(BAR0, targetMaskRegister);
        // Save Target Mask value to be restored.
        *pTargetMaskOldValue = targetMaskValData;

        // Give only Read permission to other PRI sources and Read/Write permission to GSP.
        targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _VAL, _W_DISABLED_R_ENABLED_PL0, targetMaskValData);

        switch (sourceId)
        {
            case LSF_FALCON_ID_GSP_RISCV:
                targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _GSP_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
            break;

            //
            // TODO @vsurana : Drop this switch case and keep only the code for GSP.
            // Since there will be only one case, we'll no longer be needing the sourceId too
            //
            case LSF_FALCON_ID_SEC2:
                // ACR will lock target falcon register space while bootstrapping target falcon.
                targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _SEC2_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
            break;

            default:
                status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
                goto Cleanup;
        }

        ACR_REG_WR32(BAR0, targetMaskRegister, targetMaskValData);
    }
    else // Release lock
    {
        // Reprogram TARGET_MASK_INDEX register.
        targetMaskIndexData = ACR_REG_RD32(BAR0, targetMaskIndexRegister);

        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, pTargetFlcnCfg->targetMaskIndex, targetMaskIndexData);
        ACR_REG_WR32(BAR0, targetMaskIndexRegister, targetMaskIndexData);

        // Restore TARGET_MASK register to previous value.
        targetMaskValData = *pTargetMaskOldValue;
        ACR_REG_WR32(BAR0, targetMaskRegister, targetMaskValData);

        //
        // Restore TARGET_MASK_PLM to previous value to
        // enable all the sources to access TARGET_MASK registers.
        //
        targetMaskPlm = *pTargetMaskPlmOldValue;
        ACR_REG_WR32(BAR0, targetMaskPlmRegister, targetMaskPlm);
    }

Cleanup:
    return status;
}


/*!
 * @brief Put the given falcon into reset state
 */
ACR_STATUS
acrPutFalconInReset_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32      data   = 0;
    ACR_STATUS status = ACR_OK;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // For the Falcon doesn't have a full-blown engine reset, we reset Falcon only.
    //
    if ((pFlcnCfg->pmcEnableMask == 0) && (pFlcnCfg->regSelwreResetAddr == 0))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFalconOnlyReset_HAL(pAcr, pFlcnCfg));
    }
    else
    {
        if (pFlcnCfg->pmcEnableMask != 0)
        {
            data = READ_DEVICE_ENABLE_REG();
            data &= ~pFlcnCfg->pmcEnableMask;
            WRITE_DEVICE_ENABLE_REG();
            READ_DEVICE_ENABLE_REG();
        }

        if (pFlcnCfg->regSelwreResetAddr != 0)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrAssertEngineReset_HAL(pAcr, pFlcnCfg));
        }
    }

    return ACR_OK;
}

/*!
 * @brief Bring the given falcon out of reset state
 */
ACR_STATUS
acrBringFalconOutOfReset_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (pFlcnCfg->regSelwreResetAddr != 0)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrDeassertEngineReset_HAL(pAcr, pFlcnCfg));
    }

    if (pFlcnCfg->pmcEnableMask != 0)
    {
        data = READ_DEVICE_ENABLE_REG();
        data |= pFlcnCfg->pmcEnableMask;
        WRITE_DEVICE_ENABLE_REG();
        READ_DEVICE_ENABLE_REG();
    }

    return ACR_OK;
}


/*!
 * @brief Reset only the falcon part
 */
ACR_STATUS
acrFalconOnlyReset_GH100
(
    PACR_FLCN_CONFIG  pFlcnCfg
)
{
    ACR_STATUS    status = ACR_OK;
    LwU32         sftR   = 0;
    LwU32         data   = 0;
    ACR_TIMESTAMP startTimeNs;
    LwS32         timeoutLeftNs;

    if (pFlcnCfg->bFbifPresent)
    {
        // TODO: Revisit this once FECS halt bug has been fixed

        // Wait for STALLREQ
        sftR = FLD_SET_DRF(_PFALCON, _FALCON_ENGCTL, _SET_STALLREQ, _TRUE, 0);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_ENGCTL, sftR));

        // Poll for FHSTATE.EXT_HALTED to complete
        acrGetLwrrentTimeNs_HAL(pAcr, &startTimeNs);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_FHSTATE, &data));
        while(FLD_TEST_DRF_NUM(_PFALCON, _FALCON_FHSTATE, _EXT_HALTED, 0, data))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckTimeout_HAL(pAcr, ACR_DEFAULT_TIMEOUT_NS,
                                                       startTimeNs, &timeoutLeftNs));
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_FHSTATE, &data));
        }

        // Do SFTRESET
        sftR = FLD_SET_DRF(_PFALCON, _FALCON_SFTRESET, _EXT, _TRUE, 0);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET, sftR));

        // Poll for SFTRESET to complete
        acrGetLwrrentTimeNs_HAL(pAcr, &startTimeNs);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET, &data));
        while(FLD_TEST_DRF(_PFALCON, _FALCON_SFTRESET, _EXT, _TRUE, data))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckTimeout_HAL(pAcr, ACR_DEFAULT_TIMEOUT_NS,
                                                       startTimeNs, &timeoutLeftNs));
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET, &data));
        }
    }

    // Do falcon reset
    sftR = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _SRESET, _TRUE, 0);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL, sftR));

    // Poll for SRESET to complete
    acrGetLwrrentTimeNs_HAL(pAcr, &startTimeNs);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL, &data));
    while(FLD_TEST_DRF(_PFALCON_FALCON, _CPUCTL, _SRESET, _TRUE, data))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckTimeout_HAL(pAcr, ACR_DEFAULT_TIMEOUT_NS,
                                        startTimeNs, &timeoutLeftNs));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL, &data));
    }

    return status;
}



/*!
 * @brief Programs target registers to ensure the falcon goes into LS mode
 */
ACR_STATUS
acrSetupTargetRegisters_GH100
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    ACR_STATUS       status     = ACR_OK;
    LwU32            data       = 0;
    LwU32            i          = 0;
    LwBool           bFlcnHalted;

    //
    // For few LS falcons ucodes, we use bootloader to load actual ucode,
    // so we need to restrict them to be loaded from WPR only
    //
    if (acrCheckIfFalconIsBootstrappedWithLoader_HAL(pAcr, pFlcnCfg->falconId))
    {
        for (i = 0; i < LW_PFALCON_FBIF_TRANSCFG__SIZE_1; i++)
        {
            acrProgramRegionCfg_HAL(pAcr, pFlcnCfg, i, LSF_WPR_EXPECTED_REGION_ID);
        }
    }

    // Step-1: Set SCTL priv level mask to only HS accessible
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L3));

    //
    // Step-2: Set
    //    SCTL_RESET_LVLM_EN to FALSE
    //    SCTL_STALLREQ_CLR_EN to FALSE
    //    SCTL_AUTH_EN to TRUE
    //

    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _FALSE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _FALSE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_SCTL, data));

    // Step-3: Set CPUCTL_ALIAS_EN to FALSE
    data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, 0);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data));

#ifdef ACR_RISCV_LS
    //
    // TODO: Create a proper, RISC-V-specific variant of this function.
    //       See bugs 2720166 and 2720167.
    //
    if (pFlcnCfg->uprocType == ACR_TARGET_ENGINE_CORE_RISCV)
    {
#ifdef LW_PRISCV_RISCV_CPUCTL_ALIAS_EN
        data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL, _ALIAS_EN, _FALSE, 0);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_RISCV_CPUCTL, data));
#endif
    }
#endif

    //
    // Step-4: Setup
    //        IMEM_PRIV_LEVEL_MASK
    //        DMEM_PRIV_LEVEL_MASK
    //        CPUCTL_PRIV_LEVEL_MASK
    //        DEBUG_PRIV_LEVEL_MASK
    //        EXE_PRIV_LEVEL_MASK
    //        IRQTMR_PRIV_LEVEL_MASK
    //        MTHDCTX_PRIV_LEVEL_MASK
    //        BOOTVEC_PRIV_LEVEL_MASK
    //        DMA_PRIV_LEVEL_MASK
    //        FBIF_REGIONCFG_PRIV_LEVEL_MASK
    //
    // Use level 2 protection for writes to IMEM/DMEM. We do not need read protection and leaving IMEM/DMEM
    // open can aid debug if a problem happens before the PLM is overwritten by ACR binary or the PMU task to
    // the final value. Note that level 2 is the max we can use here because this code is run inside PMU at
    // level 2. In future when we move the acr task from PMU to SEC2 and make it HS, we can afford to use
    // the final values from pFlcnCfg->{imem, dmem}PLM directly over here.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L0));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L0));

    // Raise BOOTVEC, DMA registers to be only level 3 writeable
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrUpdateBootvecPlmLevel3Writeable_HAL(pAcr, pFlcnCfg));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrUpdateDmaPlmLevel3Writeable_HAL(pAcr, pFlcnCfg));

    if (pFlcnCfg->bFbifPresent)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2));
    }
    else
    {
        //
        // FECS and GPCCS case, as they do not support FBIF
        // Changing this to R-M-W, as we have Pri-Source-Isolation from Turing
        // because of which we have more field in REGIONCFG PLM
        //
        LwU32 reg = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgMaskAddr);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _READ_PROTECTION,         _ALL_LEVELS_ENABLED, reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _READ_VIOLATION,          _REPORT_ERROR,       reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE,            reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE,            reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE,             reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_VIOLATION,         _REPORT_ERROR,       reg);
        ACR_REG_WR32(BAR0, pFlcnCfg->regCfgMaskAddr, reg);
    }

    // Setup target falcon PLMs (Lwrrently only PRIVSTATE_PLM, more to be added)
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupTargetFalconPlms_HAL(pAcr, pFlcnCfg));

    // Step-5: Check if the falcon is HALTED
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIsFalconHalted_HAL(pAcr, pFlcnCfg, &bFlcnHalted));
    if (!bFlcnHalted)
    {
        // Return error in case target falcon is not at HALT state.
        return ACR_ERROR_FLCN_NOT_IN_HALTED_STATE;
    }

    //
    // Step-6: Authorize falcon
    // TODO: Let the individual falcons ucodes take care of setting up TRANSCFG/REGIONCFG
    //

    // Program LSMODE here
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _TRUE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _TRUE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _LSMODE, _TRUE, data);
    data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_SCTL, _LSMODE_LEVEL, 0x2, data);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_SCTL, data));

    // Check if AUTH_EN is still TRUE
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_SCTL, &data));
    if (!FLD_TEST_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data))
    {
        return ACR_ERROR_LS_BOOT_FAIL_AUTH;
    }

    if (pFlcnCfg->bOpenCarve)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrProgramDmemRange(pFlcnCfg));
    }

    // Step-7: Set CPUCTL_ALIAS_EN to TRUE
#ifdef ACR_RISCV_LS
    if (pFlcnCfg->uprocType == ACR_TARGET_ENGINE_CORE_RISCV)
    {
#ifdef LW_PRISCV_RISCV_CPUCTL_ALIAS_EN
        data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL, _ALIAS_EN, _TRUE, 0);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_RISCV_CPUCTL, data));
#endif
    }
    else
#endif
    {
        data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _TRUE, 0);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data));
    }

#ifndef ACR_BSI_BOOT_PATH
    // There are some SEC2-specific registers to be initialized
    if (pFlcnCfg->falconId == LSF_FALCON_ID_SEC2)
    {
        acrSetupSec2Registers_HAL(pAcr, pFlcnCfg);
    }
#endif

    return status;
}



/*!
 * @brief Find the IMEM block from the end to load BL code
 */
ACR_STATUS
acrFindFarthestImemBl_GH100
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    LwU32              codeSizeInBytes,
    LwU32              *pDst
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      hwcfg; 
    LwU32      imemSize;
    LwU32      codeBlks;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_HWCFG, &hwcfg));
    imemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG, _IMEM_SIZE, hwcfg);
    //Checks added to fix CERT-C Warning - 16862180 Unsigned integer operation may wrap
    if (LW_U32_MAX - codeSizeInBytes < FLCN_IMEM_BLK_SIZE_IN_BYTES)
    {
        //allocating max possible codeBlks for codeSizeInBytes in [LW_U32_MAX - FLCN_IMEM_BLK_SIZE_IN_BYTES, LW_U32_MAX]
        codeBlks = LW_U32_MAX / FLCN_IMEM_BLK_SIZE_IN_BYTES;
    }
    else
    {
        codeBlks = DIV_ROUND_UP(codeSizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES);
    }
    
    if (imemSize < codeBlks)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    
    *pDst = (imemSize - codeBlks);

    return ACR_OK;
}

/*
 * Setup FECS registers before bootstrapping it. More details in bug 2627329
 */
ACR_STATUS
acrSetupCtxswRegisters_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32       data       = 0;
    LwU32       addr       = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_CTXSW_GFID, pFlcnCfg->falconInstance);

    //
    // FECS ucode reads GFID value from LW_PGRAPH_PRI_FECS_CTXSW_GFID, at the time of instance bind,
    // as the instance bind request will be sent using this GFID. The value of this register is 0 out of reset however
    // HW wants RM to explicitly program the GFID to 0.
    //
    data = ACR_REG_RD32(BAR0, addr);
    data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_GFID, _V, _ZERO, data);
    ACR_REG_WR32(BAR0, addr, data);

    //
    // During the FECS ucode load, the FECS DMA engine is triggered by ACR and that ilwolves writing the FECS arbiter registers.
    // Although the reset value of ENGINE_ID field for this register (ie LW_PGRAPH_PRI_FECS_ARB_ENGINE_CTL) is BAR2_FN0, the one
    // which we program here, but in the future if ACR is triggers the ucode load after these values are at the non-reset values,
    // then we might run into issues and thus to avoid this we explicitly program them.
    //
    addr = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_ENGINE_CTL, pFlcnCfg->falconInstance);
    data = ACR_REG_RD32(BAR0, addr);
    data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_ENGINE_CTL, _ENGINE_ID, _BAR2_FN0, data);
    ACR_REG_WR32(BAR0, addr, data);

    return ACR_OK;
}

/*!
 * @brief Issue target falcon DMA. Supports only FB -> Flcn and not the other way.
 *        Supports DMAing into both IMEM and DMEM of target falcon from FB.
 *        Always uses physical addressing for memory transfer. Expects size to be
 *        256 multiple.
 *
 * @param[in] dstOff       Destination offset in either target falcon DMEM or IMEM
 * @param[in] fbBase       Base address of fb region to be copied
 * @param[in] fbOff        Offset from fbBase
 * @param[in] sizeInBytes  Number of bytes to be transferred
 * @param[in] regionID     ACR region ID to be used for this transfer
 * @param[in] bIsSync      Is synchronous transfer?
 * @param[in] bIsDstImem   TRUE if destination is IMEM
 * @param[in] bIsSelwre    TRUE if destination is a secure IMEM block. Valid only for IMEM destinations
 * @param[in] pFlcnCfg     Falcon config
 *
 * @return ACR_OK on success
 *         ACR_ERROR_UNEXPECTED_ARGS for mismatched arguments
 *         ACR_ERROR_TGT_DMA_FAILURE if the source, destination or size is not aligned
 */
ACR_STATUS
acrIssueTargetFalconDma_GH100
(
    LwU32            dstOff,
    LwU64            fbBase,
    LwU32            fbOff,
    LwU32            sizeInBytes,
    LwU32            regionID,
    LwU8             bIsSync,
    LwU8             bIsDstImem,
    LwU8             bIsSelwre,
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS     status        = ACR_OK;
    LwU32          data          = 0;
    LwU32          dmaCmd        = 0;
    LwU32          bytesXfered   = 0;
    LwS32          timeoutLeftNs = 0;
    ACR_TIMESTAMP  startTimeNs;

    //
    // Sanity checks
    // Only does 256B DMAs
    //
    if ((!VAL_IS_ALIGNED(sizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES)) ||
        (!VAL_IS_ALIGNED(dstOff, FLCN_IMEM_BLK_SIZE_IN_BYTES))      ||
        (!VAL_IS_ALIGNED(fbOff, FLCN_IMEM_BLK_SIZE_IN_BYTES)))
    {
        return ACR_ERROR_TGT_DMA_FAILURE;
    }

    if (!bIsDstImem && bIsSelwre)
    {
        // DMEM Secure block transfers are invalid
        return ACR_ERROR_UNEXPECTED_ARGS;
    }

    //
    // Program Transcfg to point to physical mode
    //
    acrSetupCtxDma_HAL(pAcr, pFlcnCfg, pFlcnCfg->ctxDma, LW_TRUE);

    if (pFlcnCfg->bFbifPresent)
    {
        //
        // Disable CTX requirement for falcon DMA engine
        //
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL, &data));
        data = FLD_SET_DRF(_PFALCON, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, data);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL, data));
    }

    // Set REQUIRE_CTX to false
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupDmaCtl_HAL(pAcr, pFlcnCfg, LW_FALSE));

    //Program REGCONFIG
    acrProgramRegionCfg_HAL(pAcr, pFlcnCfg, pFlcnCfg->ctxDma, regionID);

    //
    // Program DMA registers
    // Write DMA base address
    //
     CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramDmaBase_HAL(pAcr, pFlcnCfg, fbBase));

    // prepare DMA command
    {
        dmaCmd = 0;
        if (bIsDstImem)
        {
            dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _IMEM, _TRUE, dmaCmd);
        }
        else
        {
            dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _IMEM, _FALSE, dmaCmd);
        }

        if (bIsSelwre)
        {
            dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _SEC, 0x1, dmaCmd);
        }

        // Allow only FB->Flcn
        dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _WRITE, _FALSE, dmaCmd);

        // Allow only 256B transfers
        dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _SIZE, _256B, dmaCmd);

        dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _CTXDMA, pFlcnCfg->ctxDma, dmaCmd);
    }

    while (bytesXfered < sizeInBytes)
    {
        data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFMOFFS, _OFFS, (dstOff + bytesXfered), 0);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFMOFFS, data));

        data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFFBOFFS, _OFFS, (fbOff + bytesXfered), 0);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFFBOFFS, data));

        // Write the command
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD, dmaCmd));

        //
        // Poll for completion
        // TODO: Make use of bIsSync
        //
        acrGetLwrrentTimeNs_HAL(pAcr, &startTimeNs);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD, &data));
        while(FLD_TEST_DRF(_PFALCON_FALCON, _DMATRFCMD, _IDLE, _FALSE, data))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckTimeout_HAL(pAcr, ACR_DEFAULT_TIMEOUT_NS,
                                                            startTimeNs, &timeoutLeftNs));
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD, &data));
        }

        bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;
    }

    return status;
}



/*!
 * @brief Programs the bootvector for Falcons
 *
 * @param[in] pFlcnCfg   Falcon configuration information
 * @param[in] bootvec    Boot vector
 */
ACR_STATUS
acrSetupBootvec_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 bootvec
)
{

    return acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg,
          REG_LABEL_FLCN_BOOTVEC, bootvec);
}



/*!
 * @brief Find the total number of DMEM blocks
 */
ACR_STATUS
acrFindTotalDmemBlocks_GH100
(
    PACR_FLCN_CONFIG  pFlcnCfg,
    LwU32             *pBlocks
)
{
    ACR_STATUS    status = ACR_OK;
    LwU32         hwcfg;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_HWCFG, &hwcfg));
    *pBlocks = DRF_VAL(_PFALCON_FALCON, _HWCFG, _DMEM_SIZE, hwcfg);

    return status;
}



/*!
 * @brief Check falcons for which we use bootloader to load actual LS ucode
 *        We need to restrict such LS falcons to be loaded only from WPR, Bug 1969345
 */
LwBool
acrCheckIfFalconIsBootstrappedWithLoader_GH100
(
    LwU32 falconId
)
{
    // From GP10X onwards, PMU, SEC2 and DPU(GSPLite for GV100) are loaded via bootloader
    if ((falconId == LSF_FALCON_ID_SEC2))
    {
        return LW_TRUE;
    }

#ifdef ACR_RISCV_LS
    if (falconId == LSF_FALCON_ID_GSP_RISCV || falconId == LSF_FALCON_ID_PMU_RISCV)
    {
        return LW_TRUE;
    }
#endif

    return LW_FALSE;
}


/*!
 * @brief Check if falcon is halted or not
 */
ACR_STATUS
acrIsFalconHalted_GH100
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    LwBool             *bIsHalted               
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL,&data));
    *bIsHalted = FLD_TEST_DRF(_PFALCON_FALCON, _CPUCTL, _HALTED, _TRUE, data);

    return status;
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
ACR_STATUS
acrPrivLoadTargetFalcon_GH100
(
    LwU32               dstOff,
    LwU32               fbOff,
    LwU32               sizeInBytes,
    LwU32               regionID,
    LwU8                bIsDstImem,
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    ACR_STATUS   status        = ACR_ERROR_ILWALID_ARGUMENT;
    LwU32        bytesXfered   = 0;
    LwU32        tag;
    LwU32        i;
    ACR_DMA_PROP dmaPropRead;
    LwU32       *pBuffer1      = (LwU32 *)g_tmpGenericBuffer;

    // Clear the buffer before performing any operations
    acrMemset_HAL(pAcr, g_tmpGenericBuffer, 0x0, ACR_GENERIC_BUF_SIZE_IN_BYTES);

    // Configure the DMA Property
    dmaPropRead.wprBase  = g_dmaProp.wprBase;
    dmaPropRead.ctxDma   = RM_GSP_DMAIDX_PHYS_VID_FN0;
    dmaPropRead.regionID = g_dmaProp.regionID;

    // TODO: Enable posted writes
    //POSTED_WRITE_INIT();

    //
    // Sanity checks
    // Only does 256B transfers
    //
    if ((!VAL_IS_ALIGNED(sizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES)) ||
        (!VAL_IS_ALIGNED(dstOff, FLCN_IMEM_BLK_SIZE_IN_BYTES))      ||
        (!VAL_IS_ALIGNED(fbOff, FLCN_IMEM_BLK_SIZE_IN_BYTES)))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Setup the IMEMC/DMEMC registers
    if (bIsDstImem)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWriteNonBlocking_HAL(pAcr, 
                               pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMC(0),
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_OFFS, 0) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_AINCW, 1)));
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWriteNonBlocking_HAL(pAcr, 
                               pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMC(0),
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_OFFS, 0) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_AINCW, 1)));
    }

    {
        tag = 0;
        while (bytesXfered < sizeInBytes)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, pBuffer1, LW_FALSE, fbOff + bytesXfered,
                                            FLCN_IMEM_BLK_SIZE_IN_BYTES, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &dmaPropRead));

            if (bIsDstImem)
            {
                // Setup the tags for the instruction memory.
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWriteNonBlocking_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMT(0), REF_NUM(LW_PFALCON_FALCON_IMEMT_TAG, tag)));
                for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / LW_SIZEOF32(LwU32); i++)
                {
                    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWriteNonBlocking_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMD(0), pBuffer1[i]));
                }
                tag++;
            }
            else
            {
                for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / LW_SIZEOF32(LwU32); i++)
                {
                    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegWriteNonBlocking_HAL(pAcr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMD(0), pBuffer1[i]));
                }
            }
            bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;
        }
    }

    // TODO: Enable posted writes?
    // POSTED_WRITE_END(LW_CSEC_MAILBOX(RM_SEC2_MAILBOX_ID_TASK_ACR));

    return status;
}

/*!
 * @brief Put the given falcon into reset state
 */
ACR_STATUS
acrAssertEngineReset_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{

    ACR_TIMESTAMP startTimeNs;
    LwU32         data          = 0;
    LwS32         timeoutLeftNs = 0;
    ACR_STATUS    status        = ACR_OK;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }


    if (pFlcnCfg->regSelwreResetAddr == 0)
    {
        return ACR_ERROR_ILWALID_RESET_ADDR;
    }

    data = FLD_SET_DRF(_PFALCON, _FALCON_ENGINE, _RESET, _ASSERT, data);
    ACR_REG_WR32(BAR0, pFlcnCfg->regSelwreResetAddr, data);

    acrGetLwrrentTimeNs_HAL(pAcr, &startTimeNs);
    do
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckTimeout_HAL(pAcr, ACR_TIMEOUT_FOR_RESET,
                                                   startTimeNs, &timeoutLeftNs));
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regSelwreResetAddr);
    }while(!FLD_TEST_DRF(_PFALCON, _FALCON_ENGINE, _RESET_STATUS, _ASSERTED, data));

    return ACR_OK;
}

/*!
 * @brief Bring the given falcon out of reset state
 */
ACR_STATUS
acrDeassertEngineReset_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{

    ACR_TIMESTAMP startTimeNs;
    LwU32         data          = 0;
    LwS32         timeoutLeftNs = 0;
    ACR_STATUS    status        = ACR_OK;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (pFlcnCfg->regSelwreResetAddr == 0)
    {
        return ACR_ERROR_ILWALID_RESET_ADDR;
    }

    data = FLD_SET_DRF(_PFALCON, _FALCON_ENGINE, _RESET, _DEASSERT, data);
    ACR_REG_WR32(BAR0, pFlcnCfg->regSelwreResetAddr, data);

    acrGetLwrrentTimeNs_HAL(pAcr, &startTimeNs);
    do
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckTimeout_HAL(pAcr, ACR_TIMEOUT_FOR_RESET,
                                                   startTimeNs, &timeoutLeftNs));
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regSelwreResetAddr);
    }while(!FLD_TEST_DRF(_PFALCON, _FALCON_ENGINE, _RESET_STATUS, _DEASSERTED, data));

    return ACR_OK;
}

