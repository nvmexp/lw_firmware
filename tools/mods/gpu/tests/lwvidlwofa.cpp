/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "codecs/lwca/lwlwvid/opticalflow/inc/ILwOFInterface.h"
#include "core/include/abort.h"
#include "core/include/fileholder.h"
#include "core/include/imagefil.h"
#include "core/include/tar.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include <lwca.h>
#include "lwca/lwdawrap.h"
#include <float.h>
#include "gpu/include/gpudev.h"
#include "gpu/include/gralloc.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/tests/lwdastst.h"
#include "gpu/utility/surfrdwr.h"
#include "gputest.h"
#include "ILwOFBuffer.h"
#include "lwofahw_api.h"
#include "lwOpticalFlowAPI2Drv.h"
#include <platform/LwThreading/LwThreadingClasses.h>

#if defined(DIRECTAMODEL)
#include "opengl/modsgl.h"
#include "common/amodel/IDirectAmodelCommon.h"
#endif

static constexpr UINT32 MIN_IMAGE_WIDTH = 32;
static constexpr UINT32 MAX_IMAGE_WIDTH = 8192;
static constexpr UINT32 MIN_IMAGE_HEIGHT = 32;
static constexpr UINT32 MAX_IMAGE_HEIGHT = 8192;
static constexpr UINT32 NO_OF_DEFAULT_INPUTS = 1;
static const string LWOFA_INPUT_FILENAME = "lwofa.bin";
static constexpr UINT32 MAX_PICKER_ATTEMPTS = 10;

//---------------------------------------------------------------------------
// Test class declaration
//---------------------------------------------------------------------------

class LwvidLwOfa : public LwdaStreamTest
{
public:
    LwvidLwOfa();

    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;
    virtual RC SetDefaultsForPicker(UINT32 idx);

    bool IsSupported() override;
    bool IsSupportedVf() override { return true; }

    SETGET_PROP(DumpFlowFile, bool);
    SETGET_PROP(DumpOutput, bool);
    SETGET_PROP(DumpCostFile, bool);
    SETGET_PROP(DumpInputImages, bool);
    SETGET_PROP(SkipLoops, string);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(DisableROIMode, bool);
    SETGET_PROP(DisableSubFrameMode, bool);
    SETGET_PROP(DisableDiagonal, bool);
    SETGET_PROP(DisableAdaptiveP2, bool);
    SETGET_PROP(SkipModeMask, UINT32);
    SETGET_PROP(SkipBitDepthMask, UINT32);
    SETGET_PROP(SkipGridSizeLog2Mask, UINT32);
    SETGET_PROP(SkipPassNumMask, UINT32);
    SETGET_PROP(SkipPydNumMask, UINT32);
    SETGET_PROP(MaxImageWidth, UINT32);
    SETGET_PROP(MaxImageHeight, UINT32);

private:
    enum OfaMode
    {
        EPISGM = 0,
        PYDSGM = 1,
        STEREO = 2
    };

    enum OfaBitDepth
    {
        BITDEPTH8  = 8,
        BITDEPTH10 = 10,
        BITDEPTH12 = 12,
        BITDEPTH16 = 16
    };

    enum OfaGridSizeLog2
    {
        GRIDSIZELOG2V0 = 0,
        GRIDSIZELOG2V1 = 1,
        GRIDSIZELOG2V2 = 2,
        GRIDSIZELOG2V3 = 3
    };

    enum OfaPassNum
    {
        PASSNUM1 = 1,
        PASSNUM2 = 2,
        PASSNUM3 = 3
    };

    enum OfaPydNum
    {
        PYDNUM1 = 1,
        PYDNUM2 = 2,
        PYDNUM3 = 3,
        PYDNUM4 = 4,
        PYDNUM5 = 5
    };

    struct TestIOParameters
    {
        LW_OF_EPIPOLAR_INFO epipolarInfo;
    };

    struct OfaBufferInfo
    {
        UINT64 pExtAlloc;
        ILwOFBuffer *pBuffer;
        UINT08 *pExtAllocHost;
        UINT32 dwWidth;
        UINT32 dwWidthInBytes;
        UINT32 dwHeight;
        UINT32 dwUserWidth;
        UINT32 dwUserHeight;
        UINT32 dwPitch;
        UINT32 dwSize;
    };

    struct OfaTarArchive
    {
        bool isArchiveRead;
        TarArchive tarArchive;
    };

    LWcontext                               m_LwContext = nullptr;
    OfaBufferInfo                           m_InputBfr[MAX_PYD_LEVEL];
    OfaBufferInfo                           m_RefInputBfr[MAX_PYD_LEVEL];
    OfaBufferInfo                           m_OutputBfr;
    OfaBufferInfo                           m_OutputCostBfr;
    std::vector< std::vector<UINT08> >      m_LwrrImagePyd;
    std::vector< std::vector<UINT08> >      m_RefImagePyd;
    ILwOFInterface*                         m_pOFHW = nullptr;
    unique_ptr<GpuGoldenSurfaces>           m_pGoldenSurfaces;
    bool                                    m_DumpFlowFile = false;
    bool                                    m_DumpCostFile = false;
    bool                                    m_DumpOutput = false;
    Surface2D                               m_OutputSurface;
    SurfaceUtils::MappingSurfaceWriter      m_OutputSurfWriter;
    StickyRC                                m_DeferRc;
    GpuTestConfiguration*                   m_pTestConfig;
    bool                                    m_DumpInputImages = false;
    FancyPicker::FpContext*                 m_pLwvidOfaFpCtx = nullptr;
    string                                  m_SkipLoops;
    set<UINT32>                             m_SkipLoopsIndices;
    bool                                    m_KeepRunning = false;
    bool                                    m_DisableROIMode = false;
    bool                                    m_DisablePydLevelFilter = false;
    bool                                    m_DisableSubFrameMode = false;
    bool                                    m_DisableDiagonal = false;
    bool                                    m_DisableAdaptiveP2 = false;
    UINT32                                  m_SkipModeMask = 0x0;
    UINT32                                  m_SkipBitDepthMask = 0x0;
    UINT32                                  m_SkipGridSizeLog2Mask = 0x0;
    UINT32                                  m_SkipPassNumMask = 0x0;
    UINT32                                  m_SkipPydNumMask = 0x0;
    static OfaTarArchive                    m_OfaTarArchive;
    static const std::map<UINT32, UINT32>   m_ModeToMask;
    static const std::map<UINT32, UINT32>   m_BitDepthToMask;
    static const std::map<UINT32, UINT32>   m_GridSizeLog2ToMask;
    static const std::map<UINT32, UINT32>   m_PassNumToMask;
    static const std::map<UINT32, UINT32>   m_PydNumToMask;
    UINT32                                  m_MaxImageWidth = MAX_IMAGE_WIDTH;
    UINT32                                  m_MaxImageHeight = MAX_IMAGE_HEIGHT;
    RC AllocateIOBuffers(const appOptionsParS &testOptions);
    RC CreateIOBuffer
    (
        const UINT32 width,
        const UINT32 height,
        const LW_OF_BUFFER_FORMAT format,
        const LW_OF_BUFFER_USAGE usage,
        ILwOFBuffer **pBuffer
    );
    RC CreateLwOpticalFlow();
    RC Deinit();
    RC DownloadData(const OfaBufferInfo &bufInfo);
    RC GetFlowData(const appOptionsParS &appOptions);
    RC InitLwOpticalFlow(appOptionsParS *pTestOptions);
    RC ReadDefaultInputImages(appOptionsParS *pTestOptions);
    RC ReleaseIOBuffers(const appOptionsParS &testOptions);
    RC RunLwOpticalFlow(const appOptionsParS &testOptions, const TestIOParameters &testParams);
    void SetDefaultArgs(appOptionsParS *pTestOptions, TestIOParameters *pTestParams);
    RC UploadData(const OfaBufferInfo &bufInfo);
    RC DumpOutputSurface
    (
        const Surface2D &surface,
        string filename,
        const appOptionsParS &appOptions
    );
    RC DumpFlowData(const appOptionsParS &appOptions) const;
    RC DumpCostOutputBuffer() const;
    RC DumpSurfacesIfRequested(const appOptionsParS &appOptions);
    RC InnerRun(const appOptionsParS &testOptions, const TestIOParameters &testParams);
    RC ReadFileFromArchive
    (
        const string &fileName,
        vector<UINT08> *content
    );
    RC RandomizeInputParams(appOptionsParS *pTestOptions, TestIOParameters *pTestParams);
    RC RandomizeInputImages(const UINT32 size, UINT08* pAddr);
    void DumpConfigStruct(const appOptionsParS &appOptions);
    RC RandomizeROIMode(appOptionsParS *pTestOptions);
    RC RandomizeSubframeMode(appOptionsParS *pTestOptions);
    void RandomizeEpiPolarInfo(TestIOParameters *pTestParams);
    RC DumpInputImages
    (
        const appOptionsParS &testOptions,
        void *plwrImage,
        void *pRefImage
    );
    RC RandomizePyramids(appOptionsParS *pTestOptions);
    RC AddInputImages(appOptionsParS *pTestOptions);
    RC ApplyConstraints(appOptionsParS *pTestOptions, TestIOParameters *pTestParams);
    RC PickUINT32ParamsWithMask
    (
        const UINT32 pickerParam,
        const std::map<UINT32, UINT32>& maskMap,
        const UINT32 skipMask,
        const string paramStr,
        UINT32 *pParam
    );
    RC PickBoolParams(const UINT32 pickerParam, const bool isDisabled, bool *pParam);
    RC ValidateTestArgs();
    RC ValidateInputRanges(UINT32 value, UINT32 min, UINT32 max, const string& valueStr);
    bool DoSkipLoop(const appOptionsParS& appOption);
};

JS_CLASS_INHERIT(LwvidLwOfa, GpuTest, "Lwvid library based OFA engine test");

LwvidLwOfa::OfaTarArchive LwvidLwOfa::m_OfaTarArchive;
const std::map<UINT32, UINT32> LwvidLwOfa::m_ModeToMask =
{
    {EPISGM, 1 << 0},
    {PYDSGM, 1 << 1},
    {STEREO, 1 << 2},
};
const std::map<UINT32, UINT32> LwvidLwOfa::m_BitDepthToMask =
{
    {BITDEPTH8,  1 << 0},
    {BITDEPTH10, 1 << 1},
    {BITDEPTH12, 1 << 2},
    {BITDEPTH16, 1 << 3},
};
const std::map<UINT32, UINT32> LwvidLwOfa::m_GridSizeLog2ToMask =
{
    {GRIDSIZELOG2V0, 1 << 0},
    {GRIDSIZELOG2V1, 1 << 1},
    {GRIDSIZELOG2V2, 1 << 2},
    {GRIDSIZELOG2V3, 1 << 3},
};
const std::map<UINT32, UINT32> LwvidLwOfa:: m_PassNumToMask =
{
    {PASSNUM1, 1 << 0},
    {PASSNUM2, 1 << 1},
    {PASSNUM3, 1 << 2},
};
const std::map<UINT32, UINT32> LwvidLwOfa::m_PydNumToMask =
{
    {PYDNUM1, 1 << 0},
    {PYDNUM2, 1 << 1},
    {PYDNUM3, 1 << 2},
    {PYDNUM4, 1 << 3},
    {PYDNUM5, 1 << 4},
};
//---------------------------------------------------------------------------
// Test class function definitions
//---------------------------------------------------------------------------

LwvidLwOfa::LwvidLwOfa()
: m_OutputSurfWriter(m_OutputSurface)
{
    memset(&m_InputBfr, 0, sizeof(m_InputBfr));
    memset(&m_RefInputBfr, 0, sizeof(m_RefInputBfr));
    memset(&m_OutputBfr, 0, sizeof(m_OutputBfr));
    memset(&m_OutputCostBfr, 0, sizeof(m_OutputCostBfr));
    m_OutputSurfWriter.SetMaxMappedChunkSize(0x80000000);
    SetName("LwvidLwOfa");
    CreateFancyPickerArray(FPK_LWVIDLWOFA_NUM_PICKERS);
    m_pTestConfig = GetTestConfiguration();
    m_pLwvidOfaFpCtx = GetFpContext();
}

RC LwvidLwOfa::Setup()
{
    RC rc;

#if defined(DIRECTAMODEL)
    DirectAmodel::AmodelAttribute directAmodelKnobs[] = {
        { DirectAmodel::IDA_KNOB_STRING, 0 },
        { DirectAmodel::IDA_KNOB_STRING, reinterpret_cast<LwU64>(
            "ChipFE::argsForPlugInClassSim -im 0x8000 -dm 0x8000 -ipm 0x500 -smax 0x8000 -cv 6 -sdk LwofaPlugin LwofaPlugin/ofaFALCONOS ") }
    };
    if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Ampere)
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ( {0xC6FA} LWOFA LwofaPlugin/ofa_plugin.dll )");
    }
    else if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Hopper)
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ( {0xB8FA} LWOFA LwofaPlugin/ofa_plugin.dll )");
    }
    else // Ada:
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ( {0xC9FA} LWOFA LwofaPlugin/ofa_plugin.dll )");
    }
    dglDirectAmodelSetAttributes(directAmodelKnobs, static_cast<LwU32>(NUMELEMS(directAmodelKnobs)));
#endif

    CHECK_RC(LwdaStreamTest::Setup());
    m_LwContext = GetLwdaInstance().GetHandle(GetBoundLwdaDevice());

    m_pGoldenSurfaces = make_unique<GpuGoldenSurfaces>(GetBoundGpuDevice());

    // Initialize output surface to default values and attach
    m_OutputSurface.SetWidth(1);
    m_OutputSurface.SetHeight(1);
    m_OutputSurface.SetColorFormat(ColorUtils::Y8);
    m_pGoldenSurfaces->AttachSurface(&m_OutputSurface, "L", 0);
    CHECK_RC(GetGoldelwalues()->SetSurfaces(m_pGoldenSurfaces.get()));

    CHECK_RC(ValidateTestArgs());

    if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Ada)
    {
        m_SkipModeMask |= m_ModeToMask.at(STEREO) | m_ModeToMask.at(EPISGM);
        m_SkipBitDepthMask |= m_BitDepthToMask.at(BITDEPTH10) |
                             m_BitDepthToMask.at(BITDEPTH12) |
                             m_BitDepthToMask.at(BITDEPTH16);      //ADA support only 8bpp
        m_DisableSubFrameMode = true;                              // No subframe support for ADA
    }

    if (!LwvidLwOfa::m_OfaTarArchive.isArchiveRead)
    {
        // Load the tarball that contains all the input images into memory
        const string tarballFilePath = Utility::DefaultFindFile(LWOFA_INPUT_FILENAME, true);
        if (!LwvidLwOfa::m_OfaTarArchive.tarArchive.ReadFromFile(tarballFilePath))
        {
            Printf(Tee::PriError, "%s does not exist\n", LWOFA_INPUT_FILENAME.c_str());
            return RC::FILE_DOES_NOT_EXIST;
        }
        LwvidLwOfa::m_OfaTarArchive.isArchiveRead = true;
    }
    return rc;
}

bool LwvidLwOfa::DoSkipLoop(const appOptionsParS& appOption)
{
    if (static_cast<UINT32>(appOption.width) > m_MaxImageWidth ||
        static_cast<UINT32>(appOption.height) > m_MaxImageHeight)
    {
        VerbosePrintf("Skipping loop %d since width %d or height %d is greater than"
            " allowed width %u height %u\n",
            m_pLwvidOfaFpCtx->LoopNum,
            appOption.width,
            appOption.height,
            m_MaxImageWidth,
            m_MaxImageHeight);
        return true;
    }

    if ((m_ModeToMask.at(appOption.ofaMode) & m_SkipModeMask) >> \
        Utility::BitScanForward(m_ModeToMask.at(appOption.ofaMode)) == 0x1)
    {
        VerbosePrintf("Skipping loop %d since mode %d is disabled through mask\n",
            m_pLwvidOfaFpCtx->LoopNum, appOption.ofaMode);
        return true;
    }

    return false;
}

RC LwvidLwOfa::Run()
{
    RC rc;
    const bool perpetualRun = m_KeepRunning;
    m_DeferRc.Clear();
    CHECK_LW_RC(lwCtxPushLwrrent(m_LwContext));
    DEFER
    {
        lwCtxPopLwrrent(&m_LwContext);
    };
    appOptionsParS defaultTestOptions[NO_OF_DEFAULT_INPUTS];
    TestIOParameters defaultTestParams[NO_OF_DEFAULT_INPUTS];
    memset(&defaultTestOptions, 0, sizeof(appOptionsParS) * NO_OF_DEFAULT_INPUTS);
    memset(&defaultTestParams, 0, sizeof(TestIOParameters) * NO_OF_DEFAULT_INPUTS);
    SetDefaultArgs(defaultTestOptions, defaultTestParams);
    bool atleastOneLoopTested = false;

    do
    {
        UINT32 startLoop = m_pTestConfig->StartLoop();
        UINT32 endLoop   = startLoop + m_pTestConfig->Loops();
        for (m_pLwvidOfaFpCtx->LoopNum = startLoop; m_pLwvidOfaFpCtx->LoopNum < endLoop;
            m_pLwvidOfaFpCtx->LoopNum++)
        {
            if (perpetualRun && !m_KeepRunning)
            {
                break;
            }
            CHECK_RC(Abort::Check());

            if (m_SkipLoopsIndices.find(m_pLwvidOfaFpCtx->LoopNum) != m_SkipLoopsIndices.end())
            {
                VerbosePrintf("Skipping loop %d\n", m_pLwvidOfaFpCtx->LoopNum);
                continue;
            }
            VerbosePrintf("\nRunning loop %d\n", m_pLwvidOfaFpCtx->LoopNum);
            m_pLwvidOfaFpCtx->Rand.SeedRandom(m_pTestConfig->Seed() + m_pLwvidOfaFpCtx->LoopNum);
            GetFancyPickerArray()->Restart();
            appOptionsParS testOptions;
            TestIOParameters testParams;
            memset(&testOptions, 0, sizeof(appOptionsParS));
            memset(&testParams, 0, sizeof(TestIOParameters));
            if (m_pLwvidOfaFpCtx->LoopNum < NO_OF_DEFAULT_INPUTS)
            {
                if (DoSkipLoop(defaultTestOptions[m_pLwvidOfaFpCtx->LoopNum]))
                    continue;

                memcpy(&testOptions, &defaultTestOptions[m_pLwvidOfaFpCtx->LoopNum],
                    sizeof(appOptionsParS));
                memcpy(&testParams, &defaultTestParams[m_pLwvidOfaFpCtx->LoopNum],
                    sizeof(TestIOParameters));
                CHECK_RC(ReadDefaultInputImages(&testOptions));
            }
            else
            {
                CHECK_RC(RandomizeInputParams(&testOptions, &testParams));
                if (DoSkipLoop(testOptions))
                    continue;
            }
            DumpConfigStruct(testOptions);
            CHECK_RC(CreateLwOpticalFlow());
            CHECK_RC(InitLwOpticalFlow(&testOptions));
            CHECK_RC(InnerRun(testOptions, testParams));
            CHECK_RC(m_DeferRc);
            atleastOneLoopTested = true;
        }
    } while(m_KeepRunning);

    if (!atleastOneLoopTested)
    {
        Printf(Tee::PriError, "No loop was exelwted during the test\n");
        return RC::UNEXPECTED_TEST_COVERAGE;
    }

    // Golden errors that are deferred due to "-run_on_error" can only be retrieved by running
    // Golden::ErrorRateTest() This must be done before clearing surfaces which is done in cleanup
    CHECK_RC(GetGoldelwalues()->ErrorRateTest(GetJSObject()));
    return rc;
}

RC LwvidLwOfa::InnerRun(const appOptionsParS &testOptions, const TestIOParameters &testParams)
{
    RC rc;
    CHECK_RC(AllocateIOBuffers(testOptions));
    DEFER
    {
        m_DeferRc = ReleaseIOBuffers(testOptions);
        m_DeferRc = Deinit();
    };
    CHECK_RC(RunLwOpticalFlow(testOptions, testParams));
    CHECK_RC(GetFlowData(testOptions));
    CHECK_RC(DumpSurfacesIfRequested(testOptions));
    GetGoldelwalues()->SetLoop(m_pLwvidOfaFpCtx->LoopNum);
    CHECK_RC(GetGoldelwalues()->Run());
    return rc;
}

RC LwvidLwOfa::DumpSurfacesIfRequested(const appOptionsParS &appOptions)
{
    RC rc;
    if (m_DumpOutput)
    {
        CHECK_RC(DumpOutputSurface(m_OutputSurface,
            Utility::StrPrintf("%s", appOptions.outFlow),
            appOptions));
    }

    if (m_DumpFlowFile)
    {
        CHECK_RC(DumpFlowData(appOptions));
    }

    if (m_DumpCostFile)
    {
        CHECK_RC(DumpCostOutputBuffer());
    }
    return rc;
}

RC LwvidLwOfa::Cleanup()
{
    StickyRC rc;
    Goldelwalues *pGoldelwalues = GetGoldelwalues();
    pGoldelwalues->ClearSurfaces();
    m_pGoldenSurfaces.reset();
    CHECK_RC(LwdaStreamTest::Cleanup());
    return rc;
}

void LwvidLwOfa::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "LwvidLwOfa test Js Properties:\n");
    Printf(pri, "\tDumpFlowFile:\t\t\t%s\n", m_DumpFlowFile ? "true" : "false");
    Printf(pri, "\tDumpOutput:\t\t\t%s\n", m_DumpOutput ? "true" : "false");
    Printf(pri, "\tDumpCostFile:\t\t\t%s\n", m_DumpCostFile ? "true" : "false");
    Printf(pri, "\tDumpInputImages:\t\t%s\n", m_DumpInputImages ? "true" : "false");
    Printf(pri, "\tSkipLoops:\t\t\t%s\n", m_SkipLoops.c_str());
    Printf(pri, "\tKeepRunning:\t\t\t%s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tDisableAdaptiveP2:\t\t%s\n", m_DisableAdaptiveP2 ? "true" : "false");
    Printf(pri, "\tDisableDiagonal:\t\t%s\n", m_DisableDiagonal ? "true" : "false");
    Printf(pri, "\tDisableROIMode:\t\t\t%s\n", m_DisableROIMode ? "true" : "false");
    Printf(pri, "\tDisableSubFrameMode:\t\t%s\n", m_DisableSubFrameMode ? "true" : "false");
    Printf(pri, "\tSkipModeMask:\t\t\t0x%0x\n", m_SkipModeMask);
    Printf(pri, "\tSkipBitDepthMask:\t\t0x%0x\n", m_SkipBitDepthMask);
    Printf(pri, "\tSkipGridSizeLog2Mask:\t\t0x%0x\n", m_SkipGridSizeLog2Mask);
    Printf(pri, "\tSkipPassNumMask:\t\t0x%0x\n", m_SkipPassNumMask);
    Printf(pri, "\tSkipPydNumMask:\t\t\t0x%0x\n", m_SkipPydNumMask);
    Printf(pri, "\tMaxImageWidth:\t\t\t%u\n", m_MaxImageWidth);
    Printf(pri, "\tMaxImageHeight:\t\t\t%u\n", m_MaxImageHeight);
    Printf(pri, "\tDisablePydLevelFilter:\t\t%s\n", m_DisablePydLevelFilter ? "true" : "false");
}

bool LwvidLwOfa::IsSupported()
{
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "LwvidLwOfa is not supported on SOC devices\n");
        return false;
    }

    OfaAlloc ofaAlloc;
    const bool supported = ofaAlloc.IsSupported(GetBoundGpuDevice());
    if (!supported)
    {
        Printf(Tee::PriLow, "LwvidLwOfa has not found supported OFA hw class\n");
    }
    return supported;
}

void LwvidLwOfa::SetDefaultArgs(appOptionsParS *pTestOptions, TestIOParameters *pTestParams)
{
    // Default values are from drivers/multimedia/OpticalFlow/tools/lwofa_emu/src/lwofa_emu.cpp
    pTestOptions[0].lwrrImage[0]            = "pyd_org_8192x8192_8bits.bin";
    pTestOptions[0].lwrrImage[1]            = "pyd_org_4096x4096_8bits.bin";
    pTestOptions[0].lwrrImage[2]            = "pyd_org_2048x2048_8bits.bin";
    pTestOptions[0].lwrrImage[3]            = "pyd_org_1024x1024_8bits.bin";
    pTestOptions[0].lwrrImage[4]            = "pyd_org_512x512_8bits.bin";
    pTestOptions[0].refImage[0]             = "pyd_ref_8192x8192_8bits.bin";
    pTestOptions[0].refImage[1]             = "pyd_ref_4096x4096_8bits.bin";
    pTestOptions[0].refImage[2]             = "pyd_ref_2048x2048_8bits.bin";
    pTestOptions[0].refImage[3]             = "pyd_ref_1024x1024_8bits.bin";
    pTestOptions[0].refImage[4]             = "pyd_ref_512x512_8bits.bin";
    pTestOptions[0].hintMv[0][0]            = "";
    pTestOptions[0].epiInfo                 = "";
    pTestOptions[0].outFlow                 = "pyd_8192x8192_8bits_out.bin";
    pTestOptions[0].outPath                 = "./dump";
    pTestOptions[0].ofaMode                 = PYDSGM;
    pTestOptions[0].sgmP1[0]                = 6;
    pTestOptions[0].sgmP1[1]                = 6;
    pTestOptions[0].sgmP1[2]                = 6;
    pTestOptions[0].sgmP1[3]                = 6;
    pTestOptions[0].sgmP1[4]                = 6;
    pTestOptions[0].sgmP2[0]                = 32;
    pTestOptions[0].sgmP2[1]                = 32;
    pTestOptions[0].sgmP2[2]                = 32;
    pTestOptions[0].sgmP2[3]                = 32;
    pTestOptions[0].sgmP2[4]                = 32;
    pTestOptions[0].sgmdP1[0]               = 6;
    pTestOptions[0].sgmdP1[1]               = 6;
    pTestOptions[0].sgmdP1[2]               = 6;
    pTestOptions[0].sgmdP1[3]               = 6;
    pTestOptions[0].sgmdP1[4]               = 6;
    pTestOptions[0].sgmdP2[0]               = 32;
    pTestOptions[0].sgmdP2[1]               = 32;
    pTestOptions[0].sgmdP2[2]               = 32;
    pTestOptions[0].sgmdP2[3]               = 32;
    pTestOptions[0].sgmdP2[4]               = 32;
    pTestOptions[0].calibFile               = "";
    pTestOptions[0].rlSearch                = false;
    pTestOptions[0].passNum[0]              = PASSNUM1;
    pTestOptions[0].passNum[1]              = PASSNUM1;
    pTestOptions[0].passNum[2]              = PASSNUM1;
    pTestOptions[0].passNum[3]              = PASSNUM1;
    pTestOptions[0].passNum[4]              = PASSNUM1;
    pTestOptions[0].pydZeroHint             = 1;
    pTestOptions[0].enDiagonal[0]           = false;
    pTestOptions[0].enDiagonal[1]           = false;
    pTestOptions[0].enDiagonal[2]           = false;
    pTestOptions[0].enDiagonal[3]           = false;
    pTestOptions[0].enDiagonal[4]           = false;
    pTestOptions[0].pydNum                  = PYDNUM5;
    pTestOptions[0].ndisp                   = 256;
    pTestOptions[0].width                   = 8192;
    pTestOptions[0].height                  = 8192;
    pTestOptions[0].adaptiveP2[0]           = true;
    pTestOptions[0].adaptiveP2[1]           = true;
    pTestOptions[0].adaptiveP2[2]           = true;
    pTestOptions[0].adaptiveP2[3]           = true;
    pTestOptions[0].adaptiveP2[4]           = true;
    pTestOptions[0].alphaLog2[0]            = 0;
    pTestOptions[0].alphaLog2[1]            = 0;
    pTestOptions[0].alphaLog2[2]            = 0;
    pTestOptions[0].alphaLog2[3]            = 0;
    pTestOptions[0].alphaLog2[4]            = 0;
    pTestOptions[0].xGridLog2[0]            = GRIDSIZELOG2V3;
    pTestOptions[0].yGridLog2[0]            = GRIDSIZELOG2V3;
    pTestOptions[0].xGridLog2[1]            = GRIDSIZELOG2V3;
    pTestOptions[0].yGridLog2[1]            = GRIDSIZELOG2V3;
    pTestOptions[0].xGridLog2[2]            = GRIDSIZELOG2V3;
    pTestOptions[0].yGridLog2[2]            = GRIDSIZELOG2V3;
    pTestOptions[0].xGridLog2[3]            = GRIDSIZELOG2V3;
    pTestOptions[0].yGridLog2[3]            = GRIDSIZELOG2V3;
    pTestOptions[0].xGridLog2[4]            = GRIDSIZELOG2V3;
    pTestOptions[0].yGridLog2[4]            = GRIDSIZELOG2V3;
    pTestOptions[0].xGridLog2[5]            = GRIDSIZELOG2V3;
    pTestOptions[0].yGridLog2[5]            = GRIDSIZELOG2V3;
    pTestOptions[0].xGridLog2[6]            = GRIDSIZELOG2V3;
    pTestOptions[0].yGridLog2[6]            = GRIDSIZELOG2V3;
    pTestOptions[0].pydConstCost[0]         = 5;
    pTestOptions[0].pydConstCost[1]         = 5;
    pTestOptions[0].pydConstCost[2]         = 5;
    pTestOptions[0].pydConstCost[3]         = 5;
    pTestOptions[0].pydConstCost[4]         = 5;
    pTestOptions[0].pydConstCostEn          = false;
    pTestOptions[0].disableCostOut          = false;
    pTestOptions[0].subPixelRefine          = true;
    pTestOptions[0].epiSearchRange          = 256;
    pTestOptions[0].bitDepth                = BITDEPTH8;
    pTestOptions[0].roiMode                 = false;
    pTestOptions[0].roiPosXStartDiv32       = 0;
    pTestOptions[0].roiPosYStartDiv8        = 0;
    pTestOptions[0].roiWidthDiv32           = 0;
    pTestOptions[0].roiHeightDiv8           = 0;
    pTestOptions[0].subframeMode            = false;
    pTestOptions[0].subframeYStartDiv16     = 0;
    pTestOptions[0].subframeHeightDiv16     = 0;
    pTestOptions[0].enablePydLevelFilter    = true;
    pTestOptions[0].pydLevelFilterType      = 0;
    pTestOptions[0].pydLevelFilterKsize[0]  = 5;
    pTestOptions[0].pydLevelFilterKsize[1]  = 3;
    pTestOptions[0].pydLevelFilterKsize[2]  = 3;
    pTestOptions[0].pydLevelFilterKsize[3]  = 5;
    pTestOptions[0].pydLevelFilterKsize[4]  = 5;
    pTestOptions[0].pydLevelFilterSigma     = 
    pTestOptions[0].pconf                   = false;
    pTestOptions[0].pydDepthHintSelect      = false;
    pTestOptions[0].tempBufCompression      = true;
    pTestOptions[0].resetOnlyPass0          = true;
    pTestOptions[0].pydConstHintFirst       = false;
    pTestOptions[0].pydHighPerfMode         = 2;

    memset(&pTestParams[0], 0, sizeof(TestIOParameters));
}

RC LwvidLwOfa::ReadFileFromArchive
(
    const string &fileName,
    vector<UINT08> *pContent
)
{
    RC rc;

    MASSERT(pContent);
    if (!pContent)
        return RC::SOFTWARE_ERROR;

    VerbosePrintf("Input filename %s\n", fileName.c_str());

    TarFile *pTarFile = LwvidLwOfa::m_OfaTarArchive.tarArchive.Find(fileName.c_str());
    if (!pTarFile)
    {
        Printf(Tee::PriError, "%s does not contain %s\n", LWOFA_INPUT_FILENAME.c_str(),
            fileName.c_str());
        return RC::FILE_READ_ERROR;
    }

    const unsigned filesize = pTarFile->GetSize();
    pContent->resize(filesize);
    pTarFile->Seek(0);
    if (pTarFile->Read(reinterpret_cast<char*>(pContent->data()), filesize) != filesize)
    {
        Printf(Tee::PriError, "File read error for %s\n", fileName.c_str());
        return RC::FILE_READ_ERROR;
    }

    return rc;
}

RC LwvidLwOfa::ReadDefaultInputImages
(
    appOptionsParS *pTestOptions
)
{
    RC rc;
    vector<UINT08> lwrImage;
    vector<UINT08> refImage;
    UINT32 bpp = (pTestOptions->bitDepth > BITDEPTH8) ? 2 : 1;

    CHECK_RC(ReadFileFromArchive(pTestOptions->lwrrImage[0], &lwrImage));
    CHECK_RC(ReadFileFromArchive(pTestOptions->refImage[0], &refImage));
    if (((pTestOptions->width * pTestOptions->height * bpp) != lwrImage.size()) ||
        (lwrImage.size() != refImage.size()))
    {
        MASSERT(!"Check the input image contents or the image dimensions!\n");
        return RC::SOFTWARE_ERROR;
    }

    m_LwrrImagePyd.assign(1, lwrImage);
    m_RefImagePyd.assign(1, refImage);

    if (static_cast<OfaMode>(pTestOptions->ofaMode) == PYDSGM)
    {
        UINT32 pydWidth = (pTestOptions->width + 1) >> 1;
        UINT32 pydHeight = (pTestOptions->height + 1) >> 1;
        for (INT32 loop = 1; loop < pTestOptions->pydNum; loop++)
        {
            CHECK_RC(ReadFileFromArchive(pTestOptions->lwrrImage[loop], &lwrImage));
            CHECK_RC(ReadFileFromArchive(pTestOptions->refImage[loop], &refImage));
            m_LwrrImagePyd.push_back(lwrImage);
            m_RefImagePyd.push_back(refImage);
            if (((pydWidth * pydHeight * bpp) != lwrImage.size()) ||
                (lwrImage.size() != refImage.size()))
            {
                MASSERT(!"Check the pyramid input image contents!\n");
                return RC::SOFTWARE_ERROR;
            }
            pydWidth = (pydWidth + 1) >> 1;
            pydHeight = (pydHeight + 1) >> 1;
        }
    }

    if (static_cast<UINT32>(pTestOptions->width) < MIN_IMAGE_WIDTH ||
        static_cast<UINT32>(pTestOptions->height) < MIN_IMAGE_HEIGHT)
    {
        Printf(Tee::PriError, "Width or height is less than supported!\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC LwvidLwOfa::CreateLwOpticalFlow()
{
    RC rc;
    LW_OF_STATUS status = CreateOFInterface(m_LwContext, &m_pOFHW);
    if (status != LW_OF_STATUS::LW_OF_SUCCESS)
    {
        Printf(Tee::PriError, "Failed to create the OFA interface!\n");
        return RC::SOFTWARE_ERROR;
    }
    status = m_pOFHW->CreateHWInstance();
    if (status != LW_OF_STATUS::LW_OF_SUCCESS)
    {
        Printf(Tee::PriError, "Failed to create the OFA HW instance!\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC LwvidLwOfa::InitLwOpticalFlow(appOptionsParS *pTestOptions)
{
    RC rc;
    LW_OF_INIT_PARAMS_API2DRV initParams;

    memset(&initParams, 0x00, sizeof(initParams));

    initParams.privData = (void *)pTestOptions;
    initParams.privDataId = LW_OF_PRIV_DATA_ID_INIT_INTERNAL;
    initParams.privDataSize = sizeof(appOptionsParS);

    //setting these parameter as they are required by API. other wise back door should be taken.
    initParams.width = pTestOptions->width;
    initParams.height = pTestOptions->height;
    initParams.mode = (LW_OF_MODE) pTestOptions->ofaMode;
    initParams.outGridSize = LW_OF_OUTPUT_VECTOR_GRID_SIZE_4;

    LW_OF_STATUS status = m_pOFHW->Initialize(&initParams);
    if (status != LW_OF_STATUS::LW_OF_SUCCESS)
    {
        Printf(Tee::PriError, "Failed to initialize the OFA HW instance!\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC LwvidLwOfa::CreateIOBuffer
(
    const UINT32 width,
    const UINT32 height,
    const LW_OF_BUFFER_FORMAT format,
    const LW_OF_BUFFER_USAGE usage,
    ILwOFBuffer **pBuffer
)
{
    RC rc;
    LW_OF_STATUS status;
    LW_OF_CREATE_BUFFER_API2DRV createBufParams = { 0 };

    memset(&createBufParams, 0, sizeof(createBufParams));
    createBufParams.width = width;
    createBufParams.height = height;
    createBufParams.bufferFormat = format;
    createBufParams.bufferUsage = usage;
    createBufParams.memoryHeap = LW_OF_MEMORY_HEAP_LWARRAY;
    status = m_pOFHW->CreateBuffer(&createBufParams, pBuffer);
    if (status != LW_OF_STATUS::LW_OF_SUCCESS)
    {
        Printf(Tee::PriError, "Failed to create the buffers!\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

// TODO replace the whole function with SUFRACE2D allocations
RC LwvidLwOfa::AllocateIOBuffers(const appOptionsParS &testOptions)
{
    RC rc;
    ILwOFBuffer *pBuffer = nullptr;
    UINT32 gridSizeX = 1 << testOptions.xGridLog2[0];
    UINT32 gridSizeY = 1 << testOptions.yGridLog2[0];
    UINT32 bpp = (testOptions.bitDepth > BITDEPTH8) ? 2 : 1;

    CHECK_RC(CreateIOBuffer(testOptions.width, testOptions.height,
        (bpp == 2) ? LW_OF_BUFFER_FORMAT_SHORT : LW_OF_BUFFER_FORMAT_GRAYSCALE8,
        LW_OF_BUFFER_USAGE_INPUT, &pBuffer));
    m_InputBfr[0].pBuffer = pBuffer;
    m_InputBfr[0].pExtAlloc = pBuffer->GetSurfaceAddr();
    m_InputBfr[0].dwWidth = testOptions.width;
    m_InputBfr[0].dwHeight = testOptions.height;
    m_InputBfr[0].dwWidthInBytes = testOptions.width * bpp;
    m_InputBfr[0].dwPitch = testOptions.width * bpp;
    m_InputBfr[0].dwSize = m_InputBfr[0].dwPitch * testOptions.height;
    CHECK_LW_RC(lwMemAllocHost((void**)&m_InputBfr[0].pExtAllocHost, m_InputBfr[0].dwSize));

    for (UINT32 pydlevel = 1; pydlevel < static_cast<UINT32>(testOptions.pydNum); pydlevel++)
    {
        m_InputBfr[pydlevel].dwWidth = (m_InputBfr[pydlevel-1].dwWidth + 1) >> 1;
        m_InputBfr[pydlevel].dwHeight = (m_InputBfr[pydlevel-1].dwHeight + 1) >> 1;
        m_InputBfr[pydlevel].dwPitch = m_InputBfr[pydlevel].dwWidth * bpp;
        m_InputBfr[pydlevel].dwWidthInBytes = m_InputBfr[pydlevel].dwWidth * bpp;
        m_InputBfr[pydlevel].dwSize = m_InputBfr[pydlevel].dwPitch * m_InputBfr[pydlevel].dwHeight;
        m_InputBfr[pydlevel].pExtAllocHost = (UINT08*)malloc(m_InputBfr[pydlevel].dwSize);
    }

    CHECK_RC(CreateIOBuffer(testOptions.width, testOptions.height,
        (bpp == 2) ? LW_OF_BUFFER_FORMAT_SHORT : LW_OF_BUFFER_FORMAT_GRAYSCALE8,
        LW_OF_BUFFER_USAGE_INPUT, &pBuffer));
    m_RefInputBfr[0].pBuffer = pBuffer;
    m_RefInputBfr[0].pExtAlloc = pBuffer->GetSurfaceAddr();
    m_RefInputBfr[0].dwWidth = testOptions.width;
    m_RefInputBfr[0].dwHeight = testOptions.height;
    m_RefInputBfr[0].dwWidthInBytes = testOptions.width * bpp;
    m_RefInputBfr[0].dwPitch = testOptions.width * bpp;
    m_RefInputBfr[0].dwSize = m_RefInputBfr[0].dwPitch * testOptions.height;
    CHECK_LW_RC(lwMemAllocHost((void**)&m_RefInputBfr[0].pExtAllocHost, m_RefInputBfr[0].dwSize));

    for (UINT32 pydlevel = 1; pydlevel < static_cast<UINT32>(testOptions.pydNum); pydlevel++)
    {
        m_RefInputBfr[pydlevel].dwWidth = (m_RefInputBfr[pydlevel - 1].dwWidth + 1) >> 1;
        m_RefInputBfr[pydlevel].dwHeight = (m_RefInputBfr[pydlevel - 1].dwHeight + 1) >> 1;
        m_RefInputBfr[pydlevel].dwPitch = m_RefInputBfr[pydlevel].dwWidth * bpp;
        m_RefInputBfr[pydlevel].dwWidthInBytes = m_RefInputBfr[pydlevel].dwWidth * bpp;
        m_RefInputBfr[pydlevel].dwSize = m_RefInputBfr[pydlevel].dwPitch * \
            m_RefInputBfr[pydlevel].dwHeight;
        m_RefInputBfr[pydlevel].pExtAllocHost = (UINT08*)malloc(m_RefInputBfr[pydlevel].dwSize);
    }

    CHECK_RC(CreateIOBuffer((testOptions.width + gridSizeX - 1) >> testOptions.xGridLog2[0],
        (testOptions.height + gridSizeY - 1) >> testOptions.yGridLog2[0],
        (testOptions.ofaMode == STEREO) ? LW_OF_BUFFER_FORMAT_SHORT :
                LW_OF_BUFFER_FORMAT_SHORT2, LW_OF_BUFFER_USAGE_OUTPUT, &pBuffer));

    m_OutputBfr.pBuffer = pBuffer;
    m_OutputBfr.pExtAlloc = pBuffer->GetSurfaceAddr();
    m_OutputBfr.dwWidth = (testOptions.width + gridSizeX - 1) >> testOptions.xGridLog2[0];
    m_OutputBfr.dwHeight = (testOptions.height + gridSizeY - 1) >> testOptions.yGridLog2[0];
    m_OutputBfr.dwUserWidth = (testOptions.width + gridSizeX - 1) >> testOptions.xGridLog2[0];
    m_OutputBfr.dwUserHeight = (testOptions.height + gridSizeY - 1) >> testOptions.yGridLog2[0];
    m_OutputBfr.dwWidthInBytes = m_OutputBfr.dwWidth * ((testOptions.ofaMode == STEREO) ?
            sizeof(LW_OF_STEREO_DISPARITY) : sizeof(LW_OF_FLOW_VECTOR));
    m_OutputBfr.dwPitch = m_OutputBfr.dwWidth * ((testOptions.ofaMode == STEREO) ?
            sizeof(LW_OF_STEREO_DISPARITY) : sizeof(LW_OF_FLOW_VECTOR));
    m_OutputBfr.dwSize = m_OutputBfr.dwPitch * m_OutputBfr.dwHeight;

    CHECK_RC(CreateIOBuffer((testOptions.width + gridSizeX - 1) >> testOptions.xGridLog2[0],
        (testOptions.height + gridSizeY - 1) >> testOptions.yGridLog2[0],
        LW_OF_BUFFER_FORMAT_GRAYSCALE8, LW_OF_BUFFER_USAGE_COST, &pBuffer));

    m_OutputCostBfr.pBuffer = pBuffer;
    m_OutputCostBfr.pExtAlloc = pBuffer->GetSurfaceAddr();
    m_OutputCostBfr.dwWidth = (testOptions.width + gridSizeX - 1) >> testOptions.xGridLog2[0];
    m_OutputCostBfr.dwHeight = (testOptions.height + gridSizeY - 1) >> testOptions.yGridLog2[0];
    m_OutputCostBfr.dwUserWidth = (testOptions.width + gridSizeX - 1) >> testOptions.xGridLog2[0];
    m_OutputCostBfr.dwUserHeight = (testOptions.height + gridSizeY - 1) >> testOptions.yGridLog2[0];
    m_OutputCostBfr.dwWidthInBytes = (testOptions.width + gridSizeX - 1) >> testOptions.xGridLog2[0];
    m_OutputCostBfr.dwPitch = (testOptions.width + gridSizeY - 1) >> testOptions.xGridLog2[0];
    m_OutputCostBfr.dwSize = m_OutputCostBfr.dwPitch * m_OutputCostBfr.dwHeight;

    CHECK_LW_RC(lwMemAllocHost((void**)&m_OutputBfr.pExtAllocHost, m_OutputBfr.dwSize));
    CHECK_LW_RC(lwMemAllocHost((void**)&m_OutputCostBfr.pExtAllocHost, m_OutputCostBfr.dwSize));

    memset(m_OutputBfr.pExtAllocHost, 0, m_OutputBfr.dwSize);
    memset(m_OutputCostBfr.pExtAllocHost, 0, m_OutputCostBfr.dwSize);

    m_OutputSurfWriter.Unmap();
    m_OutputSurface.Free();
    m_OutputSurface.SetWidth(m_OutputBfr.dwWidth);
    m_OutputSurface.SetHeight(m_OutputBfr.dwHeight);
    m_OutputSurface.SetPitch(m_OutputBfr.dwPitch);
    m_OutputSurface.SetColorFormat(ColorUtils::R8);
    m_OutputSurface.SetLocation(Memory::Coherent);
    CHECK_RC(m_OutputSurface.Alloc(GetBoundGpuDevice()));

    return rc;
}

RC LwvidLwOfa::RunLwOpticalFlow
(
    const appOptionsParS &testOptions,
    const TestIOParameters &testParams
)
{
    RC rc;
    LW_OF_STATUS status = LW_OF_SUCCESS;

    for (UINT32 pydlevel = 0; pydlevel < static_cast<UINT32>(testOptions.pydNum); pydlevel++)
    {
        UINT08 *pInHostBfr = m_InputBfr[pydlevel].pExtAllocHost;
        for (UINT32 y = 0; y < m_InputBfr[pydlevel].dwHeight; y++)
        {
            memcpy(pInHostBfr + (m_InputBfr[pydlevel].dwPitch * y),
                static_cast<UINT08*>(m_LwrrImagePyd[pydlevel].data()) +
                (m_InputBfr[pydlevel].dwWidthInBytes * y), m_InputBfr[pydlevel].dwWidthInBytes);
        }

        UINT08 *pInRefHostBfr = m_RefInputBfr[pydlevel].pExtAllocHost;
        for (UINT32 y = 0; y < m_RefInputBfr[pydlevel].dwHeight; y++)
        {
            memcpy(pInRefHostBfr + (m_RefInputBfr[pydlevel].dwPitch * y),
                static_cast<UINT08*>(m_RefImagePyd[pydlevel].data()) +
                (m_RefInputBfr[pydlevel].dwWidthInBytes * y),
                m_RefInputBfr[pydlevel].dwWidthInBytes);
        }
    }
    CHECK_RC(UploadData(m_RefInputBfr[0]));
    CHECK_RC(UploadData(m_InputBfr[0]));

    LW_OF_EXELWTE_PARAMS_API2DRV exeParams;
    LW_OF_EXELWTE_INPUT_PARAMS_INTERNAL exePrivParams;

    memset(&exeParams, 0, sizeof(exeParams));
    memset(&exePrivParams, 0, sizeof(exePrivParams));
    exeParams.disableTemporalHints = LW_OF_TRUE;
    exeParams.enableExternalHints = LW_OF_FALSE;
    exeParams.inputFrame = m_InputBfr[0].pBuffer;
    exeParams.referenceFrame = m_RefInputBfr[0].pBuffer;
    exeParams.outputBuffer = m_OutputBfr.pBuffer;
    exeParams.outputCostBuffer = m_OutputCostBfr.pBuffer;

    for (UINT32 pydlevel = 1; pydlevel < static_cast<UINT32>(testOptions.pydNum); pydlevel++)
    {
        exePrivParams.PydInputFrameptr[pydlevel-1] = m_InputBfr[pydlevel].pExtAllocHost;
        exePrivParams.PydRefFrameptr[pydlevel - 1] = m_RefInputBfr[pydlevel].pExtAllocHost;
    }
    exeParams.privData = &exePrivParams;
    exeParams.privDataId = LW_OF_PRIV_DATA_ID_EXELWTE_INTERNAL;
    exeParams.privDataSize = sizeof(LW_OF_EXELWTE_INPUT_PARAMS_INTERNAL);

    if (testOptions.ofaMode == EPISGM)
    {
        exePrivParams.epipolarParams = testParams.epipolarInfo;
    }

    status = m_pOFHW->Execute(&exeParams, nullptr, nullptr);
    if (status != LW_OF_STATUS::LW_OF_SUCCESS)
    {
        Printf(Tee::PriError, "OFA Execute failed!\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC LwvidLwOfa::Deinit()
{
    RC rc;
    m_pOFHW->Release();
    m_pOFHW = nullptr;
    return rc;
}

RC LwvidLwOfa::UploadData(const OfaBufferInfo &bufInfo)
{
    RC rc;
    LWDA_MEMCPY2D lwCopy2d;
    memset(&lwCopy2d, 0, sizeof(lwCopy2d));
    lwCopy2d.WidthInBytes = bufInfo.dwWidthInBytes;
    lwCopy2d.srcMemoryType = LW_MEMORYTYPE_HOST;
    lwCopy2d.srcHost = bufInfo.pExtAllocHost;
    lwCopy2d.srcPitch = bufInfo.dwPitch;
    lwCopy2d.dstMemoryType = LW_MEMORYTYPE_ARRAY;
    lwCopy2d.dstArray = (LWarray)bufInfo.pExtAlloc;
    lwCopy2d.Height = bufInfo.dwHeight;
    CHECK_LW_RC(lwMemcpy2D(&lwCopy2d));
    CHECK_RC(GetLwdaInstance().SynchronizeContext());
    return rc;
}

// TODO Move to Surface2D instead of writing to a file
RC LwvidLwOfa::GetFlowData
(
    const appOptionsParS &appOptions
)
{
    RC rc;
    UINT08 *sysMemPtr = (UINT08 *)m_OutputBfr.pExtAllocHost;

    CHECK_RC(GetLwdaInstance().Synchronize());
    CHECK_RC(DownloadData(m_OutputBfr));
    if (m_DumpCostFile)
    {
        CHECK_RC(DownloadData(m_OutputCostBfr));
    }

    for (UINT32 row = 0; row < m_OutputBfr.dwHeight; ++row)
    {
        CHECK_RC(m_OutputSurfWriter.WriteBytes(row * m_OutputSurface.GetPitch(),
            &sysMemPtr[row * m_OutputBfr.dwPitch], m_OutputBfr.dwPitch));
    }
    return rc;
}

RC LwvidLwOfa::DumpFlowData(const appOptionsParS &appOptions) const
{
    RC rc;
    UINT32 userWidth = m_OutputBfr.dwUserWidth;
    UINT32 userHeight = m_OutputBfr.dwUserHeight;
    UINT08 *sysMemPtr = (UINT08 *)m_OutputBfr.pExtAllocHost;
    FileHolder flowParsedData;
    CHECK_RC(flowParsedData.Open("flow.txt", "w"));
    sysMemPtr = (UINT08 *)m_OutputBfr.pExtAllocHost;
    int16_t *flowData = (int16_t*)sysMemPtr;
    int16_t mvx, mvy;
    UINT32 outPitch = m_OutputBfr.dwPitch;
    //Print only horizontal MV for Stereo SGM mode
    if (appOptions.ofaMode == STEREO)
    {
        for (UINT32 row = 0; row < userHeight; row++)
        {
            for (UINT32 col = 0; col < userWidth; col++)
            {
                mvx = flowData[col];
                fprintf(flowParsedData.GetFile(), "%d ", mvx);
            }
            flowData = (int16_t*)((UINT08*)flowData + outPitch);
            fprintf(flowParsedData.GetFile(), "\n");
        }
    }
    else
    {
        for (UINT32 row = 0; row < userHeight; row++)
        {
            for (UINT32 col = 0; col < userWidth; col++)
            {
                mvx = flowData[2 * col];
                mvy = flowData[2 * col + 1];
                fprintf(flowParsedData.GetFile(), "%d, %d ", mvx, mvy);
            }
            flowData = (int16_t*)((UINT08*)flowData + outPitch);
            //sysMemPtr += pOutputExtAlloc->GetPitch();
            fprintf(flowParsedData.GetFile(), "\n");
        }
    }
    return rc;
}

RC LwvidLwOfa::DumpCostOutputBuffer() const
{
    RC rc;
    UINT32 userWidth = m_OutputCostBfr.dwUserWidth;
    UINT32 userHeight = m_OutputCostBfr.dwUserHeight;
    UINT08* sysMemPtr = (UINT08 *)m_OutputCostBfr.pExtAllocHost;

    FileHolder costFile;
    CHECK_RC(costFile.Open("cost.txt", "w"));
    for (UINT32 row = 0; row < userHeight; row++)
    {
        for (UINT32 col = 0; col < userWidth; col++)
        {
            fprintf(costFile.GetFile(), "%d ", sysMemPtr[col]);
        }
        sysMemPtr += m_OutputCostBfr.dwPitch;
        fprintf(costFile.GetFile(), "\n");
    }
    return rc;
}

RC LwvidLwOfa::DownloadData(const OfaBufferInfo &bufInfo)
{
    RC rc;
    LWDA_MEMCPY2D lwCopy2d;

    memset(&lwCopy2d, 0, sizeof(lwCopy2d));
    lwCopy2d.WidthInBytes = bufInfo.dwWidthInBytes;
    lwCopy2d.dstMemoryType = LW_MEMORYTYPE_HOST;
    lwCopy2d.dstHost = bufInfo.pExtAllocHost;
    lwCopy2d.dstPitch = bufInfo.dwPitch;
    lwCopy2d.srcMemoryType = LW_MEMORYTYPE_ARRAY;
    lwCopy2d.srcArray = (LWarray)bufInfo.pExtAlloc;
    lwCopy2d.Height = bufInfo.dwHeight;
    CHECK_LW_RC(lwMemcpy2D(&lwCopy2d));
    CHECK_RC(GetLwdaInstance().SynchronizeContext());
    return rc;
}

RC LwvidLwOfa::ReleaseIOBuffers(const appOptionsParS &testOptions)
{
    RC rc;
    {
        if (m_InputBfr[0].pBuffer)
            m_InputBfr[0].pBuffer->Release();
        if (m_InputBfr[0].pExtAllocHost)
            CHECK_LW_RC(lwMemFreeHost(m_InputBfr[0].pExtAllocHost));

        for (UINT32 pydlevel = 1; pydlevel < static_cast<UINT32>(testOptions.pydNum); pydlevel++)
        {
            if (m_InputBfr[pydlevel].pExtAllocHost)
            {
                free(m_InputBfr[pydlevel].pExtAllocHost);
                m_InputBfr[pydlevel].pExtAllocHost = NULL;
            }
        }
    }

    {
        if (m_RefInputBfr[0].pBuffer)
            m_RefInputBfr[0].pBuffer->Release();
        if (m_RefInputBfr[0].pExtAllocHost)
            CHECK_LW_RC(lwMemFreeHost(m_RefInputBfr[0].pExtAllocHost));

        for (UINT32 pydlevel = 1; pydlevel < static_cast<UINT32>(testOptions.pydNum); pydlevel++)
        {
            if (m_RefInputBfr[pydlevel].pExtAllocHost)
            {
                free(m_RefInputBfr[pydlevel].pExtAllocHost);
                m_RefInputBfr[pydlevel].pExtAllocHost = NULL;
            }
        }
    }

    {
        if (m_OutputBfr.pBuffer)
            m_OutputBfr.pBuffer->Release();
        if (m_OutputBfr.pExtAllocHost)
            CHECK_LW_RC(lwMemFreeHost(m_OutputBfr.pExtAllocHost));
    }

    {
        if (m_OutputCostBfr.pBuffer)
            m_OutputCostBfr.pBuffer->Release();
        if (m_OutputCostBfr.pExtAllocHost)
            CHECK_LW_RC(lwMemFreeHost(m_OutputCostBfr.pExtAllocHost));
    }

    m_LwrrImagePyd.clear();
    m_RefImagePyd.clear();
    m_OutputSurfWriter.Unmap();
    m_OutputSurface.Free();
    return rc;
}

RC LwvidLwOfa::DumpOutputSurface
(
    const Surface2D &surface,
    string filename,
    const appOptionsParS &testOptions
)
{
    RC rc;
    if (!m_DumpOutput)
    {
        return rc;
    }
    if (!surface.IsAllocated())
    {
        Printf(Tee::PriError, "Surface not allocated in DumpOutputSurface.\n");
        return RC::SOFTWARE_ERROR;
    }
    if (filename.length() == 0)
    {
        filename = Utility::StrPrintf("%sOutputLoop%d.bin", GetName().c_str(),
                    m_pLwvidOfaFpCtx->LoopNum);
    }
    vector<UINT08> data;
    SurfaceUtils::MappingSurfaceReader surfaceReader(surface);
    data.resize(surface.GetSize());
    CHECK_RC(surfaceReader.ReadBytes(0, &data[0], data.size()));
    UINT08 *sysMemPtr = &data[0];

    FileHolder fileHolder;
    CHECK_RC(fileHolder.Open(filename, "wb"));
    for (UINT32 row = 0; row < m_OutputBfr.dwHeight; row++)
    {
        const UINT32 size = m_OutputBfr.dwWidth * (UINT32)((testOptions.ofaMode == STEREO) ?
                sizeof(LW_OF_STEREO_DISPARITY) : sizeof(LW_OF_FLOW_VECTOR));
        if (size != fwrite(sysMemPtr, 1, size, fileHolder.GetFile()))
        {
            Printf(Tee::PriError, "Can't Write File %s\n", filename.c_str());
            return RC::FILE_WRITE_ERROR;
        }
        sysMemPtr += m_OutputBfr.dwPitch;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Set defaults for the fancy pickers
//!
/* virtual */ RC LwvidLwOfa::SetDefaultsForPicker(UINT32 idx)
{
    FancyPicker *pPicker = &(*GetFancyPickerArray())[idx];
    RC rc;

    CHECK_RC(pPicker->ConfigRandom());
    switch (idx)
    {
        case FPK_LWVIDLWOFA_OFAMODE:
            // Bump up PYDSGM, this is most stressful
            pPicker->AddRandItem(2, PYDSGM);
            pPicker->AddRandItem(1, STEREO);
            pPicker->AddRandItem(1, EPISGM);
            break;
        case FPK_LWVIDLWOFA_IMAGE_WIDTH:
            pPicker->AddRandRange(1, MIN_IMAGE_WIDTH, MAX_IMAGE_WIDTH);
            break;
        case FPK_LWVIDLWOFA_IMAGE_HEIGHT:
            pPicker->AddRandRange(1, MIN_IMAGE_HEIGHT, MAX_IMAGE_HEIGHT);
            break;
        case FPK_LWVIDLWOFA_IMAGE_BITDEPTH:
            pPicker->AddRandItem(1, BITDEPTH8);
            pPicker->AddRandItem(1, BITDEPTH10);
            pPicker->AddRandItem(1, BITDEPTH12);
            pPicker->AddRandItem(1, BITDEPTH16);
            break;
        case FPK_LWVIDLWOFA_GRID_SIZE_LOG2:
            // Bump up grid size 1x1, this is most stressful
            pPicker->AddRandItem(3, GRIDSIZELOG2V0);
            pPicker->AddRandItem(1, GRIDSIZELOG2V1);
            pPicker->AddRandItem(1, GRIDSIZELOG2V2);
            pPicker->AddRandItem(1, GRIDSIZELOG2V3);
            break;
        case FPK_LWVIDLWOFA_PYD_NUM:
            pPicker->AddRandItem(2, PYDNUM5);
            pPicker->AddRandItem(1, PYDNUM4);
            pPicker->AddRandItem(1, PYDNUM3);
            pPicker->AddRandItem(1, PYDNUM2);
            pPicker->AddRandItem(1, PYDNUM1);
            break;
        case FPK_LWVIDLWOFA_PASS_NUM:
            pPicker->AddRandItem(2, PASSNUM3);
            pPicker->AddRandItem(1, PASSNUM1);
            pPicker->AddRandItem(1, PASSNUM2);
            break;
        case FPK_LWVIDLWOFA_ADAPTIVEP2:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_LWVIDLWOFA_ROI_MODE:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_LWVIDLWOFA_ENDIAGONAL:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_LWVIDLWOFA_SUBFRAMEMODE:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_LWVIDLWOFA_PYD_LEVEL_FILTER_EN:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_LWVIDLWOFA_PYD_FILTER_TYPE:
            pPicker->AddRandRange(1, 0, 1);
            break;
        case FPK_LWVIDLWOFA_PYD_FILTER_K_SIZE:
            pPicker->AddRandItem(1, 3);
            pPicker->AddRandItem(1, 5);
            break;
        case FPK_LWVIDLWOFA_PYD_FILTER_SIGMA:
            CHECK_RC(pPicker->FConfigRandom());
            pPicker->FAddRandRange(1, 0, 30.0);
            break;
        case FPK_LWVIDLWOFA_PYD_HIGH_PERF_MODE:
            pPicker->AddRandItem(1, 0);
            pPicker->AddRandItem(1, 2);
            break;
        default:
            MASSERT(!"Unknown picker");
            return RC::SOFTWARE_ERROR;
    }
    pPicker->CompileRandom();
    return rc;
}

void LwvidLwOfa::DumpConfigStruct(const appOptionsParS &appOptions)
{
    VerbosePrintf("Mode                 %d\n", appOptions.ofaMode);
    VerbosePrintf("PydNum               %d\n", appOptions.pydNum);
    for (UINT32 loop = 0; loop < static_cast<UINT32>(appOptions.pydNum); loop++)
    {
        VerbosePrintf("SgmP1[%d]             %d\n", loop, appOptions.sgmP1[loop]);
        VerbosePrintf("SgmP2[%d]             %d\n", loop, appOptions.sgmP2[loop]);
        VerbosePrintf("SgmdP1[%d]            %d\n", loop, appOptions.sgmdP1[loop]);
        VerbosePrintf("SgmdP2[%d]            %d\n", loop, appOptions.sgmdP2[loop]);
        VerbosePrintf("PassNum[%d]           %d\n", loop, appOptions.passNum[loop]);
        VerbosePrintf("EnDiagonal[%d]        %s\n",
            loop, appOptions.enDiagonal[loop] ? "true" : "false");
        VerbosePrintf("AdaptiveP2[%d]        %s\n",
            loop, appOptions.adaptiveP2[loop] ? "true" : "false");
        VerbosePrintf("Alphalog2[%d]         %d\n", loop, appOptions.alphaLog2[loop]);
        VerbosePrintf("PydConstCost[%d]      %d\n", loop, appOptions.pydConstCost[loop]);
    }
    VerbosePrintf("Rlsearch             %s\n", appOptions.rlSearch? "true" : "false");
    VerbosePrintf("PydZeroHint          %s\n", appOptions.pydZeroHint? "true" : "false");
    VerbosePrintf("Ndisp                %d\n", appOptions.ndisp);
    VerbosePrintf("Input image width    %d\n", appOptions.width);
    VerbosePrintf("Input image height   %d\n", appOptions.height);
    VerbosePrintf("Input xgridsize log2 %d\n", appOptions.xGridLog2[0]);
    VerbosePrintf("Input ygridsize log2 %d\n", appOptions.yGridLog2[0]);
    VerbosePrintf("pydConstCostEn       %s\n", appOptions.pydConstCostEn ? "true" : "false");
    VerbosePrintf("DisableCostOut       %s\n", appOptions.disableCostOut? "true" : "false");
    VerbosePrintf("SubPixelRefine       %s\n", appOptions.subPixelRefine? "true" : "false");
    VerbosePrintf("EpiSearchRange       %d\n", appOptions.epiSearchRange);
    VerbosePrintf("Input image bitdepth %d\n", appOptions.bitDepth);
    VerbosePrintf("RoiMode              %s\n", appOptions.roiMode? "true" : "false");
    if (appOptions.roiMode)
    {
        VerbosePrintf("RoiPosXStartDiv32    %d\n", appOptions.roiPosXStartDiv32);
        VerbosePrintf("RoiPosYStartDiv8     %d\n", appOptions.roiPosYStartDiv8);
        VerbosePrintf("RoiWidthDiv32        %d\n", appOptions.roiWidthDiv32);
        VerbosePrintf("RoiHeightDiv8        %d\n", appOptions.roiHeightDiv8);
    }
    VerbosePrintf("SubframeMode         %s\n", appOptions.subframeMode? "true" : "false");
    if (appOptions.subframeMode)
    {
        VerbosePrintf("SubframeYStartDiv16  %d\n", appOptions.subframeYStartDiv16);
        VerbosePrintf("SubframeHeightDiv16  %d\n", appOptions.subframeHeightDiv16);
    }
    VerbosePrintf("EnablePydLevelFilter         %s\n", 
        appOptions.enablePydLevelFilter ? "true" : "false");
    if (appOptions.enablePydLevelFilter)
    {
        VerbosePrintf("PydLevelFilterType   %d\n", appOptions.pydLevelFilterType);
        VerbosePrintf("PydLevelFilterSigma  %d\n", appOptions.pydLevelFilterSigma);
        for (UINT32 loop = 0; loop < static_cast<UINT32>(appOptions.pydNum); loop++)
        {
            VerbosePrintf("PydLevelFilterKsize[%d]   %f\n", loop, 
                appOptions.pydLevelFilterKsize[loop]);
        }
    }
    VerbosePrintf("Pconf                %s\n", appOptions.pconf ? "true" : "false");
    VerbosePrintf("PydDepthHintSelect   %s\n", appOptions.pydDepthHintSelect ? "true" : "false");
    VerbosePrintf("TempBufCompression   %s\n", appOptions.tempBufCompression ? "true" : "false");
    VerbosePrintf("ResetOnlyPass0       %s\n", appOptions.resetOnlyPass0 ? "true" : "false");
    VerbosePrintf("PydConstHintFirst    %s\n", appOptions.pydConstHintFirst ? "true" : "false");
}

RC LwvidLwOfa::RandomizeInputImages(const UINT32 size, UINT08* pAddr)
{
    RC rc;

    UINT32 loops = (size / sizeof(UINT32));
    for (UINT32 index = 0; index < loops; ++index)
    {
        *((UINT32*)pAddr) = m_pLwvidOfaFpCtx->Rand.GetRandom();
        pAddr += sizeof(UINT32);
    }

    loops = (size % sizeof(UINT32));
    for (UINT32 index = 0; index < loops; ++index)
    {
        *pAddr = (m_pLwvidOfaFpCtx->Rand.GetRandom() >> index) & 0xFF;
        pAddr++;
    }
    return rc;
}

RC LwvidLwOfa::PickUINT32ParamsWithMask
(
    const UINT32 pickerParam,
    const std::map<UINT32, UINT32>& maskMap,
    const UINT32 skipMask,
    const string paramStr,
    UINT32 *pParam
)
{
    RC rc;
    MASSERT(pParam);
    if (!pParam)
        return RC::SOFTWARE_ERROR;

    UINT32 numAttempts = 0;
    FancyPickerArray *fpk = GetFancyPickerArray();

    do
    {
        *pParam = (*fpk)[pickerParam].Pick();
        numAttempts ++;
    } while((maskMap.at(*pParam) & skipMask) >> Utility::BitScanForward(maskMap.at(*pParam)) == 0x1 \
        && (numAttempts < MAX_PICKER_ATTEMPTS));

    if (numAttempts == MAX_PICKER_ATTEMPTS)
    {
        Printf(Tee::PriDebug, "Max attempt reached for %s selecting first non masked value\n", \
            paramStr.c_str());
        // If max attempt is reached, select the first non masked value
        for (std::map<UINT32, UINT32>::const_iterator it = maskMap.begin(); it != maskMap.end(); \
            ++it)
        {
            if ((it->second & skipMask) >> Utility::BitScanForward(it->second) == 0x0)
            {
                *pParam = it->first;
                break;
            }
        }
    }
    return rc;
}

RC LwvidLwOfa::PickBoolParams(const UINT32 pickerParam, const bool isDisabled, bool *pParam)
{
    RC rc;
    MASSERT(pParam);
    if (!pParam)
        return RC::SOFTWARE_ERROR;

    FancyPickerArray *fpk = GetFancyPickerArray();
    if (isDisabled)
    {
        *pParam = false;
    }
    else
    {
        *pParam = (*fpk)[pickerParam].Pick();
    }
    return rc;
}

RC LwvidLwOfa::RandomizeInputParams
(
    appOptionsParS *pTestOptions,
    TestIOParameters *pTestParams
)
{
    RC rc;
    FancyPickerArray *fpk = GetFancyPickerArray();

    pTestOptions->lwrrImage[0]        = "";
    pTestOptions->refImage[0]         = "";
    pTestOptions->hintMv[0][0]        = "";
    pTestOptions->epiInfo             = "";
    pTestOptions->outFlow             = "";
    pTestOptions->outPath             = "./dump";

    PickUINT32ParamsWithMask(FPK_LWVIDLWOFA_OFAMODE, m_ModeToMask, m_SkipModeMask, \
        "Mode", &pTestOptions->ofaMode);
    pTestOptions->sgmP1[0]          = 6;
    pTestOptions->sgmP2[0]          = 32;
    pTestOptions->sgmdP1[0]         = 6;
    pTestOptions->sgmdP2[0]         = 32;
    pTestOptions->calibFile         = "";
    pTestOptions->rlSearch          = false;
    PickUINT32ParamsWithMask(FPK_LWVIDLWOFA_PASS_NUM, m_PassNumToMask, m_SkipPassNumMask, \
        "PassNum", reinterpret_cast<UINT32*>(&pTestOptions->passNum[0]));
    pTestOptions->pydZeroHint       = 1;
    PickBoolParams(FPK_LWVIDLWOFA_ENDIAGONAL, m_DisableDiagonal, &pTestOptions->enDiagonal[0]);
    if (pTestOptions->ofaMode == PYDSGM)
    {
        PickUINT32ParamsWithMask(FPK_LWVIDLWOFA_PYD_NUM, m_PydNumToMask, m_SkipPydNumMask, \
            "PydNum", reinterpret_cast<UINT32*>(&pTestOptions->pydNum));
    }
    else
    {
        pTestOptions->pydNum        = PYDNUM1;
    }
    pTestOptions->ndisp             = 256;
    pTestOptions->width             = (*fpk)[FPK_LWVIDLWOFA_IMAGE_WIDTH].Pick();
    pTestOptions->height            = (*fpk)[FPK_LWVIDLWOFA_IMAGE_HEIGHT].Pick();
    PickBoolParams(FPK_LWVIDLWOFA_ADAPTIVEP2, m_DisableAdaptiveP2, &pTestOptions->adaptiveP2[0]);
    pTestOptions->alphaLog2[0]      = 0;
    PickUINT32ParamsWithMask(FPK_LWVIDLWOFA_GRID_SIZE_LOG2, m_GridSizeLog2ToMask, \
        m_SkipGridSizeLog2Mask, "Gridsize",
        reinterpret_cast<UINT32*>(&pTestOptions->xGridLog2[0]));
    pTestOptions->yGridLog2[0]      = pTestOptions->xGridLog2[0];
    pTestOptions->pydConstCost[0]   = 5;
    pTestOptions->pydConstCostEn    = false;

    pTestOptions->disableCostOut    = false;
    pTestOptions->subPixelRefine    = true;
    pTestOptions->epiSearchRange    = 256;
    PickUINT32ParamsWithMask(FPK_LWVIDLWOFA_IMAGE_BITDEPTH, m_BitDepthToMask,
        m_SkipBitDepthMask, "BitDepth", reinterpret_cast<UINT32*>(&pTestOptions->bitDepth));
    PickBoolParams(FPK_LWVIDLWOFA_SUBFRAMEMODE, m_DisableSubFrameMode,
        &pTestOptions->subframeMode);
    PickBoolParams(FPK_LWVIDLWOFA_ROI_MODE, m_DisableROIMode, &pTestOptions->roiMode);
    PickBoolParams(FPK_LWVIDLWOFA_PYD_LEVEL_FILTER_EN, m_DisablePydLevelFilter,
        &pTestOptions->enablePydLevelFilter);
    if (pTestOptions->enablePydLevelFilter)
    {
        pTestOptions->pydLevelFilterType    = (*fpk)[FPK_LWVIDLWOFA_PYD_FILTER_TYPE].Pick();
        pTestOptions->pydLevelFilterSigma   = (*fpk)[FPK_LWVIDLWOFA_PYD_FILTER_SIGMA].FPick();

        for (UINT32 pydLvl = 0; pydLvl < MAX_PYD_LEVEL; pydLvl++)
        {
            pTestOptions->pydLevelFilterKsize[pydLvl] =
                (*fpk)[FPK_LWVIDLWOFA_PYD_FILTER_K_SIZE].Pick();
        }
    }

    pTestOptions->pconf                 = false;
    pTestOptions->pydDepthHintSelect    = false;
    pTestOptions->tempBufCompression    = true;
    pTestOptions->resetOnlyPass0        = true;
    pTestOptions->pydConstHintFirst     = false;
    pTestOptions->pydHighPerfMode       = (*fpk)[FPK_LWVIDLWOFA_PYD_HIGH_PERF_MODE].Pick();

    CHECK_RC(ApplyConstraints(pTestOptions, pTestParams));
    CHECK_RC(AddInputImages(pTestOptions));

    return rc;
}

RC LwvidLwOfa::ApplyConstraints(appOptionsParS *pTestOptions, TestIOParameters *pTestParams)
{
    RC rc;
    FancyPickerArray *fpk = GetFancyPickerArray();

    // EPISGM needs the epipolar info to be populated.
    if (pTestOptions->ofaMode == EPISGM)
    {
        RandomizeEpiPolarInfo(pTestParams);
    }

    // ROI mode and Subframemode shouldn't exist together, pick again.
    if (pTestOptions->roiMode && pTestOptions->subframeMode)
    {
        pTestOptions->subframeMode = (*fpk)[FPK_LWVIDLWOFA_SUBFRAMEMODE].Pick();
        pTestOptions->roiMode = !pTestOptions->subframeMode;
    }

    if (pTestOptions->roiMode)
    {
        CHECK_RC(RandomizeROIMode(pTestOptions));
    }
    else if (pTestOptions->subframeMode)
    {
        CHECK_RC(RandomizeSubframeMode(pTestOptions));
    }
    return rc;
}

RC LwvidLwOfa::AddInputImages(appOptionsParS *pTestOptions)
{
    RC rc;
    vector<UINT08> lwrImage;
    vector<UINT08> refImage;

    UINT32 bpp = (pTestOptions->bitDepth > BITDEPTH8) ? 2 : 1;
    UINT32 size = pTestOptions->width * pTestOptions->height * bpp;
    lwrImage.resize(size);
    refImage.resize(size);

    CHECK_RC(RandomizeInputImages(size, &lwrImage[0]));
    CHECK_RC(RandomizeInputImages(size, &refImage[0]));

    if (m_DumpInputImages)
    {
        CHECK_RC(DumpInputImages(*pTestOptions, &lwrImage[0], &refImage[0]));
    }

    m_LwrrImagePyd.assign(1, lwrImage);
    m_RefImagePyd.assign(1, refImage);
    if (pTestOptions->pydNum > 1)
    {
        CHECK_RC(RandomizePyramids(pTestOptions));
    }
    return rc;
}

RC LwvidLwOfa::RandomizeROIMode(appOptionsParS *pTestOptions)
{
    RC rc;
    /*
    ROI supports variable grid size,
    but HW needs the following additional constraints to make it work properly:
    Grid size need to be symmetric, i.e. 1x1, 2x2, 4x4, 8x8
    ROI start x& ROI width should align 32*grid_size_x
    ROI start y should align 8*max(grid_size_y, 2); ROI height align to 8*grid_size_y
    ROI width >= 32 &&ROI height >= 16; maximum size 8192x8192
     */
    // width/(32 + pow(2, xGridLog2[0]))
    UINT32 roiXLimit = pTestOptions->width >> (5 + pTestOptions->xGridLog2[0]);
    // height/(16 + pow(2, xGridLog2[0])) Height should align to 8 * max(gridsizey, 2)
    UINT32 roiYLimit = pTestOptions->height >> (4 + pTestOptions->yGridLog2[0]);
    while (roiXLimit == 0 || roiYLimit == 0)
    {
        // Grid size needs to be symmetrical always
        pTestOptions->xGridLog2[0] -= 1;
        pTestOptions->yGridLog2[0] = pTestOptions->xGridLog2[0];
        roiXLimit = pTestOptions->width >> (5 + pTestOptions->xGridLog2[0]);
        roiYLimit = pTestOptions->height >> (4 + pTestOptions->yGridLog2[0]);

        if (pTestOptions->xGridLog2[0] < 0)
        {
            Printf(Tee::PriError, "Grid sizes shouldn't be less than 0\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    pTestOptions->roiPosXStartDiv32 = m_pLwvidOfaFpCtx->Rand.GetRandom(0, roiXLimit - 1);
    pTestOptions->roiPosYStartDiv8 = m_pLwvidOfaFpCtx->Rand.GetRandom(0, roiYLimit - 1);
    pTestOptions->roiWidthDiv32 = m_pLwvidOfaFpCtx->Rand.GetRandom(1,
        roiXLimit - pTestOptions->roiPosXStartDiv32);
    pTestOptions->roiHeightDiv8 = m_pLwvidOfaFpCtx->Rand.GetRandom(1,
        roiYLimit - pTestOptions->roiPosYStartDiv8);

    // roiPosXStartDiv32 is mentioned in multiple of 32,
    // multiplying by gridsize because it needs to align with 32*gridsize
    pTestOptions->roiPosXStartDiv32 =
        pTestOptions->roiPosXStartDiv32 << pTestOptions->xGridLog2[0];
    pTestOptions->roiPosYStartDiv8 = pTestOptions->roiPosYStartDiv8 << pTestOptions->yGridLog2[0];
    pTestOptions->roiWidthDiv32 =  pTestOptions->roiWidthDiv32 << pTestOptions->xGridLog2[0];
    // ROI height align to 8*grid_size_y.
    // pTestOptions->roiHeightDiv8 is initally callwlated in the multiple of 16.
    // Multiply by 2 to get the value for 8
    pTestOptions->roiHeightDiv8 = pTestOptions->roiHeightDiv8 << (pTestOptions->yGridLog2[0] + 1);
    // TODO remove this once 2660491 is resolved. Test asserts when pydNum is greater than 1
    pTestOptions->pydNum = 1;
    return rc;
}

RC LwvidLwOfa::RandomizeSubframeMode(appOptionsParS *pTestOptions)
{
    RC rc;
    INT32 yGLog2 = pTestOptions->yGridLog2[0];
    // All layers of pyramid should have Y_start multiple of 16 pixels.
    UINT32 subframeYLimit = pTestOptions->height >> (4 + yGLog2 + pTestOptions->pydNum - 1);
    while (subframeYLimit == 0)
    {
        yGLog2 -= 1;
        if (yGLog2 < 0)
        {
            yGLog2 = 0;
            pTestOptions->pydNum -= 1;
            VerbosePrintf("YGridLog2 falls below 0, resetting to 0,"
                " decreasing pydnum by 1 to %d\n", pTestOptions->pydNum);
            if (pTestOptions->pydNum < 1)
            {
                Printf(Tee::PriError, "PydNum shouldn't be less than 1."
                    " Please check the image sizes\n");
                return RC::SOFTWARE_ERROR;
            }
        }
        subframeYLimit = pTestOptions->height >> (4 + yGLog2 + pTestOptions->pydNum - 1);
    }
    pTestOptions->subframeYStartDiv16 = m_pLwvidOfaFpCtx->Rand.GetRandom(0, subframeYLimit - 1);
    pTestOptions->subframeHeightDiv16 = m_pLwvidOfaFpCtx->Rand.GetRandom(1,
        subframeYLimit - pTestOptions->subframeYStartDiv16);
    pTestOptions->subframeYStartDiv16 =
        pTestOptions->subframeYStartDiv16 << (yGLog2 + pTestOptions->pydNum - 1);
    pTestOptions->subframeHeightDiv16 =
        pTestOptions->subframeHeightDiv16 << (yGLog2 + pTestOptions->pydNum - 1);
    pTestOptions->xGridLog2[0] = yGLog2;
    pTestOptions->yGridLog2[0] = yGLog2;
    return rc;
}

void LwvidLwOfa::RandomizeEpiPolarInfo(TestIOParameters *pTestParams)
{
    for (UINT32 outerLoop = 0; outerLoop < 3; outerLoop++)
    {
        for (UINT32 innerLoop = 0; innerLoop < 3; innerLoop++)
        {
            // Defined drivers/multimedia/OpticalFlow/OFAPI/interface/lwOpticalFlowPrivate.h
            pTestParams->epipolarInfo.F_Matrix[outerLoop][innerLoop] =
                m_pLwvidOfaFpCtx->Rand.GetRandomFloat(FLT_MIN, FLT_MAX);
            pTestParams->epipolarInfo.H_Matrix[outerLoop][innerLoop] =
                m_pLwvidOfaFpCtx->Rand.GetRandomFloat(FLT_MIN, FLT_MAX);
        }
    }
    pTestParams->epipolarInfo.epipole_x = m_pLwvidOfaFpCtx->Rand.GetRandom();
    pTestParams->epipolarInfo.epipole_y = m_pLwvidOfaFpCtx->Rand.GetRandom();
    pTestParams->epipolarInfo.direction = static_cast<short>(m_pLwvidOfaFpCtx->Rand.GetRandom());
}

RC LwvidLwOfa::DumpInputImages
(
    const appOptionsParS &testOptions,
    void *plwrImage,
    void *pRefImage
)
{
    RC rc;
    UINT32 bpp = (testOptions.bitDepth > BITDEPTH8) ? 2 : 1;
    string fileName = Utility::StrPrintf("%sLwrrImgLoop%d.png", GetName().c_str(),
                m_pLwvidOfaFpCtx->LoopNum);
    ColorUtils::Format memColorFormat = ColorUtils::Format::Y8;
    switch (bpp)
    {
        case 1:
            memColorFormat = ColorUtils::Format::Y8;
            break;
        case 2:
            memColorFormat = ColorUtils::Format::R16;
            break;
        default:
            Printf(Tee::PriError, "BPP %d is not supported!\n", bpp);
            return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(ImageFile::WritePng(fileName.c_str(), memColorFormat, plwrImage,
        testOptions.width, testOptions.height, testOptions.width * bpp, false, false));
    fileName = Utility::StrPrintf("%sRefImgLoop%d.png", GetName().c_str(),
                m_pLwvidOfaFpCtx->LoopNum);
    CHECK_RC(ImageFile::WritePng(fileName.c_str(), memColorFormat, pRefImage,
        testOptions.width, testOptions.height, testOptions.width * bpp, false, false));
    return rc;
}

RC LwvidLwOfa::RandomizePyramids(appOptionsParS *pTestOptions)
{
    RC rc;
    UINT32 pydWidth = (pTestOptions->width + 1) >> 1;
    UINT32 pydHeight = (pTestOptions->height + 1) >> 1;
    UINT32 pydNumAllowed = 1;
    UINT32 bpp = (pTestOptions->bitDepth > BITDEPTH8) ? 2 : 1;
    vector<UINT08> lwrImage;
    vector<UINT08> refImage;

    for (UINT32 loop = 1; loop < static_cast<UINT32>(pTestOptions->pydNum); loop++)
    {
        if (pydWidth < 32 || pydHeight < 32)
        {
            VerbosePrintf("pydNum %d can't be used for image width %d height %d\n",
                pTestOptions->pydNum, pTestOptions->width, pTestOptions->height);
            VerbosePrintf("Resetting pydNum to %d\n", pydNumAllowed);
            pTestOptions->pydNum = pydNumAllowed;
            break;
        }

        pTestOptions->lwrrImage[loop]       = "";
        pTestOptions->refImage[loop]        =  "";
        pTestOptions->passNum[loop]         = pTestOptions->passNum[loop - 1];
        pTestOptions->enDiagonal[loop]      = pTestOptions->enDiagonal[loop - 1];
        pTestOptions->sgmP1[loop]           = pTestOptions->sgmP1[loop - 1];
        pTestOptions->sgmP2[loop]           = pTestOptions->sgmP2[loop - 1];
        pTestOptions->sgmdP1[loop]          = pTestOptions->sgmdP1[loop - 1];
        pTestOptions->sgmdP2[loop]          = pTestOptions->sgmdP2[loop - 1];
        pTestOptions->adaptiveP2[loop]      = pTestOptions->adaptiveP2[loop - 1];
        pTestOptions->alphaLog2[loop]       = pTestOptions->alphaLog2[loop - 1];
        pTestOptions->pydConstCost[loop]    = pTestOptions->pydConstCost[loop - 1];

        lwrImage.clear();
        refImage.clear();
        UINT32 pydSize = pydWidth * pydHeight * bpp;
        lwrImage.resize(pydSize);
        refImage.resize(pydSize);
        CHECK_RC(RandomizeInputImages(pydSize, &lwrImage[0]));
        CHECK_RC(RandomizeInputImages(pydSize, &refImage[0]));
        m_LwrrImagePyd.push_back(lwrImage);
        m_RefImagePyd.push_back(refImage);
        pydNumAllowed++;

        // Reduce the width and height for next pyramid level
        pydWidth = (pydWidth + 1) >> 1;
        pydHeight = (pydHeight + 1) >> 1;
    }
    return rc;
}

RC LwvidLwOfa::ValidateTestArgs()
{
    RC rc;

    CHECK_RC(ValidateInputRanges(m_MaxImageWidth, MIN_IMAGE_WIDTH, MAX_IMAGE_WIDTH,
        "MaxImageWidth"));
    CHECK_RC(ValidateInputRanges(m_MaxImageHeight, MIN_IMAGE_HEIGHT, MAX_IMAGE_HEIGHT,
        "MaxImageHeight"));
    CHECK_RC_MSG(Utility::ParseIndices(m_SkipLoops, &m_SkipLoopsIndices),
        Utility::StrPrintf("Invalid SkipLoops testarg: %s\n", m_SkipLoops.c_str()).c_str());

    return rc;
}

RC LwvidLwOfa::ValidateInputRanges(UINT32 value, UINT32 min, UINT32 max, const string& valueStr)
{
    if (value < min || value > max)
    {
        Printf(Tee::PriError, "%s %u should be within the range, %u and %u\n",
            valueStr.c_str(), value, min, max);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    return RC::OK;
}

CLASS_PROP_READWRITE(LwvidLwOfa, DumpFlowFile, bool,
                    "Dump flow files if this flag is set");
CLASS_PROP_READWRITE(LwvidLwOfa, DumpOutput, bool,
                    "Dump the output flow vector surface if this flag is set");
CLASS_PROP_READWRITE(LwvidLwOfa, DumpCostFile, bool,
                    "Dump cost files if this flag is set");
CLASS_PROP_READWRITE(LwvidLwOfa, DumpInputImages, bool,
                    "Dump the random input images if this flag is set");
CLASS_PROP_READWRITE(LwvidLwOfa, SkipLoops, string, "Indices of the loops to skip");
CLASS_PROP_READWRITE(LwvidLwOfa, KeepRunning, bool, "Keep the test running in the background");
CLASS_PROP_READWRITE(LwvidLwOfa, DisableROIMode, bool, "Disable ROI mode if this flag is set");
CLASS_PROP_READWRITE(LwvidLwOfa, DisableSubFrameMode, bool,
                    "Disable Subframe mode if this flag is set");
CLASS_PROP_READWRITE(LwvidLwOfa, DisableDiagonal, bool,
                    "Disable Diagonal mode if this flag is set");
CLASS_PROP_READWRITE(LwvidLwOfa, DisableAdaptiveP2, bool,
                    "Disable adaptivep2 if this flag is set");
CLASS_PROP_READWRITE(LwvidLwOfa, SkipModeMask, UINT32, "Mask to skip specific modes");
CLASS_PROP_READWRITE(LwvidLwOfa, SkipBitDepthMask, UINT32, "Mask to skip specific bitdepth");
CLASS_PROP_READWRITE(LwvidLwOfa, SkipGridSizeLog2Mask, UINT32,
                    "Mask to skip specific grid size log2 value");
CLASS_PROP_READWRITE(LwvidLwOfa, SkipPassNumMask, UINT32, "Mask to skip specific passnum value");
CLASS_PROP_READWRITE(LwvidLwOfa, SkipPydNumMask, UINT32, "Mask to skip specific pydnum value");
CLASS_PROP_READWRITE(LwvidLwOfa, MaxImageWidth, UINT32, "Maximum image width to use");
CLASS_PROP_READWRITE(LwvidLwOfa, MaxImageHeight, UINT32, "Maximum image height to use");
