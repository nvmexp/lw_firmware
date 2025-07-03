/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/rc.h"
#include "tracesubchan.h"

#include "compbits_lib.h"
#include "compbits_ut.h"

///////////////////////////////////////////////////

namespace CompBits
{

RC UnitTest::InitSurfCopier()
{
    RC rc;

    CHECK_RC(m_SurfCopier.Init(this, GetSurfCA(), true, true));
    CHECK_RC(FlushCompTags());
    CHECK_RC(IlwalidateCompTags());

    return rc;
}

void UnitTest::CleanupSurfCopier()
{
    m_SurfCopier.Clear();
}
/////////////////////////////////////////////

RC UnitTest::RWTestRun()
{
    RC rc;

    CHECK_RC(m_SurfCopier.CopyData());
    CheckpointBackingStore("Initial");

    CHECK_RC(m_SurfCopier.CopyAndVerifyAllCompBits());
    CheckpointBackingStore("PostRWVerify");
    CHECK_RC(IlwalidateCompTags());

    if (m_MismatchOffsets.size() < 1)
    {
        ErrPrintf("CBC: Putting compbits should have changed backing store, but actual not.\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    m_SurfCopier.ClearAllDstCompBits();
    CheckpointBackingStore("PostLibClear");
    CHECK_RC(IlwalidateCompTags());

    if (m_MismatchOffsets.size() < 1)
    {
        ErrPrintf("CBC: Clearing compbits should have changed backing store, but actual not.\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    InfoPrintf("CBC: RW test PASS!\n");

    return rc;
}

//////////////////////////////////////////////////

RC UnitTest::RW64KRun()
{
    RC rc;

    CHECK_RC(m_SurfCopier.CopyData());
    CheckpointBackingStore("Initial");

    m_SurfCopier.CopyAllPageCompBits();
    CheckpointBackingStore("PostCopyPageCompBits");
    CHECK_RC(IlwalidateCompTags());

    if (m_MismatchOffsets.size() < 1)
    {
        ErrPrintf("CBC: CopyPageCompBits compbits should have changed backing store, but actual: changed %lu byte(s).\n",
            (long)m_MismatchOffsets.size()
            );
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    m_SurfCopier.CopyAllCompBits();
    CheckpointBackingStore("PostCopyCompBits");
    CHECK_RC(IlwalidateCompTags());

    if (m_MismatchOffsets.size() > 0)
    {
        ErrPrintf("CBC: Rewriting CopyPageCompBits compbits by CopyCompBits shouldn't change backing store.\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    m_SurfCopier.ClearAllDstPageCompBits();
    CheckpointBackingStore("PostLibClear");
    CHECK_RC(IlwalidateCompTags());

    if (m_MismatchOffsets.size() < 1)
    {
        ErrPrintf("CBC: ClearPageCompBits compbits should have changed backing store, but actual: changed %lu byte(s).\n",
            (long)m_MismatchOffsets.size()
            );
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    m_SurfCopier.ClearAllDstCompBits();
    CheckpointBackingStore("FinishClearAllCompBits");
    CHECK_RC(IlwalidateCompTags());

    if (m_MismatchOffsets.size() > 0)
    {
        ErrPrintf("CBC: Reclearing ClearPageCompBits compbits by ClearCompBits shouldn't change backing store.\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    InfoPrintf("CBC: RW64K test PASS!\n");

    return rc;
}

///////////////////////////////////////////////////////////////////////////

RC UnitTest::SurfCopy2Run()
{
    RC rc;

    CHECK_RC(FlushCompTags());
    CheckpointBackingStore("Initial");

    vector<UINT08> oldDecompData;
    CHECK_RC(SaveSurfaceDecompData(GetSurfCA(), &oldDecompData));
    CHECK_RC(m_SurfCopier.CopyData());
    m_SurfCopier.CopyAllPageCompBits();

    CheckpointBackingStore("PostCopyPageCompBits");
    CHECK_RC(IlwalidateCompTags());

    if (m_MismatchOffsets.size() == 0)
    {
        ErrPrintf("CBC: no backing store bytes changed after restoring the compression bits.\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    vector<UINT08> newDecompData;
    CHECK_RC(SaveSurfaceDecompData(m_SurfCopier.GetDstSurf()->GetMdiagSurf(),
        &newDecompData));
    if (newDecompData != oldDecompData)
    {
        ErrPrintf("CBC: SurfCopyTest2: Surface decompressed data source & dest MISMATCH after test.\n");
        CompBits::ArrayReportFirstMismatch<vector<UINT08> >(oldDecompData,
            newDecompData);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    else
    {
        InfoPrintf("CBC: SurfCopyTest2: Surface decompressed data source & dest MATCH after test.\n");
    }

    InfoPrintf("CBC: SurfCopyTest2 PASS!\n");

    m_SurfCopier.ClearAllDstPageCompBits();

    return rc;
}

RC UnitTest::DoPostRender()
{
    if (CheckError(InitSurfCopier()))
    {
        CheckError(RWTestRun());
    }
    CleanupSurfCopier();
    if (CheckError(InitSurfCopier()))
    {
        CheckError(RW64KRun());
    }
    CleanupSurfCopier();
    if (CheckError(InitSurfCopier()))
    {
        CheckError(SurfCopy2Run());
    }
    CleanupSurfCopier();

    if (m_Errors > 0)
    {
        ErrPrintf("CBC: UnitTest, Found %u errors. Test FAIL.\n",
            m_Errors
            );
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    InfoPrintf("CBC: UnitTest PASS!\n");
    return OK;
}

///////////////////////////////////////////////

CompBitsTest* TestCreator::GetCommonUnitTest(TraceSubChannel* pSubChan)
{
    return new UnitTest(pSubChan);
}

} //namespace CompBits

