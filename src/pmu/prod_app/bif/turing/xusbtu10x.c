/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   biftu10x.c
 * @brief  Contains BIF routines specific to TU10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_objpmu.h"

#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_trim.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_bif_private.h"
#include "pmu_objbif.h"
#include "pmu_xusb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*  -------------- XUSB<->PMU MESSAGE BOX INTERFACE Public functions -------------- */
//
// The message box interface between XUSB and PMU assumes the following -
//  1. There is only one outstanding message per message box at any given time.
//  2. We must wait till we receive ACK for message sent or ACK times out before
//     sending the next message on either message box.
//
/*!
 * @brief Read XUSB2PMU msgbox register on Turing+ chips
 *
 * @param[out]  pMsgBox       Pointer to message box contents
 *
 * @return      LW_TRUE if valid request is pending
 */
LwBool
bifReadXusbToPmuMsgbox_TU10X
(
    LwU8 *pMsgBox
)
{
    LwBool bIsRequestHigh = LW_FALSE;

    // Read XUSB2PMU register for receiving message box
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_PMU_XUSB2PMU);

    // Check if request line status is set to _HIGH
    if (FLD_TEST_DRF(_CPWR_PMU, _XUSB2PMU, _REQ, _HIGH, regVal))
    {
        bIsRequestHigh = LW_TRUE;

        // Read the message box received from XUSB
        *pMsgBox = DRF_VAL(_CPWR_PMU, _XUSB2PMU, _MSG, regVal);
    }

    return bIsRequestHigh;
}

/*!
 * @brief Write XUSB2PMU msgbox register on Turing+ chips
 *
 * @param[in]  reply       Ack reply to be sent back
 */
void
bifWriteXusbToPmuAckReply_TU10X
(
    LwU8 reply
)
{
    // Read XUSB2PMU register
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_PMU_XUSB2PMU);

    // Set _ACK_SEND bit to raise an interrupt to XUSB
    regVal = FLD_SET_DRF(_CPWR_PMU, _XUSB2PMU, _ACK, _SEND, regVal);

    // Set ack reply type and payload
    regVal = FLD_SET_DRF_NUM(_CPWR_PMU, _XUSB2PMU, _RDAT, reply, regVal);

    // Write the XUSB2PMU register with ack reply
    REG_WR32(CSB, LW_CPWR_PMU_XUSB2PMU, regVal);
}

/*!
 * @brief Read PMU2XUSB message box register on Turing+ chips
 *
 * @return  LwU8     Ack reply data
 */
LwU8
bifReadPmuToXusbAckReply_TU10X()
{
    LwU8 rDat = 0;

    //
    // PMU2XUSB_REQ is task field and is automatically de-asserted. ACK interrupt is triggered at posedge of ack line.
    // Ack line gets de-asserted after REQ is de-asserted above. This is end of transaction.
    // HW bug : in current implementation, pmu does not latch the ACK payload from xusb and relies on data driven by XUSB register.
    //          This (tracked in Bug 200381450) will be fixed in Ampere since it is not frequent use case for Turing and can be
    //          handled by proper checks in xusb firmware
    // So pmu ucode should not check for ACK_HIGH before parsing the ack payload
    //

    // Read PMU2XUSB register for receiving ack reply
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_PMU_PMU2XUSB);

    // Parse and extract the reply message received from XUSB
    rDat = DRF_VAL(_CPWR_PMU, _PMU2XUSB, _RDAT, regVal);

    return rDat;
}

/*!
 * @brief Write PMU2XUSB msgbox register on Turing+ chips
 *
 * @param[in]  messageBox   Message box contents
 */
FLCN_STATUS
bifWritePmuToXusbMsgbox_TU10X
(
    LwU8 messageBox
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Read PMU2XUSB register
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_PMU_PMU2XUSB);

    // Check if _ACK line status is _LOW before asserting _REQ
    if (FLD_TEST_DRF(_CPWR_PMU, _PMU2XUSB, _ACK, _LOW, regVal))
    {
        // Set _REQ_SEND bit to raise an interrupt to XUSB
        regVal = FLD_SET_DRF(_CPWR_PMU, _PMU2XUSB, _REQ, _SEND, regVal);

        // Set command type and payload
        regVal = FLD_SET_DRF_NUM(_CPWR_PMU, _PMU2XUSB, _MSG, messageBox, regVal);

        // Write the PMU2XUSB register with message box
        REG_WR32(CSB, LW_CPWR_PMU_PMU2XUSB, regVal);

        status = FLCN_OK;
    }
    else
    {
        status = FLCN_ERROR;
    }

    return status;
}

/*!
 * @brief Clear PMU2XUSB msgbox request on timeout on Turing+ chips
 */
FLCN_STATUS
bifClearPmuToXusbMsgbox_TU10X()
{
    // Read PMU2XUSB register
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_PMU_PMU2XUSB);

    // De-assert the request signal on timeout
    regVal = FLD_SET_DRF(_CPWR_PMU, _PMU2XUSB, _REQ, _IDLE, regVal);

    // Clear the message payload
    regVal = FLD_SET_DRF(_CPWR_PMU, _PMU2XUSB, _MSG, _INIT, regVal);

    // Write the PMU2XUSB register with message box
    REG_WR32(CSB, LW_CPWR_PMU_PMU2XUSB, regVal);

    return FLCN_OK;
}

/*!
 * @brief Query isochronous traffic activity on XUSB function on Turing+ chips
 */
FLCN_STATUS
bifQueryXusbIsochActivity_TU10X()
{
    LwU8 msgBox = 0;

    msgBox = FLD_SET_DRF(_PMU2XUSB, _MSGBOX_CMD, _TYPE, _ISOCH_QUERY, msgBox);
    return bifWritePmuToXusbMsgbox_HAL(&Bif, msgBox);
}
