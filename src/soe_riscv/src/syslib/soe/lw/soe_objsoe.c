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
 *
 * @file   soe_objsoe.c
 * @brief  General-purpose chip-specific routines which do not exactly fit
 *         within the bounds a particular engine in the GPU.
 */

#include "soe_objsoe.h"

#include <osptimer.h>

#define SOE_REG_POLL_PARAM_SIZE     (4)
#define SOE_REG_POLL_PARAM_ADDR     (0)
#define SOE_REG_POLL_PARAM_MASK     (1)
#define SOE_REG_POLL_PARAM_VAL      (2)
#define SOE_REG_POLL_PARAM_MODE     (3)

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
sysSYSLIB_CODE static LwBool
soePollReg32Func
(
    LwU32 *pParams
)
{
    LwU32  reg32;
    LwBool bReturn;

    reg32 = FLD_TEST_DRF(_SOE, _REG_POLL, _REG_TYPE, _BAR0,
                            pParams[SOE_REG_POLL_PARAM_MODE]) ?
                            bar0Read(pParams[SOE_REG_POLL_PARAM_ADDR]):
                            csbRead(pParams[SOE_REG_POLL_PARAM_ADDR]);

    bReturn = (reg32 & pParams[SOE_REG_POLL_PARAM_MASK]) == pParams[SOE_REG_POLL_PARAM_VAL ];

    return FLD_TEST_DRF(_SOE, _REG_POLL, _OP, _EQ,
                            pParams[SOE_REG_POLL_PARAM_MODE]) ?
                         (LwBool) bReturn : !bReturn;
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
sysSYSLIB_CODE LwBool
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
