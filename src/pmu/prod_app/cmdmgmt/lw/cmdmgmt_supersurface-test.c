/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   cmdmgmt_supersurface-test.c
 * @brief  This file contains unit tests to verify that the functions in
 *         cmdmgmt_supersurface.c work correctly.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "lwos_dma-mock.h"
#include "rmpmusupersurfif.h"
#include "lwos_ovl_desc.h"
#include "main.h"

/* ------------------------ Application Includes ---------------------------- */
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurpc.h"

/* ------------------------ Overlay Variables ------------------------------- */
/*!
 * @brief   cmdmgmt_supersurface.c tries to attach this overlay, but overlays aren't
 *          defined for the test build, so we define it here to a meaningless
 *          value
 *
 * @note    TODO: Find a general way to solve this issue for all PMU ucode
 */
OSTASK_OVL_DESC _overlay_id_dmem_cmdmgmtSsmd = 0;


/* ------------------------ Tests ------------------------------------------- */
/*!
 * Definition of the CMDMGMT Supersurface Test Suite.
 */
UT_SUITE_DEFINE(PMU_CMDMGMT_CMDMGMTSUPERSURFACE,
                UT_SUITE_SET_COMPONENT("CmdMgmt Supersurface")
                UT_SUITE_SET_DESCRIPTION("Unit tests for CmdMgmt Supersurface code")
                UT_SUITE_SET_OWNER("mmints")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))

UT_CASE_DEFINE(PMU_CMDMGMT_CMDMGMTSUPERSURFACE, cmdmgmtSuperSurfaceMemberDescriptorsInit_Negative,
                UT_CASE_SET_DESCRIPTION("Ensure cmdmgmtSuperSurfaceMemberDescriptorsInit fail when expected")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))


UT_CASE_DEFINE(PMU_CMDMGMT_CMDMGMTSUPERSURFACE, cmdmgmtSuperSurfaceMemberDescriptorsInit_Positive,
                UT_CASE_SET_DESCRIPTION("Ensure cmdmgmtSuperSurfaceMemberDescriptorsInit works correctly")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that verifies that @ref cmdmgmtSuperSurfaceMemberDescriptorsInit
 *             fails when expected.
 *
 * @details    Make sure that @ref cmdmgmtSuperSurfaceMemberDescriptorsInit propagates
 *             error codes from dmaWrite correctly if it fails.
 *
 * @see        dmaWrite_MOCK_CMDMGMTSUPERSURFACE
 */
UT_CASE_RUN(PMU_CMDMGMT_CMDMGMTSUPERSURFACE, cmdmgmtSuperSurfaceMemberDescriptorsInit_Negative)
{
    const FLCN_STATUS expectedErrors[] =
    {
        FLCN_ERR_DMA_NACK,
        FLCN_ERROR,
    };

    dmaMockInit();
    dmaMockWriteAddEntry(0U, expectedErrors[0U]);

    UT_ASSERT_EQUAL_UINT(expectedErrors[0U], cmdmgmtSuperSurfaceMemberDescriptorsInit_IMPL());
    UT_ASSERT_EQUAL_UINT(dmaMockWriteNumCalls(), 1U);

    dmaMockInit();
    dmaMockWriteAddEntry(0U, expectedErrors[1U]);

    UT_ASSERT_EQUAL_UINT(expectedErrors[1U], cmdmgmtSuperSurfaceMemberDescriptorsInit_IMPL());
    UT_ASSERT_EQUAL_UINT(dmaMockWriteNumCalls(), 1U);
}

/*!
 * @brief   Fake set of @ref RM_PMU_SUPER_SURFACE_MEMBER_DESCRIPTOR variables
 *          for @ref cmdmgmtSuperSurfaceMemberDescriptorsInit to DMA to in
 *          @ref cmdmgmtSuperSurfaceMemberDescriptorsInit_Positive
 */
static RM_PMU_SUPER_SURFACE_MEMBER_DESCRIPTOR
CmdmgmtSuperSurfaceMockDescriptors[RM_PMU_SUPER_SURFACE_MEMBER_DESCRIPTOR_COUNT];

/*!
 * @brief   Mocked memory region for @ref CmdmgmtSuperSurfaceMockDescriptors
 */
const DMA_MOCK_MEMORY_REGION
CmdmgmtSuperSurfaceMockDescriptorsRegion =
{
    .pMemDesc = &PmuInitArgs.superSurface,
    .pData = &CmdmgmtSuperSurfaceMockDescriptors,
    .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ssmd),
    .sizeOfData = sizeof(CmdmgmtSuperSurfaceMockDescriptors),
};

/*!
 * @brief      Test that verifies that @ref cmdmgmtSuperSurfaceMemberDescriptorsInit
 *             works correctly.
 *
 * @details    Make sure that @ref cmdmgmtSuperSurfaceMemberDescriptorsInit succeeds
 *             when dmaWrite succeeds, also using the dmaWrite mock that verifies
 *             that the arguments to it are correct.
 *
 * @see        dmaWrite_MOCK_CMDMGMTSUPERSURFACE
 */
UT_CASE_RUN(PMU_CMDMGMT_CMDMGMTSUPERSURFACE, cmdmgmtSuperSurfaceMemberDescriptorsInit_Positive)
{
    dmaMockInit();
    dmaMockConfigMemoryRegionInsert(&CmdmgmtSuperSurfaceMockDescriptorsRegion);

    UT_ASSERT_EQUAL_UINT(FLCN_OK, cmdmgmtSuperSurfaceMemberDescriptorsInit_IMPL());
    UT_ASSERT_EQUAL_UINT(dmaMockWriteNumCalls(), 1U);
}
