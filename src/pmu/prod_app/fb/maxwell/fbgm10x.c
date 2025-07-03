/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fbgm10x.c
 * @brief  FB Hal Functions for GM10X
 *
 * FB Hal Functions implement FB related functionalities for GM10X
 * Lwrrently the supported function is
 *    * fbSTOP - takes one parameter to stop/resume the fermi fb interface
 *               LW_TRUE  - Stop FBI
 *               LW_FALSE - Resume FBI
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_bus.h"
#include "dev_fifo.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objfifo.h"
#include "pmu_objfb.h"
#include "dbgprintf.h"
#include "config/g_fb_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

static void s_doFbStop_GM10X(void);
static void s_doFbStart_GM10X(void);


/*!
 * @brief Stops and starts framebuffer.
 *
 * This function disables/enables all traffic on the Host to FB interfaces so
 * that nothing can access the FB when disabled. For example, MCLK switching
 * requires FB not be accessed by anything else but the PMU.
 *
 * @param[in] bStop
 *     LW_TRUE stops FB, LW_FALSE starts FB.
 *
 * @param[in] params
 *     Optional parameter which can be used to pass extra information to the
 *     PMU to change the way to stop/start FB.
 */
FLCN_STATUS
fbStop_GM10X
(
    LwBool bStop,
    LwU32  params
)
{
    if (bStop)
    {
        appTaskCriticalEnter();
        s_doFbStop_GM10X();
    }
    else
    {
        s_doFbStart_GM10X();
        appTaskCriticalExit();
    }
    return FLCN_OK;
}

/*!
 * Stops the FB interface
 */
static void
s_doFbStop_GM10X(void)
{
    LwU32 reg32;

    DBG_PRINTF_SEQ(("FB_STOP Begins\n", 0, 0 ,0,0));
    //
    // Write to the LW_PBUS_ACCESS register to disable all
    // non-posted pipeline requestors except the PMU (SEC2, LWDEC/BSP, I2C,
    // JTAG, the Strap/ROM state machine, and XVE_N only make non-posted
    // requests).
    // For SEC2 and LWDEC/BSP, since they are host-based engines, we get the
    // bits from the device info table.
    //
    reg32 = REG_RD32(BAR0, LW_PBUS_ACCESS);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _I2C,   _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _JTAG,  _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _ROM,   _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _XVE_N, _DISABLED, reg32);
    reg32 &= ~BIT(GET_ENG_INTR(PMU_ENGINE_SEC2));
    reg32 &= ~BIT(GET_ENG_INTR(PMU_ENGINE_BSP));
    REG_WR32(BAR0, LW_PBUS_ACCESS, reg32);

    //
    // Send a read to any register serviced through the Host PRI Primary.
    // (For example, the LW_PBUS_ACCESS register). This will clear the
    // non-posted pipeline between the EXT-access arbiter and the PRI
    // Primary without stopping any posted requests.
    //
    // Write to the LW_PBUS_EXT_ACCESS register to disable all
    // posted pipeline requestors.
    //
    reg32 = REG_RD32(BAR0, LW_PBUS_ACCESS);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _XVE_P, _DISABLED, reg32);
    REG_WR32(BAR0, LW_PBUS_ACCESS, reg32);

    //
    // Write to the LW_PFIFO_FB_IFACE_CONTROL register to stop
    // Host from sending additional requests to FB.  This not only disables
    // Host's interface with the FB, but also will flush any writes on the
    // posted pipeline between the EXT arbiter and the PRI primary.  You now
    // have the pipelines in a state where the PMU has clear access to the
    // PRI Primary.
    //
    REG_FLD_WR_DRF_DEF(BAR0, _PFIFO, _FB_IFACE, _CONTROL, _DISABLE);

    //
    // Bug 541083 : FB_IFACE_STATUS won't report _DISABLED
    //
    // We used to poll the register LW_PFIFO_FB_IFACE_STATUS to ensure all
    // of the transactions outstanding in the FB have completed.
    // However, FB_IFACE_STATUS doesn't report _DISABLED after a page fault
    // or fbi timeout, so we read from LW_PFIFO_FB_IFACE just to give it
    // a bit of delay
    //
    reg32 = REG_RD32(BAR0, LW_PFIFO_FB_IFACE);

    // Finally, assert FB_STOP_REQ.
    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_OUTPUT_SET,
             DRF_SHIFTMASK(LW_CPWR_PMU_GPIO_OUTPUT_SET_FB_STOP_REQ));

    // Wait for FB_STOP_ACK to be asserted
    do
    {
        reg32 = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INPUT);
    }
    while (!FLD_TEST_DRF_NUM(_CPWR_PMU, _GPIO_INPUT, _FB_STOP_ACK, 1, reg32));

    DBG_PRINTF_SEQ(("FB_STOP Ends\n", 0, 0 ,0,0));
}

/*!
 * Resumes the FB interface
 */
static void
s_doFbStart_GM10X(void)
{
    LwU32 reg32;
    DBG_PRINTF_SEQ(("FB_START Begins\n", 0, 0 ,0,0));

    //
    // First, de-assert FB_STOP_REQ.
    //
    // This write is done via BAR0 instead of CSB on purpose. Internal to the
    // PMU, CSB accesses use a different path than BAR0 accesses. This causes
    // ordering issues when CSB writes are mixed with BAR0 writes since PMU
    // does not have to wait for the acks to BAR0 writes before sending out CSB
    // writes.
    //
    // Furthermore, PMU ucode for Falcon 4 uses the Falcon EXT interface for
    // BAR0 writes and thus cannot poll on LW_CPWR_PMU_BAR0_CTL_STATUS to
    // ensure that previous BAR0 writes have completed. It is also insufficient
    // to do a BAR0 read on the last register written as this does not block
    // exelwtion of CSB writes. Thus, instead of adding an arbitrary delay,
    // forcing the write to clear FBSTOP to go through BAR0 enforces ordering
    // with respect to previous BAR0 writes during FBSTOP.
    //
    REG_WR32(BAR0, LW_PPWR_PMU_GPIO_OUTPUT_CLEAR,
             DRF_SHIFTMASK(LW_PPWR_PMU_GPIO_OUTPUT_CLEAR_FB_STOP_REQ));

    // Wait for ACK to be deasserted.
    do
    {
        reg32 = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INPUT);
    }
    while (!FLD_TEST_DRF_NUM(_CPWR_PMU, _GPIO_INPUT, _FB_STOP_ACK, 0, reg32));

    //
    // The PMU writes the LW_PFIFO_FB_IFACE_CONTROL register to enable
    // Host's interface with the FB.
    //
    // TODO: investigate making this a plain write and not a read-modify-write
    //
    REG_FLD_WR_DRF_DEF(BAR0, _PFIFO, _FB_IFACE, _CONTROL, _ENABLE);

    //
    // The PMU writes the LW_PBUS_ACCESS register to re-enable
    // posted accesses.
    //
    reg32 = REG_RD32(BAR0, LW_PBUS_ACCESS);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _XVE_P, _ENABLED, reg32);
    REG_WR32(BAR0, LW_PBUS_ACCESS, reg32);

    //
    // The PMU writes the LW_PBUS_ACCESS register to enable
    // the other EXT arbiter requestors.
    // For SEC2 and LWDEC/BSP, since they are host-based engines, we get the
    // bits from the device info table.
    //
    reg32 = REG_RD32(BAR0, LW_PBUS_ACCESS);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _I2C,   _ENABLED, reg32);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _JTAG,  _ENABLED, reg32);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _ROM,   _ENABLED, reg32);
    reg32 = FLD_SET_DRF(_PBUS, _ACCESS, _XVE_N, _ENABLED, reg32);
    reg32 |= BIT(GET_ENG_INTR(PMU_ENGINE_SEC2));
    reg32 |= BIT(GET_ENG_INTR(PMU_ENGINE_BSP));
    REG_WR32(BAR0, LW_PBUS_ACCESS, reg32);

    DBG_PRINTF_SEQ(("FB_START Ends\n", 0, 0 ,0,0));
}

