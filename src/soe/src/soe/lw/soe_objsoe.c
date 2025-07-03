/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_objsoe.c
 * @brief  General-purpose chip-specific routines which do not exactly fit
 *         within the bounds a particular engine in the GPU.
 *
 * @section _soe_objsoe OBJSOE Notes
 * SOE object is created to serve general purpose chip specific routines that
 * can't be cleanly handled in task* codes without getting tangled up with
 * multiple includes of the same #defines.
 *
 * An engine specific code should still be handled in each engine hal layer,
 * but all other SOE features varying from chip to chip should all go in here.
 *
 * Ideally, we do not want to directly refer to chip specific #defines in
 * the main SOE app source codes.
 * @endsection
 */

/* ------------------------ System includes ------------------------------- */
#include "soesw.h"
#include "lwostimer.h"
/* ------------------------ Application includes --------------------------- */
#include "soe_bar0.h"
#include "soe_objhal.h"
#include "soe_objsoe.h"
#include "soe_objic.h"
#include "soe_objtimer.h"
#include "soe_cmdmgmt.h"
#include "soe_objrttimer.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define SOE_REG_POLL_PARAM_SIZE     (4)
#define SOE_REG_POLL_PARAM_ADDR     (0)
#define SOE_REG_POLL_PARAM_MASK     (1)
#define SOE_REG_POLL_PARAM_VAL      (2)
#define SOE_REG_POLL_PARAM_MODE     (3)

/* ------------------------- Prototypes ------------------------------------- */
static LwBool soePollReg32Func(LwU32 *)
    GCC_ATTRIB_SECTION("imem_resident", "soePollReg32Func");

/* ------------------------ External definitions --------------------------- */
/* ------------------------ Global variables ------------------------------- */
OBJSOE Soe;

/*!
 * Construct the SOE object.  This sets up the HAL interface used by the SOE
 * module.
 */
void
constructSoe(void)
{
    IfaceSetup->soeHalIfacesSetupFn(&Soe.hal);
}

/*!
 * Start the timer used for the OS scheduler
 *
 * Notes:
 *     - This function MUST be called PRIOR to starting the scheduler.
 *     - @ref icPreInit_HAL should be called before calling this function
 *       to set up the SOE interrupts for the timer.
 */
void
soeStartOsTicksTimer(void)
{
    FLCN_STATUS status = rttimerSetup_HAL(&Rttimer, LWRTOS_TICK_PERIOD_US);
    OS_ASSERT(status == FLCN_OK);
}

/*!
 * Poll on a csb/bar0 register until the specified condition is hit.
 *
 * @param[in]  addr      The BAR0 address to read
 * @param[in]  mask      Masks the value to compare
 * @param[in]  val       Value used to compare the bar0 register value
 * @param[in]  timeoutNs Timeout value in ns.
 * @param[in]  mode      Poll mode - see soePollReg32Func
 *
 * @return LW_TRUE  - if the specified condition is met
 * @return LW_FALSE - if timed out.
 */
LwBool
soePollReg32Ns
(
    LwU32  addr,
    LwU32  mask,
    LwU32  val,
    LwU32  timeoutNs,
    LwU32  mode
)
{
    LwU32 params[SOE_REG_POLL_PARAM_SIZE];
    params[SOE_REG_POLL_PARAM_ADDR] = addr;
    params[SOE_REG_POLL_PARAM_MASK] = mask;
    params[SOE_REG_POLL_PARAM_VAL ] = val;
    params[SOE_REG_POLL_PARAM_MODE] = mode;

    return OS_PTIMER_SPIN_WAIT_NS_COND(soePollReg32Func, params, timeoutNs);
}

/*!
 * with @ref TIMER_SPIN_WAIT_NS as way of polling on a specific field
 * in a register until the register reports a specific value or until a timeout
 * oclwrs.
 *
 * pParams[0] SOE_REG_POLL_PARAM_ADDR  - address
 * pParams[1] SOE_REG_POLL_PARAM_MASK  - mask
 * pParams[2] SOE_REG_POLL_PARAM_VAL   - value to compare
 * pParams[3] SOE_REG_POLL_PARAM_MODE  - see below
 *
 *   SOE_REG_POLL_REG_TYPE_CSB  - poll from a CSB register
 *   SOE_REG_POLL_REG_TYPE_BAR0 - poll from a BAR0 register
 *   SOE_REG_POLL_OP_EQ         - returns LW_TRUE if the masked register value
 *                                is equal to the passed value,
 *                                LW_FALSE otherwise
 *   SOE_REG_POLL_OP_NEQ        - returns LW_TRUE if the masked register value
 *                                is not equal to the passed value,
 *                                LW_FALSE otherwise
 *
 * @return LW_TRUE  - if the specificied condition is met
 * @return LW_FALSE - otherwise
 */
static LwBool
soePollReg32Func
(
    LwU32 *pParams
)
{
    LwU32  reg32;
    LwBool bReturn;

    reg32 = FLD_TEST_DRF(_SOE, _REG_POLL, _REG_TYPE, _BAR0,
                            pParams[SOE_REG_POLL_PARAM_MODE]) ?
                         REG_RD32(BAR0, pParams[SOE_REG_POLL_PARAM_ADDR]):
                         CSB_REG_RD32 (pParams[SOE_REG_POLL_PARAM_ADDR]);
    bReturn = (reg32 & pParams[SOE_REG_POLL_PARAM_MASK]) ==
                       pParams[SOE_REG_POLL_PARAM_VAL ];

    return FLD_TEST_DRF(_SOE, _REG_POLL, _OP, _EQ,
                            pParams[SOE_REG_POLL_PARAM_MODE]) ?
                         (LwBool) bReturn : !bReturn;
}

/*!
 * Writes a 32-bit value to the given BAR0 address.
 * TODO: change to use HW reg access library and remove the interface func.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 */
void
libInterfaceRegWr32
(
    LwU32  addr,
    LwU32  data
)
{
    REG_WR32(BAR0, addr, data);
}

/*!
 * Reads the given BAR0 address. The read transaction is nonposted.
 * TODO: change to use HW reg access library and remove the interface func.
 *
 * @param[in]  addr  The BAR0 address to read
 *
 * @return The 32-bit value of the requested BAR0 address
 */
LwU32
libInterfaceRegRd32
(
    LwU32  addr
)
{
    return REG_RD32(BAR0, addr);
}

