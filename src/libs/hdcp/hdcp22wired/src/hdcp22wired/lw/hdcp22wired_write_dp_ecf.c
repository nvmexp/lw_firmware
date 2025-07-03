/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_write_dp_ecf.c
 * @brief  Hdcp22 WriteDpEcf secure action handler.
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "flcnifcmn.h"
#include "hdcp22wired_cmn.h"
#include "mem_hs.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
FLCN_STATUS hdcp22wiredWriteDpEcfHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "hdcp22wiredWriteDpEcfHandler");

#ifdef GSPLITE_RTOS
static void _hdcp22wiredWriteDpEcfEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "start");
#endif

/* ------------------------ Global variables -------------------------------- */
/* ------------------------ Static Functions -------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredWriteDpEcfEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Function to start authentication session.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredWriteDpEcfHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    FLCN_STATUS status = FLCN_OK;
    PSELWRE_ACTION_WRITE_DP_ECF_ARG pActionArg = &pArg->action.secActionWriteDpEcf;
    LwU8    maxNoOfSors = 0;
    LwU8    maxNoOfHeads = 0;

    // Current state may not be authenticated thus cannot retrieve from selwreMemory.
    CHECK_STATUS(HDCP22WIRED_SEC_GET_MAX_NO_SORS_HEADS(&Hdcp22wired, &maxNoOfSors,
                                                       &maxNoOfHeads));
    if (pActionArg->sorNum >= maxNoOfSors)
    {
        status = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto label_return;
    }

#if defined(HDCP22_WAR_ECF_SELWRE_ENABLED)
    status = hdcp22wiredSelwreWriteDpEcf_HAL(&Hdcp22wired, 
                                             maxNoOfSors,
                                             maxNoOfHeads,
                                             pActionArg->sorNum,
                                             pActionArg->ecfTimeslot,
                                             pActionArg->bForceClearEcf,
                                             pActionArg->bAddStreamBack,
                                             LW_FALSE);
#else
    status = hdcp22wiredWriteDpEcf_HAL(&Hdcp22wired,
                                       pActionArg->sorNum,
                                       pActionArg->ecfTimeslot);
#endif

label_return:
    return status;
}

