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
#include "dmemovl.h"
#include "lwosreg.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_pwm.h"
#include "perf/changeseq-mock.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "pmu_objpg.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
extern LwU32 pwmSourceDescriptorsConstructedMask;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Tests ------------------------------------------ */
UT_SUITE_DEFINE(PMU_LIB_PWM,
                UT_SUITE_SET_COMPONENT("PMU Lib PWM")
                UT_SUITE_SET_DESCRIPTION("Tests PMU PWM Lib code")
                UT_SUITE_SET_OWNER("aherring"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmSourceConstructTherm,
                UT_CASE_SET_DESCRIPTION("Ensure pwmSourceDescriptorConstruct correctly constructs therm descriptors")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmSourceConstructPmgr,
                UT_CASE_SET_DESCRIPTION("Ensure pwmSourceDescriptorConstruct correctly constructs PMGR descriptors")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmSourceConstructPg,
                UT_CASE_SET_DESCRIPTION("Ensure pwmSourceDescriptorConstruct correctly constructs PG descriptors")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmSourceConstructIlwalidSource,
                UT_CASE_SET_DESCRIPTION("Ensure pwmSourceDescriptorConstruct returns an error on invalid sources")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmParamsGetIlwalidPointers,
                UT_CASE_SET_DESCRIPTION("Ensure pwmParamsGet_IMPL correctly returns an error when provided invalid pointers")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmParamsGetTrigger,
                UT_CASE_SET_DESCRIPTION("Ensure pwmParamsGet_IMPL correctly retrieves PWM Trigger control params")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmParamsGetTriggerNonIlwert,
                UT_CASE_SET_DESCRIPTION("Ensure pwmParamsGet_IMPL correctly retrieves PWM Trigger control params when asked not to ilwert the duty cycle")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmParamsGetIlwalidDescriptorType,
                UT_CASE_SET_DESCRIPTION("Ensure pwmParamsGet_IMPL correctly returns an error when provided a descriptor with an invalid type.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmParamsSetIlwalidPointer,
                UT_CASE_SET_DESCRIPTION("Ensure pwmParamsSet correctly returns an error when provided invalid pointer")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmParamsSetTrigger,
                UT_CASE_SET_DESCRIPTION("Ensure pwmParamsSet correctly sets PWM Trigger control params")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmParamsSetTriggerSkipOptional,
                UT_CASE_SET_DESCRIPTION("Ensure pwmParamsSet correctly sets PWM Trigger control params when skipping optional paths")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmParamsSetIlwalidDescriptorType,
                UT_CASE_SET_DESCRIPTION("Ensure pwmParamsSet correctly returns an error when provided a descriptor with an invalid type.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateIlwalildPointer,
                UT_CASE_SET_DESCRIPTION("Ensure pwmSourceDescriptorAllocate_IMPL returns an error when provided invalid pointer")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateIlwalidSource,
                UT_CASE_SET_DESCRIPTION("Ensure pwmSourceDescriptorAllocate_IMPL correctly returns an error when provided an invalid PWM source")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateIlwalidType,
                UT_CASE_SET_DESCRIPTION("Ensure pwmSourceDescriptorAllocate_IMPL correctly returns an error when provided an invalid PWM type")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateCallocFail,
                UT_CASE_SET_DESCRIPTION("Ensure pwmSourceDescriptorAllocate_IMPL correctly returns an error when memory allocation fails")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateTrigger,
                UT_CASE_SET_DESCRIPTION("Ensure pwmSourceDescriptorAllocate_IMPL correctly allocates trigger PWMs")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Used for return value from @ref thermPwmSourceDescriptorConstruct_HAL
 *          in @ref LibPwmSourceConstructTherm
 */
static PWM_SOURCE_DESCRIPTOR
LibPwmSourceConstructThermDescriptor;

/*!
 * @brief   Initializes state for @ref LibPwmSourceConstructTherm
 */
static void
s_libPwmPreTestSourceConstructTherm(void)
{
    thermHalMockInit();
    thermPwmSourceDescriptorConstruct_MOCK_fake.return_val = &LibPwmSourceConstructThermDescriptor;
}

/*!
 * @brief   Ensure pwmSourceDescriptorConstruct correctly constructs therm descriptors
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmSourceConstructTherm)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

    s_libPwmPreTestSourceConstructTherm();

    pPwmSrcDesc = pwmSourceDescriptorConstruct(0U, RM_PMU_PMGR_PWM_SOURCE_THERM_PWM);

    UT_ASSERT_EQUAL_UINT(thermPwmSourceDescriptorConstruct_MOCK_fake.call_count, 1U);
    UT_ASSERT_EQUAL_UINT(thermPwmSourceDescriptorConstruct_MOCK_fake.arg0_val, RM_PMU_PMGR_PWM_SOURCE_THERM_PWM);
    UT_ASSERT_EQUAL_UINT(thermPwmSourceDescriptorConstruct_MOCK_fake.arg1_val, 0U);
    UT_ASSERT_EQUAL_PTR(pPwmSrcDesc, &LibPwmSourceConstructThermDescriptor);
}

/*!
 * @brief   Used for return value from @ref pmgrPwmSourceDescriptorConstruct_HAL
 *          in @ref LibPwmSourceConstructPmgr
 */
static PWM_SOURCE_DESCRIPTOR
LibPwmSourceConstructPmgrDescriptor;

/*!
 * @brief   Initializes state for @ref LibPwmSourceConstructPmgr
 */
static void
s_libPwmPreTestSourceConstructPmgr(void)
{
    pmgrHalMockInit();
    pmgrPwmSourceDescriptorConstruct_MOCK_fake.return_val = &LibPwmSourceConstructPmgrDescriptor;
}

/*!
 * @brief   Ensure pwmSourceDescriptorConstruct correctly constructs PMGR descriptors
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmSourceConstructPmgr)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

    s_libPwmPreTestSourceConstructPmgr();

    pPwmSrcDesc = pwmSourceDescriptorConstruct(0U, RM_PMU_PMGR_PWM_SOURCE_PMGR_PWM);

    UT_ASSERT_EQUAL_UINT(pmgrPwmSourceDescriptorConstruct_MOCK_fake.call_count, 1U);
    UT_ASSERT_EQUAL_UINT(pmgrPwmSourceDescriptorConstruct_MOCK_fake.arg0_val, RM_PMU_PMGR_PWM_SOURCE_PMGR_PWM);
    UT_ASSERT_EQUAL_UINT(pmgrPwmSourceDescriptorConstruct_MOCK_fake.arg1_val, 0U);
    UT_ASSERT_EQUAL_PTR(pPwmSrcDesc, &LibPwmSourceConstructPmgrDescriptor);
}

/*!
 * @brief   Used for return value from @ref pgIslandPwmSourceDescriptorConstruct_HAL
 *          in @ref LibPwmSourceConstructPg
 */
static PWM_SOURCE_DESCRIPTOR
LibPwmSourceConstructPgDescriptor;

/*!
 * @brief   Initializes state for @ref LibPwmSourceConstructPg
 */
static void
s_libPwmPreTestSourceConstructPg(void)
{
    pgHalMockInit();
    pgIslandPwmSourceDescriptorConstruct_MOCK_fake.return_val = &LibPwmSourceConstructPgDescriptor;
}

/*!
 * @brief   Ensure pwmSourceDescriptorConstruct correctly constructs pg descriptors
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmSourceConstructPg)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

    s_libPwmPreTestSourceConstructPg();

    pPwmSrcDesc = pwmSourceDescriptorConstruct(0U, RM_PMU_PMGR_PWM_SOURCE_SCI_VID_PWM_0);

    UT_ASSERT_EQUAL_UINT(pgIslandPwmSourceDescriptorConstruct_MOCK_fake.call_count, 1U);
    UT_ASSERT_EQUAL_UINT(pgIslandPwmSourceDescriptorConstruct_MOCK_fake.arg0_val, RM_PMU_PMGR_PWM_SOURCE_SCI_VID_PWM_0);
    UT_ASSERT_EQUAL_UINT(pgIslandPwmSourceDescriptorConstruct_MOCK_fake.arg1_val, 0U);
    UT_ASSERT_EQUAL_PTR(pPwmSrcDesc, &LibPwmSourceConstructPgDescriptor);
}

/*!
 * @brief   Ensure pwmSourceDescriptorConstruct returns an error on invalid sources
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmSourceConstructIlwalidSource)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

    pPwmSrcDesc = pwmSourceDescriptorConstruct(0U, RM_PMU_PMGR_PWM_SOURCE__COUNT);

    UT_ASSERT_NULL(pPwmSrcDesc);
}

/*!
 * @brief   Ensure pwmParamsGet_IMPL correctly returns an error when provided invalid pointers
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmParamsGetIlwalidPointers)
{
    FLCN_STATUS status;

    status = pwmParamsGet_IMPL(NULL, NULL, NULL, NULL, LW_FALSE);

    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief   Value to use for the PWM duty cycle in @ref LibPwmParamsGetTrigger
 */
const LwU32
LibPwmParamsGetTriggerAddrDutyCycle = 0xDEAD0000;

/*!
 * @brief   Value to use for the PWM period in @ref LibPwmParamsGetTrigger
 */
const LwU32
LibPwmParamsGetTriggerAddrPeriod = 0xDEAD0000;

/*!
 * @brief   Pre-test initialization for LibPwmParamsGetTrigger.
 *
 * @details Sets up PWM Params struct and corresponding registers with values
 *          that should be retrieved in test.
 */
static void s_libPwmPreTestParamsGetTrigger
(
    PWM_SOURCE_DESCRIPTOR_TRIGGER *pPwmSrcDescTrigger
)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc = &pPwmSrcDescTrigger->super;

    pPwmSrcDescTrigger->addrTrigger = 0U;
    pPwmSrcDesc->type               = PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER;
    pPwmSrcDesc->bus                = REG_BUS_CSB;
    pPwmSrcDesc->addrPeriod         = 1U;
    pPwmSrcDesc->addrDutycycle      = 2U;
    pPwmSrcDesc->mask               = 0xFFFFFFFF;
    pPwmSrcDesc->doneIdx            = 1U;

    REG_WR32(CSB, pPwmSrcDesc->addrDutycycle, LibPwmParamsGetTriggerAddrDutyCycle);
    REG_WR32(CSB, pPwmSrcDesc->addrPeriod, LibPwmParamsGetTriggerAddrPeriod);
    REG_WR32(CSB, pPwmSrcDescTrigger->addrTrigger, 0xFFFFFFFF);
}

/*!
 * @brief   Ensure pwmParamsGet_IMPL correctly retrieves control parameters for
 *          PWM_SOURCE_DESCRIPTOR_TRIGGER struct.
 *
 * @details Sets specific values in helper s_libPwmPreTestParamsGetTrigger that
 *          should be returned. bIlwert is true to exercise duty cycle ilwersion
 *          path, and doneIdx is on in the trigger field to indicate done status
 *          is false.
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmParamsGetTrigger)
{
    FLCN_STATUS                     status;
    PWM_SOURCE_DESCRIPTOR_TRIGGER   pwmSrcDescTrigger;
    PWM_SOURCE_DESCRIPTOR          *pPwmSrcDesc = &pwmSrcDescTrigger.super;
    LwU32                           pwmDutyCycle;
    LwU32                           pwmPeriod;
    LwBool                          bDone;

    s_libPwmPreTestParamsGetTrigger(&pwmSrcDescTrigger);

    status = pwmParamsGet_IMPL(pPwmSrcDesc, &pwmDutyCycle, &pwmPeriod, &bDone, LW_TRUE);

    if (PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER))
    {
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
        UT_ASSERT_EQUAL_UINT32(pwmPeriod, LibPwmParamsGetTriggerAddrPeriod);
        UT_ASSERT_EQUAL_UINT32(pwmDutyCycle, (pwmPeriod - LibPwmParamsGetTriggerAddrDutyCycle));
        UT_ASSERT(!bDone);
    }
    else
    {
        UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
    }
}

/*!
 * @brief   Value to use for the PWM duty cycle in @ref LibPwmParamsGetTriggerNonIlwert
 */
const LwU32
LibPwmParamsGetTriggerNonIlwertAddrDutyCycle = 0xDEAD0000;

/*!
 * @brief   Value to use for the PWM period in @ref LibPwmParamsGetTriggerNonIlwert
 */
const LwU32
LibPwmParamsGetTriggerNonIlwertAddrPeriod = 0xDEAD0000;

/*!
 * @brief   Pre-test initialization for LibPwmParamsGetTriggerNonIlwert.
 *
 * @details Sets up PWM Params struct and corresponding registers with values
 *          that should be retrieved in test.
 */
static void s_libPwmPreTestParamsGetTriggerNonIlwert
(
    PWM_SOURCE_DESCRIPTOR_TRIGGER *pPwmSrcDescTrigger
)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc = &pPwmSrcDescTrigger->super;

    pPwmSrcDescTrigger->addrTrigger = 0U;
    pPwmSrcDesc->type               = PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER;
    pPwmSrcDesc->bus                = REG_BUS_CSB;
    pPwmSrcDesc->addrPeriod         = 1U;
    pPwmSrcDesc->addrDutycycle      = 2U;
    pPwmSrcDesc->mask               = 0xFFFFFFFF;
    pPwmSrcDesc->doneIdx            = 1U;

    REG_WR32(CSB, pPwmSrcDesc->addrDutycycle, LibPwmParamsGetTriggerNonIlwertAddrDutyCycle);
    REG_WR32(CSB, pPwmSrcDesc->addrPeriod, LibPwmParamsGetTriggerNonIlwertAddrPeriod);
    REG_WR32(CSB, pPwmSrcDescTrigger->addrTrigger, 0xFFFFFFFF);
}

/*!
 * @brief   Ensure pwmParamsGet_IMPL correctly retrieves control parameters for
 *          PWM_SOURCE_DESCRIPTOR_TRIGGER struct when asked not to ilwert the
 *          duty cycle.
 *
 * @details Sets specific values in helper s_libPwmPreTestParamsGetTriggerNonIlwert that
 *          should be returned. bIlwert is false to skip the duty cycle ilwersion
 *          path, and doneIdx is on in the trigger field to indicate done status
 *          is false.
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmParamsGetTriggerNonIlwert)
{
    FLCN_STATUS                     status;
    PWM_SOURCE_DESCRIPTOR_TRIGGER   pwmSrcDescTrigger;
    PWM_SOURCE_DESCRIPTOR          *pPwmSrcDesc = &pwmSrcDescTrigger.super;
    LwU32                           pwmDutyCycle;
    LwU32                           pwmPeriod;
    LwBool                          bDone;

    s_libPwmPreTestParamsGetTriggerNonIlwert(&pwmSrcDescTrigger);

    status = pwmParamsGet_IMPL(pPwmSrcDesc, &pwmDutyCycle, &pwmPeriod, &bDone, LW_FALSE);
    if (PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER))
    {
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
        UT_ASSERT_EQUAL_UINT32(pwmPeriod, LibPwmParamsGetTriggerNonIlwertAddrPeriod);
        UT_ASSERT_EQUAL_UINT32(pwmDutyCycle, LibPwmParamsGetTriggerNonIlwertAddrDutyCycle);
        UT_ASSERT(!bDone);
    }
    else
    {
        UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
    }
}

/*!
 * @brief   Ensure pwmParamsGet_IMPL correctly returns an error when provided a descriptor with an invalid type.
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmParamsGetIlwalidDescriptorType)
{
    FLCN_STATUS status;

    status = pwmParamsGet_IMPL
    (
        &(PWM_SOURCE_DESCRIPTOR){ .type = PWM_SOURCE_DESCRIPTOR_TYPE_ILWALID, },
        &(LwU32){0},
        &(LwU32){0},
        &(LwBool){LW_FALSE},
        LW_FALSE
    );

    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief   Ensure pwmParamsSet correctly returns an error when provided invalid pointer
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmParamsSetIlwalidPointer)
{
    FLCN_STATUS status;

    status = pwmParamsSet(NULL, 0U, 0U, LW_FALSE, LW_FALSE);

    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief   Initial value to set in PWM_SOURCE_DESCRIPTOR_TRIGGER::addrTrigger
 *          register in @ref LibPwmParamsSetTrigger
 */
static const LwU32
LibPwmParamsSetTriggerTriggerRegInit = 0x10U;

/*!
 * @brief   Pre-test initialization for @ref LibPwmParamsSetTrigger.
 *
 * @details Sets up PWM Params struct and corresponding registers with values
 *          that should be replaced in test.
 */
static void s_libPwmPreTestParamsSetTrigger
(
    PWM_SOURCE_DESCRIPTOR_TRIGGER  *pPwmSrcDescTrigger
)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc = &pPwmSrcDescTrigger->super;

    pPwmSrcDescTrigger->addrTrigger = 0U;
    pPwmSrcDesc->type               = PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER;
    pPwmSrcDesc->bus                = REG_BUS_CSB;
    pPwmSrcDesc->addrPeriod         = 1U;
    pPwmSrcDesc->addrDutycycle      = 2U;
    pPwmSrcDesc->mask               = 0xFFFFFFFF;
    pPwmSrcDesc->doneIdx            = 1U;
    pPwmSrcDesc->triggerIdx         = 1U;
    pPwmSrcDesc->bCancel            = LW_TRUE;
    pPwmSrcDesc->bTrigger           = LW_TRUE;

    REG_WR32(CSB, pPwmSrcDesc->addrDutycycle, 0xDEAD0000);
    REG_WR32(CSB, pPwmSrcDesc->addrPeriod, 0xDEADBEEF);
    REG_WR32(CSB, pPwmSrcDescTrigger->addrTrigger, LibPwmParamsSetTriggerTriggerRegInit);
}

/*!
 * @brief   Ensure pwmParamsSet correctly sets control parameters for
 *          PWM_SOURCE_DESCRIPTOR_TRIGGER struct.
 *
 * @details Sets specific values in helper s_libPwmPreTestParamsGetTrigger that
 *          should be then be set in appropriate registers. bIlwert is true to
 *          exercise duty cycle ilwersion path; bTrigger and bIlwert are set to
 *          is on in the trigger to exercise those two paths and set two bits in
 *          the resultant trigger field.
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmParamsSetTrigger)
{
    FLCN_STATUS                     status;
    PWM_SOURCE_DESCRIPTOR_TRIGGER   pwmSrcDescTrigger;
    PWM_SOURCE_DESCRIPTOR          *pPwmSrcDesc = &pwmSrcDescTrigger.super;
    LwU32                           pwmDutyCycle = 0x12345678;
    LwU32                           pwmPeriod = 0x87654321;
    LwBool                          bTrigger = LW_TRUE;
    LwBool                          bIlwert = LW_TRUE;

    s_libPwmPreTestParamsSetTrigger(&pwmSrcDescTrigger);

    status = pwmParamsSet(pPwmSrcDesc, pwmDutyCycle, pwmPeriod, bTrigger, bIlwert);

    if (PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER))
    {
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
        UT_ASSERT_EQUAL_UINT32(pwmPeriod, REG_RD32(CSB, pPwmSrcDesc->addrPeriod));
        UT_ASSERT_EQUAL_UINT32((pwmPeriod - pwmDutyCycle), REG_RD32(CSB, pPwmSrcDesc->addrDutycycle));
        UT_ASSERT_EQUAL_UINT32
        (
            LibPwmParamsSetTriggerTriggerRegInit | LWBIT32(pPwmSrcDesc->triggerIdx),
            REG_RD32(CSB, pwmSrcDescTrigger.addrTrigger)
        );
    }
    else
    {
        UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
    }
}

/*!
 * @brief   Initial value to set in PWM_SOURCE_DESCRIPTOR_TRIGGER::addrTrigger
 *          register in @ref LibPwmParamsSetTriggerSkipOptional
 */
static const LwU32
LibPwmParamsSetTriggerSkipOptionalTriggerRegInit = 0x10U;

/*!
 * @brief   Pre-test initialization for libPwmParamsSetTriggerSkipOptional.
 *
 * @details Sets up PWM Params struct and corresponding registers with values
 *          that should be replaced in test.
 */
static void s_libPwmPreTestParamsSetTriggerSkipOptional
(
    PWM_SOURCE_DESCRIPTOR_TRIGGER  *pPwmSrcDescTrigger
)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc = &pPwmSrcDescTrigger->super;

    pPwmSrcDescTrigger->addrTrigger = 0U;
    pPwmSrcDesc->type               = PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER;
    pPwmSrcDesc->bus                = REG_BUS_CSB;
    pPwmSrcDesc->addrPeriod         = 1U;
    pPwmSrcDesc->addrDutycycle      = 2U;
    pPwmSrcDesc->mask               = 0xFFFFFFFF;
    pPwmSrcDesc->doneIdx            = 1U;
    pPwmSrcDesc->triggerIdx         = 1U;
    pPwmSrcDesc->bCancel            = LW_FALSE;
    pPwmSrcDesc->bTrigger           = LW_FALSE;

    REG_WR32(CSB, pPwmSrcDesc->addrDutycycle, 0xDEAD0000);
    REG_WR32(CSB, pPwmSrcDesc->addrPeriod, 0xDEADBEEF);
    REG_WR32(CSB, pPwmSrcDescTrigger->addrTrigger, LibPwmParamsSetTriggerSkipOptionalTriggerRegInit);
}

/*!
 * @brief   Ensure pwmParamsSet correctly sets control parameters for
 *          PWM_SOURCE_DESCRIPTOR_TRIGGER struct.
 *
 * @details Sets specific values in helper s_libPwmPreTestParamsGetTrigger that
 *          should be then be set in appropriate registers. bIlwert is false to
 *          skip duty cycle ilwersion path; bTrigger and bCancel are set to
 *          off in the trigger to skip those two paths for setting the two bits
 *          in the resultant trigger field.
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmParamsSetTriggerSkipOptional)
{
    FLCN_STATUS                     status;
    PWM_SOURCE_DESCRIPTOR_TRIGGER   pwmSrcDescTrigger;
    PWM_SOURCE_DESCRIPTOR          *pPwmSrcDesc = &pwmSrcDescTrigger.super;
    LwU32                           pwmDutyCycle = 0x12345678;
    LwU32                           pwmPeriod = 0x87654321;
    LwBool                          bTrigger = LW_FALSE;
    LwBool                          bIlwert = LW_FALSE;

    s_libPwmPreTestParamsSetTriggerSkipOptional(&pwmSrcDescTrigger);

    status = pwmParamsSet(pPwmSrcDesc, pwmDutyCycle, pwmPeriod, bTrigger, bIlwert);

    if (PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER))
    {
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
        UT_ASSERT_EQUAL_UINT32(pwmPeriod, REG_RD32(CSB, pPwmSrcDesc->addrPeriod));
        UT_ASSERT_EQUAL_UINT32(pwmDutyCycle, REG_RD32(CSB, pPwmSrcDesc->addrDutycycle));
        (
            LibPwmParamsSetTriggerSkipOptionalTriggerRegInit | LWBIT32(pPwmSrcDesc->triggerIdx),
            REG_RD32(CSB, pwmSrcDescTrigger.addrTrigger)
        );
    }
    else
    {
        UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
    }
}

/*!
 * @brief   Ensure pwmParamsSet correctly returns an error when provided a
 *          descriptor with an invalid type.
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmParamsSetIlwalidDescriptorType)
{
    FLCN_STATUS status;

    status = pwmParamsSet
    (
        &(PWM_SOURCE_DESCRIPTOR){ .type = PWM_SOURCE_DESCRIPTOR_TYPE_ILWALID, },
        0U,
        0U,
        LW_FALSE,
        LW_FALSE
    );

    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief   Initializes state for @ref LibPwmSourceDescriptorAllocateIlwalildPointer
 *
 * @details Initializes the following:
 *              Resets the internal state of the PWM library (@ref pwmSourceDescriptorsConstructedMask)
 */
static void
s_libPwmSourceDescriptorAllocateIlwalildPointer(void)
{
    pwmSourceDescriptorsConstructedMask = 0U;
}

/*!
 * @brief   Ensure pwmSourceDescriptorAllocate_IMPL returns an error when provided
 *          invalid pointer
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateIlwalildPointer)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

    s_libPwmSourceDescriptorAllocateIlwalildPointer();

    pPwmSrcDesc = pwmSourceDescriptorAllocate_IMPL(0U, RM_PMU_PMGR_PWM_SOURCE_PMGR_FAN, NULL);

    UT_ASSERT_NULL(pPwmSrcDesc);
}

/*!
 * @brief   Initializes state for @ref LibPwmSourceDescriptorAllocateIlwalildSource
 *
 * @details Initializes the following:
 *              Resets the internal state of the PWM library (@ref pwmSourceDescriptorsConstructedMask)
 */
static void
s_libPwmSourceDescriptorAllocateIlwalildSource(void)
{
    pwmSourceDescriptorsConstructedMask = 0U;
}

/*!
 * @brief   Ensure pwmSourceDescriptorAllocate_IMPL correctly returns an error when
 *          provided an invalid PWM source
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateIlwalidSource)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

    s_libPwmSourceDescriptorAllocateIlwalildSource();

    pPwmSrcDesc = pwmSourceDescriptorAllocate_IMPL
    (
        0U,
        RM_PMU_PMGR_PWM_SOURCE__COUNT,
        &(PWM_SOURCE_DESCRIPTOR){0}
    );

    UT_ASSERT_NULL(pPwmSrcDesc);
}

/*!
 * @brief   Initializes state for @ref LibPwmSourceDescriptorAllocateIlwalildType
 *
 * @details Initializes the following:
 *              Resets the internal state of the PWM library (@ref pwmSourceDescriptorsConstructedMask)
 */static void
s_libPwmSourceDescriptorAllocateIlwalildType(void)
{
    pwmSourceDescriptorsConstructedMask = 0U;
}

/*!
 * @brief   Ensure pwmSourceDescriptorAllocate_IMPL correctly returns an error when
 *          provided an invalid PWM type
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateIlwalidType)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;

    s_libPwmSourceDescriptorAllocateIlwalildType();

    pPwmSrcDesc = pwmSourceDescriptorAllocate_IMPL
    (
        0U,
        RM_PMU_PMGR_PWM_SOURCE_PMGR_FAN,
        &(PWM_SOURCE_DESCRIPTOR){ .type = PWM_SOURCE_DESCRIPTOR_TYPE_ILWALID, }
    );

    UT_ASSERT_NULL(pPwmSrcDesc);
}

/*!
 * @brief   Initializes state for @ref LibPwmSourceDescriptorAllocateCallocFail
 *
 * @details Initializes:
 *              calloc mocking (via @ref callocMockInit)
 *                  Includes setting up NULL return value
 *              Resets the internal state of the PWM library (@ref pwmSourceDescriptorsConstructedMask)
 */
static void
s_libPwmSourceDescriptorAllocateCallocFailPreTest(void)
{
    callocMockInit();
    callocMockAddEntry(0U, NULL, NULL);
    pwmSourceDescriptorsConstructedMask = 0U;
}

/*!
 * @brief   Ensure pwmSourceDescriptorAllocate_IMPL correctly returns an error when
 *          memory allocation fails
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateCallocFail)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc;
    const LwU8 pwmType =
        PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)     ? PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT :
        PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)     ? PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER :
        PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)   ? PWM_SOURCE_DESCRIPTOR_TYPE_CONFIGURE :
                                                          PWM_SOURCE_DESCRIPTOR_TYPE_ILWALID;

    s_libPwmSourceDescriptorAllocateCallocFailPreTest();

    pPwmSrcDesc = pwmSourceDescriptorAllocate_IMPL
    (
        0U,
        RM_PMU_PMGR_PWM_SOURCE_PMGR_FAN,
        &(PWM_SOURCE_DESCRIPTOR){ .type = pwmType, }
    );

    UT_ASSERT_EQUAL_UINT(callocMockNumCalls(), 1U);
    UT_ASSERT_NULL(pPwmSrcDesc);
}

/*!
 * @brief   Construct @ref PWM_SOURCE_DESCRIPTOR for
 *          @ref LibPwmSourceDescriptorAllocateTrigger; also the expected data
 *          for the return value from @ref pwmSourceDescriptorAllocate_IMPL
 */
static PWM_SOURCE_DESCRIPTOR_TRIGGER
LibPwmSourceDescriptorAllocateTriggerDescExpected =
{
    .super =
    {
        .type = PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER,
        .triggerIdx = 2U,
        .doneIdx = 7U,
        .bus = REG_BUS_CSB,
        .addrPeriod = 0xf0U,
        .addrDutycycle = 0x10U,
        .mask = 0x7fU,
        .bCancel = LW_TRUE,
        .bTrigger = LW_TRUE,
    },
    .addrTrigger = 0xb0U,
};

/*!
 * @brief   PWM structure for mock allocation in
 *          @ref LibPwmSourceDescriptorAllocateTrigger
 */
static PWM_SOURCE_DESCRIPTOR_TRIGGER
LibPwmSourceDescriptorAllocateTriggerDescAllocate = {0};

/*!
 * @brief   Expected parameters for call to @ref lwosCalloc in
 *          @ref LibPwmSourceDescriptorAllocateTrigger
 */
static const
CALLOC_MOCK_EXPECTED_VALUE
LibPwmSourceDescriptorAllocateTriggerCallocExpect =
{
    .ovlIdx = OVL_INDEX_OS_HEAP,
    .size = sizeof(LibPwmSourceDescriptorAllocateTriggerDescExpected),
};

/*!
 * @brief   Initializes state for @ref LibPwmSourceDescriptorAllocateTrigger
 *
 * @details Initializes:
 *              Calloc mocking (via @ref callocMockInit)
 *                  Sets up expected call parameters to @ref LibPwmSourceDescriptorAllocateTriggerCallocExpect
 *                  Sets up return value to @ref LibPwmSourceDescriptorAllocateTriggerDescAllocate
 *              Resets the internal state of the PWM library (@ref pwmSourceDescriptorsConstructedMask)
 */
static void
s_libPwmSourceDescriptorAllocateTriggerPreTest(void)
{
    callocMockInit();
    callocMockAddEntry(0U,
                       &LibPwmSourceDescriptorAllocateTriggerDescAllocate,
                       &LibPwmSourceDescriptorAllocateTriggerCallocExpect);
    pwmSourceDescriptorsConstructedMask = 0U;
}

/*!
 * @brief   Ensure pwmSourceDescriptorAllocate_IMPL correctly allocates trigger PWMs
 */
UT_CASE_RUN(PMU_LIB_PWM, LibPwmSourceDescriptorAllocateTrigger)
{
    PWM_SOURCE_DESCRIPTOR_TRIGGER *pPwmSrcDesc;

    s_libPwmSourceDescriptorAllocateTriggerPreTest();

    pPwmSrcDesc = pwmSourceDescriptorAllocate_IMPL
    (
        LibPwmSourceDescriptorAllocateTriggerCallocExpect.ovlIdx,
        RM_PMU_PMGR_PWM_SOURCE_PMGR_FAN,
        &LibPwmSourceDescriptorAllocateTriggerDescExpected.super
    );

    UT_ASSERT_EQUAL_MEM
    (
        &LibPwmSourceDescriptorAllocateTriggerDescExpected,
        &LibPwmSourceDescriptorAllocateTriggerDescAllocate,
        sizeof(LibPwmSourceDescriptorAllocateTriggerDescExpected)
    );
}
