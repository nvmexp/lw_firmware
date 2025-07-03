/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_pwm-test.c
 * @brief   Unit tests for PWM source control functions.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "lib/lib_pwm-mock.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Tests ------------------------------------------ */
UT_SUITE_DEFINE(PMU_PWM_HAL_THERM_TU10X,
                UT_SUITE_SET_COMPONENT("PMU PWM HAL Therm TU10X")
                UT_SUITE_SET_DESCRIPTION("Tests PMU PWM HAL Therm code for TU10X")
                UT_SUITE_SET_OWNER("aherring"))

UT_CASE_DEFINE(PMU_PWM_HAL_THERM_TU10X, PwmHalThermTU10XConstructTherm,
                UT_CASE_SET_DESCRIPTION("Ensure thermPwmSourceDescriptorConstruct_TU10X correctly constructs THERM type PWM descriptors")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_PWM_HAL_THERM_TU10X, PwmHalThermTU10XConstructThermVid,
                UT_CASE_SET_DESCRIPTION("Ensure thermPwmSourceDescriptorConstruct_TU10X correctly constructs THERM VID type PWM descriptors")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_PWM_HAL_THERM_TU10X, PwmHalThermTU10XConstructThermIpcVmilwid,
                UT_CASE_SET_DESCRIPTION("Ensure thermPwmSourceDescriptorConstruct_TU10X correctly constructs THERM IPC VMIN VID type PWM descriptors")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_PWM_HAL_THERM_TU10X, PwmHalThermTU10XConstructIlwalidSource,
                UT_CASE_SET_DESCRIPTION("Ensure thermPwmSourceDescriptorConstruct_TU10X correctly returns an error when called with an invalid source")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Used for return value from @ref pwmSourceDescriptorAllocate_MOCK
 *          in @ref PwmHalThermTU10XConstructTherm
 */
static PWM_SOURCE_DESCRIPTOR
PwmHalThermTU10XConstructThermExpectedPwmSrcDesc;

/*!
 * @brief   Initializes state for @ref PwmHalThermTU10XConstructTherm
 *
 * @details Initializes the following:
 *              PWM lib mocking (via @ref libPwmMockInit)
 *                  Sets up return value of pwmSourceDescriptorAllocate_MOCK to
 *                      @ref PwmHalThermTU10XConstructThermExpectedPwmSrcDesc
 */
static void
s_pwmHalThermTU10XConstructThermPreTest(void)
{
    libPwmMockInit();
    pwmSourceDescriptorAllocate_MOCK_fake.return_val =
        &PwmHalThermTU10XConstructThermExpectedPwmSrcDesc;
}

/*!
 * @brief   Ensure thermPwmSourceDescriptorConstruct_TU10X correctly constructs
 *          THERM type PWM descriptors
 */
UT_CASE_RUN(PMU_PWM_HAL_THERM_TU10X, PwmHalThermTU10XConstructTherm)
{
    const RM_PMU_PMGR_PWM_SOURCE sources[] =
    {
        RM_PMU_PMGR_PWM_SOURCE_THERM_PWM,
    };
    LwLength i;

    for (i = 0U; i < LW_ARRAY_ELEMENTS(sources); i++)
    {
        PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

        s_pwmHalThermTU10XConstructThermPreTest();

        pPwmSrcDesc = thermPwmSourceDescriptorConstruct_TU10X(sources[i], OVL_INDEX_OS_HEAP);

        UT_ASSERT_EQUAL_UINT(pwmSourceDescriptorAllocate_MOCK_fake.call_count, 1U);
        UT_ASSERT_EQUAL_UINT(pwmSourceDescriptorAllocate_MOCK_fake.arg0_val, OVL_INDEX_OS_HEAP);
        UT_ASSERT_EQUAL_UINT(pwmSourceDescriptorAllocate_MOCK_fake.arg1_val, sources[i]);
        UT_ASSERT_NOT_NULL(pwmSourceDescriptorAllocate_MOCK_fake.arg2_val);
        UT_ASSERT_EQUAL_PTR(pPwmSrcDesc, &PwmHalThermTU10XConstructThermExpectedPwmSrcDesc);
    }
}

/*!
 * @brief   Used for return value from @ref pwmSourceDescriptorAllocate_MOCK
 *          in @ref PwmHalThermTU10XConstructThermVid
 */
static PWM_SOURCE_DESCRIPTOR
PwmHalThermTU10XConstructThermVidExpectedPwmSrcDesc;

/*!
 * @brief   Initializes state for @ref PwmHalThermTU10XConstructThermVid
 *
 * @details Initializes the following:
 *              PWM lib mocking (via @ref libPwmMockInit)
 *                  Sets up return value of pwmSourceDescriptorAllocate_MOCK to
 *                      @ref PwmHalThermTU10XConstructThermVidExpectedPwmSrcDesc
 */
static void
s_pwmHalThermTU10XConstructThermVidPreTest(void)
{
    libPwmMockInit();
    pwmSourceDescriptorAllocate_MOCK_fake.return_val =
        &PwmHalThermTU10XConstructThermVidExpectedPwmSrcDesc;
}

/*!
 * @brief   Ensure thermPwmSourceDescriptorConstruct_TU10X correctly constructs
 *          THERM VID type PWM descriptors
 */
UT_CASE_RUN(PMU_PWM_HAL_THERM_TU10X, PwmHalThermTU10XConstructThermVid)
{
    const RM_PMU_PMGR_PWM_SOURCE sources[] =
    {
        RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0,
        RM_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_1,
    };
    LwLength i;

    for (i = 0U; i < LW_ARRAY_ELEMENTS(sources); i++)
    {
        PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

        s_pwmHalThermTU10XConstructThermVidPreTest();

        pPwmSrcDesc = thermPwmSourceDescriptorConstruct_TU10X(sources[i], OVL_INDEX_OS_HEAP);

        UT_ASSERT_EQUAL_UINT(pwmSourceDescriptorAllocate_MOCK_fake.call_count, 1U);
        UT_ASSERT_EQUAL_UINT(pwmSourceDescriptorAllocate_MOCK_fake.arg0_val, OVL_INDEX_OS_HEAP);
        UT_ASSERT_EQUAL_UINT(pwmSourceDescriptorAllocate_MOCK_fake.arg1_val, sources[i]);
        UT_ASSERT_NOT_NULL(pwmSourceDescriptorAllocate_MOCK_fake.arg2_val);
        UT_ASSERT_EQUAL_PTR(pPwmSrcDesc, &PwmHalThermTU10XConstructThermVidExpectedPwmSrcDesc);
    }
}

/*!
 * @brief   Used for return value from @ref pwmSourceDescriptorAllocate_MOCK
 *          in @ref PwmHalThermTU10XConstructThermIpcVmilwid
 */
static PWM_SOURCE_DESCRIPTOR
PwmHalThermTU10XConstructThermIpcVmilwidExpectedPwmSrcDesc;

/*!
 * @brief   Initializes state for @ref PwmHalThermTU10XConstructThermIpcVmilwid
 *
 * @details Initializes the following:
 *              PWM lib mocking (via @ref libPwmMockInit)
 *                  Sets up return value of pwmSourceDescriptorAllocate_MOCK to
 *                      @ref PwmHalThermTU10XConstructThermIpcVmilwidExpectedPwmSrcDesc
 */
static void
s_pwmHalThermTU10XConstructThermIpcVmilwidPreTest(void)
{
    libPwmMockInit();
    pwmSourceDescriptorAllocate_MOCK_fake.return_val =
        &PwmHalThermTU10XConstructThermIpcVmilwidExpectedPwmSrcDesc;
}

/*!
 * @brief   Ensure thermPwmSourceDescriptorConstruct_TU10X correctly constructs
 *          THERM IPC VMIN VID type PWM descriptors
 */
UT_CASE_RUN(PMU_PWM_HAL_THERM_TU10X, PwmHalThermTU10XConstructThermIpcVmilwid)
{
    const RM_PMU_PMGR_PWM_SOURCE sources[] =
    {
        RM_PMU_PMGR_PWM_SOURCE_THERM_IPC_VMIN_VID_PWM_0,
        RM_PMU_PMGR_PWM_SOURCE_THERM_IPC_VMIN_VID_PWM_1,
    };
    LwLength i;

    for (i = 0U; i < LW_ARRAY_ELEMENTS(sources); i++)
    {
        PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

        s_pwmHalThermTU10XConstructThermIpcVmilwidPreTest();

        pPwmSrcDesc = thermPwmSourceDescriptorConstruct_TU10X(sources[i], OVL_INDEX_OS_HEAP);

        UT_ASSERT_EQUAL_UINT(pwmSourceDescriptorAllocate_MOCK_fake.call_count, 1U);
        UT_ASSERT_EQUAL_UINT(pwmSourceDescriptorAllocate_MOCK_fake.arg0_val, OVL_INDEX_OS_HEAP);
        UT_ASSERT_EQUAL_UINT(pwmSourceDescriptorAllocate_MOCK_fake.arg1_val, sources[i]);
        UT_ASSERT_NOT_NULL(pwmSourceDescriptorAllocate_MOCK_fake.arg2_val);
        UT_ASSERT_EQUAL_PTR(pPwmSrcDesc, &PwmHalThermTU10XConstructThermIpcVmilwidExpectedPwmSrcDesc);
    }
}

/*!
 * @brief   Ensure thermPwmSourceDescriptorConstruct_TU10X correctly returns an
 *          error when called with an invalid source
 */
UT_CASE_RUN(PMU_PWM_HAL_THERM_TU10X, PwmHalThermTU10XConstructIlwalidSource)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

    pPwmSrcDesc = thermPwmSourceDescriptorConstruct_TU10X(
        RM_PMU_PMGR_PWM_SOURCE__COUNT, OVL_INDEX_OS_HEAP);

    UT_ASSERT_NULL(pPwmSrcDesc);
}
