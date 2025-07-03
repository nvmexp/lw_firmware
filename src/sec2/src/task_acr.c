/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_acr.c
 * @brief  Takes care of access control regions and bootstrapping other Falcons.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "unit_dispatch.h"
#include "sec2_cmdmgmt.h"
#include "lwostask.h"
#include "sec2_objsec2.h"
#include "flcnifcmn.h"
#include "sec2_os.h"
#include "sec2_hs.h"
#include "lwos_dma_hs.h"
#include "lwosselwreovly.h"
#include "lwuproc.h"
/* ------------------------- Application Includes --------------------------- */
#include "acr.h"
#include "sec2_objacr.h"
#include "sec2_objic.h"
#include "main.h"
#include "lib_lw64.h"
#include "rmlsfm.h"
#include "config/g_acr_private.h"
#include "config/g_sec2_hal.h"
#include "config/g_lsr_hal.h"
#include "dev_falcon_v4.h"
#include "hwproject.h"
#include "g_acrlib_hal.h"
#include "acr_objacrlib.h"

//
// In this file, we are using FLCN_STATUS and ACR_STATUS interchangeably because we have calls to ACRlib functions
// This needs to be cleaned up, tracked in Bug 200497846. For now just adding below ct_assert for passing scenario
//
ct_assert(FLCN_OK == ACR_OK);

/* ------------------------- Macros and Defines ----------------------------- */
#define CHECK_FLCN_STATUS_AND_RETURN_ACR_STATUS_IF_NOT_OK(x)                    \
    do                                                                          \
    {                                                                           \
        if((status=(x)) != FLCN_OK)                                             \
        {                                                                       \
            switch(x)                                                           \
            {                                                                   \
                case FLCN_ERR_POSTED_WRITE_FAILURE:                             \
                    return ACR_ERROR_POSTED_WRITE_FAILURE;                      \
                case FLCN_ERR_POSTED_WRITE_INTERRUPTS_ENABLED:                  \
                    return ACR_ERROR_POSTED_WRITE_INTERRUPTS_ENABLED;           \
                case FLCN_ERR_POSTED_WRITE_PRI_CLUSTER_COUNT_MISMATCH:          \
                    return ACR_ERROR_POSTED_WRITE_PRI_CLUSTER_COUNT_MISMATCH;   \
                case FLCN_ERR_POSTED_WRITE_INCORRECT_PARAMS:                    \
                    return ACR_ERROR_POSTED_WRITE_INCORRECT_PARAMS;             \
                default:                                                        \
                    return ACR_ERROR_UNKNOWN;                                   \
            }                                                                   \
        }                                                                       \
    } while (LW_FALSE)



/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS _acrBootstrapFalcon(LwU32 falconId, LwU32 falconInstance, LwU32 flags, LwBool bUseVA, PRM_FLCN_U64 pWprBaseVirtual)
        GCC_ATTRIB_SECTION("imem_acr", "_acrBootstrapFalcon");
static void _acrAcceptCommand(DISP2UNIT_CMD *pDispatchedCmd, FLCN_STATUS *pAcrGatherWprRegionDetailsStatus)
        GCC_ATTRIB_SECTION("imem_acrLs", "_acrAcceptCommand");
static void _acrSendFalconStatusToRm(LwU32 status, LwU32 falconId, LwU32 falconInstance, LwU32 msgFlcn, LwU32 seqNum)
        GCC_ATTRIB_SECTION("imem_acrLs", "_acrSendFalconStatusToRm");
static void _acrGpcRgEventHandler(FLCN_STATUS *pAcrGatherWprRegionDetailsStatus)
        GCC_ATTRIB_SECTION("imem_acrLs", "_acrGpcRgEventHandler");
FLCN_STATUS _acrGatherWprRegionDetails(void)
        GCC_ATTRIB_SECTION("imem_acr", "_acrGatherWprRegionDetails");
static FLCN_STATUS _acrReadWprHeader(void)
        GCC_ATTRIB_SECTION("imem_acr", "_acrReadWprHeader");

#ifdef NEW_WPR_BLOBS
static FLCN_STATUS _acrSetupLSFalcon(PRM_FLCN_U64 pWprBase, LwU32 wprRegionID, PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper, PACR_FLCN_CONFIG pFlcnCfg, LSF_LSB_HEADER_WRAPPER *pLsbHeaderWrapper)
        GCC_ATTRIB_SECTION("imem_acr", "_acrSetupLSFalcon");
static FLCN_STATUS _acrSetupNSFalcon(PRM_FLCN_U64 pWprBase, LwU32 wprRegionID, PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper, PACR_FLCN_CONFIG pFlcnCfg, LSF_LSB_HEADER_WRAPPER *pLsbHeaderWrapper)
        GCC_ATTRIB_SECTION("imem_acr", "_acrSetupNSFalcon");
static LwBool _acrIsNsFalcon(LwU32 falconId, LwU32 falconInstance)
        GCC_ATTRIB_SECTION("imem_acr", "_acrIsNsFalcon");

#else
static FLCN_STATUS _acrSetupLSFalcon(PRM_FLCN_U64 pWprBase, LwU32 wprRegionID, PLSF_WPR_HEADER pWprHeader, PACR_FLCN_CONFIG pFlcnCfg, LSF_LSB_HEADER *pLsbHeader)
        GCC_ATTRIB_SECTION("imem_acr", "_acrSetupLSFalcon");
#endif

// HS entry point function, made Non Inline specifically.
FLCN_STATUS _acrGetRegionAndBootFalconWithHaltHs(LwU32 falconId, LwU32 falconInstance, LwU32 falconIndexMask,
                                                 LwU32 flags, LwBool bUseVA, PRM_FLCN_U64 pWprBaseVirtual,
                                                 LwBool bHalt, FLCN_STATUS *pAcrGatherWprRegionDetailsStatus)
        GCC_ATTRIB_NOINLINE()
        GCC_ATTRIB_SECTION("imem_acr", "start");

/*--------------------------Global functions*--------------------------------*/
// Functions used as substitute for lib_lw64.h library
void _lw64Sub(LwU64*, LwU64*, LwU64*)
         GCC_ATTRIB_SECTION("imem_acr", "_lw64Sub");

void _lw64Add32(LwU64*, LwU32)
        GCC_ATTRIB_SECTION("imem_acr", "_lw64Add32");

/* ------------------------- Global Variables ------------------------------- */
OBJACR Acr;
#ifdef BOOT_FROM_HS_BUILD
extern HS_FMC_PARAMS g_hsFmcParams;
#endif

/*!
 * Need to be global for alignment.
 */

#ifdef NEW_WPR_BLOBS
// WPR header, must be aligned to LSF_WPR_HEADER_ALIGNMENT for DMA access
LwU8 lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE]
    GCC_ATTRIB_ALIGN(LSF_LSB_HEADER_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_acr", "lsbHeaderWrapper");
#else
LwU8 lsbHeader[LSF_LSB_HEADER_TOTAL_SIZE_MAX]
    GCC_ATTRIB_ALIGN(LSF_LSB_HEADER_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_acr", "lsbHeader");
#endif

/*!
 * Queue for ACR Task/Commands.
 */
LwrtosQueueHandle AcrQueue;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the Command queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(task_acr, pvParameters)
{
    DISPATCH_ACR dispatch;
    FLCN_STATUS acrGatherWprRegionDetailsStatus = FLCN_ERROR;

    acrCheckSupportAndExitGc6ByAcrTask_HAL(pAcr, &acrGatherWprRegionDetailsStatus);

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(AcrQueue, &dispatch))
        {
            // Process the commands sent via the Command Queue.
            switch (dispatch.eventType)
            {
                case DISP2UNIT_EVT_COMMAND:
                {
                    _acrAcceptCommand(&dispatch.cmd, &acrGatherWprRegionDetailsStatus);
                    osCmdmgmtCmdQSweep(&(dispatch.cmd.pCmd->hdr), dispatch.cmd.cmdQueueId);

                    break;
                }
                case DISP2UNIT_EVT_COMMAND_BOOTSTRAP_GPCCS_FOR_GPCRG:
                {
                    if (SEC2CFG_FEATURE_ENABLED(SEC2_GPC_RG_SUPPORT))
                    {
                        // Bootstrap GPCCS for GPC-RG feature
                        _acrGpcRgEventHandler(&acrGatherWprRegionDetailsStatus);

                        // Clear and unmask Command queue interrupt (_PMU)
                        icCmdQueueIntrClear_HAL(&Ic, SEC2_CMDMGMT_CMD_QUEUE_PMU);
                        icCmdQueueIntrUnmask_HAL(&Ic, SEC2_CMDMGMT_CMD_QUEUE_PMU);
                    }

                    break;
                }
            }
        }
    }
}

/* ------------------------- Private Functions ------------------------------ */

/*
 * This is the HS entry point of ACR task for getting region details and
 * bootstrapping requested falcons.
 *
 * @return
 *      FLCN_OK if the Command is processed succesfully; otherwise, a detailed
 *      error code is returned.
 */
FLCN_STATUS
_acrGetRegionAndBootFalconWithHaltHs
(
    LwU32           falconId,
    LwU32           falconInstance,
    LwU32           falconIndexMask,
    LwU32           flags,
    LwBool          bUseVA,
    PRM_FLCN_U64    pWprBaseVirtual,
    LwBool          bHalt,
    FLCN_STATUS     *pAcrGatherWprRegionDetailsStatus
)
{
    LwBool bIsSmcActive = LW_TRUE;
    VALIDATE_RETURN_PC_AND_HALT_IF_HS();

    FLCN_STATUS status = FLCN_ERROR;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs, HASH_SAVING_DISABLE);
    OS_SEC_HS_LIB_VALIDATE(libDmaHs, HASH_SAVING_DISABLE);

    if ((status = sec2HsPreCheckCommon_HAL(&Sec2, LW_FALSE)) != FLCN_OK)
    {
        goto ErrorExit;
    }

    if((status = acrIsSmcActiveHs_HAL(&Acr, &bIsSmcActive)) != FLCN_OK)
    {
        goto ErrorExit;
    }

    if (!acrlibIsFalconInstanceValid_HAL(pAcrlib, falconId, falconInstance, bIsSmcActive))
    {
        status =  FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    if (!acrlibIsFalconIndexMaskValid_HAL(pAcrlib, falconId, falconIndexMask))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    // Return failure to caller, if not able to gather region details.
    if (FLCN_ERROR == *pAcrGatherWprRegionDetailsStatus)
    {
        *pAcrGatherWprRegionDetailsStatus = _acrGatherWprRegionDetails();

        if (*pAcrGatherWprRegionDetailsStatus != FLCN_OK)
        {
            status = *pAcrGatherWprRegionDetailsStatus;
            goto ErrorExit;
        }
    }


    if (falconIndexMask == LSF_FALCON_INDEX_MASK_DEFAULT_0)
    {
        status = _acrBootstrapFalcon(falconId, falconInstance, flags, LW_FALSE, NULL);
    }
    else
    {
        LwU32 falconIndexMaskLocal = falconIndexMask;
        LwU32 falconIndex          = 0x0;

        // Unicast bootloading of instances of same falcon.
        while(falconIndexMaskLocal != 0x0)
        {
            if ((falconIndexMaskLocal & 0x1) == 0x1)
            {
                status = _acrBootstrapFalcon(falconId, falconIndex, flags, LW_FALSE, NULL);
                if (status != FLCN_OK)
                {
                    goto ErrorExit;
                }
            }
            falconIndex += 1;
            falconIndexMaskLocal >>= 1;
        }
    }

ErrorExit:
    if ((status != FLCN_OK) && bHalt)
    {
        SEC2_HALT();
    }

    EXIT_HS();

    return status;
}

/*!
 * @brief acrAcceptCommand
 *        handles all incoming RM Commands
 *
 * @param[in] pDispatchedCmd Command from RM
 */
static void
_acrAcceptCommand
(
    DISP2UNIT_CMD *pDispatchedCmd,
    FLCN_STATUS   *pAcrGatherWprRegionDetailsStatus
)
{
    RM_SEC2_ACR_CMD *pCmd   = &pDispatchedCmd->pCmd->cmd.acr;
    LwU32            seqNum = pDispatchedCmd->pCmd->hdr.seqNumId;
    FLCN_STATUS      status = FLCN_OK;
    LwU32            falconId;
    LwU32            falconInstance;
    LwU32            falconIndexMask;

    switch(pCmd->cmdType)
    {
        case RM_SEC2_ACR_CMD_ID_BOOTSTRAP_FALCON:
        {
            falconId           = pCmd->bootstrapFalcon.falconId;
            falconInstance     = pCmd->bootstrapFalcon.falconInstance;
            falconIndexMask    = pCmd->bootstrapFalcon.falconIndexMask;
            // Sec2 shouldn't bootstrap itself
            if (falconId == LSF_FALCON_ID_SEC2)
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto _acrAcceptCommand_BootstrapFalcon_Exit;
            }

#ifdef NOUVEAU_SUPPORTED
            // Nouveau SEC2 profile should only boot GR falcons.
            if ((falconId != LSF_FALCON_ID_FECS) && (falconId != LSF_FALCON_ID_GPCCS))
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto _acrAcceptCommand_BootstrapFalcon_Exit;
            }
#endif

            // Load , execute and detach VPR HS overlay
            OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(acr));
            OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libDmaHs));

#ifdef WAR_DISABLE_HASH_SAVING_BUG_3335627
            OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(acr), NULL, 0, HASH_SAVING_DISABLE);
#else
            OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(acr), NULL, 0, HASH_SAVING_ENABLE);
#endif

            status = _acrGetRegionAndBootFalconWithHaltHs(falconId, falconInstance, falconIndexMask,
                                                          pCmd->bootstrapFalcon.flags, LW_FALSE, NULL, LW_FALSE,
                                                          pAcrGatherWprRegionDetailsStatus);

            OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

            OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libDmaHs));
            OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(acr));

_acrAcceptCommand_BootstrapFalcon_Exit:
            // send handled ACR id to RM
            _acrSendFalconStatusToRm(status, falconId, falconInstance, RM_SEC2_MSG_TYPE(ACR, BOOTSTRAP_FALCON), seqNum);
        }
        break;

        default:
        {
            // Invalid RM Command to SEC2.
            _acrSendFalconStatusToRm(FLCN_ERR_ILWALID_ARGUMENT, LSF_FALCON_ID_ILWALID, LSF_FALCON_INSTANCE_ILWALID,
                                     RM_SEC2_MSG_TYPE(ACR, ILWALID_COMMAND), seqNum);
            break;
        }
    }
}

/*!
 * @brief Handler for event sent by PMU (GPC-RG feature) to bootstrap GPCCS
 *
 * @param[in] pAcrGatherWprRegionDetailsStatus      Bootstrap status
 */
static void
_acrGpcRgEventHandler
(
    FLCN_STATUS *pAcrGatherWprRegionDetailsStatus
)
{
    FLCN_STATUS status = FLCN_ERROR;

    // Update the bootstrap state as STARTED
    acrGpcRgGpccsBsStateUpdateStarted_HAL(&Acr);

    // Load , execute and detach VPR HS overlay
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(acr));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libDmaHs));

    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(acr), NULL, 0, HASH_SAVING_ENABLE);

    // Bootstrap GPCCS falcon
    status = _acrGetRegionAndBootFalconWithHaltHs(LSF_FALCON_ID_GPCCS,
                                                  LSF_FALCON_INSTANCE_DEFAULT_0,
                                                  LSF_FALCON_INDEX_MASK_DEFAULT_0,
                                                  0x0, LW_FALSE, NULL, LW_FALSE,
                                                  pAcrGatherWprRegionDetailsStatus);

    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libDmaHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(acr));

    // Update the bootstrap status
    acrGpcRgGpccsBsErrorCodeUpdate_HAL(&Acr, status);

    // Update the bootstrap state as DONE
    acrGpcRgGpccsBsStateUpdateDone_HAL(&Acr);
}

/*!
 * @brief _acrGetWprHeader
 *        points to the WPR header of falconId
 *
 * @param[in]  falconId    :Falcon Id
 * @param[out] ppWprHeader :WPR header of falcon Id
 *
 * @return FLCN_OK    Success
 *         FLCN_ERROR Failed to located WPR header of falcon Id
 */

#ifdef NEW_WPR_BLOBS
static FLCN_STATUS
_acrGetWprHeader
(
    LwU32 falconId,
    PLSF_WPR_HEADER_WRAPPER *ppWprHeaderWrapper
)
{
    LwU8 index;
    PLSF_WPR_HEADER_WRAPPER pWrapper = (PLSF_WPR_HEADER_WRAPPER)Acr.pWpr;
    LwU32 wprFalconId;
    LwU32 lsbOffset;
    ACR_STATUS status;

    if (ppWprHeaderWrapper == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *ppWprHeaderWrapper = NULL;

    // Go through falcon header
    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                            &pWrapper[index], (void *)&lsbOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            &pWrapper[index], (void *)&wprFalconId));

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(wprFalconId, lsbOffset))
        {
            break;
        }

        if (wprFalconId == falconId)
        {
            *ppWprHeaderWrapper = (PLSF_WPR_HEADER_WRAPPER)&pWrapper[index];
            break;
        }
    }

    if (*ppWprHeaderWrapper == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    return FLCN_OK;
}

#else

static FLCN_STATUS
_acrGetWprHeader
(
    LwU32 falconId,
    PLSF_WPR_HEADER *ppWprHeader
)
{
    LwU8 index;
    PLSF_WPR_HEADER pWprHeader = (PLSF_WPR_HEADER)Acr.pWpr;

    if (ppWprHeader == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *ppWprHeader = NULL;

    // Go through falcon header
    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader[index].falconId, pWprHeader[index].lsbOffset))
        {
            break;
        }

        if (pWprHeader[index].falconId == falconId)
        {
            *ppWprHeader = (PLSF_WPR_HEADER)&pWprHeader[index];
            break;
        }
    }

    if (*ppWprHeader == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    return FLCN_OK;
}
#endif


#ifdef NEW_WPR_BLOBS
/*!
 * @brief _acrSetupLSFalcon:  Copies IMEM and DMEM into LS falcon
 *
 * @param[in] pWprBase   : Base addr of WPR Region
 * @param[in] regionId   : ACR Region ID
 * @param[in] pWprheader : WPR Header
 * @param[in] pFlcnCfg   : Falcon Config register
 * @param[in] pLsbHeader : LSB Header
 */
static FLCN_STATUS
_acrSetupLSFalcon
(
    PRM_FLCN_U64             pWprBase,
    LwU32                    regionId,
    PLSF_WPR_HEADER_WRAPPER  pWprHeaderWrapper,
    PACR_FLCN_CONFIG         pFlcnCfg,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper
)
{
    ACR_STATUS  status = ACR_OK;
    LwU32       addrHi;
    LwU32       addrLo;
    LwU32       wprBase;
    LwU32       blWprBase;
    LwU32       dst;
    LwU32       blCodeSize;
    LwU32       ucodeOffset;
    LwU32       appCodeOffset;
    LwU32       appCodeSize;
    LwU32       appDataOffset;
    LwU32       appDataSize;
    LwU32       blImemOffset;
    LwU32       blDataOffset;
    LwU32       blDataSize;
    LwU32       flags;
#ifdef BOOT_FROM_HS_BUILD
    LwU32       appImemOffset = 0;
    LwU32       appCodebase   = 0;
    LwU32       appDmemOffset = 0;
    LwU32       appDatabase = 0;
#endif

    //
    // Earlier wprOffset was added to lower address and then lower address was
    // checked for overflow. Gourav found that wproffset is always 0.
    // Hence we forced wprOffset as 0 in this task. We are still confirming with
    // Gobi, if there are cases where we can have wprOffset as non zero.
    //
    addrHi = pWprBase->hi;
    addrLo = pWprBase->lo;

    wprBase = (addrLo >> FLCN_IMEM_BLK_ALIGN_BITS) |
              (addrHi << (32 - FLCN_IMEM_BLK_ALIGN_BITS));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));
    blCodeSize = LW_ALIGN_UP(blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_SET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));

#ifdef BOOT_FROM_HS_BUILD
    LwU64 localWprBase;
    RM_FLCN_U64_UNPACK(&localWprBase, pWprBase);

    // WPR base is expected to be 256 bytes aligned
    localWprBase = localWprBase >> 8;

    if ((pFlcnCfg->falconId == LSF_FALCON_ID_GSPLITE) || (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibProgramFalconCodeDataBaseInScratchGroup_HAL(pAcrlib, localWprBase, pLsbHeaderWrapper, pFlcnCfg));
    }
#endif // BOOT_FROM_HS_BUILD

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupTargetRegisters_HAL(pAcrlib, pFlcnCfg));

    if (pFlcnCfg->falconId == LSF_FALCON_ID_FECS)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupCtxswRegisters_HAL(&Acr, pFlcnCfg));
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_FLAGS,
                                                                        pLsbHeaderWrapper, (void *)&flags));


    // Check if this falcon should be loaded via priv copy or DMA
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _FORCE_PRIV_LOAD, _TRUE, flags))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&ucodeOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&appCodeOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&appCodeSize));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&appDataSize));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&appDataOffset));

        // Load ucode into IMEM
        CHECK_FLCN_STATUS_AND_RETURN_ACR_STATUS_IF_NOT_OK(acrPrivLoadTargetFalcon_HAL(&Acr, 0,
              ucodeOffset + appCodeOffset, appCodeSize, regionId, LW_TRUE, pFlcnCfg));
        // Load data into DMEM
        CHECK_FLCN_STATUS_AND_RETURN_ACR_STATUS_IF_NOT_OK(acrPrivLoadTargetFalcon_HAL(&Acr, 0,
              ucodeOffset + appDataOffset, appDataSize, regionId, LW_FALSE, pFlcnCfg));

        // Set the BOOTVEC
        acrlibSetupBootvec_HAL(pAcrlib, pFlcnCfg, 0);
    }
    else
    {
        //
        // BL starts at offset, but since the IMEM VA is not zero, we need to make
        // sure FBOFF is equal to the expected IMEM VA. So adjusting the FB BASE to
        // make sure FBOFF equals to VA as expected.
        //
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&ucodeOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

        blWprBase = ((wprBase) +
                     ((ucodeOffset) >> FLCN_IMEM_BLK_ALIGN_BITS)) -
                     ((blImemOffset) >> FLCN_IMEM_BLK_ALIGN_BITS);

        // Check if code needs to be loaded at start of IMEM or at end of IMEM
        if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE, flags))
        {
            dst = 0;
        }
        else
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                                pLsbHeaderWrapper, (void *)&blCodeSize));

            dst = (acrlibFindFarthestImemBl_HAL(pAcrlib, pFlcnCfg, blCodeSize) *
                   FLCN_IMEM_BLK_SIZE_IN_BYTES);
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                                pLsbHeaderWrapper, (void *)&blCodeSize));

#ifdef BOOT_FROM_HS_BUILD
        // Load BL into IMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_FMC_PARAMS,
                                                                            pLsbHeaderWrapper, &g_hsFmcParams));
        if (g_hsFmcParams.bHsFmc)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
                dst, blWprBase, blImemOffset, blCodeSize, regionId,
                LW_TRUE, LW_TRUE, LW_TRUE, pFlcnCfg));
        }
        else
#endif
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
                  dst, blWprBase, blImemOffset, blCodeSize,
                  regionId, LW_TRUE, LW_TRUE, LW_FALSE, pFlcnCfg));
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                                pLsbHeaderWrapper, (void *)&blDataOffset));

#ifdef BOOT_FROM_HS_BUILD
        if (g_hsFmcParams.bHsFmc)
        {
            blDataSize = 0; // HS FMC has no data
        }
        else
#endif
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                                    pLsbHeaderWrapper, (void *)&blDataSize));
        }

        // Load data into DMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
            0, wprBase, blDataOffset, blDataSize,
            regionId, LW_TRUE, LW_FALSE, LW_FALSE, pFlcnCfg));

#ifdef BOOT_FROM_HS_BUILD
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_FMC_PARAMS,
                                                                            pLsbHeaderWrapper, &g_hsFmcParams));

        if (g_hsFmcParams.bHsFmc)
        {
            // Load the RTOS + App code to IMEM
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_IMEM_OFFSET,
                                                                                pLsbHeaderWrapper, &appImemOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_OFFSET,
                                                                                pLsbHeaderWrapper, &appCodeOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_SIZE,
                                                                                pLsbHeaderWrapper, &appCodeSize));

            // Ensure that app code does not overlap BL code in IMEM
            if ((appImemOffset >= dst) || ((appImemOffset + appCodeSize) >= dst))
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(ACR_ERROR_ILWALID_ARGUMENT);
            }

            // Start the app/OS code DMA with the DMA base set beyond the BL, so the tags are right
            appCodebase = wprBase + (ucodeOffset >> FLCN_IMEM_BLK_ALIGN_BITS) + (blCodeSize >> FLCN_IMEM_BLK_ALIGN_BITS);

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
                appImemOffset, appCodebase, (appCodeOffset - blCodeSize), appCodeSize, regionId,
                LW_TRUE, LW_TRUE, LW_FALSE, pFlcnCfg));

            // Load data for App.
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DMEM_OFFSET,
                                                                                pLsbHeaderWrapper, &appDmemOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET,
                                                                                pLsbHeaderWrapper, &appDataOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_SIZE,
                                                                                pLsbHeaderWrapper, &appDataSize));

            if (appDataSize != 0)
            {
                appDatabase = wprBase + (ucodeOffset >> FLCN_IMEM_BLK_ALIGN_BITS) + (appDataOffset >> FLCN_IMEM_BLK_ALIGN_BITS);

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
                    appDmemOffset, appDatabase, 0, appDataSize,
                    regionId, LW_TRUE, LW_FALSE, LW_FALSE, pFlcnCfg));
            }

            //
            // The signature is to be setup in the target falcon right after the App data
            // appDmemOffset and appDataSize should be already aligned
            //
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupHsFmcSignatureValidation_HAL(pAcrlib, pFlcnCfg, &g_hsFmcParams, blImemOffset, blCodeSize, appDmemOffset + appDataSize));
        }
#endif

#ifdef WAR_BUG_200670718
        if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC)
        {
            acrlibProgramDmaBase_HAL(pAcrlib, pFlcnCfg, blWprBase);
        }
#endif

        // Set the BOOTVEC
        acrlibSetupBootvec_HAL(pAcrlib, pFlcnCfg, blImemOffset);
    }

    // Check if Falcon wants virtual ctx
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE, flags))
    {
        acrlibSetupCtxDma_HAL(pAcrlib, pFlcnCfg, pFlcnCfg->ctxDma, LW_FALSE);
    }

    // Check if Falcon wants REQUIRE_CTX
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE, flags))
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, pFlcnCfg, LW_TRUE);
    }
    else
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, pFlcnCfg, LW_FALSE);
    }
    //
    // Program the IMEM and DMEM PLMs to the final desired values (which may
    // be level 3 for certain falcons) now that acrlib is done loading the
    // code onto the falcon.
    //
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM);

    return status;
}

/*!
 * @brief _acrSetupNSFalcon:  Copies IMEM and DMEM into NS falcon
 *
 * @param[in] pWprBase   : Base addr of WPR Region
 * @param[in] regionId   : ACR Region ID
 * @param[in] pWprheader : WPR Header
 * @param[in] pFlcnCfg   : Falcon Config register
 * @param[in] pLsbHeader : LSB Header
 */
static FLCN_STATUS
_acrSetupNSFalcon
(
    PRM_FLCN_U64             pWprBase,
    LwU32                    regionId,
    PLSF_WPR_HEADER_WRAPPER  pWprHeaderWrapper,
    PACR_FLCN_CONFIG         pFlcnCfg,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper
)
{
    ACR_STATUS  status = ACR_OK;
    LwU32       addrHi;
    LwU32       addrLo;
    LwU32       wprBase;
    LwU32       blWprBase;
    LwU32       dst;
    LwU32       blCodeSize;
    LwU32       ucodeOffset;
    LwU32       blImemOffset;
    LwU32       blDataOffset;
    LwU32       blDataSize;
    LwU32       flags;
    LwU32       data;

    //
    // Need to program IMEM/DMEM PLMs before DMA load data to IMEM/DMEM.
    // Program the IMEM and DMEM PLMs to the final desired values (which may
    // be level 3 for certain falcons) now that acrlib is done loading the
    // code onto the falcon.
    //
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibUpdateBootvecPlmLevel3Writeable_HAL(pAcrlib, pFlcnCfg));

    //
    // Earlier wprOffset was added to lower address and then lower address was
    // checked for overflow. Gourav found that wproffset is always 0.
    // Hence we forced wprOffset as 0 in this task. We are still confirming with
    // Gobi, if there are cases where we can have wprOffset as non zero.
    //
    addrHi = pWprBase->hi;
    addrLo = pWprBase->lo;

    wprBase = (addrLo >> FLCN_IMEM_BLK_ALIGN_BITS) |
              (addrHi << (32 - FLCN_IMEM_BLK_ALIGN_BITS));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));
    blCodeSize = LW_ALIGN_UP(blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_SET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_FLAGS,
                                                                        pLsbHeaderWrapper, (void *)&flags));

    //
    // BL starts at offset, but since the IMEM VA is not zero, we need to make
    // sure FBOFF is equal to the expected IMEM VA. So adjusting the FB BASE to
    // make sure FBOFF equals to VA as expected.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&ucodeOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

    blWprBase = ((wprBase) +
                 ((ucodeOffset) >> FLCN_IMEM_BLK_ALIGN_BITS)) -
                 ((blImemOffset) >> FLCN_IMEM_BLK_ALIGN_BITS);

    // Check if code needs to be loaded at start of IMEM or at end of IMEM
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE, flags))
    {
        dst = 0;
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&blCodeSize));

        dst = (acrlibFindFarthestImemBl_HAL(pAcrlib, pFlcnCfg, blCodeSize) *
               FLCN_IMEM_BLK_SIZE_IN_BYTES);
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib, dst, blWprBase, blImemOffset, blCodeSize,
                                      regionId, LW_TRUE, LW_TRUE, LW_FALSE, pFlcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blDataOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blDataSize));

    // Load data into DMEM
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
                                      0, wprBase, blDataOffset, blDataSize,
                                      regionId, LW_TRUE, LW_FALSE, LW_FALSE, pFlcnCfg));
    // Set the BOOTVEC
    acrlibSetupBootvec_HAL(pAcrlib, pFlcnCfg, blImemOffset);

    // Check if Falcon wants virtual ctx
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE, flags))
    {
        acrlibSetupCtxDma_HAL(pAcrlib, pFlcnCfg, pFlcnCfg->ctxDma, LW_FALSE);
    }

    // Check if Falcon wants REQUIRE_CTX
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE, flags))
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, pFlcnCfg, LW_TRUE);
    }
    else
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, pFlcnCfg, LW_FALSE);
    }

    //
    // Note, this instruction must be at the end of function.
    // Only everything is programmed correctly; then we just enable CPUCTL_ALIAS_EN.
    // LWENC, LWDEC1 and OFA are all NS falcons, we just need to set CPUCTL_ALIAS_EN to true.
    //
    data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _TRUE, 0);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data);

    return status;
}

#else

/*!
 * @brief _acrSetupLSFalcon:  Copies IMEM and DMEM into falcon
 *
 * @param[in] pWprBase   : Base addr of WPR Region
 * @param[in] regionId   : ACR Region ID
 * @param[in] pWprheader : WPR Header
 * @param[in] pFlcnCfg   : Falcon Config register
 * @param[in] pLsbHeader : LSB Header
 */
FLCN_STATUS
_acrSetupLSFalcon
(
    PRM_FLCN_U64     pWprBase,
    LwU32            regionId,
    PLSF_WPR_HEADER  pWprHeader,
    PACR_FLCN_CONFIG pFlcnCfg,
    PLSF_LSB_HEADER  pLsbHeader
)
{
    ACR_STATUS  status = ACR_OK;
    LwU32       addrHi;
    LwU32       addrLo;
    LwU32       wprBase;
    LwU32       blWprBase;
    LwU32       dst;

    //
    // Earlier wprOffset was added to lower address and then lower address was
    // checked for overflow. Gourav found that wproffset is always 0.
    // Hence we forced wprOffset as 0 in this task. We are still confirming with
    // Gobi, if there are cases where we can have wprOffset as non zero.
    //
    addrHi = pWprBase->hi;
    addrLo = pWprBase->lo;

    wprBase = (addrLo >> FLCN_IMEM_BLK_ALIGN_BITS) |
              (addrHi << (32 - FLCN_IMEM_BLK_ALIGN_BITS));

    pLsbHeader->blCodeSize = LW_ALIGN_UP(pLsbHeader->blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupTargetRegisters_HAL(pAcrlib, pFlcnCfg));

    // Check if this falcon should be loaded via priv copy or DMA
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _FORCE_PRIV_LOAD, _TRUE, pLsbHeader->flags))
    {
        // Load ucode into IMEM
        CHECK_FLCN_STATUS_AND_RETURN_ACR_STATUS_IF_NOT_OK(acrPrivLoadTargetFalcon_HAL(&Acr, 0,
              pLsbHeader->ucodeOffset + pLsbHeader->appCodeOffset, pLsbHeader->appCodeSize,
              regionId, LW_TRUE, pFlcnCfg));
        // Load data into DMEM
        CHECK_FLCN_STATUS_AND_RETURN_ACR_STATUS_IF_NOT_OK(acrPrivLoadTargetFalcon_HAL(&Acr, 0,
              pLsbHeader->ucodeOffset + pLsbHeader->appDataOffset, pLsbHeader->appDataSize,
              regionId, LW_FALSE, pFlcnCfg));

        // Set the BOOTVEC
        acrlibSetupBootvec_HAL(pAcrlib, pFlcnCfg, 0);
    }
    else
    {
        //
        // BL starts at offset, but since the IMEM VA is not zero, we need to make
        // sure FBOFF is equal to the expected IMEM VA. So adjusting the FB BASE to
        // make sure FBOFF equals to VA as expected.
        //
        blWprBase = ((wprBase) +
                     ((pLsbHeader->ucodeOffset) >> FLCN_IMEM_BLK_ALIGN_BITS)) -
                    ((pLsbHeader->blImemOffset) >> FLCN_IMEM_BLK_ALIGN_BITS);

        // Check if code needs to be loaded at start of IMEM or at end of IMEM
        if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE, pLsbHeader->flags))
        {
            dst = 0;
        }
        else
        {
            dst = (acrlibFindFarthestImemBl_HAL(pAcrlib, pFlcnCfg, pLsbHeader->blCodeSize) *
                   FLCN_IMEM_BLK_SIZE_IN_BYTES);
        }

        // Load ucode to IMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
              dst, blWprBase, pLsbHeader->blImemOffset, pLsbHeader->blCodeSize,
              regionId, LW_TRUE, LW_TRUE, LW_FALSE, pFlcnCfg));

        // Load data into DMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
              0, wprBase, pLsbHeader->blDataOffset, pLsbHeader->blDataSize,
              regionId, LW_TRUE, LW_FALSE, LW_FALSE, pFlcnCfg));

        // Set the BOOTVEC
        acrlibSetupBootvec_HAL(pAcrlib, pFlcnCfg, pLsbHeader->blImemOffset);
    }

    // Check if Falcon wants virtual ctx
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE, pLsbHeader->flags))
    {
        acrlibSetupCtxDma_HAL(pAcrlib, pFlcnCfg, pFlcnCfg->ctxDma, LW_FALSE);
    }

    // Check if Falcon wants REQUIRE_CTX
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE, pLsbHeader->flags))
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, pFlcnCfg, LW_TRUE);
    }
    else
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, pFlcnCfg, LW_FALSE);
    }
    //
    // Program the IMEM and DMEM PLMs to the final desired values (which may
    // be level 3 for certain falcons) now that acrlib is done loading the
    // code onto the falcon.
    //
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM);

    return status;
}
#endif

/*!
 * @brief _acrSendFalconStatusToRm:  Sends message back to RM
 *
 * @param[in] status      : Success/Error status for Input command
 * @param[in] falconId    : falcon ID of bootstrapped falcon
 * @param[in] msgType     : Message type
 * @param[in] seqNum      : Sequenec Number associated with command
 */
void
_acrSendFalconStatusToRm
(
    LwU32    status,
    LwU32    falconId,
    LwU32    falconInstance,
    LwU32    msgType,
    LwU32    seqNum
)
{
    RM_FLCN_QUEUE_HDR hdr;
    RM_SEC2_ACR_MSG msg;

    hdr.unitId    = RM_SEC2_UNIT_ACR;
    hdr.seqNumId  = seqNum;
    hdr.ctrlFlags = 0;
    hdr.size      = RM_SEC2_MSG_SIZE(ACR, BOOTSTRAP_FALCON);

    // Error code is used across all the message types, so avoiding switch-case.
    msg.msgFlcn.errorCode   = status;

    // Just fill the msg_type itself, to avoid switch-case for now.
    msg.msgType = msgType;
    switch (msgType)
    {
        case RM_SEC2_MSG_TYPE(ACR, BOOTSTRAP_FALCON):
            msg.msgFlcn.falconId        = falconId;
            msg.msgFlcn.falconInstance  = falconInstance;
        break;

        case RM_SEC2_MSG_TYPE(ACR, ILWALID_COMMAND):
        break;

        default:
           return;
    }

    // Post the reply.
    osCmdmgmtRmQueuePostBlocking(&hdr, &msg);
}

/*!
 * @brief _acrGatherWprRegionDetails
 *        Initiazes WPR  region details. WPR is a client of ACR and We can have
 *        multiple clients. WPR region is used to store the details of LS falcon
 *        and after SEC2 Boot, SEC2 ACR task assumes WPR region ID as 1 and
 *        WPR offset as 0 i.e WPR start address is same as region start address.
 */
FLCN_STATUS
_acrGatherWprRegionDetails(void)
{
    LwU64           wprStartAddress = 0;
    LwU64           wprEndAddress   = 0;
    LwU64           wprRegionSize   = 0;
    FLCN_STATUS     status          = FLCN_OK;

    Acr.wprOffset   = 0;
    Acr.wprRegionId = LSF_WPR_EXPECTED_REGION_ID;

    acrGetRegionDetails_HAL(&Acr, Acr.wprRegionId, &wprStartAddress,
                                &wprEndAddress, NULL, NULL);

    if (!wprStartAddress || !wprEndAddress)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    LW64_FLCN_U64_PACK(&Acr.wprRegionAddress.address, &wprStartAddress);
    _lw64Sub(&wprRegionSize, &wprEndAddress, &wprStartAddress);

    Acr.wprRegionAddress.params =
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, (LwU32)wprRegionSize) |
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_SEC2_DMAIDX_UCODE);

    status = _acrReadWprHeader();

    return status;
}

/*!
 * @brief   Performs 64-bit subtraction (dst = src1 - src2)
 * Substitute for lib_lw64.h function, as it was increasing resident code size.
 */
void
_lw64Sub
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    *pDst = *pSrc1 - *pSrc2;
}

/*!
 * @brief   Adds 32-bit value to 64-bit operand (op64 += val32)
 * Substitute for lib_lw64.h function, as it was increasing resident code size.
 */
void
_lw64Add32
(
    LwU64  *pOp64,
    LwU32   val32
)
{
    LWU64_TYPE op64;

    op64.val = *pOp64;
    LWU64_LO(op64) += val32;
    if (LWU64_LO(op64) < val32)
    {
        LWU64_HI(op64)++;
    }
    *pOp64 = op64.val;
}

/*!
 * @brief   Update read WPR header in OBJACR from FB
 */
static FLCN_STATUS
_acrReadWprHeader(void)
{
    FLCN_STATUS status;

#ifdef NEW_WPR_BLOBS
    // Read whole WPR header
    status = dmaRead_hs((void *)Acr.pWpr, &Acr.wprRegionAddress,
                            Acr.wprOffset, LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX);
#else
    // Read whole WPR header
    status = dmaRead_hs((void *)Acr.pWpr, &Acr.wprRegionAddress,
                            Acr.wprOffset, LSF_WPR_HEADERS_TOTAL_SIZE_MAX);
#endif

    return status;
}


#ifdef NEW_WPR_BLOBS
/*!
 * @brief _acrReadLsbHeaderWrapper:  Reads LSB header wrapper  of Falcon from FB
 *
 * @param[in] pWprHeaderWrapper     : WPR header wrapper of falcon
 * @param[out] pLsbHeaderWrapper    : LSB header wrapper of Falcon
 */
static FLCN_STATUS
_acrReadLsbHeaderWrapper
(
    PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper,
    PLSF_LSB_HEADER_WRAPPER pLsbHeaderWrapper
)
{
    LwU32      lsbSize = LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE;
    LwU32      lsbOffset;
    ACR_STATUS status;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                        pWprHeaderWrapper, (void *)&lsbOffset));

    return dmaRead_hs((void *)pLsbHeaderWrapper, &Acr.wprRegionAddress,
                               lsbOffset, lsbSize);
}

#else

/*!
 * @brief _acrReadLsbHeader:  Reads LSB header of Falcon from FB
 *
 * @param[in] pWprHeader     : WPR header of falcon
 * @param[out] pLsbHeader    : LSB header of Falcon
 */
static FLCN_STATUS
_acrReadLsbHeader
(
    PLSF_WPR_HEADER pWprHeader,
    PLSF_LSB_HEADER pLsbHeader
)
{
    LwU32   lsbSize = LW_ALIGN_UP(sizeof(LSF_LSB_HEADER),
                                  LSF_LSB_HEADER_ALIGNMENT);

    return dmaRead_hs((void *)pLsbHeader, &Acr.wprRegionAddress,
                               pWprHeader->lsbOffset, lsbSize);
}

#endif


#ifdef NEW_WPR_BLOBS

/*!
 * @brief _acrIsNsFalcon
 *        Check if input falconId is boot into NS mode
 *
 * @param[in] falconId              Falcon ID which needs to be reset
 * @param[in] falconInstance        Falcon Instance/Index of same falcon to be bootstrapped
 *
 * @return LwBool:  LW_TRUE if input falconId will boot into NS mode
 */
static LwBool
_acrIsNsFalcon
(
    LwU32  falconId,
    LwU32  falconInstance
)
{
    switch(falconId)
    {
        case LSF_FALCON_ID_LWENC:
        case LSF_FALCON_ID_LWDEC1:
        case LSF_FALCON_ID_OFA:
        case LSF_FALCON_ID_LWJPG:
            return LW_TRUE;
        default:
            return LW_FALSE;
    }

    return LW_FALSE;
}

/*!
 * @brief _acrBootstrapFalcon
 *        Falcon's UC is selwred in ACR region and only LS clients like SEC2 can
 *        access it. During resetting an engine, RM needs to contact SEC2 to reset
 *        particular falcon.
 *
 * @param[in] falconId              Falcon ID which needs to be reset
 * @param[in] falconInstance        Falcon Instance/Index of same falcon to be bootstrapped
 * @param[in] flags                 Flags used for Bootstraping Falcon
 * @param[in] bUseVA                Set if Falcon uses Virtual Address
 * @param[in] pWprBaseVirtual       VA corresponding to WPR start
 *
 * @return FLCN_STATUS:    OK if successful reset of Falcon
 */
FLCN_STATUS
_acrBootstrapFalcon
(
    LwU32           falconId,
    LwU32           falconInstance,
    LwU32           flags,
    LwBool          bUseVA,
    PRM_FLCN_U64    pWprBaseVirtual
)
{
    ACR_STATUS      status        = ACR_OK;
    ACR_STATUS      statusCleanup = ACR_OK;
    PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper;
    ACR_FLCN_CONFIG flcnCfg;
    PLSF_LSB_HEADER_WRAPPER pLsbHeaderWrapper;
    PRM_FLCN_U64    pWprBase;
    LwU32           targetMaskPlmOldValue = 0;
    LwU32           targetMaskOldValue    = 0;
    LwBool          isNsFalcon            = LW_FALSE;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, falconInstance, &flcnCfg));

    pLsbHeaderWrapper = (PLSF_LSB_HEADER_WRAPPER)lsbHeaderWrapper;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetWprHeader(falconId, &pWprHeaderWrapper));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrReadLsbHeaderWrapper(pWprHeaderWrapper, pLsbHeaderWrapper));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLockFalconRegSpace_HAL(pAcrlib, ACRLIB_RUNNING_FALCON, &flcnCfg,
                                        LW_TRUE, &targetMaskPlmOldValue, &targetMaskOldValue));

    if (!FLD_TEST_DRF(_RM_SEC2_ACR, _CMD_BOOTSTRAP, _FALCON_FLAGS_RESET, _NO, flags) ||
        (falconId != LSF_FALCON_ID_LWDEC))
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibResetFalcon_HAL(pAcrlib, &flcnCfg));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibPollForScrubbing_HAL(pAcrlib, &flcnCfg));
    }

#ifdef WAR_SELWRE_THROTTLE_MINING_BUG_3263169
    //
    // Secure PLMs (and a decode trap) for PMU and FBFALCON for mining throttling.
    // This must be done right here, after engine is reset (otherwise the reset above would erase
    // the PLMs), and before lsrAcrSetupLSRiscvExt_HAL configures LW_PPWR_RISCV_BCR_DMACFG_SEC
    // (which, out of reset, has a PLM allowing only L3 writes). This way, even if SEC2 fails
    // before completing the secure-throttle mining WAR, the attacker won't be able to launch PMU!
    //
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(sec2SelwreThrottleMiningWAR_HAL(&Acr, falconId));
#endif // WAR_SELWRE_THROTTLE_MINING_BUG_3263169

    // Setup the LS falcon
    if (!bUseVA)
    {
        pWprBase = &Acr.wprRegionAddress.address;
    }
    else
    {
        pWprBase = pWprBaseVirtual;
    }

#ifdef ACR_RISCV_LS
    if (flcnCfg.uprocType == ACR_TARGET_ENGINE_CORE_RISCV)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(lsrAcrSetupLSRiscvExt_HAL(&Acr, pWprBase, Acr.wprRegionId, &flcnCfg, pLsbHeaderWrapper));
    }
    else if (flcnCfg.uprocType == ACR_TARGET_ENGINE_CORE_RISCV_EB)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(lsrAcrSetupLSRiscvExternalBoot_HAL(&Acr, pWprBase, Acr.wprRegionId, &flcnCfg, pLsbHeaderWrapper));
    }
    else
#endif
    {
        isNsFalcon = _acrIsNsFalcon(falconId, falconInstance);

        if (isNsFalcon)
        {
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrSetupNSFalcon(pWprBase, Acr.wprRegionId, pWprHeaderWrapper, &flcnCfg, pLsbHeaderWrapper));
        }
        else
        {
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrSetupLSFalcon(pWprBase, Acr.wprRegionId, pWprHeaderWrapper, &flcnCfg, pLsbHeaderWrapper));
        }
    }

    //
    // Enable AES HS mode for LWDEC0 steady state binary, since it is not yet
    //  colwerted to PKC.
    //
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibEnableAndSelectAES_HAL(pAcrlib, falconId));

Cleanup:
    statusCleanup = acrlibLockFalconRegSpace_HAL(pAcrlib, ACRLIB_RUNNING_FALCON, &flcnCfg, LW_FALSE,
                                                    &targetMaskPlmOldValue, &targetMaskOldValue);

    return (status != ACR_OK ? status : statusCleanup);
}

#else

FLCN_STATUS
_acrBootstrapFalcon
(
    LwU32           falconId,
    LwU32           falconInstance,
    LwU32           flags,
    LwBool          bUseVA,
    PRM_FLCN_U64    pWprBaseVirtual
)
{
    ACR_STATUS      status        = ACR_OK;
    ACR_STATUS      statusCleanup = ACR_OK;
    PLSF_WPR_HEADER pWprHeader;
    ACR_FLCN_CONFIG flcnCfg;
    PLSF_LSB_HEADER pLsbHeader;
    PRM_FLCN_U64    pWprBase;
    LwU32           targetMaskPlmOldValue = 0;
    LwU32           targetMaskOldValue    = 0;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, falconInstance, &flcnCfg));

    pLsbHeader = (PLSF_LSB_HEADER)lsbHeader;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetWprHeader(falconId, &pWprHeader));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrReadLsbHeader(pWprHeader, pLsbHeader));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLockFalconRegSpace_HAL(pAcrlib, ACRLIB_RUNNING_FALCON,
                                        &flcnCfg, LW_TRUE, &targetMaskPlmOldValue, &targetMaskOldValue));

    if (!FLD_TEST_DRF(_RM_SEC2_ACR, _CMD_BOOTSTRAP, _FALCON_FLAGS_RESET, _NO, flags) ||
        (falconId != LSF_FALCON_ID_LWDEC))
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibResetFalcon_HAL(pAcrlib, &flcnCfg));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibPollForScrubbing_HAL(pAcrlib, &flcnCfg));
    }

    // Setup the LS falcon
    if (!bUseVA)
    {
        pWprBase = &Acr.wprRegionAddress.address;
    }
    else
    {
        pWprBase = pWprBaseVirtual;
    }

#ifdef ACR_RISCV_LS
    if (flcnCfg.uprocType == ACR_TARGET_ENGINE_CORE_RISCV)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(lsrAcrSetupLSRiscv_HAL(&Acr, pWprBase, Acr.wprRegionId, &flcnCfg, pLsbHeader));
    }
    else
#endif
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrSetupLSFalcon(pWprBase, Acr.wprRegionId, pWprHeader, &flcnCfg, pLsbHeader));
    }

    //
    // Enable AES HS mode for LWDEC0 steady state binary, since it is not yet
    //  colwerted to PKC.
    //
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibEnableAndSelectAES_HAL(pAcrlib, falconId));

Cleanup:
    statusCleanup = acrlibLockFalconRegSpace_HAL(pAcrlib, ACRLIB_RUNNING_FALCON, &flcnCfg, LW_FALSE,
                                            &targetMaskPlmOldValue, &targetMaskOldValue);

    return (status != ACR_OK ? status : statusCleanup);
}
#endif
