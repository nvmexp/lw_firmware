/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_FUSESENSETEST_H
#define INCLUDED_FUSESENSETEST_H

#include <vector>

#include "gputest.h"

class Fuse;

class RC;

/**
 * Test for checking the stability of the fuse sensing hardware.
 *
 * This test will trigger a fresh re-sense of the fuse block and compare
 * against the values found in the file specified by GoldenFilename.
 *
 * Note: This is not a standard Mfg test. This is intended for isolated
 * cases and/or for use in a shmoo.
 *
 * For each given chip, this test should be run at ideal conditions with
 * ExportGoldens set to true and GoldenFilename specified in order to output
 * the state of the fuse block. Then for each non-ideal condition, rerun this
 * test specifying the same file for GoldenFilename.
 *
 * Note: This "golden" file is not related to the standard golden values file.
 * The gold file is specific to an individual chip, and thus must be regenerated
 * for each chip under test.
 */

class FuseSenseTest : public GpuTest
{
public:
    FuseSenseTest();
    virtual ~FuseSenseTest() {}

    virtual bool IsSupported();
    virtual RC Run();

    SETGET_PROP(ExportGoldens, bool);
    SETGET_PROP(GoldenFilename, string);

private:
    bool m_ExportGoldens;
    string m_GoldenFilename;

    static const UINT32 magicNumber = 0x11235813;

    RC CheckGoldenFile();
    RC ReReadFuseBlock(Fuse* fuse, vector<UINT32>* fuseBlock);
    RC ImportGoldenFile(vector<UINT32>* fuseBlock);
    RC ExportGoldenFile(const vector<UINT32>& fuseBlock);
    RC CompareFusesToGold(const vector<UINT32>& fuses, const vector<UINT32>& gold);
};

#endif
