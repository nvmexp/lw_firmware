/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/downbin/fselement.h"

#include "mockdownbinrules.h"

#include "gtest/gtest.h"

using namespace Downbin;

//------------------------------------------------------------------------------
TEST(DownbinFsElementSet, ModifyDisableMask)
{
    FsElementSet tpcs(FsUnit::TPC, 4, false);

    Downbin::Settings settings;
    settings.bModifyDefective = false;
    settings.bModifyReconfig = false;

    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x0U);

    EXPECT_EQ(tpcs.DisableElements(0x0, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(tpcs.GetDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(tpcs.GetDefectiveMask()->GetMask(), 0x0U);
    EXPECT_EQ(tpcs.GetReconfigMask()->GetMask(), 0x0U);

    EXPECT_EQ(tpcs.DisableElements(0x1, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(tpcs.GetDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(tpcs.GetDefectiveMask()->GetMask(), 0x0U);
    EXPECT_EQ(tpcs.GetReconfigMask()->GetMask(), 0x0U);

    EXPECT_EQ(tpcs.DisableElements(0x4, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(tpcs.GetDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(tpcs.GetDefectiveMask()->GetMask(), 0x0U);
    EXPECT_EQ(tpcs.GetReconfigMask()->GetMask(), 0x0U);
}

//------------------------------------------------------------------------------
TEST(DownbinFsElementSet, ModifyDefectiveMask)
{
    FsElementSet tpcs(FsUnit::TPC, 4, false);

    Downbin::Settings settings;
    settings.bModifyDefective = true;
    settings.bModifyReconfig = false;

    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x0U);

    EXPECT_EQ(tpcs.DisableElements(0x1, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(tpcs.GetDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(tpcs.GetDefectiveMask()->GetMask(), 0x1U);
    EXPECT_EQ(tpcs.GetReconfigMask()->GetMask(), 0x0U);

    EXPECT_EQ(tpcs.DisableElements(0x4, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(tpcs.GetDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(tpcs.GetDefectiveMask()->GetMask(), 0x5U);
    EXPECT_EQ(tpcs.GetReconfigMask()->GetMask(), 0x0U);
}

//------------------------------------------------------------------------------
TEST(DownbinFsElementSet, ModifyReconfigMask)
{
    FsElementSet tpcs(FsUnit::TPC, 4, false);

    Downbin::Settings settings;
    settings.bModifyDefective = false;
    settings.bModifyReconfig = true;

    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x0U);

    EXPECT_EQ(tpcs.DisableElements(0x1, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(tpcs.GetDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(tpcs.GetDefectiveMask()->GetMask(), 0x0U);
    EXPECT_EQ(tpcs.GetReconfigMask()->GetMask(), 0x1U);

    EXPECT_EQ(tpcs.DisableElements(0x4, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(tpcs.GetDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(tpcs.GetDefectiveMask()->GetMask(), 0x0U);
    EXPECT_EQ(tpcs.GetReconfigMask()->GetMask(), 0x5U);
}

//------------------------------------------------------------------------------
TEST(DownbinFsElementSet, IlwalidMask)
{
    FsElementSet tpcs(FsUnit::TPC, 4, false);
    Downbin::Settings settings;

    EXPECT_EQ(tpcs.DisableElements(0x10, settings), RC::BAD_PARAMETER);
}

//------------------------------------------------------------------------------
TEST(DownbinFsElementSet, CallListeners)
{
    FsElementSet tpcs(FsUnit::TPC, 4, false);
    Downbin::Settings settings;
    shared_ptr<MockDownbinRule> pMockRule = make_shared<MockDownbinRule>();
    tpcs.AddOnElementDisabledListener(dynamic_pointer_cast<DownbinRule>(pMockRule));

    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x0U);

    pMockRule->SetCallCount(0);
    EXPECT_EQ(tpcs.DisableElements(0x0, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(pMockRule->GetCallCount(), 0U);

    pMockRule->SetCallCount(0);
    EXPECT_EQ(tpcs.DisableElements(0x5, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(pMockRule->GetCallCount(), 1U);

    pMockRule->SetCallCount(0);
    EXPECT_EQ(tpcs.DisableElements(0x4, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(pMockRule->GetCallCount(), 0U);

    pMockRule->SetCallCount(0);
    EXPECT_EQ(tpcs.DisableElements(0xF, settings), RC::OK);
    EXPECT_EQ(tpcs.GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(pMockRule->GetCallCount(), 1U);
}
