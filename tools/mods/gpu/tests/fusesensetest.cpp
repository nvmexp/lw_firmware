/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "fusesensetest.h"

#include "core/include/fileholder.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/fuse/fuse.h"
#include "core/include/platform.h"
#include "core/include/version.h"

FuseSenseTest::FuseSenseTest() :
    m_ExportGoldens(false),
    m_GoldenFilename("")
{
    SetName("FuseSenseTest");
}

bool FuseSenseTest::IsSupported()
{
    bool isPermitted = (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK);
    bool isHW = Platform::GetSimulationMode() == Platform::Hardware;
    bool hasGoldFilename = m_GoldenFilename != "";
    if (!hasGoldFilename)
    {
        Printf(Tee::PriHigh, "FuseSenseTest requires GoldenFilename be specified.\n");
    }
    return isHW && isPermitted && hasGoldFilename;
}

RC FuseSenseTest::Run()
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        Printf(Tee::PriError, "FuseSenseTest is not supported\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC rc;
    vector<UINT32> readFuses;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    // Bug 1722075 - SEC Fuse Read cannot happen once RM's SEC2RTOS has started;
    // therefore, in order to avoid a timeout error, we skip fuse re-reading on
    // chips that need both SEC2RTOS and SEC Fuse Read.
    if (pSubdev->HasBug(1722075))
    {
        VerbosePrintf("This chip does not support Fuse Re-read for T96.\n");
        pSubdev->GetFuse()->GetCachedFuseReg(&readFuses);
    }
    else
    {
        CHECK_RC(ReReadFuseBlock(pSubdev->GetFuse(), &readFuses));
    }

    if (m_ExportGoldens)
    {
        return ExportGoldenFile(readFuses);
    }

    vector<UINT32> goldens;
    CHECK_RC(ImportGoldenFile(&goldens));

    CHECK_RC(CompareFusesToGold(readFuses, goldens));

    return OK;
}

RC FuseSenseTest::ReReadFuseBlock(Fuse* fuse, vector<UINT32>* fuseBlock)
{
    fuse->MarkFuseReadDirty();
    return fuse->GetCachedFuseReg(fuseBlock);
}

RC FuseSenseTest::ImportGoldenFile(vector<UINT32>* goldens)
{
    RC rc;

    CHECK_RC(CheckGoldenFile());

    FileHolder file;
    long fileSize;

    CHECK_RC(file.Open(m_GoldenFilename, "rb"));
    CHECK_RC(Utility::FileSize(file.GetFile(), &fileSize));

    fileSize /= sizeof(UINT32);

    goldens->resize(fileSize);

    size_t count = fread((void*)&(*goldens)[0], sizeof(UINT32), fileSize, file.GetFile());

    if (count != (size_t)fileSize || count == 0)
    {
        Printf(Tee::PriHigh, "Failed to read fuse block golden values.\n");
        return RC::FILE_READ_ERROR;
    }

    // Get rid of the signature
    goldens->erase(goldens->begin());

    return rc;
}

RC FuseSenseTest::ExportGoldenFile(const vector<UINT32>& fuseBlock)
{
    RC rc;

    CHECK_RC(CheckGoldenFile());

    FileHolder file;
    CHECK_RC(file.Open(m_GoldenFilename, "wb"));
    UINT32 extra = magicNumber;
    size_t count = fwrite(&extra, sizeof(UINT32), 1, file.GetFile());
    count += fwrite(&fuseBlock[0], sizeof(UINT32), fuseBlock.size(), file.GetFile());
    if (count != fuseBlock.size() + 1)
    {
        Printf(Tee::PriHigh, "Failed to write fuse block golden values.\n");
        return RC::FILE_WRITE_ERROR;
    }
    return rc;
}

RC FuseSenseTest::CompareFusesToGold
(
    const vector<UINT32>& fuses,
    const vector<UINT32>& gold
)
{
    if (gold.size() != fuses.size())
    {
        Printf(Tee::PriHigh, "%s golden values than read fuse rows.\n",
            gold.size() > fuses.size() ? "More" : "Fewer");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    RC rc;

    for (UINT32 i = 0; i < fuses.size(); i++)
    {
        if (fuses[i] != gold[i])
        {
            Printf(Tee::PriHigh, "Miscompare at row %u: Expected: %08x  Actual: %08x\n",
                i, gold[i], fuses[i]);
            rc = RC::GOLDEN_VALUE_MISCOMPARE;
        }
    }
    return rc;
}

RC FuseSenseTest::CheckGoldenFile()
{
    RC rc;
    FileHolder file;
    long fileSize;
    CHECK_RC(file.Open(m_GoldenFilename, "ab+"));
    rewind(file.GetFile());
    CHECK_RC(Utility::FileSize(file.GetFile(), &fileSize));

    // If the file already existed, make sure it's the one we're after
    if (fileSize > 0)
    {
        UINT32 signature;
        size_t count = fread(&signature, sizeof(UINT32), 1, file.GetFile());
        if (count == 0 || signature != magicNumber)
        {
            Printf(Tee::PriHigh, "Incorrect Golden File: %s\n", m_GoldenFilename.c_str());
            return RC::GOLDEN_VALUE_NOT_FOUND;
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

JS_CLASS_INHERIT(FuseSenseTest, GpuTest,
                 "Fuse block test");

CLASS_PROP_READWRITE(FuseSenseTest, ExportGoldens, bool,
                     "Bool: Export read fuse values as goldens instead of comparing.");
CLASS_PROP_READWRITE(FuseSenseTest, GoldenFilename, string,
                     "Name of golden values file.");
