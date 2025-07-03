/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   libintfchdcp22wired.c
 * @brief  Container-object for the GSP Hdcp 2.2 routines.  Contains
 *         generic non-HAL interrupt-routines plus logic required to hook-up
 *         chip-specific HAL-routines.
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "gspsw.h"

/* ------------------------ OpenRTOS Includes ------------------------------ */
#include "lib_intfchdcp22wired.h"
#include "tasks.h"
#include "partswitch.h"
#include "hdcp22wired_selwreaction.h"

/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ Global Variables ------------------------------- */
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
 * @brief This is a library interface function to provide secure action argument storage.
 *
 * @return     Address of selwreAction arguement storage.
 */
sysSYSLIB_CODE void*
libIntfcHdcp22wiredGetSelwreActionArgument(void)
{
#if LWRISCV_PARTITION_SWITCH
    return (void *)PARTSWITCH_SHARED_CARVEOUT_VA;
#else
    return NULL;
#endif // LWRISCV_PARTITION_SWITCH
}

/*!
 * @brief This is a library interface function for secure action.
 *
 * @param[in]  pArg             Pointer to secure action structure.
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
sysSYSLIB_CODE FLCN_STATUS
libIntfcHdcp22wiredSelwreAction
(
    SELWRE_ACTION_ARG  *pSecActArg
)
{
    // TODO stubbed
    return FLCN_OK;
}

/*!
 * @brief This is a library interface function for sending hdcp22 response to RM.
 *
 * @param[in]  hdr              Header preceding each MSG to RM
 * @param[in]  hdcp22msg        MSG data
 *
 * @return     FLCN_STATUS      FLCN_OK if successful
 */
sysSYSLIB_CODE FLCN_STATUS
libIntfcHdcp22SendResponse
(
    RM_FLCN_QUEUE_HDR   hdr,
    RM_FLCN_HDCP22_MSG  hdcp22msg
)
{
    RM_FLCN_MSG_GSP gspMsg;

    gspMsg.hdr = hdr;
    gspMsg.msg.hdcp22wired = hdcp22msg;

    return lwrtosQueueSendBlocking(rmMsgRequestQueue, &gspMsg, sizeof(gspMsg));
}

