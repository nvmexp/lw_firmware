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
 * @file    cmdmgmt_instrumentation.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#if (PMUCFG_FEATURE_ENABLED(ARCH_RISCV))
#include "drivers/odp.h"
#endif

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "lwos_instrumentation.h"
#include "rmpmusupersurfif.h"
#include "cmdmgmt/cmdmgmt_instrumentation.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

#include "g_pmurpc.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Compile time checks ---------------------------- */
// Assure that the external indexes are always in sync with the internal ones.
#if (PMUCFG_FEATURE_ENABLED(ARCH_RISCV))
ct_assert(RM_PMU_ODP_STATS_ENTRY_ID_CODE_MISS       == ODP_STATS_ENTRY_ID_CODE_MISS);
ct_assert(RM_PMU_ODP_STATS_ENTRY_ID_DATA_MISS_CLEAN == ODP_STATS_ENTRY_ID_DATA_MISS_CLEAN);
ct_assert(RM_PMU_ODP_STATS_ENTRY_ID_DATA_MISS_DIRTY == ODP_STATS_ENTRY_ID_DATA_MISS_DIRTY);
ct_assert(RM_PMU_ODP_STATS_ENTRY_ID_MPU_MISS        == ODP_STATS_ENTRY_ID_MPU_MISS);
ct_assert(RM_PMU_ODP_STATS_ENTRY_ID_DMA_SUSPEND     == ODP_STATS_ENTRY_ID_DMA_SUSPENDED);
ct_assert(RM_PMU_ODP_STATS_ENTRY_ID_COMBINED        == ODP_STATS_ENTRY_ID_COMBINED);
ct_assert(RM_PMU_ODP_STATS_ENTRY_ID__COUNT          == ODP_STATS_ENTRY_ID__COUNT);
#endif

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Set up the super surface for instrumentation.
 *
 * @param[in/out]   pParams     RPC in/out parameters
 */
FLCN_STATUS
pmuInstrumentationBegin(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (LWOS_INSTR_IS_ENABLED())
    {
        RM_FLCN_MEM_DESC bufDesc = PmuInitArgs.superSurface;
        LwU64 superSurfaceQueueAddress;

        LwU32 bufferOffset = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(
                turingAndLater.instrumentation.instBuffer.data);
        LwU32 bufferSize = RM_PMU_SUPER_SURFACE_MEMBER_SIZE(
                turingAndLater.instrumentation.instBuffer.data);

        // Callwlate the super surface offset.
        LwU64_ALIGN32_UNPACK(&superSurfaceQueueAddress, &bufDesc.address);
        lw64Add32(&superSurfaceQueueAddress, bufferOffset);

        LwU32 superSurfaceSize = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                         PmuInitArgs.superSurface.params);

        //
        // If the allocated super surface size doesn't contain th whole buffer,
        // we messed up something RM-side when allocating the surface.
        //
        if (bufferOffset + bufferSize > superSurfaceSize)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
        }

        LwU64_ALIGN32_PACK(&bufDesc.address, &superSurfaceQueueAddress);

        bufDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                         bufferSize, bufDesc.params);

        // Initialize instrumentation with the buffer.
        LWOS_INSTRUMENTATION_BEGIN(&bufDesc);
    }

    return status;
}

/*!
 * @brief   Get static instrumentation info
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_INSTRUMENTATION_GET_INFO
 */
FLCN_STATUS
pmuRpcCmdmgmtInstrumentationGetInfo
(
    RM_PMU_RPC_STRUCT_CMDMGMT_INSTRUMENTATION_GET_INFO *pParams
)
{
    pParams->bEnabled = LWOS_INSTR_IS_ENABLED();
    pParams->bIsTimerPrecise = LWOS_INSTR_USE_PTIMER();

    return FLCN_OK;
}

/*!
 * @brief   Get instrumentation configuration
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_INSTRUMENTATION_GET_CONTROL
 */
FLCN_STATUS
pmuRpcCmdmgmtInstrumentationGetControl
(
    RM_PMU_RPC_STRUCT_CMDMGMT_INSTRUMENTATION_GET_CONTROL *pParams
)
{
    if (LWOS_INSTR_IS_ENABLED())
    {
        memcpy(pParams->mask, LwosInstrumentationEventMask,
               LW2080_CTRL_FLCN_LWOS_INST_MASK_SIZE_BYTES);
    }

    return FLCN_OK;
}

/*!
 * @brief   Set instrumentation configuration
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_INSTRUMENTATION_SET_CONTROL
 */
FLCN_STATUS
pmuRpcCmdmgmtInstrumentationSetControl
(
    RM_PMU_RPC_STRUCT_CMDMGMT_INSTRUMENTATION_SET_CONTROL *pParams
)
{
    if (LWOS_INSTR_IS_ENABLED())
    {
        memcpy(LwosInstrumentationEventMask, pParams->mask,
               LW2080_CTRL_FLCN_LWOS_INST_MASK_SIZE_BYTES);
    }

    return FLCN_OK;
}

/*!
 * @brief   Insert a recalibration event
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_INSTRUMENTATION_RECALIBRATE
 */
FLCN_STATUS
pmuRpcCmdmgmtInstrumentationRecalibrate
(
    RM_PMU_RPC_STRUCT_CMDMGMT_INSTRUMENTATION_RECALIBRATE *pParams
)
{
    lwosVarNOP(pParams);
    return LWOS_INSTRUMENTATION_RECALIBRATE();
}

/*!
 * @brief   Retrieves RISCV ODP profiling information.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_ODP_STATS_GET
 */
FLCN_STATUS
pmuRpcCmdmgmtOdpStatsGet
(
    RM_PMU_RPC_STRUCT_CMDMGMT_ODP_STATS_GET *pParams
)
{
#if (PMUCFG_FEATURE_ENABLED(ARCH_RISCV))
    if (LWOS_USTREAMER_IS_ENABLED())
    {
        LwU32 index;

        // Take care of the fact that timings are not supported for all events.
        memset(&(pParams->entry), 0x00, sizeof(pParams->entry));

        for (index = 0U; index < RM_PMU_ODP_STATS_ENTRY_ID__COUNT; index++)
        {
            LwU64_ALIGN32_PACK(&(pParams->entry[index].missCnt),
                               &(riscvOdpStats.missCnt[index]));
            if (index < ODP_STATS_ENTRY_ID__CAPTURE_TIME)
            {
                LwU64 timeSum =
                    riscvOdpStats.timeSum[index] << LWRISCV_MTIME_TICK_SHIFT;

                LwU64_ALIGN32_PACK(&(pParams->entry[index].timeSum), &timeSum);

                //
                // Min/max times after shifting are not expected to overflow
                // LwU32 since any ODP should be serviced within 4.29 seconds.
                //
                pParams->entry[index].timeMin =
                    riscvOdpStats.timeMin[index] << LWRISCV_MTIME_TICK_SHIFT;
                pParams->entry[index].timeMax =
                    riscvOdpStats.timeMax[index] << LWRISCV_MTIME_TICK_SHIFT;
            }
        }

        return FLCN_OK;
    }
    else
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }
#else
    return FLCN_ERR_NOT_SUPPORTED;
#endif
}

/*!
 * @brief   Retrieves data section usage info.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_DATA_SECTIONS_USAGE_GET
 */
FLCN_STATUS
pmuRpcCmdmgmtDataSectionsUsageGet
(
    RM_PMU_RPC_STRUCT_CMDMGMT_DATA_SECTIONS_USAGE_GET *pParams
)
{
#if (PMUCFG_FEATURE_ENABLED(ARCH_RISCV))
ct_assert(RM_PMU_UPROC_DATA_SECTION_COUNT_MAX > OVL_COUNT_DMEM());
#endif
    if (LWOS_USTREAMER_IS_ENABLED())
    {
        FLCN_STATUS status;
        LwU32 sectionIdx;

        for (sectionIdx = 0U;
             sectionIdx < LW_ARRAY_ELEMENTS(pParams->entry);
             sectionIdx ++)
        {
            status = lwosDataSectionMetadataGet(sectionIdx,
                &(pParams->entry[sectionIdx].sizeMax),
                &(pParams->entry[sectionIdx].sizeUsed));
            if (status != FLCN_OK)
            {
                break;
            }
        }

        if (status == FLCN_ERR_ILWALID_INDEX)
        {
            pParams->sectionCount = sectionIdx;
            status = FLCN_OK;
        }

        return status;
    }
    else
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }
}

/* ------------------------- Private Functions ------------------------------ */
