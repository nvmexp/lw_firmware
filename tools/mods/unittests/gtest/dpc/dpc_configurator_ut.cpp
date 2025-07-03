/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <iostream>
#include <string>
#include "gmock/gmock.h"

#include "dpc_private_impl.h"
#include "gpu/display/lwdisplay/lw_disp_c6.h"
#include "class/cl507d.h"
#include "class/cl887d.h"

using namespace std;
using namespace testing;
using namespace DispPipeConfig;
///////////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////////
class LwDisplayMock : public LwDisplayC6
{
public:
    LwDisplayMock() : LwDisplayC6(nullptr) { };
    MOCK_CONST_METHOD1(IsMultiStreamDisplay, bool(UINT32));
    MOCK_METHOD1(GetDetected, RC(DisplayIDs*));
};
///////////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////////
class DpcConfiguratorSorAssignment : public Test
{
public:
    unique_ptr<DispPipeConfiguratorPimpl> pPimpl;
    DPCCfgEntry dpcCfgEntry;
    UINT32 sorExcludeMask = 0;
    LwDisplayMock lwDisplayMock;

    DpcConfiguratorSorAssignment():pPimpl(new DispPipeConfiguratorPimpl(nullptr))
    {
        // Needed for DispPipeConfiguratorPimpl::ComputeSorExcludeMaskForSingleHeadMst
        EXPECT_CALL(lwDisplayMock, IsMultiStreamDisplay(_)).Times(1).WillOnce(Return(false));
        pPimpl->m_pLwDisplay = &lwDisplayMock;
        dpcCfgEntry.m_DisplayIDs = { 0x100 };
    }

};
///////////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////////
class DpcConfiguratorSorAssignmentSingleHead : public DpcConfiguratorSorAssignment
{
public:
    Display::AssignSorSetting assignSorSetting = Display::AssignSorSetting::ONE_HEAD_MODE;
};

TEST_F(DpcConfiguratorSorAssignmentSingleHead, AllSorsSpecifiedExplicitly)
{
    dpcCfgEntry.m_OrIndex = 0;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 0xF; // 0xF for explicitly specified

    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0xE);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////////
class DpcConfiguratorSorAssignment2Head : public DpcConfiguratorSorAssignment
{
public:
    LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;
    Display::AssignSorSetting assignSorSetting = Display::AssignSorSetting::TWO_HEADS_MODE;
};

TEST_F(DpcConfiguratorSorAssignment2Head, WhenSorIsNotSpecified)
{
    dpcCfgEntry.m_OrIndex = UNINITIALIZED_SOR_INDEX;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 0;

    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0x0);
}

TEST_F(DpcConfiguratorSorAssignment2Head, WhenSorIsNotSpecifiedButOthersHaveSpecified)
{
    dpcCfgEntry.m_OrIndex = UNINITIALIZED_SOR_INDEX;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 1<<2 | 1<<0;

    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0x5);
}

TEST_F(DpcConfiguratorSorAssignment2Head, WhenSorIsSpecifiedLtE1)
{
    dpcCfgEntry.m_OrIndex = 1;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 0;

    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0xC);
}

TEST_F(DpcConfiguratorSorAssignment2Head, WhenSorIsSpecifiedGt1)
{
    dpcCfgEntry.m_OrIndex = 3;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 0;

    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0x6);
}

TEST_F(DpcConfiguratorSorAssignment2Head, WhenSorIsSpecifiedComplexCase)
{
    dpcCfgEntry.m_OrIndex = 3;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 1<<0 | 1<<2;

    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0x5);
}

TEST_F(DpcConfiguratorSorAssignment2Head, SorsNotAvailable)
{
    dpcCfgEntry.m_OrIndex = 3;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 0xF;

    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfiguratorSorAssignment2Head, WhenSorZeroIsSpecified)
{
    dpcCfgEntry.m_OrIndex = 0;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 1<<1 | 1<<2;

    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0x6);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////////
class DpcConfiguratorSorAssignmentEDP : public DpcConfiguratorSorAssignment
{
public:
    Display::AssignSorSetting assignSorSetting = Display::AssignSorSetting::ONE_HEAD_MODE;

    const UINT08 m_PadlinkLetter = 'C';
    const UINT32 m_ReservedPadlink = m_PadlinkLetter - 'A';
    const UINT32 m_ReservedSorIndex = 2;

    void UpdateReservedEdpMap_PadlinkC_Sor2()
    {
        pPimpl->m_ReservedSORInfoMap[1<<m_ReservedPadlink] = m_ReservedSorIndex;
    }

    void SetUp()
    {
        UpdateReservedEdpMap_PadlinkC_Sor2();
    }
};

TEST_F(DpcConfiguratorSorAssignmentEDP, ReservedSorShouldBeAllocatedForEDP)
{
    dpcCfgEntry.m_PadLinksMask = 1 << m_ReservedPadlink;
    dpcCfgEntry.m_OrIndex = UNINITIALIZED_SOR_INDEX;
    dpcCfgEntry.m_Protocol = CrcHandler::CRCProtocol::CRC_DP_SST;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 1<<3; // SOR 3 in use

    SorExcludeMask sorExcludeMask = 0x0;
    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);

    ASSERT_THAT(sorExcludeMask, 0xB); // 0b1011 so that SOR2 gets assigned
}

TEST_F(DpcConfiguratorSorAssignmentEDP, ReservedSorShouldNotBeAllocatedForNonEDP)
{
    // Fail if no other SOR is available
    constexpr UINT32 nonReservedPadLink = ('D' - 'A');
    dpcCfgEntry.m_PadLinksMask = 1 << nonReservedPadLink;
    dpcCfgEntry.m_OrIndex = UNINITIALIZED_SOR_INDEX;
    dpcCfgEntry.m_Protocol = CrcHandler::CRCProtocol::CRC_DP_SST;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 1<<3 | 1<< 1| 1<<0; // SOR 2 is free but reserved

    SorExcludeMask sorExcludeMask = 0x0;
    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfiguratorSorAssignmentEDP, WithReservedPadlink_TMDS_SorNotSpecified_ShouldPickNonResevedSor)
{
    dpcCfgEntry.m_PadLinksMask = 1 << m_ReservedPadlink;
    dpcCfgEntry.m_OrIndex = UNINITIALIZED_SOR_INDEX;
    dpcCfgEntry.m_Protocol = CrcHandler::CRCProtocol::CRC_TMDS_A;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 1<<3;

    SorExcludeMask sorExcludeMask = 0x0;
    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);
}

TEST_F(DpcConfiguratorSorAssignmentEDP,
       WithReservedPadlink_DpSstDualHead_SorNotpecified_ShouldPickResevedSor)
{
    dpcCfgEntry.m_PadLinksMask = 1 << m_ReservedPadlink;
    dpcCfgEntry.m_OrIndex = UNINITIALIZED_SOR_INDEX;
    dpcCfgEntry.m_Protocol = CrcHandler::CRCProtocol::CRC_DP_SST;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 1<<3;
    Display::AssignSorSetting assignSorSetting = Display::AssignSorSetting::TWO_HEADS_MODE;

    SorExcludeMask sorExcludeMask = 0x0;
    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);

    ASSERT_THAT(sorExcludeMask, 0xA);
}

TEST_F(DpcConfiguratorSorAssignmentEDP, WithReservedPadlink_DpSst_SorNotSpecified_ShouldPickEdpSor)
{
    dpcCfgEntry.m_PadLinksMask = 1 << m_ReservedPadlink;
    dpcCfgEntry.m_OrIndex = UNINITIALIZED_SOR_INDEX;
    dpcCfgEntry.m_Protocol = CrcHandler::CRCProtocol::CRC_DP_SST;
    pPimpl->m_SorExcludeMaskLwrrentLoop = 1<<3;

    SorExcludeMask sorExcludeMask = 0x0;
    ASSERT_THAT(pPimpl->ComputeSorExcludeMask(dpcCfgEntry,
        assignSorSetting, &sorExcludeMask), RC::OK);

    ASSERT_THAT(sorExcludeMask, 0xB);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////////
class DpcConfigValidatorMst : public Test
{
public:
    DPCCfgEntries dpcCfgEntries;
    LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;
    DpcConfigValidator dpcConfigValidator;
    PadlinkToOrIndex padlinkToOrIndex;
    DynamicDisplayId2Headmask dynamicId2HeadmaskMap;
    LwDisplayMock lwDisplayMock;
    unique_ptr<DispPipeConfiguratorPimpl> pPimpl;

    const PadLinksMask m_UnspecifiedPadlinksMask = 0x0;

    void AddDpcCfgEntryPadlink
    (
        CrcHandler::CRCProtocol protocol,
        PadLinksMask padLinksMask
    );
    void AddDpcCfgEntrySor
    (
        CrcHandler::CRCProtocol protocol,
        INT32 orIndex
    );
    void AddDpcCfgEntrySorAndPadlink
    (
        CrcHandler::CRCProtocol protocol,
        INT32 orIndex,
        PadLinksMask padLinksMask
    );
    void AddDpcCfgEntryEx
    (
        CrcHandler::CRCProtocol protocol,
        INT32 orIndex,
        PadLinksMask padLinksMask,
        HeadMask headMask
    );
    void AddDummyEntryInConnDescription(const DisplayID& displayId);
    void BuildConnDescription();
    void AddDummyEntryInDynamic2RootDisplayIDMap
    (
        const DisplayID& dynamicDisplayId,
        const DisplayID& rootDisplayId
    );

    DpcConfigValidatorMst()
    : dpcConfigValidator(static_cast<INT32>(Tee::PriNone),
          false, false, nullptr, reservedSORInfoMap, 0x0),
          pPimpl(new DispPipeConfiguratorPimpl(nullptr))
    {

    }
};

void DpcConfigValidatorMst::AddDpcCfgEntryPadlink
(
    CrcHandler::CRCProtocol protocol,
    PadLinksMask padLinksMask
)
{
    DPCCfgEntry dpcCfgEntry;
    dpcCfgEntry.m_Protocol = protocol;
    dpcCfgEntry.m_PadLinksMask = padLinksMask;
    dpcCfgEntries.push_back(dpcCfgEntry);
}

void DpcConfigValidatorMst::AddDpcCfgEntrySor
(
    CrcHandler::CRCProtocol protocol,
    INT32 orIndex
)
{
    DPCCfgEntry dpcCfgEntry;
    dpcCfgEntry.m_Protocol = protocol;
    dpcCfgEntry.m_OrIndex = orIndex;
    dpcCfgEntries.push_back(dpcCfgEntry);
}

void DpcConfigValidatorMst::AddDpcCfgEntrySorAndPadlink
(
   CrcHandler::CRCProtocol protocol,
   INT32 orIndex,
   PadLinksMask padLinksMask
)
{
    DPCCfgEntry dpcCfgEntry;
    dpcCfgEntry.m_Protocol = protocol;
    dpcCfgEntry.m_OrIndex = orIndex;
    dpcCfgEntry.m_PadLinksMask = padLinksMask;
    dpcCfgEntries.push_back(dpcCfgEntry);
}

void DpcConfigValidatorMst::AddDpcCfgEntryEx
(
    CrcHandler::CRCProtocol protocol,
    INT32 orIndex,
    PadLinksMask padLinksMask,
    HeadMask headMask
)
{
    DPCCfgEntry dpcCfgEntry;
    dpcCfgEntry.m_Protocol = protocol;
    dpcCfgEntry.m_OrIndex = orIndex;
    dpcCfgEntry.m_PadLinksMask = padLinksMask;
    dpcCfgEntry.m_Heads = headMask;
    dpcCfgEntries.push_back(dpcCfgEntry);
}

TEST_F(DpcConfigValidatorMst, WhenSameSorIsUsedForMstAndNonMst)
{
    SorExcludeMask sorExcludeMask = 0x0;
    constexpr INT32 orIndex = 0x0;
    const LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;

    AddDpcCfgEntrySor(CrcHandler::CRCProtocol::CRC_DP_SST, orIndex);
    AddDpcCfgEntrySor(CrcHandler::CRCProtocol::CRC_DP_MST, orIndex);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &sorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorMst, WhenDiffSorIsUsedForMstAndNonMst)
{
    SorExcludeMask sorExcludeMask = 0x0;
    constexpr INT32 orIndexForCfg1 = 0x0;
    constexpr INT32 orIndexForCfg2 = 0x1;
    const LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;

    AddDpcCfgEntrySor(CrcHandler::CRCProtocol::CRC_DP_SST, orIndexForCfg1);
    AddDpcCfgEntrySor(CrcHandler::CRCProtocol::CRC_DP_MST, orIndexForCfg2);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0x3);
}

TEST_F(DpcConfigValidatorMst, WhenSameSorIsUsedForMst)
{
    SorExcludeMask sorExcludeMask = 0x0;
    constexpr INT32 orIndex = 0x1;
    constexpr PadLinksMask padLinksMask = 1<<('D' - 'A');
    const LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;

    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndex, padLinksMask);
    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndex, padLinksMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0x2);
}

TEST_F(DpcConfigValidatorMst, WhenSorIsSpecifiedForMultipleMst)
{
    SorExcludeMask sorExcludeMask = 0x0;
    constexpr INT32 orIndex = 0x1;
    constexpr INT32 orIndexForCfg2 = 0x0;
    constexpr PadLinksMask padLinksMask = 1<<('D' - 'A');
    constexpr PadLinksMask padLinksMaskForCfg2 = 1<<('C' - 'A');
    const LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;

    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndex, padLinksMask);
    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndexForCfg2, padLinksMaskForCfg2);
    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndex, padLinksMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 0x3);
}

TEST_F(DpcConfigValidatorMst, WhenSamePadlinkIsUsedForMstAndNonMst)
{
    constexpr PadLinksMask padLinksMask = 1<<('D' - 'A');
    Padlink2MstPanelCntMap padlink2MstPanelCntMap;

    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_SST, padLinksMask);
    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, padLinksMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingPadlinks(dpcCfgEntries,
        &padlink2MstPanelCntMap), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorMst, WhenDiffPadlinkIsUsedForMstAndNonMst)
{
    constexpr PadLinksMask padLinksMaskForCfg1 = 1<<('D' - 'A');
    constexpr PadLinksMask padLinksMaskForCfg2 = 1<<('C' - 'A');
    Padlink2MstPanelCntMap padlink2MstPanelCntMap;

    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_SST, padLinksMaskForCfg1);
    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, padLinksMaskForCfg2);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingPadlinks(dpcCfgEntries,
        &padlink2MstPanelCntMap), RC::OK);
    ASSERT_THAT(padlink2MstPanelCntMap.size(), 0x1);
}

TEST_F(DpcConfigValidatorMst, WhenSamePadlinkIsUsedForMst)
{ 
    constexpr PadLinksMask padLinksMask = 1<<('A' - 'A');
    Padlink2MstPanelCntMap padlink2MstPanelCntMap;

    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, padLinksMask);
    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, padLinksMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingPadlinks(dpcCfgEntries,
        &padlink2MstPanelCntMap), RC::OK);
    ASSERT_THAT(padlink2MstPanelCntMap.size(), 0x1);
}

TEST_F(DpcConfigValidatorMst, WhenPadlinkIsSpecifiedForMultipleMst)
{
    constexpr PadLinksMask padLinksMask = 1<<('A' - 'A');
    Padlink2MstPanelCntMap padlink2MstPanelCntMap;

    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, padLinksMask);
    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, m_UnspecifiedPadlinksMask);
    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, padLinksMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingPadlinks(dpcCfgEntries,
        &padlink2MstPanelCntMap), RC::OK);
    ASSERT_THAT(padlink2MstPanelCntMap.size(), 0x2);
}

void DpcConfigValidatorMst::AddDummyEntryInConnDescription(const DisplayID& displayId)
{
    dpcConfigValidator.m_ConnectorsDescription.insert({displayId, {}});
}

void DpcConfigValidatorMst::BuildConnDescription()
{
    auto pushConnectorsDescriptionEntries = [&]
    (
        const DisplayID& displayId,
        const PadLinksMask padLinksMask,
        const UINT32 protocols
    )
    {
        dpcConfigValidator.m_ConnectorsDescription.insert({displayId, {padLinksMask, {protocols}}});
    };
    pushConnectorsDescriptionEntries(0x100, 1<<('C' - 'A'), LW887D_SOR_SET_CONTROL_PROTOCOL_DP_B);
    pushConnectorsDescriptionEntries(0x200, 1<<('D' - 'A'), LW887D_SOR_SET_CONTROL_PROTOCOL_DP_B);
    pushConnectorsDescriptionEntries(0x400, 1<<('D' - 'A'),
                                     LW507D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_B);
    pushConnectorsDescriptionEntries(0x800, 1<<('E' - 'A'), LW887D_SOR_SET_CONTROL_PROTOCOL_DP_A);
    pushConnectorsDescriptionEntries(0x1000, 1<<('E' - 'A'),
                                     LW507D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_A);
    pushConnectorsDescriptionEntries(0x2000, 1<<('F' - 'A'), LW887D_SOR_SET_CONTROL_PROTOCOL_DP_B);
    pushConnectorsDescriptionEntries(0x4000, 1<<('F' - 'A'),
                                     LW507D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_B);
}

void DpcConfigValidatorMst::AddDummyEntryInDynamic2RootDisplayIDMap
(
    const DisplayID& dynamicDisplayId,
    const DisplayID& rootDisplayId
)
{
    dpcConfigValidator.m_Dynamic2RootDisplayIDMap.insert({dynamicDisplayId, rootDisplayId});
}

TEST_F(DpcConfigValidatorMst, WithUnspecifiedPadlink_SuitablePadlinkGetsAssigned)
{
    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, m_UnspecifiedPadlinksMask);
    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, m_UnspecifiedPadlinksMask);

    EXPECT_CALL(lwDisplayMock, GetDetected(_)).Times(2).WillRepeatedly(Return(RC::OK));
    dpcConfigValidator.m_pLwDisplay = &lwDisplayMock;

    dpcConfigValidator.m_AvailableDisplayIDs = {0x100, 0x200, 0x8000, 0x10000};

    BuildConnDescription();
    AddDummyEntryInConnDescription(0x8000);
    AddDummyEntryInConnDescription(0x10000);

    AddDummyEntryInDynamic2RootDisplayIDMap(0x8000, 0x0);
    AddDummyEntryInDynamic2RootDisplayIDMap(0x10000, 0x0);

    dpcConfigValidator.m_SpecifiedPadLinks = {0x0};

    ASSERT_THAT(dpcConfigValidator.DetermineDisplayIDsForEachLoop(0, &dpcCfgEntries), RC::OK);

    ASSERT_THAT(static_cast<INT32>(dpcCfgEntries[0].m_PadLinksMask), 1<<('C' - 'A'));
    ASSERT_THAT(static_cast<INT32>(dpcCfgEntries[1].m_PadLinksMask), 1<<('D' - 'A'));
}

TEST_F(DpcConfigValidatorMst, WithUnspecifiedPadlinkAndSameSorUsed_ShouldFail)
{
    SorExcludeMask sorExcludeMask = 0x0;
    constexpr INT32 orIndex = 0x2;
    const LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;

    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndex, m_UnspecifiedPadlinksMask);
    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndex, m_UnspecifiedPadlinksMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &sorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorMst, WithUnspecifiedPadlinkAndDifferentSorsUsed_ShouldPass)
{
    SorExcludeMask sorExcludeMask = 0x0;
    constexpr INT32 orIndexForCfg1 = 0x1;
    constexpr INT32 orIndexForCfg2 = 0x2;
    const LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;

    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndexForCfg1, m_UnspecifiedPadlinksMask);
    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndexForCfg2, m_UnspecifiedPadlinksMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 1<<1 | 1<<2);
}

TEST_F(DpcConfigValidatorMst,
       WithOneUnspecifiedPadlinkAndOneSpecifiedPadlink_SameSorUsed_ShouldFail)
{
    SorExcludeMask sorExcludeMask = 0x0;
    constexpr INT32 orIndex = 0x2;
    constexpr PadLinksMask padLinksMaskForCfg2 = 1<<('D' - 'A');
    const LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;

    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndex, m_UnspecifiedPadlinksMask);
    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndex, padLinksMaskForCfg2);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &sorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorMst,
       WithOneUnspecifiedPadlinkAndOneSpecifiedPadlink_DiffSorsUsed_ShouldPass)
{
    SorExcludeMask sorExcludeMask = 0x0;
    constexpr INT32 orIndexForCfg1 = 0x1;
    constexpr INT32 orIndexForCfg2 = 0x2;
    constexpr PadLinksMask padLinksMaskForCfg2 = 1<<('D' - 'A');
    const LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;

    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndexForCfg1, m_UnspecifiedPadlinksMask);
    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndexForCfg2, padLinksMaskForCfg2);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &sorExcludeMask), RC::OK);
    ASSERT_THAT(sorExcludeMask, 1<<1 | 1<<2);
}

TEST_F(DpcConfigValidatorMst, WhenDummyDynamicIdsDontMatchWithSimulatedIds_UpdateDummyDynamicIds)
{
    constexpr INT32 orIndexForCfg1 = 0x1;
    constexpr INT32 orIndexForCfg2 = 0x0;
    constexpr PadLinksMask padLinksMaskForCfg1 = 1<<('D' - 'A');
    constexpr HeadMask headMaskForCfg1 = 0x1;
    constexpr HeadMask headMaskForCfg2 = 0x4;
    AddDpcCfgEntryEx(CrcHandler::CRCProtocol::CRC_DP_MST, orIndexForCfg1,
                     padLinksMaskForCfg1, headMaskForCfg1);
    AddDpcCfgEntryEx(CrcHandler::CRCProtocol::CRC_DP_MST, orIndexForCfg2,
                     m_UnspecifiedPadlinksMask, headMaskForCfg2);

    auto AddAssumedEntryInDynamicIdMap = [&]
    (
        const DisplayID& dynamicDisplayId,
        const HeadMask headMask
    )
    {
        dynamicId2HeadmaskMap.insert({dynamicDisplayId, headMask});
    };
    AddAssumedEntryInDynamicIdMap(0x10000, 0x1);
    AddAssumedEntryInDynamicIdMap(0x8000, 0x4);

    map <DisplayIDs, UINT32> mstDisplayIDs2simulatedDisplayIDMask;
    auto AddRMSimulatedEntryInDynamicIdMap = [&]
    (
        const DisplayID& dynamicDisplayId,
        const UINT32 simulatedDisplayIDMask
    )
    {
        mstDisplayIDs2simulatedDisplayIDMask.insert({DisplayIDs(1, dynamicDisplayId),
                                                    simulatedDisplayIDMask});
    };
    AddRMSimulatedEntryInDynamicIdMap(0x10000, 0x8000);
    AddRMSimulatedEntryInDynamicIdMap(0x8000, 0x10000);

    for (const auto& item: mstDisplayIDs2simulatedDisplayIDMask)
    {
        ASSERT_THAT(pPimpl->UpdateDummyDynamicIdsWithSimulatedIds(&dpcCfgEntries, item.first,
            dynamicId2HeadmaskMap, item.second), RC::OK);
    }

    ASSERT_THAT(dpcCfgEntries[0].m_DisplayIDs[0], 0x8000);
    ASSERT_THAT(dpcCfgEntries[1].m_DisplayIDs[0], 0x10000);
}

TEST_F(DpcConfigValidatorMst, WhenDiffSorsAreUsedForChainedMst)
{
    SorExcludeMask sorExcludeMask = 0x0;
    constexpr INT32 orIndexForCfg1 = 0x1;
    constexpr INT32 orIndexForCfg2 = 0x2;
    constexpr PadLinksMask padLinksMask = 1<<('D' - 'A');
    const LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;

    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndexForCfg1, padLinksMask);
    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndexForCfg2, padLinksMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &sorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorMst, UpdateCfgsHavingUninitializedSorInMstChain)
{
    constexpr INT32 orIndexForCfg2 = 0x1;
    constexpr PadLinksMask padLinksMask = 1<<('D' - 'A');

    AddDpcCfgEntryPadlink(CrcHandler::CRCProtocol::CRC_DP_MST, padLinksMask);
    AddDpcCfgEntrySorAndPadlink(CrcHandler::CRCProtocol::CRC_DP_MST,
        orIndexForCfg2, padLinksMask);
    padlinkToOrIndex.insert({padLinksMask, orIndexForCfg2});

    ASSERT_THAT(dpcConfigValidator.UpdateOrIndicesInMst(padlinkToOrIndex, &dpcCfgEntries), RC::OK);
    // Check whether the first cfg has been assigned an appropriate SOR index
    ASSERT_THAT(dpcCfgEntries[0].m_OrIndex, orIndexForCfg2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////////
class DpcConfigValidatorEDP : public DpcConfigValidatorMst
{
public:
    void AddDpcCfgEntry
    (
        CrcHandler::CRCProtocol protocol,
        PadLinksMask padLinksMask,
        INT32 orIndex,
        HeadMask heads
    )
    {
        DPCCfgEntry dpcCfgEntry;
        dpcCfgEntry.m_Protocol = protocol;
        dpcCfgEntry.m_PadLinksMask = padLinksMask;
        dpcCfgEntry.m_OrIndex = orIndex;
        dpcCfgEntry.m_Heads = heads;
        dpcCfgEntries.push_back(dpcCfgEntry);
    }

    const UINT08 m_PadlinkLetter = 'C';
    const UINT32 m_ReservedPadlink = m_PadlinkLetter - 'A';
    const UINT32 m_ReservedSorIndex = 0;
    SorExcludeMask m_SorExcludeMask = 0x0;
    PadlinkToOrIndex padlinkToOrIndex;
    void UpdateReservedEdpMap_PadlinkC_Sor0()
    {
        dpcConfigValidator.m_ReservedSORInfoMap[1<<m_ReservedPadlink] = m_ReservedSorIndex;
    }

    void SetUp()
    {
        UpdateReservedEdpMap_PadlinkC_Sor0();
    }
};

TEST_F(DpcConfigValidatorEDP, WithReservedSor_TMDS_PadlinkSpecified_ShouldFail)
{
    constexpr UINT32 nonEdpPadlink = 'D' - 'A';
    constexpr HeadMask singleHeadMask = 1;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_TMDS_A,
        1 << nonEdpPadlink, m_ReservedSorIndex, singleHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorEDP, WithReservedSor_TMDS_PadlinkNotSpecified_ShouldFail)
{
    constexpr HeadMask singleHeadMask = 1;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_TMDS_A, 0, m_ReservedSorIndex, singleHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorEDP, WithReservedSor_DpSst_PadlinkSpecifiedIsNonEdp_ShouldFail)
{
    constexpr UINT32 nonEdpPadlink = 'D' - 'A';
    constexpr HeadMask singleHeadMask = 1;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_DP_SST,
        1 << nonEdpPadlink, m_ReservedSorIndex, singleHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorEDP, WithReservedSor_DpSst_PadlinkNotSpecified_ShouldPickEdp)
{
    constexpr HeadMask singleHeadMask = 1;
    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_DP_SST, m_UnspecifiedPadlinksMask,
                   m_ReservedSorIndex, singleHeadMask);

    EXPECT_CALL(lwDisplayMock, GetDetected(_)).Times(1).WillOnce(Return(RC::OK));
    dpcConfigValidator.m_pLwDisplay = &lwDisplayMock;

    dpcConfigValidator.m_AvailableDisplayIDs = {0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000};
    dpcConfigValidator.m_SpecifiedPadLinks = {0x0};

    BuildConnDescription();

    ASSERT_THAT(dpcConfigValidator.DetermineDisplayIDsForEachLoop(0, &dpcCfgEntries), RC::OK);
    ASSERT_THAT(dpcCfgEntries[0].m_PadLinksMask, 1 << m_ReservedPadlink);
}

TEST_F(DpcConfigValidatorEDP, WithReservedSor_DpSstDualHead_PadlinkSpecifiedIsNonEdp_ShouldFail)
{
    constexpr UINT32 nonEdpPadlink = 'D' - 'A';
    constexpr HeadMask dualHeadMask = 3;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_DP_SST,
        1 << nonEdpPadlink, m_ReservedSorIndex, dualHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorEDP, WithReservedSor_DpSstDualHead_PadlinkNotSpecified_ShouldPickEdp)
{
    constexpr HeadMask dualHeadMask = 12;
    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_DP_SST, m_UnspecifiedPadlinksMask,
                   m_ReservedSorIndex, dualHeadMask);

    EXPECT_CALL(lwDisplayMock, GetDetected(_)).Times(1).WillOnce(Return(RC::OK));
    dpcConfigValidator.m_pLwDisplay = &lwDisplayMock;

    dpcConfigValidator.m_AvailableDisplayIDs = {0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000};
    dpcConfigValidator.m_SpecifiedPadLinks = {0x0};

    BuildConnDescription();

    ASSERT_THAT(dpcConfigValidator.DetermineDisplayIDsForEachLoop(0, &dpcCfgEntries), RC::OK);
    ASSERT_THAT(dpcCfgEntries[0].m_PadLinksMask, 1 << m_ReservedPadlink);
}

TEST_F(DpcConfigValidatorEDP, WithReservedPadlink_TMDS_SorSpecifiedReserved_ShouldFail)
{
    constexpr HeadMask singleHeadMask = 1;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_TMDS_A, 1 << m_ReservedPadlink,
        m_ReservedSorIndex, singleHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorEDP, WithReservedPadlink_TMDS_SorSpecifiedNonReserved_ShouldPass)
{
    constexpr UINT32 nonReservedSor = 1;
    constexpr HeadMask singleHeadMask = 1;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_TMDS_A, 1 << m_ReservedPadlink,
        nonReservedSor, singleHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::OK);
}

TEST_F(DpcConfigValidatorEDP, WithReservedPadlink_DpSst_SorSpecifiedReserved_ShouldPass)
{
    constexpr HeadMask singleHeadMask = 1;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_DP_SST, 1 << m_ReservedPadlink,
        m_ReservedSorIndex, singleHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::OK);
}

TEST_F(DpcConfigValidatorEDP, WithReservedPadlink_DpSst_SorSpecifiedNonReserved_ShouldFail)
{
    constexpr UINT32 nonReservedSor = 1;
    constexpr HeadMask singleHeadMask = 1;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_DP_SST, 1 << m_ReservedPadlink,
        nonReservedSor, singleHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::SOFTWARE_ERROR);
}

TEST_F(DpcConfigValidatorEDP, WithReservedPadlink_DpSstDualHead_SorSpecifiedReserved_ShouldPass)
{
    constexpr HeadMask dualHeadMask = 3;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_DP_SST, 1 << m_ReservedPadlink,
        m_ReservedSorIndex, dualHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::OK);
}

TEST_F(DpcConfigValidatorEDP, WithReservedPadlink_DpSstDualHead_SorSpecifiedNonReserved_ShouldFail)
{
    constexpr UINT32 nonReservedSor = 1;
    constexpr HeadMask dualHeadMask = 3;

    AddDpcCfgEntry(CrcHandler::CRCProtocol::CRC_DP_SST, 1 << m_ReservedPadlink,
        nonReservedSor, dualHeadMask);

    ASSERT_THAT(dpcConfigValidator.CheckForConflictingOR(
        dpcCfgEntries, &padlinkToOrIndex, &m_SorExcludeMask), RC::SOFTWARE_ERROR);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////////
class DpcConfigValidatorRasterLockHeads : public Test
{
public:
    using HeadsMaskVector = vector<UINT32>;
    LwDispUtils::DCBInfoHelper::ReservedSORInfoMap reservedSORInfoMap;
    DpcConfigValidator m_DpcConfigValidator;
    RasterLockHeadsMap m_RasterLockHeadsMap;

    DpcConfigValidatorRasterLockHeads()
    : m_DpcConfigValidator(static_cast<INT32>(Tee::PriNone),
          false, false, nullptr, reservedSORInfoMap, 0x0)
    {

    }
    void AddHeadMasksToDpcCfg(const HeadsMaskVector& headsVector); 
    void AddHeadsToRasterLock(HeadIndex primaryHead, const HeadsSet& secondaryHeadSet);
};

void DpcConfigValidatorRasterLockHeads::AddHeadMasksToDpcCfg(const HeadsMaskVector& headsVector)
{
    DPCCfgEntries dpcCfgEntries;

    for (const auto& heads : headsVector)
    {
        DPCCfgEntry dpcCfgEntry;
        dpcCfgEntry.m_Heads = heads;
        dpcCfgEntries.push_back(dpcCfgEntry);
    }

    m_DpcConfigValidator.m_DPCCfgEntriesList.push_back(dpcCfgEntries);
}

void DpcConfigValidatorRasterLockHeads::AddHeadsToRasterLock
(
    HeadIndex primaryHead,
    const HeadsSet& secondaryHeadSet
)
{
    m_RasterLockHeadsMap[primaryHead] = secondaryHeadSet;
}

TEST_F(DpcConfigValidatorRasterLockHeads, ValidRasterLockHeadIsEmpty)
{
    RasterLockHeadsMap m_RasterLockHeadsMap;
    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::OK);
}

TEST_F(DpcConfigValidatorRasterLockHeads, IlwalidRasterLockHeadNotEmptyAndLoops)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0x1});
    AddHeadMasksToDpcCfg({0x2});

    AddHeadsToRasterLock(0, {1});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::ILWALID_TEST_INPUT);
}

TEST_F(DpcConfigValidatorRasterLockHeads, IlwalidRasterLockPrimaryHeadMissingInCfg)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0x2});

    AddHeadsToRasterLock(0, {1});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::ILWALID_TEST_INPUT);
}

TEST_F(DpcConfigValidatorRasterLockHeads, IlwalidRasterLockSecHeadMissingInCfg)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0x1});

    AddHeadsToRasterLock(0, {1});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::ILWALID_TEST_INPUT);
}

TEST_F(DpcConfigValidatorRasterLockHeads, Ilwalid2Head1OrPairLockedOnDiffPrimaryHeads)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();
    AddHeadMasksToDpcCfg({0xC, 0x1, 0x2});

    AddHeadsToRasterLock(0, {2});
    AddHeadsToRasterLock(1, {3});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::ILWALID_TEST_INPUT);
}

TEST_F(DpcConfigValidatorRasterLockHeads, Ilwalid2Head1OrSecHeadMissingInRasterLockHeads1)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0xC, 0x1});

    AddHeadsToRasterLock(0, {2});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::ILWALID_TEST_INPUT);
}

TEST_F(DpcConfigValidatorRasterLockHeads, Ilwalid2Head1OrSecHeadMissingInRasterLockHeads2)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0xC, 0x1});

    AddHeadsToRasterLock(2, {0});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::ILWALID_TEST_INPUT);
}

TEST_F(DpcConfigValidatorRasterLockHeads, Ilwalid2Head1OrPriHeadMissingInRasterLockHeads)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0xC, 0x1});

    AddHeadsToRasterLock(0, {3});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::ILWALID_TEST_INPUT);
}

TEST_F(DpcConfigValidatorRasterLockHeads, IlwalidRasterLockedOn2Head1OrSecHead)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0xC, 0x1});

    AddHeadsToRasterLock(3, {0});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::ILWALID_TEST_INPUT);
}

TEST_F(DpcConfigValidatorRasterLockHeads, ValidRasterLockSingleHeads)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0x1, 0x2});

    AddHeadsToRasterLock(0, {1});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::OK);
}

TEST_F(DpcConfigValidatorRasterLockHeads, ValidRasterLock2Head1OrOnSingleHead)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0xC, 0x1});

    AddHeadsToRasterLock(0, {2, 3});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::OK);
}

TEST_F(DpcConfigValidatorRasterLockHeads, ValidRasterLockSingleHeadOn2Head1Or)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0xC, 0x1});

    AddHeadsToRasterLock(2, {0, 3});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::OK);
}

TEST_F(DpcConfigValidatorRasterLockHeads, ValidRasterLock2Head1OrOn2Head1Or)
{
    m_DpcConfigValidator.m_DPCCfgEntriesList.clear();

    AddHeadMasksToDpcCfg({0xC, 0x3});

    AddHeadsToRasterLock(2, {0, 1, 3});

    ASSERT_THAT(m_DpcConfigValidator.CheckForConflictingRasterLockHeads(m_RasterLockHeadsMap),
        RC::OK);
}
