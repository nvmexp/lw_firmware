/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   libintfchdcp1x.c
 * @brief  Container-object for the GSP Hdcp1.x routines.  Contains
 *         generic non-HAL interrupt-routines plus logic required to hook-up
 *         chip-specific HAL-routines.
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "gspsw.h"
#include "gsp_bar0.h"

/* ------------------------ OpenRTOS Includes ------------------------------ */
#include "lib_intfchdcp.h"
#include "tasks.h"
// TODO: Move hdcp1.x secure action structure to hdcp1.x header file.
#include "hdcp22wired_selwreaction.h"
#include "partswitch.h"

/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
// TEMP HACK
//
// After refactoring, this shared global data structure should be protected by
// a mutex or be removed completely.
//
extern LwrtosQueueHandle    rmMsgRequestQueue;

/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief This is a library interface function for accessing Secure Bus in HS mode
 *
 * @param[in]  bIsReadReq        Is secure bus read request or write request
 * @param[in]  addr              address of register to be read or written
 * @param[in]  dataIn            Value to be written (not applicable for read request)
 * @param[out] dDataOut          Pointer to be filled with read value (not applicable for write request)
 *
 * @return     FLCN_STATUS       FLCN_OK if read or write request is successful
 */
sysSYSLIB_CODE FLCN_STATUS
libIntfcHdcpSecBusAccess
(
    LwBool bIsReadReq,
    LwU32  addr,
    LwU32  dataIn,
    LwU32 *pDataOut
)
{
#if LWRISCV_PARTITION_SWITCH
    // TODO: remove the action handler.
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)PARTSWITCH_SHARED_CARVEOUT_VA;
    FLCN_STATUS         status  = FLCN_OK;

    if(bIsReadReq && pDataOut == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType                      = SELWRE_ACTION_REG_ACCESS;
    pArg->action.regAccessArg.bIsReadReq  = bIsReadReq;
    pArg->action.regAccessArg.addr        = addr;
    pArg->action.regAccessArg.val         = dataIn;

    status = hdcp22wiredMpSwitchSelwreAction();

    if (bIsReadReq)
    {
        // To read request, retVal is filled up in above function.
        if (status == FLCN_OK)
        {
            *pDataOut = pArg->action.regAccessArg.retVal;
        }
        else
        {
            *pDataOut  = 0xbadfffff;
        }
    }

    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
#endif

    return FLCN_OK;
}

/*!
 * @brief This is a library interface function for sending hdcp1.x response to RM.
 *
 * @param[in]  hdr              Header preceding each MSG to RM
 * @param[in]  msg              MSG data
 *
 * @return     FLCN_STATUS      FLCN_OK if successful
 */
sysSYSLIB_CODE FLCN_STATUS
libIntfcHdcpSendResponse
(
    RM_FLCN_QUEUE_HDR   hdr,
    RM_FLCN_HDCP_MSG    msg
)
{
    RM_FLCN_MSG_GSP gspMsg;

    gspMsg.hdr = hdr;
    gspMsg.msg.hdcp = msg;

    return lwrtosQueueSendBlocking(rmMsgRequestQueue, &gspMsg,
                                   sizeof(gspMsg));
}

