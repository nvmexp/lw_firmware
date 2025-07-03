/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_bif.c
 * @brief  PMU Bus Interface Task
 *
 * This task performs various bus interface logic, including changing bus
 * speed and ASPM.
 */

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

/* ------------------------- Application Includes -------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "os/pmu_lwos_task.h"
#include "pmu_objbif.h"
#include "pmu_objpmu.h"
#include "pmu_objclk.h"
#include "pmu_xusb.h"
#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Static Variables ------------------------------ */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the BIF task.
 */
typedef union
{
    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_BIF_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_BIF;

/*!
 * @brief   Defines the command buffer for the BIF task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(BIF, "dmem_bifCmdBuffer");

/* ------------------------- Global Variables ------------------------------ */
OBJBIF Bif
    GCC_ATTRIB_SECTION("dmem_bif", "Bif");
OBJBIF_RESIDENT BifResident;

OBJBIF_MARGINING MarginingData
    GCC_ATTRIB_SECTION("dmem_bifLaneMargining", "MarginingData");

LwrtosTaskHandle OsTaskBif;

/* ------------------------- Prototypes ------------------------------------ */
static FLCN_STATUS s_bifEventHandle          (DISPATCH_BIF *pRequest);
#if (PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB))
static FLCN_STATUS s_bifEventXUsbToPmuMsgbox (void);
static FLCN_STATUS s_bifEventPmuToXUsbAck    (void);
static LwU8   s_bifGetMaxSupportedLinkSpeed  (void);
static LwBool s_bifIsPcieClkDomainTypeFixed  (void);
#endif // PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB)

lwrtosTASK_FUNCTION(task_bif, pvParameters);

/* ------------------------- Private Functions ------------------------------ */

#if (PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB))

static FLCN_STATUS
s_bifEventXUsbToPmuMsgbox(void)
{
    // Init debug-mode event structure
    PMU_RM_RPC_STRUCT_BIF_USBC_DEBUG rpc;
    FLCN_STATUS status = FLCN_OK;

    memset(&rpc, 0x00, sizeof(rpc));
    rpc.flcnStatus = FLCN_ERR_RPC_ILWALID_INPUT;

    LwU8 msgBox = 0;

    if (bifReadXusbToPmuMsgbox_HAL(&Bif, &msgBox))
    {
        LwU8 tempMsgBox = 0;

        // Parse the command type and forward command to appropriate task
        LwU8 cmdType = DRF_VAL(_XUSB2PMU, _MSGBOX_CMD, _TYPE, msgBox);
        switch (cmdType)
        {
            case LW_XUSB2PMU_MSGBOX_CMD_TYPE_PCIE_REQ:
            {
                // Send back immediate generic ACK to indicate PMU received and will queue request from XUSB to arbiter
                tempMsgBox = FLD_SET_DRF(_XUSB2PMU, _RDAT_REPLY, _TYPE, _GENERIC_ACK, tempMsgBox);
                bifWriteXusbToPmuAckReply_HAL(&Bif, tempMsgBox);

                // If BIF task is not loaded or pcie genclk is a FIXED domain then bail early.
                if ((!BIF_IS_LOADED()) ||
                    (s_bifIsPcieClkDomainTypeFixed()))
                {
                    goto bif_skip_pcie_request;
                }

                LwU8 payload = DRF_VAL(_XUSB2PMU, _MSGBOX_CMD, _PAYLOAD, msgBox);
                switch (payload)
                {
                    case LW_XUSB2PMU_MSGBOX_CMD_PAYLOAD_PCIE_REQ_UNLOCK:
                    {
                        // Clear perf limit
                        if (PMUCFG_FEATURE_ENABLED(PMU_BIF_PMU_PERF_LIMIT))
                        {
                            OSTASK_OVL_DESC ovlDescList[] = {
                                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
                            };

                            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                            {
                                status = bifSetPmuPerfLimits(
                                            LW2080_CTRL_PERF_PERF_LIMIT_ID_BIF_USBC_PMU_PCIE_MIN,
                                            LW_TRUE,
                                            BIF_LINK_SPEED_LOCK_GEN1/*unused*/);
                            }
                            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

                            // Populate debug-mode event structure
                            if (Bif.bUsbcDebugMode)
                            {
                                rpc.eventMask = FLD_SET_DRF(_RM_PMU_BIF,
                                                             _USBC_DEBUG_EVENT,
                                                             _REQ_TYPE_PCIE, _UNLOCK,
                                                             rpc.eventMask);
                                rpc.flcnStatus = status;
                            }
                        }
                        break;
                    }

                    case LW_XUSB2PMU_MSGBOX_CMD_PAYLOAD_PCIE_REQ_LOCK:
                    {
                        // Find max supported GENspeed to lock to
                        LwU8 lockSpeed = s_bifGetMaxSupportedLinkSpeed();

                        // Set perf arbiter limit to lock to above callwlated genspeed
                        if (PMUCFG_FEATURE_ENABLED(PMU_BIF_PMU_PERF_LIMIT))
                        {
                            OSTASK_OVL_DESC ovlDescList[] = {
                                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
                            };

                            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                            {
                                status = bifSetPmuPerfLimits(
                                            LW2080_CTRL_PERF_PERF_LIMIT_ID_BIF_USBC_PMU_PCIE_MIN,
                                            LW_FALSE,
                                            lockSpeed);
                            }
                            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

                            // Populate debug-mode event structure
                            if (Bif.bUsbcDebugMode)
                            {
                                rpc.eventMask  = FLD_SET_DRF(_RM_PMU_BIF,
                                                             _USBC_DEBUG_EVENT,
                                                             _REQ_TYPE_PCIE, _LOCK,
                                                             rpc.eventMask);
                                rpc.flcnStatus = status;
                            }
                        }
                        break;
                    }
                }
bif_skip_pcie_request:
                //
                // Fire a notification message box to XUSB indicating PASS/FAIL status of the _PCIE_REQ
                // Since bif task queues a SYNChronous request, the arbiter wont return status till
                // request completes. So it is safe to fire status notification to XUSB here
                //
                tempMsgBox = 0;
                tempMsgBox = FLD_SET_DRF(_PMU2XUSB, _MSGBOX_CMD, _TYPE, _PCIE_REQ_STATUS, tempMsgBox);
                if (status == FLCN_OK)
                {
                    tempMsgBox = FLD_SET_DRF(_PMU2XUSB, _MSGBOX_CMD, _PAYLOAD,
                                               _PCIE_REQ_STATUS_PASS, tempMsgBox);
                }
                else
                {
                    tempMsgBox = FLD_SET_DRF(_PMU2XUSB, _MSGBOX_CMD, _PAYLOAD,
                                               _PCIE_REQ_STATUS_FAIL, tempMsgBox);
                }

                status = bifWritePmuToXusbMsgbox_HAL(&Bif, tempMsgBox);

                break;
            }
        }
    }

    // If debug mode is enabled, fire a debug event to RM
    if (Bif.bUsbcDebugMode)
    {
        PMU_RM_RPC_EXELWTE_BLOCKING(status, BIF, USBC_DEBUG, &rpc);
    }

    return status;
}

static FLCN_STATUS
s_bifEventPmuToXUsbAck(void)
{
    // Init debug-mode event structure
    PMU_RM_RPC_STRUCT_BIF_USBC_DEBUG rpc;
    FLCN_STATUS status = FLCN_OK;

    memset(&rpc, 0x00, sizeof(rpc));
    rpc.flcnStatus = FLCN_ERR_RPC_ILWALID_INPUT;

    // Read reply data sent by XUSB with ACK
    LwU8 ackReply = bifReadPmuToXusbAckReply_HAL(&Bif);

    // Parse the command type and forward command to appropriate task
    LwU8 ackReplyType = DRF_VAL(_PMU2XUSB, _RDAT_REPLY, _TYPE, ackReply);
    switch (ackReplyType)
    {
        case LW_PMU2XUSB_RDAT_REPLY_TYPE_GENERIC_ACK:
        {
            // Place holder for generic ACK from XUSB
            // Populate debug-mode event structure
            if (Bif.bUsbcDebugMode)
            {
                rpc.eventMask  = FLD_SET_DRF(_RM_PMU_BIF,
                                             _USBC_DEBUG_EVENT,
                                             _ACK_TYPE_GENERIC, _VAL,
                                             rpc.eventMask);
                rpc.flcnStatus = status;
            }
            break;
        }

        case LW_PMU2XUSB_RDAT_REPLY_TYPE_ISOCH_QUERY:
        {
            // Mark isoch reply received
            Bif.bIsochReplyReceived = LW_TRUE;

            // If BIF task is not loaded or pcie genclk is a FIXED domain then bail early.
            if ((!BIF_IS_LOADED()) ||
                (s_bifIsPcieClkDomainTypeFixed()))
            {
                goto bif_skip_isoch_request;
            }

            LwU8 payload = DRF_VAL(_PMU2XUSB, _RDAT_REPLY, _PAYLOAD, ackReply);
            switch (payload)
            {
                case LW_PMU2XUSB_RDAT_REPLY_PAYLOAD_ISOCH_QUERY_IDLE:
                {
                    // Clear perf limit
                    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_PMU_PERF_LIMIT))
                    {
                        OSTASK_OVL_DESC ovlDescList[] = {
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
                        };

                        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                        {
                            status = bifSetPmuPerfLimits(
                                        LW2080_CTRL_PERF_PERF_LIMIT_ID_BIF_USBC_PMU_PCIE_MIN,
                                        LW_TRUE,
                                        BIF_LINK_SPEED_LOCK_GEN1/*unused*/);
                        }
                        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

                        // Populate debug-mode event structure
                        if (Bif.bUsbcDebugMode)
                        {
                            rpc.eventMask  = FLD_SET_DRF(_RM_PMU_BIF,
                                                         _USBC_DEBUG_EVENT,
                                                         _ACK_TYPE_ISOCH, _IDLE,
                                                         rpc.eventMask);
                            rpc.flcnStatus = status;
                        }
                    }
                    break;
                }

                case LW_PMU2XUSB_RDAT_REPLY_PAYLOAD_ISOCH_QUERY_ACTIVE:
                {
                    // Find max supported GENspeed to lock to
                    LwU8 lockSpeed = s_bifGetMaxSupportedLinkSpeed();

                    // Set perf arbiter limit to lock to above callwlated genspeed
                    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_PMU_PERF_LIMIT))
                    {
                        OSTASK_OVL_DESC ovlDescList[] = {
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
                        };

                        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                        {
                            status = bifSetPmuPerfLimits(
                                        LW2080_CTRL_PERF_PERF_LIMIT_ID_BIF_USBC_PMU_PCIE_MIN,
                                        LW_FALSE,
                                        lockSpeed);
                        }
                        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

                        // Populate debug-mode event structure
                        if (Bif.bUsbcDebugMode)
                        {
                            rpc.eventMask  = FLD_SET_DRF(_RM_PMU_BIF,
                                                         _USBC_DEBUG_EVENT,
                                                         _ACK_TYPE_ISOCH, _ACTIVE,
                                                         rpc.eventMask);
                            rpc.flcnStatus = status;
                        }
                    }
                    break;
                }
            }

            //
            // Send a notification event to RM indicating that PMU has
            // received response to ISOCH query. This notification is used
            // by RM to clear the RM pcie genspeed PERF_LIMIT preset during RM bifStateLoad
            //
            PMU_RM_RPC_STRUCT_BIF_XUSB_ISOCH rpcIsoch;

            PMU_RM_RPC_EXELWTE_BLOCKING(status, BIF, XUSB_ISOCH, &rpcIsoch);
bif_skip_isoch_request:
            break;
        }
    }

    // If debug mode is enabled, fire a debug event to RM
    if (Bif.bUsbcDebugMode)
    {
        // RC-TODO, ensure proper error handling.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, BIF, USBC_DEBUG, &rpc);
    }

    return status;
}

/*!
 * @brief   Helper call to find max suppported link speed.
 */
static LwU8
s_bifGetMaxSupportedLinkSpeed(void)
{
    LwU8 maxSpeed = BIF_LINK_SPEED_LOCK_GEN1;

    //
    // USB ISOCH traffic latency requirement cannot tolerate a GEN speed change.
    // So we need to lock to highest supported GEN speed in these cases -
    //      i. when PMU is notified of upcoming ISOCH traffic on USB,
    //     ii. when PMU first boot straps and finds ISOCH traffic active on USB
    // ISOCH traffic also has a BW requirement, but by satisfying latency req above,
    // USB will automatically get the maximum bandwidth possible
    // So find max supported genspeed to lock to
    //
    while ((Bif.bifCaps &
            RM_PMU_BIF_CAPS_GENPCIE(RM_PMU_BIF_LINK_SPEED_GENPCIE(maxSpeed))) != 0)
    {
        maxSpeed++;
    }

    return maxSpeed;
}

/*!
 * @brief   Helper call to find if PCIE clock domain is FIXED.
 */
static LwBool
s_bifIsPcieClkDomainTypeFixed(void)
{
    CLK_DOMAIN *pClkDom           = NULL;
    LwBool      bClkDomainIsFixed = LW_FALSE;

    OSTASK_OVL_DESC ovlDescList[] = {
        // Required for clkDomainsGetByDomain and clkDomainIsFixed
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pClkDom = clkDomainsGetByDomain(clkWhich_PCIEGenClk);
        if (pClkDom == NULL)
        {
            PMU_BREAKPOINT();
        }

        bClkDomainIsFixed = clkDomainIsFixed(pClkDom);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return bClkDomainIsFixed;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB)

/*!
 * @brief   Helper call handling events sent to BIF task.
 */
static FLCN_STATUS
s_bifEventHandle
(
    DISPATCH_BIF *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pRequest->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;

            switch (pRequest->hdr.unitId)
            {
                case RM_PMU_UNIT_BIF:
                {
                    status = pmuRpcProcessUnitBif(&(pRequest->rpc));
                    break;
                }
            }
            break;
        }

        case BIF_EVENT_ID_LANE_MARGIN:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBifMargining)
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bifLaneMargining)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = bifDoLaneMargining_HAL(&Bif, pRequest->signalLaneMargining.laneNum);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            else
            {
                PMU_BREAKPOINT();
            }
            break;
        }

#if (PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB))
        case BIF_EVENT_ID_XUSB_TO_PMU_MSGBOX:
        {
            status = s_bifEventXUsbToPmuMsgbox();
            break;
        }

        case BIF_EVENT_ID_PMU_TO_XUSB_ACK:
        {
            status = s_bifEventPmuToXUsbAck();
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB)
    }

    return status;
}

/* ------------------------- Public Functions ------------------------------ */
/*!
 * @brief      Initialize the BIF Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
bifPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32   queueSize = 0;

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, BIF);
    if (status != FLCN_OK)
    {
        goto bifPreInitTask_exit;
    }

    //
    // Ensure we have queue slot for each possible scenarios
    // For blocking CMDs from RM(non-blocking CMDs are not allowed)
    //
    queueSize++;

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING))
    {
        // To handle Step Margining commands scheduled from ISR
        queueSize++;
        // Handle Margining command scheduled from task BIF(bifDoLaneMargining*)
        queueSize++;
        //
        // Slot for accomodating loss of synchronization between task Lowlatency
        // and task BIF
        //
        queueSize++;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB))
    {
        // for XUSB msgbox forwarded from ISR to BIF task for XUSB2PMU_REQ
        queueSize++;
        // for PMU msgbox forwarded from ISR to BIF task for PMU2XUSB_ACK rdata
        queueSize++;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    {
        // Assure that we have queue slot for each callback.
        queueSize++;    // OsCBBifXusbIsochTimeout
        // Handle continuous callbacks scheduled for Pex Power Savings
        queueSize++;    // OsCBBifCheckL1ResidencyTimeout
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, BIF), queueSize,
                                     sizeof(DISPATCH_BIF));
    if (status != FLCN_OK)
    {
        goto bifPreInitTask_exit;
    }

bifPreInitTask_exit:
    return status;
}

/*!
 * Entry point for the BIF task. This task does not receive any parameters,
 * and never returns.
 */
lwrtosTASK_FUNCTION(task_bif, pvParameters)
{
    DISPATCH_BIF dispatch;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBifInit)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (FLCN_OK != bifPostInit())
        {
            PMU_HALT();
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, BIF), &dispatch, status, PMU_TASK_CMD_BUFFER_PTRS_GET(BIF))
    {
        status = s_bifEventHandle(&dispatch);
    }
    LWOS_TASK_LOOP_END(status);
}
