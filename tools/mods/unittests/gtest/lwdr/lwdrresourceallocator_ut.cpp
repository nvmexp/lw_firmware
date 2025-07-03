/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwdisplayrandomv2/lwdrutils.h"

#include "gmock/gmock.h"
using namespace testing;

#include <memory>

TEST(Basic, DisallowsProtocolIfNotPossible)
{
    ProtocolToDisplayIDsMap availableDisplayIDs;
    DisplayIDToPadlinkMaskMap availablePadLinks;
    SorMask availableSorMask = 0xF;
    EmbeddedDpInfoMap embeddedDpInfoMap;

    LwdrResourceAllocator lwdrRM(availableDisplayIDs, availablePadLinks,
        availableSorMask, embeddedDpInfoMap);

    ASSERT_THAT(lwdrRM.IsProtocolPossible(CrcHandler::CRC_TMDS_A), false);
}

class LwdrRATester : public Test
{
public:
    void BasicSetupWithoutHeads()
    {
        ProtocolToDisplayIDsMap availableDisplayIDs;
        DisplayIDs displayIDs;
        displayIDs.push_back(0x100);
        displayIDs.push_back(0x200);
        availableDisplayIDs[CrcHandler::CRC_TMDS_A] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_DP_SST] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_DP_DUAL_SST] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_DP_MST] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR] = displayIDs;

        DisplayIDToPadlinkMaskMap availablePadLinks;
        availablePadLinks[0x100] = 1 << 0;
        availablePadLinks[0x200] = 1 << 1;

        SorMask availableSorMask = 0xF;

        lwdrRaTester = std::make_unique<LwdrResourceAllocator>(availableDisplayIDs,
            availablePadLinks, availableSorMask, embeddedDpInfoMap);
    }

    virtual void SetUp()
    {
        BasicSetupWithoutHeads();

        Protocol2HeadMaskMap availableHeads;
        availableHeads[CrcHandler::CRC_TMDS_A] = 1 << 0 | 1 << 1;
        availableHeads[CrcHandler::CRC_DP_SST] = 0xF;;
        availableHeads[CrcHandler::CRC_DP_DUAL_SST] = 0xF;
        availableHeads[CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR] = 0xF;
        availableHeads[CrcHandler::CRC_DP_MST] = 0xF;

        lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);
    }

    void ResetWithSingleSharedPadlink()
    {
        ProtocolToDisplayIDsMap availableDisplayIDs;
        DisplayIDs displayIDs;
        displayIDs.push_back(0x100);
        displayIDs.push_back(0x200);
        availableDisplayIDs[CrcHandler::CRC_TMDS_A] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_DP_DUAL_SST] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_DP_SST] = displayIDs;

        DisplayIDToPadlinkMaskMap availablePadLinks;
        availablePadLinks[0x100] = 1 << 0;
        availablePadLinks[0x200] = 1 << 0;

        SorMask availableSorMask = 0xF;

        lwdrRaTester = std::make_unique<LwdrResourceAllocator>(availableDisplayIDs,
            availablePadLinks, availableSorMask, embeddedDpInfoMap);

        Protocol2HeadMaskMap availableHeads;
        availableHeads[CrcHandler::CRC_TMDS_A] = 1 << 0 | 1 << 1;
        availableHeads[CrcHandler::CRC_DP_DUAL_SST] = 0xF;

        lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);
    }

    void ResetWithSingleSor()
    {
        ProtocolToDisplayIDsMap availableDisplayIDs;
        DisplayIDs displayIDs;
        displayIDs.push_back(0x100);
        displayIDs.push_back(0x200);
        availableDisplayIDs[CrcHandler::CRC_TMDS_A] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_DP_DUAL_SST] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_DP_SST] = displayIDs;

        DisplayIDToPadlinkMaskMap availablePadLinks;
        availablePadLinks[0x100] = 1 << 0;
        availablePadLinks[0x200] = 1 << 0;

        SorMask availableSorMask = 0x1;

        lwdrRaTester = std::make_unique<LwdrResourceAllocator>(availableDisplayIDs,
           availablePadLinks, availableSorMask, embeddedDpInfoMap);

        Protocol2HeadMaskMap availableHeads;
        availableHeads[CrcHandler::CRC_TMDS_A] = 1 << 0 | 1 << 1;
        availableHeads[CrcHandler::CRC_DP_DUAL_SST] = 0xF;

        lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);
    }

    std::unique_ptr<LwdrResourceAllocator> lwdrRaTester;
    EmbeddedDpInfoMap embeddedDpInfoMap;
};

TEST_F(LwdrRATester, AllowsTMDSAIfPossible)
{
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), true);
}

TEST_F(LwdrRATester, DisallowsTMDSAIfAllDisplayIDsUsed)
{
    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMask;
    SorMask pickedSorMask;

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_TMDS_A,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_TMDS_A,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), false);
}

TEST_F(LwdrRATester, PicksTwoDisplayIDsForDPDualSST)
{
    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMask;
    SorMask pickedSorMask;

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_DUAL_SST), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_DUAL_SST,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);
    ASSERT_THAT(pickedDisplayIDs.size(), 2);
}

TEST_F(LwdrRATester, DisallowsDPDualSSTIfAtLeast2DisplayIDNotAvailable)
{
    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMask;
    SorMask pickedSorMask;

    // Allocate the default available DisplayIDs
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_DUAL_SST), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_DUAL_SST,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_DUAL_SST), false);
}

TEST_F(LwdrRATester, DisallowsDPDualSSTIfAtLeast2PadlinksNotAvailable)
{
    ResetWithSingleSharedPadlink();

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_DUAL_SST), false);
}

TEST_F(LwdrRATester, DisallowsAllProtocolsIfNoMatchingHeadsAvailable)
{
    BasicSetupWithoutHeads();

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), false);
}

TEST_F(LwdrRATester, DisallowsProtocolIfNoMatchingHeadsAvailable)
{
    BasicSetupWithoutHeads();

    Protocol2HeadMaskMap availableHeads;
    availableHeads[CrcHandler::CRC_TMDS_A] = 1 << 0;

    lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), true);

    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMask;
    SorMask pickedSorMask;

    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_TMDS_A,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), false);
}

TEST_F(LwdrRATester, Disallows2HeadsIfPairHeadsNotAvailable)
{
    BasicSetupWithoutHeads();

    Protocol2HeadMaskMap availableHeads;
    availableHeads[CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR] = 1 << 0 | 1 << 2;
    lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR), false);
}

TEST_F(LwdrRATester, Picks2HeadsFor2H1ORTwice)
{
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR), true);

    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMask;
    SorMask pickedSorMask;

    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);
    ASSERT_THAT(Utility::CountBits(pickedHeadMask), 2);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);
    ASSERT_THAT(Utility::CountBits(pickedHeadMask), 2);
}

TEST_F(LwdrRATester, PicksHead2And3HeadsFor2H1ORIfHead0or1IsUsed)
{
    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMask;
    SorMask pickedSorMask;

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_TMDS_A,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);
    ASSERT_THAT(pickedHeadMask, 1 << 2 | 1 << 3);
}

TEST_F(LwdrRATester, DisallowsProtocolIfNoDisplayIDConsideringOtherProtocols)
{
    DisplayIDs pickedDisplayIDsDpSST;
    HeadMask pickedHeadMaskDpSST;
    SorMask pickedSorMask;
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_SST), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_SST,
        &pickedDisplayIDsDpSST, &pickedHeadMaskDpSST, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_MST), true);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_SST), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_SST,
        &pickedDisplayIDsDpSST, &pickedHeadMaskDpSST, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_MST), false);
}

TEST_F(LwdrRATester, DisallowsProtocolIfNoHeadConsideringOtherProtocols)
{
    BasicSetupWithoutHeads();

    Protocol2HeadMaskMap availableHeads;
    availableHeads[CrcHandler::CRC_DP_SST] = 1 << 0;
    availableHeads[CrcHandler::CRC_DP_MST] = 1 << 0;
    lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);

    DisplayIDs pickedDisplayIDsDpSST;
    HeadMask pickedHeadMaskDpSST;
    SorMask pickedSorMask;

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_MST), true);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_SST), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_SST,
        &pickedDisplayIDsDpSST, &pickedHeadMaskDpSST, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_MST), false);
}

TEST_F(LwdrRATester, PicksTheUnunsedDisplayIDConsideringOtherProtocols)
{
    SorMask pickedSorMask;
    DisplayIDs pickedDisplayIDsDpSST;
    HeadMask pickedHeadMaskDpSST;
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_SST), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_SST,
        &pickedDisplayIDsDpSST, &pickedHeadMaskDpSST, &pickedSorMask), RC::OK);

    DisplayIDs pickedDisplayIDsDpMST;
    HeadMask pickedHeadMaskDpMST;
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_MST), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_MST,
        &pickedDisplayIDsDpMST, &pickedHeadMaskDpMST, &pickedSorMask), RC::OK);

    ASSERT_NE(pickedHeadMaskDpSST, pickedHeadMaskDpMST);
    ASSERT_NE(pickedDisplayIDsDpSST, pickedDisplayIDsDpMST);
}

TEST_F(LwdrRATester, DisallowsProtocolIfPadlinksUsed)
{
    ResetWithSingleSharedPadlink();

    Protocol2HeadMaskMap availableHeads;
    availableHeads[CrcHandler::CRC_DP_SST] = 1 << 0;
    availableHeads[CrcHandler::CRC_TMDS_A] = 1 << 0;
    lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);

    DisplayIDs pickedDisplayIDsDpSST;
    HeadMask pickedHeadMaskDpSST;
    SorMask pickedSorMask;
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_SST), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_SST,
        &pickedDisplayIDsDpSST, &pickedHeadMaskDpSST, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), false);
}

TEST_F(LwdrRATester, DisallowsProtocolIfSorNotAvailable)
{
    ResetWithSingleSor();

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), true);
    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMask;
    SorMask pickedSorMask;
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_TMDS_A,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), false);
}

class LwdrRAEdpTester : public Test
{
public:
    void BasicSetupWithoutHeads(UINT32 sorMask)
    {
        ProtocolToDisplayIDsMap availableDisplayIDs;
        DisplayIDs displayIDs;
        displayIDs.push_back(0x200);
        availableDisplayIDs[CrcHandler::CRC_DP_SST] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_TMDS_A] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR] = displayIDs;

        DisplayIDToPadlinkMaskMap availablePadLinks;
        availablePadLinks[0x200] = 1 << 1;

        SorMask availableSorMask = sorMask;

        embeddedDpInfoMap[0x200] =
            std::make_pair(Display::Encoder::LinkLetter::LINK_A, edpSorIndex);

        lwdrRaTester = std::make_unique<LwdrResourceAllocator>(availableDisplayIDs,
            availablePadLinks, availableSorMask, embeddedDpInfoMap);
    }

    virtual void SetUp()
    {
        BasicSetupWithoutHeads(0xF);

        Protocol2HeadMaskMap availableHeads;
        availableHeads[CrcHandler::CRC_DP_SST] = 0xF;
        availableHeads[CrcHandler::CRC_TMDS_A] = 0xF;
        availableHeads[CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR] = 0xF;

        lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);
    }

    std::unique_ptr<LwdrResourceAllocator> lwdrRaTester;
    EmbeddedDpInfoMap embeddedDpInfoMap;
    const UINT32 edpSorIndex = 2;
};

TEST_F(LwdrRAEdpTester, Disallows2H1OrIfOnlyEDpDisplayIDAvailable)
{
    // This should not be required as DisplayIDs belonging to EDp
    // should not come in m_AvailableDisplayIDS
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR), false);
}

TEST_F(LwdrRAEdpTester, PicksEDpSorForEDpDisplayID)
{
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_SST), true);
    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMask;
    SorMask pickedSorMask;
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_SST,
        &pickedDisplayIDs, &pickedHeadMask, &pickedSorMask), RC::OK);

    ASSERT_THAT(pickedSorMask, 1 << edpSorIndex);
}

TEST_F(LwdrRAEdpTester, DisallowsAnyProtocolIfOnlyEDpSorAvailable)
{
    BasicSetupWithoutHeads(1 << edpSorIndex);

    Protocol2HeadMaskMap availableHeads;
    availableHeads[CrcHandler::CRC_DP_SST] = 0xF;
    availableHeads[CrcHandler::CRC_TMDS_A] = 0xF;
    availableHeads[CrcHandler::CRC_DP_DUAL_HEAD_ONE_OR] = 0xF;

    lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), false);
}

class LwdrRAMstTester : public Test
{
public:
    void BasicSetupWithoutHeads(UINT32 sorMask)
    {
        ProtocolToDisplayIDsMap availableDisplayIDs;
        DisplayIDs displayIDs;
        displayIDs.push_back(0x200);
        displayIDs.push_back(0x100);
        availableDisplayIDs[CrcHandler::CRC_DP_MST] = displayIDs;
        availableDisplayIDs[CrcHandler::CRC_TMDS_A] = displayIDs;

        DisplayIDToPadlinkMaskMap availablePadLinks;
        availablePadLinks[0x200] = 1 << 0;
        availablePadLinks[0x100] = 1 << 1;

        SorMask availableSorMask = sorMask;

        lwdrRaTester = std::make_unique<LwdrResourceAllocator>(availableDisplayIDs,
            availablePadLinks, availableSorMask, embeddedDpInfoMap);
    }

    virtual void SetUp()
    {
        BasicSetupWithoutHeads(0xF);

        Protocol2HeadMaskMap availableHeads;
        availableHeads[CrcHandler::CRC_DP_MST] = 0xF;
        availableHeads[CrcHandler::CRC_TMDS_A] = 0xF;

        lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);
    }

    std::unique_ptr<LwdrResourceAllocator> lwdrRaTester;
    EmbeddedDpInfoMap embeddedDpInfoMap;
};

TEST_F(LwdrRAMstTester, PicksAllRemainingHeadsForMst)
{
    Protocol2HeadMaskMap availableHeads;
    availableHeads[CrcHandler::CRC_DP_MST] = 0xF;
    availableHeads[CrcHandler::CRC_TMDS_A] = 0xF;

    lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), true);
    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMaskTmds;
    SorMask pickedSorMask;
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_TMDS_A,
        &pickedDisplayIDs, &pickedHeadMaskTmds, &pickedSorMask), RC::OK);

    HeadMask pickedHeadMaskMst;
    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_DP_MST), true);
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_DP_MST,
        &pickedDisplayIDs, &pickedHeadMaskMst, &pickedSorMask), RC::OK);
    ASSERT_THAT(pickedHeadMaskMst|pickedHeadMaskTmds, 0xF);
}

TEST(LwdrAllocationTester, DisplayIDsExhausted)
{
    ProtocolToDisplayIDsMap availableDisplayIDs;
    DisplayIDs displayIDs;
    displayIDs.push_back(0x200);
    availableDisplayIDs[CrcHandler::CRC_TMDS_A] = displayIDs;

    DisplayIDToPadlinkMaskMap availablePadLinks;
    availablePadLinks[0x200] = 1 << 0;

    SorMask availableSorMask = 0xF;

    EmbeddedDpInfoMap embeddedDpInfoMap;

    std::unique_ptr<LwdrResourceAllocator> lwdrRaTester = std::make_unique<LwdrResourceAllocator>(
       availableDisplayIDs, availablePadLinks, availableSorMask, embeddedDpInfoMap);

    Protocol2HeadMaskMap availableHeads;
    availableHeads[CrcHandler::CRC_TMDS_A] = 0xF;

    lwdrRaTester->InitForNextAllocationSeq(0xF, availableHeads);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), true);
    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMaskTmds;
    SorMask pickedSorMask;
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_TMDS_A,
        &pickedDisplayIDs, &pickedHeadMaskTmds, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsAllocationPossible(), false);
}

TEST(LwdrAllocationTester, HeadsExhausted)
{
    ProtocolToDisplayIDsMap availableDisplayIDs;
    DisplayIDs displayIDs;
    displayIDs.push_back(0x200);
    displayIDs.push_back(0x100);
    availableDisplayIDs[CrcHandler::CRC_TMDS_A] = displayIDs;

    DisplayIDToPadlinkMaskMap availablePadLinks;
    availablePadLinks[0x200] = 1 << 0;
    availablePadLinks[0x100] = 1 << 1;

    SorMask availableSorMask = 0xF;

    EmbeddedDpInfoMap embeddedDpInfoMap;

    std::unique_ptr<LwdrResourceAllocator> lwdrRaTester = std::make_unique<LwdrResourceAllocator>(
       availableDisplayIDs, availablePadLinks, availableSorMask, embeddedDpInfoMap);

    Protocol2HeadMaskMap availableHeads;
    availableHeads[CrcHandler::CRC_TMDS_A] = 0xF;

    lwdrRaTester->InitForNextAllocationSeq(0x1, availableHeads);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), true);
    DisplayIDs pickedDisplayIDs;
    HeadMask pickedHeadMaskTmds;
    SorMask pickedSorMask;
    ASSERT_THAT(lwdrRaTester->PickDisplayPipes(CrcHandler::CRC_TMDS_A,
        &pickedDisplayIDs, &pickedHeadMaskTmds, &pickedSorMask), RC::OK);

    ASSERT_THAT(lwdrRaTester->IsProtocolPossible(CrcHandler::CRC_TMDS_A), false);
    ASSERT_THAT(lwdrRaTester->IsAllocationPossible(), false);
}
