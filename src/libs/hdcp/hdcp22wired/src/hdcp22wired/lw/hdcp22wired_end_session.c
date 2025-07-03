/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_end_session.c
 * @brief  Hdcp22 EndSession secure action handler.
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "flcnifcmn.h"
#include "hdcp22wired_cmn.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
#ifdef GSPLITE_RTOS
static void _hdcp22wiredEndSessionEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22EndSession", "start");
#endif

FLCN_STATUS hdcp22wiredEndSessionHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22EndSession", "hdcp22wiredEndSessionHandler");

FLCN_STATUS hdcp22wiredEndSession(void)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredEndSession");

/* ------------------------ Static Functions -------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredEndSessionEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/* ------------------------ Function Definitions ---------------------------- */
/*!
 * @brief Function to reset authentication session.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredEndSessionHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    FLCN_STATUS         status = FLCN_OK;
    SELWRE_ACTION_TYPE  prevState = SELWRE_ACTION_ILWALID;
    LwU8                sorNum;

    // Reset internal secrets.
    //
    // HDCP22_SECRET_SOR_NUM ( reset need to be after saveSessionSecret ),
    // HDCP22_SECRET_NUM_STREAMS,
    // HDCP22_SECRET_DP_TYPE_MASK,
    // are saved in _hdcp22SaveSession() and not reset.
    //

    //
    // Writes to secure memory at EndSession would reset integrity
    // hash without check.
    //
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_LINK_IDX,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_IS_MST,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_IS_REPEATER,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_STREAM_ID_TYPE,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_DP_TYPE_MASK,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_RTX,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_DKEY,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_MAX_NO_SORS,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_MAX_NO_HEADS,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

    // Retrieve SOR number to save active session secrets.
    status = hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                 HDCP22_SECRET_SOR_NUM,
                                                 (LwU8 *)&sorNum);
    //
    // The first endSession to retrieve sorNum in the beginning of task would
    // fail due to hash not yet initialized and no need to save.
    //
    if (status == FLCN_OK)
    {
        // Save active session secrets (encKd, rnSesCrypt) before reset.
        CHECK_STATUS(hdcp22wiredSaveSessionSecrets_HAL(&Hdcp22wired, sorNum));
    }

    // Reset encKd, rnSesCrypt.
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_KD,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));

#ifndef HDCP22_KMEM_ENABLED
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_RSESSION,
                                                    (LwU8 *)NULL,
                                                    LW_TRUE));
#endif

    // Update Previous state as End Session.
    prevState = SELWRE_ACTION_END_SESSION;
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_PREV_STATE,
                                                    (LwU8 *)&prevState,
                                                    LW_FALSE));

label_return:
    return status;
}

