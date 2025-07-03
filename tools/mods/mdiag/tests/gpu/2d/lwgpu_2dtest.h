/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

// parent class 2d tests, provides common functionality
// derived from LWGpuSingleChannelTest

#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/buf.h"
#include "gpu/include/gralloc.h"

#define lwgpu_2dtest_params LWGpu2dTest::Params
#define USE_BASE_LW_2D_TEST_FUNC 1

enum CrcMode {
    CRC_MODE_NOTHING=0,
    CRC_MODE_CHECK,
    CRC_MODE_GEN
};
enum ImageDumpFreq {
    IMAGE_DUMP_NEVER=0,
    IMAGE_DUMP_ON_EXIT,
    IMAGE_DUMP_PER_MAJOR,
    IMAGE_DUMP_PER_MINOR
};

class LWGpu2dTest : public LWGpuSingleChannelTest
{
protected:
    char          _testName[128];
    char          _stateReportName[MAX_FILENAME_SIZE];

    // check mode and CRC file/profile data:
    CrcMode       _crcMode;
    char          _crcFilename[MAX_FILENAME_SIZE];
    CrcProfile    *_crcProfilePtr = nullptr;
    char          _crcSection[128];

    // dumping images after ever minor or major iteration:
    ImageDumpFreq _imageDumpFreq;

    _DMA_TARGET m_DstTarget;
    _DMA_TARGET m_SrcTarget;
    _PAGE_LAYOUT m_DstLayout;
    _PAGE_LAYOUT m_SrcLayout;

    bool m_blocklinear;
    unsigned m_Log2blockHeight;
    unsigned m_Log2blockDepth;

    typedef list<string>     CrcChain;
    CrcChain                 m_CrcChain;

public:
    static const ParamDecl Params[];

    LWGpu2dTest(ArgReader *pParams);
    virtual ~LWGpu2dTest();

    virtual int Setup2d(const char *testName, string smcEngineLabel);

    virtual void PreRun(void);

    virtual void CleanUp(void) { LWGpuSingleChannelTest::CleanUp(); }
    virtual bool AddCrc(const char* crc) { m_CrcChain.push_back(crc); return true; }

    void ProcessCommonArgs();

    int LoadCrcProfile();
    int SaveCrcProfile();

    int FindCrcSection(const char *sectionLabel_pre,
                       const char *sectionLable_post);
};
