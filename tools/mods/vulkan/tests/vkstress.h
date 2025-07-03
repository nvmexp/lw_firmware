/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#pragma once

#include "vkmain.h"
#include "util.h"
#include "vkimage.h"
#include "vkinstance.h"
#include "vkcmdbuffer.h"
#include "vkbuffer.h"
#include "vktexture.h"
#include "vkpipeline.h"
#include "vkframebuffer.h"
#include "vksampler.h"
#include "vkshader.h"
#include "vkrenderpass.h"
#include "vkdescriptor.h"
#include "swapchain.h"
#include "vksemaphore.h"
#include "vkgoldensurfaces.h"
#include "vkquery.h"
#include "vkutil.h"
#include "glm.hpp"
#include "gtc/matrix_ilwerse.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtx/hash.hpp"
#include "gtx/rotate_vector.hpp"
#include "vulkan/shared_sources/lwmath/lwmath.h"
#include <queue>

#ifndef VULKAN_STANDALONE
#include "core/include/tasker.h"
#include "gpu/tests/gputest.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/pwmutil.h"
#endif

class Raytracer;

//-------------------------------------------------------------------------------------------------
//(see https://lwpro-samples.github.io/vk_raytracing_tutorial/vkrt_tuto_intersection.md.html)
// For how raytracing is implemented in VkStress. The example above is closely followed and the
// raytracing shaders are pulled from that example.


//-------------------------------------------------------------------------------------------------
//! \brief Vulkan based test
//!
class VkStress : public GpuTest
{
public:
    VkStress();
    virtual ~VkStress();
    RC                        AddExtraObjects(JSContext *cx, JSObject *obj);
    RC                        Cleanup() override;
    bool                      IsSupported() override;
    RC                        Run() override;
    RC                        Setup() override;
    RC                        ApplyMaxPowerSettings();

    enum TxMode
    {
        TX_MODE_STATIC    // All textures are the same size (MaxTextureSize)
       ,TX_MODE_INCREMENT // Texture sizes monotonically increase from [MinTextureSize, MaxTextureSize] $
       ,TX_MODE_RANDOM    // Texture sizes are random within range [MinTextureSize, MaxTextureSize]
    };


    enum PulseMode
    {
        NO_PULSE = 0,
        SWEEP = 1,
        SWEEP_LINEAR = 2,
        FMAX = 3,
        TILE_MASK = 4,
        MUSIC = 5,
        RANDOM = 6,
        TILE_MASK_CONSTANT_DUTY = 7,
        TILE_MASK_USER = 8,
        TILE_MASK_GPC = 9,
        TILE_MASK_GPC_SHUFFLE = 10
    };
    enum FMaxPerfTarget
    {
        PERF_TARGET_NONE = 0,
        PERF_TARGET_RELIABILITY = 1
    };

    enum ComputeCmds
    {
        NO_OP = 0,
        INITIALIZE = 1,
        START_ARITHMETIC_OPS = 2,
        STOP_ARITHMETIC_OPS = 3
    };
    enum
    {
        WARP_SIZE = 32,
        MAX_SMS = 144
    };

    GET_PROP(NumFrames,             UINT32);
    SETGET_PROP(FramesPerSubmit, UINT32);
    SETGET_PROP(IndexedDraw,        bool);
    SETGET_PROP(SkipDirectDisplay,  bool);
    SETGET_PROP(SendColor,          bool);
    SETGET_PROP(ApplyFog,           bool);
    SETGET_PROP(ExponentialFog,     bool);
    SETGET_PROP(UseMipMapping,      bool);
    SETGET_PROP(UsePlainColorMipMaps, bool);
    SETGET_PROP(DrawLines,          bool);
    SETGET_PROP(DrawTessellationPoints, bool);
    SETGET_PROP(Ztest,              bool);
    SETGET_PROP(Stencil,            bool);
    SETGET_PROP(ColorLogic,         bool);
    SETGET_PROP(ColorBlend,         bool);
    SETGET_PROP(Debug,              UINT32);
    SETGET_PROP(EnableValidation,   bool);
    SETGET_PROP(EnableErrorLogger,  bool);
    SETGET_PROP(ShaderReplacement,  bool);
    SETGET_PROP(LoopMs,             UINT32);      //
    SETGET_PROP(RuntimeMs,          UINT32);
    SETGET_PROP(SampleCount,        UINT32);
    SETGET_PROP(NumLights,          UINT32);
    SETGET_PROP(NumTextures,        UINT32);
    SETGET_PROP(PpV,                double);
    SETGET_PROP(UseTessellation,    bool);
    SETGET_PROP(UseMeshlets,        bool);
    SETGET_PROP(BoringXform,        bool);
    SETGET_PROP(CameraScaleFactor,  FLOAT32);
    SETGET_PROP(MinTextureSize,     UINT32);
    SETGET_PROP(MaxTextureSize,     UINT32);
    SETGET_PROP(TxMode,             UINT32);
    SETGET_PROP(CorruptionStep,     UINT32);
    SETGET_PROP(CorruptionSwapchainIdx, UINT32);
    SETGET_PROP(CorruptionLocationOffsetBytes, UINT64);
    SETGET_PROP(CorruptionLocationPercentOfSize, UINT32);
    SETGET_PROP(PrintPerf,          bool);
    SETGET_PROP(ClearColor,         vector<UINT32>);
    SETGET_PROP(DumpPngOnError,     bool);
    SETGET_PROP(DumpPngMask,        UINT32);    // mask used to control which pngs to dump when no
                                                // errors are detected
    SETGET_PROP(FarZMultiplier,     float);
    SETGET_PROP(NearZMultiplier,    float);
    SETGET_PROP(KeepRunning,        bool);
    RC InternalSetBufferCheckMs(UINT32 val); // custom function to track if the user set this.
    UINT32 InternalGetBufferCheckMs() const { return m_BufferCheckMs; }
    SETGET_PROP_FUNC(BufferCheckMs, UINT32, InternalSetBufferCheckMs, InternalGetBufferCheckMs);

    SETGET_PROP(UseCompute,         bool);
    SETGET_JSARRAY_PROP_LWSTOM(ComputeShaderSelection); // which shader to use on which queue
    SETGET_PROP(NumComputeQueues,   UINT32);
    SETGET_JSARRAY_PROP_LWSTOM(ComputeInnerLoops);
    SETGET_JSARRAY_PROP_LWSTOM(ComputeOuterLoops);
    SETGET_JSARRAY_PROP_LWSTOM(ComputeRuntimeClks);
    SETGET_PROP(ComputeDisableIMMA, bool);
    SETGET_PROP(AsyncCompute,       bool);
    SETGET_PROP(PrintTuningInfo,    bool);
    SETGET_PROP(PrintSCGInfo,       bool);
    SETGET_PROP(PrintComputeCells,   bool);
    SETGET_PROP(DrawJobTimeNs,      UINT64);
    SETGET_PROP(MovingAvgSampleWindow, UINT64);
    SETGET_PROP(Dither,             bool);
    SETGET_PROP(PulseMode,          UINT32);
    SETGET_PROP(LowHz,              FLOAT64);
    SETGET_PROP(HighHz,             FLOAT64);
    SETGET_PROP(DutyPct,            FLOAT64);
    SETGET_PROP(OctavesPerSecond,   FLOAT64);
    SETGET_PROP(StepHz,             FLOAT64);
    SETGET_PROP(FMaxAverageDutyPctMax, FLOAT32);
    SETGET_PROP(FMaxAverageDutyPctMin, FLOAT32);
    SETGET_PROP(FMaxConstTargetMHz, UINT32);
    SETGET_PROP(FMaxCtrlHz,         UINT32);
    SETGET_PROP(FMaxForcedDutyInc,  FLOAT32);
    SETGET_PROP(FMaxFraction,       FLOAT32);
    SETGET_PROP(FMaxGainP,          FLOAT32);
    SETGET_PROP(FMaxGainI,          FLOAT32);
    SETGET_PROP(FMaxGainD,          FLOAT32);
    SETGET_PROP(FMaxPerfTarget,     UINT32);
    SETGET_PROP(FMaxWriteCsv,       bool);
    SETGET_PROP(StepTimeUs,         UINT64);
    SETGET_PROP(TileMask,           UINT32);
    SETGET_PROP(NumActiveGPCs,      UINT32);
    SETGET_PROP(SkipDrawChecks,     bool);
    SETGET_PROP(SynchronousMode,    bool);
    SETGET_PROP(WarpsPerSM,         UINT32);
    SETGET_PROP(TxD,                FLOAT32);
    SETGET_PROP(UseRandomTextureData, bool);
    SETGET_PROP(UseHTex,            bool);
    SETGET_PROP(PrintExtensions,    bool);
    SETGET_PROP(MaxHeat,            bool);
    SETGET_PROP(AllowedMiscompares, UINT32);
    SETGET_PROP(DisplayRaytracing,  bool);
    SETGET_PROP(UseRaytracing,      bool);
    SETGET_PROP(AnisotropicFiltering, bool);
    SETGET_PROP(LinearFiltering,    bool);
    SETGET_PROP(TexCompressionMask, UINT32);
    SETGET_PROP(TexReadsPerDraw,    UINT32);
    SETGET_PROP(TexIdxStride,       UINT32);
    SETGET_PROP(TexIdxFarOffset,    UINT32);
    double GetFps() const { return m_RememberedFps; }
    //Debug properies
    SETGET_PROP(ShowQueryResults,   bool);
    SETGET_PROP(GenUniqueFilenames, bool);
    GET_PROP(ZeroFb,                bool);
#ifdef VULKAN_STANDALONE_KHR
    SETGET_PROP(DetachedPresentMs,  UINT32);
#endif
#ifdef VULKAN_STANDALONE
    SETGET_PROP(EnableDebugMarkers, bool);
    SETGET_PROP(MaxPower,           bool);
#endif

#ifndef VULKAN_STANDALONE
    SETGET_JSARRAY_PROP_LWSTOM(Notes);
#endif
    SETGET_PROP(RandMinFreqHz,      FLOAT64);
    SETGET_PROP(RandMaxFreqHz,      FLOAT64);
    SETGET_PROP(RandMinDutyPct,     FLOAT64);
    SETGET_PROP(RandMaxDutyPct,     FLOAT64);
    SETGET_PROP(RandMinLengthMs,    UINT64);
    SETGET_PROP(RandMaxLengthMs,    UINT64);

    static constexpr FLOAT32 MAX_FREQ_POSSIBLE = -1.0f;
    static constexpr UINT32 BUFFERCHECK_MS_DISABLED = (~0U);
private:
    // A helper class for handling progress
    class ProgressHandler
    {
    public:
        void ProgressHandlerInit
        (
            UINT64 durationInTicks,
            UINT64 totNumFrames,
            bool bIgnoreFrameCheck
        );
        void Update(UINT64 frameNumber);
        static constexpr UINT64 NumSteps() { return m_NumSteps; }
        UINT64 Step() const;
        bool Done() const;
        UINT64 ElapsedTimeMs();
    private:
        static constexpr UINT64 m_NumSteps = 100;

        UINT64 m_DurationInTicks = 0;
        UINT64 m_TotNumFrames = 0;
        UINT64 m_FrameNum = 0;
        UINT64 m_TimeStart = 0;
        UINT64 m_Step = 0; // Status variable: value between 0 and m_NumSteps
        bool   m_bIgnoreFrameCheck = false;
    };

    ProgressHandler m_ProgressHandler;

    //! \brief Manages how much work VkStress sends
    //!
    //! WorkController provides a simple interface for VkStress to calibrate
    //! FramesPerSubmit and any delays required to achieve pulsing.
    //!
    //! This base class provides the interface/implementation necessary to
    //! callwlate OutputFrames based on a time target. The derived classes will
    //! specify the target DrawJob and delay durations.
    class WorkController
    {
    public:
        explicit WorkController(VkStress* pTest) : m_pVkStress(pTest) {}
        virtual ~WorkController() { }

        virtual RC Setup();

        //! \brief Returns how many frames the next DrawJob should have
        //!
        //! This function will keep returning the same value until more work is
        //! reported and the client calls Evaluate().
        UINT32 GetNumFrames() const;

        //! \brief Report how long the most recently completed DrawJob took
        //!
        //! The reason we must report both the begin/end times is so that the
        //! controller can spot any delays between DrawJobs (pulses). This
        //! includes intentional delays and unaccounted dead-time/overhead.
        void RecordGfxTimeNs(UINT64 beginNs, UINT64 endNs);

        //! \brief Run the controller
        //!
        //! Makes the controller recallwlate work/delay time and come up
        //! with new values for OutputFrames and OutputDelayTimeNs.
        RC Evaluate();

        //! \brief Make the controller always output the same number of frames
        void ForceNumFrames(UINT32 numFrames);

        //! \brief Print information about the controller's state and callwlations
        void PrintDebugInfo();
        void PrintStats();
        virtual bool Done() const { return true; }
        virtual bool IgnoreWaveformAclwracy() const { return false; }

        GET_PROP(OutputDelayTimeNs, UINT64); // Should be used to tune delays for pulsing
        SETGET_PROP(TargetWorkTimeNs, UINT64);
        GET_PROP(TargetDelayTimeNs, UINT64);
        SETGET_PROP(WindowSize, UINT64);
        SETGET_PROP(Dither, bool);
        SETGET_PROP(PrintPri, Tee::Priority);
        SETGET_PROP(RoundToQuadFrame, bool);

        static constexpr UINT64 MIN_OUTPUT_DELAY_NS = 1000ULL;
        static constexpr FLOAT32 MIN_OUTPUT_FRAMES = 1.0f;

    protected:
        //! \brief Returns how long the next DrawJob should take
        virtual UINT64 CallwlateWorkTimeNs() = 0;

        //! \brief Returns how much delay we need after the next DrawJob
        virtual UINT64 CallwlateDelayTimeNs() = 0;

        // Needed to provide an initial estimate when controller starts
        void SetOutputDelayTimeNs(UINT64 timeNs);

        bool IsPulsing() const;

        struct WorkTime
        {
            UINT64 beginNs = 0;
            UINT64 endNs = 0;
        };
        WorkTime m_LwrrGfxTimes = {};
        WorkTime m_PrevGfxTimes = {};

        Tee::Priority m_PrintPri = Tee::PriSecret;

        // Controller outputs
        FLOAT64 m_OutputFrames = 80.0; // 80 works well on a 1-1-1 TU10x config at min clocks
        UINT64 m_OutputDelayTimeNs = 5 * MIN_OUTPUT_DELAY_NS;

        VkStress* m_pVkStress = nullptr;

    private:
        // Internal state
        UINT64 m_TargetWorkTimeNs = 0;
        UINT64 m_TargetDelayTimeNs = 0;

        FLOAT64 m_PredictedOutputFrames = 0.0;
        FLOAT64 m_PredictedDelayNs = 0.0;

        UINT64 m_ObservedGfxTimeNs = 0;
        UINT64 m_ObservedLatencyTimeNs = 0;

        // User-configurable state
        UINT64 m_WindowSize = 10ULL; // Moving average window size
        bool m_Dither = true;
        bool m_ForceOutputFrames = false;

        // Statistical data.
        struct cmdStats
        {
            UINT64 minCmdLatencyNs;
            UINT64 maxCmdLatencyNs;
            UINT64 avgCmdLatencyNs;
            UINT64 totCmdLatencyNs;
            UINT64 count;
        };
        cmdStats m_Stats = { ~0UL, 0, 0, 0, 0 };

        // Force GetNumFrames() to round to a multiple of 4
        bool m_RoundToQuadFrame = false;

    };

    class StaticWorkController final : public WorkController
    {
    public:
        StaticWorkController(VkStress* pTest, UINT64 drawJobTimeNs);
        void SetDrawJobTimeNs(UINT64 drawJobTimeNs);
        UINT64 GetDrawJobTimeNs() { return m_DrawJobTimeNs; }
    private:
        UINT64 CallwlateWorkTimeNs() override;
        UINT64 CallwlateDelayTimeNs() override;

        UINT64 m_DrawJobTimeNs = 50'000'000ULL; // 50ms
    };

    //! \brief Base class for WorkControllers that have a duty cycle
    class DutyWorkController : public WorkController
    {
    public:
        SETGET_PROP(DutyPct, FLOAT64);

    protected:
        explicit DutyWorkController(VkStress* pTest) : WorkController(pTest) {}
        FLOAT64 m_DutyPct = 30.0;
    };

    //! \brief Pulses workload from LowHz to HighHz
    //!
    //! SweepWorkController tries to adjust FramesPerSubmit and
    //! OutputDelayTimeNs to meet a certain waveform (i.e. frequency and duty
    //! cycle %). It constantly adjusts its target frequency (LwrrHz) from
    //! LowHz to HighHz and back again. It uses an exponential function to
    //! control how quickly it sweeps frequency over time.
    //!
    //! It is implemented using a simple state machine with three states:
    //! sweep up, sweep down, and hold. First, it briefly holds at LowHz
    //! so that it colwerges. Then, it sweeps up and holds at HighHz to colwerge
    //! to the highest frequency. Finally, it sweeps back down and the cycle
    //! repeats.
    //!
    //! Simple illustration of the state machine:
    //! (HOLD -> UP -> HOLD -> DOWN) ---> (HOLD -> UP -> HOLD -> DOWN) ...
    //!
    class SweepWorkController final : public DutyWorkController
    {
    public:
        explicit SweepWorkController(VkStress* pTest) : DutyWorkController(pTest) {}
        SETGET_PROP(HighHz, FLOAT64);
        SETGET_PROP(OctavesPerSecond, FLOAT64);
        SETGET_PROP(StepHz, FLOAT64);
        SETGET_PROP(StepTimeUs, UINT64);
        SETGET_PROP(LinearSweep, bool);

        GET_PROP(LowHz, FLOAT64);
        GET_PROP(LwrrHz, FLOAT64);

        void SetLowHz(FLOAT64 hz);

        bool Done() const override { return m_FinishedOneSweepUp; }
        bool IgnoreWaveformAclwracy() const override;

    private:
        UINT64 CallwlateWorkTimeNs() override;
        UINT64 CallwlateDelayTimeNs() override;
        bool PulsingToMaxFreqPossible() const;

        enum class SweepState
        {
            UP,
            DOWN,
            HOLD
        };

        void Sweep(SweepState direction);
        void Hold();
        void NextState();

        // User-configurable args
        FLOAT64 m_LowHz = 50.0;
        FLOAT64 m_HighHz = MAX_FREQ_POSSIBLE;
        FLOAT64 m_TempHighHz = 2500.0;
        FLOAT64 m_OctavesPerSecond = 2.0;
        FLOAT64 m_StepHz = 1000.0;
        UINT64 m_StepTimeUs = 50000ULL;
        bool m_LinearSweep = false;

        // Internal state
        FLOAT64 m_LwrrHz = 250.0;

        static constexpr UINT32 NUM_SWEEP_STATES = 4;
        SweepState m_SweepStates[NUM_SWEEP_STATES] =
        {
            SweepState::HOLD,
            SweepState::UP,
            SweepState::HOLD,
            SweepState::DOWN
        };
        UINT32 m_StateIdx = 0;
        UINT64 m_BeginTime = 0;
        UINT64 m_CyclesHolding = 0;
        bool m_FinishedOneSweepUp = false;
    };

    class TileMaskWorkController : public DutyWorkController
    {
    public:
        TileMaskWorkController(VkStress* pTest, UINT64 drawJobTimeNs);
        void SetDrawJobTimeNs(UINT64 drawJobTimeNs);
        UINT64 GetDrawJobTimeNs() { return m_DrawJobTimeNs; }

    private:
        UINT64 CallwlateWorkTimeNs() override;
        UINT64 CallwlateDelayTimeNs() override;

        UINT64 m_DrawJobTimeNs = 5'000'000ULL; // 5ms
    };

    class NoteController : public WorkController
    {
    public:
        explicit NoteController(VkStress* pTest) : WorkController(pTest) {}

        RC Setup() override;

        struct Note
        {
            FLOAT64 FreqHz;
            FLOAT64 DutyPct;
            UINT64 LengthMs;
            string Lyric;

            string ToString() const;
        };
        using Notes = vector<Note>;

        bool IgnoreWaveformAclwracy() const override;

    protected:
        virtual Note GetNextNote() = 0;

    private:
        UINT64 CallwlateWorkTimeNs() override;
        UINT64 CallwlateDelayTimeNs() override;
        UINT64 m_StartTickCount = 0; // Tick count when LwrrNote started playing
        Note m_LwrrNote = { 1046.5, 25.0, 250, "" }; // Set a default value for m_TargetWorkTimeNs
    };

    class MusicController : public NoteController
    {
    public:
        explicit MusicController(VkStress* pTest) : NoteController(pTest) {}

        bool Done() const override;
        void SetNotes(const Notes& sequence);

    private:
        Note GetNextNote() override;
        Notes m_Notes = {};
        Notes::size_type m_NoteIdx = 0;
        UINT64 m_NumNotesPlayed = 0;
    };

    class RandomNoteController : public NoteController
    {
    public:
        explicit RandomNoteController(VkStress* pTest) : NoteController(pTest) {}

        bool Done() const override;

        SETGET_PROP(MinFreqHz, FLOAT64);
        SETGET_PROP(MaxFreqHz, FLOAT64);
        SETGET_PROP(MinDutyPct, FLOAT64);
        SETGET_PROP(MaxDutyPct, FLOAT64);
        SETGET_PROP(MinLengthMs, UINT64);
        SETGET_PROP(MaxLengthMs, UINT64);

    private:
        Note GetNextNote() override;

        FLOAT64 m_MinFreqHz = 50;
        FLOAT64 m_MaxFreqHz = 20000;
        FLOAT64 m_MinDutyPct = 10;
        FLOAT64 m_MaxDutyPct = 90;
        UINT64 m_MinLengthMs = 20;
        UINT64 m_MaxLengthMs = 200;
    };

    // Largest vertex format 64 bytes
    struct vtx_f4_f4_f4_f2
    {
        FLOAT32 pos[4];     //{ x, y, z, w }
        FLOAT32 norm[4];    //{ x, y, z, w }
        FLOAT32 col[4];     //{ r, g, b, a }
        FLOAT32 texcoord[2];    //{ s, t } no 'r' or 'q'
        FLOAT32 padding[2];
    };

    // Larger vertex format: 32 bytes.
    struct vtx_f3_f3_f2
    {
        FLOAT32 pos[3];     //{ x, y, z }
        FLOAT32 norm[3];    //{ x, y, z }
        FLOAT32 tex0[2];    //{ s, t } no 'r' or 'q'
    };
    struct vtx_f4_f4_f2
    {
        FLOAT32 pos[4];     //{ x, y, z }
        FLOAT32 norm[4];    //{ x, y, z }
        FLOAT32 tex0[2];    //{ s, t } no 'r' or 'q'
    };
    struct vtx_f2
    {
        FLOAT32 pos[2];     //{ x, y }
        vtx_f2(FLOAT32 x, FLOAT32 y) { pos[0] = x; pos[1] = y; }
    };
    struct vtx_f3
    {
        FLOAT32 pos[3];     //{ x, y, z }
        vtx_f3(FLOAT32 x, FLOAT32 y, FLOAT32 z) { pos[0] = x; pos[1] = y; pos[2] = z; }
    };
    struct Light
    {
        FLOAT32 pos[4];     //{ x, y, z, w }
        FLOAT32 color[3];   //{ r, g, b }
        FLOAT32 radius;
    };
    static constexpr UINT32 MAX_LIGHTS = 16;
    struct LightUBO
    {
        Light lights[MAX_LIGHTS];
        FLOAT32 camera[4];  //{ x, y, z, w }
    };

    struct SpecializationData
    {
        UINT32  numLights;
        FLOAT32 xTessRate;
        FLOAT32 yTessRate;
        UINT32  numSamplers;
        UINT32  numTextures;
        FLOAT32 texelDensity;
        UINT32  texReadsPerDraw;
        UINT32  surfWidth;
    };
    struct ComputeSpecializationData
    {
        UINT32  asyncCompute;
    };

    static constexpr UINT32 MESH_INTERNAL_COLS = 8;
    static constexpr UINT32 MESH_INTERNAL_ROWS = 8;

#ifndef VULKAN_STANDALONE
    mglTestContext                          m_mglTestContext;

    class FMaxController final : public PIDDutyController
    {
    public:
        FMaxController(PIDGains gains);
        FMaxController() = default;
        FLOAT32 CallwlateError() override;
        const char* GetUnits() const override;

        GpuSubdevice  *m_pGpuSubdevice = nullptr;
        FLOAT32        m_FMaxFraction = 0.0f;
        FMaxPerfTarget m_FMaxPerfTarget = PERF_TARGET_NONE;
        UINT64         m_LwrrentGpcClockHz = 0;
        UINT64         m_LwrrentTargetGpcClockHz = 0;
    };

    FMaxController m_FreqController;
#endif

    //Vulkan specific constructs
    //Note all varibles with m_H??? are host accessible objects
    //     variables without the 'H' are device accessible objects.
    VulkanInstance                          m_VulkanInst;
    VulkanDevice*                           m_pVulkanDev = nullptr;
#ifdef VULKAN_STANDALONE_KHR
    unique_ptr<SwapChainKHR>                m_SwapChainKHR = nullptr;
#endif
    unique_ptr<SwapChainMods>               m_SwapChainMods = nullptr;
    SwapChain *                             m_SwapChain = nullptr;

    UINT32                                  m_LwrrentSwapChainIdx = 0;
    vector<unique_ptr<VulkanImage>>         m_DepthImages;

    unique_ptr<VulkanBuffer>                m_HUniformBufferMVPMatrix = nullptr;
    unique_ptr<VulkanBuffer>                m_UniformBufferMVPMatrix = nullptr;

    unique_ptr<VulkanBuffer>                m_HUniformBufferLights;
    unique_ptr<VulkanBuffer>                m_UniformBufferLights;

    //Vertex buffers
    unique_ptr<VulkanBuffer>                m_VBVertices = nullptr;

    //Index buffer
    unique_ptr<VulkanBuffer>                m_HIndexBuffer = nullptr;
    unique_ptr<VulkanBuffer>                m_IndexBuffer = nullptr;

    //SMid to GPCidx buffer
    unique_ptr<VulkanBuffer>                m_Sm2GpcBuffer = nullptr;

    //Vertex buffer attributes
    VBBindingAttributeDesc                  m_VBBindingAttributeDesc;

    //buffer bindings
    UINT32                                  m_VBBindingVertices = 0;

    vector<unique_ptr<VulkanTexture>>       m_Textures; // device local
    vector<VulkanSampler>                   m_Samplers;

    vector<VkWriteDescriptorSet>            m_WriteDescriptorSets;
    //Command buffers
    unique_ptr<VulkanCmdPool>               m_CmdPool = nullptr;
    unique_ptr<VulkanCmdBuffer>             m_InitCmdBuffer = nullptr;

    // These variables are required to implement BufferCheckMs using a detatched thread.
    // and a transfer queue.
    unique_ptr<VulkanCmdPool>               m_CheckSurfaceCmdPool = nullptr;
    unique_ptr<VulkanCmdBuffer>             m_CheckSurfaceCmdBuffer = nullptr;
    unique_ptr<VulkanCmdPool>               m_CheckSurfaceTransPool = nullptr;
    unique_ptr<VulkanCmdBuffer>             m_CheckSurfaceTransBuffer = nullptr;
    struct BufferCheckInfo
    {
        UINT32 swapchainIdx;
        UINT64 lastDrawJobIdx;
    };
    struct CheckSurfaceParams
    {
        UINT32 swapchainIdx;
        UINT32 numFrames;
    };
    class CheckSurfaceQueue
    {
    public:
        CheckSurfaceQueue();
        ~CheckSurfaceQueue();
        void Push(CheckSurfaceParams params);
        bool GetParams(CheckSurfaceParams *pParams);
        UINT32 Size();

    private:
        std::queue<CheckSurfaceParams>   m_Queue;
        void*                m_Mutex;
    };
    CheckSurfaceQueue                       m_CheckSurfaceQueue;
    ModsEvent*                              m_pCheckSurfaceEvent = nullptr;
    bool                                    m_RunCheckSurfaceThread = true;
    StickyRC                                m_CheckSurfaceRC;

    ModsEvent*                              m_pThreadExitEvent = nullptr;

    using DrawJob = unique_ptr<VulkanCmdBuffer>;// Render command buffer, tied to one framebuffer
    using DrawJobs = vector<DrawJob>;
    DrawJobs                                m_PreDrawCmdBuffers;
    DrawJobs                                m_DrawJobs;
    UINT64                                  m_DrawJobIdx = 0;
    const UINT64                            m_NumDrawJobs = 2;
    const UINT64                            m_NumComputeJobs = 2;
    unique_ptr<VulkanRenderPass>            m_RenderPass = nullptr;
    unique_ptr<VulkanRenderPass>            m_ClearRenderPass = nullptr;
    unique_ptr<DescriptorInfo>              m_DescriptorInfo = nullptr;

    //Shaders
    unique_ptr<VulkanShader>                m_MeshletsTaskShader = nullptr;
    unique_ptr<VulkanShader>                m_MeshletsMainShader = nullptr;
    unique_ptr<VulkanShader>                m_VertexShader = nullptr;
    unique_ptr<VulkanShader>                m_TessellationControlShader = nullptr;
    unique_ptr<VulkanShader>                m_TessellationEvaluationShader = nullptr;
    unique_ptr<VulkanShader>                m_FragmentShader = nullptr;

    vector<unique_ptr<VulkanFrameBuffer>>   m_FrameBuffers;
    VulkanPipeline                          m_GraphicsPipeline;

    vector<VkSemaphore>                     m_RenderSem;
    vector<VkFence>                         m_RenderFences;
    // Can't use nullptr init as on x86 VkSemaphore is actually uint64_t
    vector<VulkanSemaphore>                 m_SwapChainAcquireSems;
    vector<VulkanCmdBuffer>                 m_PresentCmdBuf;
    VkViewport                              m_Viewport = { 0.0, 0.0, 0.0, 0.0, 0, 0 };
    VkRect2D                                m_Scissor;
    // Internal resources needed to gather timestamps
    vector<unique_ptr<VulkanQuery>>         m_GfxTSQueries;
    static constexpr UINT32                 MAX_NUM_TIMESTAMPS = 2;
    static constexpr UINT32                 MAX_NUM_TS_QUERIES = 8;
    // Index into the graphics timestamp query pools
    UINT32                                  m_QueryIdx = 0;

    struct Sm2Gpc
    {
        UINT32 gpcId[MAX_SMS];
    };
    Sm2Gpc                                  m_Sm2Gpc;

    // Internal resources needed if m_UseCompute is true
    struct ComputeParameters
    {
        UINT32 width;       //number of threads in the X dim
        UINT32 height;      //number of threads in the Y dim
        INT32 outerLoops;
        INT32 innerLoops;
        UINT64 runtimeClks; //approximate amount of time to run.
        UINT64 pGpuSem;     //GFX GPU semaphore to signal when all the SMs are loaded
    };
    // Turing can have up to 48 TPCs and 96 SMs.
    struct ScgStats
    {
        UINT32 smLoaded[MAX_SMS];
        UINT32 smKeepRunning[MAX_SMS];         //SMs last value of the KeepRunning variable.
        UINT64 smStartClock[MAX_SMS];          //Initial SM clock value for tuning
        UINT64 smEndClock[MAX_SMS];            //Final SM clock value for tuning
        UINT32 smFP16Miscompares[MAX_SMS];     //miscompares that occur using float16_t ops
        UINT32 smFP32Miscompares[MAX_SMS];     //miscompares that occur using float16_t ops
        UINT32 smFP64Miscompares[MAX_SMS];     //miscompares that occur using double ops
        UINT32 smIterations[MAX_SMS];          //Exelwtion counter for performance measurements
    };

    struct ComputeWork
    {
        ComputeParameters                       m_ComputeParameters;

        ComputeSpecializationData               m_ComputeSpecializationData = { 0 };

        vector<unique_ptr<VulkanQuery>>         m_ComputeTSQueries;

        // Pointers to mapped VkBuffers
        ScgStats *                              m_pScgStats = nullptr;
        UINT64 *                                m_pCells = nullptr;
        unique_ptr<VulkanCmdPool>               m_ComputeCmdPool = nullptr;
        vector<unique_ptr<VulkanCmdBuffer>>     m_ComputeCmdBuffers;
        unique_ptr<VulkanPipeline>              m_ComputePipeline = nullptr;

        vector<unique_ptr<VulkanCmdBuffer>>     m_ComputeLoadedCmdBuffers;
        unique_ptr<VulkanPipeline>              m_ComputeLoadedPipeline = nullptr;
        // Gfx waits on this to indicate compute shaders have loaded into the SMs //$
        vector<VkSemaphore>                     m_ComputeLoadedSemaphores;

        vector<VkWriteDescriptorSet>            m_ComputeWriteDescriptorSets;
        unique_ptr<DescriptorInfo>              m_ComputeDescriptorInfo = nullptr;
        unique_ptr<VulkanBuffer>                m_ComputeUBOParameters = nullptr;
        unique_ptr<VulkanBuffer>                m_ComputeSBOStats = nullptr;
        unique_ptr<VulkanBuffer>                m_ComputeSBOCells = nullptr;
        unique_ptr<VulkanBuffer>                m_ComputeKeepRunning = nullptr;

        // CPU waits on this to indicate it can rebuild the cmd buffer.
        vector<VkFence>                         m_ComputeFences;
        // Compute waits on this to indicate the previous launch was complete
        vector<VkSemaphore>                     m_ComputeSemaphores;

        StickyRC                                m_ComputeRC;
        UINT64                                  m_ComputeJobIdx = 0;
        UINT32                                  m_ComputeQueryIdx = 0;
    };

    vector<ComputeWork>                     m_ComputeWorks;

    bool                                    m_UseCompute = false;
    vector<unique_ptr<VulkanShader>>        m_ComputeShaders;
    vector<string>                          m_ComputeProgs;
    vector<UINT32>                          m_ComputeShaderOpsPerInnerLoop;
    vector<UINT32>                          m_ComputeShaderSelection;

    unique_ptr<VulkanShader>                m_ComputeLoadedShader = nullptr;
    string                                  m_ComputeLoadedProg;

    UINT32                                  m_NumComputeQueues = 1;
    vector<UINT32>                          m_ComputeInnerLoops;
    vector<UINT32>                          m_ComputeOuterLoops;
    vector<UINT32>                          m_ComputeRuntimeClks;
    UINT32                                  m_NumSMs = 0;
    UINT32                                  m_WarpsPerSM = 1;
    // Local workgroup size (defined in the shader code)
    bool                                    m_ComputeDisableIMMA = false;
    bool                                    m_AsyncCompute = false;
    bool                                    m_PrintTuningInfo = false;
    bool                                    m_PrintSCGInfo = false;
    bool                                    m_PrintComputeCells = false;

#ifndef VULKAN_STANDALONE
    JsArray                                 m_JsNotes = {};
#endif

    RC                  AddComputeExtensions(vector<string>& extNames);
    void                AddDbgMessagesToIgnore();
    RC                  AddHTexExtensions(vector<string>& extNames);
    RC                  AddMeshletExtensions(vector<string>& extNames);
    RC                  AllocateComputeResources();
    RC                  BuildComputeCmdBuffer(ComputeWork * pCW);
    RC                  CheckForComputeErrors(UINT32 swapchainIdx);
    RC                  CleanupComputeResources();

    void                DumpComputeCells(const char *pTitle);
    void                DumpComputeData(const char *pTitle);
    void                DumpQueryResults();
    string              GetUniqueFilename(const char *pTestName, const char * pExt);
    virtual RC          InitFromJs();
    RC                  SetupComputeBuffers(ComputeWork * pCW, UINT32 queueIdx);
    RC                  SetupComputeDescriptors(ComputeWork * pCW);
    RC                  SetupComputeSemaphoresAndFences();
    RC                  SetupComputeShader();
    RC                  SetupComputeFMAShader(string *computeProg, UINT32 *computeOpsPerInnerLoop) const;
    RC                  SetupComputeIMMAShader(string *computeProg, UINT32 *computeOpsPerInnerLoop) const;
    RC                  SetupComputeLoadedShader();
    RC                  TuneWorkload(UINT64 drawJobIdx);
    void                TuneComputeWorkload(UINT32 computeQueueIdx);
    RC                  VerifyQueueRequirements
                        (
                            UINT32 *pNumGraphicsQueues,
                            UINT32 *pNumTransferQueues,
                            UINT32 *pNumComputeQueues,
                            UINT32 *pSurfCheckGfxQueueIdx,
                            UINT32 *pSurfCheckTransQueueIdx
                        );
    RC                  WaitForPreviousDrawJob();

    //Geometry and color related properties
    vector<UINT32>                          m_ClearColor = { 0x12A55A3C, 0x123CA55A, 0x125A3CA5 };
    GLCamera                                m_Camera;

    //Time keepers
    Time                                    m_IsSupportedTimeCallwlator;
    Time                                    m_SetupTimeCallwlator;
    Time                                    m_RunTimeCallwlator;
    Time                                    m_CleanupTimeCallwlator;
    // Speedup and colwience variables
    bool                                    m_ZeroFb = false;
    Tee::Priority                           m_BufCkPrintPri = Tee::PriLow;
    //Test properties
    UINT32                                  m_FramesPerSubmit = 0;
    UINT32                                  m_NumFrames = 0;
    bool                                    m_CorruptionDone = false;
    bool                                    m_IndexedDraw = true;
    bool                                    m_SkipDirectDisplay = false;
    bool                                    m_SendColor = true;
    bool                                    m_UseMipMapping = true;
    bool                                    m_UsePlainColorMipMaps = false; // To visualize the different mipmap levels $
    bool                                    m_ApplyFog = true;
    bool                                    m_ExponentialFog = true;
    bool                                    m_DrawLines = false;
    bool                                    m_DrawTessellationPoints = false;
    bool                                    m_Ztest = true;
    bool                                    m_PrintExtensions = false;
    bool                                    m_ShowQueryResults = false;
    // FarZMultiplier is set to a large value so that we get many non-zero
    // values in the depth buffer. NearZMultiplier is small so that all
    // depth values are zero, which is necessary for depth buffer checking.
    float                                   m_FarZMultiplier = 2500.0f;
    float                                   m_NearZMultiplier = 0.4f;

    bool                                    m_Stencil = true;
    bool                                    m_ColorLogic = true;
    bool                                    m_ColorBlend = false;
    UINT32                                  m_Debug = 0;
    bool                                    m_DumpPngOnError = true;
    UINT32                                  m_DumpPngMask = VkUtil::CHECK_ALL_SURFACES;
    UINT32                                  m_DumpPng = 0;
    Goldelwalues *                          m_pGolden = nullptr;
    vector<unique_ptr<VulkanGoldenSurfaces>> m_GoldenSurfaces;
    string                                  m_FragProg;
    string                                  m_VtxProg;
    string                                  m_TessCtrlProg;
    string                                  m_TessEvalProg;
    string                                  m_MeshTaskProg;
    string                                  m_MeshMainProg;
    bool                                    m_EnableValidation = false;
    bool                                    m_EnableErrorLogger = true;
    bool                                    m_ShaderReplacement = false;
    bool                                    m_UseHTex = true;
    UINT32                                  m_BufferCheckMs = 5000;
    bool                                    m_BufferCheckMsSetByJs = false;
    UINT64                                  m_NextBufferCheckMs = 0;
    int                                     m_RenderWidth = 0;  // in pixels, ignoring FSAA stuff
    int                                     m_RenderHeight = 0; // in pixels, ignoring FSAA stuff
    // Legacy GLStress specific variables & functions
    double                      m_PpV = 625;             // pixels per vertex(controls prim size)
    bool                        m_UseTessellation = true;
    bool                        m_UseMeshlets = false;
    UINT32                      m_LoopMs = 4000;     // time to loop in msecs
    UINT32                      m_RuntimeMs = 0;     // Alias to LoopMs for unification across MODS
    // Current rendering info.
    UINT32          m_Rows = 0;
    UINT32          m_Cols = 0;
    UINT32          m_VxPerPlane = 0;
    UINT32          m_NumVertexes = 0;  // number of vertices needed to draw both near/far planes
    UINT32          m_IdxPerPlane = 0;
    UINT32          m_NumIndexes = 0;
    bool            m_BoringXform= false;       // use a straight-through x.y == pixel location
    FLOAT32         m_CameraScaleFactor = 2.05f;
    UINT32          m_MinTextureSize = 8;       // minimum size of textures in texels (w & h)
    UINT32          m_MaxTextureSize = 128;     // maximum size of textures in texels (w & h)
    UINT32          m_SampleCount = 1;
    vector<unique_ptr<VulkanImage>> m_MsaaImages;

    UINT32          m_NumLights = 1;
    SpecializationData m_SpecializationData = { 0 };
    UINT32          m_NumTextures = 32;
    bool            m_UseRandomTextureData = false;

    UINT32          m_TxMode = TX_MODE_STATIC;
    FLOAT32         m_TxD = 0.0f;
    UINT32          m_TexCompressionMask = 0;
    UINT32          m_TexIdx = 0;
    UINT32          m_TexReadsPerDraw = 2;
    UINT32          m_TexIdxStride = 1;
    UINT32          m_TexIdxFarOffset = 0;

    static constexpr UINT32 CORRUPTION_STEP_DISABLED = 0xFFFFFFFF;
    UINT32          m_CorruptionStep = CORRUPTION_STEP_DISABLED;
    UINT32          m_CorruptionSwapchainIdx = ~0;
    UINT64          m_CorruptionLocationOffsetBytes = 200000ULL;
    UINT32          m_CorruptionLocationPercentOfSize = 50;

    bool            m_PrintPerf = false;
    bool            m_KeepRunning = false;

    unique_ptr<WorkController> m_WorkController = {};
    UINT64          m_DrawJobTimeNs = 50'000'000ULL; // 50ms
    UINT64          m_MovingAvgSampleWindow = 10ULL;
    bool            m_Dither = true;
    UINT32          m_PulseMode = NO_PULSE;
    FLOAT64         m_LowHz = 50.0;
    FLOAT64         m_HighHz = MAX_FREQ_POSSIBLE;
    FLOAT64         m_DutyPct = 30.0;
    LwU32           m_LwrrentPowerMilliWatts = 0;
    FLOAT32         m_FMaxAverageDutyPctMax = 90.0f;
    FLOAT32         m_FMaxAverageDutyPctMin = 10.0f;
    UINT32          m_FMaxConstTargetMHz = 0;
    UINT32          m_FMaxCtrlHz = 100;
    UINT32          m_FMaxDutyPctSamples = 0;
    UINT64          m_FMaxDutyPctSum = 0;
    FLOAT32         m_FMaxForcedDutyInc = 1.0f;
    FLOAT32         m_FMaxFraction = 0.97f;
    FLOAT32         m_FMaxGainP = 0.1f;
    FLOAT32         m_FMaxGainI = 0.01f;
    FLOAT32         m_FMaxGainD = 0.05f;
    UINT64          m_FMaxIdleGpc = 0;
    UINT32          m_FMaxPerfTarget = PERF_TARGET_RELIABILITY;
    bool            m_FMaxWriteCsv = false;
    FLOAT64         m_OctavesPerSecond = 2.0;
    FLOAT64         m_StepHz = 1000.0;
    UINT64          m_StepTimeUs = 50000ULL; // 50ms
    bool            m_SynchronousMode = true;
    UINT32          m_AvailGraphicsQueues = 0;
    UINT32          m_AvailTransferQueues = 0;
    UINT32          m_AvailComputeQueues = 0;
    bool            m_MaxHeat = false;
    double          m_RememberedFps = 0;
    UINT64          m_PrevRandMask = 0;
    UINT32          m_HighestGPCIdx = 0;
    UINT32          m_TileMask = 0;
    UINT32          m_NumActiveGPCs = 0;
    bool            m_SkipDrawChecks = false;
    bool            m_GenUniqueFilenames = false;
#ifndef VULKAN_STANDALONE
    PStateOwner     m_PStateOwner = {};
    Perf::PerfPoint m_OrigPerfPoint = {};
    bool            m_PStatesClaimed = false;
    bool            m_PerfPointOverriden = false;
    GpuUtility::ValueStats m_GPCShuffleStats;
#else
    bool            m_EnableDebugMarkers = false;
    bool            m_MaxPower = false;
#endif
    UINT32          m_AllowedMiscompares = 0;
    struct Miscompare
    {
        Miscompare() = default;
        Miscompare(VkUtil::SurfaceCheck s, UINT32 c) : surfType(s), count(c) {}
        VkUtil::SurfaceCheck surfType = VkUtil::CHECK_ALL_SURFACES;
        UINT32 count = 0;
    };
    using SwapchainIndex = UINT32;
    using Miscompares = vector<vector<Miscompare>>;
    Miscompares     m_Miscompares = {};
    // Raytracing variables
    bool                        m_UseRaytracing = false;
    bool                        m_DisplayRaytracing = false;
    unique_ptr<Raytracer>       m_Rt;

    bool                        m_AnisotropicFiltering = true;
    bool                        m_LinearFiltering = true;

#ifdef VULKAN_STANDALONE_KHR
    UINT32                      m_DetachedPresentMs = 0;
    UINT32                      m_DetachedPresentQueueIdx = 0;
#endif

    // Random pulsing arguments
    FLOAT64                     m_RandMinFreqHz = 50.0;
    FLOAT64                     m_RandMaxFreqHz = 20000.0;
    FLOAT64                     m_RandMinDutyPct = 10.0;
    FLOAT64                     m_RandMaxDutyPct = 90.0;
    UINT64                      m_RandMinLengthMs = 20;
    UINT64                      m_RandMaxLengthMs = 200;

    //Default APIs
    RC                          AllocateVulkanDeviceResources();
    RC                          ApplyMaxHeatSettings();
    RC                          BuildDrawJob();
    void                        SetDepthStencilState(VkCommandBuffer cmdBuf, UINT32 frameMod4);
    RC                          AddFrameToCmdBuffer(UINT32 frameIdx, VulkanCmdBuffer* dj);
    RC                          BuildPreDrawCmdBuffer();
    RC                          CalcGeometrySizePlanes();
    RC                          CalcTextureSize(UINT32 texIdx, UINT32* pTxSize);
    RC                          CheckErrors();
    RC                          CheckSurface(UINT32 flags, UINT32 swapchainIdx, UINT32 numFrames);
    // This function is required to implement BufferCheckMs using a detatched thread.
    static void                 CheckSurfaceThread(void *vkStress);

    struct ComputeThreadArgs
    {
        void *pVkStress;
        UINT32 ComputeWorkIdx;
    };
    static void                 ComputeThread(void *pComputeThreadArgs);
    void                        ComputeThread(UINT32 computeWorkIdx);

    void                        CleanupAfterSetup() const;
#ifndef VULKAN_STANDALONE
    RC                          CorruptImage();
#endif
    RC                          CreateGoldenSurfaces();
    RC                          CreateGraphicsPipelines();
    RC                          CreateComputePipeline();
    RC                          CreateRenderPass() const;
    void                        DumpSurface
                                (
                                    char * szTitle
                                    ,UINT32 top
                                    ,UINT32 left
                                    ,UINT32 width
                                    ,UINT32 height
                                    ,UINT32 pitch
                                    ,UINT32 bpp      //bytes per pixel
                                    ,void *pSurface
                                );
    void                        GenGeometryPlanes
                                (
                                    vector<UINT32>* pIndices
                                    ,vector<vtx_f4_f4_f4_f2>* pVertices
                                ) const;
    UINT32                      GetBadPixels
                                (
                                    const char * pTitle,
                                    unique_ptr<VulkanGoldenSurfaces> &gs,
                                    UINT32 surfIdx,
                                    UINT32 maxErrors,
                                    UINT32 expectedValue,
                                    UINT32 *pSrc,
                                    UINT32 pixelMask // set to ~0 for RGBA,
                                                     // 0xffffff for depth, &
                                                     // 0xff000000 for stencil
                                );
    VkImageCreateFlags          GetImageCreateFlagsForHTex() const;
    RC                          HandleMiscompares();
    RC                          InnerCheckErrors(UINT32 flags, UINT32 swapIdx, UINT32 numFrames);
    RC                          InnerRun();

#ifndef VULKAN_STANDALONE
    static void                 FMaxThread(void *vkStress);
    void                        FMaxThread();
    void                        PowerControlCallback(LwU32 milliWatts) override;
    void                        PrintJsProperties(Tee::Priority pri) override;
#endif

    void                        PrintIndexData
                                (
                                    vector<UINT32>& indices
                                    ,vector<vtx_f4_f4_f4_f2>& vertices
                                ) const;
    RC                          ReadSm2Gpc();
    RC                          RunFrames();
    RC                          SetupBuffers();
    RC                          SetupLights() const;
    RC                          SetupDepthBuffer();
    RC                          SetupDescriptors();
    RC                          SetDisplayImage(UINT32 imageIndex);
    RC                          SetupFrameBuffer();
    RC                          SetupSemaphoresAndFences();
    RC                          SetupShaders();
    RC                          SetupMeshletsTaskShader();
    RC                          SetupMeshletMainShader();
    RC                          SetupVertexShader();
    RC                          SetupFragmentShader();
    RC                          SetupTessellationControlShader();
    RC                          SetupTessellationEvaluationShader();
    RC                          SetupSamplers();
    RC                          SetupTextures();
    RC                          SetupQueryObjects();
    RC                          SetupWorkController();
    void                        SignalSurfaceCheck(UINT32 swapChainIdx, UINT32 numFrames);
    void                        StoreVertex
                                (
                                    const VkStress::vtx_f4_f4_f4_f2 & bigVtx
                                    ,UINT32 index
                                    ,vector<vtx_f4_f4_f4_f2> * pVertices
                                ) const;
    RC                          SendWorkToGpu();
    RC                          SubmitPreDrawCmdBufferAndPresent();
    RC                          SetupMsaaImage();
    RC                          SetupDrawCmdBuffers();
    RC                          UpdateProgressAndYield();
    RC                          WaitForFences
                                (
                                    UINT32          fenceCount
                                    ,const VkFence *pFences
                                    ,VkBool32       waitAll
                                    ,UINT64         timeoutNs
                                    ,bool           usePolling
                                );

#ifdef VULKAN_STANDALONE_KHR
    RC                          PresentTestImage(VkSemaphore* pWaitSema);
    static void                 PresentThread(void *VkStress);
    void                        PresentThread();
#endif

    //! \brief Generates a random bitmask with numBits set
    UINT64                      GenRandMask(UINT32 numBits, UINT32 numMaskBits);
    bool                        IsPulsing() const;
    UINT32                      GetNumGfxSMs();
};
