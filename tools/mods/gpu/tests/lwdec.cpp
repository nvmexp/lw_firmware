/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/channel.h"
#include "core/include/color.h"
#include "core/include/display.h"
#include "core/include/fpicker.h"
#include "core/include/memtypes.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/tar.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "core/include/utility.h"
#include "ctr64encryptor.h"
#include "gpu/include/gcximpl.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/nfysema.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/perf/pwrwait.h"
#include "gpu/utility/surf2d.h"
#include "gputest.h"
#include "gputestc.h"
#include "lwos.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

#include "ctrl/ctrl0080.h"
#include "class/cla0b0.h"
#include "class/clb0b0.h"
#include "class/clb6b0.h"
#include "class/clc0b0.h"
#include "class/clc3b0.h"
#include "class/clc4b0.h"
#include "class/clc6b0.h"
#include "class/clc7b0.h"
#include "class/clb8b0.h"

#include "gpu/tests/lwencoders/cla0b0engine.h"
#include "gpu/tests/lwencoders/clb0b0engine.h"
#include "gpu/tests/lwencoders/clb6b0engine.h"
#include "gpu/tests/lwencoders/clc0b0socengine.h"
#include "gpu/tests/lwencoders/clc1b0engine.h"
#include "gpu/tests/lwencoders/clc2b0engine.h"
#include "gpu/tests/lwencoders/clc4b0engine.h"

#include "gpu/tests/lwencoders/h264parser.h"
#include "gpu/tests/lwencoders/h26xparser.h"
#include "gpu/tests/lwencoders/h265parser.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <vector>

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/foreach.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/fusion/include/find_if.hpp>
#include <boost/fusion/include/make_vector.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/mpl/int.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/count_if.hpp>
#include <boost/range/algorithm/generate.hpp>
#include <boost/range/combine.hpp>
#include <boost/type_traits/is_same.hpp>

namespace fusion = boost::fusion;
namespace mpl = boost::mpl;
namespace ba = boost::adaptors;

// Compile time map between the engine class id and C++ class.
// `PhysEngine::PhysEngine` will use it to create a C++ class that supports the
// engine.
typedef tuple<
    tuple<mpl::int_<LWA0B0_VIDEO_DECODER>, ClA0B0Engine *>
  , tuple<mpl::int_<LWB0B0_VIDEO_DECODER>, ClB0B0Engine *>
  , tuple<mpl::int_<LWB6B0_VIDEO_DECODER>, ClB6B0Engine *>
  , tuple<mpl::int_<LWB8B0_VIDEO_DECODER>, ClB6B0Engine *>
  , tuple<mpl::int_<LWC0B0_VIDEO_DECODER>, ClC0B0SocEngine *>
  , tuple<mpl::int_<LWC1B0_VIDEO_DECODER>, ClC1B0Engine *>
  , tuple<mpl::int_<LWC2B0_VIDEO_DECODER>, ClC2B0Engine *>
  , tuple<mpl::int_<LWC3B0_VIDEO_DECODER>, ClC2B0Engine *>
  , tuple<mpl::int_<LWC4B0_VIDEO_DECODER>, ClC4B0Engine *>
  , tuple<mpl::int_<LWC6B0_VIDEO_DECODER>, ClC4B0Engine *>
  , tuple<mpl::int_<LWC7B0_VIDEO_DECODER>, ClC4B0Engine *>
  > EngineImpls;

// The following three functions allow to call PhysEngine constructor (below)
// with the same set of arguments for all LWDEC engine HW classes:
// PhysEngine(
//     engineIdx,
//     testConfig,
//     gpuDevice,
//     lwdecAlloc,
//     goldValues,
//     gcxBubble,
//     verbosePrintPri,
//     memLocation,
//     frontEndSemaphore
// )
// The order of the last three arguments is arbitrary. PhysEngine::PhysEngine
// will pick up the correct arguments that a ClXXXXEngine class needs.

// Takes `NewArgs...` from the tuple `v` and packs them into a new tuple.
template <typename V, typename... NewArgs>
auto RepackTuple(V &&v, const tuple<NewArgs...> &)
{
    return make_tuple(*fusion::find<NewArgs>(forward<V>(v))...);
}

// Unfolds `from` into a parameters list and passes it to the constructor of `Obj`.
template <typename Obj, typename Tuple, size_t... idx>
Obj* ConstructFromTupleHelper(Tuple &&from, index_sequence<idx...>)
{
    return new Obj(get<idx>(forward<Tuple>(from))...);
}

// Constructs `Obj` from selected parts of `from`. `args` lists the arguments to select and pass
// to the constructor of `Obj`.
template <typename Obj, typename V, typename... Args>
Obj* ConstructFromTuple(V &&from, const tuple<Args...> &args)
{
    return ConstructFromTupleHelper<Obj>(
        RepackTuple(forward<V>(from), args),
        make_index_sequence<sizeof...(Args)>()
    );
}

class PhysEngine
{
public:
    typedef vector<unique_ptr<LWDEC::Engine>> Streams;

    template <typename... Args>
    PhysEngine
    (
        UINT32 engineId,
        GpuTestConfiguration &testConfig,
        GpuDevice &bndDev,
        LwdecAlloc &lwdecAlloc,
        Goldelwalues *goldValues,
        unique_ptr<GCxBubble> gcxBubble,
        bool supportsEncryption,
        Args... args // extra arguments to pass to the engine constructors
    ) : m_GoldValues(*goldValues) // Make a copy of golden values object. We need a copy per engine,
                                  // because if run_on_error is on, an error in CRC is delayed until
                                  // `Goldelwalues::ErrorRateTest` exelwtion. If we don't have a
                                  // copy per engine instance, at the moment of
                                  // `Goldelwalues::ErrorRateTest`, we have one `GpuTest::m_Golden`
                                  // with no information what engine instance has failed.
      , m_LwCh(nullptr, bind(&GpuTestConfiguration::FreeChannel, &testConfig, placeholders::_1))
      , m_EngineId(engineId)
      , m_SupportsEncription(supportsEncryption)
      , m_GCxBubble(move(gcxBubble))
    {
        Channel *ch;
        if (OK != testConfig.AllocateChannelWithEngine(&ch,
                                                       &m_LwChHandle,
                                                       &lwdecAlloc,
                                                       engineId))
        {
            return;
        }
        m_LwCh.reset(ch);

        const auto supClass = lwdecAlloc.GetSupportedClass(&bndDev);

        bool found = false;
        fusion::for_each(EngineImpls(), [&](auto v)
        {
            if (found)
            {
                return;
            }
            // the first type of the compile time map is the engine class id.
            typedef tuple_element_t<0, decltype(v)> ClassId;
            if (ClassId::value == supClass)
            {
                // the second type of the compile time map is engine C++ class.
                typedef remove_pointer_t<decay_t<
                    tuple_element_t<1, decltype(v)>
                  >> Engine;
                typedef typename Engine::ConstructorArgs ConstructorArgs;
                m_LwdecEngine.reset(ConstructFromTuple<Engine>(
                    make_tuple(ch, lwdecAlloc.GetHandle(), args...),
                    ConstructorArgs()
                ));
                if (OK == m_LwdecEngine->InitChannel())
                {
                    found = true;
                }
                else
                {
                    m_LwdecEngine.reset();
                }
            }
        });

        m_InDataGoldSurfs.reset(new GpuGoldenSurfaces(&bndDev));
        m_OutDataGoldSurfs.reset(new GpuGoldenSurfaces(&bndDev));
        m_HistDataGoldSurfs.reset(new GpuGoldenSurfaces(&bndDev));
    }

    RC AddStream
    (
        const TarArchive &fileArchive,
        const string &fileName,
        const char *goldSufix,
        bool checkEncryption,
        LwDecCodec codec,
        GpuSubdevice *subDev
    )
    {
        RC rc;

        const TarFile *tarFile = fileArchive.Find(fileName);

        if (nullptr == tarFile)
        {
            Printf(Tee::PriHigh, "Error loading %s\n", fileName.c_str());
            return RC::FILE_DOES_NOT_EXIST;
        }
        unique_ptr<LWDEC::Engine> newLwdec(m_LwdecEngine->Clone());
        newLwdec->SetEncryption(checkEncryption);
        switch (codec)
        {
            case LwDecCodec::H264:
                rc = newLwdec->InitFromH264File(fileName.c_str(), tarFile, subDev->GetParentDevice(),
                                                subDev, m_LwChHandle);
                break;
            case LwDecCodec::H265:
                rc = newLwdec->InitFromH265File(fileName.c_str(), tarFile, subDev->GetParentDevice(),
                                                subDev, m_LwChHandle);
                break;
        }
        if (OK == rc)
        {
            newLwdec->SetStreamGoldSuffix(goldSufix);
            m_Streams.push_back(move(newLwdec));
        }
        else if (RC::UNSUPPORTED_FUNCTION == rc)
        {
            // the codec is just not supported, it's not an error
            rc.Clear();
        }

        return rc;
    }

    RC DuplicateStreams
    (
        GpuSubdevice *subDev,
        PhysEngine &from
    )
    {
        RC rc;

        m_Streams.clear();
        for (auto &strm : from.m_Streams)
        {
            unique_ptr<LWDEC::Engine> newLwdec(m_LwdecEngine->Clone());
            newLwdec->SetEncryption(m_SupportsEncription && strm->GetEncryption());
            CHECK_RC(newLwdec->InitFromAnother(subDev->GetParentDevice(), subDev, m_LwChHandle, *strm));
            newLwdec->SetStreamGoldSuffix(strm->GetStreamGoldSuffix());
            m_Streams.push_back(move(newLwdec));
        }

        return rc;
    }

    template <typename ActivateBubblyGen, typename DelayGen>
    RC Run
    (
        bool saveFrames,
        GpuDevice *dev,
        GpuSubdevice *subDev,
        FLOAT64 skTimeout,
        void *pMutex,
        ActivateBubblyGen &&actFun,
        DelayGen &&delayFun
    )
    {
        RC rc;

        Streams::iterator it;
        for (it = m_Streams.begin(); m_Streams.end() != it; (*it)->Free(), ++it)
        {
            CHECK_RC((*it)->InitOutput(dev, subDev, m_LwChHandle, skTimeout));
            CHECK_RC((*it)->UpdateInDataGoldens(m_InDataGoldSurfs.get(), &m_GoldValues));

            size_t numPics = (*it)->GetNumPics();
            for (size_t i = 0; numPics > i; ++i)
            {
                if (saveFrames)
                {
                    // save frames that are marked for output
                    auto s = (*it)->GetInterface<LWDEC::Saver>();
                    CHECK_RC(s->SaveFrames("eng%i-%f-%c-%n.png", GetEngineInstance()));
                }
                // update goldens from the frames that are marked for output
                CHECK_RC((*it)->UpdateOutPicsGoldens(m_OutDataGoldSurfs.get(), &m_GoldValues));
                CHECK_RC((*it)->PrintInputData());
                CHECK_RC((*it)->DecodeOneFrame(skTimeout));

                if (m_GCxBubble)
                {
                    Tasker::WaitOnBarrier();
                    if (actFun() && m_GoldValues.GetAction() != Goldelwalues::Store)
                    {
                        Tasker::MutexHolder mh(pMutex);
                        CHECK_RC(m_GCxBubble->ActivateBubble(delayFun()));
                    }
                    Tasker::WaitOnBarrier();
                }

                CHECK_RC((*it)->UpdateHistGoldens(m_HistDataGoldSurfs.get(), &m_GoldValues));
                CHECK_RC((*it)->PrintHistogram());
            }
            if (saveFrames)
            {
                // save frames left in DPB
                auto s = (*it)->GetInterface<LWDEC::Saver>();
                CHECK_RC(s->SaveFrames("eng%i-%f-%c-%n.png", GetEngineInstance()));
            }
            CHECK_RC((*it)->UpdateOutPicsGoldens(m_OutDataGoldSurfs.get(), &m_GoldValues));
        }

        if (m_GCxBubble)
        {
            m_GCxBubble->PrintStats();
        }

        return rc;
    }

    UINT32 GetEngineInstance() const
    {
        return LW2080_ENGINE_TYPE_LWDEC_IDX(m_EngineId);
    }

    auto GetGoldelwalues()
    {
        return &m_GoldValues;
    }

    auto GetGoldelwalues() const
    {
        return &m_GoldValues;
    }

private:
    typedef decltype
    (
        bind
        (
            &GpuTestConfiguration::FreeChannel,
            declval<GpuTestConfiguration *>(),
            placeholders::_1
        )
    ) ChannelDeleter;

    Goldelwalues                  m_GoldValues;
    unique_ptr<GpuGoldenSurfaces> m_InDataGoldSurfs;
    unique_ptr<GpuGoldenSurfaces> m_OutDataGoldSurfs;
    unique_ptr<GpuGoldenSurfaces> m_HistDataGoldSurfs;

    unique_ptr<Channel, ChannelDeleter> m_LwCh;
    LwRm::Handle                        m_LwChHandle = 0;
    UINT32                              m_EngineId = LW2080_ENGINE_TYPE_LWDEC(0);
    Streams                             m_Streams;
    unique_ptr<LWDEC::Engine>           m_LwdecEngine; // a primer for cloning
    bool                                m_SupportsEncription = true;

    unique_ptr<GCxBubble>  m_GCxBubble;
};

class LWDECTest : public GpuTest
{
public:
    LWDECTest();

    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;

    bool IsSupported() override;
    RC SetDefaultsForPicker(UINT32 Idx) override;

    SETGET_PROP(DisableEncryption, bool);
    SETGET_PROP(SaveFrames, bool);
    SETGET_PROP(FESemOnPascal, bool);
    SETGET_PROP(DoGCxCycles, UINT32);
    SETGET_PROP(ForceGCxPstate, UINT32);
    SETGET_PROP(EngineSkipMask, UINT32);
    SETGET_PROP(EngEncryptionMask, UINT32);
    SETGET_PROP(RunParallel, bool);

private:
    static constexpr size_t numH264Streams = 3;
    static constexpr size_t numH265Streams = 2;

    // Creates an array property `Engines` in the JS counterpart, then calls
    // `Goldelwalues::ErrorRateTest` for each element of `Engines`.
    RC ErrorRateTest();

    static void RunThreadDispatch(void *thisPtr)
    {
        static_cast<LWDECTest *>(thisPtr)->RunThread();
    }

    void RunThread();

    vector<RC>             m_EngResults;

    LwdecAlloc             m_lwdecAlloc;

    GpuTestConfiguration  *m_testConfig = nullptr;

    static const char*     archiveFileName;

    static const char*     h264FileNames[numH264Streams];
    static const char*     h264GoldenSuffixes[numH264Streams];
    static const bool      h264CheckEncryption[numH264Streams];

    static const char*     h265FileNames[numH265Streams];
    static const char*     h265GoldenSuffixes[numH265Streams];

    static const char*     shortStreamForSim;
    static const char*     shortStreamForSimSuffix;

    bool                   m_DisableEncryption = false;
    bool                   m_SaveFrames = false;
    bool                   m_FESemOnPascal = false;
    vector<size_t>         m_H264Streams;
    vector<size_t>         m_H265Streams;

    UINT32                 m_DoGCxCycles = GCxPwrWait::GCxModeDisabled;
    UINT32                 m_ForceGCxPstate = Perf::ILWALID_PSTATE;
    FancyPickerArray      *m_pPickers;
    UINT32                 m_EngineSkipMask = 0;
    UINT32                 m_EngEncryptionMask = 1;
    bool                   m_RunParallel = true;

    vector<UINT32>         m_ChannelClasses;
    vector<PhysEngine>     m_PhysEngines;
    Tasker::Mutex          m_pMutex;
};

JS_CLASS_INHERIT(LWDECTest, GpuTest, "LWDEC test");
CLASS_PROP_READWRITE(LWDECTest, SaveFrames, bool, "Save decoded frames");
CLASS_PROP_READWRITE(LWDECTest, FESemOnPascal, bool, "Use frontend semaphore on Pascal");
CLASS_PROP_READWRITE(LWDECTest, DisableEncryption, bool, "Do not encrypt the input stream and do"
                                                         " not test the decryption feature.");
CLASS_PROP_READWRITE(LWDECTest, DoGCxCycles, UINT32,
                     "Perform GCx cycles (0: disable, 1: GC5, 2: GC6, 3: RTD3).");
CLASS_PROP_READWRITE(LWDECTest, ForceGCxPstate, UINT32,
                     "Force to pstate prior to GCx entry.");
CLASS_PROP_READWRITE(LWDECTest, EngineSkipMask, UINT32,
                     "A mask to skip LWDEC engines from testing. Bit 1 in position N will direct "
                     "the test to skip engine N (default = 0)");
CLASS_PROP_READWRITE(LWDECTest, EngEncryptionMask, UINT32,
                     "A mask of engines that support encryption (default = 1)");
CLASS_PROP_READWRITE(LWDECTest, RunParallel, bool,
                     "Test all the LWDEC engines in parallel or sequentially.");

const char* LWDECTest::h264FileNames[numH264Streams] =
{
    "in_to_tree-001.264", // 4K UHD (2160p) resolution
    "sunflower-007.264",  // HD resolution CABAC
    "sunflower-008.264",  // HD resolution CAVLC
};
const char* LWDECTest::h264GoldenSuffixes[numH264Streams] =
{
    "-H.264-in-to-tree",
    "-H.264-sunflower-stream-007",
    "-H.264-sunflower-stream-008",
};
const bool LWDECTest::h264CheckEncryption[numH264Streams] =
{
    true, true, true
};
const char* LWDECTest::archiveFileName = "lwdec.bin";

const char* LWDECTest::h265FileNames[numH265Streams] =
{
    "old-town-cross-001.265",
    "in_to_tree-002.265"
};
const char* LWDECTest::h265GoldenSuffixes[numH265Streams] =
{
    "-H.265-old-town-cross",
    "-H.265-in-to-tree"
};

const char* LWDECTest::shortStreamForSim = "bboxes-001.264";
const char* LWDECTest::shortStreamForSimSuffix = "bboxes-001";

LWDECTest::LWDECTest()
  : m_testConfig(GetTestConfiguration())
  , m_pMutex(nullptr, &Tasker::FreeMutex)
{
    SetName("LWDECTest");

    // On Linux on CheetAh, we cannot test encryption, due to the lack of the
    // secure OS.  But secure OS is available on Android.
    // The TEGRA_MODS define indicates that we're compiling MODS for CheetAh,
    // either Linux or Android, while the BIONIC define indicates that
    // we're compiling MODS for Android.
#if defined(TEGRA_MODS) && ! defined(BIONIC)
    m_DisableEncryption = true;
#endif

    m_lwdecAlloc.SetNewestSupportedClass(LWC4B0_VIDEO_DECODER);
    CreateFancyPickerArray(FPK_LWDEC_GCX_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
}

RC LWDECTest::SetDefaultsForPicker(UINT32 idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &fp = (*pPickers)[idx];

    switch (idx)
    {
        case FPK_LWDEC_GCX_ACTIVATE_BUBBLE:
            fp.ConfigRandom();
            fp.AddRandItem(60, true);
            fp.AddRandItem(40, false);
            fp.CompileRandom();
            break;
        case FPK_LWDEC_GCX_PWRWAIT_DELAY_MS:
            fp.ConfigRandom();
            fp.AddRandRange(80, 6, 25);
            fp.AddRandRange(20, 25, 100);
            fp.CompileRandom();
            break;
    }
    return OK;
}

RC LWDECTest::Setup()
{
    RC rc;

    unique_ptr<LWDEC::Engine> lwdec;

    GpuDevice    *gpuDevice = GetBoundGpuDevice();
    GpuSubdevice *gpuSubdevice = GetBoundGpuSubdevice();


    // Range check the m_DoGCxCycles variable.
    if (m_DoGCxCycles > GCxPwrWait::GCxModeRTD3)
    {
        Printf(Tee::PriError,
               "DoGCxCycles=%d is invalid, only %d-%d are valid.\n",
               m_DoGCxCycles, GCxPwrWait::GCxModeDisabled, GCxPwrWait::GCxModeRTD3);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    JavaScriptPtr js;
    JSObject *testObj = GetJSObject();
    JsArray jsArray;
    m_H264Streams.clear();
    if (OK == js->GetProperty(testObj, "H264Streams", &jsArray))
    {
        for (JsArray::const_iterator it = jsArray.begin(); jsArray.end() != it; ++it)
        {
            UINT32 streamNum;
            CHECK_RC(js->FromJsval(*it, &streamNum));
            if (streamNum < numH264Streams)
            {
                m_H264Streams.push_back(streamNum);
            }
        }
    }
    else
    {
        for (size_t i = 0; numH264Streams > i; ++i)
        {
            m_H264Streams.push_back(i);
        }
    }

    m_H265Streams.clear();
    jsArray.clear();
    if (OK == js->GetProperty(testObj, "H265Streams", &jsArray))
    {
        for (JsArray::const_iterator it = jsArray.begin(); jsArray.end() != it; ++it)
        {
            UINT32 streamNum;
            CHECK_RC(js->FromJsval(*it, &streamNum));
            if (streamNum < numH265Streams)
            {
                m_H265Streams.push_back(streamNum);
            }
        }
    }
    else
    {
        for (size_t i = 0; numH265Streams > i; ++i)
        {
            m_H265Streams.push_back(i);
        }
    }

    // We need this since we check for ShortenTestForSim in PrintJsProperties
    InitFromJs();

    const auto supClass = m_lwdecAlloc.GetSupportedClass(gpuDevice);

    // GpuTest::Setup prints out the JS properties of the test class. Thus
    // GpuTest::Setup is called after we filled in m_H245Streams and
    // m_H265Streams above.
    CHECK_RC(GpuTest::Setup());

    // For GCxBubble
    CHECK_RC(GpuTest::AllocDisplayAndSurface(true));

    vector<UINT32> engines;
    CHECK_RC(gpuDevice->GetEnginesFilteredByClass({ supClass }, &engines, GetBoundRmClient()));
    auto newEnd = remove_if(engines.begin(),
                            engines.end(),
                            [this](UINT32 eng)
        { return !!((1 << LW2080_ENGINE_TYPE_LWDEC_IDX(eng)) & m_EngineSkipMask); });
    engines.erase(newEnd, engines.end());
    if (engines.empty())
    {
        Printf(Tee::PriError,
               "The engine mask is set to skip all engines.\n");
        return RC::ILWALID_INPUT;
    }

    // Only generate goldens on the first available engine
    if (GetGoldelwalues()->GetAction() != Goldelwalues::Check)
        engines.resize(1);

    m_testConfig->SetAllowMultipleChannels(true);

    const string lwdecArchive = Utility::DefaultFindFile(archiveFileName, true);
    TarArchive fileArchive;
    if (!fileArchive.ReadFromFile(lwdecArchive, true))
    {
        Printf(Tee::PriError, "Error loading %s\n", archiveFileName);
        return RC::FILE_DOES_NOT_EXIST;
    }

    for (auto lwrEng : engines)
    {
        unique_ptr<GCxBubble> gcxBubble;
        if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
        {
            gcxBubble = make_unique<GCxBubble>(gpuSubdevice);
            gcxBubble->SetGCxParams(
                GetBoundRmClient(),
                GetBoundGpuDevice()->GetDisplay(),
                GetDisplayMgrTestContext(),
                m_ForceGCxPstate,
                GetTestConfiguration()->Verbose(),
                GetTestConfiguration()->TimeoutMs(),
                m_DoGCxCycles);
            CHECK_RC(gcxBubble->Setup());
        }

        bool useFESem = true;
        if (0xC2B0 == supClass)
        {
            useFESem = m_FESemOnPascal;
        }
        if (0xC4B0 == supClass)
        {
            useFESem = false;
        }

        bool useIntraFrame = (supClass == 0xC6B0) || (supClass == 0xC7B0);

        const bool bEngSupportsEncryption =
            (0 != ((1U << LW2080_ENGINE_TYPE_LWDEC_IDX(lwrEng)) & m_EngEncryptionMask));
        m_PhysEngines.emplace_back(
            lwrEng,
           *m_testConfig,
           *gpuDevice,
            m_lwdecAlloc,
            GetGoldelwalues(),
            move(gcxBubble),
            bEngSupportsEncryption,
            GetVerbosePrintPri(),
            m_testConfig->NotifierLocation(),
            useFESem,
            useIntraFrame
        );
        if (1 < m_PhysEngines.size())
        {
            m_PhysEngines.back().DuplicateStreams
            (
                gpuSubdevice,
                m_PhysEngines.front()
            );
        }
        else
        {
            if (m_testConfig->ShortenTestForSim())
            {
                CHECK_RC(m_PhysEngines.back().AddStream(
                    fileArchive,
                    shortStreamForSim,
                    shortStreamForSimSuffix,
                    false,
                    LwDecCodec::H264,
                    gpuSubdevice
                ));
            }
            else
            {
                for (size_t strmIdx = 0; m_H265Streams.size() > strmIdx; ++strmIdx)
                {
                    size_t i = m_H265Streams[strmIdx];
                    CHECK_RC(m_PhysEngines.back().AddStream(
                        fileArchive,
                        h265FileNames[i],
                        h265GoldenSuffixes[i],
                        false,
                        LwDecCodec::H265,
                        gpuSubdevice
                    ));
                }

                for (size_t strmIdx = 0; m_H264Streams.size() > strmIdx; ++strmIdx)
                {
                    size_t i = m_H264Streams[strmIdx];
                    bool checkEncryption = false;
                    if (!m_DisableEncryption && GetGoldelwalues()->GetAction() != Goldelwalues::Store)
                    {
                        // send encrypted stream if both the stream is marked for encryption and
                        // the engine supports encryption
                        checkEncryption = h264CheckEncryption[i] && bEngSupportsEncryption;
                    }
                    CHECK_RC(m_PhysEngines.back().AddStream(
                        fileArchive,
                        h264FileNames[i],
                        h264GoldenSuffixes[i],
                        checkEncryption,
                        LwDecCodec::H264,
                        gpuSubdevice
                    ));
                }
            }
        }
    }

    if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
    {
        m_pMutex.reset(Tasker::AllocMutex("LwdecGCx::m_pMutex", Tasker::mtxUnchecked));
    }
    return rc;
}

void LWDECTest::RunThread()
{
    GpuDevice    *gpuDevice = GetBoundGpuDevice();
    GpuSubdevice *gpuSubdevice = GetBoundGpuSubdevice();

    auto engineIdx = Tasker::GetThreadIdx();
    m_EngResults[engineIdx] = m_PhysEngines[engineIdx].Run(
        m_SaveFrames,
        gpuDevice,
        gpuSubdevice,
        m_testConfig->TimeoutMs(),
        m_pMutex.get(),
        [&]() -> bool
        {
            return (*m_pPickers)[FPK_LWDEC_GCX_ACTIVATE_BUBBLE].Pick() != 0;
        },
        [&]() -> UINT32
        {
            return (*m_pPickers)[FPK_LWDEC_GCX_PWRWAIT_DELAY_MS].Pick();
        }
    );
}

RC LWDECTest::Run()
{
    StickyRC rc;

    m_EngResults.resize(m_PhysEngines.size());

    if (m_RunParallel)
    {
        auto threads = Tasker::CreateThreadGroup
        (
            &RunThreadDispatch,
            this,
            static_cast<UINT32>(m_PhysEngines.size()),
            "LWDEC engines",
            true,
            nullptr
        );
        CHECK_RC(Tasker::Join(threads));
    }
    else
    {
        GpuDevice    *gpuDevice = GetBoundGpuDevice();
        GpuSubdevice *gpuSubdevice = GetBoundGpuSubdevice();
        const bool stopOnError = GetGoldelwalues()->GetStopOnError();
        for (UINT32 engineIdx = 0; engineIdx < m_PhysEngines.size(); engineIdx++)
        {
            m_EngResults[engineIdx] = m_PhysEngines[engineIdx].Run(
                m_SaveFrames,
                gpuDevice,
                gpuSubdevice,
                m_testConfig->TimeoutMs(),
                m_pMutex.get(),
                [&]() -> bool
                {
                    return (*m_pPickers)[FPK_LWDEC_GCX_ACTIVATE_BUBBLE].Pick() != 0;
                },
                [&]() -> UINT32
                {
                    return (*m_pPickers)[FPK_LWDEC_GCX_PWRWAIT_DELAY_MS].Pick();
                }
            );
            if (m_EngResults[engineIdx] != OK && stopOnError)
                break;
        }
    }

    if (!boost::algorithm::all_of_equal(m_EngResults, OK))
    {
        string errMsg;
        for (auto notOkEng : boost::combine(m_PhysEngines, m_EngResults) |
                             ba::filtered([](const auto &e) { return get<1>(e) != OK; }))
        {
            using Utility::StrPrintf;
            auto engInst = get<0>(notOkEng).GetEngineInstance();
            rc = get<1>(notOkEng);
            ErrorMap err(get<1>(notOkEng));
            errMsg += StrPrintf("LWDEC engine %u returned an error: %s\n", engInst, err.Message());
        }
        Printf(Tee::PriError, "%s", errMsg.c_str());
    }

    for (const auto &pe : m_PhysEngines)
    {
        GetGoldelwalues()->Consolidate(*pe.GetGoldelwalues());
    }

    rc = ErrorRateTest();

    return rc;
}

RC LWDECTest::Cleanup()
{
    RC rc = OK;

    m_lwdecAlloc.Free();

    m_PhysEngines.clear();

    m_pMutex.reset();

    CHECK_RC(GpuTest::Cleanup());

    return rc;
}

void LWDECTest::PrintJsProperties(Tee::Priority pri)
{
    using Utility::StrPrintf;

    GpuTest::PrintJsProperties(pri);

    string propStr;

    Printf(pri, "LWDEC test Js Properties:\n");
    Printf(pri, "\tDisableEncryption:\t\t%s\n", m_DisableEncryption ? "true" : "false");
    Printf(pri, "\tSaveFrames:\t\t\t%s\n", m_SaveFrames ? "true" : "false");
    Printf(pri, "\tFESemOnPascal:\t\t\t%s\n", m_FESemOnPascal ? "true" : "false");
    Printf(pri, "\tDoGCxCycles:\t\t\t%u\n", m_DoGCxCycles);
    Printf(pri, "\tRunParallel:\t\t\t%s\n", m_RunParallel ? "true" : "false");
    if (Perf::ILWALID_PSTATE ==  m_ForceGCxPstate)
    {
        Printf(pri, "\tForceGCxPstate:\t\t\tilwalid PState (default)\n");
    }
    else
    {
        Printf(pri, "\tForceGCxPstate:\t\t\t%u\n", m_ForceGCxPstate);
    }

    propStr = "\tH264Streams:\t\t\t{";
    if (m_testConfig->ShortenTestForSim())
    {
        propStr += StrPrintf("%s", shortStreamForSim);
    }
    else
    {
        for (size_t i = 0; m_H264Streams.size() > i; ++i)
        {
            if (0 != i)
            {
                propStr += ", ";
            }
            propStr += StrPrintf("%s", h264FileNames[m_H264Streams[i]]);
        }
    }
    propStr += "}\n";
    Printf(pri, "%s", propStr.c_str());

    propStr = "\tH265Streams:\t\t\t{";
    if (!m_testConfig->ShortenTestForSim())
    {
        for (size_t i = 0; m_H265Streams.size() > i; ++i)
        {
            if (0 != i)
            {
                propStr += ", ";
            }
            propStr += StrPrintf("%s", h265FileNames[m_H265Streams[i]]);
        }
    }
    propStr += "}\n";
    Printf(pri, "%s", propStr.c_str());
    Printf(pri, "\tEngineSkipMask:\t\t\t0x%0x\n", m_EngineSkipMask);
    Printf(pri, "\tEngEncryptionMask:\t\t0x%0x\n", m_EngEncryptionMask);
}

bool LWDECTest::IsSupported()
{
    if (!m_lwdecAlloc.IsSupported(GetBoundGpuDevice()))
        return false;

    if (GetBoundGpuSubdevice()->IsSOC())
    {
        return false;
    }

    if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
    {
        GCxBubble gcxBubble(GetBoundGpuSubdevice());
        if (!gcxBubble.IsGCxSupported(m_DoGCxCycles, m_ForceGCxPstate))
        {
            return false;
        }
    }

    return true;
}

RC LWDECTest::ErrorRateTest()
{
    StickyRC rc;

    auto &js = *JavaScriptPtr();
    JSContext *cx = nullptr;
    CHECK_RC(js.GetContext(&cx));

    JSObject *lwDecObj = GetJSObject();
    JSObject *lwDecEngObj = nullptr;

    jsval engJsval;
    if (!JS_GetProperty(cx, lwDecObj, "Engines", &engJsval) || JSVAL_VOID == engJsval)
    {
        vector<jsval> initVector(m_PhysEngines.size());
        for (size_t i = 0; i < initVector.size(); i++)
        {
            initVector[i] = OBJECT_TO_JSVAL(JS_NewObject(cx, &js_ObjectClass, nullptr, nullptr));
        }
        lwDecEngObj = JS_NewArrayObject(cx, static_cast<jsint>(m_PhysEngines.size()), &initVector[0]);
        engJsval = OBJECT_TO_JSVAL(lwDecEngObj);
        if (!JS_DefineProperty(cx, lwDecObj, "Engines", engJsval, nullptr, nullptr, 0))
        {
            Printf(Tee::PriError, "Unable to create named property Engines\n");
            return RC::CANNOT_COLWERT_STRING_TO_JSVAL;
        }
    }
    else
    {
        JS_ValueToObject(cx, engJsval, &lwDecEngObj);
    }

    string errMsg;
    for (size_t i = 0; m_PhysEngines.size() > i; ++i)
    {
        JSObject *engElemObj;
        CHECK_RC(js.GetElement(lwDecEngObj, static_cast<INT32>(i), &engElemObj));
        CHECK_RC(js.SetProperty(engElemObj, "EngineInstance", m_PhysEngines[i].GetEngineInstance()));

        RC engRc = m_PhysEngines[i].GetGoldelwalues()->ErrorRateTest(engElemObj);
        // don't fail right away, finish rating the test
        rc = engRc;
        if (OK != engRc)
        {
            using Utility::StrPrintf;
            auto engInst = m_PhysEngines[i].GetEngineInstance();
            ErrorMap err(engRc);
            errMsg += StrPrintf("LWDEC engine %u returned an error: %s\n", engInst, err.Message());
        }
    }
    if (OK != rc)
    {
        Printf(Tee::PriError, "%s", errMsg.c_str());
    }

    return rc;
}
