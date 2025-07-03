/*
 * Copyright (c) 2019, LWPU CORPORATION. All rights reserved.
 *
 * LWPU CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from LWPU CORPORATION is strictly prohibited.
 */

#include "test-macros.h"
#include "acr.h"
#include "rmlsfm.h"
#include "t21x/t210/dev_pwr_csb.h"
#include "g_acr_hal.h"

/* TODO: port this suite to using LibUT APIs */
UT_SUITE_DEFINE(ACR_FIND_WPR,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))

UT_SUITE_DEFINE(ACR_SCRUB_WPR,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))

RM_FLCN_ACR_DESC    g_desc;
ACR_DMA_PROP        g_dmaProp;
extern ACR_SCRUB_STATE     g_scrubState;

void clear_globals(void)
{
    memset(&g_desc, 0, sizeof(RM_FLCN_ACR_DESC));
    memset(&g_dmaProp, 0, sizeof(ACR_DMA_PROP));
    memset(&g_scrubState, 0, sizeof(ACR_SCRUB_STATE));
}

void print_status(ACR_STATUS status)
{
    if (status == ACR_OK)
        utf_printf("RETURN ACR_OK\n");
    else if (status == ACR_ERROR_ILWALID_REGION_ID)
        utf_printf("RETURN ACR_ERROR_ILWALID_REGION_ID\n");
    else if (status == ACR_ERROR_NO_WPR)
        utf_printf("RETURN ACR_ERROR_NO_WPR\n");
    else if (status == ACR_ERROR_DMA_FAILURE)
        utf_printf("RETURN ACR_ERROR_NO_WPR\n");
    else
        utf_printf("Unexpected output\n");
}

void falcon_setup(void)
{
    falc_stxb_i((LwU32*) LW_CPWR_FALCON_DMEMAPERT, 0,
        DRF_NUM(_CPWR_FALCON, _DMEMAPERT, _ENABLE,   1) |
        DRF_NUM(_CPWR_FALCON, _DMEMAPERT, _TIME_UNIT,2) |
        DRF_NUM(_CPWR_FALCON, _DMEMAPERT, _TIME_OUT, 255));
}

UT_CASE_DEFINE(ACR_FIND_WPR, WPRTestValidArgs,
  UT_CASE_SET_DESCRIPTION("TODO")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(ACR_FIND_WPR, WPRTestValidArgs)
{
    LwU32      wprIndex = 0;
    ACR_STATUS status = ACR_ERROR_NO_WPR;

    falcon_setup();
    clear_globals();
    // Ideal Inputs for FindWprRegions as there is only 1 WPR region andwprRegionID is not known so set to 0x0.
    g_desc.regions.noOfRegions = 1;
    g_desc.wprRegionID = 0;

    status = acrFindWprRegions_T234(&wprIndex);

    print_status(status);

    utf_printf("START address is %x\n", g_desc.regions.regionProps[0].startAddress);
    utf_printf("END address is %x\n", g_desc.regions.regionProps[0].endAddress);

    UT_ASSERT_EQUAL_UINT(status, ACR_OK);
}

UT_CASE_DEFINE(ACR_FIND_WPR, WPRTestRegionsLessThanWprRegionId,
  UT_CASE_SET_DESCRIPTION("TODO")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(ACR_FIND_WPR, WPRTestRegionsLessThanWprRegionId)
{
    LwU32      wprIndex = 0;
    ACR_STATUS status = ACR_ERROR_NO_WPR;

    clear_globals();
    // Negative test case 1
    g_desc.regions.noOfRegions = 1;
    g_desc.wprRegionID = 2;
    status = acrFindWprRegions_T234(&wprIndex);

    print_status(status);

    utf_printf("START address is %x\n", g_desc.regions.regionProps[0].startAddress);
    utf_printf("END address is %x\n", g_desc.regions.regionProps[0].endAddress);

    UT_ASSERT_EQUAL_UINT(status, ACR_OK);
}

UT_CASE_DEFINE(ACR_FIND_WPR, WPRTestRegionIdwithIncorrectRegionProps,
  UT_CASE_SET_DESCRIPTION("TODO")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(ACR_FIND_WPR, WPRTestRegionIdwithIncorrectRegionProps)
{
    LwU32      wprIndex = 0;
    ACR_STATUS status = ACR_ERROR_NO_WPR;

    clear_globals();
    // Negative test case 2
    g_desc.regions.noOfRegions = 1;
    g_desc.wprRegionID = 4;
    g_desc.regions.regionProps[0].regionID = 4;

    status = acrFindWprRegions_T234(&wprIndex);

    print_status(status);

    utf_printf("START address is %x\n", g_desc.regions.regionProps[0].startAddress);
    utf_printf("END address is %x\n", g_desc.regions.regionProps[0].endAddress);

    UT_ASSERT_EQUAL_UINT(status, ACR_OK);
}

UT_CASE_DEFINE(ACR_FIND_WPR, WPRTestNoRegionsWithWprId,
  UT_CASE_SET_DESCRIPTION("TODO")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(ACR_FIND_WPR, WPRTestNoRegionsWithWprId)
{
    LwU32      wprIndex = 0;
    ACR_STATUS status   = ACR_ERROR_NO_WPR;

    clear_globals();

    // Negative test case 4
    g_desc.regions.noOfRegions = 0;
    g_desc.wprRegionID = 2;

    status = acrFindWprRegions_T234(&wprIndex);

    print_status(status);

    utf_printf("START address is %x\n", g_desc.regions.regionProps[0].startAddress);
    utf_printf("END address is %x\n", g_desc.regions.regionProps[0].endAddress);

    UT_ASSERT_EQUAL_UINT(status, ACR_OK);
}

UT_CASE_DEFINE(ACR_SCRUB_WPR, WPRTestScrubWprDoNothing,
  UT_CASE_SET_DESCRIPTION("TODO")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))
 
UT_CASE_RUN(ACR_SCRUB_WPR, WPRTestScrubWprDoNothing)
{
    ACR_STATUS status     = ACR_ERROR_NO_WPR;
    LwU32      nextOffset = 0;
    LwU32      size       = 0;

    clear_globals();

    g_scrubState.scrubTrack = 40;
    nextOffset = 32;
    size = 16;

    status = acrScrubUnusedWprWithZeroes_GM200(nextOffset, size);
    print_status(status);
    UT_ASSERT_EQUAL_UINT(status, ACR_OK);
}

UT_CASE_DEFINE(ACR_SCRUB_WPR, WPRTestScrubWprDmaZeroes,
  UT_CASE_SET_DESCRIPTION("TODO")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(ACR_SCRUB_WPR, WPRTestScrubWprDmaZeroes)
{
    ACR_STATUS status     = ACR_ERROR_NO_WPR;
    LwU32      nextOffset = 0;
    LwU32      size       = 0;

    clear_globals();
    g_scrubState.scrubTrack = 24;
    nextOffset = 32;
    size = 16;

    status = acrScrubUnusedWprWithZeroes_GM200(nextOffset, size);
    print_status(status);
    UT_ASSERT_EQUAL_UINT(status, ACR_OK);
}
