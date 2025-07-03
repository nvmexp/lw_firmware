/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "lwgpu_2dtest.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "sim/IChip.h"
#include "mdiag/utils/crc.h"
#include "gpu/utility/blocklin.h"
#include "core/include/gpu.h"
#include "lwos.h"
#include "lwmisc.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/utils/utils.h"

const ParamDecl LWGpu2dTest::Params[] = {
    MEMORY_SPACE_PARAMS("_dst", "destination surface"),
    MEMORY_SPACE_PARAMS("_src", "source surface"),

    // layout setup
    { "-surface_layout", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                               ParamDecl::GROUP_START), 0, 1, "surface layout (pitch or blocklinear)" },
    { "-blocklinear", "0", ParamDecl::GROUP_MEMBER, 0, 0, "surface has blocklinear layout" },
    { "-pitch",       "1", ParamDecl::GROUP_MEMBER, 0, 0, "surface has pitch layout" },
    UNSIGNED_PARAM("-block_height", "block height in gobs"),
    UNSIGNED_PARAM("-block_height_a", "block height in gobs (surface type A)"),
    UNSIGNED_PARAM("-block_height_b", "block height in gobs (surface type B)"),
    UNSIGNED_PARAM("-block_depth",  "block depth in gobs"),

    LAST_PARAM};

LWGpu2dTest::LWGpu2dTest(ArgReader *pParams) :
    LWGpuSingleChannelTest(pParams)
{
    strcpy(_testName, "");
    strcpy(_stateReportName, "");

    _crcMode = CRC_MODE_CHECK;
    strcpy(_crcFilename, "");
    strcpy(_crcSection, "");

    _imageDumpFreq = IMAGE_DUMP_NEVER;

    getStateReport()->enable();

    m_SrcTarget = GetLocationFromParams(m_pParams, "src", _DMA_TARGET_VIDEO);
    m_DstTarget = GetLocationFromParams(m_pParams, "dst", _DMA_TARGET_VIDEO);
    m_SrcLayout = GetPageLayoutFromParams(m_pParams, "src");
    m_DstLayout = GetPageLayoutFromParams(m_pParams, "dst");

    m_blocklinear = (m_pParams->ParamUnsigned("-surface_layout", 0) == 0);
    m_Log2blockHeight = log2(m_pParams->ParamUnsigned("-block_height", 16));
    m_Log2blockDepth = log2(m_pParams->ParamUnsigned("-block_depth", 1));

    if (Platform::GetSimulationMode() != Platform::Amodel)
        m_CrcChain.push_back("lw50f");
    m_CrcChain.push_back("lw50a");
}

LWGpu2dTest::~LWGpu2dTest()
{
    delete _crcProfilePtr;
}

int LWGpu2dTest::Setup2d(const char *testName, string smcEngineLabel)
{
    strncpy(_testName, testName, sizeof(_testName) - 1);

    if (!strlen(_stateReportName))
        strcpy(_stateReportName, _testName);

    // default CRC filename:
    if (!strlen(_crcFilename))
        sprintf(_crcFilename, "%s.crc", _testName);

    getStateReport()->init(_stateReportName);

    if (!SetupLWGpuResource(0, nullptr, smcEngineLabel))
    {
        return 0;
    }

    // Legacy mode: only GR0 available
    // SMC mode: With subscription model (each SmcEngine has its own LwRm*), 
    // the client only sees GR0
    UINT32 engineId = MDiagUtils::GetGrEngineId(0);

    int retValue = SetupSingleChanTest(engineId);
    return retValue;
}

void LWGpu2dTest::PreRun(void)
{
    LWGpuSingleChannelTest::PreRun();
}

#define DEFINED_AND_PRESENT(str) \
    (m_pParams->ParamDefined(str) && m_pParams->ParamPresent(str))

void LWGpu2dTest::ProcessCommonArgs()
{
    if (m_pParams->ParamPresent("-crcfile"))
        strcpy(_crcFilename, m_pParams->ParamStr("-crcfile"));

    if (m_pParams->ParamPresent("-o"))
        strcpy(_stateReportName, m_pParams->ParamStr("-o"));

    if (m_pParams->ParamPresent("-no_check"))
        _crcMode = CRC_MODE_NOTHING;

    // -gen is preferred option
    // but support others as well, for compatibility
    if (m_pParams->ParamPresent("-gen") ||
            DEFINED_AND_PRESENT("-dumpcrc") ||
            DEFINED_AND_PRESENT("-dumpCrc"))
        _crcMode = CRC_MODE_GEN;

    // -dumpExit is preferred option
    // but support others as well, for compatibility
    if (DEFINED_AND_PRESENT("-dumpExit"))
        _imageDumpFreq = IMAGE_DUMP_ON_EXIT;

    if (DEFINED_AND_PRESENT("-dumpMajor"))
        _imageDumpFreq = IMAGE_DUMP_PER_MAJOR;

    if (DEFINED_AND_PRESENT("-dumpMinor"))
        _imageDumpFreq = IMAGE_DUMP_PER_MINOR;
}

int LWGpu2dTest::LoadCrcProfile()
{
    InfoPrintf("CRC file: '%s'\n", _crcFilename);
    switch (_crcMode)
    {
        case CRC_MODE_NOTHING:
            ErrPrintf("Warning: LWGpu2dTest::LoadCrcProfile() called, and "
                   "crcMode is CRC_MODE_NOTHING\n");
            return 0;
        case CRC_MODE_CHECK:
            InfoPrintf("CRC mode: checking CRCs\n");
            break;
        case CRC_MODE_GEN:
            InfoPrintf("CRC mode: generating CRCs\n");
            break;
        default:
            ErrPrintf("CRC mode: UNDEFINED\n");
            return 0;
    }

    delete _crcProfilePtr;
    _crcProfilePtr = new CrcProfile();

    if (_crcMode == CRC_MODE_GEN)
    {
        FILE *fp = fopen(_crcFilename, "r");
        if (!fp)
        {
            InfoPrintf("CRC file %s doesn't exist, will create it\n",
                       _crcFilename);
            return 1;
        }
    }

    if (!_crcProfilePtr->LoadFromFile(_crcFilename) &&
        (_crcMode == CRC_MODE_CHECK))
    {
        ErrPrintf("Can't Load CRC File %s\n", _crcFilename);
        SetStatus(Test::TEST_NO_RESOURCES);
        getStateReport()->error("CRC file not found\n");
        return 0;
    }

    return 1;
}

int LWGpu2dTest::SaveCrcProfile()
{
    if (!_crcProfilePtr)
    {
        ErrPrintf("LWGpu2dTest::SaveCrcFile() called before crcProfile was "
                  "properly initialzied\n");
        return 0;
    }

    if (_crcMode != CRC_MODE_GEN)
    {
        ErrPrintf("LWGpu2dTest::SaveCrcFile() called, but -gen not "
                  "specified on commandline\n");
        return 0;
    }

    InfoPrintf("saving CRC file %s\n", _crcFilename);
    return _crcProfilePtr->SaveToFile(_crcFilename);
}

static void assembleCrcSection(const char *pre, const char *gpu,
                               const char *post, char *crcSection)
{
    if (pre && *pre && post && *post)
        sprintf(crcSection, "%s_%s_%s", pre, gpu, post);
    else if (pre && *pre)
        sprintf(crcSection, "%s_%s", pre, gpu);
    else if (post && *post)
        sprintf(crcSection, "%s_%s", gpu, post);
    else
        sprintf(crcSection, "%s", gpu);
}

int LWGpu2dTest::FindCrcSection(const char *sectionLabel_pre,
                                const char *sectionLabel_post)
{
    if (_crcMode == CRC_MODE_NOTHING)
    {
        // even if we are not checking or generating CRCs, this string
        // may be used for other things, such as in .bmp filenames
        assembleCrcSection(sectionLabel_pre, lwgpu->GetArchString(),
                           sectionLabel_post, _crcSection);
        return 1;
    }

    if (!_crcProfilePtr)
    {
        ErrPrintf("LWGpu2dTest::GetCrcSection() called before crcProfile was "
                  "properly initialzied\n");
        return 0;
    }

    if (_crcMode == CRC_MODE_GEN)
    {
        // use head of crc chain for name of CRC section
        // in the past, the name of the chip was the head of the CRC chain
        // but for "lw50", the head of the chain is "lw50f".
        const char *arch_name;
        CrcChain::const_iterator gpukey = m_CrcChain.begin();
        if (gpukey != m_CrcChain.end() && gpukey->c_str())
            arch_name = gpukey->c_str();
        else
            arch_name = lwgpu->GetArchString();

        assembleCrcSection(sectionLabel_pre, arch_name,
                           sectionLabel_post, _crcSection);
        if (_crcProfilePtr->SectionExists(_crcSection))
            InfoPrintf("Adding/replacing CRCs in existing CRC section '%s'\n",
                       _crcSection);
        else
            InfoPrintf("Adding CRCs in new CRC section '%s'\n",
                       _crcSection);
        return 1;
    }

    CrcChain::const_iterator gpukey = m_CrcChain.begin();
    bool found_section = false;

    if (gpukey == m_CrcChain.end() || !gpukey->c_str())
    {
        // if gpu doesn't have chain:

        WarnPrintf("GPU %s doesn't have CRC chain\n", lwgpu->GetArchString());
        assembleCrcSection(sectionLabel_pre, lwgpu->GetArchString(),
                           sectionLabel_post, _crcSection);

        if (_crcProfilePtr->SectionExists(_crcSection))
            found_section = true;
    }
    else
    {
        // follow chain, looking for key

        while (!found_section && gpukey != m_CrcChain.end() && gpukey->c_str())
        {
            assembleCrcSection(sectionLabel_pre, gpukey->c_str(),
                               sectionLabel_post, _crcSection);
            InfoPrintf("following CRC chain: %s\n", _crcSection);

            if (_crcProfilePtr->SectionExists(_crcSection))
                found_section = true;
            else
                ++gpukey;
        }
    }

    if (found_section)
    {
        InfoPrintf("using CRC section: %s\n", _crcSection);
    }
    else
    {
        assembleCrcSection(sectionLabel_pre, lwgpu->GetArchString(),
                           sectionLabel_post, _crcSection);
        ErrPrintf("CRC section not found for label '%s'\n", _crcSection);
        return 0;
    }

    return 1;
}
