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
 * @file    volttestgh100.c
 * @brief   PMU HAL functions related to volt tests for GH100+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "volt/objvolt.h"
#include "pmu_objpmu.h"

#include "config/g_volt_private.h"
#include "config/g_volt_hal.h"
#include "g_pmurpc.h"


/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/*
 * This structure is used to cache all HW values for registers that each
 * test uses and restore them back after each test/subtest completes. Inline
 * comments for each variable to register mapping.
 */

/* ------------------------- Public Function Prototypes  -------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
/* ------------------------- Compile Time Checks ---------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Tests if FORCE_VMIN_MASK functionality works as 
           expected by triggering the event that enforces VMIN capping.
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
voltTestForceVmin_GH100
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE *pParams
)
{
    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED;
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @brief  Tests if VIDx_PWM_BOUND HW block works as expected 
           and is able to bound the incoming VIDx_PWM_HI value.
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
voltTestVidPwmBoundFloor_GH100
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE* pParams
)
{
    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED;
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @brief  Tests if VIDxPWM_BOUND HW block works as 
           expected and is able to bound the incoming VIDx_PWM_HI value
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
voltTestVidPwmBoundCeil_GH100
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE* pParams
)
{
    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED;
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @brief  Tests if CLVC offset is applied correctly 
           by VIDPWM HW in the final VIDPWM value computation
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
voltTestPositiveClvcOffset_GH100
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE* pParams
)
{
    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED;
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @brief  Tests if negative CLVC offset is applied correctly 
           by VIDPWM HW in the final VIDPWM value computation.
 *
 * @param[out]  pParams   Test Params indicating status of test
 *                        and output value.
 *
 * @return      FLCN_OK                If test is supported on the GPU.
 * @return      FLCN_ERR_NOT_SUPPORTED If test is not supported on the GPU.
 *
 * @note        Return status does not indicate whether the test has passed
 *              or failed. It only indicates support for the test. For
 *              pass/fail condition, refer to pParams->outStatus.
 */
FLCN_STATUS
voltTestNegativeClvcOffset_GH100
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE* pParams
)
{
    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED;
    return FLCN_ERR_NOT_SUPPORTED;
}
