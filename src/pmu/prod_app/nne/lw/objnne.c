/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   objnne.c
 * @brief  Neural Net Engine (NNE) related interfaces
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "objnne.h"
#include "boardobj/boardobjgrp.h"
#include "nne/nne_parm_ram.h"
#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJNNE  Nne
    GCC_ATTRIB_SECTION("dmem_nne", "Nne");

OBJNNE_RESIDENT NneResident = {{0}};

BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY NneBoardObjGrpHandlers[]
GCC_ATTRIB_SECTION(LW_ARCH_VAL("dmem_nne", "dmem_globalRodata"), "NneBoardObjGrpHandlers") =
{
#if (PMUCFG_FEATURE_ENABLED(PMU_NNE_VAR))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(NNE, NNE_VAR,
        nneVarBoardObjGrpIfaceModel10Set, ga10xAndLater.nne.nneVarGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_NNE_VAR))

#if (PMUCFG_FEATURE_ENABLED(PMU_NNE_LAYER))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(NNE, NNE_LAYER,
        nneLayerBoardObjGrpIfaceModel10Set, ga10xAndLater.nne.nneLayerGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_NNE_LAYER))

#if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(NNE, NNE_DESC,
        nneDescBoardObjGrpIfaceModel10Set, ga10xAndLater.nne.nneDescGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC))
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc constructNne
 */
FLCN_STATUS
constructNne(void)
{
    return FLCN_OK;
}

/*!
 * @copydoc nnePreInit
 */
void
nnePreInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            nneDescPreInit(),
            nnePreInit_exit);
    }

nnePreInit_exit:
    // TODO-aherring: propagate errors
    if (status != FLCN_OK)
    {
        PMU_HALT();
    }
}

/*!
 * @copydoc nnePostInit
 */
FLCN_STATUS
nnePostInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Initialize OBJNNE
    memset(&Nne, 0x0, sizeof(OBJNNE));

    // NNE_VARS post-init.
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_VAR))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            nneVarsPostInit(),
            nnePostInit_exit);
    }

    // Run NNE_DESC post-init
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            nneDescPostInit(),
            nnePostInit_exit);
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamPostInit(),
        nnePostInit_exit);

nnePostInit_exit:
    return FLCN_OK;
}

/*!
 * @brief   Exelwtes generic BOARD_OBJ_GRP_CMD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_NNE_BOARD_OBJ_GRP_CMD
 * @param[in]       pBuffer Scratch buffer address and size structure
 *
 * @return   FLCN_OK    On success
 */
FLCN_STATUS
pmuRpcNneBoardObjGrpCmd
(
    RM_PMU_RPC_STRUCT_NNE_BOARD_OBJ_GRP_CMD *pParams,
    PMU_DMEM_BUFFER                         *pBuffer
)
{
    FLCN_STATUS status = FLCN_ERR_FEATURE_NOT_ENABLED;
 
    // Note: pBuffer->size is validated in boardObjGrpIfaceModel10GetStatus_SUPER

    switch (pParams->commandId)
    {
        case RM_PMU_BOARDOBJGRP_CMD_SET:
        {
            status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                pParams->classId,                            // classId
                pBuffer,                                     // pBuffer
                LW_ARRAY_ELEMENTS(NneBoardObjGrpHandlers),   // numEntries
                NneBoardObjGrpHandlers,                      // pEntries
                pParams->commandId);                         // commandId
            break;
        }

        case RM_PMU_BOARDOBJGRP_CMD_GET_STATUS:
        {
            status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                pParams->classId,                            // classId
                pBuffer,                                     // pBuffer
                LW_ARRAY_ELEMENTS(NneBoardObjGrpHandlers),   // numEntries
                NneBoardObjGrpHandlers,                      // pEntries
                pParams->commandId);                         // commandId
            break;
        }

        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            break;
        }
    }

    return status;
}

/*!
 * @copydoc nneService
 */
void
nneService(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC))
    {
        nneDescInterruptService_HAL(&Nne);
    }
}
