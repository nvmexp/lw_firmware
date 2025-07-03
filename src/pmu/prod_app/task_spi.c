/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_spi.c
 * @brief  PMU SPI Task
 *
 * <more detailed description to follow>
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "task_spi.h"
#include "os/pmu_lwos_task.h"
#include "pmu_objspi.h"
#include "pmu_objpmu.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Mspios and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_spiEventHandle(DISPATCH_SPI *pRequest);

lwrtosTASK_FUNCTION(task_spi, pvParameters);

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the SPI task.
 */
typedef union
{
    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_SPI_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_SPI;

/*!
 * @brief   Defines the command buffer for the SPI task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(SPI, "dmem_spiCmdBuffer");

/* ------------------------- Global Variables ------------------------------- */
OBJSPI    Spi
    GCC_ATTRIB_SECTION("dmem_spi", "Spi");

/*!
 * Array of handler entries for RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 */
BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY s_spiBoardObjGrpHandlers[]
    GCC_ATTRIB_SECTION(LW_ARCH_VAL("dmem_spi", "dmem_globalRodata"), "s_spiBoardObjGrpHandlers") =
{
#if (PMUCFG_FEATURE_ENABLED(PMU_SPI_DEVICE))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_SET_ONLY(SPI, SPI_DEVICE,
        spiDeviceBoardObjGrpIfaceModel10Set),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_SPI_DEVICE))
};

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the SPI Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
spiPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 queueSize = 1;

    OSTASK_CREATE(status, PMU, SPI);
    if (status != FLCN_OK)
    {
        goto spiPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    {
        // Ensure that we have queue slot for each callback.
        queueSize++;    // OsCBSpi
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, SPI), queueSize,
                                     sizeof(DISPATCH_SPI));
    if (status != FLCN_OK)
    {
        goto spiPreInitTask_exit;
    }

spiPreInitTask_exit:
    return status;
}

/*!
 * Entry point for the SPI task. This task does not receive any parameters.
 */
lwrtosTASK_FUNCTION(task_spi, pvParameters)
{
    DISPATCH_SPI  dispatch;

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, SPI), &dispatch, status, PMU_TASK_CMD_BUFFER_PTRS_GET(SPI))
    {
        status = s_spiEventHandle(&dispatch);
    }
    LWOS_TASK_LOOP_END(status);
#else
    lwosVarNOP(PMU_TASK_CMD_BUFFER_PTRS_GET(SPI));

    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&SpiOsTimer, LWOS_QUEUE(PMU, SPI),
                                                  &dispatch, lwrtosMAX_DELAY)
        {
            (void)s_spiEventHandle(&dispatch);
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&SpiOsTimer, lwrtosMAX_DELAY)
    }
#endif
}

/*!
 * @brief   Exelwtes generic SPI_INIT RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_SPI_INIT
 */
FLCN_STATUS
pmuRpcSpiInit
(
    RM_PMU_RPC_STRUCT_SPI_INIT *pParams
)
{
    if (pParams->accessStart > pParams->accessEnd)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return spiInit(pParams->devIndex, pParams->accessStart,
                pParams->accessEnd - pParams->accessStart);
}

/*!
 * @brief   Exelwtes generic SPI_ERASE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_SPI_ERASE
 */
FLCN_STATUS
pmuRpcSpiErase
(
    RM_PMU_RPC_STRUCT_SPI_ERASE *pParams
)
{
    return spiErase(pParams->devIndex, pParams->offset, pParams->size);
}

/*!
 * @brief   Exelwtes generic SPI_READ RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_SPI_READ
 * @param[in]       pBuffer Scratch buffer address and size structure
 */
FLCN_STATUS
pmuRpcSpiRead
(
    RM_PMU_RPC_STRUCT_SPI_READ *pParams,
    PMU_DMEM_BUFFER            *pBuffer
)
{
    FLCN_STATUS status;

#if PMUCFG_FEATURE_ENABLED(PMU_FBQ) && PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
    if (pBuffer->size < RM_PMU_SPI_XFER_MAX_BLOCK_SIZE)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto pmuRpcSpiRead_exit;
    }
#endif

    status =
        spiRead(pParams->devIndex, pParams->offset, pParams->size, pBuffer->pBuf);
    goto pmuRpcSpiRead_exit;

pmuRpcSpiRead_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic SPI_WRITE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_SPI_WRITE
 * @param[in]       pBuffer Scratch buffer address and size structure
 */
FLCN_STATUS
pmuRpcSpiWrite
(
    RM_PMU_RPC_STRUCT_SPI_WRITE *pParams,
    PMU_DMEM_BUFFER             *pBuffer
)
{
    FLCN_STATUS status;

#if PMUCFG_FEATURE_ENABLED(PMU_FBQ) && PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
    if (pBuffer->size < RM_PMU_SPI_XFER_MAX_BLOCK_SIZE)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto pmuRpcSpiWrite_exit;
    }
#endif

    status =
        spiWrite(pParams->devIndex, pParams->offset, pParams->size, pBuffer->pBuf);
    goto pmuRpcSpiWrite_exit;

pmuRpcSpiWrite_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic SPI_DEINIT RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_SPI_DEINIT
 */
FLCN_STATUS
pmuRpcSpiDeinit
(
    RM_PMU_RPC_STRUCT_SPI_DEINIT *pParams
)
{
    return spiDeInit(pParams->devIndex);
}

/*!
 * @brief   Exelwtes generic BOARD_OBJ_GRP_CMD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_SPI_BOARD_OBJ_GRP_CMD
 * @param[in]       pBuffer Scratch buffer address and size structure
 */
FLCN_STATUS
pmuRpcSpiBoardObjGrpCmd
(
    RM_PMU_RPC_STRUCT_SPI_BOARD_OBJ_GRP_CMD *pParams,
    PMU_DMEM_BUFFER                         *pBuffer
)
{
    FLCN_STATUS status  = FLCN_ERR_FEATURE_NOT_ENABLED;

    // Note: pBuffer->size is validated in boardObjGrpIfaceModel10GetStatus_SUPER

    switch (pParams->commandId)
    {
        case RM_PMU_BOARDOBJGRP_CMD_SET:
        {
            OSTASK_OVL_DESC     ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, spiLibConstruct)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(s_spiBoardObjGrpHandlers),    // numEntries
                    s_spiBoardObjGrpHandlers,                       // pEntries
                    pParams->commandId);                            // commandId
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        default:
        {
            status = FLCN_ERR_ILWALID_INDEX;
            break;
        }
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Helper call handling events sent to SPI task.
 */
static FLCN_STATUS
s_spiEventHandle
(
    DISPATCH_SPI *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pRequest->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = pmuRpcProcessUnitSpi(&(pRequest->rpc));
            break;
        }
    }

    return status;
}
