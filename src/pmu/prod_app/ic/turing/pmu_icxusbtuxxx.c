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
 * @file   pmu_icxusbtuxxx.c
 * @brief  Contains all Interrupt Control routines specific to XUSB on Turing.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_falcon_csb_addendum.h"
#include "dev_pwr_pri.h"
#include "dev_lw_xve.h"
#include "dev_graphics_nobundle.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic.h"
#include "pmu_objbif.h"
#include "pmu_xusb.h"
#include "pmu_bar0.h"

#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void  s_icXusbMsgSend_TUXXX(LwU8 eventType)
    GCC_ATTRIB_SECTION("imem_resident", "s_icXusbMsgSend_TUXXX");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Specific handler-routine for dispatching XUSB messages.
 */
static void  s_icXusbMsgSend_TUXXX(LwU8 eventType)
{
    DISPATCH_BIF dispatchBif;

    dispatchBif.hdr.eventType = eventType;
    lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, BIF), &dispatchBif,
                             sizeof(dispatchBif));
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Enable all XUSB interrupts
 */
void
icXusbIntrEnable_TUXXX(void)
{
    //
    // Check for fuse settings (added in Bug 200346106) to make sure XUSB function is enabled.
    // Enable XUSB-PMU message box interrupts only if XUSB function is enabled.
    //
    if (FLD_TEST_DRF(_XVE, _XUSB_STATUS, _ENABLED, _TRUE, REG_RD32(BAR0_CFG, LW_XVE_XUSB_STATUS)))
    {
        // Enable XUSB2PMU_REQ interrupt
        REG_WR32(CSB, LW_CPWR_PMU_XUSB2PMU,
                 DRF_DEF(_CPWR_PMU, _XUSB2PMU, _REQ_INTR_EN, _ENABLED));

        // Enable PMU2XUSB_ACK interrupt
        REG_WR32(CSB, LW_CPWR_PMU_PMU2XUSB,
                 DRF_DEF(_CPWR_PMU, _PMU2XUSB, _ACK_INTR_EN, _ENABLED));
    }
}

/*!
 * Service all XUSB interrupts
 */
void
icServiceXusbIntr_TUXXX(void)
{
    LwU32 clearBits  = 0; // Handle Turing interrupts
    LwU32 regval = REG_RD32(CSB, LW_CPWR_PMU_INTR_1);
    DBG_PRINTF_ISR(("LW_CPWR_PMU_INTR_1 0x%x \n", regval, 0, 0, 0));

    //
    // XUSB2PMU_REQ Interrupt
    //
    if (FLD_TEST_DRF(_CPWR, _PMU_INTR_1, _XUSB2PMU_REQ, _PENDING, regval))
    {
        clearBits = FLD_SET_DRF(_CPWR, _PMU_INTR_1, _XUSB2PMU_REQ, _CLEAR, clearBits);
        s_icXusbMsgSend_TUXXX(BIF_EVENT_ID_XUSB_TO_PMU_MSGBOX);
    }

    //
    // PMU2XUSB_ACK Interrupt
    //
    if (FLD_TEST_DRF(_CPWR, _PMU_INTR_1, _PMU2XUSB_ACK, _PENDING, regval))
    {
        clearBits = FLD_SET_DRF(_CPWR, _PMU_INTR_1, _PMU2XUSB_ACK, _CLEAR, clearBits);
        s_icXusbMsgSend_TUXXX(BIF_EVENT_ID_PMU_TO_XUSB_ACK);
    }

    // Clear handled interrupts using a blocking write
    DBG_PRINTF_ISR(("Clear INTR_1\n", 0, 0, 0, 0));
    REG_WR32_STALL(CSB, LW_CPWR_PMU_INTR_1, clearBits);
}
