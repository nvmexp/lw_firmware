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
/*
 * This test is intended to mimic the same functionality as the original GLStressTest, however
 * instead of using the OpenGL graphics layer we will be using the new Vulkan graphics layer.
 * Given that there are some differences between the two layers that need to be considered. They
 * are:
 * Vulkan doesn't support QUAD_STRIP primitives:
 * - So we have to decompose each square in the grid into two triangles and use a TRIANGLE_STRIP
 *   primitive.
 *
 * Compute Queue:
 * - We will also be trying to increase the stressfullness by including a compute shader that runs
 *   in a separate queue in parallel with the graphics shaders.
 *
 * In addition the following GLStress features still need to be implemented:
 * - Boring transformation
 * - Torus geometry
 * - Power monitoring
 * - DoGpuCompare
 * - DrawLines (needs some fixing to accomodate a LINE_STRIP)
 * - Rotation
 * - 3D textures
 * - Workload pulsing using a do-nothing compute shader
**************************************************************************************************/
#include "vkstress.h"
#include "vkdev.h"
#include "vktexfill.h"
#include "vkutil.h"
#include "vkstress/vkstressray.h"
#include "core/include/fileholder.h"

#ifndef VULKAN_STANDALONE
#include "core/include/abort.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/tasker_p.h"
#include "core/include/utility.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/pwrsub.h"
#include "gpu/utility/scopereg.h"
#include "core/include/cmdline.h"
#endif

//-------------------------------------------------------------------------------------------------
#include <align.h>
#include <array>
#include "core/include/imagefil.h"
#define INCREMENT_NEXT_PATTERN (-1)
#define RT_IMAGE_IDX (m_Miscompares.size()-1)
using namespace VkUtil;

#define DS_BINDING_MVP_UBO    0
#define DS_BINDING_LIGHT_UBO  1
#define DS_BINDING_SAMPLERS   2
#define DS_BINDING_TEXTURES   3
#define DS_BINDING_MESHLET_VB 4
#define DS_BINDING_SM2GPC     5

#define TO_STRING(x) TO_STRING_INTERNAL(x)
#define TO_STRING_INTERNAL(x) #x

namespace
{
    UINT32 NearestLowerPowerOf2(const UINT32 value)
    {
        // If value is zero or already a power of two, return it
        if (!value || !(value & (value - 1)))
        {
            return value;
        }

        const auto logOfResult = static_cast<UINT32>(
                                 floor(log2(static_cast<FLOAT32>(value))));
        return static_cast<UINT32>(1U << (logOfResult - 1));
    }
}

void VkStress::ProgressHandler::ProgressHandlerInit
(
    const UINT64 durationInTicks
   ,const UINT64 totNumFrames
   ,const bool bIgnoreFrameCheck
)
{
    m_bIgnoreFrameCheck = bIgnoreFrameCheck;
    m_DurationInTicks = ((0 == durationInTicks) ? UINT64(1) : durationInTicks); // 1 tick min
    m_TimeStart = Xp::QueryPerformanceCounter();
    m_Step = 0;

    // TotalNumFrames MUST be a multiple of 4.
    constexpr auto pri = Tee::PriDebug;
    if (bIgnoreFrameCheck)
    {
        m_TotNumFrames = totNumFrames;
    }
    else
    {
        m_TotNumFrames = ((totNumFrames + 3) / 4) * 4;
    }
    m_TotNumFrames = ((0 == m_TotNumFrames) ? UINT64(4) : m_TotNumFrames);
    Printf(pri, "Initializing ProgressHandler.\n");
    Printf(pri, "  IgnoreFrameCheck: %s.\n", bIgnoreFrameCheck ? "true" : "false");
    Printf(pri, "  Minimum duration to go for: %llu ticks\n", m_DurationInTicks);
    Printf(pri, "  Minimum number of frames to go for: %llu frames\n", m_TotNumFrames);
    Printf(pri, "  Progress granularity: %llu steps\n", m_NumSteps);
}

void VkStress::ProgressHandler::Update(const UINT64 frameNumber)
{
    m_FrameNum = frameNumber;
    const UINT64 timeNow = Xp::QueryPerformanceCounter();
    // Compute progress
    const UINT64 timeProgress =
        (m_NumSteps * (timeNow - m_TimeStart)) / m_DurationInTicks;
    const UINT64 imageProgress = (m_NumSteps * frameNumber) / m_TotNumFrames;
    m_Step = (timeProgress < imageProgress) ? timeProgress : imageProgress;
    // Uncomment this line if you want more debug information on progress
    //Printf(Tee::PriDebug, "  Step: %llu / %llu\n", m_Step, m_NumSteps);
}

UINT64 VkStress::ProgressHandler::Step() const
{
    return (m_Step < static_cast<UINT64>(m_NumSteps)) ?
            m_Step :
            static_cast<UINT64>(m_NumSteps);
}

bool VkStress::ProgressHandler::Done() const
{
    // Note: Even with time based program termination the overriding principal here is
    //       that we can't end until we have exelwted a multiple of 4 frames unless
    //       m_bIgnoreFrameCheck = true which is stickly used for debugging.
    if (m_bIgnoreFrameCheck)
    {
        return (m_Step >= static_cast<UINT64>(m_NumSteps));
    }
    else
    {
        return ((m_Step >= static_cast<UINT64>(m_NumSteps)) && ((m_FrameNum % 4) == 0));
    }
}

UINT64 VkStress::ProgressHandler::ElapsedTimeMs()
{
    const UINT64 ticksPerMs = Xp::QueryPerformanceFrequency() / 1000ULL;
    const UINT64 timeNow = Xp::QueryPerformanceCounter();
    return (timeNow - m_TimeStart) / ticksPerMs;
}

//-------------------------------------------------------------------------------------------------
VkStress::VkStress()
{
    m_pGolden = GetGoldelwalues();
}

/*virtual*/ VkStress::~VkStress()
{
}
//-------------------------------------------------------------------------------------------------
bool VkStress::IsSupported()
{
    m_IsSupportedTimeCallwlator.StartTimeRecording();
    DEFER
    {
        m_IsSupportedTimeCallwlator.StopTimeRecording();
    };
#ifndef VULKAN_STANDALONE
    if (IsPulsing() && GetBoundGpuSubdevice()->IsEmOrSim())
    {
        return false;
    }
    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_VULKAN))
    {
        return false;
    }

    if ((m_UseMeshlets || m_UseCompute) && GetBoundGpuDevice()->GetFamily() < GpuDevice::Turing)
    {
        return false;
    }

    if (m_UseRaytracing)
    {
        // If the GPU doesn't have any TTUs return false.
        if (GetBoundGpuSubdevice()->GetRTCoreCount() == 0)
        {
            return false;
        }
    }
#endif

    return true;
}

#ifndef VULKAN_STANDALONE
void VkStress::PowerControlCallback(LwU32 milliWatts)
{
    m_LwrrentPowerMilliWatts = milliWatts;
}

namespace
{
    const char* GetTxModeStr(VkStress::TxMode txMode)
    {
        switch (txMode)
        {
            case VkStress::TX_MODE_STATIC:
                return "static";
            case VkStress::TX_MODE_INCREMENT:
                return "increment";
            case VkStress::TX_MODE_RANDOM:
                return "random";
            default:
                return "unknown";
        }
    }

    const char* GetPulseModeStr(VkStress::PulseMode pm)
    {
        switch (pm)
        {
            case VkStress::NO_PULSE:
                return "disabled";
            case VkStress::SWEEP:
                return "sweep";
            case VkStress::SWEEP_LINEAR:
                return "sweep linear";
            case VkStress::FMAX:
                return "fmax";
            case VkStress::TILE_MASK:
                return "tile mask";
            case VkStress::MUSIC:
                return "music";
            case VkStress::RANDOM:
                return "random";
            case VkStress::TILE_MASK_CONSTANT_DUTY:
                return "tile mask constant duty";
            case VkStress::TILE_MASK_USER:
                return "tile mask user";
            case VkStress::TILE_MASK_GPC:
                return "tile mask gpc";
            case VkStress::TILE_MASK_GPC_SHUFFLE:
                return "tile mask gpc shuffle";
            default:
                return "unknown";
        }
    }
}

static string PrintUINT32Array(const vector<UINT32> &arr)
{
    string ret = "[";
    UINT32 remaining = static_cast<UINT32>(arr.size());

    for (UINT32 val : arr)
    {
        ret += Utility::StrPrintf("%u", val);
        remaining--;
        if (remaining)
        {
            ret += ", ";
        }
    }

    ret += "]";

    return ret;
}

//-------------------------------------------------------------------------------------------------
// Please keep this list alphabetized for easy reading!
void VkStress::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "VkStress Js Properties:\n");

    const char * TF[] = { "false", "true" };
    Printf(pri, "\t%-32s %u\n", "AllowedMiscompares:",          m_AllowedMiscompares);
    Printf(pri, "\t%-32s %s\n", "AnisotropicFiltering:",        TF[m_AnisotropicFiltering]);
    Printf(pri, "\t%-32s %s\n", "ApplyFog:",                    TF[m_ApplyFog]);
    Printf(pri, "\t%-32s %s\n", "AsyncCompute:",                TF[m_AsyncCompute]);
    Printf(pri, "\t%-32s %s\n", "BoringXform:",                 TF[m_BoringXform]);
    if (m_BufferCheckMs == BUFFERCHECK_MS_DISABLED)
    {
        Printf(pri, "\t%-32s %s\n", "BufferCheckMs:",          "disabled");
    }
    else
    {
        Printf(pri, "\t%-32s %u\n", "BufferCheckMs:",           m_BufferCheckMs);
    }
    Printf(pri, "\t%-32s %f\n", "CameraScaleFactor:",           m_CameraScaleFactor);
    Printf(pri, "\t%-32s 0x%08x, 0x%08x, 0x%08x\n", "ClearColor(LE_A8R8G8B8):",
           m_ClearColor[0], m_ClearColor[1], m_ClearColor[2]);
    Printf(pri, "\t%-32s %s\n", "ColorBlend:",                  TF[m_ColorBlend]);
    Printf(pri, "\t%-32s %s\n", "ColorLogic:",                  TF[m_ColorLogic]);
    Printf(pri, "\t%-32s %s\n", "ComputeDisableIMMA:",          TF[m_ComputeDisableIMMA]);
    Printf(pri, "\t%-32s %s\n", "ComputeInnerLoops:",
        PrintUINT32Array(m_ComputeInnerLoops).c_str());
    Printf(pri, "\t%-32s %s\n", "ComputeOuterLoops:",
        PrintUINT32Array(m_ComputeOuterLoops).c_str());
    Printf(pri, "\t%-32s %s\n", "ComputeRuntimeClks:",
        PrintUINT32Array(m_ComputeRuntimeClks).c_str());
    Printf(pri, "\t%-32s %s\n", "ComputeShaderOpsPerInnerLoop:",
        PrintUINT32Array(m_ComputeShaderOpsPerInnerLoop).c_str());
    Printf(pri, "\t%-32s %s\n", "ComputeShaderSelection:",
        PrintUINT32Array(m_ComputeShaderSelection).c_str());
    if (m_CorruptionStep == CORRUPTION_STEP_DISABLED)
    {
        Printf(pri, "\t%-32s %s\n", "CorruptionStep:",          "disabled");
    }
    else
    {
        Printf(pri, "\t%-32s %u\n", "CorruptionStep:",              m_CorruptionStep);
    }
    Printf(pri, "\t%-32s %llu\n", "CorruptionLocationOffsetBytes:", m_CorruptionLocationOffsetBytes); //$
    Printf(pri, "\t%-32s %u\n", "CorruptionLocationPercentOfSize:", m_CorruptionLocationPercentOfSize); //$
    Printf(pri, "\t%-32s 0x%08x\n", "Debug:",                   m_Debug);
    Printf(pri, "\t%-32s %llu\n", "DrawJobTimeNs:",             m_DrawJobTimeNs);
    Printf(pri, "\t%-32s %s\n", "DrawLines:",                   TF[m_DrawLines]);
    Printf(pri, "\t%-32s %s\n", "DrawTessellationPoints:",      TF[m_DrawTessellationPoints]);
    Printf(pri, "\t%-32s %s\n", "EnableValidation:",            TF[m_EnableValidation]);
    Printf(pri, "\t%-32s %s\n", "EnableErrorLogger:",           TF[m_EnableErrorLogger]);
    Printf(pri, "\t%-32s %s\n", "ExponentialFog:",              TF[m_ExponentialFog]);
    Printf(pri, "\t%-32s %f\n", "FarZMultiplier:",              m_FarZMultiplier);
    Printf(pri, "\t%-32s %u\n", "FramesPerSubmit:",             m_FramesPerSubmit);
    Printf(pri, "\t%-32s %s\n", "GenUniqueFilenames:",          TF[m_GenUniqueFilenames]);
    Printf(pri, "\t%-32s %s\n", "Indexed Draw:",                TF[m_IndexedDraw]);
    Printf(pri, "\t%-32s %s\n", "KeepRunning:",                 TF[m_KeepRunning]);
    Printf(pri, "\t%-32s %s\n", "LinearFiltering:",             TF[m_LinearFiltering]);
    Printf(pri, "\t%-32s %u\n", "LoopMs:",                      m_LoopMs);
    Printf(pri, "\t%-32s %s\n", "MaxHeat:",                     TF[m_MaxHeat]);
    Printf(pri, "\t%-32s %u\n", "MaxTextureSize:",              m_MaxTextureSize);
    Printf(pri, "\t%-32s %u\n", "MinTextureSize:",              m_MinTextureSize);
    Printf(pri, "\t%-32s %llu\n", "MovingAvgSampleWindow:",     m_MovingAvgSampleWindow);
    Printf(pri, "\t%-32s %f\n", "NearZMultiplier:",             m_NearZMultiplier);
    Printf(pri, "\t%-32s %u\n", "NumComputeQueues:",            m_NumComputeQueues);
    Printf(pri, "\t%-32s %u\n", "NumLights:",                   m_NumLights);
    Printf(pri, "\t%-32s %u\n", "NumTextures:",                 m_NumTextures);
    Printf(pri, "\t%-32s %s\n", "PrintExtensions:",             TF[m_PrintExtensions]);
    Printf(pri, "\t%-32s %s\n", "PrintPerf:",                   TF[m_PrintPerf]);
    Printf(pri, "\t%-32s %s\n", "PrintSCGInfo:",                TF[m_PrintSCGInfo]);
    Printf(pri, "\t%-32s %s\n", "PrintTuningInfo:",             TF[m_PrintTuningInfo]);
    Printf(pri, "\t%-32s %f\n", "PpV:",                         m_PpV);
    Printf(pri, "\t%-32s %u\n", "RuntimeMs:",                   m_RuntimeMs);
    Printf(pri, "\t%-32s %u\n", "SampleCount:",                 m_SampleCount);
    Printf(pri, "\t%-32s %s\n", "SendColor:",                   TF[m_SendColor]);
    Printf(pri, "\t%-32s %s\n", "Skip Direct Display:",         TF[m_SkipDirectDisplay]);
    Printf(pri, "\t%-32s %s\n", "Stencil:",                     TF[m_Stencil]);
    Printf(pri, "\t%-32s %s\n", "SynchronousMode:",             TF[m_SynchronousMode]);
    Printf(pri, "\t%-32s %u\n", "TexCompressionMask:",          m_TexCompressionMask);
    Printf(pri, "\t%-32s %u\n", "TexIdxFarOffset:",             m_TexIdxFarOffset);
    Printf(pri, "\t%-32s %u\n", "TexIdxStride:",                m_TexIdxStride);
    Printf(pri, "\t%-32s %u\n", "TexReadsPerDraw:",             m_TexReadsPerDraw);
    Printf(pri, "\t%-32s 0x%08x\n", "TileMask:",                m_TileMask);
    Printf(pri, "\t%-32s %s\n", "TxMode:",                      GetTxModeStr(static_cast<TxMode>(m_TxMode))); //$
    Printf(pri, "\t%-32s %f\n", "TxD:",                         m_TxD);
    Printf(pri, "\t%-32s %s\n", "UseCompute:",                  TF[m_UseCompute]);
    Printf(pri, "\t%-32s %s\n", "UseHTex:",                     TF[m_UseHTex]);
    Printf(pri, "\t%-32s %s\n", "UseMeshlets:",                 TF[m_UseMeshlets]);
    Printf(pri, "\t%-32s %s\n", "UseMipMapping:",               TF[m_UseMipMapping]);
    Printf(pri, "\t%-32s %s\n", "UseRandomTextureData:",        TF[m_UseRandomTextureData]);
    Printf(pri, "\t%-32s %s\n", "UseTessellation:",             TF[m_UseTessellation]);
    Printf(pri, "\t%-32s %u\n", "WarpsPerSM:",                  m_WarpsPerSM);
    Printf(pri, "\t%-32s %s\n", "Ztest:",                       TF[m_Ztest]);

    const PulseMode pm = static_cast<PulseMode>(m_PulseMode);
    if (pm != NO_PULSE)
    {
        Printf(pri, "VkStressPulse Js Properties:\n");
        Printf(pri, "\t%-32s %.3f\n", "DutyPct:",               m_DutyPct);
        Printf(pri, "\t%-32s %.3f\n", "HighHz:",                m_HighHz);
        Printf(pri, "\t%-32s %.1f\n", "FMaxAverageDutyPctMax:", m_FMaxAverageDutyPctMax);
        Printf(pri, "\t%-32s %.1f\n", "FMaxAverageDutyPctMin:", m_FMaxAverageDutyPctMin);
        Printf(pri, "\t%-32s %u\n",   "FMaxConstTargetMHz:",    m_FMaxConstTargetMHz);
        Printf(pri, "\t%-32s %u\n",   "FMaxCtrlHz:",            m_FMaxCtrlHz);
        Printf(pri, "\t%-32s %.6f\n", "FMaxForcedDutyInc:",     m_FMaxForcedDutyInc);
        Printf(pri, "\t%-32s %.6f\n", "FMaxFraction:",          m_FMaxFraction);
        Printf(pri, "\t%-32s %.6f\n", "FMaxGainP:",             m_FMaxGainP);
        Printf(pri, "\t%-32s %.6f\n", "FMaxGainI:",             m_FMaxGainI);
        Printf(pri, "\t%-32s %.6f\n", "FMaxGainD:",             m_FMaxGainD);
        Printf(pri, "\t%-32s %u\n",   "FMaxPerfTarget:",        m_FMaxPerfTarget);
        Printf(pri, "\t%-32s %s\n",   "FMaxWriteCsv:",          TF[m_FMaxWriteCsv]);
        Printf(pri, "\t%-32s %.3f\n", "OctavesPerSecond:",      m_OctavesPerSecond);
        Printf(pri, "\t%-32s %s\n",   "PulseMode:",             GetPulseModeStr(pm));
        Printf(pri, "\t%-32s %.3f\n", "LowHz:",                 m_LowHz);
        Printf(pri, "\t%-32s %.3f\n", "StepHz:",                m_StepHz);
        Printf(pri, "\t%-32s %llu\n", "StepTimeUs:",            m_StepTimeUs);
    }
    if (pm == RANDOM)
    {
        Printf(pri, "\t%-32s %.3f\n", "RandMaxDutyPct",         m_RandMaxDutyPct);
        Printf(pri, "\t%-32s %.3f\n", "RandMaxFreqHz",          m_RandMaxFreqHz);
        Printf(pri, "\t%-32s %llu\n", "RandMaxLengthMs",        m_RandMaxLengthMs);
        Printf(pri, "\t%-32s %.3f\n", "RandMinDutyPct",         m_RandMinDutyPct);
        Printf(pri, "\t%-32s %.3f\n", "RandMinFreqHz",          m_RandMinFreqHz);
        Printf(pri, "\t%-32s %llu\n", "RandMinLengthMs",        m_RandMinLengthMs);
    }

    // Print the Raytacer JS properties if they exist.
    if (m_Rt.get())
    {
        m_Rt->PrintJsProperties(pri);
    }
}

//-------------------------------------------------------------------------------------------------
RC VkStress::CorruptImage()
{
    if (m_CorruptionLocationPercentOfSize >= 100)
    {
        Printf(Tee::PriError, "CorruptionLocationPercentOfSize must be less than 100\n");
        return RC::ILWALID_ARGUMENT;
    }

    RC rc;
    UINT32 dupMemHandle = 0;

    m_CorruptionSwapchainIdx = m_LwrrentSwapChainIdx;

    VulkanImage *pImage = (m_SampleCount > 1) ? m_MsaaImages[0].get() :
                         m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx);

    CHECK_RC(pImage->GetDupMemHandle(GetBoundGpuDevice(), &dupMemHandle));

    UINT64 corruptionOffset =
        m_CorruptionLocationPercentOfSize * pImage->GetDeviceMemorySize() / 100;

    // There is no easy way to determine location within a line as it is affected
    // by factors as block linear layout and the number of multi-samples,
    // so just hard code it to something that lwrrently shows as not being at the edge:
    corruptionOffset += m_CorruptionLocationOffsetBytes;

    UINT32 *addr;

    CHECK_RC(LwRmPtr()->MapMemory(dupMemHandle, corruptionOffset, 4,
        reinterpret_cast<void **>(&addr), 0, GetBoundGpuSubdevice()));

    addr[0] = 0xA55AA5;

    LwRmPtr()->UnmapMemory(dupMemHandle, addr, 0, GetBoundGpuSubdevice());
    return rc;
}
#endif
//-------------------------------------------------------------------------------------------------
RC VkStress::SetupDepthBuffer()
{
    RC rc;
    for (auto& depthImage : m_DepthImages)
    {
        depthImage->SetFormat(VK_FORMAT_D24_UNORM_S8_UINT);
        depthImage->SetImageProperties(
            m_RenderWidth,
            m_RenderHeight,
            1,      // mipmap levels
            VulkanImage::ImageType::DEPTH_STENCIL);

        // VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required for goldens/pngs
        CHECK_VK_TO_RC(depthImage->CreateImage(
            VkImageCreateFlags(0),
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_TILING_OPTIMAL,
            static_cast<VkSampleCountFlagBits>(m_SampleCount),
            "DepthImage"));
        CHECK_VK_TO_RC(depthImage->AllocateAndBindMemory(m_ZeroFb ?
                                                         VK_MEMORY_PROPERTY_SYSMEM_BIT :
                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        CHECK_VK_TO_RC(depthImage->SetImageLayout(
            m_InitCmdBuffer.get(),
            depthImage->GetImageAspectFlags(),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,   //new layout
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,       //new access
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT));
        CHECK_VK_TO_RC(depthImage->CreateImageView());
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::SetupMsaaImage()
{
    RC rc;

    if (!m_pVulkanDev->GetPhysicalDevice()->IsSampleCountSupported(m_SampleCount))
    {
        Printf(Tee::PriError, "SampleCount=%u unsupported (min %u, max %d)\n",
               m_SampleCount, 1, m_pVulkanDev->GetPhysicalDevice()->GetMaxSampleCount());
        return RC::ILWALID_ARGUMENT;
    }

    for (auto& msaaImage : m_MsaaImages)
    {
        msaaImage->SetFormat(m_SwapChain->GetSurfaceFormat());
        msaaImage->SetImageProperties(
            m_RenderWidth,
            m_RenderHeight,
            1,      // mipmap levels
            VulkanImage::ImageType::COLOR);

        CHECK_VK_TO_RC(msaaImage->CreateImage(
            VkImageCreateFlags(0),
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_TILING_OPTIMAL,
            static_cast<VkSampleCountFlagBits>(m_SampleCount),
            "MSAAImage"));
        CHECK_VK_TO_RC(msaaImage->AllocateAndBindMemory(m_ZeroFb ?
                                                       VK_MEMORY_PROPERTY_SYSMEM_BIT :
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

        CHECK_VK_TO_RC(msaaImage->SetImageLayout(
            m_InitCmdBuffer.get(),
            VK_IMAGE_ASPECT_COLOR_BIT,                        // new aspect
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,         // new layout
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,             // new access
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT));
        CHECK_VK_TO_RC(msaaImage->CreateImageView());
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::SetupBuffers()
{
    if (m_CameraScaleFactor <= 0.0f)
    {
        Printf(Tee::PriError, "CameraScaleFactor must be greater than 0\n");
        return RC::ILWALID_ARGUMENT;
    }

    if (m_FarZMultiplier < 1.0f)
    {
        Printf(Tee::PriError, "FarZMultiplier must be greater than or equal to 1.0\n");
        return RC::ILWALID_ARGUMENT;
    }

    if (m_NearZMultiplier <= 0.0f)
    {
        Printf(Tee::PriError, "NearZMultiplier must be greater than 0\n");
        return RC::ILWALID_ARGUMENT;
    }

    if (m_PpV <= 0.0)
    {
        Printf(Tee::PriError, "PpV must be greater than 0\n");
        return RC::ILWALID_ARGUMENT;
    }

    RC rc;

    //uniform buffer for matrix
    CHECK_RC(m_Camera.SetFrustum(
        m_RenderWidth / -m_CameraScaleFactor,
        m_RenderWidth / m_CameraScaleFactor,
        m_RenderHeight / -m_CameraScaleFactor,
        m_RenderHeight / m_CameraScaleFactor,
        100,
        200 + m_FarZMultiplier * 100));
    m_Camera.SetCameraPosition(0, 0, 0); //Scene is at origin and camera at (0, 0, zpos)

    // Get a model matrix that translates x, y, z but no rotation or scaling
    GLMatrix modelMatrix =
        GLCamera::GetModelMatrix(m_RenderWidth/-2.0f, m_RenderHeight/-2.0f, -200.0f, 0, 0, 0, 1, 1, 1);
    GLMatrix mvpMatrix = m_Camera.GetMVPMatrix(modelMatrix);
    if (m_BoringXform)
    {
        mvpMatrix.MakeIdentity();
    }
    UINT32 mvpMatrixSize = sizeof(mvpMatrix.mat);

    // send the Model/View/Projection Matrix to the vertex shader via the Uniform Buffer.
    CHECK_VK_TO_RC(m_HUniformBufferMVPMatrix->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        ,mvpMatrixSize
        ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(m_HUniformBufferMVPMatrix->SetData(&mvpMatrix.mat[0][0], mvpMatrixSize, 0));

    CHECK_VK_TO_RC(m_UniformBufferMVPMatrix->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,mvpMatrixSize
        ,m_ZeroFb ? VK_MEMORY_PROPERTY_SYSMEM_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get(),
                                        m_HUniformBufferMVPMatrix.get(),
                                        m_UniformBufferMVPMatrix.get()));
    m_HUniformBufferMVPMatrix->Cleanup();

    CHECK_RC(SetupLights());

    //Geometry
    vector<UINT32> indices;
    vector<vtx_f4_f4_f4_f2> vertices; // pos, norm, col, texcoord
    CHECK_RC(CalcGeometrySizePlanes());

    GenGeometryPlanes(&indices, &vertices);

    // Create interleaved buffer of vertex attributes
    VulkanBuffer hostVB(m_pVulkanDev);
    UINT32 bufferSize =
        static_cast<UINT32>(vertices.size() * sizeof(vtx_f4_f4_f4_f2));
    CHECK_VK_TO_RC(hostVB.CreateBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        ,bufferSize
        ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(hostVB.SetData(vertices.data(), bufferSize, 0));

    const VkBufferUsageFlagBits verticesUsage = m_UseMeshlets ?
        static_cast<VkBufferUsageFlagBits>(0) : VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    CHECK_VK_TO_RC(m_VBVertices->CreateBuffer(
        verticesUsage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,bufferSize
        ,m_ZeroFb ? VK_MEMORY_PROPERTY_SYSMEM_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get()
                                        ,&hostVB
                                        ,m_VBVertices.get()));
    hostVB.Cleanup();

    if ( (m_PulseMode == TILE_MASK_GPC) || (m_PulseMode == TILE_MASK_GPC_SHUFFLE) )
    {
        VulkanBuffer hostSm2GpcBuffer(m_pVulkanDev);
        CHECK_VK_TO_RC(hostSm2GpcBuffer.CreateBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            ,sizeof(m_Sm2Gpc)
            ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        CHECK_VK_TO_RC(hostSm2GpcBuffer.SetData(&m_Sm2Gpc, sizeof(m_Sm2Gpc), 0));

        CHECK_VK_TO_RC(m_Sm2GpcBuffer->CreateBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            ,sizeof(m_Sm2Gpc)
            ,m_ZeroFb ? VK_MEMORY_PROPERTY_SYSMEM_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get()
                                            ,&hostSm2GpcBuffer
                                            ,m_Sm2GpcBuffer.get()));
        hostSm2GpcBuffer.Cleanup();
    }

    if (m_UseMeshlets)
        return rc;

    // Create index buffer
    // This is more efficient than just sending down the vertices because we are drawing a series
    // of strips using a triangle list (which duplicates many of the vertices)
    bufferSize = static_cast<UINT32>(indices.size() * sizeof(UINT32));
    CHECK_VK_TO_RC(m_HIndexBuffer->CreateBuffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        ,bufferSize
        ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(m_HIndexBuffer->SetData(indices.data(), bufferSize, 0));

    CHECK_VK_TO_RC(m_IndexBuffer->CreateBuffer(
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        ,bufferSize
        ,m_ZeroFb ? VK_MEMORY_PROPERTY_SYSMEM_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get()
                                        ,m_HIndexBuffer.get()
                                        ,m_IndexBuffer.get()));
    m_HIndexBuffer->Cleanup();

    //Set the input binding and attribute descriptions of vertex data
    VkVertexInputBindingDescription vertexBinding;
    vertexBinding.binding = m_VBBindingVertices;            //index 0
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexBinding.stride = sizeof(struct vtx_f4_f4_f4_f2);
    m_VBBindingAttributeDesc.m_BindingDesc.push_back(vertexBinding);

    VkVertexInputAttributeDescription attributesDesc;
    attributesDesc.binding = m_VBBindingVertices;
    attributesDesc.location = 0;
    attributesDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributesDesc.offset = offsetof(vtx_f4_f4_f4_f2, pos);
    m_VBBindingAttributeDesc.m_AttributesDesc.push_back(attributesDesc);

    attributesDesc.binding = m_VBBindingVertices;
    attributesDesc.location = 1;
    attributesDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributesDesc.offset = offsetof(vtx_f4_f4_f4_f2, norm);
    m_VBBindingAttributeDesc.m_AttributesDesc.push_back(attributesDesc);

    attributesDesc.binding = m_VBBindingVertices;
    attributesDesc.location = 2;
    attributesDesc.format = VK_FORMAT_R32G32_SFLOAT;
    attributesDesc.offset = offsetof(vtx_f4_f4_f4_f2, texcoord);
    m_VBBindingAttributeDesc.m_AttributesDesc.push_back(attributesDesc);

    if (m_SendColor)
    {
        attributesDesc.binding = m_VBBindingVertices;
        attributesDesc.location = 3;
        attributesDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributesDesc.offset = offsetof(vtx_f4_f4_f4_f2, col);
        m_VBBindingAttributeDesc.m_AttributesDesc.push_back(attributesDesc);
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::SetupLights() const
{
    if (m_NumLights > MAX_LIGHTS)
    {
        Printf(Tee::PriError, "m_NumLights=%u unsupported (max %u)\n", m_NumLights, MAX_LIGHTS);
        return RC::BAD_PARAMETER;
    }

    RC rc;

    LightUBO lightUBO = {};

    lightUBO.camera[3] = 1.0f;

    const FLOAT32 radius = static_cast<FLOAT32>(max(m_RenderWidth, m_RenderHeight));

    //                       x               y                z      w        r     g     b       radius    //$
    lightUBO.lights[0]  = { { m_RenderWidth*3/6.0f, m_RenderHeight*5/6.0f, 95.0f, 1.0f}, { 1.0F, 1.0F, 1.0F }, radius }; //$
    lightUBO.lights[1]  = { { m_RenderWidth*1/6.0f, m_RenderHeight*1/6.0f, 95.0f, 1.0f}, { 1.0F, 0.3F, 0.3F }, radius }; //$
    lightUBO.lights[2]  = { { m_RenderWidth*5/6.0f, m_RenderHeight*1/6.0f, 95.0f, 1.0f}, { 0.3F, 1.0F, 0.3F }, radius }; //$
    lightUBO.lights[3]  = { { m_RenderWidth*3/6.0f, m_RenderHeight*3/6.0f, 95.0f, 1.0f}, { 0.3F, 0.3F, 1.0F }, radius }; //$
    lightUBO.lights[4]  = { { m_RenderWidth*5/6.0f, m_RenderHeight*5/6.0f, 95.0f, 1.0f}, { 1.0F, 1.0f, 0.3F }, radius }; //$
    lightUBO.lights[5]  = { { m_RenderWidth*1/6.0f, m_RenderHeight*5/6.0f, 95.0f, 1.0f}, { 1.0F, 0.3F, 1.0F }, radius }; //$
    lightUBO.lights[6]  = { { m_RenderWidth*3/6.0f, m_RenderHeight*1/6.0f, 95.0f, 1.0f}, { 0.3F, 1.0F, 1.0F }, radius }; //$
    lightUBO.lights[7]  = { { m_RenderWidth*2/6.0f, m_RenderHeight*3/6.0f, 95.0f, 1.0f}, { 1.0F, 0.3F, 0.3F }, radius }; //$
    lightUBO.lights[8]  = { { m_RenderWidth*2/6.0f, m_RenderHeight*2/6.0f, 95.0f, 1.0f}, { 0.5F, 0.5F, 0.5F }, radius }; //$
    lightUBO.lights[9]  = { { m_RenderWidth*2/6.0f, m_RenderHeight*4/6.0f, 95.0f, 1.0f}, { 0.5F, 0.5F, 0.7F }, radius }; //$
    lightUBO.lights[10] = { { m_RenderWidth*4/6.0f, m_RenderHeight*2/6.0f, 95.0f, 1.0f}, { 0.5F, 0.7F, 0.5F }, radius }; //$
    lightUBO.lights[11] = { { m_RenderWidth*4/6.0f, m_RenderHeight*4/6.0f, 95.0f, 1.0f}, { 0.5F, 0.7F, 0.7F }, radius }; //$
    lightUBO.lights[12] = { { m_RenderWidth*3/6.0f, m_RenderHeight*4/6.0f, 95.0f, 1.0f}, { 0.7F, 0.5F, 0.5F }, radius }; //$
    lightUBO.lights[13] = { { m_RenderWidth*4/6.0f, m_RenderHeight*3/6.0f, 95.0f, 1.0f}, { 0.7F, 0.5F, 0.7F }, radius }; //$
    lightUBO.lights[14] = { { m_RenderWidth*4/6.0f, m_RenderHeight*5/6.0f, 95.0f, 1.0f}, { 0.7F, 0.7F, 0.5F }, radius }; //$
    lightUBO.lights[15] = { { m_RenderWidth*2/6.0f, m_RenderHeight*1/6.0f, 95.0f, 1.0f}, { 0.7F, 0.7F, 0.7F }, radius }; //$

    CHECK_VK_TO_RC(m_HUniformBufferLights->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        ,sizeof(lightUBO)
        ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(m_HUniformBufferLights->SetData(&lightUBO, sizeof(lightUBO), 0));

    CHECK_VK_TO_RC(m_UniformBufferLights->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,sizeof(lightUBO)
        ,m_ZeroFb ? VK_MEMORY_PROPERTY_SYSMEM_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get(),
                                        m_HUniformBufferLights.get(),
                                        m_UniformBufferLights.get()));
    m_HUniformBufferLights->Cleanup();

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::CalcGeometrySizePlanes()
{
    if (m_PpV < 250.0 && !m_UseTessellation && !m_UseMeshlets)
    {
        Printf(Tee::PriWarn, "Low PpV without tessellation or meshlets may increase setup time\n");
    }

    double numVertices = m_RenderWidth * m_RenderHeight / m_PpV;

    if (numVertices < 4.0)
    {
        // Minimum numVertices is 4 (we draw one big quad).
        numVertices = 4.0;
    }
    else if (numVertices > 1e6 && !m_UseTessellation && !m_UseMeshlets)
    {
        // Let's limit ourselves to no more than 1m vertices.
        numVertices = 1e6;
    }

    // Find a number of rows, columns of vertices that matches.
    m_Cols = static_cast<UINT32>(0.5 + m_RenderWidth / sqrt(m_PpV));
    if (m_Cols < 1)
        m_Cols = 1;

    m_Rows = static_cast<UINT32>(0.5 + numVertices / m_Cols);
    if (m_Rows < 1)
        m_Rows = 1;

    if (m_UseMeshlets)
    {
        if (m_Cols < (2 * MESH_INTERNAL_COLS))
        {
            m_Cols = 2 * MESH_INTERNAL_COLS;
        }
        m_Cols = ALIGN_UP(m_Cols, MESH_INTERNAL_COLS);

        if (m_Rows < (2 * MESH_INTERNAL_ROWS))
        {
            m_Rows = (2 * MESH_INTERNAL_ROWS);
        }
        m_Rows = ALIGN_UP(m_Rows, MESH_INTERNAL_ROWS);

        m_IdxPerPlane = 0;
        m_VxPerPlane = (m_Rows + 1) * (m_Cols + 1) -
            (m_Cols / MESH_INTERNAL_COLS) * (MESH_INTERNAL_COLS - 1) *
            (m_Rows / MESH_INTERNAL_ROWS) * (MESH_INTERNAL_ROWS - 1);
    }
    else if (m_UseTessellation)
    {
        // We will send only a few vertices to HW, and let the gpu turn
        // them into many for us.
        m_SpecializationData.yTessRate = static_cast<FLOAT32>(m_Rows);
        m_SpecializationData.xTessRate = static_cast<FLOAT32>(m_Cols);
        m_Cols = 1;
        m_Rows = 1;
        m_IdxPerPlane = m_Rows * m_Cols * 4;
        m_VxPerPlane  = (m_Rows+1) * (m_Cols+1);

        VerbosePrintf("%s will use m_XTessRate = %.0f and YTessRate = %.0f\n",
            GetName().c_str(),
            m_SpecializationData.xTessRate,
            m_SpecializationData.yTessRate);
    }
    else
    {
        m_IdxPerPlane = m_Rows * m_Cols * 6;
        m_VxPerPlane  = (m_Rows+1) * (m_Cols+1);

        VerbosePrintf("%s will use %d rows and %d cols, X, Y step of %.3f, %.3f\n",
            GetName().c_str(),
            m_Rows,
            m_Cols,
            m_RenderWidth / static_cast<double>(m_Cols),
            m_RenderHeight / static_cast<double>(m_Rows));
    }

    m_NumVertexes = m_VxPerPlane * 2;
    m_NumIndexes  = m_IdxPerPlane * 2;

    VerbosePrintf("m_VxPerPlane:%d m_NumVertexes:%d m_IdxPerPlane:%d m_NumIndexes:%d\n",
            m_VxPerPlane, m_NumVertexes, m_IdxPerPlane, m_NumIndexes);
    return RC::OK;
}

//-------------------------------------------------------------------------------------------------
void VkStress::GenGeometryPlanes
(
    vector<UINT32>* pIndices
    ,vector<vtx_f4_f4_f4_f2>* pVertices
) const
{
    // Notes on the vertex transforms:  (assume !BoringXform, w=1024, h=768)
    //
    // GL_MODELVIEW is from glTranslated(-w/2, -h/2, -100)
    //
    //     1 0 0 -512
    //     0 1 0 -384
    //     0 0 1 -100
    //     0 0 0  1
    //
    // GL_PROJECTION is from glFrustum(-w/2, w/2,  -h/2, h/2,  100, 300)
    //
    //     0.1953 0       0    0
    //     0      0.2604  0    0
    //     0      0      -2 -300
    //     0      0      -1    0
    //
    // Note that the sequence of the vertices differs for near/far.
    // In the near plane, v[0] is upper-right (1024, 768) and v[n-1] is at (0, 0).
    // In the far plane,  v[0] is at (0, 0) and v[n-1] is at (1024, 768).
    //
    // Also, the quad-strip indices are sent in different order for near/far.
    //
    // Near plane
    // (object)            (world)           (clip)             (normalized)
    // LL:    0   0 99 1  -512 -384 -101 1  -100 -100 -98 101  -0.99 -0.99 -0.97
    // UL:    0 768 99 1  -512  384 -101 1  -100  100 -98 101  -0.99 -0.99 -0.97
    // LR: 1024   0 99 1   512 -384 -101 1   100 -100 -98 101   0.99 -0.99 -0.97
    // UR: 1024 768 99 1   512  384 -101 1   100  100 -98 101   0.99 -0.99 -0.97
    // Cen  512 384 49 1     0    0 -151 1     0    0   2 151   0     0     0.01
    //
    // Far plane
    // (object)            (world)           (clip)             (normalized)
    // LL:    0   0 89 1  -512 -384 -111 1  -100 -100 -78 111  -0.90 -0.90 -0.70
    // UL:    0 768 89 1  -512  384 -111 1  -100  100 -78 111  -0.90  0.90 -0.70
    // LR: 1024   0 89 1   512 -384 -111 1   100 -100 -78 111   0.90 -0.90 -0.70
    // UR: 1024 768 89 1   512  384 -111 1   100  100 -78 111   0.90  0.90 -0.70
    // Cen  512 384 39 1     0    0 -161 1     0    0  22 161   0     0     0.13
    //
    // When generating the Z values for the near and far planes we want values for
    // the near plane to be small enough so that the depth buffer shows all zeros.
    // Setting the initial value = 99 and using a multiplier of 0-40 to callwlate
    // the smaller Z values towards the center of the frame is fine.
    // For the far plane we set the initial value = 89 and using a multiplier > 2000
    // to callwlate smaller Z values towards the center of the frame will generate
    // non-zero values in the depth buffer for >25% of the pixels.

    // These colors come from the LWPU corporate brand guidelines for our
    // shades of green
    static constexpr FLOAT32 bgColors[3][3] =
    {
        { 0.462745f, 0.725490f, 0.0f }      // LWPU green (PANTONE 376C)
       ,{ 0.160784f, 0.278431f, 0.239216f } // LWPU green 2 (PANTONE 560C)
       ,{ 0.050980f, 0.321569f, 0.341176f } // LWPU green 3 (PANTONE 7476C)
    };

    // Fill in the vertex data:
    pVertices->resize(m_NumVertexes);
    UINT32 ivtxNear = 0;
    UINT32 ivtxFar  = m_VxPerPlane;
    for (UINT32 row=0;  row <= m_Rows;  row++)
    {
        const FLOAT32 y = row * (m_RenderHeight) / static_cast<FLOAT32>(m_Rows);

        for (UINT32 col=0;  col <= m_Cols; col++)
        {
            const FLOAT32 x = col * (m_RenderWidth) / static_cast<FLOAT32>(m_Cols);
            //                       pos      norm      color     texcoord
            vtx_f4_f4_f4_f2 v =
                { { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 }, { 0, 0 }};

            // Color is arbitrary, so cycle through shades of LWPU green :)
            v.col[0] = bgColors[col%3][0];
            v.col[1] = bgColors[col%3][1];
            v.col[2] = bgColors[col%3][2];

            // Normal points towards camera (& light) at center,
            // 45% away from camera at center of each edge.
            // Lighting will make center 100% brightness, dimming towards corners.
            v.norm[0] = x / static_cast<FLOAT32>(m_RenderWidth) - 0.5f;
            v.norm[1] = y / static_cast<FLOAT32>(m_RenderHeight) - 0.5f;
            v.norm[2] = sqrt(1.0f - v.norm[0]*v.norm[0] - v.norm[1]*v.norm[1]);

            if (m_BoringXform)
            {
                v.pos[0] = x / (m_RenderWidth/2.0f) - 1.0f;
                v.pos[1] = y / (m_RenderHeight/2.0f) - 1.0f;
                v.pos[2] = 0.1f;
                v.texcoord[0] = v.pos[0] / m_RenderWidth;
                v.texcoord[1] = v.pos[1] / m_RenderHeight;
                StoreVertex(v, ivtxNear, pVertices);
                ivtxNear++;

                v.pos[0] *= 0.9f;
                v.pos[1] *= 0.9f;
                v.pos[2] = 0.2f;
                v.texcoord[0] = v.pos[0] / m_RenderWidth;
                v.texcoord[1] = v.pos[1] / m_RenderHeight;
                StoreVertex(v, ivtxFar, pVertices);
                ivtxFar++;
            }
            else
            {
                // Z ranges from 100 at the near view-plane to -100 at the far.
                // We have to almost touch the near view-plane at the edges
                // to draw the whole screen when using a perspective projection.
                // Use "farther" (smaller) Z values towards the center of the screen.
                FLOAT32 z;

                // Callwlate the distance from the edge of the screen to the center.
                // edge = 0.0, center = 1.0.
                const float xDist = 1.0f - fabs((x - m_RenderWidth/2.0f)  / (m_RenderWidth/2.0f));
                const float yDist = 1.0f - fabs((y - m_RenderHeight/2.0f) / (m_RenderHeight/2.0f));

                // We want the far plane to generate non-zero values in >=25% of the depth buffer
                // So keep the multiplier >= 1.0
                if (xDist > yDist)
                    z = 89 - (yDist * m_FarZMultiplier * 100);
                else
                    z = 89 - (xDist * m_FarZMultiplier * 100);
                v.pos[0] = x;
                v.pos[1] = y;
                v.pos[2] = z;
                v.texcoord[0] = v.pos[0] / m_MaxTextureSize * powf(2.0f, m_TxD); // scale S
                v.texcoord[1] = v.pos[1] / m_MaxTextureSize * powf(2.0f, m_TxD); // scale T

                if (!m_UseMeshlets || ((row % MESH_INTERNAL_ROWS) == 0) ||
                    ((col % MESH_INTERNAL_COLS) == 0))
                {
                    StoreVertex(v, ivtxFar, pVertices);
                    ivtxFar++;
                }

                v.pos[0] = (m_RenderWidth) - x;
                v.pos[1] = (m_RenderHeight) - y;
                // We don't want the near plane to generate any non-zero values in the depth buffer
                // buffer, otherwise the CheckSurface() will fail. So keep the multiplier <= .4
                if (xDist > yDist)
                    z = 99 - (yDist * m_NearZMultiplier * 100);
                else
                    z = 99 - (xDist * m_NearZMultiplier * 100);

                v.pos[2] = z;
                v.norm[0] *= -1.0f;
                v.norm[1] *= -1.0f;
                v.texcoord[0] = v.pos[0] / m_MaxTextureSize * powf(2.0f, m_TxD); // normalize S
                v.texcoord[1] = v.pos[1] / m_MaxTextureSize * powf(2.0f, m_TxD); // normalize T

                if (!m_UseMeshlets || ((row % MESH_INTERNAL_ROWS) == 0) ||
                    ((col % MESH_INTERNAL_COLS) == 0))
                {
                    StoreVertex(v, ivtxNear, pVertices);
                    ivtxNear++;
                }
            }
        }
    }

    if (m_UseMeshlets)
    {
        return;
    }

    if (!m_UseTessellation)
    {
        // Fill in the indexes, both planes:
        // Note: Vulkan does not have support for QUAD_STRIPS so we have to decompose each QUAD
        //       into 2 triangles while keeping the winding correct, clock-wise.
        //UINT32 * pidxNear = (UINT32 *) glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_WRITE);
        pIndices->resize(m_NumIndexes);
        UINT32 * pidxNear = pIndices->data();
        UINT32 * pidxFar  = pidxNear + m_IdxPerPlane;

        MASSERT(pIndices->size() == ((6 + 6) * m_Rows * m_Cols));
        MASSERT(pIndices->size() == (2 * m_IdxPerPlane));
        for (UINT32 t = 0; t < m_Rows; t++)
        {
            MASSERT((pidxNear + 5) < (pIndices->data() + m_IdxPerPlane));
            MASSERT((pidxFar + 5) < (pIndices->data() + 2 * m_IdxPerPlane));

            pidxFar[0]  = m_VxPerPlane + (t+1)*(m_Cols+1);  // 1st triangle of quad in far plane
            pidxFar[1]  = m_VxPerPlane + (t)  *(m_Cols+1);
            pidxFar[2]  = pidxFar[0] + 1;
            pidxFar[3]  = pidxFar[2];                       // 2nd triangle of quad in far plane
            pidxFar[4]  = pidxFar[1];
            pidxFar[5]  = pidxFar[4] + 1;

            pidxNear[0] = (t)  *(m_Cols+1);                 // 1st triangle of quad in near plane
            pidxNear[1] = (t+1)*(m_Cols+1);
            pidxNear[2] = pidxNear[0] + 1;
            pidxNear[3] = pidxNear[2];                      // 2nd triangle of quad in near plane
            pidxNear[4] = pidxNear[1];
            pidxNear[5] = pidxNear[4] + 1;

            pidxFar += 6;
            pidxNear += 6;

            for (UINT32 s = 1; s < m_Cols; s++)
            {
                MASSERT((pidxNear + 5) < (pIndices->data() + m_IdxPerPlane));
                MASSERT((pidxFar + 5) < (pIndices->data() + 2 * m_IdxPerPlane));

                pidxFar[0] = pidxFar[-6] + 1;               // 1st triangle
                pidxFar[1] = pidxFar[-5] + 1;
                pidxFar[2] = pidxFar[-4] + 1;
                pidxFar[3] = pidxFar[-3] + 1;               // 2nd triangle
                pidxFar[4] = pidxFar[-2] + 1;
                pidxFar[5] = pidxFar[-1] + 1;

                pidxNear[0] = pidxNear[-6] + 1;             // 1st triangle
                pidxNear[1] = pidxNear[-5] + 1;
                pidxNear[2] = pidxNear[-4] + 1;
                pidxNear[3] = pidxNear[-3] + 1;             // 2nd triangle
                pidxNear[4] = pidxNear[-2] + 1;
                pidxNear[5] = pidxNear[-1] + 1;
                pidxFar += 6;
                pidxNear += 6;
            }
        }
        PrintIndexData(*pIndices, *pVertices);
    }
    else
    {
        pIndices->resize(m_NumIndexes);
        UINT32 * pidxNear = pIndices->data();
        UINT32 * pidxFar = pidxNear + m_IdxPerPlane;

        MASSERT(pIndices->size() == ((4 + 4) * m_Rows));
        MASSERT(pIndices->size() == (2 * m_IdxPerPlane));
        for (UINT32 t = 0; t < m_Rows; t++)
        {
            MASSERT((pidxNear + 3) < (pIndices->data() + m_IdxPerPlane));
            MASSERT((pidxFar + 3) < (pIndices->data() + 2 * m_IdxPerPlane));

            pidxFar[0] = m_VxPerPlane + (t)    *(m_Cols + 1);
            pidxFar[1] = pidxFar[0] + 1;
            pidxFar[2] = m_VxPerPlane + (t + 1)*(m_Cols + 1);
            pidxFar[3] = pidxFar[2] + 1;

            pidxNear[0] = (t)    *(m_Cols + 1);
            pidxNear[1] = (t + 1)*(m_Cols + 1);
            pidxNear[2] = pidxNear[0] + 1;
            pidxNear[3] = pidxNear[1] + 1;

            pidxFar += 4;
            pidxNear += 4;
        }
        PrintIndexData(*pIndices, *pVertices);
    }
}

//-------------------------------------------------------------------------------------------------
// This feature is really intended to debug the geometry and will only print if m_Debug is
// properly set.
void VkStress::PrintIndexData
(
    vector<UINT32>& indices
    ,vector<vtx_f4_f4_f4_f2>& vertices
) const
{
    if (m_Debug & DBG_GEOMETRY)
    {
        UINT32 farIdx;
        UINT32 nearIdx;
        MASSERT(indices.size() == m_IdxPerPlane*2);

        VerbosePrintf("Index buffer:\n%-27s %-27s\n", "Far", "Near");
        for (nearIdx = 0, farIdx = m_IdxPerPlane; nearIdx < m_IdxPerPlane; nearIdx++, farIdx++)
        {
            VerbosePrintf("%4.4d {%6.1f,%6.1f,%3.1f}  %4.4d {%6.1f,%6.1f,%3.1f}\n" //$
                   ,indices [farIdx]
                   ,vertices[indices[farIdx]].pos[0]
                   ,vertices[indices[farIdx]].pos[1]
                   ,vertices[indices[farIdx]].pos[2]
                   ,indices[nearIdx]
                   ,vertices[indices[nearIdx]].pos[0]
                   ,vertices[indices[nearIdx]].pos[1]
                   ,vertices[indices[nearIdx]].pos[2]
                  );
        }
    }
}

RC VkStress::ReadSm2Gpc()
{
    RC rc;
    // For now the arrau is used only for gpc tile masking, but in future
    // it may serve to improve error reporting.
    if ( (m_PulseMode != TILE_MASK_GPC) && (m_PulseMode != TILE_MASK_GPC_SHUFFLE) )
        return rc;

#ifdef VULKAN_STANDALONE
    // Simulate a GPU with 70 SMs for debugging:
    m_Sm2Gpc = {
          0, 0, 1, 1, 2, 2, 3, 3, 5, 5, 4, 4,
          0, 0, 1, 1, 2, 2, 3, 3, 5, 5, 4, 4,
          0, 0, 1, 1, 2, 2, 3, 3, 5, 5, 4, 4,
          0, 0, 1, 1, 2, 2, 3, 3, 5, 5, 4, 4,
          0, 0, 1, 1, 2, 2, 3, 3, 5, 5, 0, 0,
          1, 1, 2, 2, 3, 3, 4, 4, 5, 5 };

#else
    m_Sm2Gpc = {};

    LW2080_CTRL_GR_GET_GLOBAL_SM_ORDER_PARAMS smOrder = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetBoundGpuSubdevice(),
        LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER,
        &smOrder, sizeof(smOrder)));

    static_assert(MAX_SMS <= LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER_MAX_SM_COUNT,
        "MAX_SMS can't be larger than LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER_MAX_SM_COUNT");
    for (UINT32 idx = 0; idx < MAX_SMS; idx++)
    {
        m_Sm2Gpc.gpcId[idx] = smOrder.globalSmId[idx].gpcId;
    }
#endif

    m_HighestGPCIdx = 0;
    for (UINT32 idx = 0; idx < MAX_SMS; idx++)
    {
        if ( m_Sm2Gpc.gpcId[idx] > m_HighestGPCIdx)
        {
            m_HighestGPCIdx = m_Sm2Gpc.gpcId[idx];
        }
    }
    if (m_NumActiveGPCs > (m_HighestGPCIdx + 1))
    {
        m_NumActiveGPCs = m_HighestGPCIdx + 1;
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
void VkStress::StoreVertex
(
    const VkStress::vtx_f4_f4_f4_f2 & bigVtx
    ,UINT32 index
    ,vector<vtx_f4_f4_f4_f2>* pVertices
) const
{
    static bool bFirstTime = true;
    static char header[] = "IdxFar  {       pos        },{      norm       },{ TexCoord }::IdxNear{       pos        },{      norm       },{ TexCoord }"; //$
    if (m_Debug & DBG_GEOMETRY)
    {
        if (bFirstTime)
        {
            VerbosePrintf("%s\n", header);
            bFirstTime = false;
        }
        VerbosePrintf("%07d {%6.1f,%6.1f,%3.1f},{%5.2f,%5.2f,%5.2f},{%5.2f,%1.2f} ", //$
               index,
               bigVtx.pos[0],  bigVtx.pos[1],  bigVtx.pos[2],
               bigVtx.norm[0], bigVtx.norm[1], bigVtx.norm[2],
               bigVtx.texcoord[0], bigVtx.texcoord[1]);
        if (index < m_VxPerPlane)
            VerbosePrintf("\n");
    }

    (*pVertices)[index] = bigVtx;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::SetupSamplers()
{
    RC rc;

    const auto& vulkanPhysDev = m_pVulkanDev->GetPhysicalDevice();
    VkBool32 anisotropyEnable = VK_FALSE;
    FLOAT32 maxAnisotropy = 0.0f;
    if (m_AnisotropicFiltering)
    {
        if (!vulkanPhysDev->GetFeatures2().features.samplerAnisotropy)
        {
#ifndef VULKAN_STANDALONE
            Printf(Tee::PriError, "The Vulkan driver does not support anisotropic filtering\n");
            return RC::UNSUPPORTED_FUNCTION;
#else
            m_AnisotropicFiltering = false;
#endif
        }
        else
        {
            anisotropyEnable = VK_TRUE;
            maxAnisotropy = vulkanPhysDev->GetLimits().maxSamplerAnisotropy;
        }
    }

    const VkFilter filter = m_LinearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    const VkSamplerMipmapMode mipmapMode = m_UseMipMapping ?
        VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;

    // HTex Image Restrictions:
    // samplers must use addressing mode VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE.
    // VkImageCreateInfo::imageType must be VK_IMAGE_TYPE_2D or VK_IMAGE_TYPE_3D
    // VkImageCreateInfo::flags must NOT include VK_IMAGE_CREATE_LWBE_COMPATIBLE_BIT
    // VkImageCreateInfo::usage must NOT include VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    const VkSamplerAddressMode addressMode = m_UseHTex ?
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE :
        VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkSamplerCreateInfo info =
    {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO // sType;
        ,nullptr                            // pNext;
        ,0                                  // flags;
        ,filter                             // magFilter;
        ,filter                             // minFilter;
        ,mipmapMode                         // mipmapMode;
        ,addressMode                        // addressModeU;
        ,addressMode                        // addressModeV;
        ,addressMode                        // addressModeW;
        ,0.0f                               // mipLodBias;
        ,anisotropyEnable                   // anisotropyEnable;
        ,maxAnisotropy                      // maxAnisotropy;
        ,VK_FALSE                           // compareEnable;
        ,VK_COMPARE_OP_NEVER                // compareOp;
        ,0.0f                               // minLod;
        ,m_UseMipMapping ? 16.0f : 0.0f     // maxLod; allow up to 16 levels of mipmapping
        ,VK_BORDER_COLOR_INT_OPAQUE_WHITE   // borderColor;
        ,VK_FALSE                           // unnormalizedCoordinates
    };

    // Create a sampler for each texture read in the fragment shader
    // We need to create at least one sampler, even if it is unused, in
    // order to be compatible with our descriptor set.
    const UINT32 numSamplers = max(1U, m_TexReadsPerDraw);
    const UINT32 maxNumSamplers = min(vulkanPhysDev->GetLimits().maxSamplerAllocationCount,
        vulkanPhysDev->GetLimits().maxPerStageDescriptorSamplers);
    if (numSamplers > maxNumSamplers)
    {
        Printf(Tee::PriError, "TexReadsPerDraw (%u) is more than supported (%u)\n",
            numSamplers, maxNumSamplers);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    for (UINT32 samplerIdx = 0; samplerIdx < numSamplers; samplerIdx++)
    {
        m_Samplers.emplace_back(m_pVulkanDev);
        CHECK_VK_TO_RC(m_Samplers.back().CreateSampler(info));
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
// Note: The VkTest exelwtable pulls from a public version of vulkan.h that is available to the
//       general public. This feature has not been published and officially released yet. So
//       instead of sprinkling #ifdefs throughout the code I created an accessor here until its
//       released for general consumption. LEB
// TODO: Remove this once its officially available to the general public.
VkImageCreateFlags VkStress::GetImageCreateFlagsForHTex() const
{
#ifndef VULKAN_STANDALONE
    return (m_UseHTex ? VK_IMAGE_CREATE_CORNER_SAMPLED_BIT_LW : 0);
#else
    return 0;
#endif
}

//-------------------------------------------------------------------------------------------------
// Note1:Texture data that resides in device local memory can not be mapped as host visible. So
//       we have to create a host visible staging texture and use this to copy data to the
//       device-local textures.
// Note2:The PatternClass uses a VK_FORMAT_B8G8R8A8_UNORM or VK_FORMAT_B8G8R8A8_UINT format.
// Note3: When reading back the texture data from the device we have to use sub-optimal settings
//        for the initialLayout or the validation layers will error out. According to a
//        presentation from Piers Daniels our GPUs don't care about the layout and we should always
//        be using VK_IMAGE_LAYOUT_GENERAL.
//-------------------------------------------------------------------------------------------------
RC VkStress::SetupTextures()
{
    RC rc;

    // Validate texture test arguments
    const auto& limits = m_pVulkanDev->GetPhysicalDevice()->GetLimits();
    if (m_TxMode == TX_MODE_STATIC)
    {
        if (m_MaxTextureSize > limits.maxImageDimension2D ||
            m_MaxTextureSize == 0)
        {
            Printf(Tee::PriError, "MaxTextureSize must not be zero\n");
            Printf(Tee::PriError,
                "In TX_MODE_STATIC, MinTextureSize is ignored but "
                "MaxTextureSize (%u) must be within the accepted range (min %u, max %u)\n",
                m_MaxTextureSize, 1, limits.maxImageDimension2D);
            return RC::BAD_PARAMETER;
        }
    }
    else
    {
        if (m_MaxTextureSize > limits.maxImageDimension2D ||
            m_MinTextureSize == 0)
        {
            Printf(Tee::PriError,
                "MinTextureSize=%u and/or MaxTextureSize=%u are "
                "out of the accepted range (min %u, max %u)\n",
                m_MinTextureSize, m_MaxTextureSize, 1, limits.maxImageDimension2D);
            return RC::BAD_PARAMETER;
        }
        if (m_MinTextureSize > m_MaxTextureSize)
        {
            Printf(Tee::PriError,
                "MaxTextureSize (%u) must be larger than or equal to MinTextureSize (%u)\n",
                m_MaxTextureSize, m_MinTextureSize);
            return RC::BAD_PARAMETER;
        }
    }
    // TODO: Remove the constraint on the number of allocations once we have
    // suballocation working for VulkanImages
    const UINT32 maxNumTextures = std::min(limits.maxPerStageDescriptorSampledImages,
                                           limits.maxMemoryAllocationCount);
    if (m_NumTextures > maxNumTextures || !m_NumTextures)
    {
        Printf(Tee::PriError, "NumTextures=%u unsupported (min %u, max %u)\n",
               m_NumTextures, 1, maxNumTextures);
        return RC::BAD_PARAMETER;
    }

    if (m_TxMode == TX_MODE_INCREMENT && m_NumTextures <= 1)
    {
        Printf(Tee::PriError, "TX_MODE_INCREMENT requires NumTextures > 1)\n");
        return RC::BAD_PARAMETER;
    }

    // Turn on mipmap testing mode (replace every texture with plain colours)
    VulkanTexture::SetTestMipMapMode(m_UsePlainColorMipMaps);

    for (UINT32 texIdx = 0; texIdx < m_NumTextures; texIdx++)
    {
        UINT32 texSize;
        CHECK_RC(CalcTextureSize(texIdx, &texSize));

        //HTex Image Restrictions:
        //Samplers must use addressing mode VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE.
        //VkImageCreateInfo::imageType must be VK_IMAGE_TYPE_2D or VK_IMAGE_TYPE_3D
        //VkImageCreateInfo::flags must NOT include VK_IMAGE_CREATE_LWBE_COMPATIBLE_BIT
        //VkImageCreateInfo::usage must NOT include VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        m_Textures.emplace_back(make_unique<VulkanTexture>(m_pVulkanDev));
        auto& tex = *m_Textures.back();
        if (m_UseHTex)
        {   // When using HTex we want odd sized (non power of 2) textures.
            texSize |= 0x01;
        }
        tex.SetTextureDimensions(texSize, texSize, 1);
        if (m_TexCompressionMask & (1<<texIdx))
        {
            tex.SetFormat(VK_FORMAT_BC7_UNORM_BLOCK);
        }
        else
        {
            tex.SetFormat(VK_FORMAT_B8G8R8A8_UNORM);
        }
        tex.SetFormatPixelSizeBytes(4);
        tex.SetUseMipMap(m_UseMipMapping);

        CHECK_VK_TO_RC(tex.AllocateImage(
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            ,GetImageCreateFlagsForHTex()
            ,m_ZeroFb ? VK_MEMORY_PROPERTY_SYSMEM_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            ,VK_IMAGE_LAYOUT_UNDEFINED
            ,VK_IMAGE_TILING_OPTIMAL
            ,"DeviceTexture1"));

        // Before we can use the texture filler class we have to make sure the ImageLayout
        // is correct. This is because the transfer queue can't change the ImageLayout in all
        // cases.
        auto & image = tex.GetTexImage();
        CHECK_VK_TO_RC(image.SetImageLayout(m_InitCmdBuffer.get()
                       ,image.GetImageAspectFlags()
                       ,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                       ,VK_ACCESS_TRANSFER_WRITE_BIT
                       ,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT));

        if (!tex.IsSamplingSupported())
        {
            return RC::MODS_VK_ERROR_FORMAT_NOT_SUPPORTED;
        }
    }

    // Fill each texture with a different pattern
    VulkanTextureFiller texFiller;
    CHECK_RC(texFiller.Setup(m_pVulkanDev,
                             0, // transferQueueIdx required by the PaternFiller
                             0, // graphicsQueueIdx required by the PatternFiller
                             0, // computeQueueIdx required by the ComputeFiller
                             GetTestConfiguration()->Seed(),
                             m_UseRandomTextureData));
    CHECK_RC(texFiller.Fill(m_Textures));

    return rc;
}

RC VkStress::CalcTextureSize(const UINT32 texIdx, UINT32* pTxSize)
{
    RC rc;
    MASSERT(texIdx < m_NumTextures && "Texture ID out-of-bounds");
    MASSERT(pTxSize);
    auto& chosenTextureSize = *pTxSize;

    switch (m_TxMode)
    {
        case TX_MODE_STATIC:
            // In this mode, we use the value of MaxTextureSize
            chosenTextureSize = m_MaxTextureSize;
            break;
        case TX_MODE_INCREMENT:
        case TX_MODE_RANDOM:
        {
            MASSERT(m_MinTextureSize <= m_MaxTextureSize &&
                    "Max texture size must be larger than min texture size");

            const auto minTextureSize = NearestLowerPowerOf2(m_MinTextureSize);
            const auto maxTextureSize = NearestLowerPowerOf2(m_MaxTextureSize);
            // In order to use all powers of 10 equally, we must work in log domain
            const FLOAT32 logRangeOfTextures = static_cast<FLOAT32>(
                                               log2(maxTextureSize) - log2(minTextureSize));

            UINT32 valueInRange = 0;
            if (m_TxMode == TX_MODE_INCREMENT)
            {
                // Choose texture size in range proportionally to ratio lwrrentTexId/maxTexId
                const FLOAT32 ratio = static_cast<FLOAT32>(texIdx) /
                                      static_cast<FLOAT32>(m_NumTextures - 1);
                valueInRange = static_cast<UINT32>(round(ratio * logRangeOfTextures));
            }
            else // RANDOM
            {
                valueInRange = GetFpContext()->Rand.GetRandom(0,
                                                static_cast<UINT32>(logRangeOfTextures));
            }
            const UINT32 powerOf2ToUse = static_cast<UINT32>(log2(minTextureSize)) + valueInRange;
            chosenTextureSize = (1U << powerOf2ToUse);
            MASSERT(chosenTextureSize <= m_MaxTextureSize &&
                    chosenTextureSize >= minTextureSize &&
                    "Something is wrong in the algorithm...");
            break;
        }
        default:
        {
            chosenTextureSize = m_MaxTextureSize;
            Printf(Tee::PriError, "Unknown TxMode\n");
            return RC::ILWALID_ARGUMENT;
        }
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::CreateRenderPass() const
{
    RC rc;

    // Add attachments
    VkAttachmentDescription colorAttachment = { };
    colorAttachment.format = m_SwapChain->GetSurfaceFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    if (m_SampleCount > 1)
    {
        // This attachment becomes resolve element which
        // gets automatically overwritten (loadOp is effectively ignored):
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    else
    {
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }
    //storing is necessary for display:
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    m_RenderPass->PushAttachmentDescription(AttachmentType::COLOR, &colorAttachment);

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    m_ClearRenderPass->PushAttachmentDescription(AttachmentType::COLOR, &colorAttachment);

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.samples = static_cast<VkSampleCountFlagBits>(m_SampleCount);
    depthAttachment.format = m_DepthImages[0]->GetFormat();
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    m_RenderPass->PushAttachmentDescription(AttachmentType::DEPTH, &depthAttachment);

    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //clear depth
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    m_ClearRenderPass->PushAttachmentDescription(AttachmentType::DEPTH, &depthAttachment);

    // MSAA image
    if (m_SampleCount > 1)
    {
        colorAttachment.samples = static_cast<VkSampleCountFlagBits>(m_SampleCount);
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        // We have to store so it can be used as input to the next frame.
        // MODS didn't see the problem (possibly because nothing had a chance
        // to modify the buffer), but vktest.exe shows visible corruption in second
        // and subsequent frames when it is set to STORE_OP_DONT_CARE:
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        m_RenderPass->PushAttachmentDescription(AttachmentType::COLOR, &colorAttachment);

        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        m_ClearRenderPass->PushAttachmentDescription(
            AttachmentType::COLOR, &colorAttachment);
    }

    // Add subpasses
    VkSubpassDescription subpassDesc = {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;

    VkAttachmentReference resolveOrColorReference[1];
    resolveOrColorReference[0].attachment = 0;
    resolveOrColorReference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference[1];
    depthReference[0].attachment = 1;
    depthReference[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorMsaaReference[1];
    colorMsaaReference[0].attachment = 2;
    colorMsaaReference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    if (m_SampleCount == 1)
    {
        subpassDesc.pColorAttachments       = resolveOrColorReference;
        subpassDesc.pDepthStencilAttachment = depthReference;
    }
    else
    {
        subpassDesc.pColorAttachments       = colorMsaaReference;
        subpassDesc.pDepthStencilAttachment = depthReference;
        subpassDesc.pResolveAttachments     = resolveOrColorReference;
    }

    m_RenderPass->PushSubpassDescription(&subpassDesc);
    CHECK_VK_TO_RC(m_RenderPass->CreateRenderPass());

    m_ClearRenderPass->PushSubpassDescription(&subpassDesc);
    CHECK_VK_TO_RC(m_ClearRenderPass->CreateRenderPass());

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::SetupDescriptors()
{
    RC rc;

    const UINT32 maxDescriptorSets = 1; //using only 1 descriptor set

    vector<VkDescriptorSetLayoutBinding> layoutBindings;
    layoutBindings.reserve(8);

    const auto PushLayoutBinding = [&layoutBindings]
    (
        UINT32             index,
        VkDescriptorType   descriptorType,
        UINT32             descriptorCount,
        VkShaderStageFlags stageFlags
    )
    {
        VkDescriptorSetLayoutBinding binding = { };
        binding.binding            = index;
        binding.descriptorType     = descriptorType;
        binding.descriptorCount    = descriptorCount;
        binding.stageFlags         = stageFlags;
        binding.pImmutableSamplers = nullptr;

        layoutBindings.push_back(binding);
    };

    const VkShaderStageFlags meshFlag = m_UseMeshlets ? VK_SHADER_STAGE_MESH_BIT_LW
                                      : VkShaderStageFlags(0);

    // Descriptor 0: MVP Uniform Buffer Object (UBO) in VS
    PushLayoutBinding(DS_BINDING_MVP_UBO,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                      1,
                      VK_SHADER_STAGE_VERTEX_BIT | meshFlag);

    // Descriptor 1: Light UBO in VS/FS
    PushLayoutBinding(DS_BINDING_LIGHT_UBO,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                      1,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | meshFlag);

    // Descriptor 2: Samplers in FS
    PushLayoutBinding(DS_BINDING_SAMPLERS,
                      VK_DESCRIPTOR_TYPE_SAMPLER,
                      static_cast<UINT32>(m_Samplers.size()),
                      VK_SHADER_STAGE_FRAGMENT_BIT);

    // Descriptor 3: Textures in FS
    PushLayoutBinding(DS_BINDING_TEXTURES,
                      VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                      m_NumTextures,
                      VK_SHADER_STAGE_FRAGMENT_BIT);

    // Descriptor 4: CPU generated vertexes for Meshlets
    if (m_UseMeshlets)
    {
        PushLayoutBinding(DS_BINDING_MESHLET_VB,
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          1,
                          VK_SHADER_STAGE_MESH_BIT_LW);
    }

    // Descriptor 5: GPC Map
    if ( (m_PulseMode == TILE_MASK_GPC) || (m_PulseMode == TILE_MASK_GPC_SHUFFLE) )
    {
        PushLayoutBinding(DS_BINDING_SM2GPC,
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          1,
                          VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    const UINT32 numLayoutBindings = static_cast<UINT32>(layoutBindings.size());

    CHECK_VK_TO_RC(m_DescriptorInfo->CreateDescriptorSetLayout(0 // firstIndex
        ,numLayoutBindings
        ,&layoutBindings[0]));

    // List all the descriptors used
    vector<VkDescriptorPoolSize> descPoolSizeInfo =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }
       ,{ VK_DESCRIPTOR_TYPE_SAMPLER, static_cast<UINT32>(m_Samplers.size()) }
       ,{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_NumTextures }
       ,{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }
    };

    // Create descriptor pool
    CHECK_VK_TO_RC(m_DescriptorInfo->CreateDescriptorPool(maxDescriptorSets,
                                                          descPoolSizeInfo));
    //allocate descriptor sets
    CHECK_VK_TO_RC(m_DescriptorInfo->AllocateDescriptorSets(0, maxDescriptorSets, 0));

    //update the descriptors
    m_WriteDescriptorSets.resize(numLayoutBindings);

    //Setup all common fields
    for (UINT32 descIdx = 0; descIdx < numLayoutBindings; descIdx++)
    {
        m_WriteDescriptorSets[descIdx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        m_WriteDescriptorSets[descIdx].pNext = nullptr;
        m_WriteDescriptorSets[descIdx].dstSet = m_DescriptorInfo->GetDescriptorSet(0);
        m_WriteDescriptorSets[descIdx].dstArrayElement = 0;
        m_WriteDescriptorSets[descIdx].dstBinding = layoutBindings[descIdx].binding;
    }

    // Descriptor 0: MVP UBO in VS
    VkDescriptorBufferInfo bufferInfo = m_UniformBufferMVPMatrix->GetBufferInfo();
    m_WriteDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    m_WriteDescriptorSets[0].descriptorCount = 1;
    m_WriteDescriptorSets[0].pBufferInfo = &bufferInfo;

    // Descriptor 1: Light UBO in VS/FS
    VkDescriptorBufferInfo lightBufferInfo = m_UniformBufferLights->GetBufferInfo();
    m_WriteDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    m_WriteDescriptorSets[1].descriptorCount = 1;
    m_WriteDescriptorSets[1].pBufferInfo = &lightBufferInfo;

    // Descriptor 2: Samplers in FS
    vector<VkDescriptorImageInfo> samplerInfos = {};
    for (UINT32 samplerIdx = 0; samplerIdx < m_Samplers.size(); samplerIdx++)
    {
        samplerInfos.push_back(m_Samplers[samplerIdx].GetDescriptorImageInfo());
    }
    m_WriteDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    m_WriteDescriptorSets[2].descriptorCount = static_cast<UINT32>(m_Samplers.size());
    m_WriteDescriptorSets[2].pImageInfo = samplerInfos.data();

    // Descriptor 3: Textures in FS
    vector<VkDescriptorImageInfo> texInfos;
    texInfos.reserve(m_NumTextures);
    for (const auto& tex : m_Textures)
    {
        texInfos.push_back(tex->GetDescriptorImageInfo());
    }
    m_WriteDescriptorSets[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    m_WriteDescriptorSets[3].descriptorCount = m_NumTextures;
    m_WriteDescriptorSets[3].pImageInfo = texInfos.data();

    // Descriptor 4: CPU generated vertexes for Meshlets
    VkDescriptorBufferInfo verInfo;
    if (m_UseMeshlets)
    {
        verInfo = m_VBVertices->GetBufferInfo();
        m_WriteDescriptorSets[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        m_WriteDescriptorSets[4].descriptorCount = 1;
        m_WriteDescriptorSets[4].pBufferInfo = &verInfo;
    }

    // Descriptor 5: GPC Map
    VkDescriptorBufferInfo tilesInfo;
    if ( (m_PulseMode == TILE_MASK_GPC) || (m_PulseMode == TILE_MASK_GPC_SHUFFLE) )
    {
        const size_t setIdx = m_WriteDescriptorSets.size() - 1;
        tilesInfo = m_Sm2GpcBuffer->GetBufferInfo();
        m_WriteDescriptorSets[setIdx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        m_WriteDescriptorSets[setIdx].descriptorCount = 1;
        m_WriteDescriptorSets[setIdx].pBufferInfo = &tilesInfo;
    }

    // Multiple descriptors across multiple sets can be updated at once
    m_DescriptorInfo->UpdateDescriptorSets(m_WriteDescriptorSets.data(),
                                           static_cast<UINT32>(m_WriteDescriptorSets.size()));
    return rc;
}

#ifndef VULKAN_STANDALONE
#define GET_SET_JS_ARRAY(member) \
RC VkStress::Get##member(JsArray *val) const \
{ \
    JavaScriptPtr pJavaScript; \
    return pJavaScript->ToJsArray(m_##member, val); \
} \
RC VkStress::Set##member(JsArray *val) \
{ \
    JavaScriptPtr pJavaScript; \
    return pJavaScript->FromJsArray(*val, &m_##member); \
}

GET_SET_JS_ARRAY(ComputeShaderSelection)
GET_SET_JS_ARRAY(ComputeInnerLoops)
GET_SET_JS_ARRAY(ComputeOuterLoops)
GET_SET_JS_ARRAY(ComputeRuntimeClks)

#undef GET_SET_JS_ARRAY
#endif

//-------------------------------------------------------------------------------------------------
// Sets up the the VkShader objects
//
// Note: For debugging use "ShaderReplacement" mode
//
RC VkStress::SetupShaders()
{
    RC rc;

    if (m_UseMeshlets)
    {
        if (m_MeshTaskProg.empty())
        {
            CHECK_RC(SetupMeshletsTaskShader());
        }

        CHECK_VK_TO_RC(m_MeshletsTaskShader->CreateShader(
            VK_SHADER_STAGE_TASK_BIT_LW
            ,m_MeshTaskProg
            ,"main"
            ,m_ShaderReplacement));

        if (m_MeshMainProg.empty())
        {
            CHECK_RC(SetupMeshletMainShader());
        }
        CHECK_VK_TO_RC(m_MeshletsMainShader->CreateShader(
            VK_SHADER_STAGE_MESH_BIT_LW
            ,m_MeshMainProg
            ,"main"
            ,m_ShaderReplacement));
    }
    else
    {
        if (m_VtxProg.empty())
        {
            CHECK_RC(SetupVertexShader());
        }
        CHECK_VK_TO_RC(m_VertexShader->CreateShader(VK_SHADER_STAGE_VERTEX_BIT
            ,m_VtxProg
            ,"main"
            ,m_ShaderReplacement));
        if (m_UseTessellation)
        {
            if (!m_pVulkanDev->GetPhysicalDevice()->GetFeatures().tessellationShader)
            {
                Printf(Tee::PriError, "VkDevice does not support tessellation\n");
                return RC::UNSUPPORTED_FUNCTION;
            }
            if (m_TessCtrlProg.empty())
            {
                CHECK_RC(SetupTessellationControlShader());
            }
            CHECK_VK_TO_RC(m_TessellationControlShader->CreateShader(
                VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
                ,m_TessCtrlProg
                ,"main"
                ,m_ShaderReplacement));

            if (m_TessEvalProg.empty())
            {
                CHECK_RC(SetupTessellationEvaluationShader());
            }
            CHECK_VK_TO_RC(m_TessellationEvaluationShader->CreateShader(
                VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
                ,m_TessEvalProg
                ,"main"
                ,m_ShaderReplacement));
        }
    }

#ifdef VULKAN_STANDALONE_POWERPULSE
    extern const char* const s_PowerPulseConstantShader;
    VulkanShader constantShader(m_pVulkanDev);
    CHECK_VK_TO_RC(constantShader.CreateShader(VK_SHADER_STAGE_COMPUTE_BIT
                                               ,s_PowerPulseConstantShader
                                               ,"main"
                                               ,false));
#endif

    if (m_FragProg.empty())
    {
        CHECK_RC(SetupFragmentShader());
    }
    CHECK_VK_TO_RC(m_FragmentShader->CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT
                                                  ,m_FragProg
                                                  ,"main"
                                                  ,m_ShaderReplacement));

    if (m_UseCompute)
    {
        if (m_ComputeProgs.empty())
        {
            CHECK_RC(SetupComputeShader());
        }

        m_ComputeShaders.resize(m_ComputeProgs.size());
        for (UINT32 shaderIdx = 0; shaderIdx < m_ComputeProgs.size(); shaderIdx++)
        {
            m_ComputeShaders[shaderIdx] = make_unique<VulkanShader>(m_pVulkanDev);
        }

        for (UINT32 shaderIdx = 0; shaderIdx < m_ComputeProgs.size(); shaderIdx++)
        {
            VulkanShader* computeShader = m_ComputeShaders[shaderIdx].get();

            CHECK_VK_TO_RC(computeShader->CreateShader(VK_SHADER_STAGE_COMPUTE_BIT
                , m_ComputeProgs[shaderIdx]
                , "main"
                , m_ShaderReplacement));

            // Local workgroup size (defined in the shader code)
            UINT32 computeLocalSizeX = 0;
            UINT32 computeLocalSizeY = 0;

            CHECK_RC(computeShader->FindToken(string("local_size_x"),
                &computeLocalSizeX,
                computeShader->GetShaderSource()));
            CHECK_RC(computeShader->FindToken(string("local_size_y"),
                &computeLocalSizeY,
                computeShader->GetShaderSource()));

            if (computeLocalSizeX != 32 || computeLocalSizeY != m_WarpsPerSM)
            {
                Printf(Tee::PriError,
                    "Compute shader is missing local_size_x or local_size_y"
                    " or their values are incorrect.\n");
                Printf(Tee::PriError,
                    "Proper syntax is: \"layout (local_size_x = 32, local_size_y = %d) in;\"",
                    m_WarpsPerSM);
                return RC::BAD_PARAMETER;
            }
        }
        VerbosePrintf("groupCountX:%d groupCountY:1 local_size_x:%d local_size_y:%d \n",
                      m_NumSMs, WARP_SIZE, m_WarpsPerSM);

        if (!m_AsyncCompute)
        {
            //Load the computeLoaded shader that waits for the SMs to be loaded with compute threads
            UINT32 localX = 0;
            UINT32 localY = 0;
            if (m_ComputeLoadedProg.empty())
            {
                CHECK_RC(SetupComputeLoadedShader());
            }
            CHECK_VK_TO_RC(m_ComputeLoadedShader->CreateShader(VK_SHADER_STAGE_COMPUTE_BIT
                                                             ,m_ComputeLoadedProg
                                                             ,"main"
                                                             ,m_ShaderReplacement));
            CHECK_RC(m_ComputeLoadedShader->FindToken(string("local_size_x"),
                                                      &localX,
                                                      m_ComputeLoadedShader->GetShaderSource()));
            CHECK_RC(m_ComputeLoadedShader->FindToken(string("local_size_y"),
                                                      &localY,
                                                      m_ComputeLoadedShader->GetShaderSource()));
            if (localX != 32 || localY != 1)
            {
                Printf(Tee::PriError,
                    "ComputeLoaded shader is missing local_size_x or local_size_y"
                    " or their values are incorrect.\n");
                Printf(Tee::PriError,
                    "Proper syntax is: \"layout (local_size_x = 32, local_size_y = 1) in;\"");
                return RC::BAD_PARAMETER;
            }
        }
    }
    return rc;
}

RC VkStress::SetupMeshletsTaskShader()
{
    RC rc;
    // Just determine the number of meshlet tasks.
    // Each task handles one "column" and up to 31 "rows".
    m_MeshTaskProg =
    HS_("//Meshlet Task Shader\n"
        "#version 450\n"
        "#extension GL_LW_mesh_shader : require\n"
        "layout(local_size_x = 1) in;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    gl_TaskCountLW = ") + Utility::StrPrintf("%u;\n",
            m_Cols/MESH_INTERNAL_COLS * m_Rows/MESH_INTERNAL_ROWS) +
        "}\n";
    return rc;
}

// This is the second implementation of meshlet shaders as the first one didn't
// provide correct results across different PpV values (bugs 2549325 & 3164544).
// Gathered data shows that within a frame there are multiple ROP operations
// oclwrring on the same pixel with just slightly different z value used during
// ROP ztest. The difference is at 6th digit after decimal.
// This is causing the depth test to reject some ROP operations.
// The stencil operations were also affected.
//
// The new shader uses a mixture of vertices generated by the CPU placed on
// the edges of the mesh and generated by the shader that are all located
// inside of the mesh.
// The original theory was that the extra ROP operations are coming only from
// edge overlaps between meshes. However testing showed that only disabling
// mesh generated vertices (MESH_INTERNAL_COLS=1, MESH_INTERNAL_ROWS=1) removed
// the different z values.

// For more information about the shader refer to this page:
// https://confluence.lwpu.com/display/GM/Meshlets#Meshlets-Newimplementation(December2020)

RC VkStress::SetupMeshletMainShader()
{
    RC rc;
    m_MeshMainProg =
    HS_("//Meshlet Main Shader\n"
        "#version 450\n"
        "#extension GL_LW_mesh_shader : require\n"
        "layout(local_size_x = 32) in;\n"
        "layout(max_vertices = 128) out;\n"
        "layout(max_primitives = 128) out;\n"
        "layout(triangles) out;\n"
        "\n");
    if (0 != m_NumLights)
    {
        m_MeshMainProg +=
        HS_("layout (constant_id = 0) const int NUM_LIGHTS = 16;\n"
            "struct Light\n"
            "{\n"
            "    vec4 position;\n"
            "    vec3 color;\n"
            "    float radius;\n"
            "};\n"
            "layout (binding = " TO_STRING(DS_BINDING_LIGHT_UBO) ") uniform LBO\n"
            "{\n"
            "   Light lights[NUM_LIGHTS];\n"
            "   vec4 lightViewPos;\n"
            "};\n");
    }
    m_MeshMainProg +=
    HS_("layout (push_constant) uniform PushConstant\n"
        "{\n"
        "    uint textIndex;\n"
        "    int  FarPlane;\n"
        "};\n"
        "\n"
        "layout (binding = " TO_STRING(DS_BINDING_MVP_UBO) ") uniform MBO\n"
        "{\n"
        "    mat4 mvp;\n"
        "};\n"
        "\n"
        "struct CpuVertex\n"
        "{\n"
        "    vec4 position;\n"
        "    vec4 norm;\n"
        "    vec4 color;\n"
        "    vec2 texcoord;\n"
        "    vec2 padding;\n"
        "};\n"
        "layout(binding = " TO_STRING(DS_BINDING_MESHLET_VB) ") buffer VBO\n"
        "{\n"
        "    CpuVertex cpuVertices[];\n"
        "};\n"
        "\n"
        "layout (location = 0) out IO\n"
        "{\n"
        "    vec3 normal;\n");
    m_MeshMainProg += m_SendColor ? HS_("    vec4 color;\n") : "";
    m_MeshMainProg +=
    HS_("    vec2 texCoord;\n"
        "    vec4 fragPos;\n");
    if (m_NumLights  != 0)
    {
        m_MeshMainProg += HS_("    vec3 lightVec[NUM_LIGHTS];\n");
    }
    m_MeshMainProg +=
    HS_("} opv[];\n"
        "layout (location = 21) perprimitiveLW out PerPrimitive\n"
        "{\n"
        "    flat vec4 outColor;\n"
        "} opp[];\n"
        "\n"
        "vec4 interpolate4(vec4 left, vec4 right, vec4 up, vec4 down, vec2 posCoord)\n"
        "{\n"
        "    vec4 lr = mix(left, right, posCoord.x);\n"
        "    vec4 ud = mix(up, down, posCoord.y);\n"
        "    return mix(lr, ud, 0.5);\n"
        "}\n"
        "vec3 interpolate3(vec3 left, vec3 right, vec3 up, vec3 down, vec2 posCoord)\n"
        "{\n"
        "    vec3 lr = mix(left, right, posCoord.x);\n"
        "    vec3 ud = mix(up, down, posCoord.y);\n"
        "    return mix(lr, ud, 0.5);\n"
        "}\n"
        "vec2 interpolate2(vec2 left, vec2 right, vec2 up, vec2 down, vec2 posCoord)\n"
        "{\n"
        "    vec2 lr = mix(left, right, posCoord.x);\n"
        "    vec2 ud = mix(up, down, posCoord.y);\n"
        "    return mix(lr, ud, 0.5);\n"
        "}\n"
        "\n"
        "void main()\n"
        "{\n"
        "    const uint taskIdx = gl_WorkGroupID.x;\n"
        "    const uint threadIdx = gl_LocalIlwocationID.x;\n"
        "    const uint numCpuCols = ") + Utility::StrPrintf("%u;\n", m_Cols);
    m_MeshMainProg +=
    HS_("    const uint numCpuRows = ") + Utility::StrPrintf("%u;\n", m_Rows);
    m_MeshMainProg +=
    HS_("    const uint numMeshCols = ") + Utility::StrPrintf("%u;\n", MESH_INTERNAL_COLS);
    m_MeshMainProg +=
    HS_("    const uint numMeshRows = ") + Utility::StrPrintf("%u;\n", MESH_INTERNAL_ROWS);
    m_MeshMainProg +=
    HS_("        const uint numMeshesH = numCpuCols/numMeshCols;\n"
        "        const uint numMeshesV = numCpuRows/numMeshRows;\n"
        "        const uint numMeshesVertH = numMeshCols + 1;\n"
        "        const uint numMeshesVertV = numMeshRows + 1;\n"
        "        const uint numFullCpuVertPerRow = numCpuCols + 1;\n"
        "        const uint numReducedCpuVertPerRow = numMeshesH + 1;\n"
        "        const uint numFullAndAllReducedCpuVert = numFullCpuVertPerRow + (numMeshRows - 1)*numReducedCpuVertPerRow;\n"
        "        const uint numGeneratedVertices = (numMeshCols - 1) * (numMeshRows - 1);\n"
        "        const uint numCpuVertPerPlane = numFullCpuVertPerRow * (numCpuRows + 1) -\n"
        "            numMeshesH * numMeshesV * numGeneratedVertices;\n"
        "        uint baseCpuVertIdx = numFullAndAllReducedCpuVert*(taskIdx/numMeshesH) +\n"
        "            ((FarPlane != 0) ? numCpuVertPerPlane : 0);\n"
        "        uint baseFullCpuVertIdx = baseCpuVertIdx + numMeshCols*(taskIdx%numMeshesH);\n"
        "        uint baseReducedCpuVertIdx = baseCpuVertIdx + numFullCpuVertPerRow + (taskIdx%numMeshesH);\n"
        "        uint destVertIdx = 0;\n"
        "        uint srcCpuVertIdx = -1;\n"
        "        if (threadIdx < numMeshesVertH)\n"
        "        {\n"
        "            srcCpuVertIdx = baseFullCpuVertIdx + threadIdx;\n"
        "            destVertIdx = threadIdx;\n"
        "        } else if (threadIdx < 2*numMeshesVertH) {\n"
        "            srcCpuVertIdx = baseFullCpuVertIdx + numFullAndAllReducedCpuVert + threadIdx - numMeshesVertH;\n"
        "            destVertIdx = numMeshRows*numMeshesVertH + threadIdx - numMeshesVertH;\n"
        "        } else if (threadIdx < (2*numMeshesVertH+numMeshRows-1)) {\n"
        "            srcCpuVertIdx = baseReducedCpuVertIdx + numReducedCpuVertPerRow * (threadIdx - 2*numMeshesVertH);\n"
        "            destVertIdx = numMeshesVertH + numMeshesVertH*(threadIdx - 2*numMeshesVertH);\n"
        "        } else if (threadIdx < (2*numMeshesVertH + 2*(numMeshRows - 1))) {\n"
        "            srcCpuVertIdx = baseReducedCpuVertIdx + 1 + numReducedCpuVertPerRow * (threadIdx - (2*numMeshesVertH + numMeshRows-1));\n"
        "            destVertIdx = 2*numMeshesVertH - 1 + numMeshesVertH*(threadIdx - (2*numMeshesVertH + numMeshRows-1));\n"
        "        }\n"
        "        if (srcCpuVertIdx != -1)\n"
        "        {\n"
        "            gl_MeshVerticesLW[destVertIdx].gl_Position = cpuVertices[srcCpuVertIdx].position * mvp;\n"
        "            opv[destVertIdx].normal   = cpuVertices[srcCpuVertIdx].norm.xyz;\n");
    m_MeshMainProg += m_SendColor ?
    HS_("            opv[destVertIdx].color    = cpuVertices[srcCpuVertIdx].color;\n") : "";
    m_MeshMainProg +=
    HS_("            opv[destVertIdx].texCoord = cpuVertices[srcCpuVertIdx].texcoord;\n"
        "            opv[destVertIdx].fragPos  = gl_MeshVerticesLW[destVertIdx].gl_Position;\n");
#ifndef VULKAN_STANDALONE
    bool HasBug2541586 = GetBoundGpuSubdevice()->HasBug(2541586);
#else
    bool HasBug2541586 = true;
#endif
    if (HasBug2541586)
    {
        // When generating SPIRV code, we can't use a normal "for loop" to index into the light
        // arrays. The only thing that appears to work is to manually unroll the for loop.
        for (unsigned idx = 0; idx < m_NumLights; idx++)
        {
            m_MeshMainProg += Utility::StrPrintf(
            HS_("        opv[destVertIdx].lightVec[%d] = lights[%d].position.xyz - cpuVertices[srcCpuVertIdx].position.xyz;\n"),
                idx, idx);
        }
    }
    else
    {
        m_MeshMainProg += (0 != m_NumLights) ?
        HS_("        for (int lightIdx = 0; lightIdx < NUM_LIGHTS; lightIdx++)\n"
            "        {\n"
            "            opv[destVertIdx].lightVec[lightIdx] = lights[lightIdx].position.xyz - cpuVertices[srcCpuVertIdx].position.xyz;\n"
            "        }\n") : "";
    }
    m_MeshMainProg +=
    HS_("    }\n"
        "\n"
        "    memoryBarrierShared();\n"
        "\n"
        "    for (uint generatedVertexIndex = threadIdx; generatedVertexIndex < numGeneratedVertices; generatedVertexIndex += gl_WorkGroupSize.x)\n"
        "    {\n"
        "        const uint leftVertIndex = numMeshesVertH*(1 + generatedVertexIndex/(numMeshCols-1));\n"
        "        const uint rightVertIndex = leftVertIndex + numMeshCols;\n"
        "        const uint upVertIndex = 1 + generatedVertexIndex%(numMeshCols-1);\n"
        "        const uint dowlwertIndex = upVertIndex + numMeshRows*numMeshesVertH;\n"
        "        const uint newVertIndex = leftVertIndex + 1 + generatedVertexIndex%(numMeshCols-1);\n"
        "        const vec2 newVertInterpolationPos = vec2(upVertIndex/(1.0*numMeshCols), (1.0 + generatedVertexIndex/(numMeshCols-1))/(1.0*numMeshRows));\n"
        "        gl_MeshVerticesLW[newVertIndex].gl_Position = interpolate4(\n"
        "            gl_MeshVerticesLW[leftVertIndex].gl_Position,\n"
        "            gl_MeshVerticesLW[rightVertIndex].gl_Position,\n"
        "            gl_MeshVerticesLW[upVertIndex].gl_Position,\n"
        "            gl_MeshVerticesLW[dowlwertIndex].gl_Position,\n"
        "            newVertInterpolationPos);\n"
        "        opv[newVertIndex].normal = interpolate3(\n"
        "            opv[leftVertIndex].normal,\n"
        "            opv[rightVertIndex].normal,\n"
        "            opv[upVertIndex].normal,\n"
        "            opv[dowlwertIndex].normal,\n"
        "            newVertInterpolationPos);\n"
        "        opv[newVertIndex].normal.z = 1;\n"
        "        opv[newVertIndex].normal = normalize(opv[newVertIndex].normal);\n");
    if (m_SendColor)
    {
        m_MeshMainProg +=
        HS_("        opv[newVertIndex].color = interpolate4(\n"
            "            opv[leftVertIndex].color,\n"
            "            opv[rightVertIndex].color,\n"
            "            opv[upVertIndex].color,\n"
            "            opv[dowlwertIndex].color,\n"
            "            newVertInterpolationPos);\n");
    }
    m_MeshMainProg +=
    HS_("        opv[newVertIndex].texCoord = interpolate2(\n"
        "            opv[leftVertIndex].texCoord,\n"
        "            opv[rightVertIndex].texCoord,\n"
        "            opv[upVertIndex].texCoord,\n"
        "            opv[dowlwertIndex].texCoord,\n"
        "            newVertInterpolationPos);\n"
        "        opv[newVertIndex].fragPos = gl_MeshVerticesLW[newVertIndex].gl_Position;\n");
    if (HasBug2541586)
    {
        for (unsigned idx = 0; idx < m_NumLights; idx++)
        {
            m_MeshMainProg += Utility::StrPrintf(
            HS_("        opv[newVertIndex].lightVec[%d] = interpolate3(\n"
                "            opv[leftVertIndex].lightVec[%d],\n"
                "            opv[rightVertIndex].lightVec[%d],\n"
                "            opv[upVertIndex].lightVec[%d],\n"
                "            opv[dowlwertIndex].lightVec[%d],\n"
                "            newVertInterpolationPos);\n"),
                idx, idx, idx, idx, idx);
        }
    }
    else
    {
        m_MeshMainProg += (0 != m_NumLights) ?
        HS_("        for (int lightIdx = 0; lightIdx < NUM_LIGHTS; lightIdx++)\n"
            "        {\n"
            "            opv[newVertIndex].lightVec[lightIdx] = interpolate3(\n"
            "                opv[leftVertIndex].lightVec[lightIdx],\n"
            "                opv[rightVertIndex].lightVec[lightIdx],\n"
            "                opv[upVertIndex].lightVec[lightIdx],\n"
            "                opv[dowlwertIndex].lightVec[lightIdx],\n"
            "                newVertInterpolationPos);\n"
            "        }\n") : "";
    }
    m_MeshMainProg +=
    HS_("    };\n"
        "\n");

    m_MeshMainProg +=
    HS_("\n"
        "    for (uint generatedDoubleTri = threadIdx; generatedDoubleTri < numMeshCols * numMeshRows; generatedDoubleTri += gl_WorkGroupSize.x)\n"
        "    {\n"
        "        const uint triBaseIdx = numMeshesVertH * (generatedDoubleTri/numMeshCols) + (generatedDoubleTri % numMeshCols); "
        "        opp[2 * generatedDoubleTri + 0].outColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "        gl_PrimitiveIndicesLW[6*generatedDoubleTri + 0]  = triBaseIdx + 0;\n"
        "        gl_PrimitiveIndicesLW[6*generatedDoubleTri + 1]  = triBaseIdx + 1;\n"
        "        gl_PrimitiveIndicesLW[6*generatedDoubleTri + 2]  = triBaseIdx + 0 + numMeshesVertH;\n"
        "        opp[2 * generatedDoubleTri + 1].outColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
        "        gl_PrimitiveIndicesLW[6*generatedDoubleTri + 3]  = triBaseIdx + 1;\n"
        "        gl_PrimitiveIndicesLW[6*generatedDoubleTri + 4]  = triBaseIdx + 0 + numMeshesVertH;\n"
        "        gl_PrimitiveIndicesLW[6*generatedDoubleTri + 5]  = triBaseIdx + 1 + numMeshesVertH;\n"
        "    }\n"
        "    if (threadIdx >= 1) return;\n"
        "    gl_PrimitiveCountLW = 2 * numMeshCols * numMeshRows;\n"
        "}\n");

    return rc;
}

RC VkStress::SetupVertexShader()
{
    RC rc;
    m_VtxProg =
    HS_("//Vertex Shader\n"
        "#version 450 core\n"
        "#extension GL_ARB_separate_shader_objects : enable\n"
        "#extension GL_ARB_shading_language_420pack : enable\n"
        "#pragma optionLW (unroll all)\n"
        "layout (std140, binding = " TO_STRING(DS_BINDING_MVP_UBO) ") uniform bufferVals\n"
        "{\n"
        "    mat4 mvp;\n"
        "} myBufferVals;\n"
        "layout (location = 0) in vec4 pos;\n"
        "layout (location = 1) in vec3 normal;\n"
        "layout (location = 2) in vec2 texcoord;\n");
    if (m_SendColor)
    {
        m_VtxProg += HS_("layout (location = 3) in vec4 color;\n");
    }

    // Light UBO and outputs
    if (0 != m_NumLights)
    {
        m_VtxProg +=
       HS_("layout (constant_id = 0) const int NUM_LIGHTS = 16;\n"
           "struct Light\n"
           "{\n"
           "    vec4 position;\n"
           "    vec3 color;\n"
           "    float radius;\n"
           "};\n"
           "layout (std140, binding = " TO_STRING(DS_BINDING_LIGHT_UBO) ") uniform UBO\n"
           "{\n"
           "   Light lights[NUM_LIGHTS];\n"
           "   vec4 viewPos;\n"
           "} ubo;\n");
    }

    m_VtxProg +=
    HS_("layout (location = 0) out IO\n"
        "{\n"
        "    vec3 Normal;\n");
    m_VtxProg += m_SendColor ? HS_("    vec4 Color;\n") : "";
    m_VtxProg +=
    HS_("    vec2 TexCoord;\n"
        "    vec4 Position;\n");
    m_VtxProg += (m_NumLights) ?
    HS_("    vec3 LightVec[NUM_LIGHTS];\n"): "";
    m_VtxProg +=
    HS_("} vOut;\n");
    m_VtxProg +=
    HS_("out gl_PerVertex\n"
        "{\n"
        "    vec4 gl_Position;\n"
        "};\n");
    m_VtxProg +=
    HS_("void main()\n"
        "{\n"
        "   vOut.Normal = normal;\n"
        "   vOut.TexCoord = texcoord;\n"
        "   gl_Position = pos * myBufferVals.mvp;\n"
        "   vOut.Position = gl_Position;\n");
    if (m_SendColor)
    {
        m_VtxProg +=
    HS_("   vOut.Color = color;\n");
    }
    m_VtxProg += (0 != m_NumLights) ?
    HS_("   for (int lightIdx = 0; lightIdx < NUM_LIGHTS; lightIdx++)\n"
        "   {\n"
        "      vOut.LightVec[lightIdx] = ubo.lights[lightIdx].position.xyz - pos.xyz;\n"
        "   }\n") : "";
    m_VtxProg += "}\n";

    return rc;
}

RC VkStress::SetupFragmentShader()
{
    RC rc;

    const bool gpcTileMaskEnabled =
        (m_PulseMode == TILE_MASK_GPC) ||
        (m_PulseMode == TILE_MASK_GPC_SHUFFLE);
    const bool tileMaskEnabled =
        (m_PulseMode == TILE_MASK) ||
        (m_PulseMode == TILE_MASK_CONSTANT_DUTY) ||
        (m_PulseMode == TILE_MASK_USER) ||
        gpcTileMaskEnabled;

    m_FragProg = HS_("//Fragment Shader\n");
    if (m_UseMeshlets && m_DrawLines)
    {
        // This is not really drawing lines (just piggybacking on the control
        // variable), but solid filled triangles that have clearly visible
        // border lines.
        // The alternating colors for the triangles are selected in the per
        // primitive "opp" array in the meshlet task.

        m_FragProg +=
        HS_("#version 450\n"
            "layout (location = 21) in PerPrimitive\n"
            "{\n"
            "    flat vec4 primColor;\n"
            "};\n"
            "layout (location = 0) out vec4 outColor;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    outColor = primColor;\n"
            "}\n");

        return rc;
    }

    m_FragProg +=
    HS_("#version 450 core\n"
        "#extension GL_LW_shader_sm_builtins : enable\n"
        "#extension GL_ARB_separate_shader_objects : enable\n"
        "#extension GL_ARB_shading_language_420pack : enable\n"
        "#pragma optionLW (unroll all)\n");
    m_FragProg += tileMaskEnabled ?
    HS_("#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n") : "";
    m_FragProg += (0 != m_NumLights) ?
    HS_("layout (constant_id = 0) const int NUM_LIGHTS = 16;\n"
        "struct Light\n"
        "{\n"
        "    vec4 position;\n"
        "    vec3 color;\n"
        "    float radius;\n"
        "};\n"
        "layout (std140, binding = " TO_STRING(DS_BINDING_LIGHT_UBO) ") uniform UBO\n"
        "{\n"
        "   Light lights[NUM_LIGHTS];\n"
        "   vec4 viewPos;\n"
        "} ubo;\n") : "";
    m_FragProg +=
    HS_("layout (constant_id = 3) const int NUM_SAMPLERS = 2;\n"
        "layout (constant_id = 4) const int NUM_TEXTURES = 32;\n"
        "layout (constant_id = 6) const int NUM_TEX_READS = 1;\n"
        "layout (constant_id = 7) const int SURFACE_WIDTH = 640;\n"
        "layout (binding = " TO_STRING(DS_BINDING_SAMPLERS) ") uniform sampler samplers[NUM_SAMPLERS];\n"
        "layout (binding = " TO_STRING(DS_BINDING_TEXTURES) ") uniform texture2D textures[NUM_TEXTURES];\n");
    m_FragProg += gpcTileMaskEnabled ?
    HS_("layout (binding = " TO_STRING(DS_BINDING_SM2GPC) ") buffer sm2gpc\n"
        "{\n"
        "    uint gpcId[];\n"
        "};\n") : "";
    m_FragProg +=
    HS_("layout (push_constant) uniform PushConstants\n"
        "{\n"
        "   uint texIndex;\n");
    m_FragProg += tileMaskEnabled ?
    HS_("   int64_t tileMask; // mask of tiles to enable\n") : "";
    m_FragProg +=
    HS_("};\n");

    // Input attributes
    m_FragProg +=
    HS_("layout (location = 0) in IO\n"
        "{\n"
        "    vec3 Normal;\n");
    m_FragProg += m_SendColor ? HS_("    vec4 Color;\n") : "";
    m_FragProg +=
    HS_("    vec2 TexCoord;\n"
        "    vec4 Position;\n");
    m_FragProg += (m_NumLights) ?
    HS_("    vec3 LightVec[NUM_LIGHTS];\n"): "";
    m_FragProg +=
    HS_("} fragIn;\n");

    // Output attributes
    m_FragProg +=
    HS_("layout (location = 0) out vec4 outColor;\n");
    // Program constants
    if (m_ApplyFog)
    {
        m_FragProg +=
        HS_("const vec4 fogColor = vec4(0.6, 0.6, 0.6, 1.0);\n"
            "const float fogDensity = 2.0;\n");
    }

    // Functions
    // Fog. Return a value from 0.0 (no fog) to 1.0 (full fog)
    // Clamping it to 0.9 instead of 1.0 to avoid losing texture detail,
    // which could mask errors.
    m_FragProg += (m_ApplyFog) ?
    HS_("float linearFog()\n"
        "{\n"
        "    float minZ = -fragIn.Position.w;\n"
        "    float maxZ = 0;\n"
        "    float dist = (fragIn.Position.z-minZ)/(maxZ-minZ);\n"
        "    float factor = dist / fogDensity;\n"
        "    return clamp(factor, 0.0, 0.9);\n"
        "}\n"
        "float exponentialFog()\n"
        "{\n"
        "    float minZ = -fragIn.Position.w;\n"
        "    float maxZ = 0;\n"
        "    float dist = (fragIn.Position.z-minZ)/(maxZ-minZ);\n"
        "    float d = fogDensity * dist * dist;\n" // sq of dist for stronger slope
        "    return clamp((1.0 - exp2(-d)), 0.0, 0.9);\n" // ilwerse exponential capped at 0.9
        "}\n") : "";

    // Kill the fragment if it falls within a tile that has been disabled
    m_FragProg += gpcTileMaskEnabled ?
    HS_("bool killFrag()\n"
        "{\n"
        "    uint gpcId = gpcId[gl_SMIDLW];\n"
        "    return (( (1<<gpcId) & tileMask) == 0);\n"
        "}\n"): "";
    m_FragProg += (tileMaskEnabled && !gpcTileMaskEnabled) ?
    HS_("bool killFrag()\n"
        "{\n"
        "    const int tileWidth = 16;\n"
        "    const int tileHeight = 16;\n"
        "    const int tilesPerRow = SURFACE_WIDTH / tileWidth;\n"
        "    int tileX = int(gl_FragCoord.x) / tileWidth;\n"
        "    int tileY = int(gl_FragCoord.y) / tileHeight;\n"
        "    int tileIndex = tileY * tilesPerRow + tileX;\n"
        "    int tileIndexMod64 = tileIndex % 64;\n"
        "    return ((1UL << tileIndexMod64) & tileMask) == 0;\n"
        "}\n"): "";

    // Shader's entry point
    m_FragProg +=
    HS_("void main()\n"
        "{\n");

    m_FragProg += tileMaskEnabled ?
    HS_("    if (killFrag())\n"
        "        discard;\n") : "";

    if (m_SendColor)
    {
        m_FragProg += HS_("    outColor = fragIn.Color;\n");
    }
    else
    {
        m_FragProg += HS_("    outColor = vec4(0.0, 0.0, 0.0, 1.0);\n");
    }

    m_FragProg +=
    HS_("    uint texIdx;\n"
        "    vec4 texColor;\n"
        "    for (uint texToRead = 0; texToRead < NUM_TEX_READS; texToRead++)\n"
        "    {\n"
        "        texIdx = (texIndex + texToRead) % NUM_TEXTURES;\n"
        "        texColor = texture(sampler2D(textures[texIdx], samplers[texToRead]), \n"
        "                           fragIn.TexCoord, 0.0);\n"
        "        outColor = mix(outColor, texColor, texColor.w);\n"
        "    }\n");


    m_FragProg += (0 != m_NumLights) ?
    HS_("   vec3 N = normalize(fragIn.Normal);\n"
        "   vec3 diffuseColor = vec3(0.0);\n"
        "   vec3 specColor = vec3(0.0);\n"
        "   for (int lightIdx = 0; lightIdx < NUM_LIGHTS; lightIdx++)\n"
        "   {\n"

        // Diffuse lighting component
        "       vec3 lightNormalized = normalize(fragIn.LightVec[lightIdx]);\n"
        "       float lightDist = length(fragIn.LightVec[lightIdx]);\n"
        "       float lambertian = max(dot(lightNormalized, N), 0.0);\n"
        "       float attenuation = ubo.lights[lightIdx].radius / (pow(lightDist, 2.0) + 1.0);\n"
        "       attenuation = clamp(attenuation, 0.0, 1.0);\n"
        "       float attenLambertian = attenuation * lambertian;\n"
        "       diffuseColor += attenLambertian * ubo.lights[lightIdx].color;\n"

        // Spelwlar lighting component
        // https://en.wikipedia.org/wiki/Blinn%E2%80%93Phong_shading_model#Fragment_shader
        "       vec3 viewDir = normalize(-fragIn.Position.xyz);\n"
        "       vec3 halfDir = normalize(lightNormalized + viewDir);\n"
        "       float specAngle = max(dot(halfDir, N), 0.0);\n"
        "       float specIntensity = attenuation * pow(specAngle, 8.0);\n"
        "       specColor += specIntensity * ubo.lights[lightIdx].color;\n"
        "   }\n"
        "   outColor = mix(outColor, vec4(diffuseColor, 1.0), 0.15);\n"
        "   outColor = mix(outColor, vec4(specColor, 1.0), 0.1);\n") : "";

    if (m_ApplyFog)
    {
        if (m_ExponentialFog)
        {
            m_FragProg +=
                HS_("   outColor = mix(outColor, fogColor, exponentialFog());\n");
        }
        else
        {
            m_FragProg +=
                HS_("   outColor = mix(outColor, fogColor, linearFog());\n");
        }
    }

    m_FragProg += "}\n";

    return rc;
}

RC VkStress::SetupTessellationControlShader()
{
    RC rc;
    m_TessCtrlProg =
    HS_("//Tessellation Control Shader\n"
        "#version 450 core\n"
        "#pragma optionLW (unroll all)\n"
        "layout(vertices=4) out;\n");

    m_TessCtrlProg += m_NumLights ?
    HS_("layout (constant_id = 0) const int NUM_LIGHTS = 16;\n") : "";

    m_TessCtrlProg +=
    HS_("layout (constant_id = 1) const float TESS_LEVEL_H = 8.0;\n"
        "layout (constant_id = 2) const float TESS_LEVEL_V = 8.0;\n"
        "layout (constant_id = 5) const float TEXEL_DENSITY = 0.0;\n"
        "layout (location = 0) in IO\n"
        "{\n"
        "    vec3 Normal;\n");
    m_TessCtrlProg += m_SendColor ? HS_("    vec4 Color;\n") : "";
    m_TessCtrlProg +=
    HS_("    vec2 TexCoord;\n"
        "    vec4 FragPos;\n");
    m_TessCtrlProg += m_NumLights ?
    HS_("    vec3 LightVec[NUM_LIGHTS];\n"): "";
    m_TessCtrlProg +=
    HS_("} vIn[];\n"
        "layout (location = 0) out IO\n"
        "{\n"
        "    vec3 Normal;\n");
    m_TessCtrlProg += m_SendColor ? HS_("    vec4 Color;\n") : "";
    m_TessCtrlProg +=
    HS_("    vec2 TexCoord;\n"
        "    vec4 FragPos;\n");
    m_TessCtrlProg += m_NumLights ?
    HS_("    vec3 LightVec[NUM_LIGHTS];\n"): "";
    m_TessCtrlProg +=
    HS_("} vOut[];\n");

    //Note: We have to match the gl_PerVertex built-in block from the upstream shader or we get a
    //      validation error.
    m_TessCtrlProg +=
    HS_("in gl_PerVertex\n"
        "{\n"
        "    vec4 gl_Position;\n"
        "} gl_in[gl_MaxPatchVertices];\n");

    m_TessCtrlProg +=
    HS_("void main()\n"
        "{\n"
        "  if (gl_IlwocationID == 0)\n"
        "  {\n"
        "    gl_TessLevelOuter[0] = TESS_LEVEL_H;\n"
        "    gl_TessLevelOuter[1] = TESS_LEVEL_V;\n"
        "    gl_TessLevelOuter[2] = TESS_LEVEL_H;\n"
        "    gl_TessLevelOuter[3] = TESS_LEVEL_V;\n"
        "    gl_TessLevelInner[0] = TESS_LEVEL_V;\n"
        "    gl_TessLevelInner[1] = TESS_LEVEL_H;\n"
        "  }\n"
        "  vOut[gl_IlwocationID].Normal = vIn[gl_IlwocationID].Normal;\n");
    m_TessCtrlProg += m_SendColor ?
    HS_("  vOut[gl_IlwocationID].Color = vIn[gl_IlwocationID].Color;\n") : "";
    m_TessCtrlProg +=
    HS_("  vOut[gl_IlwocationID].TexCoord = TEXEL_DENSITY * vec2(vIn[gl_IlwocationID].FragPos);\n"
        "  vOut[gl_IlwocationID].FragPos = vIn[gl_IlwocationID].FragPos;\n"
        "  gl_out[gl_IlwocationID].gl_Position = gl_in[gl_IlwocationID].gl_Position;\n");

    m_TessCtrlProg += m_NumLights ?
    HS_("  for (int lightIdx = 0; lightIdx < NUM_LIGHTS; lightIdx++)\n"
        "  {\n"
        "    vOut[gl_IlwocationID].LightVec[lightIdx] = vIn[gl_IlwocationID].LightVec[lightIdx];\n"
        "  }\n") : "";
    m_TessCtrlProg +=
        "}\n";

    return rc;
}

RC VkStress::SetupTessellationEvaluationShader()
{
    RC rc;
    m_TessEvalProg =
    HS_("//Tessellation Eval Shader\n"
        "#version 450 core\n"
        "#pragma optionLW (unroll all)\n");

    if (m_DrawTessellationPoints)
    {
        m_TessEvalProg +=
    HS_("layout(quads, point_mode) in;\n");
    }
    else
    {
        m_TessEvalProg +=
    HS_("layout(quads) in;\n");
    }

    m_TessEvalProg += m_NumLights ?
    HS_("layout (constant_id = 0) const int NUM_LIGHTS = 16;\n") : "";
    m_TessEvalProg +=
    HS_("layout (push_constant) uniform PushConstantBlock\n"
        "{\n"
        "    uint  textIndex;\n"
        "    float ZPlaneMultiplier;\n"
        "};\n"
        "layout (location = 0) in IO\n"
        "{\n"
        "    vec3 Normal;\n");
    m_TessEvalProg += m_SendColor ? HS_("    vec4 Color;\n") : "";
    m_TessEvalProg +=
    HS_("    vec2 TexCoord;\n"
        "    vec4 FragPos;\n");
    m_TessEvalProg += m_NumLights ?
    HS_("    vec3 LightVec[NUM_LIGHTS];\n") : "";
    m_TessEvalProg +=
    HS_("} vIn[];\n");
    m_TessEvalProg +=
    HS_("layout (location = 0) out IO\n"
        "{\n"
        "    vec3 Normal;\n");
    m_TessEvalProg += m_SendColor ? HS_("    vec4 Color;\n") : "";
    m_TessEvalProg +=
    HS_("    vec2 TexCoord;\n"
        "    vec4 FragPos;\n");
    m_TessEvalProg += m_NumLights ?
    HS_("    vec3 LightVec[NUM_LIGHTS];\n") : "";
    m_TessEvalProg +=
    HS_("} vOut;\n");
    m_TessEvalProg +=
    HS_("\n"
        "vec4 interpolate4(vec4 a, vec4 b, vec4 c, vec4 d, vec3 tessCoord)\n"
        "{\n"
        "    vec4 r0 = mix(a, b, tessCoord.x);\n"
        "    vec4 r1 = mix(c, d, tessCoord.x);\n"
        "    return mix(r0, r1, tessCoord.y);\n"
        "}\n"
        "\n"
        "vec3 interpolate3(vec3 a, vec3 b, vec3 c, vec3 d, vec3 tessCoord)\n"
        "{\n"
        "    vec3 r0 = mix(a, b, tessCoord.x);\n"
        "    vec3 r1 = mix(c, d, tessCoord.x);\n"
        "    return mix(r0, r1, tessCoord.y);\n"
        "}\n"
        "\n"
        "vec2 interpolate2(vec2 a, vec2 b, vec2 c, vec2 d, vec3 tessCoord)\n"
        "{\n"
        "    vec2 r0 = mix(a, b, tessCoord.x);\n"
        "    vec2 r1 = mix(c, d, tessCoord.x);\n"
        "    return mix(r0, r1, tessCoord.y);\n"
        "}\n"
        "\n"
        "void main()\n"
        "{\n"
        "    gl_Position = interpolate4(gl_in[0].gl_Position, gl_in[1].gl_Position, \n"
        "                               gl_in[2].gl_Position, gl_in[3].gl_Position, \n"
        "                               gl_TessCoord);\n"
        "    float component = 100 - max(abs(gl_Position.x), abs(gl_Position.y));\n"
        "    gl_Position.z += (0.0008 * ZPlaneMultiplier * component);\n"
        "    gl_Position.w += 0.5 * component;\n");
    if (m_SendColor)
    {
        m_TessEvalProg +=
        HS_("    vOut.Color = interpolate4(vIn[0].Color, vIn[1].Color, \n"
            "                            vIn[2].Color, vIn[3].Color, \n"
            "                            gl_TessCoord);\n");
    }
    m_TessEvalProg +=
    HS_("    vOut.Normal = interpolate3(vIn[0].Normal, vIn[1].Normal, \n"
        "                             vIn[2].Normal, vIn[3].Normal, \n"
        "                             gl_TessCoord);\n"
        "    vOut.Normal.x += abs(0.01*gl_Position.x);\n"
        "    vOut.Normal.y += abs(0.01*gl_Position.y);\n"
        "    vOut.Normal.z = 1;\n"
        "    vOut.Normal = normalize(vOut.Normal);\n"
        "    vOut.TexCoord = interpolate2(vIn[0].TexCoord, vIn[1].TexCoord, \n"
        "                              vIn[2].TexCoord, vIn[3].TexCoord, \n"
        "                              gl_TessCoord);\n"
        "    vOut.FragPos = interpolate4(vIn[0].FragPos, vIn[1].FragPos, \n"
        "                                vIn[2].FragPos, vIn[3].FragPos, \n"
        "                                gl_TessCoord);\n");
    m_TessEvalProg += m_NumLights ?
    HS_("    for (int lightIdx = 0; lightIdx < NUM_LIGHTS; lightIdx++)\n"
        "    {\n"
        "      vOut.LightVec[lightIdx] = interpolate3(vIn[0].LightVec[lightIdx], \n"
        "                                             vIn[1].LightVec[lightIdx], \n"
        "                                             vIn[2].LightVec[lightIdx], \n"
        "                                             vIn[3].LightVec[lightIdx], \n"
        "                                             gl_TessCoord);\n"
        "    }\n") : "";
    m_TessEvalProg +=
        "}\n";

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::SetupFrameBuffer()
{
    RC rc;

    m_FrameBuffers.reserve(m_SwapChain->GetNumImages());
    for (UINT32 i = 0; i < m_SwapChain->GetNumImages(); i++)
    {
        m_FrameBuffers.emplace_back(make_unique<VulkanFrameBuffer>(m_pVulkanDev));
    }

    for (UINT32 i = 0; i < m_FrameBuffers.size(); i++)
    {
        vector<VkImageView> attachments;
        //push color image
        MASSERT(m_RenderPass->GetAttachmentType(0) == AttachmentType::COLOR);
        attachments.push_back(m_SwapChain->GetImageView(i));

        //push depth image
        MASSERT(m_RenderPass->GetAttachmentType(1) == AttachmentType::DEPTH);
        attachments.push_back(m_DepthImages[i]->GetImageView());

        if (m_SampleCount > 1)
        {
            MASSERT(m_RenderPass->GetAttachmentType(2) == AttachmentType::COLOR);
            attachments.push_back(m_MsaaImages[i]->GetImageView());
        }

        MASSERT(m_SwapChain->GetSwapChainExtent().width == m_DepthImages[i]->GetWidth());
        m_FrameBuffers[i]->SetWidth(m_SwapChain->GetSwapChainExtent().width);

        MASSERT(m_SwapChain->GetSwapChainExtent().height == m_DepthImages[i]->GetHeight());
        m_FrameBuffers[i]->SetHeight(m_SwapChain->GetSwapChainExtent().height);

        CHECK_VK_TO_RC(m_FrameBuffers[i]->CreateFrameBuffer(attachments,
            m_RenderPass->GetRenderPass()));
    }
    return rc;
}

void VkStress::SetDepthStencilState(VkCommandBuffer cmdBuf, UINT32 frameMod4)
{
    switch (frameMod4)
    {
        // Initial depth is 0.0 for first loop, near for subsequent loops.
        // Initial stencil is 0 for all loops.
        // Draw at near view plane, touches all pixels.
        // Replace Z with near, stencil with 1.
        case 0:
            if (m_Ztest)
            {
                m_pVulkanDev->CmdSetDepthCompareOpEXT(cmdBuf,
                                                      VK_COMPARE_OP_GREATER_OR_EQUAL);
            }
            if (m_Stencil)
            {
                m_pVulkanDev->CmdSetStencilOpEXT(cmdBuf,
                                                 VK_STENCIL_FACE_FRONT_AND_BACK,    // faceMask
                                                 VK_STENCIL_OP_KEEP,                // failOp
                                                 VK_STENCIL_OP_INCREMENT_AND_CLAMP, // passOp
                                                 VK_STENCIL_OP_KEEP,                // depthFailOp
                                                 m_UseMeshlets                      // compareOp
                                                    ? VK_COMPARE_OP_LESS_OR_EQUAL
                                                    : VK_COMPARE_OP_EQUAL);
                m_pVulkanDev->CmdSetStencilReference(cmdBuf,
                                                     VK_STENCIL_FACE_FRONT_AND_BACK,// faceMask
                                                     0);                            // reference
                if (!m_UseMeshlets)
                {
                    m_pVulkanDev->CmdSetStencilCompareMask(cmdBuf,
                                                           VK_STENCIL_FACE_FRONT_AND_BACK,// faceMask
                                                           0xFFU);                        // compareMask
                }
            }
            break;

        // Second draw:
        // Initial Z is near.
        // Initial S is 1.
        // Draw far plane, not all pixels are touched.
        // Replace some Z with far, some stencil with 2.
        case 1:
            if (m_Ztest)
            {
                m_pVulkanDev->CmdSetDepthCompareOpEXT(cmdBuf,
                                                      m_UseMeshlets
                                                        ? VK_COMPARE_OP_ALWAYS
                                                        : VK_COMPARE_OP_GREATER_OR_EQUAL);
            }
            if (m_Stencil)
            {
                m_pVulkanDev->CmdSetStencilOpEXT(cmdBuf,
                                                 VK_STENCIL_FACE_FRONT_AND_BACK,    // faceMask
                                                 VK_STENCIL_OP_KEEP,                // failOp
                                                 VK_STENCIL_OP_INCREMENT_AND_CLAMP, // passOp
                                                 VK_STENCIL_OP_KEEP,                // depthFailOp
                                                 m_UseMeshlets                      // compareOp
                                                    ? VK_COMPARE_OP_LESS_OR_EQUAL
                                                    : VK_COMPARE_OP_EQUAL);
                m_pVulkanDev->CmdSetStencilReference(cmdBuf,
                                                     VK_STENCIL_FACE_FRONT_AND_BACK,// faceMask
                                                     1);                            // reference
                if (!m_UseMeshlets)
                {
                    m_pVulkanDev->CmdSetStencilCompareMask(cmdBuf,
                                                           VK_STENCIL_FACE_FRONT_AND_BACK,// faceMask
                                                           0xFFU);                        // compareMask
                }
            }
            break;

        // Third draw:
        // Initial Z is mixed near/far.
        // Initial S is mixed 1/2.
        // Draw far plane, touch only same the pixels as 2nd draw.
        // Replace some Z with far, some stencil with 3.
        case 2:
            if (m_Ztest)
            {
                m_pVulkanDev->CmdSetDepthCompareOpEXT(cmdBuf,
                                                      m_UseMeshlets
                                                        ? VK_COMPARE_OP_ALWAYS
                                                        : VK_COMPARE_OP_EQUAL);
            }
            if (m_Stencil)
            {
                m_pVulkanDev->CmdSetStencilOpEXT(cmdBuf,
                                                 VK_STENCIL_FACE_FRONT_AND_BACK,    // faceMask
                                                 VK_STENCIL_OP_KEEP,                // failOp
                                                 m_UseMeshlets                      // passOp
                                                    ? VK_STENCIL_OP_DECREMENT_AND_WRAP
                                                    : VK_STENCIL_OP_INCREMENT_AND_CLAMP,
                                                 VK_STENCIL_OP_KEEP,                // depthFailOp
                                                 m_UseMeshlets                      // compareOp
                                                    ? VK_COMPARE_OP_LESS_OR_EQUAL
                                                    : VK_COMPARE_OP_EQUAL);
                m_pVulkanDev->CmdSetStencilReference(cmdBuf,
                                                     VK_STENCIL_FACE_FRONT_AND_BACK,// faceMask
                                                     2);                            // reference
                if (!m_UseMeshlets)
                {
                    m_pVulkanDev->CmdSetStencilCompareMask(cmdBuf,
                                                           VK_STENCIL_FACE_FRONT_AND_BACK,// faceMask
                                                           0xFFU);                        // compareMask
                }
            }
            break;

        // Fourth draw:
        // Initial Z is mixed near/far.
        // Initial S is mixed 1/3.
        // Draw near plane, touches all pixels.
        // Replace all Z with near, all stencil with 0.
        default:
            MASSERT(frameMod4 == 3);
            if (m_Ztest)
            {
                m_pVulkanDev->CmdSetDepthCompareOpEXT(cmdBuf,
                                                      VK_COMPARE_OP_LESS_OR_EQUAL);
            }
            if (m_Stencil)
            {
                m_pVulkanDev->CmdSetStencilOpEXT(cmdBuf,
                                                 VK_STENCIL_FACE_FRONT_AND_BACK,    // faceMask
                                                 VK_STENCIL_OP_INCREMENT_AND_CLAMP, // failOp
                                                 m_UseMeshlets                      // passOp
                                                    ? VK_STENCIL_OP_DECREMENT_AND_WRAP
                                                    : VK_STENCIL_OP_ZERO,
                                                 VK_STENCIL_OP_INCREMENT_AND_CLAMP, // depthFailOp
                                                 m_UseMeshlets                      // compareOp
                                                    ? VK_COMPARE_OP_LESS_OR_EQUAL
                                                    : VK_COMPARE_OP_EQUAL);
                m_pVulkanDev->CmdSetStencilReference(cmdBuf,
                                                     VK_STENCIL_FACE_FRONT_AND_BACK,// faceMask
                                                     1);                            // reference
                if (!m_UseMeshlets)
                {
                    m_pVulkanDev->CmdSetStencilCompareMask(cmdBuf,
                                                           VK_STENCIL_FACE_FRONT_AND_BACK,// faceMask
                                                           0xFDU);                        // compareMask
                }
            }
            break;
    }
}

//------------------------------------------------------------------------------------------------
// Vulkan requires that we create a new pipeline state for every combination of non-dynamic state.
// Since VkStress modifies both Depth & Stencil states we have to create 4 separate pipelines.
RC VkStress::CreateGraphicsPipelines()
{
    RC rc;

    m_GraphicsPipeline = VulkanPipeline(m_pVulkanDev);

    VkPrimitiveTopology prim = m_DrawLines ?
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP :
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (m_UseMeshlets)
    {
        // Meshlets use yellow-green triangles to draw the lines:
        prim = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
    else if (m_UseTessellation)
    {
        m_GraphicsPipeline.SetTessellationState(4);
        prim = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }

    m_GraphicsPipeline.SetInputAssemblyTopology(prim, VK_FALSE);

    // Color blending take presedence over color logic
    if (m_ColorBlend)
    {
        // Color blending only applies to fixed-point or floating-point
        // color attachments. If the attachment has an integer format then
        // blending is not applied.
        VkPipelineColorBlendAttachmentState cbas;
        cbas.colorWriteMask = 0xf;  //output all channels RGBA
        cbas.blendEnable = VK_TRUE;
        cbas.colorBlendOp = VK_BLEND_OP_ADD;
        cbas.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
        cbas.dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;

        cbas.alphaBlendOp = VK_BLEND_OP_ADD;
        cbas.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cbas.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        vector<VkPipelineColorBlendAttachmentState> colorBlend;
        colorBlend.push_back(cbas);
        float blendConstants[4] = { 1.0, 1.0, 1.0, 1.0 };

        m_GraphicsPipeline.SetColorBlendState(
        colorBlend
        ,VK_FALSE   //logicOpEnable
        ,VK_LOGIC_OP_NO_OP
        ,blendConstants);
    }
    else if (m_ColorLogic)
    {
        VkPipelineColorBlendAttachmentState cbas;
        cbas.colorWriteMask = 0xf;  //output all channels RGBA
        cbas.blendEnable = VK_FALSE;
        cbas.alphaBlendOp = VK_BLEND_OP_ADD;
        cbas.colorBlendOp = VK_BLEND_OP_ADD;
        cbas.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbas.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbas.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbas.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        vector<VkPipelineColorBlendAttachmentState> colorBlend;
        colorBlend.push_back(cbas);
        const VkLogicOp logicOp = VK_LOGIC_OP_XOR;
        float blendConstants[4] = { 1.0, 1.0, 1.0, 1.0 };

        m_GraphicsPipeline.SetColorBlendState(
        colorBlend
        ,VK_TRUE
        ,logicOp
        ,blendConstants);
    }

    // We can't use the "default" Depth & Stencil operations so override here
    VkPipelineDepthStencilStateCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.depthTestEnable = (m_Ztest) ? VK_TRUE : VK_FALSE;
    info.depthWriteEnable =(m_Ztest) ? VK_TRUE : VK_FALSE;
    info.depthBoundsTestEnable = VK_FALSE;
    info.minDepthBounds = 0;
    info.maxDepthBounds = 0;
    info.stencilTestEnable = (m_Stencil) ? VK_TRUE : VK_FALSE;
    info.back.writeMask = 0xff;

    // Reasonable defaults, overriden by dynamic state
    // aka glDepthFunc(GL_GEQUAL)
    info.depthCompareOp     = VK_COMPARE_OP_GREATER_OR_EQUAL;
    // aka glStencilFunc(GL_EQUAL, 0, 0xff)
    info.back.compareOp     = VK_COMPARE_OP_EQUAL;
    info.back.reference     = 0;
    info.back.compareMask   = 0xff;
    // aka glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    info.back.failOp        = VK_STENCIL_OP_KEEP;
    info.back.depthFailOp   = VK_STENCIL_OP_KEEP;
    info.back.passOp        = VK_STENCIL_OP_INCREMENT_AND_CLAMP;

    if (m_Ztest)
    {
        m_GraphicsPipeline.EnableDynamicState(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT);
    }
    if (m_Stencil)
    {
        m_GraphicsPipeline.EnableDynamicState(VK_DYNAMIC_STATE_STENCIL_OP_EXT);
        m_GraphicsPipeline.EnableDynamicState(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        if (!m_UseMeshlets)
        {
            m_GraphicsPipeline.EnableDynamicState(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
        }
    }

    info.front = info.back;
    m_GraphicsPipeline.SetDepthStencilState(&info);

    // Set viewport and scissor state
    m_Viewport.width = static_cast<float>(m_RenderWidth);
    m_Viewport.height = static_cast<float>(m_RenderHeight);
    m_Viewport.minDepth = 0.0f;
    m_Viewport.maxDepth = 1.0f;
    m_Viewport.x = 0;
    m_Viewport.y = 0;

    m_Scissor.extent.width = m_RenderWidth;
    m_Scissor.extent.height = m_RenderHeight;
    m_Scissor.offset.x = 0;
    m_Scissor.offset.y = 0;

    VkPipelineViewportStateCreateInfo vpInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO
        ,nullptr
        ,0
        ,1
        ,&m_Viewport
        ,1
        ,&m_Scissor
    };
    m_GraphicsPipeline.SetViewPortAndScissorState(&vpInfo);

    vector<VkPipelineShaderStageCreateInfo> shaderStages;
    if (m_UseMeshlets)
    {
        shaderStages.push_back(m_MeshletsTaskShader->GetShaderStageInfo());
        shaderStages.push_back(m_MeshletsMainShader->GetShaderStageInfo());

        m_GraphicsPipeline.AddPushConstantRange(
        {
            VK_SHADER_STAGE_MESH_BIT_LW,
            4, // offset
            sizeof(UINT32)
        });
    }
    else
    {
        shaderStages.push_back(m_VertexShader->GetShaderStageInfo());
    }

    if (m_UseTessellation)
    {
        shaderStages.push_back(m_TessellationControlShader->GetShaderStageInfo());
        shaderStages.push_back(m_TessellationEvaluationShader->GetShaderStageInfo());

        m_GraphicsPipeline.AddPushConstantRange(
        {
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            4, // offset
            sizeof(FLOAT32)
        });
    }
    shaderStages.push_back(m_FragmentShader->GetShaderStageInfo());

    // Pass the number of lights as a specialization entry to VS/FS
    // This gets used by the shaders as the NUM_LIGHTS constant
    VkSpecializationMapEntry specializationEntries[] =
    {
        {
            0,                                       // constantID
            offsetof(SpecializationData, numLights), // offset
            sizeof(SpecializationData::numLights)    // size
        },
        {
            1,                                       // constantID
            offsetof(SpecializationData, xTessRate), // offset
            sizeof(SpecializationData::xTessRate)    // size
        },
        {
            2,                                       // constantID
            offsetof(SpecializationData, yTessRate), // offset
            sizeof(SpecializationData::yTessRate)    // size
        },
        {
            3,
            offsetof(SpecializationData, numSamplers),
            sizeof(SpecializationData::numSamplers)
        },
        {
            4,
            offsetof(SpecializationData, numTextures),
            sizeof(SpecializationData::numTextures)
        },
        {
            5,
            offsetof(SpecializationData, texelDensity),
            sizeof(SpecializationData::texelDensity)
        },
        {
            6,
            offsetof(SpecializationData, texReadsPerDraw),
            sizeof(SpecializationData::texReadsPerDraw)
        },
        {
            7,
            offsetof(SpecializationData, surfWidth),
            sizeof(SpecializationData::surfWidth)
        }
    };

    m_SpecializationData.numLights = m_NumLights;
    m_SpecializationData.numSamplers = static_cast<UINT32>(m_Samplers.size());
    m_SpecializationData.numTextures = m_NumTextures;
    m_SpecializationData.texelDensity = powf(2.0f, m_TxD) / m_MaxTextureSize;
    m_SpecializationData.texReadsPerDraw = m_TexReadsPerDraw;
    m_SpecializationData.surfWidth = m_RenderWidth;

    VkSpecializationInfo specializationInfo = { };
    specializationInfo.mapEntryCount =
        sizeof(specializationEntries) / sizeof(specializationEntries[0]);
    specializationInfo.pMapEntries = specializationEntries;
    specializationInfo.dataSize = sizeof(m_SpecializationData);
    specializationInfo.pData = &m_SpecializationData;

    for (auto& shaderStage : shaderStages)
    {
        shaderStage.pSpecializationInfo = &specializationInfo;
    }

    m_GraphicsPipeline.AddPushConstantRange(
    {
        VK_SHADER_STAGE_FRAGMENT_BIT // stageFlags
       ,0                            // offset
       ,4                            // size (a UINT32 for the texture index)
    });

    if ( (m_PulseMode == TILE_MASK) ||
         (m_PulseMode == TILE_MASK_CONSTANT_DUTY) ||
         (m_PulseMode == TILE_MASK_USER) ||
         (m_PulseMode == TILE_MASK_GPC) ||
         (m_PulseMode == TILE_MASK_GPC_SHUFFLE) )
    {
        // Add a pushconstant range for tileMask
        m_GraphicsPipeline.AddPushConstantRange(
        {
            VK_SHADER_STAGE_FRAGMENT_BIT // stageFlags
           ,8                            // offset
           ,8                            // size
        });
    }

    CHECK_VK_TO_RC(m_GraphicsPipeline.SetupGraphicsPipeline(
        &m_VBBindingAttributeDesc
        ,m_DescriptorInfo->GetDescriptorSetLayout(0)
        ,shaderStages
        ,m_RenderPass->GetRenderPass()
        ,0 //subpass
        ,static_cast<VkSampleCountFlagBits>(m_SampleCount)));

    return rc;
}

//------------------------------------------------------------------------------------------------
void VkStress::CleanupAfterSetup() const
{
    m_MeshletsTaskShader->Cleanup();
    m_MeshletsMainShader->Cleanup();
    m_VertexShader->Cleanup();
    m_TessellationControlShader->Cleanup();
    m_TessellationEvaluationShader->Cleanup();
    m_FragmentShader->Cleanup();
    return;
}

//------------------------------------------------------------------------------------------------
RC VkStress::SetupSemaphoresAndFences()
{
    RC rc;

    VkSemaphoreCreateInfo semInfo;
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = nullptr;
    semInfo.flags = 0;

    m_RenderSem.resize(2);
    for (auto& sem : m_RenderSem)
    {
        CHECK_VK_TO_RC(m_pVulkanDev->CreateSemaphore(
            &semInfo
            ,nullptr
            ,&sem));
    }

    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;

    m_RenderFences.resize(2);
    for (auto& fence : m_RenderFences)
    {
        CHECK_VK_TO_RC(m_pVulkanDev->CreateFence(
            &fenceInfo
            ,nullptr
            ,&fence));
    }

    return rc;
}

//------------------------------------------------------------------------------------------------
// Create a pre-draw command buffer to initialize the contents of both the color and
// depth/stencil buffers. This way we can reuse the DrawCmdBuffer for rendering the quad-frames
// 100s of times.
RC VkStress::BuildPreDrawCmdBuffer()
{
    RC rc;

    //Note1: It's ok to have this vector on the stack because when we call vkBeginRenderPass the
    //      data is copied into an internal variable of __VkCommandBuffer object in vkcore.cpp.
    //Note2: If you use the color.uint32 or color.int32 fields the data must still represent the
    //       float value. So for example a full red value of 1.0 would be 0x3f800000 and not
    //       0xffffffff.
    vector<VkClearValue> clearValues(m_SampleCount > 1 ? 3 : 2);

    VK_DEBUG_MARKER(markerPreDraw, 0.5f, 0.0f, 0.5f)

    for (UINT32 i = 0; i < m_PreDrawCmdBuffers.size(); i++)
    {
        clearValues[0].color.float32[0] = ((m_ClearColor[i] >> 16) & 0xff) * (1.0F/255.0F); //red
        clearValues[0].color.float32[1] = ((m_ClearColor[i] >>  8) & 0xff) * (1.0F/255.0F); //green
        clearValues[0].color.float32[2] = ((m_ClearColor[i] >>  0) & 0xff) * (1.0F/255.0F); //blue
        clearValues[0].color.float32[3] = ((m_ClearColor[i] >> 24) & 0xff) * (1.0F/255.0F); //alpha
        clearValues[1].depthStencil = { 0.0f, 0 };
        if (m_SampleCount > 1)
        {
            clearValues[2].color = clearValues[0].color;
        }

        CHECK_VK_TO_RC(m_PreDrawCmdBuffers[i]->BeginCmdBuffer());

        VK_DEBUG_MARKER_BEGIN(markerPreDraw, m_EnableDebugMarkers, m_pVulkanDev,
            m_PreDrawCmdBuffers[i]->GetCmdBuffer(), "PreDraw%d", i);

        VkRenderPassBeginInfo renderPassBeginInfo = {};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.pNext = nullptr;
            renderPassBeginInfo.renderPass = m_ClearRenderPass->GetRenderPass();
            renderPassBeginInfo.renderArea.offset.x = 0;
            renderPassBeginInfo.renderArea.offset.y = 0;
            renderPassBeginInfo.renderArea.extent.width = m_FrameBuffers[i]->GetWidth();
            renderPassBeginInfo.renderArea.extent.height = m_FrameBuffers[i]->GetHeight();
            renderPassBeginInfo.clearValueCount = static_cast<UINT32>(clearValues.size());
            renderPassBeginInfo.pClearValues = clearValues.data();
            renderPassBeginInfo.framebuffer = m_FrameBuffers[i]->GetVkFrameBuffer();

            // Begin/end the RenderPass to apply the clear values to framebuffer
            m_pVulkanDev->CmdBeginRenderPass(m_PreDrawCmdBuffers[i]->GetCmdBuffer(),
                &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            m_pVulkanDev->CmdEndRenderPass(m_PreDrawCmdBuffers[i]->GetCmdBuffer());

        VK_DEBUG_MARKER_END(markerPreDraw);

        CHECK_VK_TO_RC(m_PreDrawCmdBuffers[i]->EndCmdBuffer());
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::BuildDrawJob()
{
    RC rc;

    VK_DEBUG_MARKER(markerDrawBegin, 0.0f, 0.8f, 0.0f)
    VK_DEBUG_MARKER(markerDrawSub, 0.0f, 0.0f, 0.8f)
    VK_DEBUG_MARKER(markerDrawEnd, 0.8f, 0.0f, 0.0f)

    auto& lwrrCmdBuf = m_DrawJobs[m_DrawJobIdx % m_NumDrawJobs];

    CHECK_VK_TO_RC(lwrrCmdBuf->ResetCmdBuffer());

    // We use 2 timestamp queries for graphics + 2 for raytracing per draw job.
    auto& lwrrFb = m_FrameBuffers[m_LwrrentSwapChainIdx];
    if (!m_QueryIdx && m_NumFrames && m_ShowQueryResults)
    {
        DumpQueryResults();
    }

    CHECK_VK_TO_RC(lwrrCmdBuf->BeginCmdBuffer());

    VK_DEBUG_MARKER_BEGIN(markerDrawBegin, m_EnableDebugMarkers, m_pVulkanDev,
        lwrrCmdBuf->GetCmdBuffer(), "Draw%lld_begin", m_DrawJobIdx);

    m_GfxTSQueries[m_QueryIdx]->CmdReset(lwrrCmdBuf.get(), 0, MAX_NUM_TIMESTAMPS);
    m_GfxTSQueries[m_QueryIdx]->CmdWriteTimestamp(lwrrCmdBuf.get()
                                        ,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                                        ,0);

    if (m_UseCompute && !m_AsyncCompute)
    {
        UINT32 startRunning =  START_ARITHMETIC_OPS;
        // update parameters inline
        for (auto &cw : m_ComputeWorks)
        {
            CHECK_VK_TO_RC(VkUtil::UpdateBuffer(lwrrCmdBuf.get()
                                 ,cw.m_ComputeKeepRunning.get()
                                 ,0 //dst offset
                                 ,sizeof(UINT32)
                                 ,&startRunning
                                 ,VK_ACCESS_TRANSFER_WRITE_BIT //srcAccess
                                 ,VK_ACCESS_SHADER_READ_BIT   // dstAccess
                                 ,VK_PIPELINE_STAGE_TRANSFER_BIT // srcStageMask
                                 ,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)); //dstStageMask
        }
    }

    VkRenderPassBeginInfo renderPassBeginInfo = { };
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = nullptr;
    renderPassBeginInfo.renderPass = m_RenderPass->GetRenderPass();
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = lwrrFb->GetWidth();
    renderPassBeginInfo.renderArea.extent.height = lwrrFb->GetHeight();
    renderPassBeginInfo.clearValueCount = 0;
    renderPassBeginInfo.pClearValues = nullptr;
    renderPassBeginInfo.framebuffer = lwrrFb->GetVkFrameBuffer();
    m_pVulkanDev->CmdBeginRenderPass(lwrrCmdBuf->GetCmdBuffer(),
        &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind vertex and index buffers
    if (!m_UseMeshlets)
    {
        const VkBuffer vtxBuffer = m_VBVertices->GetBuffer();
        const VkDeviceSize bufferOffset = 0;
        m_pVulkanDev->CmdBindVertexBuffers(
            lwrrCmdBuf->GetCmdBuffer() // commandBuffer
            ,m_VBBindingVertices                // firstBinding
            ,1                                  // bindingCount
            ,&vtxBuffer                         // pBuffersHandles
            ,&bufferOffset);                    // pBufferOffsets

        if (m_IndexedDraw)
        {
            m_pVulkanDev->CmdBindIndexBuffer(
                lwrrCmdBuf->GetCmdBuffer()     //commandBuffer
                ,m_IndexBuffer->GetBuffer()             //buffer being bound
                ,0                                      //starting offset in bytes in the buffer
                ,VK_INDEX_TYPE_UINT32);                 //indices are 32bits
        }
    }

    // We use the same descriptor set for all of the pipeline objects.
    const VkDescriptorSet descSet = m_DescriptorInfo->GetDescriptorSet(0);
    m_pVulkanDev->CmdBindDescriptorSets(lwrrCmdBuf->GetCmdBuffer()
        ,VK_PIPELINE_BIND_POINT_GRAPHICS                //pipelineBindPoint
        ,m_GraphicsPipeline.GetPipelineLayout()    //layout object
        ,0          //firstSet
        ,1          //descriptorSetCount
        ,&descSet   //descriptorSet
        ,0          //dynamicOffsetCount
        ,nullptr    //dynamicOffsets
    );

    if ( (m_PulseMode == TILE_MASK) ||
         (m_PulseMode == TILE_MASK_CONSTANT_DUTY) ||
         (m_PulseMode == TILE_MASK_USER) ||
         (m_PulseMode == TILE_MASK_GPC) ||
         (m_PulseMode == TILE_MASK_GPC_SHUFFLE) )
    {
        UINT64 tileMask;
        switch (m_PulseMode)
        {
            case TILE_MASK:
            case TILE_MASK_CONSTANT_DUTY:
                // Change the mask only when the frame is black.
                // "SetRoundToQuadFrame(true)" should be used.
                if (m_NumFrames % 4 == 0)
                {
                    DutyWorkController* pWc = dynamic_cast<DutyWorkController*>(m_WorkController.get());
                    MASSERT(pWc);
                    const UINT32 numBits =
                        max(1U, static_cast<UINT32>(floor(pWc->GetDutyPct() / 100.0 * 64.0)));
                    tileMask = GenRandMask(numBits, 64);
                    m_PrevRandMask = tileMask;
                    if (m_Debug & DBG_FMAX)
                    {
                        Printf(Tee::PriNormal, "Sending mask with %u bits: 0x%016llx, duty%%=%.1f\n",
                            numBits, tileMask, pWc->GetDutyPct());
                    }
                }
                else
                {
                    tileMask = m_PrevRandMask;
                }
                break;
            case TILE_MASK_USER:
                tileMask = m_TileMask;
                tileMask <<= 32;
                tileMask |= m_TileMask;
                break;
            case TILE_MASK_GPC:
            {
                UINT32 tileMask32;
                CHECK_RC(GetBoundGpuSubdevice()->HwGpcToVirtualGpcMask(m_TileMask, &tileMask32));
                tileMask = tileMask32;
                break;
            }
            case TILE_MASK_GPC_SHUFFLE:
                // Change the mask only when the frame is black.
                // "SetRoundToQuadFrame(true)" should be used.
                if (m_NumFrames % 4 == 0)
                {
                    // Randomly pick m_NumActiveGPCs bits set in the mask:
                    tileMask = GenRandMask(m_NumActiveGPCs, m_HighestGPCIdx + 1);
                    m_PrevRandMask = tileMask;
                    if (m_Debug & DBG_FMAX)
                    {
                        Printf(Tee::PriNormal, "Sending mask with %u bits: 0x%08llx\n",
                            m_NumActiveGPCs, tileMask);
                    }
#ifndef VULKAN_STANDALONE
                    m_GPCShuffleStats.AddPoint(static_cast<UINT32>(tileMask));
#endif
                }
                else
                {
                    tileMask = m_PrevRandMask;
                }
                break;
            default:
                tileMask = 1;
        }

        m_pVulkanDev->CmdPushConstants(lwrrCmdBuf->GetCmdBuffer(),
                                       m_GraphicsPipeline.GetPipelineLayout(),
                                       VK_SHADER_STAGE_FRAGMENT_BIT,
                                       8, // index into the push constant buffer
                                       8, // number of bytes to push
                                       &tileMask);
    }

    VK_DEBUG_MARKER_END(markerDrawBegin);

    MASSERT(m_FramesPerSubmit); // We need to draw at least one frame
    for (UINT32 frameIdx = 0; frameIdx < m_FramesPerSubmit; frameIdx++)
    {
        VK_DEBUG_MARKER_BEGIN(markerDrawSub, m_EnableDebugMarkers, m_pVulkanDev,
            lwrrCmdBuf->GetCmdBuffer(), "Draw%lld_sub%d", m_DrawJobIdx, frameIdx);

        CHECK_RC(AddFrameToCmdBuffer(m_NumFrames + frameIdx, lwrrCmdBuf.get()));

        VK_DEBUG_MARKER_END(markerDrawSub);
    }

    VK_DEBUG_MARKER_BEGIN(markerDrawEnd, m_EnableDebugMarkers, m_pVulkanDev,
        lwrrCmdBuf->GetCmdBuffer(), "Draw%lld_end", m_DrawJobIdx);

    m_pVulkanDev->CmdEndRenderPass(lwrrCmdBuf->GetCmdBuffer());

    if (m_UseCompute && !m_AsyncCompute)
    {
        UINT32 stopRunning = STOP_ARITHMETIC_OPS;
        // update parameters inline
        for (auto &cw : m_ComputeWorks)
        {
            CHECK_VK_TO_RC(VkUtil::UpdateBuffer(lwrrCmdBuf.get()
                                 ,cw.m_ComputeKeepRunning.get()
                                 ,0 //dst offset
                                 ,sizeof(UINT32)
                                 ,&stopRunning
                                 ,VK_ACCESS_TRANSFER_WRITE_BIT //srcAccess
                                 ,VK_ACCESS_SHADER_READ_BIT   // dstAccess
                                 ,VK_PIPELINE_STAGE_TRANSFER_BIT // srcStageMask
                                 ,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)); //dstStageMask
        }
    }

    m_GfxTSQueries[m_QueryIdx]->CmdWriteTimestamp(lwrrCmdBuf.get()
                                        ,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                                        ,1);
    CHECK_VK_TO_RC(m_GfxTSQueries[m_QueryIdx]->CmdCopyResults(lwrrCmdBuf.get()
                                     ,0                      //first index
                                     ,2                      //count
                                     ,VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

    VK_DEBUG_MARKER_END(markerDrawEnd);

    CHECK_VK_TO_RC(lwrrCmdBuf->EndCmdBuffer());

    if (m_Rt.get())
    {
        UINT32 idx = static_cast<UINT32>((m_DrawJobIdx % m_NumDrawJobs));
        CHECK_RC(m_Rt->BuildDrawJob(idx, m_QueryIdx, m_NumFrames));
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::AddFrameToCmdBuffer(UINT32 frameIdx, VulkanCmdBuffer* pCmdBuf)
{
    RC rc;
    const UINT32 frameMod4 = frameIdx % 4;
    const bool isFarFrame = (frameMod4 == 1 || frameMod4 == 2);

    m_pVulkanDev->CmdBindPipeline(pCmdBuf->GetCmdBuffer(),
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  m_GraphicsPipeline.GetPipeline());

    SetDepthStencilState(pCmdBuf->GetCmdBuffer(), frameMod4);

    const UINT32 texIdx = isFarFrame ? m_TexIdx + m_TexIdxFarOffset : m_TexIdx;
    m_pVulkanDev->CmdPushConstants(pCmdBuf->GetCmdBuffer(),
                                   m_GraphicsPipeline.GetPipelineLayout(),
                                   VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, // index into the push constant buffer
                                   4, // number of bytes to push
                                   &texIdx);
    if (m_UseMeshlets)
    {
        const UINT32 farPlane = (frameMod4 == 1 || frameMod4 == 2) ? 1 : 0;
        m_pVulkanDev->CmdPushConstants(pCmdBuf->GetCmdBuffer(),
                                       m_GraphicsPipeline.GetPipelineLayout(),
                                       VK_SHADER_STAGE_MESH_BIT_LW,
                                       4, // index into the push constant buffer
                                       sizeof(farPlane), // number of bytes to push
                                       &farPlane);
    }
    else if (m_UseTessellation)
    {
        const FLOAT32 zPlaneMultiplier = (frameMod4 == 1 || frameMod4 == 2)
                                         ? m_FarZMultiplier : m_NearZMultiplier;
        m_pVulkanDev->CmdPushConstants(pCmdBuf->GetCmdBuffer(),
                                       m_GraphicsPipeline.GetPipelineLayout(),
                                       VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                       4, // index into the push constant buffer
                                       sizeof(zPlaneMultiplier), // number of bytes to push
                                       &zPlaneMultiplier);
    }

    // After the last frame of a quadframe, advance the texture index
    if (frameMod4 == 3)
    {
        m_TexIdx = (m_TexIdx + m_TexIdxStride) % m_NumTextures;
    }

    // Draw!
    if (m_UseMeshlets)
    {
        m_pVulkanDev->CmdDraw(pCmdBuf->GetCmdBuffer(), 1, 1, 0, 0);
    }
    else if (m_IndexedDraw)
    {
        const UINT32 firstIndex = isFarFrame ? m_IdxPerPlane : 0;
        m_pVulkanDev->CmdDrawIndexed(
            pCmdBuf->GetCmdBuffer()
            ,m_IdxPerPlane          // number of vertices to draw
            ,1                      // instance count
            ,firstIndex             // first index (changes for far/near planes)
            ,0                      // vertex offset
            ,0);                    // first instance
    }
    else
    {
        const UINT32 firstIndex = isFarFrame ? m_VxPerPlane : 0;
        m_pVulkanDev->CmdDraw(
            pCmdBuf->GetCmdBuffer() // commandBuffer
            ,m_VxPerPlane           // vertex count
            ,1                      // instance count
            ,firstIndex             // first vertex
            ,0);                    // first instance
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::SetupDrawCmdBuffers()
{
    RC rc;

    // Create two DrawJobs for double buffering of the GPU workload
    for (size_t ii = 0; ii < m_NumDrawJobs; ii++)
    {
        m_DrawJobs.emplace_back(make_unique<VulkanCmdBuffer>(m_pVulkanDev));
        CHECK_VK_TO_RC(m_DrawJobs.back()->AllocateCmdBuffer(m_CmdPool.get()));
    }

    // numFBs is likely to be three
    const size_t numFBs = m_FrameBuffers.size();
    for (size_t ii = 0; ii < numFBs; ii++)
    {
        m_PreDrawCmdBuffers.emplace_back(make_unique<VulkanCmdBuffer>(m_pVulkanDev));
        CHECK_VK_TO_RC(m_PreDrawCmdBuffers.back()->AllocateCmdBuffer(m_CmdPool.get()));
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
// Submit the pre-draw command buffer to initialize each swapchain imageView.
RC VkStress::SubmitPreDrawCmdBufferAndPresent()
{
    RC rc;
    MASSERT(m_PreDrawCmdBuffers.size() <= m_SwapChainAcquireSems.size());
    // Initialize all of the swapchain images.
    for (UINT32 idx = 0; idx < m_PreDrawCmdBuffers.size(); idx++)
    {
        m_LwrrentSwapChainIdx = m_SwapChain->GetNextSwapChainImage(m_SwapChainAcquireSems[idx].GetSemaphore());
    #ifdef VULKAN_STANDALONE_KHR
        MASSERT(m_LwrrentSwapChainIdx == idx);
    #endif
        VkSemaphore preDrawCompleteSem = VK_NULL_HANDLE;
        UINT32 numSignalSem = 0;

        VkSemaphore waitSemaphore = VK_NULL_HANDLE;
    #ifdef VULKAN_STANDALONE_KHR
        if (m_DetachedPresentMs == 0)
        {
            //change layout to color attachment before draw
            CHECK_VK_TO_RC(m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx)->SetImageLayout(
                m_InitCmdBuffer.get()
                ,m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx)->GetImageAspectFlags()
                ,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL           //new layout
                ,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT               //new access
                ,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT));   //new stage

            // Wait for AcquireNextImageKHR to complete before drawing into the image
            waitSemaphore = m_SwapChainAcquireSems[idx].GetSemaphore();

            // We need to synchronize presentation of the clear frame
            preDrawCompleteSem = m_RenderSem.front();
            numSignalSem = 1;
        }
    #endif

        CHECK_VK_TO_RC(m_PreDrawCmdBuffers[m_LwrrentSwapChainIdx]->ExelwteCmdBuffer(
            waitSemaphore               //wait semaphore
            ,numSignalSem
            ,&preDrawCompleteSem        //signal semapore
            ,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT //wait stage mask
            ,true                       //wait on fence
            ,VK_NULL_HANDLE,            // Create a temporary fence
            false,                      // use polling
            true));                     // resetAfteExec

    #ifdef VULKAN_STANDALONE_KHR
        if (m_DetachedPresentMs == 0)
        {
            //change layout to present mode
            CHECK_VK_TO_RC(m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx)->SetImageLayout(
                m_InitCmdBuffer.get()
                ,m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx)->GetImageAspectFlags()
                ,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                ,VK_ACCESS_MEMORY_READ_BIT
                ,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));

            CHECK_VK_TO_RC(m_SwapChain->PresentImage(
                m_LwrrentSwapChainIdx
                ,preDrawCompleteSem   // wait semaphore
                ,VK_NULL_HANDLE       // ignored by SwapChainKHR
                ,true));

            // Without detached present, we only render to swap chain image 0
            // and the remaining ones contain just a copy from the one at index 0.
            break;
        }
    #else
        CHECK_RC(SetDisplayImage(m_LwrrentSwapChainIdx));
    #endif
    }
    return rc;
}

#ifdef VULKAN_STANDALONE_KHR
RC VkStress::PresentTestImage(VkSemaphore* pWaitSema)
{
    RC rc;

    // Copy image from swap chain 0 where VkStress draws to nth swap chain image
    VulkanImage& srcImg = *m_SwapChain->GetSwapChainImage(0);
    VulkanImage& dstImg = *m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx);

    VulkanCmdBuffer& cmdBuf = m_PresentCmdBuf[m_LwrrentSwapChainIdx];
    CHECK_VK_TO_RC(cmdBuf.ResetCmdBuffer());
    CHECK_VK_TO_RC(cmdBuf.BeginCmdBuffer());

    CHECK_VK_TO_RC(srcImg.SetImageLayout(&cmdBuf,
                                         srcImg.GetImageAspectFlags(),
                                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         VK_ACCESS_TRANSFER_READ_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT));
    CHECK_VK_TO_RC(dstImg.SetImageLayout(&cmdBuf,
                                         dstImg.GetImageAspectFlags(),
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_ACCESS_TRANSFER_WRITE_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT));
    VkImageCopy copyRegion = { };
    copyRegion.srcSubresource.aspectMask = srcImg.GetImageAspectFlags();
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = dstImg.GetImageAspectFlags();
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent.width              = srcImg.GetWidth();
    copyRegion.extent.height             = srcImg.GetHeight();
    copyRegion.extent.depth              = 1;
    m_pVulkanDev->CmdCopyImage(cmdBuf.GetCmdBuffer(),
                               srcImg.GetImage(),
                               srcImg.GetImageLayout(),
                               dstImg.GetImage(),
                               dstImg.GetImageLayout(),
                               1,
                               &copyRegion);
    CHECK_VK_TO_RC(dstImg.SetImageLayout(&cmdBuf,
                                         dstImg.GetImageAspectFlags(),
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                         VK_ACCESS_MEMORY_READ_BIT,
                                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));
    CHECK_VK_TO_RC(cmdBuf.EndCmdBuffer());

    const UINT32 nextSemIdx = (m_LwrrentSwapChainIdx + 1) % m_SwapChain->GetNumImages();
    VkSemaphore nextSema = m_SwapChainAcquireSems[nextSemIdx].GetSemaphore();
    CHECK_VK_TO_RC(cmdBuf.ExelwteCmdBuffer(*pWaitSema,
                                           1,
                                           &nextSema,
                                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           false,
                                           VK_NULL_HANDLE));

    // Present nth swap chain image
    CHECK_VK_TO_RC(m_SwapChain->PresentImage(m_LwrrentSwapChainIdx,
                                             nextSema,
                                             VK_NULL_HANDLE, // ignored by SwapChainKHR
                                             false));
    *pWaitSema = nextSema;

    return RC::OK;
}
#endif

RC VkStress::SendWorkToGpu()
{
    RC rc;

    if (m_SynchronousMode)
    {
        Tasker::WaitOnBarrier();
    }

    const size_t lwrrDrawJobIdx = static_cast<size_t>(m_DrawJobIdx % m_NumDrawJobs);
    const size_t prevDrawJobIdx = static_cast<size_t>((m_DrawJobIdx - 1) % m_NumDrawJobs);
    const auto& lwrrCmdBuf = m_DrawJobs[m_DrawJobIdx % m_NumDrawJobs];
    const auto& lwrrRenderFence = m_RenderFences[lwrrDrawJobIdx];

    VkSemaphore waitSema = m_DrawJobIdx ? m_RenderSem[prevDrawJobIdx] : static_cast<VkSemaphore>(VK_NULL_HANDLE);
#ifdef VULKAN_STANDALONE_KHR
    if (m_DetachedPresentMs == 0)
    {
        const UINT32 semIdx = (m_LwrrentSwapChainIdx + 1) % m_SwapChain->GetNumImages();
        waitSema = m_SwapChainAcquireSems[semIdx].GetSemaphore();
        m_LwrrentSwapChainIdx = m_SwapChain->GetNextSwapChainImage(waitSema);
        MASSERT(m_LwrrentSwapChainIdx == semIdx);

        // Go back to swap chain image 0 where we do XOR
        while (m_LwrrentSwapChainIdx)
        {
            CHECK_RC(PresentTestImage(&waitSema));

            // Acquire next swap chain image
            m_LwrrentSwapChainIdx = m_SwapChain->GetNextSwapChainImage(waitSema);
        }

        // Change the render target layout back to optimal
        CHECK_VK_TO_RC(m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx)->SetImageLayout(
            m_InitCmdBuffer.get()
            ,m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx)->GetImageAspectFlags()
            ,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL           //new layout
            ,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT               //new access
            ,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT));   //new stage
    }
#endif

    if (m_Debug & DBG_BUFFER_CHECK)
    {   // Set Verbose = true so see this print as well.
        VerbosePrintf("Running DrawJobIdx:%lld using swapChainIdx:%d FramePerSubmit:%d\n",
                      m_DrawJobIdx, m_LwrrentSwapChainIdx, m_FramesPerSubmit);
    }

    if (m_UseCompute && !m_AsyncCompute)
    {
        for (UINT32 ii = 0; ii < m_ComputeWorks.size(); ii++)
        {
            ComputeWork &cw = m_ComputeWorks[ii];
            CHECK_RC(BuildComputeCmdBuffer(&cw));
            // Launch a compute shader that will wait for all of the SMs to load and
            // then exit to cause a GPU semaphore to be signaled. The Gfx cmd buffer will wait
            // on this GPU semaphore before proceeding.
            // Semaphores: wait on render, signal computeLoaded
            CHECK_VK_TO_RC(cw.m_ComputeLoadedCmdBuffers[lwrrDrawJobIdx]->ExelwteCmdBuffer(
                waitSema,
                (ii == 0) ? 1 : 0,                               //num signal sems
                (ii == 0) ? &cw.m_ComputeLoadedSemaphores[lwrrDrawJobIdx] : nullptr,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                false,                              //don't wait on a fence
                VK_NULL_HANDLE));                   //don't signal any fence

            if (waitSema != VK_NULL_HANDLE)
            {
                waitSema = VK_NULL_HANDLE;
            }
        }
        for (auto &cw : m_ComputeWorks)
        {
            // Launch the "real" compute shader that:
            // 1. Updates the smLoaded[] for each SM. Then
            // 2. Waits for the Start cmd. Then
            // 3. Performs the workload and periodically polls for the Stop cmd to exit.
            // The Start/Stop cmds are done using buffer updates from the graphics queue.
            // Semaphores: wait on compute, signal compute
            VkSemaphore waitComputeSema = m_DrawJobIdx ?
                cw.m_ComputeSemaphores[prevDrawJobIdx] : static_cast<VkSemaphore>(VK_NULL_HANDLE);
            CHECK_VK_TO_RC(cw.m_ComputeCmdBuffers[lwrrDrawJobIdx]->ExelwteCmdBuffer(
                waitComputeSema                            // waitSemaphore
                ,1                                         // numSignalSemaphores
                ,&cw.m_ComputeSemaphores[lwrrDrawJobIdx]   // pSignalSemaphores
                ,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                ,false                                     // don't wait on the fence
                ,cw.m_ComputeFences[lwrrDrawJobIdx]        // signal fence
                ,false));                                  // usePolling
        }

        // Launch the graphics workload. This launch doesn't happen until the
        // CompueLoadedShader exits and signals the ComputeLoadedSemaphore.
        // Semaphores: wait on computeLoaded, signal render
        CHECK_VK_TO_RC(lwrrCmdBuf->ExelwteCmdBuffer(
            m_ComputeWorks[0].m_ComputeLoadedSemaphores[lwrrDrawJobIdx], //wait sem
            1,                                         //num signal sems
            &m_RenderSem[lwrrDrawJobIdx],              //signal sem
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            false,                                     // don't wait on a fence
            lwrrRenderFence));                         // signal fence
    }
    else
    {
        CHECK_VK_TO_RC(lwrrCmdBuf->ExelwteCmdBuffer(
            waitSema,                           // wait semaphore
            1,                                  // num signal semaphores
            &m_RenderSem[lwrrDrawJobIdx],       // signal sem
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            false,                              // don't wait on a fence
            lwrrRenderFence));                  // signal fence
    }
    if (m_Rt.get())
    {
        CHECK_RC(m_Rt->SendWorkToGpu(m_DrawJobIdx));
    }

#ifdef VULKAN_STANDALONE_KHR
    if (m_DetachedPresentMs == 0)
    {
        //change layout to present mode
        CHECK_VK_TO_RC(m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx)->SetImageLayout(
            m_InitCmdBuffer.get()
            ,m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx)->GetImageAspectFlags()
            ,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            ,VK_ACCESS_MEMORY_READ_BIT
            ,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));

        CHECK_VK_TO_RC(m_SwapChain->PresentImage(
            m_LwrrentSwapChainIdx                               // image index
            ,m_RenderSem[lwrrDrawJobIdx]                        // wait on semaphore
            ,VK_NULL_HANDLE                                     // ignored by SwapChainKHR
            ,true));
    }
#endif

    return rc;
}

//------------------------------------------------------------------------------------------------
// Note: We are going to use a detached thread to check the swapchain images bound to the
//       goldensurfaces. To do the we must externally synchronize all access to the GPU. So we
//       must use a different VkQueue for any commands running in a detached thread.
//       In our case we use Transfer Queue 1 and Graphics Queue 1 for this purpose.
//       So m_CheckSurfaceTransBuffer is allocated from the Transfer Queue 1 and
//       m_CheckSurfaceCmdBuffer is allocated from Graphics Queue 1.
RC VkStress::CreateGoldenSurfaces()
{
    RC rc;

    const UINT32 swapChainSize = static_cast<UINT32>(m_SwapChain->GetNumImages());
    for (UINT32 surfacesIdx = 0; surfacesIdx < swapChainSize; surfacesIdx++)
    {
        m_GoldenSurfaces.emplace_back(make_unique<VulkanGoldenSurfaces>(&m_VulkanInst));
        auto &gs = m_GoldenSurfaces[surfacesIdx];

        CHECK_VK_TO_RC(gs->AddSurface("c"
                                      ,m_SwapChain->GetSwapChainImage(surfacesIdx)
                                      ,m_CheckSurfaceCmdBuffer.get()
                                      ,m_CheckSurfaceTransBuffer.get()));

        CHECK_VK_TO_RC(gs->AddSurface("z"
                                      ,m_DepthImages[surfacesIdx].get()
                                      ,m_CheckSurfaceCmdBuffer.get()
                                      ,m_CheckSurfaceTransBuffer.get()));
    }
    return rc;
}
void VkStress::AddDbgMessagesToIgnore()
{
    m_VulkanInst.ClearDebugMessagesToIgnore();

    if (m_UseCompute)
    {
        // This warning is generated because we are using features from the VK_LW_internal
        // extension which is not advertised. Validation Layers 1.2.137
        m_VulkanInst.AddDebugMessageToIgnore(
        {
            VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT
            ,0
            ,"Validation"
            ,"vkCreateComputePipelines: pCreateInfos[0].pNext chain includes a structure with unknown VkStructureType (1000103003)%c" //$
            ,';'
        });
    }

    if (m_Rt.get())
    {
        m_Rt->AddDebugMessagesToIgnore();
    }

    //*********************************************************************************************
#if defined(VULKAN_STANDALONE_KHR)
    if (m_SampleCount == 1)
    {
        // vktest.exe has to use swapchain KHR which doesn't allow for single
        // image chain. Since we are always rendering to just one image we
        // also always present only this one. Ignore the validation error
        // related to that (so far the hack works despite the message):
        m_VulkanInst.AddDebugMessageToIgnore(
        {
            VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT
            ,97
            ,"DS"
            ,"vkQueuePresentKHR: Swapchain image index 0 has not been acquired%c"
            ,'.'
        });
        // Add new message to support validations layers 1.1.106.0
        m_VulkanInst.AddDebugMessageToIgnore(
        {
            VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT
            ,0
            ,"Validation"
            ,"vkQueuePresentKHR: Swapchain image index 0 has not been acquired%c"
            ,'.'
        });

    }
#endif
}
//------------------------------------------------------------------------------------------------
RC VkStress::AllocateVulkanDeviceResources()
{
    RC rc;

    // Need to set the timeout threshold for the VulkanDevice before creating
    // any command buffers that run on it so that they inherit this timeout.
    UINT64 timeoutNs = static_cast<UINT64>(GetTestConfiguration()->TimeoutMs()) * 1000000ULL;

    // Compute shaders can cause the workload to be exceptionally long if you use large tuning
    // values. So add a bunch of margin here as a buffer.
    if (m_UseCompute || m_UseRaytracing)
    {
        timeoutNs *= 3;
    }
    m_pVulkanDev->SetTimeoutNs(timeoutNs);

    m_SwapChainMods = make_unique<SwapChainMods>(&m_VulkanInst, m_pVulkanDev);
#ifdef VULKAN_STANDALONE_KHR
    m_SwapChainKHR = make_unique<SwapChainKHR>(&m_VulkanInst, m_pVulkanDev);
#endif

    CHECK_RC(SetupSemaphoresAndFences());

    //create swap chain
    if (m_ZeroFb)
    {
        m_SwapChainMods->SetImageMemoryType(VK_MEMORY_PROPERTY_SYSMEM_BIT);
#ifdef VULKAN_STANDALONE_KHR
        m_SwapChainKHR->SetImageMemoryType(VK_MEMORY_PROPERTY_SYSMEM_BIT);
#endif
    }
#ifdef VULKAN_STANDALONE_KHR
    const VkResult vk_resKHR = m_SwapChainKHR->Init(
        m_Hinstance
        ,m_HWindow
        // When MSAA is enabled we can use multiple buffers as they are not used to accumulate the
        // state. But without MSAA the image allocated by KHR is used to accumulate state. We
        // might want to reorg that in future.
        ,(m_SampleCount == 1) && (m_DetachedPresentMs == 0) ?
            SwapChain::SINGLE_IMAGE_MODE :
            SwapChain::MULTI_IMAGE_MODE
        ,m_RenderFences.front()
        ,m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx()
        ,m_DetachedPresentQueueIdx
    );
    CHECK_VK_TO_RC(vk_resKHR);
    m_SwapChain = m_SwapChainKHR.get();
    if (m_DetachedPresentMs > 0)
    {
#endif
    const VkResult vk_resMods = m_SwapChainMods->Init(
        m_RenderWidth
        ,m_RenderHeight
        ,(m_BufferCheckMs == BUFFERCHECK_MS_DISABLED) ?
            SwapChain::SINGLE_IMAGE_MODE :
            SwapChain::MULTI_IMAGE_MODE
        ,m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx()
        ,0 //queueIndex
    );
    CHECK_VK_TO_RC(vk_resMods);
    m_SwapChain = m_SwapChainMods.get();
#ifdef VULKAN_STANDALONE_KHR
    }
#endif

    m_SwapChainAcquireSems.resize(m_SwapChain->GetNumImages());
    for (VulkanSemaphore& sem : m_SwapChainAcquireSems)
    {
        sem = VulkanSemaphore(m_pVulkanDev);
        CHECK_VK_TO_RC(sem.CreateBinarySemaphore());
    }

    // Keep track of miscompares for each swapchain image
    UINT32 numImages = static_cast<UINT32>(m_SwapChain->GetNumImages());
    if (m_UseRaytracing)
        numImages++;
    m_Miscompares.resize(numImages);

    for (unsigned i = 0; i < m_SwapChain->GetNumImages(); i++)
    {
        m_DepthImages.emplace_back(make_unique<VulkanImage>(m_pVulkanDev));
    }
    m_HUniformBufferMVPMatrix = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_UniformBufferMVPMatrix = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_VBVertices = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_HIndexBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_IndexBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_Sm2GpcBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_CmdPool = make_unique<VulkanCmdPool>(m_pVulkanDev);
    m_InitCmdBuffer = make_unique<VulkanCmdBuffer>(m_pVulkanDev);

    m_CheckSurfaceCmdPool = make_unique<VulkanCmdPool>(m_pVulkanDev);
    m_CheckSurfaceCmdBuffer = make_unique<VulkanCmdBuffer>(m_pVulkanDev);
    // The transfer queue will utilize the hardware's copy engines.
    m_CheckSurfaceTransPool = make_unique<VulkanCmdPool>(m_pVulkanDev);
    m_CheckSurfaceTransBuffer = make_unique<VulkanCmdBuffer>(m_pVulkanDev);

    m_RenderPass = make_unique<VulkanRenderPass>(m_pVulkanDev);
    m_DescriptorInfo = make_unique<DescriptorInfo>(m_pVulkanDev);
    m_MeshletsTaskShader = make_unique<VulkanShader>(m_pVulkanDev);
    m_MeshletsMainShader = make_unique<VulkanShader>(m_pVulkanDev);
    m_VertexShader = make_unique<VulkanShader>(m_pVulkanDev);
    m_TessellationControlShader = make_unique<VulkanShader>(m_pVulkanDev);
    m_TessellationEvaluationShader = make_unique<VulkanShader>(m_pVulkanDev);
    m_FragmentShader = make_unique<VulkanShader>(m_pVulkanDev);
    for (unsigned i = 0; i < m_SwapChain->GetNumImages(); i++)
    {
        m_MsaaImages.push_back(make_unique<VulkanImage>(m_pVulkanDev));
    }
    m_ClearRenderPass = make_unique<VulkanRenderPass>(m_pVulkanDev);
    m_HUniformBufferLights = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_UniformBufferLights = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_RC(AllocateComputeResources());

    return rc;
}

RC VkStress::InitFromJs()
{
    RC rc;
#ifndef VULKAN_STANDALONE
    CHECK_RC(GpuTest::InitFromJs());
    if (m_Rt.get())
    {
        CHECK_RC(m_Rt->InitFromJs());
    }
#endif
    return rc;
}

// This routine is here so we can flag if the m_BufferCheckMs was specifically set by the user.
// See more comments in VerifyQueueRequirements()
RC VkStress::InternalSetBufferCheckMs(UINT32 val)
{
    m_BufferCheckMs = val;
    m_BufferCheckMsSetByJs = true;
    return RC::OK;
}

//-------------------------------------------------------------------------------------------------
// Validate the number of required queues against the number of availabe queues. If there
// are not enough then print an error message and return an error. Here are the requirements
// Feature      GraphicsQueues  ComputeQueues   TransferQueues
// Basic            1               0               1
// BufferCheckMs    1               -               1
// m_UseCompute                     1-8             -
// m_UseRaytracing  1               1               -
// Note: When ComputeTexFiller is implemented you will need to add 1 ComputeQueue to the basic
//       features requirement.
RC VkStress::VerifyQueueRequirements
(
    UINT32 *pNumGraphicsQueues,
    UINT32 *pNumTransferQueues,
    UINT32 *pNumComputeQueues,
    UINT32 *pSurfCheckGfxQueueIdx,
    UINT32 *pSurfCheckTransQueueIdx
)
{
    StickyRC rc;
    // Initialize the GPU with the required extensions and features.
    // Before we try to initialize the VulkanDevice with the number of queues we would like to have
    // lets see what is avaialble and return an error if necessary.
    m_AvailGraphicsQueues = m_pVulkanDev->GetPhysicalDevice()->GetMaxGraphicsQueues();
    m_AvailTransferQueues = m_pVulkanDev->GetPhysicalDevice()->GetMaxTransferQueues();
    m_AvailComputeQueues  = m_pVulkanDev->GetPhysicalDevice()->GetMaxComputeQueues();

    // Initialize with basic requirements.
    *pNumGraphicsQueues = 1;
    *pNumTransferQueues = 1;
    *pNumComputeQueues  = 0;
    // Some CheetAh chips only have a single transfer queue (ie 1 CopyEngine). So the default is to
    // disable this feature if the user hasn't specifically set its value. Otherwise we will
    // return an error.
    if (m_AvailTransferQueues == 1 && !m_BufferCheckMsSetByJs)
    {
        m_BufferCheckMs = BUFFERCHECK_MS_DISABLED;
    }

    // Min queues for generic VkStress
    if (m_BufferCheckMs != BUFFERCHECK_MS_DISABLED)
    {
        ++(*pNumGraphicsQueues);
        ++(*pNumTransferQueues);
    }

    *pSurfCheckGfxQueueIdx = *pNumGraphicsQueues - 1;
    *pSurfCheckTransQueueIdx = *pNumTransferQueues - 1;

    if (m_UseCompute)
    {
        *pNumComputeQueues = m_NumComputeQueues;
    }
    if (m_UseRaytracing)
    {
        ++(*pNumGraphicsQueues);
        ++(*pNumComputeQueues);
    }

#ifdef VULKAN_STANDALONE_KHR
    if (m_DetachedPresentMs > 0)
    {
        m_DetachedPresentQueueIdx = *pNumGraphicsQueues;
        ++(*pNumGraphicsQueues);
    }
#endif

    if (*pNumGraphicsQueues > m_AvailGraphicsQueues)
    {
        Printf(Tee::PriError, "Feature set requires %d graphics queues, only %d are available.\n",
               *pNumGraphicsQueues, m_AvailGraphicsQueues);
        rc = RC::MODS_VK_ERROR_TOO_MANY_OBJECTS;
    }
    if (*pNumTransferQueues > m_AvailTransferQueues)
    {
        Printf(Tee::PriError, "Feature set requires %d transfer queues, only %d are available.\n",
               *pNumTransferQueues, m_AvailTransferQueues);
        rc = RC::MODS_VK_ERROR_TOO_MANY_OBJECTS;
    }
    if (*pNumComputeQueues > m_AvailComputeQueues)
    {
        Printf(Tee::PriError, "Feature set requires %d compute queues, only %d are available.\n",
               *pNumComputeQueues, m_AvailComputeQueues);
        rc = RC::MODS_VK_ERROR_TOO_MANY_OBJECTS;
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::Setup()
{
    StickyRC rc;
    const bool enableVkDebug = true;
    m_SetupTimeCallwlator.StartTimeRecording();
    DEFER
    {
        m_SetupTimeCallwlator.StopTimeRecording();
    };

#ifdef VULKAN_STANDALONE
    m_Rt = make_unique<Raytracer>(m_pVulkanDev);
#endif
    // We have to unconditionally create teh Raytracer object so that the associate JS object
    // can be properly managed (see AddExtraObjects). However if raytracing is not enabled we
    // need to release and delete the C++ object because all subsequent code will query the
    // m_Rt unique pointer to execute raytracing.
    if (!m_UseRaytracing)
    {
        Raytracer * pRt = m_Rt.release();
        delete pRt;
    }
#ifndef VULKAN_STANDALONE
    if (GetBoundGpuSubdevice()->HasBug(2150982) && m_UseCompute)
    {
        SetDisableRcWatchdog(true);
    }
#endif

    CHECK_RC(GpuTest::Setup());

    m_RenderWidth = GetTestConfiguration()->SurfaceWidth();
    m_RenderHeight = GetTestConfiguration()->SurfaceHeight();

    if (m_MaxHeat)
    {
        CHECK_RC(ApplyMaxHeatSettings());
    }

#ifndef VULKAN_STANDALONE
    m_ZeroFb = (GetBoundGpuSubdevice()->GetFB()->GetGraphicsRamAmount() == 0);
#endif
    const UINT32 dumpPng = m_pGolden->GetDumpPng();
    if (dumpPng & (Goldelwalues::OnError | Goldelwalues::OnBadPixel))
    {
        m_DumpPngOnError = true;
    }
    if (dumpPng & (Goldelwalues::OnCheck | Goldelwalues::OnStore))
    {
        m_DumpPng = m_DumpPngMask;
    }

    m_BufCkPrintPri = (m_Debug & DBG_BUFFER_CHECK) ? Tee::PriNormal : Tee::PriSecret;

    if (m_WarpsPerSM == 0 || m_WarpsPerSM > 16)
    {
        Printf(Tee::PriWarn, "WarpsPerSM of %d is out of range. Setting to a value of 1.\n",
               m_WarpsPerSM);
        m_WarpsPerSM = 1;
    }
    // Validate some of the user's input here
    if (m_BufferCheckMs != BUFFERCHECK_MS_DISABLED)
    {
        if (m_FramesPerSubmit % 4)
        {
            Printf(Tee::PriWarn,
                   "Disabling BufferCheckMs due to custom FramesPerSubmit value\n");
            m_BufferCheckMs = BUFFERCHECK_MS_DISABLED;
        }
        if (!m_BufferCheckMs)
        {
            Printf(Tee::PriError, "BufferCheckMs must be greater than 0\n");
            return RC::ILWALID_ARGUMENT;
        }
        m_NextBufferCheckMs = m_BufferCheckMs;
    }

#ifndef VULKAN_STANDALONE
    if (m_PulseMode == TILE_MASK_GPC_SHUFFLE)
    {
        m_GPCShuffleStats.Reset();
    }

    // Force m_SkipDirectDisplay if the GPU doesn't have a display engine.
    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_ISO_DISPLAY))
    {
        m_SkipDirectDisplay = true;
    }
    if (!m_SkipDirectDisplay)
    {
        // Need to explicitly allocate display for DisplayImage use later:
        CHECK_RC(GpuTest::AllocDisplay());
    }

    // Init dgl stuff
    CHECK_RC(m_mglTestContext.SetProperties(
        GetTestConfiguration()
        ,false      // double buffer
        ,GetBoundGpuDevice()
        ,DisplayMgr::RequireNullDisplay // The test manages the display manually
        ,0           //don't override color format
        ,false       //we explicitly enforce FBOs depending on the testmode.
        ,false       //don't render to sysmem
        ,0));        //do not use layered FBO
    if (!m_mglTestContext.IsSupported())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(m_mglTestContext.Setup());

    UINT32 gpuIndex = GetBoundGpuSubdevice()->GetGpuInst();
#else
    UINT32 gpuIndex = GetTestConfiguration()->GetGpuIndex();
#endif
    GetFpContext()->Rand.SeedRandom(GetTestConfiguration()->Seed());
    debugReport::EnableErrorLogger(m_EnableErrorLogger);
    vector<string> extensionNames =
    {
        "VK_EXT_blend_operation_advanced"   //Core VkStress
       ,"VK_EXT_extended_dynamic_state"     // depth/stencil ops in dynamic state
    };
    CHECK_VK_TO_RC(m_VulkanInst.InitVulkan(enableVkDebug
                                           ,gpuIndex
                                           ,extensionNames
                                           ,false // don't initialize the device yet
                                           ,m_EnableValidation
                                           ,m_PrintExtensions));
    m_pVulkanDev = m_VulkanInst.GetVulkanDevice();
    MASSERT(m_pVulkanDev);

    m_NumSMs = m_pVulkanDev->GetPhysicalDevice()->GetSMBuiltinsPropertiesLW().shaderSMCount;
    if ((m_NumSMs == 0) && m_UseCompute)
    {
        Printf(Tee::PriError, "NumSMs is 0\n");
        return RC::SOFTWARE_ERROR;
    }
#ifdef VULKAN_STANDALONE
    GetBoundGpuSubdevice()->SetDeviceId(
        m_pVulkanDev->GetPhysicalDevice()->GetVendorID(),
        m_pVulkanDev->GetPhysicalDevice()->GetDeviceID());
    if (m_MaxPower)
    {
        CHECK_RC(ApplyMaxPowerSettings());
    }
#else
    MASSERT(m_NumSMs == GetBoundGpuSubdevice()->ShaderCount());
#endif

    if (m_NumActiveGPCs)
    {
        if (m_PulseMode == NO_PULSE)
        {
            // For an easier UI, non-zero NumActiveGPCs should activate the mode:
            m_PulseMode = TILE_MASK_GPC_SHUFFLE;
        }
        else if (m_PulseMode != TILE_MASK_GPC_SHUFFLE)
        {
            Printf(Tee::PriError, "PulseMode can't be set to %d when NumActiveGPCs=%d\n",
                m_PulseMode, m_NumActiveGPCs);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    if (m_UseCompute && IsPulsing())
    {
        Printf(Tee::PriError,
               "Can't enable Compute & PulseMode at the same time due to bug 2150982\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (m_UseTessellation && m_UseMeshlets)
    {
        Printf(Tee::PriError, "Tessellation and Meshlets can't be both enabled.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (static_cast<PulseMode>(m_PulseMode) == TILE_MASK && m_UseRaytracing)
    {
        Printf(Tee::PriError, "PulseMode=TILE_MASK and UseRaytracing can't be both enabled.\n");
    }
    if (m_SkipDrawChecks)
    {
        Printf(Tee::PriWarn, "%s: SkipDrawChecks is active!\n", GetName().c_str());
    }

    // This section of code is a bit touchy. We have a VulkanDevice that has not been initialized.
    // Returning before the device is initialized or the Vulkan resources are allocated  will
    // cause all kinds of problems in Cleanup(). So initialize the device and allocate the
    // resources even if we don't have all of the required extensions and then fail.
    rc = AddMeshletExtensions(extensionNames);
    rc = AddHTexExtensions(extensionNames);
    rc = AddComputeExtensions(extensionNames);

    // Initialize the Raytracer
    if (m_Rt.get())
    {
        m_Rt->SetVulkanDevice(m_pVulkanDev);
        m_Rt->SetWidth(m_RenderWidth);
        m_Rt->SetHeight(m_RenderHeight);
        m_Rt->SetSeed(GetTestConfiguration()->Seed());
        m_Rt->SetZeroFb(m_ZeroFb);
        m_Rt->SetDebug(m_Debug);
        m_Rt->SetNumTsQueries(MAX_NUM_TS_QUERIES);
        m_Rt->SetSwapChainMode(m_BufferCheckMs == BUFFERCHECK_MS_DISABLED ?
                               SwapChain::SINGLE_IMAGE_MODE :
                               SwapChain::MULTI_IMAGE_MODE);
        rc = m_Rt->AddExtensions(extensionNames);
    }

    UINT32 numGraphicsQueues = 0;
    UINT32 numTransferQueues = 0;
    UINT32 numComputeQueues  = 0;
    UINT32 surfCheckGfxQueueIdx = 0;
    UINT32 surfCheckTransQueueIdx = 0;

    CHECK_RC(VerifyQueueRequirements(&numGraphicsQueues, &numTransferQueues, &numComputeQueues,
                                     &surfCheckGfxQueueIdx, &surfCheckTransQueueIdx));

    // Raytracing will use the last compute/graphics queues.
    if (m_Rt.get())
    {
        // Add additional queues for raytracing (VkStressRay)
        m_Rt->SetComputeQueueIdx(numComputeQueues-1);
        m_Rt->SetGraphicsQueueIdx(numGraphicsQueues-1);
    }

    // Add this Ignore messages before initialization because some are generated during
    // initialization
    AddDbgMessagesToIgnore();

    CHECK_VK_TO_RC(m_pVulkanDev->Initialize(extensionNames,
                                            m_PrintExtensions,
                                            numGraphicsQueues,
                                            numTransferQueues,
                                            numComputeQueues));

#ifdef VULKAN_STANDALONE
    if (m_EnableDebugMarkers && (
            (m_pVulkanDev->DebugMarkerSetObjectTagEXT == nullptr) ||
            (m_pVulkanDev->DebugMarkerSetObjectNameEXT == nullptr) ||
            (m_pVulkanDev->CmdDebugMarkerBeginEXT == nullptr) ||
            (m_pVulkanDev->CmdDebugMarkerEndEXT == nullptr) ||
            (m_pVulkanDev->CmdDebugMarkerInsertEXT == nullptr) ) )
    {
        m_EnableDebugMarkers = false;
        Printf(Tee::PriWarn, "Unable to enable debug markers.\n");
    }
#endif

    CHECK_RC(ReadSm2Gpc());

    CHECK_RC(AllocateVulkanDeviceResources());

    CHECK_VK_TO_RC(m_CmdPool->InitCmdPool(
        m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx(), 0));
    CHECK_VK_TO_RC(m_InitCmdBuffer->AllocateCmdBuffer(m_CmdPool.get()));

    // The CheckSurface routine runs in a detached thread, so we need to have separate queues for
    // proper synchronization when BufferCheckMs is enabled. When BufferCheckMs is disabled we
    // only call this thread at the end of the test so we can use the same queues as the main
    // thread.
    CHECK_VK_TO_RC(m_CheckSurfaceCmdPool->InitCmdPool(
        m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx(), surfCheckGfxQueueIdx));
    CHECK_VK_TO_RC(m_CheckSurfaceCmdBuffer->AllocateCmdBuffer(m_CheckSurfaceCmdPool.get()));

    CHECK_VK_TO_RC(m_CheckSurfaceTransPool->InitCmdPool(
        m_pVulkanDev->GetPhysicalDevice()->GetTransferQueueFamilyIdx(), surfCheckTransQueueIdx));
    CHECK_VK_TO_RC(m_CheckSurfaceTransBuffer->AllocateCmdBuffer(m_CheckSurfaceTransPool.get()));

#ifdef VULKAN_STANDALONE_KHR
    if (m_DetachedPresentMs == 0)
    {
        m_PresentCmdBuf.resize(m_SwapChainAcquireSems.size());
        for (VulkanCmdBuffer& cmdBuf : m_PresentCmdBuf)
        {
            cmdBuf = VulkanCmdBuffer(m_pVulkanDev);
            CHECK_VK_TO_RC(cmdBuf.AllocateCmdBuffer(m_CmdPool.get()));
        }
    }
#endif

    CHECK_RC(SetupDepthBuffer());
    if (m_SampleCount > 1)
    {
        CHECK_RC(SetupMsaaImage());
    }
    CHECK_RC(SetupBuffers());
    UINT32 computeWorkIdx = 0;
    for (auto &cw : m_ComputeWorks)
    {
        CHECK_RC(SetupComputeBuffers(&cw, computeWorkIdx++));
    }

    CHECK_RC(SetupSamplers());
    CHECK_RC(SetupTextures());
    CHECK_RC(CreateRenderPass());
    CHECK_RC(SetupDescriptors());
    for (auto &cw : m_ComputeWorks)
    {
        CHECK_RC(SetupComputeDescriptors(&cw));
    }

    DEFER
    {
        CleanupAfterSetup();
    };

    CHECK_RC(SetupShaders());
    CHECK_RC(SetupFrameBuffer());
    CHECK_RC(SetupDrawCmdBuffers());
    CHECK_RC(BuildPreDrawCmdBuffer());
    CHECK_RC(CreateGraphicsPipelines());
    CHECK_RC(CreateComputePipeline());
    CHECK_RC(CreateGoldenSurfaces());
    CHECK_RC(SetupQueryObjects());
    CHECK_RC(SetupWorkController());

    if (m_Rt.get())
    {
        CHECK_RC(m_Rt->SetupRaytracing(m_GoldenSurfaces,
                                       m_CheckSurfaceCmdBuffer.get(),
                                       m_CheckSurfaceTransBuffer.get()));
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Lwrrently we are only using the timestamp queries. We have two query pools:
// one for graphics and the other for compute. Each command buffer submission
// results in "start" and "stop" timestamps being added to the query pool
// results, which is a cirlwlar buffer that holds up to MAX_NUM_TIMESTAMPS.
// Even Entries will be a "start" timestamp, odd entries will be a "stop" timestamp,
RC VkStress::SetupQueryObjects()
{
    RC rc;
    m_GfxTSQueries.reserve(MAX_NUM_TS_QUERIES);

    // We had to switch over to an array of Query objects because each queue submit would reset
    // all of the query entries from the cmd pool when validation is enabled.
    for (UINT32 i = 0; i < MAX_NUM_TS_QUERIES; i++)
    {
        m_GfxTSQueries.emplace_back(make_unique<VulkanQuery>(m_pVulkanDev));
        CHECK_RC(m_GfxTSQueries.back()->Init(VK_QUERY_TYPE_TIMESTAMP, MAX_NUM_TIMESTAMPS, 0));
        CHECK_RC(m_GfxTSQueries.back()->CmdReset(m_InitCmdBuffer.get(), 0, MAX_NUM_TIMESTAMPS));
    }
    for (auto &cw : m_ComputeWorks)
    {
        cw.m_ComputeTSQueries.reserve(MAX_NUM_TS_QUERIES);
        for (UINT32 i = 0; i < MAX_NUM_TS_QUERIES; i++)
        {
            cw.m_ComputeTSQueries.emplace_back(make_unique<VulkanQuery>(m_pVulkanDev));
            CHECK_RC(cw.m_ComputeTSQueries.back()->Init(VK_QUERY_TYPE_TIMESTAMP,
                                                        MAX_NUM_TIMESTAMPS, 0));
            CHECK_RC(cw.m_ComputeTSQueries.back()->CmdReset(m_InitCmdBuffer.get(),
                                                            0,
                                                            MAX_NUM_TIMESTAMPS));
        }
    }
    //Setup other query objects here.
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::SetDisplayImage(UINT32 imageIndex)
{
    RC rc;
    if (m_Rt.get())
    {
        m_Rt->SetLwrrentSwapChainIdx(imageIndex);
    }

#ifndef VULKAN_STANDALONE
    if (!m_SkipDirectDisplay)
    {
        GpuUtility::DisplayImageDescription desc;
        if (m_DisplayRaytracing && m_Rt.get())
        {
            CHECK_RC(m_Rt->GetSwapChain()->GetSwapChainImage(imageIndex)->
                PopulateDisplayImageDescription(GetBoundGpuDevice(), &desc));
            CHECK_RC(GetDisplayMgrTestContext()->DisplayImage(&desc,
                DisplayMgr::DONT_WAIT_FOR_UPDATE, 1));
        }
        if (m_SampleCount > 1)
        {
            // The resolve image stays blank, use the msaa image instead:
            CHECK_RC(m_MsaaImages[imageIndex]->PopulateDisplayImageDescription(
                GetBoundGpuDevice(), &desc));
        }
        else
        {
            CHECK_RC(m_SwapChain->GetSwapChainImage(imageIndex)->
                PopulateDisplayImageDescription(GetBoundGpuDevice(), &desc));
        }
        CHECK_RC(GetDisplayMgrTestContext()->DisplayImage(&desc,
            DisplayMgr::DONT_WAIT_FOR_UPDATE));
    }
#endif
    return rc;
}
//-------------------------------------------------------------------------------------------------
RC VkStress::Run()
{
    StickyRC rc;
    m_RunTimeCallwlator.StartTimeRecording();
    DEFER
    {
        m_RunTimeCallwlator.StopTimeRecording();
    };

    // Reset for successive calls to Run()
    m_NumFrames = 0;
    m_CorruptionDone = false;
    m_DrawJobIdx = 0;
    if (!m_AsyncCompute)
    {
        for (auto &cw : m_ComputeWorks)
        {
            cw.m_ComputeJobIdx = 0;
        }
    }
    m_RememberedFps = 0;

    const UINT64 ticksPerMs = Xp::QueryPerformanceFrequency() / 1000ULL;
    const UINT32 runtimeMs = m_RuntimeMs ? m_RuntimeMs : m_LoopMs;
    const bool bIgnoreFrameCheck = (m_Debug & DBG_IGNORE_FRAME_CHECK) != 0;
    m_ProgressHandler.ProgressHandlerInit(runtimeMs * ticksPerMs,
                                          GetTestConfiguration()->Loops(),
                                          bIgnoreFrameCheck);
    CHECK_RC(PrintProgressInit(m_ProgressHandler.NumSteps()));

    // Don't use CHECK_RC() here because if CorruptionStep is set we will fail before clearing the
    // error.
    rc = InnerRun();

    // Our progress logic needs to ensure that we never declare done in the middle of a quad-frame
    // unless we have an error.
    if (rc == RC::OK && !bIgnoreFrameCheck)
    {
        MASSERT((m_NumFrames % 4) == 0);
    }

    // Golden destructor will fail if you don't call this API.
    m_pGolden->SetDeferredRcWasRead(true);

    if (m_CorruptionStep != CORRUPTION_STEP_DISABLED)
    {
        if (m_CorruptionDone && (rc == RC::GPU_STRESS_TEST_FAILED))
        {
            rc.Clear();
        }
        else
        {
            rc = RC::UNEXPECTED_TEST_COVERAGE;
            Printf(Tee::PriError, "CorruptionStep was not successfully performed!\n");
        }
    }
    return rc;
}

//------------------------------------------------------------------------------------------------
// Print out a rectangular region of the given surface using the specified bpp format.
void VkStress::DumpSurface
(
    char * szTitle
    ,UINT32 top
    ,UINT32 left
    ,UINT32 width
    ,UINT32 height
    ,UINT32 pitch
    ,UINT32 bpp      //bytes per pixel
    ,void *pSurface
)
{
    UINT32 cnt = 0;
    UINT32 offset = 0;
    UINT08 *pSrc = static_cast<UINT08*>(pSurface);
    Printf(Tee::PriNormal
           ,"Dumping %s surface on frame:%d region (t, l, w, h):%d, %d, %d, %d\n"
           ,szTitle, m_NumFrames, top, left, width, height);

    for (UINT32 h = top; h < top+height; h++)
    {
        // callwlate the byte offset for left row.
        offset = (h * pitch) + (left * bpp);
        for (UINT32 w = left; w < left+width; w++, offset+=bpp)
        {
            switch (bpp)
            {
                case 4:
                    Printf(Tee::PriNormal, "0x%08x ", *reinterpret_cast<UINT32*>(pSrc + offset));
                    break;
                case 2:
                    Printf(Tee::PriNormal, "0x%04x ", *reinterpret_cast<UINT16*>(pSrc + offset));
                    break;
                case 1:
                    Printf(Tee::PriNormal, "0x%02x ", *static_cast<UINT08*>(pSrc + offset));
                    break;
                default:
                    break;
            }
            if (++cnt % 16 == 0)
            {
                Printf(Tee::PriNormal, "\n");
            }
        }
    }
    Printf(Tee::PriNormal, "\n");
}

//------------------------------------------------------------------------------------------------
// Signal the SurfaceCheck thread to check a specific VkImage surface.
void VkStress::SignalSurfaceCheck(UINT32 swapChainIdx, UINT32 numFrames)
{
    m_CheckSurfaceQueue.Push({ swapChainIdx, numFrames });
    // If we assert here something terrible went wrong with the CheckSufaceThread.
    MASSERT(m_CheckSurfaceQueue.Size() <= m_SwapChain->GetNumImages());
    Printf(m_BufCkPrintPri,
           "SignalSurfaceCheck(): setting CheckSurfaceEvent(swapChainIdx:%d, numFrames:%d),"
           " next check at %lldms.\n", swapChainIdx, numFrames, m_NextBufferCheckMs);

    Tasker::SetEvent(m_pCheckSurfaceEvent);

    return;
}

//------------------------------------------------------------------------------------------------
// Create a detached thread that uses a transfer queue and command buffer to check the swapchain
// image for completeness.
// Note the swapchain image being checked is not the active image being drawn to so this thread
// can run asychronously with the main thread because we triple buffer the swapchain surfaces and
// guard them using a mutex to prevent the CheckSurfaceThread and the main thread from
// simultaneously accessing the same surface.
// The main thread will do the following:
// - acquire the current swapchain's mutex before allowing the GPU to use it, then once the
//   swapchain is no longer being used by the GPU release the mutex
// - signal this thread to check the swapchain image that was just released.
// Note: The BufferCheckMs mechanism did not work in Windows because the swapchains are rotated
//       too quickly and we did not have the resolution we now have. It may now work in Windows but
//       I haven't confirmed that yet. So safeguards are put inplace to prevent that condition.
//       Also, we disable BufferCheckMs with pulsing because we do not always send quadframes. We
//       can only do this check if m_NumFrames % 4 == 0.
//       When BufferCheckMs is disabled this thread will only be signaled at the end of the test.
// Note: Once we have checked this out on Windows and are very confident that it works in all
//       corner cases we can remove the print statements. So far I have tested this algorithm on
//       the following range of values:
//       FramesPerSubmit: 4 - 10000 and BufferCheckMs: 30ms - 500ms
void VkStress::CheckSurfaceThread(void * vkStress)
{
    Tasker::DetachThread detach;

    VkStress * pVkStress = static_cast<VkStress*>(vkStress);
    Tee::Priority printPri = pVkStress->m_BufCkPrintPri;
    dglThreadAttach();

    DEFER
    {
        dglThreadDetach();
    };

    for (;;)
    {
        // Wait for the event to trigger
        Printf(printPri, "\tCheckSurfaceThread():Waiting on CheckSurfaceEvent...\n");
        Tasker::WaitOnEvent(pVkStress->m_pCheckSurfaceEvent);

        // If there was a surface check requested, do it
        CheckSurfaceParams params;
        while (pVkStress->m_CheckSurfaceQueue.GetParams(&params))
        {
            UINT32 idx = params.swapchainIdx;
            Printf(printPri, "\tCheckSurfaceThread():AcquireMutex(swapChainIdx:%d)\n", idx);
            Tasker::MutexHolder mh(pVkStress->m_SwapChain->GetSwapChainImage(idx)->GetMutex());
            Printf(printPri, "\tCheckSurfaceThread(): checking surface:%d\n", idx);
            {
                char buff[100];
                snprintf(buff, sizeof(buff), "\tCheckSurfaceThread(): CheckSurface(%d) took", idx);
                Utility::StopWatch myStopwatch(buff, printPri);
                pVkStress->InnerCheckErrors(CHECK_ALL_SURFACES, idx, params.numFrames);
            }
            Printf(printPri, "\tCheckSurfaceThread(): ReleaseMutex(swapChainIdx:%d)\n", idx);
        }
        if (!pVkStress->m_RunCheckSurfaceThread)
        {
            break;
        }
    }
    Printf(printPri, "\tCheckSurfaceThread(): exiting...\n");
}

UINT32 VkStress::GetBadPixels
(
    const char * pTitle,
    unique_ptr<VulkanGoldenSurfaces> &gs,
    UINT32 surfIdx,
    UINT32 maxErrors,
    UINT32 expectedValue,
    UINT32 *pSrc,
    UINT32 pixelMask // set to ~0 for RGBA, 0xffffff for depth, & 0xff000000 for stencil
)
{
    UINT32 offset       = 0;
    UINT32 badPixels    = 0;
    UINT32 bpp          = ColorUtils::PixelBytes(gs->Format(surfIdx));

    for (UINT32 h = 0; h < gs->Height(surfIdx); h++)
    {
        offset = h * (gs->Pitch(surfIdx) / bpp);
        for (UINT32 w = 0; w < gs->Width(surfIdx); w++, offset++)
        {
            UINT32 pixel = pSrc[offset];
            if ((pixel & pixelMask) != expectedValue)
            {
                if (badPixels < maxErrors)
                {
                    VerbosePrintf("%s miscompare at x:%d y:%d expected:0x%x found:0x%x\n",
                           pTitle, w, h, expectedValue, pixel & pixelMask);
                }
                else if (badPixels == maxErrors)
                {
                    VerbosePrintf("Too many bad pixels, canceling prints\n");
                }
                badPixels++;
            }
        }
    }
    return badPixels;
}

string VkStress::GetUniqueFilename(const char *pTestName, const char * pExt)
{
    string filename = LwDiagUtils::StripFilename(CommandLine::LogFileName().c_str());
    string::size_type pos = filename.find_last_of("/\\");
    if ((pos != string::npos) && (pos != (filename.size()-1)))
    {
        filename.push_back(filename[pos]); // append the proper type of separator
        filename += Utility::GetTimestampedFilename(pTestName, pExt);
    }
    else
    {
        filename = Utility::GetTimestampedFilename(pTestName, pExt);
    }
    return filename;
}

//------------------------------------------------------------------------------------------------
// This routine will do the following:
// m_DumpPng controls which surfaces (color, depth, & stencil, raytracing) will be written to a
// png file.
// If m_DumpPngOnError is true and the surface check fails, the failing surface will be written
// to a png file.
// flags will specify what surfaces should be checked (see SurfaceCheck enum).
// The color surface will be compared against the background color to insure every pixel matches.
// The raytracing surface will be compared against the Rt background colore to insure every pixel
// matches.
// The depth & stencil surfaces will be compared against zero.
// If any surface fails, try to identify which pixel { .xy } coordinate has miscompared.
// Preconditions:
// - The number of frames rendered to this swapchain image is a multiple of 4 for color & depth
//   and is a multiple of 2 for raytracing.
// - This function is running in a detached thread.
// - All of the GPU work for this swapchain image has already been drained by some other thread.
// - The color & raytracing surfaces are in the R8G8B8A8 color format.
// - The depthStencil surface is in the VK_FORMAT_D24_UNORM_S8_UINT format.
RC VkStress::CheckSurface(UINT32 flags, UINT32 swapchainIdx, UINT32 numFrames)
{
    RC rc;

    if (m_SkipDrawChecks)
    {
        return RC::OK;
    }

    if ((numFrames % 4) && !(m_Debug & DBG_IGNORE_FRAME_CHECK))
    {
        Printf(Tee::PriError,
               "Attempting to check surfaces on the wrong frame. numFrames=%d\n", numFrames);
        m_CheckSurfaceRC = RC::SOFTWARE_ERROR;
        return RC::SOFTWARE_ERROR;
    }

    const int surfIdx = 0;
    auto &gs = m_GoldenSurfaces[swapchainIdx];

    UINT32 badPixels    = 0;
    UINT32 maxErrors    = 10;
    UINT32 *pSrc        = nullptr;
    UINT32 offset       = 0;
    gs->Ilwalidate();

    if ((flags & CHECK_COLOR_SURFACE) || (m_DumpPng & CHECK_COLOR_SURFACE))
    {
        pSrc = static_cast<UINT32*>(gs->GetCachedAddress(
                         0                      //int surfNum,
                        ,Goldelwalues::opCpuDma //not used
                        ,0                      // not used
                        ,nullptr));             //vector<UINT08> *surfDumpBuffer
    }

    // Checking any surface on a frame that is not a modulo of 4 will always fail.
    if ((flags & CHECK_COLOR_SURFACE) && (numFrames % 4 == 0))
    {
        UINT32 clrColor = m_ClearColor[swapchainIdx];
        badPixels = GetBadPixels("Pixel", gs, surfIdx, maxErrors, clrColor, pSrc, ~0);

        // When corruption is enabled we only corrupt a single pixel on a specific swapchain.
        // Verify that actually happened.
        if (m_CorruptionDone && m_CorruptionSwapchainIdx == swapchainIdx && badPixels != 1)
        {
            Printf(Tee::PriError, "Injected corruption at swapchain image %d %s detected.\n",
                   m_CorruptionSwapchainIdx, badPixels ? "was" : "was not");
            Printf(Tee::PriError, "Expected 1 bad pixel but detected %d bad pixels\n", badPixels);
            m_CheckSurfaceRC = RC::UNEXPECTED_TEST_COVERAGE;
        }
        else if (badPixels)
        {
            m_Miscompares[swapchainIdx].emplace_back(CHECK_COLOR_SURFACE, badPixels);
        }
    }
    if ((badPixels && m_DumpPngOnError) || (m_DumpPng & CHECK_COLOR_SURFACE))
    {
        CHECK_RC(ImageFile::WritePng(
            m_GenUniqueFilenames ?
                GetUniqueFilename("vkstressC", "png").c_str() :
                Utility::StrPrintf("vkstressC%4.4d.png", m_NumFrames).c_str()
            ,VkUtil::ColorUtilsFormat(m_SwapChain->GetSwapChainImage(swapchainIdx)->GetFormat())
            ,pSrc
            ,gs->Width(surfIdx)
            ,gs->Height(surfIdx)
            ,gs->Pitch(surfIdx)
            ,false //AlphaToRgb
            ,false));   //YIlwerted
    }
    badPixels = 0;
    // Now check the depth/stencil surface
    // Note: The depth/stencil values are not compressed and the buffer is segmented into 2
    //       separate sections, the top 3/4 is allocated for depth and the bottom 1/4 is for
    //       stencil. In addition the depth component will consume 32 bits with only 24 bits
    //       being valid, and the stencil component will consume 8 bits.
    // Note: If the sample count > 1 we can't copy the depthStencil image to a VkBuffer object.
    //       It's not clear on how to resolve the multi-sampled depthStencil surface right now. So
    //       I will have to get back to this when I have all the details LEB 12/8/2017
    if (m_SampleCount == 1)
    {
        if ((flags & CHECK_DEPTH_STENCIL_SURFACE) ||
            (m_DumpPng & CHECK_DEPTH_STENCIL_SURFACE))
        {
            pSrc = static_cast<UINT32*>(gs->GetCachedAddress(
                             1                      //int surfNum,
                            ,Goldelwalues::opCpuDma //not used
                            ,0                      // not used
                            ,nullptr));             //vector<UINT08> *surfDumpBuffer
        }
        if ((flags & CHECK_DEPTH_SURFACE) && (numFrames % 4 == 0))
        {
            const UINT32 depthValue = 0;
            const UINT32 depthMask = 0x00ffffff;
            //To debug always dump the center of the display. This is where things change the most.
            //DumpSurface("Depth", 378, 500, 20, 10, gs->Pitch(surfIdx), bpp, pSrc);
            badPixels = GetBadPixels("Depth", gs, surfIdx, maxErrors, depthValue, pSrc, depthMask);
            if (badPixels)
            {
                m_Miscompares[swapchainIdx].emplace_back(CHECK_DEPTH_SURFACE, badPixels);
            }
        }
        if ((badPixels && m_DumpPngOnError) || (m_DumpPng & CHECK_DEPTH_SURFACE))
        {
            CHECK_RC(ImageFile::WritePng(
                m_GenUniqueFilenames ?
                    GetUniqueFilename("vkstressD", "png").c_str() :
                    Utility::StrPrintf("vkstressD%4.4d.png", m_NumFrames).c_str()
                ,ColorUtils::X8Z24
                ,pSrc
                ,gs->Width(surfIdx)
                ,gs->Height(surfIdx)
                ,gs->Pitch(surfIdx)
                ,false //AlphaToRgb
                ,false));   //YIlwerted
        }

        // Prepare to dump png and/or check stencil
        badPixels = 0;
        UINT08 *pStencil = reinterpret_cast<UINT08*>(pSrc) +
            (gs->Width(surfIdx) * gs->Height(surfIdx) * 4);
        if ((flags & CHECK_STENCIL_SURFACE) && (numFrames % 4 == 0))
        {
            //now check stencil (note: pitch = width in this case because stencil is only 1 byte)
            const UINT08 stencilValue = 0;
            //DumpSurface("Stencil", 378, 500, 20, 10, gs->Width(surfIdx), 1, pStencil);
            offset = 0;
            for (UINT32 h = 0; h < gs->Height(surfIdx); h++)
            {
                offset = h * gs->Width(surfIdx);
                for (UINT32 w = 0; w < gs->Width(surfIdx); w++, offset++)
                {
                    if (pStencil[offset] != stencilValue)
                    {
                        if (badPixels < maxErrors)
                        {
                            VerbosePrintf(
                                "Stencil miscompare at x:%d y:%d expected:0x%x found:0x%x\n",
                                w, h, stencilValue, pStencil[offset]);
                        }
                        else if (badPixels == maxErrors)
                        {
                            VerbosePrintf("Too many bad stencil values, canceling prints\n");
                        }
                        badPixels++;
                    }
                }
            }
            if (badPixels)
            {
                m_Miscompares[swapchainIdx].emplace_back(CHECK_STENCIL_SURFACE, badPixels);
            }
        }
        if ((badPixels && m_DumpPngOnError) || (m_DumpPng & CHECK_STENCIL_SURFACE))
        {
            CHECK_RC(ImageFile::WritePng(
                m_GenUniqueFilenames ?
                    GetUniqueFilename("vkstressS", "png").c_str() :
                    Utility::StrPrintf("vkstressS%4.4d.png", m_NumFrames).c_str()
                ,ColorUtils::S8
                ,pStencil
                ,gs->Width(surfIdx)
                ,gs->Height(surfIdx)
                ,gs->Width(surfIdx)
                ,false //AlphaToRgb
                ,false));   //YIlwerted
        }
    }
    if (m_Rt.get())
    {
        // If XOR logic is disabled we will always fail the check surface so just return early.
        if (m_Rt->GetDisableXORLogic())
        {
            return rc;
        }
        badPixels = 0;
        UINT32 idx = m_Rt->GetGoldenSurfaceIdx() + swapchainIdx;
        if ((flags & CHECK_RT_SURFACE) && !(numFrames % 2))
        {
            // Raytracing surface is the last golden surface
            auto &rtgs = m_GoldenSurfaces[idx];
            rtgs->Ilwalidate();
            UINT32 *pSrc = static_cast<UINT32*>(rtgs->GetCachedAddress(
                             0                      //int surfNum,
                            ,Goldelwalues::opCpuDma //not used
                            ,0                      // not used
                            ,nullptr));             //vector<UINT08> *surfDumpBuffer
            badPixels = GetBadPixels("RtPixel", rtgs, surfIdx, maxErrors, 0x00, pSrc, ~0);
            if (badPixels)
            {
                // Add the bad pixels to the list.
                m_Miscompares[RT_IMAGE_IDX].emplace_back(CHECK_RT_SURFACE, badPixels);
            }
        }
        if ((badPixels && m_DumpPngOnError) || (m_DumpPng & CHECK_RT_SURFACE))
        {
            rc = m_Rt->DumpImage(numFrames,
                                 m_GoldenSurfaces[idx].get(),
                                 m_GenUniqueFilenames,
                                 LwDiagUtils::StripFilename(CommandLine::LogFileName().c_str()));
        }
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
// -Detach our own thread for better accuracy when pulsing
// -Create a detached thread for surface checking
// -Execute the correct run function to render the geometry.
// -Destroy the detached thread
// -Return the final RC value.
RC VkStress::InnerRun()
{
    StickyRC rc;
    { // consequence of using DEFER is that all code except the StickyRC or RC must be at a lower
      // scope if you are trying to update the rc variable.
    Tasker::DetachThread detach;

    //Create a detached thread to perform the surface checks in parallel with the rendering.
    m_pCheckSurfaceEvent = Tasker::AllocEvent("CheckSurfaceEvent", false);
    const auto tid = Tasker::CreateThread(CheckSurfaceThread,
                                          this,
                                          Tasker::MIN_STACK_SIZE,
                                          "CheckSurfaceThread");

    m_pThreadExitEvent = Tasker::AllocEvent("ThreadExitEvent", true);

#ifndef VULKAN_STANDALONE
    Tasker::ThreadID fmaxTid = Tasker::NULL_THREAD;
    if (m_PulseMode == FMAX || m_PulseMode == TILE_MASK)
    {
        m_FMaxDutyPctSamples = 0;
        m_FMaxDutyPctSum = 0;
        fmaxTid = Tasker::CreateThread(FMaxThread,
            this,
            Tasker::MIN_STACK_SIZE,
            "FMaxThread");
    }
#endif
#ifdef VULKAN_STANDALONE_KHR
    Tasker::ThreadID presentTid = Tasker::NULL_THREAD;
    if (m_DetachedPresentMs > 0)
    {
        presentTid = Tasker::CreateThread(PresentThread,
                                          this,
                                          Tasker::MIN_STACK_SIZE,
                                          "PresentThread");
    }
#endif
    DEFER
    {
        //Signal the thread to exit & wait until it ends
        m_RunCheckSurfaceThread = false;
        Tasker::SetEvent(m_pCheckSurfaceEvent);
        Tasker::Join(tid);
        Tasker::FreeEvent(m_pCheckSurfaceEvent);
        rc = m_CheckSurfaceRC;

        Tasker::SetEvent(m_pThreadExitEvent);

#ifndef VULKAN_STANDALONE
        if (fmaxTid != Tasker::NULL_THREAD)
        {
            Tasker::Join(fmaxTid);
            fmaxTid = Tasker::NULL_THREAD;
            if (m_FMaxDutyPctSamples != 0)
            {
                const FLOAT32 averageDutyPct = (m_FMaxDutyPctSum / m_FMaxDutyPctSamples) / 100.0f;
                VerbosePrintf("FMaxController: average dutyCycle = %.1f%%\n", averageDutyPct);
                if (averageDutyPct > m_FMaxAverageDutyPctMax)
                {
                    Printf(Tee::PriError,
                        "Average Duty Pct = %.2f larger than allowed %.2f\n",
                        averageDutyPct, m_FMaxAverageDutyPctMax);
                    rc = RC::UNEXPECTED_TEST_PERFORMANCE;
                }
                if (averageDutyPct < m_FMaxAverageDutyPctMin)
                {
                    Printf(Tee::PriError,
                        "Average Duty Pct = %.2f smaller than allowed %.2f\n",
                        averageDutyPct, m_FMaxAverageDutyPctMin);
                    rc = RC::UNEXPECTED_TEST_PERFORMANCE;
                }
            }
        }
#endif
#ifdef VULKAN_STANDALONE_KHR
        if (presentTid != Tasker::NULL_THREAD)
        {
            Tasker::Join(presentTid);
            presentTid = Tasker::NULL_THREAD;
        }
#endif

        Tasker::FreeEvent(m_pThreadExitEvent);
        m_pThreadExitEvent = nullptr;
    };

#ifndef VULKAN_STANDALONE
    auto& regs = GetBoundGpuSubdevice()->Regs();
    unique_ptr<ScopedRegister> ptimerReg;
    if (regs.IsSupported(MODS_PTIMER_GR_TICK_FREQ) && Xp::HasClientSideResman())
    {
        ptimerReg = make_unique<ScopedRegister>(GetBoundGpuSubdevice(),
            regs.LookupAddress(MODS_PTIMER_GR_TICK_FREQ));
        regs.Write32(MODS_PTIMER_GR_TICK_FREQ_SELECT_MAX);
    }
#endif

    CHECK_RC(SubmitPreDrawCmdBufferAndPresent());
    CHECK_RC(RunFrames());
    if (m_UseCompute && m_AsyncCompute)
    {
        for (UINT32 workIdx = 0; workIdx < m_NumComputeQueues; workIdx++)
        {
            CHECK_RC(m_ComputeWorks[workIdx].m_ComputeRC);
        }
    }
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Wait for one or more fences to be signaled before returning. If usePolling = true then call
// Yield() between each iteration of checking the fence status.
// Note: GetFenceStatus() doesn't appear to  return the correct state all of the time. I can
// visually see that the frames are still being drawn when GetFenceStatus() has already reported
// the fence is signaled. Need to debug this further. In the meantime WaitForFences() with a zero
// timeout condition works as expected. LEB
RC VkStress::WaitForFences
(
    UINT32          fenceCount
    ,const VkFence *pFences
    ,VkBool32       waitAll
    ,UINT64         timeoutNs
    ,bool           usePolling
)
{
    VkResult res = VK_SUCCESS;
    if (usePolling)
    {
        const UINT64 endTime = Xp::GetWallTimeNS() + timeoutNs;
        while ((res = m_pVulkanDev->WaitForFences(fenceCount, pFences, VK_TRUE, 0)) == VK_TIMEOUT)
        {
            if (Xp::GetWallTimeNS() > endTime)
            {
                break;
            }
            Tasker::Yield();
        }
    }
    else
    {
        res = m_pVulkanDev->WaitForFences(fenceCount, pFences, VK_TRUE, timeoutNs);
    }

    if (res == VK_SUCCESS)
    {
        res = m_pVulkanDev->ResetFences(fenceCount, pFences);
    }
    else
    {
        map<VkResult, string> fenceStatus;
        fenceStatus[VK_SUCCESS] = "signaled";
        fenceStatus[VK_NOT_READY] = "unsignaled";
        fenceStatus[VK_ERROR_DEVICE_LOST] = "lost device";
        for (UINT32 i = 0; i < fenceCount; i++)
        {
            VkResult fenceRes = m_pVulkanDev->GetFenceStatus(pFences[i]);
            Printf(Tee::PriNormal, "pFences[%d] = %s\n", i, fenceStatus[fenceRes].c_str());
        }
        Printf(Tee::PriNormal, "Last known WorkController state and timestamp queries:\n");
        m_WorkController->PrintDebugInfo();
        m_WorkController->PrintStats();
        DumpQueryResults();
    }
    return (VkUtil::VkErrorToRC(res));
}

void VkStress::ComputeThread(void *pComputeThreadArgs)
{
    ComputeThreadArgs *pArgs = static_cast<ComputeThreadArgs*>(pComputeThreadArgs);
    VkStress * pVkStress = static_cast<VkStress*>(pArgs->pVkStress);
    pVkStress->ComputeThread(pArgs->ComputeWorkIdx);
}

void VkStress::ComputeThread(UINT32 computeWorkIdx)
{
    if (!m_UseCompute)
    {
        MASSERT(!"ComputeThread started with UseCompute=false");
        return;
    }
    if (computeWorkIdx >= m_ComputeWorks.size())
    {
        MASSERT(!"ComputeThread started with m_ComputeWorks not filled");
        return;
    }

    Tasker::DetachThread detach;

    dglThreadAttach();

    DEFER
    {
        dglThreadDetach();
    };

    ComputeWork &cw = m_ComputeWorks[computeWorkIdx];

    cw.m_ComputeRC.Clear();
    cw.m_ComputeJobIdx = 0;
    cw.m_ComputeQueryIdx = 0;

    while ((cw.m_ComputeRC == RC::OK) &&
        (m_KeepRunning || !(m_ProgressHandler.Done() && m_WorkController->Done())))
    {
        const size_t lwrrComputeJobIdx = static_cast<size_t>(cw.m_ComputeJobIdx % m_NumComputeJobs);
        const size_t prevComputeJobIdx = static_cast<size_t>((cw.m_ComputeJobIdx - 1) % m_NumComputeJobs);
        cw.m_ComputeRC = BuildComputeCmdBuffer(&cw);

        VkSemaphore waitComputeSema = cw.m_ComputeJobIdx ?
            cw.m_ComputeSemaphores[prevComputeJobIdx] : static_cast<VkSemaphore>(VK_NULL_HANDLE);
        cw.m_ComputeRC = VkUtil::VkErrorToRC(cw.m_ComputeCmdBuffers[lwrrComputeJobIdx]->ExelwteCmdBuffer(
            waitComputeSema                             // waitSemaphore
            ,1                                          // numSignalSemaphores
            ,&cw.m_ComputeSemaphores[lwrrComputeJobIdx] // pSignalSemaphores
            ,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
            ,false                                      // don't wait on the fence
            ,cw.m_ComputeFences[lwrrComputeJobIdx]      // signal fence
            ,false));                                    // usePolling

        if (cw.m_ComputeJobIdx)
        {
            VkFence fence = cw.m_ComputeFences[prevComputeJobIdx];
            cw.m_ComputeRC = WaitForFences(1, &fence, VK_TRUE, m_pVulkanDev->GetTimeoutNs(), false);

            TuneComputeWorkload(computeWorkIdx);
        }
        cw.m_ComputeJobIdx++;
        cw.m_ComputeQueryIdx = (cw.m_ComputeQueryIdx + 1) % MAX_NUM_TS_QUERIES;
    }

    UINT32 usedSms = 0;
    for (UINT32 smIdx = 0; smIdx < m_NumSMs; smIdx++)
    {
        usedSms += (cw.m_pScgStats->smLoaded[smIdx]) ? 1 : 0;
    }
    if (usedSms < m_NumSMs)
    {
        cw.m_ComputeRC = RC::UNEXPECTED_TEST_PERFORMANCE;
        Printf(Tee::PriError,
            "Not all SMs were used for compute queue %d - %d/%d\n",
            computeWorkIdx, usedSms, m_NumSMs);
    }
}

RC VkStress::RunFrames()
{
    RC rc;

    vector<Tasker::ThreadID> computeThreadIDs;
    vector<ComputeThreadArgs> computeThreadArgs; // Need to be in scope until "Join"
    if (m_UseCompute && m_AsyncCompute)
    {
        computeThreadIDs.resize(m_NumComputeQueues);
        computeThreadArgs.resize(m_NumComputeQueues);
        for (UINT32 workIdx = 0; workIdx < m_NumComputeQueues; workIdx++)
        {
            ComputeThreadArgs *pCTArgs = &computeThreadArgs[workIdx];
            pCTArgs->pVkStress = this;
            pCTArgs->ComputeWorkIdx = workIdx;
            computeThreadIDs[workIdx] = Tasker::CreateThread(ComputeThread,
                pCTArgs,
                Tasker::MIN_STACK_SIZE,
                Utility::StrPrintf("ComputeThread%d", workIdx).c_str());
        }
    }

    // A queue to identify the SwapChains in use by the GPU. This queue will have at most 2
    // entries because we only have 2 drawJobs in process at any given point in time. Worst
    // case scenerio would be that every drawJob uses a different swapchain.
    // This is for internal tracking only. The GPU doesn't pull from this queue.
    std::queue<BufferCheckInfo> myGpuQueue;

    DEFER
    {
        if (rc != RC::OK)
        {
            // release the swapchain mutexes so the checkSurface thread can rejoin
            while (!myGpuQueue.empty())
            {
                m_SwapChain->GetSwapChainImage(myGpuQueue.front().swapchainIdx)->ReleaseMutex();
                myGpuQueue.pop();
            }
        }
        if (m_UseCompute && m_AsyncCompute)
        {
            for (auto threadId : computeThreadIDs)
            {
                Tasker::Join(threadId);
            }
        }
    };
    auto ReleaseMutexAndSignalSurfaceCheck = [&](UINT32 numFrames)
    {
        Printf(m_BufCkPrintPri, "RunFrames() ReleaseMutex(swapChainIdx:%d, drawJob:%lld)\n",
               myGpuQueue.front().swapchainIdx, myGpuQueue.front().lastDrawJobIdx);
        m_SwapChain->GetSwapChainImage(myGpuQueue.front().swapchainIdx)->ReleaseMutex();
        SignalSurfaceCheck(myGpuQueue.front().swapchainIdx, numFrames);
        myGpuQueue.pop();
    };
    auto AcquireMutexAndSaveSwapchainIdx = [&]()
    {
        Printf(m_BufCkPrintPri, "RunFrames() AcquireMutex(swapChainIdx:%d, drawJob:%lld)\n",
               m_LwrrentSwapChainIdx, m_DrawJobIdx);
        m_SwapChain->GetSwapChainImage(m_LwrrentSwapChainIdx)->AcquireMutex();
        myGpuQueue.push({ m_LwrrentSwapChainIdx, m_DrawJobIdx });
    };
    m_FramesPerSubmit = m_WorkController->GetNumFrames();

    // Lock the resource so the CheckSurface thread can't get to it until the GPU is done with it.
    AcquireMutexAndSaveSwapchainIdx();
    while (m_KeepRunning || !(m_ProgressHandler.Done() && m_WorkController->Done()))
    {
        CHECK_RC(BuildDrawJob());

        // If we are switching swapchains, acquire the resource. Otherwise update the queue with
        // the current drawJob so we can determine when to release the swapchain.
        if (myGpuQueue.front().swapchainIdx == m_LwrrentSwapChainIdx)
        {
            myGpuQueue.front().lastDrawJobIdx = m_DrawJobIdx;
        }
        else
        {
            MASSERT(myGpuQueue.size() < 2);
            AcquireMutexAndSaveSwapchainIdx();
        }

        CHECK_RC(SendWorkToGpu());
        m_NumFrames += m_FramesPerSubmit;

        // Delay sending work to the GPU when pulsing
        if (IsPulsing())
        {
            Utility::DelayNS(static_cast<UINT32>(m_WorkController->GetOutputDelayTimeNs()));
        }

        if (m_DrawJobIdx)
        {
            CHECK_RC(WaitForPreviousDrawJob());

            // At this point there is only 1 outstanding drawJob. So lets see if we can release
            // one of the swapchain resources.
            if (myGpuQueue.front().lastDrawJobIdx < m_DrawJobIdx)
            {
                ReleaseMutexAndSignalSurfaceCheck(m_NumFrames - m_FramesPerSubmit);
            }
            CHECK_RC(TuneWorkload(m_DrawJobIdx-1));
        }
        CHECK_RC(CheckErrors());

        // Update housekeeping
        m_QueryIdx = (m_QueryIdx + 1) % MAX_NUM_TS_QUERIES;
        m_DrawJobIdx++;
        if (!m_AsyncCompute)
        {
            for (auto &cw : m_ComputeWorks)
            {
                cw.m_ComputeJobIdx++;
                cw.m_ComputeQueryIdx = (cw.m_ComputeQueryIdx + 1) % MAX_NUM_TS_QUERIES;
            }
        }

        CHECK_RC(UpdateProgressAndYield());
        CHECK_RC(m_CheckSurfaceRC);
    }

    // When we exit the loop above there will be one drawjob that is still in process.
    // Wait for it to finish and then check the surface.
    CHECK_RC(WaitForPreviousDrawJob());
    ReleaseMutexAndSignalSurfaceCheck(m_NumFrames);
    MASSERT(myGpuQueue.empty());
    if (m_ShowQueryResults)
    {
        DumpQueryResults();
    }

    return (rc);
}

RC VkStress::WaitForPreviousDrawJob()
{
    RC rc;
    vector<VkFence> waitFences(2 +  m_ComputeWorks.size());
    UINT32 numFences = 0;
    const UINT32 prevDrawJobIdx = static_cast<UINT32>((m_DrawJobIdx - 1) % m_NumDrawJobs);
    waitFences[numFences++] = m_RenderFences[prevDrawJobIdx];
    if (!m_AsyncCompute)
    {
        for (auto &cw : m_ComputeWorks)
        {
            waitFences[numFences++] = cw.m_ComputeFences[prevDrawJobIdx];
        }
    }
    if (m_Rt.get())
    {
        waitFences[numFences++] = m_Rt->GetFence(prevDrawJobIdx);
    }
    CHECK_RC(WaitForFences(numFences, waitFences.data(), VK_TRUE,
                           m_pVulkanDev->GetTimeoutNs(),
                           false));
    return rc;
}

//-------------------------------------------------------------------------------------------------
// \brief Checks for errors in the render target(s) using either golden values or the internal
//        CheckSurface() routine.
//        If BufferCheckMs is enabled then start drawing to the next swapchain and update the
//        the display engine to draw from the new swapchain.
RC VkStress::CheckErrors()
{
    RC rc;
    const auto skipCount = m_pGolden->GetSkipCount();
    if ((m_BufferCheckMs != BUFFERCHECK_MS_DISABLED) &&
        (m_ProgressHandler.ElapsedTimeMs() > m_NextBufferCheckMs) &&
        (m_NumFrames % 4 == 0))
    {
        m_NextBufferCheckMs = m_ProgressHandler.ElapsedTimeMs() + m_BufferCheckMs;
        const UINT32 nextSemIdx = (m_LwrrentSwapChainIdx + 1) % m_SwapChain->GetNumImages();
        const VkSemaphore waitSem = m_SwapChainAcquireSems[nextSemIdx].GetSemaphore();
        m_LwrrentSwapChainIdx = m_SwapChain->GetNextSwapChainImage(waitSem);
#ifdef VULKAN_STANDALONE_KHR
        MASSERT(m_LwrrentSwapChainIdx == m_LwrrentSwapChainIdx);
#endif

        // Tell display engine to scan from the new swapchain image
        SetDisplayImage(m_LwrrentSwapChainIdx);
    }
    else if (skipCount && (m_NumFrames % skipCount == 0))
    {
        CHECK_VK_TO_RC(m_pVulkanDev->DeviceWaitIdle());
        CHECK_RC(InnerCheckErrors(CHECK_ALL_SURFACES, m_LwrrentSwapChainIdx, m_NumFrames));
    }
#ifndef VULKAN_STANDALONE
    else
    {
        const auto step = m_ProgressHandler.Step();
        if ((m_CorruptionStep != CORRUPTION_STEP_DISABLED) &&
            !m_CorruptionDone && (step >= m_CorruptionStep) &&
            (m_NumFrames % 4 == 0))
        {
            Printf(Tee::PriNormal, "Corrupting image on swapchainIdx:%d at step %lld\n",
                   m_LwrrentSwapChainIdx, step);
            CHECK_RC(CorruptImage());
            m_CorruptionDone = true;

            // move to the next swapchain to cause a SurfaceCheck to be signalled.
            m_LwrrentSwapChainIdx = m_SwapChain->GetNextSwapChainImage(m_SwapChainAcquireSems[0].GetSemaphore());
            // Tell display engine to scan from the new swapchain image
            SetDisplayImage(m_LwrrentSwapChainIdx);
        }
    }
#endif
    return rc;
}

//-------------------------------------------------------------------------------------------------
// \brief Finds any miscompares and reports them
RC VkStress::InnerCheckErrors
(
    UINT32 flags,
    UINT32 swapchainIdx,
    UINT32 numFrames
)
{
    RC rc;

    if (!m_Miscompares[swapchainIdx].empty())
    {
        m_Miscompares[swapchainIdx].clear();
    }
    if (m_UseRaytracing && !m_Miscompares[RT_IMAGE_IDX].empty())
    {
        m_Miscompares[RT_IMAGE_IDX].clear();
    }

    CHECK_RC(CheckSurface(flags, swapchainIdx, numFrames));
    CHECK_RC(CheckForComputeErrors(swapchainIdx));
    CHECK_RC(HandleMiscompares());

    return rc;
}


RC VkStress::HandleMiscompares()
{
    UINT32 totalMiscompares = 0;
    for (const auto lwrrMiscompares : m_Miscompares)
    {
        for (const auto& miscompare : lwrrMiscompares)
        {
            totalMiscompares += miscompare.count;
        }
    }
    const bool tooManyErrors = totalMiscompares > m_AllowedMiscompares;
    if (tooManyErrors && m_AllowedMiscompares)
    {
        Printf(Tee::PriError, "Total allowed miscompares (%u) exceeded (%u)\n",
            m_AllowedMiscompares, totalMiscompares);
    }
    Tee::Priority miscomparePri = Tee::PriError;
    if (!tooManyErrors)
    {
        miscomparePri = Tee::PriWarn;
    }
    if (m_CorruptionDone)
    {
        miscomparePri = Tee::PriNormal;
    }
    bool graphicsErrors = false;
    const auto numSwapchainImages = m_Miscompares.size();
    for (size_t swapchainIdx = 0; swapchainIdx < numSwapchainImages; swapchainIdx++)
    {
        const auto& miscompares = m_Miscompares[swapchainIdx];
        for (const auto& miscompare : miscompares)
        {
            if (!miscompare.count)
            {
                continue;
            }
            Printf(miscomparePri,
                "%s found %u bad value(s) in %s buffer[%lu] on frame %u\n",
                GetName().c_str(), miscompare.count,
                SurfaceTypeToString(miscompare.surfType),
                swapchainIdx,
                m_NumFrames);

            if (miscompare.surfType == CHECK_COLOR_SURFACE ||
                miscompare.surfType == CHECK_DEPTH_SURFACE ||
                miscompare.surfType == CHECK_STENCIL_SURFACE ||
                miscompare.surfType == CHECK_RT_SURFACE)
            {
                graphicsErrors = true;
                continue;
            }

            UINT32 workIdx = 0;
            for (auto &cw : m_ComputeWorks)
            {
                for (UINT32 sm = 0; sm < m_NumSMs; sm++)
                {
                    if (miscompare.surfType == CHECK_COMPUTE_FP16 &&
                        cw.m_pScgStats->smFP16Miscompares[sm])
                    {
                        Printf(miscomparePri, "%u FP16 compute miscompare(s) on SM%u workIdx=%d\n",
                            cw.m_pScgStats->smFP16Miscompares[sm], sm, workIdx);
                    }
                    if (miscompare.surfType == CHECK_COMPUTE_FP32 &&
                        cw.m_pScgStats->smFP32Miscompares[sm])
                    {
                        Printf(miscomparePri, "%u FP32 compute miscompare(s) on SM%u workIdx=%d\n",
                            cw.m_pScgStats->smFP32Miscompares[sm], sm, workIdx);
                    }
                    if (miscompare.surfType == CHECK_COMPUTE_FP64 &&
                        cw.m_pScgStats->smFP64Miscompares[sm])
                    {
                        Printf(miscomparePri, "%u FP64 compute miscompare(s) on SM%u workIdx=%d\n",
                            cw.m_pScgStats->smFP64Miscompares[sm], sm, workIdx);
                    }
                }
                workIdx++;
            }
        }
    }

    if (tooManyErrors && !(m_Debug & DBG_IGNORE_FRAME_CHECK))
    {
        m_CheckSurfaceRC = graphicsErrors ?
            RC::GPU_STRESS_TEST_FAILED : RC::GPU_COMPUTE_MISCOMPARE;
        return m_CheckSurfaceRC;
    }

    return RC::OK;
}

//-------------------------------------------------------------------------------------------------
// \brief Updates test progress, checks for CTRL+C, and yields to other tests
RC VkStress::UpdateProgressAndYield()
{
    RC rc;

    m_ProgressHandler.Update(m_NumFrames);
    const auto step = m_ProgressHandler.Step();

    CHECK_RC(EndLoopMLE(step));
    CHECK_RC(PrintProgressUpdate(step));

    CHECK_RC(Abort::Check());

    // Yield on every second draw job. This ensures that we will have a buffer
    // of work for the GPU to consume during the yield.
    if (IsPulsing() && (m_DrawJobIdx % m_NumDrawJobs == 0))
    {
        Tasker::Yield();
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Cleanup *ALL* resources and don't rely on the object's destructors to do this for you because:
// 1. Object destruction order is not guarenteed, it oclwrs when the JS garbage collector thinks
//    its time to call the destructor.
// 2. If you  intend to run this application prior to another OpenGL application that tries to
//    setup the desktop. Then all of the Vulkan objects need to be freed up before we call
//    dglMakeLwrrent(). This is because deep inside of dglMakeLwrrent() it calls
//    RecoverModeSwitch() which will try to discard all of its internal texture allocations and
//    fails with an assert in __glLWMemMgrFreeMem() because the memory header is still valid.
RC VkStress::Cleanup()
{
    StickyRC rc;
    vector<FLOAT64> computeWorksTOps;

    m_CleanupTimeCallwlator.StartTimeRecording();

    if (!m_ComputeShaderOpsPerInnerLoop.empty())
    {
        computeWorksTOps.resize(m_ComputeWorks.size());

        for (UINT32 cwi = 0; cwi < m_ComputeWorks.size(); cwi++)
        {
            ComputeWork &cw = m_ComputeWorks[cwi];

            const UINT64 opsPerIteration = m_ComputeInnerLoops[cwi] *
                m_ComputeShaderOpsPerInnerLoop[m_ComputeShaderSelection[cwi]];

            UINT64 totalOps = 0;
            for (UINT32 i = 0; i < m_NumSMs; i++)
            {
                totalOps += cw.m_pScgStats->smIterations[i];
            }
            totalOps *= opsPerIteration;

            const FLOAT64 runtimeSeconds = m_RunTimeCallwlator.GetTimeSinceLastRecordingUS() / 1e6;
            computeWorksTOps[cwi] = totalOps / runtimeSeconds / 1e12;
        }
    }

#ifndef VULKAN_STANDALONE
    if (!m_SkipDirectDisplay && GetDisplayMgrTestContext())
    {
        if (m_DisplayRaytracing && m_Rt.get())
        {
            rc = GetDisplayMgrTestContext()->DisplayImage(
                static_cast<Surface2D*>(nullptr), DisplayMgr::WAIT_FOR_UPDATE, 1);
        }
        rc = GetDisplayMgrTestContext()->DisplayImage(
            static_cast<Surface2D*>(nullptr), DisplayMgr::WAIT_FOR_UPDATE);
    }
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    if (m_PerfPointOverriden)
    {
        VerbosePrintf("Restoring original PerfPoint %s\n", m_OrigPerfPoint.name(true).c_str());
        rc = pPerf->SetPerfPoint(m_OrigPerfPoint);
        m_PerfPointOverriden = false;
    }
    if (m_PStatesClaimed)
    {
        m_PStateOwner.ReleasePStates();
        m_PStatesClaimed = false;
    }
#endif

    // Only attempt to free Vulkan device resources if the VulkanDevice object
    // was created successfully.
    if (m_pVulkanDev)
    {
        if (m_pVulkanDev->IsInitialized())
        {

            rc = m_pVulkanDev->DeviceWaitIdle();

            rc = CleanupComputeResources();
            if (m_Rt.get())
            {
                m_Rt->CleanupResources();
            }
            m_Samplers.clear();

            if (m_DescriptorInfo.get())
            {
                rc = VkUtil::VkErrorToRC(m_DescriptorInfo->Cleanup());
            }

            if (m_UniformBufferLights.get())
            {
                m_UniformBufferLights->Cleanup();
            }
            if (m_UniformBufferMVPMatrix.get())
            {
                m_UniformBufferMVPMatrix->Cleanup();
            }
            if (m_Sm2GpcBuffer.get())
            {
                m_Sm2GpcBuffer->Cleanup();
            }
            if (m_VBVertices.get())
            {
                m_VBVertices->Cleanup();
            }
            if (m_IndexBuffer.get())
            {
                m_IndexBuffer->Cleanup();
            }
            // Destroy framebuffers & textures explicitly
            for (const auto& fb : m_FrameBuffers)
            {
                if (fb.get())
                {
                     fb->Cleanup();
                }
            }
            m_FrameBuffers.clear();

            for (const auto& tx : m_Textures)
            {
                if (tx.get())
                {
                    tx->Cleanup();
                }
            }
            m_Textures.clear();

            // Destroy the pipeline
            m_GraphicsPipeline.Cleanup();

            // Destroy VulkanImages
            m_DepthImages.clear();
            m_MsaaImages.clear();

            // destroy RenderPasses
            if (m_RenderPass.get())
            {
                m_RenderPass->Cleanup();
            }
            if (m_ClearRenderPass.get())
            {
                m_ClearRenderPass->Cleanup();
            }

            m_GoldenSurfaces.clear();

            // Destroy fences and semaphores
            for (const auto& fence : m_RenderFences)
            {
                m_pVulkanDev->DestroyFence(fence);
            }
            m_RenderFences.clear();

            for (const auto& sem : m_RenderSem)
            {
                m_pVulkanDev->DestroySemaphore(sem);
            }
            m_RenderSem.clear();

            // Explicitly Free up all of the CmdBuffers
            for (const auto& job : m_DrawJobs)
            {
                if (job.get())
                {
                    job->FreeCmdBuffer();
                }
            }
            m_DrawJobs.clear();

            for (const auto& job : m_PreDrawCmdBuffers)
            {
                if (job.get())
                {
                    job->FreeCmdBuffer();
                }
            }
            m_PreDrawCmdBuffers.clear();

            m_SwapChainAcquireSems.clear();
            m_PresentCmdBuf.clear();
            if (m_InitCmdBuffer.get())
            {
                m_InitCmdBuffer->FreeCmdBuffer();
            }
            if (m_CheckSurfaceCmdBuffer.get())
            {
                m_CheckSurfaceCmdBuffer->FreeCmdBuffer();
            }
            if (m_CheckSurfaceTransBuffer.get())
            {
                m_CheckSurfaceTransBuffer->FreeCmdBuffer();
            }
            if (m_CmdPool.get())
            {
                m_CmdPool->DestroyCmdPool();
            }
            if (m_CheckSurfaceCmdPool.get())
            {
                m_CheckSurfaceCmdPool->DestroyCmdPool();
            }
            if (m_CheckSurfaceTransPool.get())
            {
                m_CheckSurfaceTransPool->DestroyCmdPool();
            }

            m_GfxTSQueries.clear();

            for (auto &cw : m_ComputeWorks)
            {
                cw.m_ComputeTSQueries.clear();
            }
            m_SwapChain = nullptr;
            if (m_SwapChainMods.get())
            {
                m_SwapChainMods->Cleanup();
            }
#ifdef VULKAN_STANDALONE_KHR
            if (m_SwapChainKHR.get())
            {
                m_SwapChainKHR->Cleanup();
            }
#endif
        }
        m_pVulkanDev->Shutdown();
        m_pVulkanDev = nullptr;
    }
    // these need to be outside the if(m_pVulkanDev) check because we could have an early failure
    // in Setup() and still need to destroy the vulkan instance.
    m_VulkanInst.DestroyVulkan();

#ifndef VULKAN_STANDALONE
    m_JsNotes.clear();
    rc = m_mglTestContext.Cleanup();
#endif

    const FLOAT64 runtimeSeconds = m_RunTimeCallwlator.GetTimeSinceLastRecordingUS() / 1e6;
    const FLOAT64 fps = runtimeSeconds ? m_NumFrames / runtimeSeconds : 0.0;
    m_RememberedFps = fps;
#ifndef VULKAN_STANDALONE
    SetPerformanceMetric(ENCJSENT("FPS"), fps);
    if (m_PulseMode == TILE_MASK_GPC_SHUFFLE)
    {
        m_GPCShuffleStats.PrintReport(GetVerbosePrintPri(), "GPC shuffle");
    }
#endif

    rc = GpuTest::Cleanup();

    m_CleanupTimeCallwlator.StopTimeRecording();

    const Tee::Priority perfPrintPri = m_PrintPerf ? Tee::PriNormal : GetVerbosePrintPri();
    Printf(perfPrintPri,
        "IsSupported:%.3f secs, Setup:%.3f secs, Run:%.3f secs, Cleanup:%.3f secs,"
        " FPS:%.3f Total frames:%d"
        ,m_IsSupportedTimeCallwlator.GetTimeSinceLastRecordingUS() / 1e6f
        ,m_SetupTimeCallwlator.GetTimeSinceLastRecordingUS() / 1e6f
        ,runtimeSeconds
        ,m_CleanupTimeCallwlator.GetTimeSinceLastRecordingUS() / 1e6f
        ,fps
        ,m_NumFrames);
    if (m_Rt.get() && runtimeSeconds)
    {
        const FLOAT64 rtFps = m_Rt->GetNumFrames() / runtimeSeconds;
        Printf(perfPrintPri, ", RtFPS:%.3f Total Rt frames:%d", rtFps, m_Rt->GetNumFrames());
    }
    for (UINT32 cwi = 0; cwi < computeWorksTOps.size(); cwi++)
    {
        Printf(perfPrintPri, ", Compute%d: %.6f TOps", cwi, computeWorksTOps[cwi]);
    }
    Printf(perfPrintPri, "\n");

    return rc;
}

static RC CheckInitUINT32Array
(
    vector<UINT32> *arr,
    UINT32 requiredSize,
    UINT32 initValue,
    const char *name
)
{
    if (arr->empty())
    {
        arr->resize(requiredSize, initValue);
    }
    else
    {
        if (arr->size() < requiredSize)
        {
            Printf(Tee::PriError, "\"%s\" array is smaller then required %d\n",
                name, requiredSize);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    return RC::OK;
}

//-------------------------------------------------------------------------------------------------
// Allocate all of the required Vulkan resources to run a compute shader in parallel with the
// graphics pipeline.
RC VkStress::AllocateComputeResources()
{
    RC rc;
    if (!m_UseCompute)
    {
        return rc;
    }

    if (m_NumComputeQueues < 1)
    {
        Printf(Tee::PriError, "At least one compute queue is required.\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_NumComputeQueues > m_AvailComputeQueues)
    {
        Printf(Tee::PriError,
            "NumComputeQueues = %d is larger than AvailComputeQueues = %d.\n",
            m_NumComputeQueues, m_AvailComputeQueues);
        return RC::SOFTWARE_ERROR;
    }

    //----------------------------------
    // allocate resources
    if (!m_AsyncCompute)
    {
        m_ComputeLoadedShader = make_unique<VulkanShader>(m_pVulkanDev);
    }

    m_ComputeWorks.resize(m_NumComputeQueues);
    for (auto &cw : m_ComputeWorks)
    {
        cw.m_ComputeCmdPool = make_unique<VulkanCmdPool>(m_pVulkanDev);
        cw.m_ComputeUBOParameters = make_unique<VulkanBuffer>(m_pVulkanDev);
        cw.m_ComputePipeline = make_unique<VulkanPipeline>(m_pVulkanDev);
        cw.m_ComputeSBOStats = make_unique<VulkanBuffer>(m_pVulkanDev);
        cw.m_ComputeSBOCells = make_unique<VulkanBuffer>(m_pVulkanDev);
        cw.m_ComputeKeepRunning = make_unique<VulkanBuffer>(m_pVulkanDev);
        cw.m_ComputeDescriptorInfo = make_unique<DescriptorInfo>(m_pVulkanDev);

        //--------------------------------------------------------------
        // we need one set of resources for each draw job (Ping vs Pong)
        cw.m_ComputeFences.resize(2);
        cw.m_ComputeSemaphores.resize(2);
        cw.m_ComputeCmdBuffers.reserve(2);

        if (!m_AsyncCompute)
        {
            cw.m_ComputeLoadedSemaphores.resize(2);
            cw.m_ComputeLoadedPipeline = make_unique<VulkanPipeline>(m_pVulkanDev);
            cw.m_ComputeLoadedCmdBuffers.reserve(2);
        }
    }

    CHECK_RC(CheckInitUINT32Array(&m_ComputeInnerLoops, m_NumComputeQueues, 20, "ComputeInnerLoops"));
    CHECK_RC(CheckInitUINT32Array(&m_ComputeOuterLoops, m_NumComputeQueues, 1, "ComputeOuterLoops"));
    CHECK_RC(CheckInitUINT32Array(&m_ComputeRuntimeClks, m_NumComputeQueues, 0, "ComputeRuntimeClks"));

    CHECK_RC(SetupComputeSemaphoresAndFences());
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Setup the requires semaphores and fences to run the compute shader in parallel with the graphics
// workload. The compute pipeline uses the same ping-pong algorithm as the graphics pipeline.
// However we only need a single command buffer, semaphore, & fence for each ping & pong run.
RC VkStress::SetupComputeSemaphoresAndFences()
{
    RC rc;
    if (!m_UseCompute)
    {
        return rc;
    }

    VkSemaphoreCreateInfo semInfo;
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = nullptr;
    semInfo.flags = 0;

    // Setup the compute fence in non-signaled state
    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;

    for (auto &cw : m_ComputeWorks)
    {
        const size_t numComputeJobs = 2;
        for (size_t i = 0; i < numComputeJobs; i++)
        {
            CHECK_VK_TO_RC(m_pVulkanDev->CreateSemaphore(&semInfo, nullptr, &cw.m_ComputeSemaphores[i]));
            CHECK_VK_TO_RC(m_pVulkanDev->CreateFence(&fenceInfo, nullptr, &cw.m_ComputeFences[i]));
            if (!m_AsyncCompute)
            {
                CHECK_VK_TO_RC(m_pVulkanDev->CreateSemaphore(&semInfo,
                                                             nullptr,
                                                             &cw.m_ComputeLoadedSemaphores[i]));
            }
        }
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Setup the compute command and data buffers.
// We use 3 data buffers as follows:
// - UBO ComputeParameters: This defines the dispatch parameters for the compute work.
// - SBO SCGStats: This buffer is used to log the simultaneous compute & graphics statistics.
// - SBO ComputeResults: This buffer is made available to the compute shader to log any debug or
//       result information in a single 64 bit values for each thread.
RC VkStress::SetupComputeBuffers(ComputeWork *pCW, UINT32 queueIdx)
{
    RC rc;

    CHECK_VK_TO_RC(pCW->m_ComputeCmdPool->InitCmdPool(
        m_pVulkanDev->GetPhysicalDevice()->GetComputeQueueFamilyIdx(), queueIdx));

    for (size_t i = 0; i < m_NumComputeJobs; i++)
    {
        pCW->m_ComputeCmdBuffers.emplace_back(make_unique<VulkanCmdBuffer>(m_pVulkanDev));
        CHECK_VK_TO_RC(pCW->m_ComputeCmdBuffers[i]->AllocateCmdBuffer(pCW->m_ComputeCmdPool.get()));

        if (!m_AsyncCompute)
        {
            //Note: This CmdBuffer must be exelwted on the compute queue.
            pCW->m_ComputeLoadedCmdBuffers.emplace_back(make_unique<VulkanCmdBuffer>(m_pVulkanDev));
            CHECK_VK_TO_RC(pCW->m_ComputeLoadedCmdBuffers[i]->AllocateCmdBuffer(pCW->m_ComputeCmdPool.get())); //$
        }
    }

    //---------------------------------------------------------------------------------------------
    // Setup global buffers, UBO params, SBO stats, & SBO cells
    UINT32 buffSize = sizeof(ComputeParameters);
    CHECK_VK_TO_RC(pCW->m_ComputeUBOParameters->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,buffSize
        ,m_ZeroFb ? VK_MEMORY_PROPERTY_SYSMEM_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    UINT32 queueFamilies[2] =
    {
        m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx()
        ,m_pVulkanDev->GetPhysicalDevice()->GetComputeQueueFamilyIdx()
    };
    // Some chips report the same queue for both graphic & compute (ie Pascal), and we need to
    // report the number of "unique" queues to prevent a validation error.
    UINT32 numFamilies = (queueFamilies[0] == queueFamilies[1]) ? 1 : 2;
    VkSharingMode sharingMode = (numFamilies > 1) ?
        VK_SHARING_MODE_CONLWRRENT :
        VK_SHARING_MODE_EXCLUSIVE;
    CHECK_VK_TO_RC(pCW->m_ComputeKeepRunning->CreateBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,sizeof(UINT32)
        ,m_ZeroFb ? VK_MEMORY_PROPERTY_SYSMEM_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ,sharingMode
        ,numFamilies
        ,queueFamilies));
    buffSize = sizeof(ScgStats);

    CHECK_VK_TO_RC(pCW->m_ComputeSBOStats->CreateBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,buffSize
        ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        ,sharingMode
        ,numFamilies
        ,queueFamilies));
    CHECK_VK_TO_RC(pCW->m_ComputeSBOStats->Map(
        sizeof(ScgStats)    // size
        ,0                  // offset
        ,reinterpret_cast<void **>(&pCW->m_pScgStats)));

    //-------------------------------
    // Initialize the SimParameters
    pCW->m_ComputeParameters.pGpuSem = 0;
    pCW->m_ComputeParameters.width = CHECKED_CAST(INT32, m_NumSMs * WARP_SIZE);
    pCW->m_ComputeParameters.height = CHECKED_CAST(INT32, m_WarpsPerSM);

    // Be careful with this one increasing it too much can cause the compute workload to run
    // much longer than the graphics work.
    pCW->m_ComputeParameters.outerLoops = CHECKED_CAST(INT32, m_ComputeOuterLoops[queueIdx]);

    // Be careful with this one, increasing it too much will cause saturation of the
    // aclwmulator and generate SM miscompares.
    pCW->m_ComputeParameters.innerLoops = CHECKED_CAST(INT32, m_ComputeInnerLoops[queueIdx]);

    if (pCW->m_ComputeParameters.outerLoops <= 0 ||
        pCW->m_ComputeParameters.innerLoops <= 0)
    {
        Printf(Tee::PriError, "ComputeInnerLoops and ComputeOuterLoops must be greater than 0\n");
        return RC::ILWALID_ARGUMENT;
    }

    // Start with default and override if required.
    pCW->m_ComputeParameters.runtimeClks = m_ComputeRuntimeClks[queueIdx];
    if (!pCW->m_ComputeParameters.runtimeClks)
        pCW->m_ComputeParameters.runtimeClks =  60000 * 1000LL;

    // Setup the SBO buffer for compute to output a single 64bit value per thread
    buffSize = pCW->m_ComputeParameters.width * pCW->m_ComputeParameters.height * sizeof(UINT64);
    CHECK_VK_TO_RC(pCW->m_ComputeSBOCells->CreateBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        ,buffSize
        ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        ,sharingMode
        ,numFamilies
        ,queueFamilies));

    CHECK_VK_TO_RC(VkUtil::UpdateBuffer(pCW->m_ComputeCmdBuffers[0].get()
                         ,pCW->m_ComputeUBOParameters.get()
                         ,0 //dst offset
                         ,sizeof(ComputeParameters)
                         ,&pCW->m_ComputeParameters
                         ,VK_ACCESS_TRANSFER_WRITE_BIT  //srcAccess
                         ,VK_ACCESS_UNIFORM_READ_BIT    // dstAccess
                         ,VK_PIPELINE_STAGE_TRANSFER_BIT // srcStageMask
                         ,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT));

    //-------------------------------
    // Initialize KeepRunning to  1
    UINT32 keepRunning = 1;
    CHECK_VK_TO_RC(VkUtil::FillBuffer(pCW->m_ComputeCmdBuffers[0].get()
                       ,pCW->m_ComputeKeepRunning.get()
                       ,0
                       ,sizeof(keepRunning)
                       ,keepRunning
                       ,VK_ACCESS_TRANSFER_WRITE_BIT    //srcAccess
                       ,VK_ACCESS_SHADER_READ_BIT       //dstAccess
                       ,VK_PIPELINE_STAGE_TRANSFER_BIT  //srcStageMask
                       ,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)); //dstStageMask

    //-------------------------------
    // Initialize ScgStats to zero
    CHECK_VK_TO_RC(VkUtil::FillBuffer(pCW->m_ComputeCmdBuffers[0].get()
                       ,pCW->m_ComputeSBOStats.get()
                       ,0
                       ,sizeof(ScgStats)
                       ,0
                       ,VK_ACCESS_TRANSFER_WRITE_BIT    //srcAccess
                       ,VK_ACCESS_SHADER_READ_BIT       //dstAccess
                       ,VK_PIPELINE_STAGE_TRANSFER_BIT  //srcStageMask
                       ,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)); //dstStageMask

    //---------------------------------
    // Write the initial cell states
    CHECK_VK_TO_RC(pCW->m_ComputeSBOCells->Map(buffSize, 0, reinterpret_cast<void **>(&pCW->m_pCells)));
    UINT32 offset = pCW->m_ComputeParameters.width * pCW->m_ComputeParameters.height;
    for (UINT32 i = 0; i < offset; i++)
    {
        pCW->m_pCells[i] = 0xdeadbeef;
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
//
RC VkStress::CreateComputePipeline()
{
    RC rc;
    if (!m_UseCompute)
    {
        return rc;
    }

    UINT32 launchQueueIdx = 0;
    for (auto &cw : m_ComputeWorks)
    {
        VulkanShader *vs = m_ComputeShaders[m_ComputeShaderSelection[launchQueueIdx]].get();

        if (m_ComputeWorks.size() > 1)
        {
            cw.m_ComputePipeline->SetCTALaunchQueueIdx(launchQueueIdx);
            launchQueueIdx++;
        }

        // Setup any specialization data
        VkSpecializationMapEntry specializationEntries[] =
        {
            {
                0,
                offsetof(ComputeSpecializationData, asyncCompute),
                sizeof(ComputeSpecializationData::asyncCompute)
            }
        };
        cw.m_ComputeSpecializationData.asyncCompute = m_AsyncCompute;

        VkSpecializationInfo specializationInfo = { };
        specializationInfo.mapEntryCount =
            sizeof(specializationEntries) / sizeof(specializationEntries[0]);
        specializationInfo.pMapEntries = specializationEntries;
        specializationInfo.dataSize = sizeof(cw.m_ComputeSpecializationData);
        specializationInfo.pData = &cw.m_ComputeSpecializationData;

        VkPipelineShaderStageCreateInfo createInfo = vs->GetShaderStageInfo();
        createInfo.pSpecializationInfo = &specializationInfo;

        // Setup any push constants

        // Setup the compute pipeline
        CHECK_VK_TO_RC(cw.m_ComputePipeline->SetupComputePipeline(
            cw.m_ComputeDescriptorInfo->GetDescriptorSetLayout(0)
            ,createInfo));

        if (!m_AsyncCompute)
        {
            CHECK_VK_TO_RC(cw.m_ComputeLoadedPipeline->SetupComputePipeline(
                cw.m_ComputeDescriptorInfo->GetDescriptorSetLayout(0)
                ,m_ComputeLoadedShader->GetShaderStageInfo()));
        }
    }
    return (rc);
}

//-------------------------------------------------------------------------------------------------
// We build 2 command buffers, 1 that waits for all of the SMs to load with compute work and then
// exits to signal a GPU semaphore and one that actually does the compute workload.
RC VkStress::BuildComputeCmdBuffer(ComputeWork *pCW)
{
    RC rc;

    VK_DEBUG_MARKER(markerCompute, 0.0f, 0.0f, 0.5f)
    VK_DEBUG_MARKER(markerComputeLoad, 0.0f, 0.5f, 0.0f)

    auto& cmdBuffer = pCW->m_ComputeCmdBuffers[pCW->m_ComputeJobIdx % pCW->m_ComputeCmdBuffers.size()];
    CHECK_VK_TO_RC(cmdBuffer->ResetCmdBuffer());
    CHECK_VK_TO_RC(cmdBuffer->BeginCmdBuffer());

    VK_DEBUG_MARKER_BEGIN(markerCompute, m_EnableDebugMarkers, m_pVulkanDev,
        cmdBuffer->GetCmdBuffer(), "Compute%lld", pCW->m_ComputeJobIdx);

    CHECK_VK_TO_RC(pCW->m_ComputeTSQueries[pCW->m_ComputeQueryIdx]->CmdReset(cmdBuffer.get(), 0, 2));
    CHECK_VK_TO_RC(pCW->m_ComputeTSQueries[pCW->m_ComputeQueryIdx]->CmdWriteTimestamp(cmdBuffer.get()
                                        ,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                        ,0));
    // Bind the pipeline
    m_pVulkanDev->CmdBindPipeline(
        cmdBuffer->GetCmdBuffer()
        ,VK_PIPELINE_BIND_POINT_COMPUTE
        ,pCW->m_ComputePipeline->GetPipeline());

    // bind the descriptor sets
    VkDescriptorSet descSet[1] = { pCW->m_ComputeDescriptorInfo->GetDescriptorSet(0) };
    m_pVulkanDev->CmdBindDescriptorSets(
        cmdBuffer->GetCmdBuffer()
        ,VK_PIPELINE_BIND_POINT_COMPUTE
        ,pCW->m_ComputePipeline->GetPipelineLayout()
        ,0          // firstSet
        ,1          // descriptorSetCount
        ,descSet    // pDescriptorSets
        ,0          // dynamicOffsetCount
        ,nullptr    // pDynamicOffsets
        );

    // update parameters inline
    CHECK_VK_TO_RC(VkUtil::UpdateBuffer(cmdBuffer.get()
                         ,pCW->m_ComputeUBOParameters.get()
                         ,0 //dst offset
                         ,sizeof(ComputeParameters)
                         ,&pCW->m_ComputeParameters
                         ,VK_ACCESS_TRANSFER_WRITE_BIT //srcAccess
                         ,VK_ACCESS_UNIFORM_READ_BIT   // dstAccess
                         ,VK_PIPELINE_STAGE_TRANSFER_BIT // srcStageMask
                         ,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)); //dstStageMask

    m_pVulkanDev->CmdDispatch(
        cmdBuffer->GetCmdBuffer()
        ,m_NumSMs       // groupCountX = NumSMs
        ,1              // groupCountY = 1
        ,1);            // groupCountZ

    CHECK_VK_TO_RC(pCW->m_ComputeTSQueries[pCW->m_ComputeQueryIdx]->CmdWriteTimestamp(cmdBuffer.get()
                                        ,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                                        ,1));
    CHECK_VK_TO_RC(pCW->m_ComputeTSQueries[pCW->m_ComputeQueryIdx]->CmdCopyResults(cmdBuffer.get()
                                     ,0                      //first index
                                     ,2                      //count
                                     ,VK_QUERY_RESULT_64_BIT));     //sizeof result

    VK_DEBUG_MARKER_END(markerCompute);

    CHECK_VK_TO_RC(cmdBuffer->EndCmdBuffer());

    if (!m_AsyncCompute)
    {
        // This one just waits for all the SMs to load compute work, then exits.
        auto& computeLoadedCmdBuffer =
            (pCW->m_ComputeLoadedCmdBuffers)[m_DrawJobIdx % pCW->m_ComputeLoadedCmdBuffers.size()];
        CHECK_VK_TO_RC(computeLoadedCmdBuffer->ResetCmdBuffer(0));
        CHECK_VK_TO_RC(computeLoadedCmdBuffer->BeginCmdBuffer());

        VK_DEBUG_MARKER_BEGIN(markerComputeLoad, m_EnableDebugMarkers, m_pVulkanDev,
            cmdBuffer->GetCmdBuffer(), "ComputeLoad%lld", m_DrawJobIdx);

        // Bind the pipeline
        m_pVulkanDev->CmdBindPipeline(
            computeLoadedCmdBuffer->GetCmdBuffer()
            ,VK_PIPELINE_BIND_POINT_COMPUTE
            ,pCW->m_ComputeLoadedPipeline->GetPipeline());

        // bind the descriptor sets
        m_pVulkanDev->CmdBindDescriptorSets(
            computeLoadedCmdBuffer->GetCmdBuffer()
            ,VK_PIPELINE_BIND_POINT_COMPUTE
            ,pCW->m_ComputeLoadedPipeline->GetPipelineLayout()
            ,0          // firstSet
            ,1          // descriptorSetCount
            ,descSet    // pDescriptorSets
            ,0          // dynamicOffsetCount
            ,nullptr    // pDynamicOffsets
            );

        // update parameters inline
        CHECK_VK_TO_RC(VkUtil::UpdateBuffer(computeLoadedCmdBuffer.get()
                             ,pCW->m_ComputeUBOParameters.get()
                             ,0 //dst offset
                             ,sizeof(ComputeParameters)
                             ,&pCW->m_ComputeParameters
                             ,VK_ACCESS_TRANSFER_WRITE_BIT //srcAccess
                             ,VK_ACCESS_UNIFORM_READ_BIT   // dstAccess
                             ,VK_PIPELINE_STAGE_TRANSFER_BIT // srcStageMask
                             ,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)); //dstStageMask

        m_pVulkanDev->CmdDispatch(computeLoadedCmdBuffer->GetCmdBuffer(), 1, 1, 1);

        VK_DEBUG_MARKER_END(markerComputeLoad);

        CHECK_VK_TO_RC(computeLoadedCmdBuffer->EndCmdBuffer());
    }

    return (rc);
}

//-------------------------------------------------------------------------------------------------
//
RC VkStress::SetupComputeShader()
{
    RC rc;

    if (!m_UseCompute)
    {
        return RC::OK;
    }

    if ((m_NumSMs == 0) || (m_NumSMs > MAX_SMS))
    {
        // May need to increase the size of the SCG arrays below.
        Printf(Tee::PriError, "Number of SMs is greater than Compute Shader can handle.\n");
        return RC::SOFTWARE_ERROR;
    }

    const bool useIMMA = !m_ComputeDisableIMMA &&
        m_pVulkanDev->HasExt(VulkanDevice::ExtVK_LW_cooperative_matrix);

    m_ComputeProgs.resize(useIMMA ? 2 : 1);
    m_ComputeShaderOpsPerInnerLoop.resize(useIMMA ? 2 : 1);

    UINT32 progIdx = 0;

    if (useIMMA)
    {
        CHECK_RC(SetupComputeIMMAShader(&m_ComputeProgs[progIdx],
            &m_ComputeShaderOpsPerInnerLoop[progIdx]));
        progIdx++;
    }

    CHECK_RC(SetupComputeFMAShader(&m_ComputeProgs[progIdx],
        &m_ComputeShaderOpsPerInnerLoop[progIdx]));

    if (m_ComputeShaderSelection.empty())
    {
        m_ComputeShaderSelection.resize(m_NumComputeQueues);
        if (m_NumComputeQueues == 1)
        {
            m_ComputeShaderSelection[0] = 0;
        }
        else
        {
            for (UINT32 computeQueueIdx = 0; computeQueueIdx < m_NumComputeQueues; computeQueueIdx++)
            {
                m_ComputeShaderSelection[computeQueueIdx] =
                    useIMMA ?
                        ((computeQueueIdx < (m_NumComputeQueues-1)) ? 0 : 1) :
                        0;
            }
        }
    }
    else if (m_ComputeShaderSelection.size() < m_NumComputeQueues)
    {
        Printf(Tee::PriError,
            "\"ComputeShaderSelection\" array is smaller then required %d\n",
            m_NumComputeQueues);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    for (UINT32 cqIdx = 0; cqIdx < m_NumComputeQueues; cqIdx++)
    {
        if (m_ComputeShaderSelection[cqIdx] >= m_ComputeProgs.size())
        {
            Printf(Tee::PriError, "ComputeShaderSelection values must be between 0 and %zu\n",
                m_ComputeShaders.size());
            return RC::ILWALID_ARGUMENT;
        }
    }

    return rc;
}

// TODO: Modify this code to do more math intensive operations and/or have the code be
// self-checking.
// Some notes about this compute shader:
// 1. One SM can handle a maximum of 32 warps (1024 threads) conlwrrently
// 2. A local block size of 32x1 consumes 1 warps, and we don't want to oversubscribe the
//    number of warps or the graphics workload will stall waiting for the compute to finish.
// 3. The number of blocks is determined by the m_NumSMs & m_WarpsPerSM vars
// 4. runtimeClks is not really a nanosecond resolution. It's SM clock cycles and on Volta
//    appears to be in 0.5ns intervals. However with our PState algorithm this clock will not
//    be consistent. We don't have access to a global PTIMER and this is the best we can do
//    for a fallback (DONT RUN FOREVER) mechanism. The runtimeClks is recallwlated after each
//    launch to determine what the current clks/ns ratio is.
// The compute shader that does the following:
// -Waits for all of the SMs to load the compute threads then signals the GPU semaphore
// -Waits for the Start cmd.
// -Performs arithemetic ops and polls for the Stop cmd to exit.
// Note: The Start/Stop cmds are done using buffer updates from the graphics queue.

static const char* const s_VkStressComputeFMAShader = HS_(R"glsl(
#version 450  // GL version 4.5 assumed
#extension GL_ARB_compute_shader: enable
#extension GL_LW_shader_sm_builtins : require  // Needed for gl_SMCountLW
#extension GL_ARB_gpu_shader_int64 : enable    //uint64_t datatype
#extension GL_ARB_shader_clock : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable //float16_t
#pragma optionLW (unroll all)
const double TwoExp32         = 4294967296.0;  // 0x100000000 (won't fit in UINT32)
const double TwoExp32Less1    = 4294967295.0;  //  0xffffffff
#define TwoExp32Ilw           (1.0/TwoExp32)
#define TwoExp32Less1Ilw      (1.0/TwoExp32Less1)
#define SZ_ARRAY    16
#define MAX_SMS     144
// Lwca to GLSL Colwersions:
// gl_NumWorkGroups.xyz == gridDim.xyz
// gl_WorkGroupID.xyz == blockIdx.xyz
// gl_WorkGroupSize.xyz == blockDim.xyz
// gl_LocalIlwocationID.xyz == threadIdx.xyz
#define gX gl_GlobalIlwocationID.x
#define gY gl_GlobalIlwocationID.y
#define lX gl_LocalIlwocationID.x
#define lY gl_LocalIlwocationID.y

layout (local_size_x = 32, local_size_y = 1) in;

layout (constant_id = 0) const int ASYNC_COMPUTE = 0;

layout(std140, binding=0) uniform parameters
{
    int simWidth;
    int simHeight;
    int outerLoops;
    int innerLoops;
    uint64_t runtimeClks; //maximum number of clks to run the shader
    uint64_t pGpuSem; // graphics GPU semaphore to signal once all the SMs are loaded
};

layout(std430, binding=1) volatile coherent buffer stats
{
    uint     smLoaded[MAX_SMS];
    uint     smKeepRunning[MAX_SMS];     //final keepRunning value of thread 0 of each warp.
    uint64_t smStartClock[MAX_SMS];      //clock value when the kernel started
    uint64_t smEndClock[MAX_SMS];        //clock value when the kernel stopped
    uint     smFP16Miscompares[MAX_SMS]; //miscompares using float16_t ops
    uint     smFP32Miscompares[MAX_SMS]; //miscompares using float ops
    uint     smFP64Miscompares[MAX_SMS]; //miscompares using double ops
    uint     smIterations[MAX_SMS];      //exelwtion counter for performance measurements
};
layout(std430, binding=2) buffer Cells { uint64_t cells[]; };
layout(std430, binding=3) volatile coherent buffer KeepRunning { uint keepRunning; };

// Shared memory writes increase stress
shared float   s_Values[gl_WorkGroupSize.x][gl_WorkGroupSize.y];
shared uint64_t s_StartClk[gl_WorkGroupSize.x][gl_WorkGroupSize.y];
shared uint64_t s_EndClk[gl_WorkGroupSize.x][gl_WorkGroupSize.y];
uint GetRandom(inout uint pState)
{
    uint64_t temp = pState;
    pState = uint((1664525UL * temp + 1013904223UL) & 0xffffffff);
    return pState;
}

void InitRandomState(in uint seed, inout uint pState)
{
    uint i;
    pState = seed;
    GetRandom(pState);
    // randomize a random number of times between 0 - 31
    for (i = 0; i< (pState & 0x1F); i++)
        GetRandom(pState);
}

float GetRandomFloat(inout uint pState, in double min, in double max)
{
    double binSize = (max - min) * TwoExp32Less1Ilw;
    float temp = float(min + binSize * GetRandom(pState));
    return temp;
}

double GetRandomDouble(inout uint  pState, in double min, in double max)
{
    double binSize = (max - min) * TwoExp32Ilw;
    double coarse = binSize * GetRandom(pState);
    return min + coarse + (GetRandom(pState) * binSize * TwoExp32Less1Ilw);
}

float GetRandomFloatMeanSD(inout uint  pState, float mean, float stdDeviation)
{
    float rand1, rand2, s, z;

    //s must be 0 < s < 1
    do {
        rand1 = GetRandomFloat(pState, -1.0, 1.0);
        rand2 = GetRandomFloat(pState, -1.0, 1.0);
        s = rand1 * rand1 + rand2 * rand2;
    } while (s < 1E-2 || s >= 1.0);

    s = sqrt((-2.0 * log(s)) / s);
    z = rand1 * s;

    return(mean + z * stdDeviation);
}

double GetRandomDoubleMeanSD(inout uint  pState,
                             in double mean,
                             in double stdDeviation)
{
    double rand1, rand2, s, z;

    //s must be 0 < s < 1
    do {
        rand1 = GetRandomDouble(pState, -1.0, 1.0);
        rand2 = GetRandomDouble(pState, -1.0, 1.0);
        s = rand1 * rand1 + rand2 * rand2;
    } while (s == 0 || s >= 1.0);

    s = sqrt((-2.0 * log(float(s))) / s);
    z = rand1 * s;

    return(mean + z * stdDeviation);
}

float GenRandomFloat(in uint seed)
{
    uint randomState = 0;

    InitRandomState(seed, randomState);
    return GetRandomFloatMeanSD(randomState, 0.0F, 5.0F);
}

double GenRandomDouble(in uint seed)
{
    uint randomState = 0;

    InitRandomState(seed, randomState);
    return GetRandomDoubleMeanSD(randomState, 0.0LF, 5.0LF);
}

void Callwlate(inout float16_t halves[SZ_ARRAY],
               inout float floats[SZ_ARRAY],
               inout float16_t accHalf,
               inout float accFloat)
{
    // Run multiple loops of the aclwmulator to improve stressfulness
    for (int inner = 0; inner < innerLoops; inner++)
    {
        #pragma unroll
        for (int i = 0; i < SZ_ARRAY / 2; i++)
        {
            accHalf = fma(halves[i], halves[i + SZ_ARRAY / 2], accHalf);
            accFloat = fma(floats[i], floats[i + SZ_ARRAY / 2], accFloat);
            s_Values[lX][lY] = floats[i];
        }
    }
}

void main()
{
    if ((gX >= simWidth) || (gY >= simHeight))
    {
        return;
    }
    if ((lX == 0) && (lY == 0))
    {
        atomicExchange(smLoaded[gl_SMIDLW], 1);
    }
    s_StartClk[lX][lY] = clockARB();
    s_EndClk[lX][lY] = s_StartClk[lX][lY] + runtimeClks;
    uint iterations = 0;
    float16_t halves[SZ_ARRAY];
    float floats[SZ_ARRAY];

    float16_t initHalf = float16_t(0.0);
    float initFloat = 0.0;

    // Initialize vectors with random data
    for (int i = 0; i < SZ_ARRAY; i++)
    {
        halves[i]  = float16_t(GenRandomFloat(gl_LocalIlwocationIndex+i));
        floats[i] = GenRandomFloat(gl_LocalIlwocationIndex+i);
    }
    // now wait for all the SMs to load up their compute threads.
    if ((lX + lY) == 0)
    {
        uint allSMsLoaded = 0;
        int i = 0;
        //wait for all of the SMs to load some compute work
        while((ASYNC_COMPUTE == 0) && (allSMsLoaded < gl_SMCountLW) && (clockARB() < s_EndClk[lX][lY]))
        {
            allSMsLoaded = 0;
            for (i = 0; i < gl_SMCountLW; i++)
            {
                allSMsLoaded += (smLoaded[i] & 0x1);
            }
        }
    }
    barrier();
    //wait for the START cmd
    while((ASYNC_COMPUTE == 0) && ((keepRunning & 0x3) != 2) && (clockARB() < s_EndClk[lX][lY]));

    // Initialize aclwmulation result with vector dot-product FP16/FP64 operations
    Callwlate(halves, floats, initHalf, initFloat);
    iterations++;
    float16_t resHalf = initHalf;
    float  resFloat  = initFloat;
    cells[gY * simWidth + gX] = 0;

    //run until the STOP cmd
    while( ( ((keepRunning & 0x3) != 3) || (ASYNC_COMPUTE != 0) )
             && (clockARB() < s_EndClk[lX][lY]) )
    {
        // This is the heart of the power-stress kernel. The general principle is to activate
        // multiple arithmetic engines at a time. For instance, on SM75 f32 and f16 exercise
        // different engines, so we can issue instructions to both to increase power draw.
        //
        // The instructions used here aren't as optimal as hand-tuned SASS code,
        // but they seem to work well enough for the purposes of drawing power.
        //
        // We set our result to half the previous result and half the new
        // aclwmulated value. This allows us to easily check if there was an
        // error in the computation by comparing against the original value.
        for (int outer = 0; outer < outerLoops; outer++)
        {
            float16_t accHalf = float16_t(0.0);
            float accFloat  = 0.0;

            Callwlate(halves, floats, accHalf, accFloat);
            iterations++;

            resHalf = (float16_t(0.5) * resHalf) + (float16_t(0.5) * accHalf);
            resFloat = (0.5 * resFloat) + (0.5 * accFloat);
        }
        if (resHalf != initHalf)
        {
            atomicAdd(smFP16Miscompares[gl_SMIDLW], 1);
        }
        if (resFloat != initFloat)
        {
            atomicAdd(smFP32Miscompares[gl_SMIDLW], 1);
        }
    }
    barrier();
    if ((lX + lY) == 0)
    {
        smKeepRunning[gl_SMIDLW] =  keepRunning;
        smStartClock[gl_SMIDLW] = s_StartClk[lX][lY];
        smEndClock[gl_SMIDLW] = clockARB();
    }
    if (lX == 0)
    {
        atomicAdd(smIterations[gl_SMIDLW], iterations);
    }
    barrier();
}
)glsl");

RC VkStress::SetupComputeFMAShader(string *computeProg, UINT32 *computeOpsPerInnerLoop) const
{
    MASSERT(computeProg);
    MASSERT(computeOpsPerInnerLoop);

    constexpr UINT32 szArray = 16;
    constexpr UINT32 numFmaCalls = 2;
    constexpr UINT32 localSizeX = 32;
    constexpr UINT32 numMuls = 1;
    constexpr UINT32 numAdds = 1;
    *computeOpsPerInnerLoop = numFmaCalls * szArray/2 * localSizeX * (numMuls + numAdds);

    *computeProg = s_VkStressComputeFMAShader;

    return RC::OK;
}

static const char* const s_VkStressComputeIMMAShader = HS_(R"glsl(
#version 450
#extension GL_KHR_memory_scope_semantics : enable
#extension GL_LW_integer_cooperative_matrix: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
#extension GL_LW_shader_sm_builtins : require
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_ARB_shader_clock : enable

#define MAX_SMS 144
#define gX gl_GlobalIlwocationID.x
#define gY gl_GlobalIlwocationID.y
#define lX gl_LocalIlwocationID.x
#define lY gl_LocalIlwocationID.y

#define SIZE_M 8  // Result rows
#define SIZE_N 8  // Result cols
#define SIZE_K 32  // Matrix A columns

layout (local_size_x = 32, local_size_y = 1) in;

layout (constant_id = 0) const int ASYNC_COMPUTE = 0;

layout(std140, binding=0) uniform parameters
{
    int simWidth;
    int simHeight;
    int outerLoops;
    int innerLoops;
    uint64_t runtimeClks; //maximum number of clks to run the shader
    uint64_t pGpuSem; // graphics GPU semaphore to signal once all the SMs are loaded
};
layout(std430, binding=1) volatile coherent buffer stats
{
    uint     smLoaded[MAX_SMS];
    uint     smKeepRunning[MAX_SMS];     //final keepRunning value of thread 0 of each warp.
    uint64_t smStartClock[MAX_SMS];      //clock value when the kernel started
    uint64_t smEndClock[MAX_SMS];        //clock value when the kernel stopped
    uint     smFP16Miscompares[MAX_SMS]; //miscompares using float16_t ops
    uint     smFP32Miscompares[MAX_SMS]; //miscompares using float ops
    uint     smFP64Miscompares[MAX_SMS]; //miscompares using double ops
    uint     smIterations[MAX_SMS];      //exelwtion counter for performance measurements
};
layout(std430, binding=2) buffer Cells { uint64_t cells[]; };
layout(std430, binding=3) volatile coherent buffer KeepRunning { uint keepRunning; };

// Shared memory writes increase stress
shared float    s_Values[gl_WorkGroupSize.x][gl_WorkGroupSize.y];
shared uint64_t s_StartClk[gl_WorkGroupSize.x][gl_WorkGroupSize.y];
shared uint64_t s_EndClk[gl_WorkGroupSize.x][gl_WorkGroupSize.y];
uint GetRandom(inout uint pState)
{
    uint64_t temp = pState;
    pState = uint((1664525UL * temp + 1013904223UL) & 0xffffffff);
    return pState;
}

void InitRandomState(in uint seed, inout uint pState)
{
    uint i;
    pState = seed;
    GetRandom(pState);
    // randomize a random number of times between 0 - 31
    for (i = 0; i < (pState & 0x1F); i++)
        GetRandom(pState);
}

void ResetAclwmulation(inout icoopmatLW<32, gl_ScopeSubgroup, SIZE_M, SIZE_N> mat)
{
    for (uint i = 0; i < mat.length(); i++)
    {
        mat[i] = 0;
    }
}

icoopmatLW<8, gl_ScopeSubgroup, SIZE_M, SIZE_K> matA1;
icoopmatLW<8, gl_ScopeSubgroup, SIZE_M, SIZE_K> matA2;
icoopmatLW<8, gl_ScopeSubgroup, SIZE_M, SIZE_K> matA3;
icoopmatLW<8, gl_ScopeSubgroup, SIZE_K, SIZE_N> matB1;
icoopmatLW<8, gl_ScopeSubgroup, SIZE_K, SIZE_N> matB2;
icoopmatLW<8, gl_ScopeSubgroup, SIZE_K, SIZE_N> matB3;

void Callwlate(inout icoopmatLW<32, gl_ScopeSubgroup, SIZE_M, SIZE_N> matX)
{
    for (int inner = 0; inner < innerLoops; inner++)
    {
        matX = coopMatMulAddLW(matA1, matB1, matX);
        matX = coopMatMulAddLW(matA2, matB2, matX);
        matX = coopMatMulAddLW(matA3, matB3, matX);
        matX = coopMatMulAddLW(matA1, matB2, matX);
        matX = coopMatMulAddLW(matA2, matB3, matX);
        matX = coopMatMulAddLW(matA3, matB1, matX);
        matX = coopMatMulAddLW(matA2, matB1, matX);
        matX = coopMatMulAddLW(matA1, matB3, matX);
        matX = coopMatMulAddLW(matA3, matB2, matX);
    }
}

void main()
{
    if ((gX >= simWidth) || (gY >= simHeight))
    {
        return;
    }
    if ((lX == 0) && (lY == 0))
    {
        atomicExchange(smLoaded[gl_SMIDLW], 1);
    }

    icoopmatLW<32, gl_ScopeSubgroup, SIZE_M, SIZE_N> matFirstResult;

    uint iterations = 0;
    uint randomState = 0;
    InitRandomState(gl_LocalIlwocationIndex, randomState);
    // "length()" is only a fragment of matrix that a thread operates on
    for (uint i = 0; i < matA1.length(); i++)
    {
        matA1[i] = int8_t(GetRandom(randomState) >> 16);
        matA2[i] = int8_t(GetRandom(randomState) >> 16);
        matA3[i] = int8_t(GetRandom(randomState) >> 16);
    }
    for (uint i = 0; i < matB1.length(); i++)
    {
        matB1[i] = int8_t(GetRandom(randomState) >> 16);
        matB2[i] = int8_t(GetRandom(randomState) >> 16);
        matB3[i] = int8_t(GetRandom(randomState) >> 16);
    }
    ResetAclwmulation(matFirstResult);
    Callwlate(matFirstResult);
    iterations++;

    s_StartClk[lX][lY] = clockARB();
    s_EndClk[lX][lY] = s_StartClk[lX][lY] + runtimeClks;
    // now wait for all the SMs to load up their compute threads.
    if ( (ASYNC_COMPUTE == 0) && ((lX + lY) == 0) )
    {
        uint allSMsLoaded;
        int i;
        //wait for all of the SMs to load some compute work
        while((allSMsLoaded < gl_SMCountLW) && (clockARB() < s_EndClk[lX][lY]))
        {
            allSMsLoaded = 0;
            for (i = 0; i < gl_SMCountLW; i++)
            {
                allSMsLoaded += (smLoaded[i] & 0x1);
            }
        }
    }
    barrier();
    // wait for the START cmd
    while((ASYNC_COMPUTE == 0) && ((keepRunning & 0x3) != 2) && (clockARB() < s_EndClk[lX][lY]));

    icoopmatLW<32, gl_ScopeSubgroup, SIZE_M, SIZE_N> matLwrrentResult;
    // run until the STOP cmd
    do {
        for (int outer = 0; outer < outerLoops; outer++)
        {
            ResetAclwmulation(matLwrrentResult);
            Callwlate(matLwrrentResult);
            iterations++;
        }
        for (uint i = 0; i < matFirstResult.length(); i++)
        {
            if (matLwrrentResult[i] != matFirstResult[i])
            {
                atomicAdd(smFP32Miscompares[gl_SMIDLW], 1);
            }
        }
    } while( ( ((keepRunning & 0x3) != 3) || (ASYNC_COMPUTE != 0) )
             && (clockARB() < s_EndClk[lX][lY]) );

    // Debug - to see the result matrix with "PrintComputeCells" and "PrintTuningInfo":
    //    coopMatStoreLW(matLwrrentResult, cells, 0, 4, false);

    barrier();
    if ((lX + lY) == 0)
    {
        smKeepRunning[gl_SMIDLW] = keepRunning;
        smStartClock[gl_SMIDLW] = s_StartClk[lX][lY];
        smEndClock[gl_SMIDLW] = clockARB();
    }
    if (lX == 0)
    {
        atomicAdd(smIterations[gl_SMIDLW], iterations);
    }
    barrier();
}
)glsl");

RC VkStress::SetupComputeIMMAShader(string *computeProg, UINT32 *computeOpsPerInnerLoop) const
{
    MASSERT(computeProg);
    MASSERT(computeOpsPerInnerLoop);

    constexpr UINT32 numCoopMatMulAddLWCalls = 9;
    constexpr UINT32 sizeM = 8;
    constexpr UINT32 sizeN = 8;
    constexpr UINT32 sizeK = 32;
    constexpr UINT32 numMuls = 1;
    constexpr UINT32 numAdds = 1;
    *computeOpsPerInnerLoop =
        numCoopMatMulAddLWCalls * sizeM * sizeN * sizeK * (numMuls + numAdds);

    *computeProg = s_VkStressComputeIMMAShader;

    return RC::OK;
}

static const char* const s_VkStressComputeLoadedShader = HS_(R"glsl(
#version 450  // GL version 4.5 assumed, required for uint64_t
#extension GL_ARB_compute_shader: enable
#extension GL_LW_shader_sm_builtins : require  // Needed for gl_SMCountLW
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_ARB_shader_clock : enable // needed for clockARB()
#define MAX_SMS     144

layout (local_size_x = 32, local_size_y = 1) in;

layout(std140, binding=0) uniform parameters
{
    int simWidth;
    int simHeight;
    int outerLoops;
    int innerLoops;
    uint64_t runtimeClks; //maximum time to run the shader
};
layout(std430, binding=1) buffer stats
{
    uint     smLoaded[MAX_SMS];
    uint     smKeepRunning[MAX_SMS];     //keepRunning of thread 0 of each warp.
    uint64_t smStartClock[MAX_SMS];      //clock value when the kernel started
    uint64_t smEndClock[MAX_SMS];        //clock value when the kernel stopped
    uint     smFP16Miscompares[MAX_SMS]; //miscompares using float16_t ops
    uint     smFP32Miscompares[MAX_SMS]; //miscompares using float ops
    uint     smFP64Miscompares[MAX_SMS]; //miscompares using double ops
    uint     smIterations[MAX_SMS];      //exelwtion counter for performance measurements
};
layout(std430, binding=2) buffer Cells { uint64_t cells[]; };

void main()
{
    int gX = int(gl_GlobalIlwocationID.x);// x = 0 - ((local_size_x * WorkgroupSize.x)-1)
    int gY = int(gl_GlobalIlwocationID.y);// y = 0 - ((local_size_y * WorkgroupSize.y)-1)
    //Exit if this is not the first thread
    if ((gX + gY) > 1)
    {
        return;
    }
    uint64_t endClk = clockARB() + runtimeClks;
    uint64_t clk = clockARB();
    //wait for all of the SMs to load some compute work
    uint allSMsLoaded = 0;
    int i = 0;
    //wait for all of the SMs to load some compute work
    while((allSMsLoaded != gl_SMCountLW) && (clk < endClk))
    {
        allSMsLoaded = 0;
        for (i = 0; i < gl_SMCountLW; i++)
        {
            allSMsLoaded += (smLoaded[i] & 0x1);
        }
        clk = clockARB();
    }
}
)glsl");

RC VkStress::SetupComputeLoadedShader()
{
    if (!m_UseCompute)
    {
        return RC::OK;
    }

    if ((m_NumSMs == 0) || (m_NumSMs > MAX_SMS))
    {
        // May need to increase the size of the SCG arrays below.
        Printf(Tee::PriError, "Number of SMs is greater than Compute Shader can handle.\n");
        return RC::SOFTWARE_ERROR;
    }

    m_ComputeLoadedProg = s_VkStressComputeLoadedShader;
    return RC::OK;
}

//-------------------------------------------------------------------------------------------------
// Dump the most recent query results. This is useful for debugging any anomolies between the
// compute, graphics, and raytracing queues. We will print the oldest timestamp first and most
// recent last. Raytracing queries only get printed if raytracing is enabled.
void VkStress::DumpQueryResults()
{
    Printf(Tee::PriNormal,
        "Printing timestamp data for the last %u work submissions(G=Graphics, C=Compute, R=Raytracing, T=Total)\n",
        MAX_NUM_TS_QUERIES);

    string strHeader = Utility::StrPrintf("%-18s %-18s %-10s ",
                                          "G-Start", "G-Stop", "G-Elapsed");
    string lwrrQueryLine;
    for (UINT32 ii = 0; ii < MAX_NUM_TS_QUERIES; ii++)
    {
        UINT64 minQuery = -1;
        UINT64 maxQuery = 0;
        const UINT32 lwrIdx = (m_QueryIdx+ 1 + ii) % MAX_NUM_TS_QUERIES;
        const UINT64* const pGfxResults =
            reinterpret_cast<const UINT64*>(m_GfxTSQueries[lwrIdx]->GetResultsPtr());

        // print the most recent query last.
        const UINT64 elapsedGfx = pGfxResults[1] - pGfxResults[0];

        lwrrQueryLine += Utility::StrPrintf("0x%16llx 0x%16llx %10lld",
            pGfxResults[0], pGfxResults[1], elapsedGfx);

        minQuery = min(min(pGfxResults[0], pGfxResults[1]), minQuery);
        maxQuery = max(max(pGfxResults[0], pGfxResults[1]), maxQuery);
        for (auto &cw : m_ComputeWorks)
        {
            const UINT32 lwrComputeIdx = (cw.m_ComputeQueryIdx + 1 + ii) % MAX_NUM_TS_QUERIES;
            if (ii == 0)
            {
                strHeader += Utility::StrPrintf("%-18s %-18s %-10s %-10s ",
                                                "C-Start", "C-Stop", "C-Elapsed", "G-C Delta");
            }
            const UINT64* const pCmpResults =
                reinterpret_cast<const UINT64*>(cw.m_ComputeTSQueries[lwrComputeIdx]->GetResultsPtr());
            const UINT64 elapsedCmp = pCmpResults[1] - pCmpResults[0];
            const UINT64 deltaNs = elapsedGfx > elapsedCmp ?
                elapsedGfx - elapsedCmp : elapsedCmp - elapsedGfx;
            lwrrQueryLine += Utility::StrPrintf(" 0x%16llx 0x%16llx %10lld %10lld",
                pCmpResults[0], pCmpResults[1], elapsedCmp, deltaNs);

            minQuery = min(min(pCmpResults[0], pCmpResults[1]), minQuery);
            maxQuery = max(max(pCmpResults[0], pCmpResults[1]), maxQuery);
        }

        if (m_Rt.get())
        {
            if (ii == 0)
            {
                strHeader += Utility::StrPrintf("%-18s %-18s %-10s %-10s ",
                                                "R-Start", "R-Stop", "R-Elapsed", "G-R Delta");
            }
            UINT64 rtTS[2] = { m_Rt->GetTSResults(lwrIdx, 0), m_Rt->GetTSResults(lwrIdx, 1) };
            const UINT64 elapsedRt = rtTS[1] = rtTS[0];
            const UINT64 deltaRt = elapsedGfx > elapsedRt ?
                elapsedGfx - elapsedRt : elapsedRt - elapsedGfx;
            lwrrQueryLine += Utility::StrPrintf(" 0x%16llx 0x%16llx %10lld %10lld",
                   rtTS[0], rtTS[1], elapsedRt, deltaRt);

            minQuery = min(min(rtTS[0], rtTS[1]), minQuery);
            maxQuery = max(max(rtTS[0], rtTS[1]), maxQuery);
        }

        if (ii == 0)
        {
            strHeader += Utility::StrPrintf("%-18s %-18s %-10s ",
                                            "T-Start", "T-Stop", "T-Elapsed");
        }
        lwrrQueryLine += Utility::StrPrintf(" 0x%16llx 0x%16llx %10lld",
               minQuery, maxQuery, (maxQuery - minQuery));

        lwrrQueryLine += "\n";
    }
    strHeader += "\n";
    Printf(Tee::PriNormal, strHeader.c_str());
    Printf(Tee::PriNormal, lwrrQueryLine.c_str());
}

//-------------------------------------------------------------------------------------------------
// Dump the output of the Compute data.
// This routine is very helpful in debugging the computer shader. Each thread that runs a computer
// shader can output a single 64 bit value to the m_pSimCells[].
void VkStress::DumpComputeCells(const char * pTitle)
{
    if (!m_UseCompute || !m_PrintComputeCells)
    {
        return;
    }

    UINT32 queueIdx = 0;
    for (auto &cw : m_ComputeWorks)
    {
        if (pTitle)
        {
            Printf(Tee::PriNormal, "%s ComputeQueue = %d\n", pTitle, queueIdx);
        }
        Printf(Tee::PriNormal, "garbage          runtimeClks      lastClk          startClk         endClk           SMID             elapsedClks      KeepRunning(3snapshots)\n"); //$
        //                      0000000000000001 00000000713fb300 0000000102970074 000000009156c1cb 00000001029674cb 0000000000000000 0000000000174cd8 0000000200020002 //$
        const UINT32 sz = cw.m_ComputeParameters.height * cw.m_ComputeParameters.width;

        for (UINT32 y = 0; y < sz;)
        {
            for (UINT32 x = 0; x < 8 && y < sz; x++, y++)
            {
                Printf(Tee::PriNormal, "%016llx ", cw.m_pCells[y]);
            }
            Printf(Tee::PriNormal, "\n");
        }
        Printf(Tee::PriNormal, "\n");
        queueIdx++;
    }
}

// Check to see if the compute shaders have produced consistent results.
RC VkStress::CheckForComputeErrors(UINT32 swapchainIdx)
{
    if (!m_UseCompute)
    {
        return RC::OK;
    }

    UINT32 fp16Miscompares = 0;
    UINT32 fp32Miscompares = 0;
    UINT32 fp64Miscompares = 0;

    // Check the results
    for (auto &cw : m_ComputeWorks)
    {
        for (UINT32 sm = 0; sm < m_NumSMs; sm++)
        {
            fp16Miscompares += cw.m_pScgStats->smFP16Miscompares[sm];
            fp32Miscompares += cw.m_pScgStats->smFP32Miscompares[sm];
            fp64Miscompares += cw.m_pScgStats->smFP64Miscompares[sm];
        }
    }

    m_Miscompares[swapchainIdx].emplace_back(CHECK_COMPUTE_FP16, fp16Miscompares);
    m_Miscompares[swapchainIdx].emplace_back(CHECK_COMPUTE_FP32, fp32Miscompares);
    m_Miscompares[swapchainIdx].emplace_back(CHECK_COMPUTE_FP64, fp64Miscompares);

    if (fp16Miscompares || fp32Miscompares || fp64Miscompares || m_PrintSCGInfo)
    {
        DumpComputeData("SCG Results");
    }

    return RC::OK;
}

void VkStress::DumpComputeData(const char *pTitle)
{
    if (!(m_UseCompute && m_PrintSCGInfo))
    {
        return;
    }

    UINT32 queueIdx = 0;
    for (auto &cw : m_ComputeWorks)
    {
        Printf(Tee::PriNormal, "%s ComputeQueue = %d\n", pTitle, queueIdx);
        Printf(Tee::PriNormal, "Compute parameters queueidx %d: height:%d width:%d runtimeClks:%lld\n"
               ,queueIdx
               ,cw.m_ComputeParameters.height
               ,cw.m_ComputeParameters.width
               ,cw.m_ComputeParameters.runtimeClks);

        UINT32 count = 0;
        Printf(Tee::PriNormal, "%-18s:", "smLoaded");
        for (UINT32 i = 0; i < m_NumSMs; i++)
        {
            Printf(Tee::PriNormal, "%02d:%011u ", i, cw.m_pScgStats->smLoaded[i]);
            count += cw.m_pScgStats->smLoaded[i];
        }
        Printf(Tee::PriNormal, "Tot:%011u\n", count);

        count = 0;
        Printf(Tee::PriNormal, "%-18s:", "smFP16Miscompares");
        for (UINT32 i = 0; i < m_NumSMs; i++)
        {
            Printf(Tee::PriNormal, "%02d:%011u ", i, cw.m_pScgStats->smFP16Miscompares[i]);
            count += cw.m_pScgStats->smFP16Miscompares[i];
        }
        Printf(Tee::PriNormal, "Tot:%011u\n", count);

        count = 0;
        Printf(Tee::PriNormal, "%-18s:", "smFP32Miscompares");
        for (UINT32 i = 0; i < m_NumSMs; i++)
        {
            Printf(Tee::PriNormal, "%02d:%011u ", i, cw.m_pScgStats->smFP32Miscompares[i]);
            count += cw.m_pScgStats->smFP32Miscompares[i];
        }
        Printf(Tee::PriNormal, "Tot:%011u\n", count);

        count = 0;
        Printf(Tee::PriNormal, "%-18s:", "smFP64Miscompares ");
        for (UINT32 i = 0; i < m_NumSMs; i++)
        {
            Printf(Tee::PriNormal, "%02d:%011u ", i, cw.m_pScgStats->smFP64Miscompares[i]);
            count += cw.m_pScgStats->smFP64Miscompares[i];
        }
        Printf(Tee::PriNormal, "Tot:%011u\n", count);

        queueIdx++;
    }
}

//-------------------------------------------------------------------------------------------------
// Create a descriptor layout to match the Compute shader resources.
RC VkStress::SetupComputeDescriptors(ComputeWork *pCW)
{
    RC rc;

    const UINT32 maxDescriptorSets = 1;
    const UINT32 numBindings = 4;
    // List all the descriptors used
    const vector<VkDescriptorPoolSize> descPoolSizeInfo =
    { //       type,                         count
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } // for Parameters
       ,{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } // for Stats
       ,{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } // for Cells[]
       ,{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } // for KeepRunning
    };

    array<VkDescriptorSetLayoutBinding, numBindings> layoutBindings = {};
    for (UINT32 bindIdx = 0; bindIdx < numBindings; bindIdx++)
    {
        layoutBindings[bindIdx].binding = bindIdx;
        layoutBindings[bindIdx].pImmutableSamplers = nullptr;
        layoutBindings[bindIdx].descriptorCount = 1;
        layoutBindings[bindIdx].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[bindIdx].descriptorType = descPoolSizeInfo[bindIdx].type;
    }

    CHECK_VK_TO_RC(pCW->m_ComputeDescriptorInfo->CreateDescriptorSetLayout(
        0 // firstIndex
        ,static_cast<UINT32>(layoutBindings.size())
        ,layoutBindings.data()));

        // Create descriptor pool
    CHECK_VK_TO_RC(pCW->m_ComputeDescriptorInfo->CreateDescriptorPool(
        maxDescriptorSets,
        descPoolSizeInfo));

    //allocate descriptor sets
    CHECK_VK_TO_RC(pCW->m_ComputeDescriptorInfo->AllocateDescriptorSets(0, maxDescriptorSets, 0));

    //update the descriptors
    pCW->m_ComputeWriteDescriptorSets.resize(numBindings);

    // Descriptor 0: UBO parameters
    VkDescriptorBufferInfo bufferInfo = pCW->m_ComputeUBOParameters->GetBufferInfo();
    pCW->m_ComputeWriteDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pCW->m_ComputeWriteDescriptorSets[0].pNext = nullptr;
    pCW->m_ComputeWriteDescriptorSets[0].dstSet = pCW->m_ComputeDescriptorInfo->GetDescriptorSet(0);
    pCW->m_ComputeWriteDescriptorSets[0].dstArrayElement = 0;
    pCW->m_ComputeWriteDescriptorSets[0].dstBinding = 0;
    pCW->m_ComputeWriteDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pCW->m_ComputeWriteDescriptorSets[0].descriptorCount = 1;
    pCW->m_ComputeWriteDescriptorSets[0].pBufferInfo = &bufferInfo;

    // Descriptor 1: SBO stats
    VkDescriptorBufferInfo statsInfo = pCW->m_ComputeSBOStats->GetBufferInfo();
    pCW->m_ComputeWriteDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pCW->m_ComputeWriteDescriptorSets[1].pNext = nullptr;
    pCW->m_ComputeWriteDescriptorSets[1].dstSet = pCW->m_ComputeDescriptorInfo->GetDescriptorSet(0);
    pCW->m_ComputeWriteDescriptorSets[1].dstArrayElement = 0;
    pCW->m_ComputeWriteDescriptorSets[1].dstBinding = 1;
    pCW->m_ComputeWriteDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pCW->m_ComputeWriteDescriptorSets[1].descriptorCount = 1;
    pCW->m_ComputeWriteDescriptorSets[1].pBufferInfo = &statsInfo;

    // Descriptor 2: SBO cells
    VkDescriptorBufferInfo info = pCW->m_ComputeSBOCells->GetBufferInfo();
    pCW->m_ComputeWriteDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pCW->m_ComputeWriteDescriptorSets[2].pNext = nullptr;
    pCW->m_ComputeWriteDescriptorSets[2].dstSet = pCW->m_ComputeDescriptorInfo->GetDescriptorSet(0);
    pCW->m_ComputeWriteDescriptorSets[2].dstArrayElement = 0;
    pCW->m_ComputeWriteDescriptorSets[2].dstBinding = 2;
    pCW->m_ComputeWriteDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pCW->m_ComputeWriteDescriptorSets[2].descriptorCount = 1;
    pCW->m_ComputeWriteDescriptorSets[2].pBufferInfo = &info;

    // Descriptor 3: KeepRunning flag
    VkDescriptorBufferInfo keepRunningInfo = pCW->m_ComputeKeepRunning->GetBufferInfo();
    pCW->m_ComputeWriteDescriptorSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pCW->m_ComputeWriteDescriptorSets[3].pNext = nullptr;
    pCW->m_ComputeWriteDescriptorSets[3].dstSet = pCW->m_ComputeDescriptorInfo->GetDescriptorSet(0);
    pCW->m_ComputeWriteDescriptorSets[3].dstArrayElement = 0;
    pCW->m_ComputeWriteDescriptorSets[3].dstBinding = 3;
    pCW->m_ComputeWriteDescriptorSets[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pCW->m_ComputeWriteDescriptorSets[3].descriptorCount = 1;
    pCW->m_ComputeWriteDescriptorSets[3].pBufferInfo = &keepRunningInfo;

    pCW->m_ComputeDescriptorInfo->UpdateDescriptorSets(pCW->m_ComputeWriteDescriptorSets.data()
        ,static_cast<UINT32>(pCW->m_ComputeWriteDescriptorSets.size()));

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC VkStress::CleanupComputeResources()
{
    StickyRC rc;
    if (!m_UseCompute)
    {
        return rc;
    }

    for (auto &cw : m_ComputeWorks)
    {
        cw.m_ComputeWriteDescriptorSets.clear();
    }

    if (m_pVulkanDev)
    {
        if (m_ComputeLoadedShader.get())
        {
            m_ComputeLoadedShader->Cleanup();
        }

        for (auto &cs : m_ComputeShaders)
        {
            cs->Cleanup();
        }
        m_ComputeShaders.clear();

        for (auto &cw : m_ComputeWorks)
        {
            if (cw.m_ComputeLoadedPipeline.get())
            {
                cw.m_ComputeLoadedPipeline->Cleanup();
            }
            for (const auto& sem : cw.m_ComputeLoadedSemaphores)
            {
                m_pVulkanDev->DestroySemaphore(sem);
            }
            if (cw.m_ComputeDescriptorInfo.get())
            {
                rc = VkUtil::VkErrorToRC(cw.m_ComputeDescriptorInfo->Cleanup());
            }
            for (const auto& cb : cw.m_ComputeLoadedCmdBuffers)
            {
                if (cb.get())
                {
                    cb->FreeCmdBuffer();
                }
            }
            if (cw.m_ComputeUBOParameters.get())
            {
                cw.m_ComputeUBOParameters->Cleanup();
            }
            if (cw.m_ComputeSBOCells.get())
            {
                if (cw.m_ComputeSBOCells->IsMapped())
                {
                    rc = VkUtil::VkErrorToRC(cw.m_ComputeSBOCells->Unmap());
                }
                cw.m_ComputeSBOCells->Cleanup();
            }
            if (cw.m_ComputeSBOStats.get())
            {
                if (cw.m_ComputeSBOStats->IsMapped())
                {
                    rc = VkUtil::VkErrorToRC(cw.m_ComputeSBOStats->Unmap());
                }
                cw.m_ComputeSBOStats->Cleanup();
            }
            if (cw.m_ComputeKeepRunning.get())
            {
                cw.m_ComputeKeepRunning->Cleanup();
            }
            if (cw.m_ComputePipeline.get())
            {
                cw.m_ComputePipeline->Cleanup();
            }
            for (const auto& fence : cw.m_ComputeFences)
            {
                m_pVulkanDev->DestroyFence(fence);
            }
            for (const auto& sem : cw.m_ComputeSemaphores)
            {
                m_pVulkanDev->DestroySemaphore(sem);
            }
            for (const auto& cb : cw.m_ComputeCmdBuffers)
            {
                if (cb.get())
                {
                    cb->FreeCmdBuffer();
                }
            }
            if (cw.m_ComputeCmdPool.get())
            {
                cw.m_ComputeCmdPool->DestroyCmdPool();
            }
        }
        m_ComputeWorks.clear();
    }
    return rc;
}

RC VkStress::SetupWorkController()
{
    RC rc;

    switch (static_cast<PulseMode>(m_PulseMode))
    {
        case NO_PULSE:
        case TILE_MASK_USER:
        case TILE_MASK_GPC:
        case TILE_MASK_GPC_SHUFFLE:
        {
            if (!m_DrawJobTimeNs)
            {
                Printf(Tee::PriError, "DrawJobTimeNs must be non-zero\n");
                return RC::ILWALID_ARGUMENT;
            }
            m_WorkController = make_unique<StaticWorkController>(this, m_DrawJobTimeNs);
            break;
        }
        case FMAX:
        case SWEEP:
        case SWEEP_LINEAR:
        {
            m_WorkController = make_unique<SweepWorkController>(this);
            SweepWorkController* pSweep = dynamic_cast<SweepWorkController*>(m_WorkController.get()); //$
            MASSERT(pSweep); // Coverity CID 5864260.
            if (m_LowHz <= 0.0 || (m_LowHz > m_HighHz && m_HighHz != MAX_FREQ_POSSIBLE))
            {
                Printf(Tee::PriError, "LowHz must be greater than 0 and less than HighHz\n");
                return RC::ILWALID_ARGUMENT;
            }
            if (m_HighHz <= 0.0 && m_HighHz != MAX_FREQ_POSSIBLE)
            {
                Printf(Tee::PriError, "HighHz must be greater than 0\n");
                return RC::ILWALID_ARGUMENT;
            }

            if (m_DutyPct <= 0.0 || m_DutyPct >= 100.0)
            {
                Printf(Tee::PriError, "DutyPct must be in the range (0.0, 100.0)\n");
                return RC::ILWALID_ARGUMENT;
            }
            pSweep->SetDutyPct(m_DutyPct);

            pSweep->SetLowHz(m_LowHz); // Call after SetDutyPct as the duty
                                       // value is used in callwlations.
            pSweep->SetHighHz(m_HighHz);

            if (m_PulseMode == SWEEP)
            {
                if (!m_OctavesPerSecond)
                {
                    Printf(Tee::PriError, "OctavesPerSecond must be non-zero\n");
                    return RC::ILWALID_ARGUMENT;
                }
                pSweep->SetOctavesPerSecond(m_OctavesPerSecond);
            }
            else if (m_PulseMode == SWEEP_LINEAR)
            {
                if (!(m_StepHz && m_StepTimeUs))
                {
                    Printf(Tee::PriError, "StepHz and StepTimeUs must be non-zero\n");
                    return RC::ILWALID_ARGUMENT;
                }
                pSweep->SetStepHz(m_StepHz);
                pSweep->SetStepTimeUs(m_StepTimeUs);
                pSweep->SetLinearSweep(true);
            }
            break;
        }
        case TILE_MASK:
        case TILE_MASK_CONSTANT_DUTY:
        {
            m_WorkController = make_unique<TileMaskWorkController>(this, m_DrawJobTimeNs);
            TileMaskWorkController* pWc = dynamic_cast<TileMaskWorkController*>(
                m_WorkController.get());
            MASSERT(pWc);
            pWc->SetDutyPct(m_DutyPct);
            break;
        }
        case MUSIC:
        {
#ifndef VULKAN_STANDALONE
            m_WorkController = make_unique<MusicController>(this);
            MusicController* pMusic = dynamic_cast<MusicController*>(m_WorkController.get());
            NoteController::Notes notes;
            if (m_JsNotes.empty())
            {
                notes.push_back({ 1046.5, 30.0, 250 });
                notes.push_back({ 1318.5, 30.0, 250 });
                notes.push_back({ 1568.0, 30.0, 250 });
                notes.push_back({ 2093.0, 30.0, 250 });
            }
            else
            {
                // Parse m_JsNotes, which is an array of JSObjects.
                //
                // Rules:
                //  1) FreqHz and LengthMs are required.
                //  2) DutyPct and Lyric are optional.
                JavaScriptPtr pJs;
                for (const auto& note : m_JsNotes)
                {
                    JSObject* pJsNote;
                    CHECK_RC(pJs->FromJsval(note, &pJsNote));
                    NoteController::Note newNote;
                    CHECK_RC(pJs->GetProperty(pJsNote, "FreqHz", &newNote.FreqHz));
                    CHECK_RC(pJs->GetProperty(pJsNote, "LengthMs", &newNote.LengthMs));
                    newNote.DutyPct = m_DutyPct;
                    pJs->GetProperty(pJsNote, "DutyPct", &newNote.DutyPct);
                    pJs->GetProperty(pJsNote, "Lyric", &newNote.Lyric);
                    if (newNote.FreqHz <= 0.0 ||
                        newNote.DutyPct <= 0.0 ||
                        newNote.DutyPct >= 100.0 ||
                        newNote.LengthMs == 0)
                    {
                        Printf(Tee::PriError, "Invalid note: %s\n", newNote.ToString().c_str());
                        return RC::BAD_PARAMETER;
                    }
                    notes.push_back(newNote);
                }
            }
            pMusic->SetNotes(notes);
            break;
#else
            // We don't support JsArray/JSObject in standalone VkTest
            Printf(Tee::PriError, "MUSIC PulseMode is not supported\n");
            return RC::UNSUPPORTED_FUNCTION;
#endif // VULKAN_STANDALONE
        }
        case RANDOM:
        {
            m_WorkController = make_unique<RandomNoteController>(this);
            RandomNoteController* pController =
                dynamic_cast<RandomNoteController*>(m_WorkController.get());
            pController->SetMinFreqHz(m_RandMinFreqHz);
            pController->SetMaxFreqHz(m_RandMaxFreqHz);
            pController->SetMinDutyPct(m_RandMinDutyPct);
            pController->SetMaxDutyPct(m_RandMaxDutyPct);
            pController->SetMinLengthMs(m_RandMinLengthMs);
            pController->SetMaxLengthMs(m_RandMaxLengthMs);
            break;
        }
        default:
            Printf(Tee::PriError, "Unknown PulseMode\n");
            return RC::ILWALID_ARGUMENT;
    }

    // Configure the FMAX controller
    if (m_PulseMode == FMAX || m_PulseMode == TILE_MASK)
    {
        if ((m_FMaxConstTargetMHz != 0) && (m_FMaxFraction != 0.0))
        {
            Printf(Tee::PriError, "FMaxConstTargetMHz and FMaxFraction can't be both non-zero\n");
            return RC::ILWALID_ARGUMENT;
        }
        if ((m_FMaxConstTargetMHz != 0) && (m_FMaxPerfTarget != PERF_TARGET_NONE))
        {
            Printf(Tee::PriError,
                "FMaxConstTargetMHz and FMaxPerfTarget can't be both active\n");
            return RC::ILWALID_ARGUMENT;
        }
        if (m_PulseMode == FMAX)
        {
            if (m_FMaxCtrlHz == 0)
            {
                m_FMaxCtrlHz = 100;
            }
            SweepWorkController* pSweep = dynamic_cast<SweepWorkController*>(m_WorkController.get()); //$
            MASSERT(pSweep);
            pSweep->SetLowHz(m_FMaxCtrlHz);
            pSweep->SetHighHz(m_FMaxCtrlHz);
        }
        // Ignore errors from GetClock, the FMAX controller can start with 0:
        m_FMaxIdleGpc = 0;
#ifndef VULKAN_STANDALONE
        GetBoundGpuSubdevice()->GetClock(Gpu::ClkGpc, &m_FMaxIdleGpc, nullptr, nullptr, nullptr);
#endif
        if (m_FMaxForcedDutyInc < 0.0f)
        {
            Printf(Tee::PriError, "FMaxForcedDutyInc must not be negative\n");
            return RC::ILWALID_ARGUMENT;
        }
        if (m_FMaxFraction < 0.0f || m_FMaxFraction > 100.0f)
        {
            Printf(Tee::PriError, "FMaxFraction must be between 0.0 and 100.0\n");
            return RC::ILWALID_ARGUMENT;
        }
    }

    m_WorkController->SetPrintPri(m_PrintTuningInfo ? Tee::PriNormal : Tee::PriSecret);
    if (!m_MovingAvgSampleWindow)
    {
        Printf(Tee::PriError, "MovingAvgSampleWindow must be non-zero\n");
        return RC::ILWALID_ARGUMENT;
    }
    m_WorkController->SetWindowSize(m_MovingAvgSampleWindow);

#ifndef VULKAN_STANDALONE
    if (GetBoundGpuSubdevice()->IsEmOrSim())
    {
        // Shorten the test by only sending one quadframe at a time
        m_WorkController->ForceNumFrames(4);
    }
#endif

    CHECK_RC(m_WorkController->Setup());

    const PulseMode pm = static_cast<PulseMode>(m_PulseMode);
    if (m_FramesPerSubmit)
    {
        m_WorkController->ForceNumFrames(m_FramesPerSubmit);
    }
    else if (pm == NO_PULSE ||
        pm == TILE_MASK ||
        pm == TILE_MASK_CONSTANT_DUTY ||
        pm == TILE_MASK_GPC_SHUFFLE)
    {
        m_WorkController->SetRoundToQuadFrame(true);
    }
    return rc;
}

RC VkStress::TuneWorkload(UINT64 drawJobIdx)
{
    RC rc;

    const UINT64 lastQueryIdx = m_QueryIdx ? m_QueryIdx-1 : MAX_NUM_TS_QUERIES-1;

    const UINT64 * const pGfxResults =
        reinterpret_cast<const UINT64*>(m_GfxTSQueries[lastQueryIdx]->GetResultsPtr());
    const UINT64 elapsedGfx = pGfxResults[1] - pGfxResults[0];

    DumpComputeCells(nullptr);

    if (m_PrintTuningInfo && m_UseCompute && !m_AsyncCompute)
    {
        UINT32 queueIdx = 0;
        for (auto &cw : m_ComputeWorks)
        {
            const UINT64 lastComputeQueryIdx = cw.m_ComputeQueryIdx ?
                cw.m_ComputeQueryIdx-1 : MAX_NUM_TS_QUERIES-1;

            const UINT64 * pCmpResults = reinterpret_cast<const UINT64 *>
                (cw.m_ComputeTSQueries[lastComputeQueryIdx]->GetResultsPtr());
            UINT64 elapsedCmp = pCmpResults[1] - pCmpResults[0];

            UINT32 computeJobsCompleted = 0;
            UINT32 computeJobsTimedOut = 0;
            for (UINT32 sm = 0; sm < m_NumSMs; sm++)
            {
                if ((cw.m_pScgStats->smKeepRunning[sm] & 0x03) == 0x03)
                {
                    computeJobsCompleted++;
                }
                else
                {
                    computeJobsTimedOut++;
                }
            }

            Printf(Tee::PriNormal,
                "Cmp%d(start:0x%09llx stop:0x%09llx elapsed:%09lld delta:%09lld"
                " runtimeClks:%09lld timeout:%03d complete:%05d)\n"
                ,queueIdx
                ,pCmpResults[0]
                ,pCmpResults[1]
                ,elapsedCmp
                ,elapsedGfx - elapsedCmp
                ,cw.m_ComputeParameters.runtimeClks
                ,computeJobsTimedOut
                ,computeJobsCompleted);

            queueIdx++;
        }
    }

    m_WorkController->RecordGfxTimeNs(pGfxResults[0], pGfxResults[1]);
    // We need atleast 2 completed draw jobs to prime the controller.
    if (drawJobIdx >= 2)
    {
        CHECK_RC(m_WorkController->Evaluate());
    }
    m_FramesPerSubmit = m_WorkController->GetNumFrames();

    if (!m_FramesPerSubmit)
    {
        Printf(Tee::PriError, "WorkController returned 0 FramesPerSubmit\n");
        return RC::SOFTWARE_ERROR;
    }

    // Update the fallback runtimeClks parameter in case we don't see the buffer updates.
    // This prevents the shader from running longer than we want. Callwlate the number of
    // clocks based on what we observe from the shader and add a 5% margin for variation.
    // Note: The user can override this by setting m_ComputeRuntimeClks for debugging purposes
    if (m_UseCompute && !m_AsyncCompute)
    {
        for (UINT32 computeQueueIdx = 0; computeQueueIdx < m_ComputeWorks.size(); computeQueueIdx++)
        {
            TuneComputeWorkload(computeQueueIdx);
        }
    }
    return rc;
}

void VkStress::TuneComputeWorkload(UINT32 computeQueueIdx)
{
    auto &cw = m_ComputeWorks[computeQueueIdx];
    UINT64 workTimeNs = m_WorkController->GetTargetWorkTimeNs();

    if (m_ComputeRuntimeClks[computeQueueIdx])
    {
        cw.m_ComputeParameters.runtimeClks = m_ComputeRuntimeClks[computeQueueIdx];
    }
    else
    {
        const UINT64 lastComputeQueryIdx = cw.m_ComputeQueryIdx ?
            cw.m_ComputeQueryIdx-1 : MAX_NUM_TS_QUERIES-1;
        const UINT64 * pCmpResults = reinterpret_cast<const UINT64 *>
            (cw.m_ComputeTSQueries[lastComputeQueryIdx]->GetResultsPtr());
        UINT64 elapsedCmp = pCmpResults[1] - pCmpResults[0];

        double clksPerNs =
            (double)(cw.m_pScgStats->smEndClock[0] - cw.m_pScgStats->smStartClock[0]) /
            (double)elapsedCmp;
        UINT64 margin = static_cast<UINT64>(workTimeNs * 0.05f);
        cw.m_ComputeParameters.runtimeClks =
            static_cast<UINT64>(ceil(clksPerNs * workTimeNs)) + margin;
    }
}

RC VkStress::AddMeshletExtensions(vector<string>& extNames)
{
    if (!m_UseMeshlets)
        return RC::OK;

    // This extension is required for meshlets. Fail if it is not supported.
    if (!m_pVulkanDev->HasExt(VulkanDevice::ExtVK_LW_mesh_shader))
    {   // enable this extension
        Printf(Tee::PriError, "The required extension \"VK_LW_mesh_shader\" is not supported!\n");
        return RC::MODS_VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    extNames.push_back("VK_LW_mesh_shader");
    return RC::OK;
}

RC VkStress::AddHTexExtensions(vector<string>& extNames)
{
    if (!m_UseHTex)
        return RC::OK;

    // This feature is optionally supported. If it's not present then just disable HTex.
    if (m_pVulkanDev->HasExt(VulkanDevice::ExtVK_LW_corner_sampled_image))
    {
        extNames.push_back("VK_LW_corner_sampled_image");
    }
    else
    {
        VerbosePrintf("VK_LW_corner_sampled image is not supported. Disabling HTex\n");
        m_UseHTex = false;
    }
    return RC::OK;
}

RC VkStress::AddComputeExtensions(vector<string>& extNames)
{
    if (!m_UseCompute)
        return RC::OK;
    StickyRC rc;
    if (m_pVulkanDev->HasExt(VulkanDevice::ExtVK_LW_shader_sm_builtins))
    {
        extNames.push_back("VK_LW_shader_sm_builtins");
    }
    else
    {
        Printf(Tee::PriError,
               "The required extensions \"VK_LW_shader_sm_builtins\" is not supported!\n");
        rc = RC::MODS_VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    if (m_pVulkanDev->HasExt(VulkanDevice::ExtVK_KHR_shader_clock))
    {
        extNames.push_back("VK_KHR_shader_clock");
    }
    else
    {
        Printf(Tee::PriError,
               "The required extensions \"VK_KHR_shader_clock\" is not supported!\n");
        rc = RC::MODS_VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    if (m_pVulkanDev->HasExt(VulkanDevice::ExtVK_KHR_shader_float16_int8))
    {
        extNames.push_back("VK_KHR_shader_float16_int8");
    }
    else
    {
        Printf(Tee::PriError,
               "The required extensions \"VK_KHR_shader_float16_int8\" is not supported!\n");
        rc = RC::MODS_VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    if (m_pVulkanDev->HasExt(VulkanDevice::ExtVK_LW_cooperative_matrix))
    {
        extNames.push_back("VK_LW_cooperative_matrix");
    }

    return rc;
}

VkStress::StaticWorkController::StaticWorkController(VkStress* pTest, UINT64 drawJobTimeNs) :
    WorkController(pTest)
{
    SetDrawJobTimeNs(drawJobTimeNs);
}

void VkStress::StaticWorkController::SetDrawJobTimeNs(UINT64 drawJobTimeNs)
{
    m_DrawJobTimeNs = drawJobTimeNs;
    SetTargetWorkTimeNs(m_DrawJobTimeNs);
}

RC VkStress::WorkController::Setup()
{
    auto pTestConfig = m_pVkStress->GetTestConfiguration();
    const UINT64 pixelsPerFrame =
        pTestConfig->DisplayWidth() * pTestConfig->DisplayHeight() * m_pVkStress->GetSampleCount();

    // This magic value comes from looking at the measured FPS values on
    // Turing at different gpcclk and GPC floorsweeping configs.
    const FLOAT64 pixelsPerShaderPerClk = m_pVkStress->GetZeroFb() ? 0.03235294 : 0.3235294;

    UINT64 gpcclkHz = 1'000'000'000ULL; // 1GHz
    UINT32 numSMs = m_pVkStress->GetNumGfxSMs();
#ifndef VULKAN_STANDALONE
    if (!Platform::IsVirtFunMode())
    {
        const RC rc = m_pVkStress->GetBoundGpuSubdevice()->GetClock(
            Gpu::ClkGpc, &gpcclkHz, nullptr, nullptr, nullptr);
        if (rc != RC::OK)
        {
            Printf(Tee::PriWarn, "Failed to get GPC clock in work controller, assuming 1 GHz\n");
        }
    }
#endif

    const FLOAT64 estimatedPixelsPerSecond = pixelsPerShaderPerClk * numSMs * gpcclkHz;
    const FLOAT64 estimatedFPS = estimatedPixelsPerSecond / pixelsPerFrame;
    Printf(Tee::PriLow, "Estimated PPS=%.1f, FPS=%.1f\n",
        estimatedPixelsPerSecond, estimatedFPS);

    // We require m_TargetWorkTimeNs to already be initialized
    MASSERT(m_TargetWorkTimeNs);
    m_OutputFrames = estimatedFPS * (m_TargetWorkTimeNs / 1e9);

    // There are several things that will cap performance (e.g. power-capping)
    // So somewhat arbitrarily reduce the initial estimate by half to be on
    // the safer side.
    m_OutputFrames /= 2;
    m_OutputFrames = max(1.0, m_OutputFrames);
    Printf(Tee::PriLow, "Initial FramesPerSubmit=%.1f\n", m_OutputFrames);

    return RC::OK;
}

UINT32 VkStress::WorkController::GetNumFrames() const
{
    // For non-pulsing versions, m_NumFrames is allowed to be between 0.0 and 1.0.
    // This can happen on smaller chips when VkStress has been configured with
    // settings that can severely limit draw calls/second (e.g. large render target
    // or MSAA). In this case, we will round up and send a single frame.
    // Otherwise, round m_OutputFrames to the nearest integer.
    UINT32 outputFrames = static_cast<UINT32>(max(round(m_OutputFrames), 1.0));

    if (m_RoundToQuadFrame)
    {
        // Round down to the nearest multiple of 4. Always output at least 4.
        return max(outputFrames & ~0x3U, 4U);
    }

    return outputFrames;
}

void VkStress::WorkController::RecordGfxTimeNs(UINT64 beginNs, UINT64 endNs)
{
    m_PrevGfxTimes = m_LwrrGfxTimes;
    m_LwrrGfxTimes.beginNs = beginNs;
    m_LwrrGfxTimes.endNs = endNs;
}
void VkStress::WorkController::PrintStats()
{
    const float avgLatency = m_Stats.count ?
        (m_Stats.totCmdLatencyNs / m_Stats.count) / 1000.0f : 0;
    Printf(m_PrintPri, "MinCmdLatency:%6.3fus MaxCmdLatency:%6.3fus AvgCmdLatency:%6.3fus\n",
                  m_Stats.minCmdLatencyNs / 1000.0f,
                  m_Stats.maxCmdLatencyNs / 1000.0f,
                  avgLatency);

}
RC VkStress::WorkController::Evaluate()
{
    RC rc;

    DEFER
    {
        if (rc != RC::OK || m_PrintPri == Tee::PriNormal)
        {
            PrintDebugInfo();
        }
    };

    m_TargetWorkTimeNs = CallwlateWorkTimeNs();
    m_TargetDelayTimeNs = CallwlateDelayTimeNs();
    if (!m_TargetWorkTimeNs || (!m_TargetDelayTimeNs && IsPulsing()))
    {
        Printf(Tee::PriError,
            "WorkController: invalid target work (%llu) and/or delay (%llu) time\n",
            m_TargetWorkTimeNs, m_TargetDelayTimeNs);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    m_ObservedGfxTimeNs = m_LwrrGfxTimes.endNs - m_LwrrGfxTimes.beginNs;
    m_ObservedLatencyTimeNs = m_PrevGfxTimes.endNs != 0 ?
        m_LwrrGfxTimes.beginNs - m_PrevGfxTimes.endNs : 0;

    // If the DrawJob takes more than 100 seconds (arbitrary), report an error
    // as this should never happen outside of emulation/simulation.
    if (!m_ObservedGfxTimeNs || m_ObservedGfxTimeNs > 100000000000)
    {
        Tee::Priority pri = Tee::PriError;

#if !defined(VULKAN_STANDALONE) && !defined(DIRECTAMODEL)
        rc = RC::MODS_VK_GENERIC_ERROR;
        const auto pGpuSub = m_pVkStress->GetBoundGpuSubdevice();
        if ((pGpuSub->IsEmOrSim() && m_ObservedGfxTimeNs) || pGpuSub->HasBug(2114920))
        {
            pri = Tee::PriSecret;
            rc.Clear();
        }
#endif

        Printf(pri, "Invalid GfxTimeNs=0x%llx\n", m_ObservedGfxTimeNs);
        return rc;
    }

    // Solve equation: oldOutputFrames / gfxTime = predictedOutputFrames / targetTime
    m_PredictedOutputFrames = m_OutputFrames / m_ObservedGfxTimeNs * m_TargetWorkTimeNs;
    if (!m_ForceOutputFrames)
    {
        m_OutputFrames = (m_OutputFrames * (m_WindowSize - 1) + m_PredictedOutputFrames) /
            m_WindowSize;

        // If not dithering, choose the lower value as it will result in the
        // higher frequency. We want to always meet or exceed the target frequency.
        if (!m_Dither)
        {
            m_OutputFrames = floor(m_OutputFrames);
        }
    }

    if (IsPulsing())
    {
        // The first time the controller runs, we will only have one valid
        // timestamp query. We need two timestamps to see the effective delay.
        // So the first time the controller runs, we will skip adjusting
        // OutputDelayTimeNs.
        if (m_ObservedLatencyTimeNs)
        {
            m_PredictedDelayNs = static_cast<FLOAT64>(m_OutputDelayTimeNs) /
                m_ObservedLatencyTimeNs * m_TargetDelayTimeNs;
            m_OutputDelayTimeNs = static_cast<UINT64>(round(static_cast<FLOAT64>(
                (m_OutputDelayTimeNs * (m_WindowSize - 1) + m_PredictedDelayNs))) / m_WindowSize);

            // Clamp OutputDelayTimeNs to prevent it from reaching 0 and
            // excessively large values when pulsing up to the max frequency.
            m_OutputDelayTimeNs = max(m_OutputDelayTimeNs, MIN_OUTPUT_DELAY_NS / 2);
            m_OutputDelayTimeNs = min(m_OutputDelayTimeNs, 2 * m_TargetDelayTimeNs);

            // Assume that we cannot aclwrately delay for less than 1us by
            // using Utility::DelayNS()
            if (m_OutputDelayTimeNs < MIN_OUTPUT_DELAY_NS && !IgnoreWaveformAclwracy())
            {
                Printf(Tee::PriError, "Workload latency is too large\n");
                CHECK_RC(RC::CANNOT_MEET_WAVEFORM);
            }
        }

        // Clamp OutputFrames to prevent it from reaching 0 when pulsing up to the
        // max frequency.
        m_OutputFrames = max(m_OutputFrames, 0.25);

        // If we have to send less than one frame, then we cannot send a small-
        // enough workload to the GPU to meet TargetWorkTimeNs. We should fail
        // rather than misleading the user into believing that they are running at a
        // higher frequency.
#ifndef VULKAN_STANDALONE
        if (m_OutputFrames < MIN_OUTPUT_FRAMES && !IgnoreWaveformAclwracy())
        {
            Printf(Tee::PriError, "Cannot send small-enough workload\n");
            CHECK_RC(RC::CANNOT_MEET_WAVEFORM);
        }
#endif
    }
    // Collect some stats
    if (m_ObservedLatencyTimeNs < 100000000)
    {
        m_Stats.minCmdLatencyNs = min(m_Stats.minCmdLatencyNs, m_ObservedLatencyTimeNs);
        m_Stats.maxCmdLatencyNs = max(m_Stats.maxCmdLatencyNs, m_ObservedLatencyTimeNs);
        m_Stats.totCmdLatencyNs += m_ObservedLatencyTimeNs;
        m_Stats.count++;
    }

    return rc;
}

void VkStress::WorkController::ForceNumFrames(UINT32 numFrames)
{
    m_OutputFrames = static_cast<FLOAT64>(numFrames);
    m_ForceOutputFrames = true;
}

void VkStress::WorkController::PrintDebugInfo()
{
    Printf(m_PrintPri, "WorkController debug info:\n");
    Printf(m_PrintPri, " Targets: WorkTimeNs=%09llu DelayTimeNs=%09llu\n",
        m_TargetWorkTimeNs, m_TargetDelayTimeNs);

    const UINT64 totalTimeNs = m_ObservedGfxTimeNs + m_ObservedLatencyTimeNs;
    const FLOAT64 totalTimeSec = totalTimeNs / 1e9;
    const FLOAT64 hz = 1 / totalTimeSec;
    const FLOAT64 duty = static_cast<FLOAT64>(m_ObservedGfxTimeNs) / totalTimeNs * 100.0;

    Printf(m_PrintPri,
        " Observed: GfxTimeNs=%09llu begin=%llu end=%llu CmdLatencyNs=%09llu Hz=%.3f Duty=%.3f%%\n"
        ,m_ObservedGfxTimeNs, m_LwrrGfxTimes.beginNs, m_LwrrGfxTimes.endNs
        ,m_ObservedLatencyTimeNs, hz, duty);
    Printf(m_PrintPri,
        " Outputs: Frames=%.3f PredictedF=%.3f DelayTimeNs=%llu PredictedDelayTimeNs=%.3f\n",
        m_OutputFrames, m_PredictedOutputFrames, m_OutputDelayTimeNs, m_PredictedDelayNs);
}

bool VkStress::WorkController::IsPulsing() const
{
    MASSERT(m_pVkStress);
    return m_pVkStress->IsPulsing();
}

void VkStress::WorkController::SetOutputDelayTimeNs(UINT64 timeNs)
{
    m_OutputDelayTimeNs = timeNs;
}

UINT64 VkStress::StaticWorkController::CallwlateWorkTimeNs()
{
    return m_DrawJobTimeNs;
}

UINT64 VkStress::StaticWorkController::CallwlateDelayTimeNs()
{
    // Not pulsing, no delays!
    return 0;
}

void VkStress::SweepWorkController::SetLowHz(FLOAT64 hz)
{
    m_LowHz = hz;
    m_LwrrHz = hz;
    SetTargetWorkTimeNs(static_cast<UINT64>((1.0 / hz) * (m_DutyPct / 100.0) * 1e9));

    // Guess that we should delay for as long as the target delay time
    SetOutputDelayTimeNs(
        max(static_cast<UINT64>(round((1.0 / hz) * (1.0 - m_DutyPct / 100.0) * 1e9)), 1ULL));
}

UINT64 VkStress::SweepWorkController::CallwlateWorkTimeNs()
{
    const SweepState lwrrState = m_SweepStates[m_StateIdx];
    if (lwrrState == SweepState::HOLD)
    {
        Hold();
    }
    else
    {
        Sweep(lwrrState);
    }

    MASSERT(m_LwrrHz);
    return static_cast<UINT64>(round((1.0 / m_LwrrHz) * (m_DutyPct / 100.0) * 1e9));
}

UINT64 VkStress::SweepWorkController::CallwlateDelayTimeNs()
{
    return static_cast<UINT64>(round((1.0 / m_LwrrHz) * (1.0 - m_DutyPct / 100.0) * 1e9));
}

// Sweeps the workload frequency up or down. When sweeping up, there are two
// cases: we are either sweeping up to MAX_FREQ_POSSIBLE or up to HighHz.
// In the former case, we must keep track of the highest frequency that
// we hit in order to know which frequency to sweep down from (TempHighHz).
void VkStress::SweepWorkController::Sweep(SweepState direction)
{
    const UINT64 timeNow = Xp::QueryPerformanceCounter();
    if (!m_BeginTime)
    {
        m_BeginTime = timeNow;
    }
    const FLOAT64 elapsedSec = static_cast<FLOAT64>(timeNow - m_BeginTime) /
                                                    Xp::QueryPerformanceFrequency();

    if (direction == SweepState::UP)
    {
        m_TempHighHz = m_LwrrHz;
        if (m_HighHz == MAX_FREQ_POSSIBLE)
        {
            if (m_OutputFrames < MIN_OUTPUT_FRAMES || m_OutputDelayTimeNs < MIN_OUTPUT_DELAY_NS)
            {
                Printf(Tee::PriLow, "SweepWorkController reached %.2fHz\n", m_TempHighHz);
                NextState();
            }
            else
            {
                if (m_LinearSweep)
                {
                    m_LwrrHz = m_LowHz + floor((elapsedSec * 1e6) / m_StepTimeUs) * m_StepHz;
                }
                else
                {
                    m_LwrrHz = m_LowHz * pow(2.0, elapsedSec * m_OctavesPerSecond);
                }
            }
        }
        else if (m_LwrrHz >= m_HighHz)
        {
            m_LwrrHz = m_HighHz;
            NextState();
        }
        else
        {
            if (m_LinearSweep)
            {
                m_LwrrHz = m_LowHz + floor((elapsedSec * 1e6) / m_StepTimeUs) * m_StepHz;
            }
            else
            {
                m_LwrrHz = m_LowHz * pow(2.0, elapsedSec * m_OctavesPerSecond);
            }
        }
    }
    else // SweepState::DOWN
    {
        const FLOAT64 highHz = m_HighHz == MAX_FREQ_POSSIBLE ? m_TempHighHz : m_HighHz;
        if (m_LinearSweep)
        {
            m_LwrrHz = m_HighHz - floor((elapsedSec * 1e6) / m_StepTimeUs) * m_StepHz;
        }
        else
        {
            m_LwrrHz = highHz * pow(0.5, elapsedSec * m_OctavesPerSecond);
        }
        if (m_LwrrHz <= m_LowHz)
        {
            m_LwrrHz = m_LowHz;
            NextState();
        }
    }
}

void VkStress::SweepWorkController::Hold()
{
    if (m_CyclesHolding++ >= static_cast<UINT64>(2.5 * GetWindowSize()))
    {
        NextState();
    }
}

void VkStress::SweepWorkController::NextState()
{
    m_StateIdx = (m_StateIdx + 1) % NUM_SWEEP_STATES;

    // Clear the data associated with the new state
    if (m_SweepStates[m_StateIdx] == SweepState::HOLD)
    {
        m_CyclesHolding = 0;
    }
    else if (m_SweepStates[m_StateIdx] == SweepState::DOWN)
    {
        m_FinishedOneSweepUp = true;
        m_BeginTime = 0;
    }
    else
    {
        m_BeginTime = 0;
    }
}

bool VkStress::SweepWorkController::PulsingToMaxFreqPossible() const
{
    return m_HighHz == VkStress::MAX_FREQ_POSSIBLE;
}

bool VkStress::SweepWorkController::IgnoreWaveformAclwracy() const
{
    return PulsingToMaxFreqPossible();
}

VkStress::TileMaskWorkController::TileMaskWorkController(VkStress* pTest, UINT64 drawJobTimeNs) :
    DutyWorkController(pTest)
{
    SetDrawJobTimeNs(drawJobTimeNs);
}

void VkStress::TileMaskWorkController::SetDrawJobTimeNs(UINT64 drawJobTimeNs)
{
    m_DrawJobTimeNs = drawJobTimeNs;
    SetTargetWorkTimeNs(m_DrawJobTimeNs);
}

UINT64 VkStress::TileMaskWorkController::CallwlateWorkTimeNs()
{
    return m_DrawJobTimeNs;
}

UINT64 VkStress::TileMaskWorkController::CallwlateDelayTimeNs()
{
    return 0;
}

RC VkStress::NoteController::Setup()
{
    SetTargetWorkTimeNs(static_cast<UINT64>(
        (1.0 / m_LwrrNote.FreqHz) * (m_LwrrNote.DutyPct / 100.0) * 1e9));
    return WorkController::Setup();
}

string VkStress::NoteController::Note::ToString() const
{
    return Utility::StrPrintf("freq=%.1fHz, duty=%.1f%%, length=%llums, lyric=%s",
        this->FreqHz, this->DutyPct, this->LengthMs, this->Lyric.c_str());
}

bool VkStress::NoteController::IgnoreWaveformAclwracy() const
{
    return true;
}

UINT64 VkStress::NoteController::CallwlateWorkTimeNs()
{
    const UINT64 lwrrTickCount = Xp::QueryPerformanceCounter();
    const UINT64 ticksPerMs = Xp::QueryPerformanceFrequency() / 1000ULL;
    MASSERT(ticksPerMs);
    if (!ticksPerMs)
    {
        return 0;
    }
    const UINT64 elapsedMs = (lwrrTickCount - m_StartTickCount) / ticksPerMs;

    if (!m_StartTickCount || elapsedMs >= m_LwrrNote.LengthMs)
    {
        Note nextNote = GetNextNote();

        // If we have played at least one note...
        if (m_StartTickCount)
        {
            // If we held the last note for too long, shorten the next one
            UINT64 extraMs = elapsedMs - m_LwrrNote.LengthMs;
            if (extraMs >= nextNote.LengthMs)
            {
                extraMs = nextNote.LengthMs - 1;
            }
            nextNote.LengthMs -= extraMs;
        }

        m_LwrrNote = nextNote;
        Printf(m_PrintPri, "Playing note: %s\n", m_LwrrNote.ToString().c_str());
        if (!m_LwrrNote.Lyric.empty())
        {
            Printf(Tee::PriNormal, "%s\n", m_LwrrNote.Lyric.c_str());
        }
        m_StartTickCount = lwrrTickCount;
    }

    return static_cast<UINT64>(round(
        (1.0 / m_LwrrNote.FreqHz) * (m_LwrrNote.DutyPct / 100.0) * 1e9));
}

UINT64 VkStress::NoteController::CallwlateDelayTimeNs()
{
    return static_cast<UINT64>(round(
        (1.0 / m_LwrrNote.FreqHz) * (1.0 - m_LwrrNote.DutyPct / 100.0) * 1e9));
}

bool VkStress::MusicController::Done() const
{
    return m_NumNotesPlayed > static_cast<UINT64>(m_Notes.size());
}

void VkStress::MusicController::SetNotes(const Notes& notes)
{
    m_Notes = notes;
}

VkStress::NoteController::Note VkStress::MusicController::GetNextNote()
{
    Note returnNote = m_Notes[m_NoteIdx];

    const auto numNotes = m_Notes.size();
    if (numNotes > 1)
    {
        m_NoteIdx = (m_NoteIdx + 1) % numNotes;
    }
    m_NumNotesPlayed++;

    return returnNote;
}

bool VkStress::RandomNoteController::Done() const
{
    return true;
}

VkStress::NoteController::Note VkStress::RandomNoteController::GetNextNote()
{
    MASSERT(m_pVkStress);
    Note returnNote;
    auto& rand = m_pVkStress->GetFpContext()->Rand;

    returnNote.FreqHz = rand.GetRandomDouble(m_MinFreqHz, m_MaxFreqHz);
    returnNote.DutyPct = rand.GetRandomDouble(m_MinDutyPct, m_MaxDutyPct);
    returnNote.LengthMs = rand.GetRandom64(m_MinLengthMs, m_MaxLengthMs);

    return returnNote;
}

bool VkStress::IsPulsing() const
{
    const PulseMode pm = static_cast<PulseMode>(GetPulseMode());
    switch (pm)
    {
        case SWEEP:
        case SWEEP_LINEAR:
        case FMAX:
        case MUSIC:
        case RANDOM:
            return true;
        case TILE_MASK:
        case TILE_MASK_CONSTANT_DUTY:
        case TILE_MASK_USER:
        case TILE_MASK_GPC:
        case TILE_MASK_GPC_SHUFFLE:
        case NO_PULSE:
            return false;
        default:
            MASSERT(!"Unknown PulseMode");
            return false;
    }
}

RC VkStress::ApplyMaxPowerSettings()
{
    if (m_MaxHeat)
    {
        Printf(Tee::PriError, "MaxHeat cannot be used with ApplyMaxPowerSettings\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    switch (GetBoundGpuSubdevice()->DeviceId())
    {
#if LWCFG(GLOBAL_CHIP_T234)
        case Gpu::T234:
        {
            m_MaxTextureSize = 256; // 512 for high undershoot average
            m_TexReadsPerDraw = 3;
            m_TexIdxFarOffset = 1;
            m_AnisotropicFiltering = false;
            m_UseTessellation = false;
            m_UseMeshlets = true;
            m_Ztest = true;
            m_Stencil = true;
            m_PpV = 250;
            m_FarZMultiplier = 1.0;
            m_NumLights = 1;
            m_ApplyFog = false;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA107)
        case Gpu::GA107:
        {
            m_MaxTextureSize = 256;
            m_TexReadsPerDraw = 3;
            m_TexIdxFarOffset = 1;
            m_AnisotropicFiltering = false;
            m_UseTessellation = false;
            m_UseMeshlets = true;
            m_Ztest = false;
            m_Stencil = true;
            m_PpV = 250;
            m_FarZMultiplier = 1.0;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA106)
        case Gpu::GA106:
        {
            m_MaxTextureSize = 256;
            m_TexReadsPerDraw = 4;
            m_TexIdxFarOffset = 1;
            m_TexIdxStride = 2;
            m_AnisotropicFiltering = false;
            m_UseTessellation = false;
            m_UseMeshlets = true;
            m_Ztest = false;
            m_Stencil = true;
            m_PpV = 150;
            m_FarZMultiplier = 1.0;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA104)
        case Gpu::GA104:
        {
            m_MaxTextureSize = 256;
            m_TexReadsPerDraw = 4;
            m_TexIdxFarOffset = 3;
            m_TexIdxStride = 2;
            m_AnisotropicFiltering = false;
            m_UseTessellation = false;
            m_UseMeshlets = true;
            m_Ztest = false;
            m_Stencil = false;
            m_PpV = 450;
            m_FarZMultiplier = 1.0f;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA103)
        case Gpu::GA103:
        {
            m_NumTextures = 8;
            m_AnisotropicFiltering = false;
            m_UseTessellation = false;
            m_UseMeshlets = true;
            m_FarZMultiplier = 1.0f;
            m_ExponentialFog = false;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA102)
        case Gpu::GA102:
        {
            if (Utility::CountBits(GetBoundGpuSubdevice()->GetFsImpl()->FbpMask()) > 2)
            {
                m_TexReadsPerDraw = 3;
                m_TexIdxFarOffset = 3;
                m_TexIdxStride = 2;
                m_Stencil = false;
                m_PpV = 200;
            }
            else
            {
                m_TexReadsPerDraw = 2;
                m_TexIdxFarOffset = 2;
                m_TexIdxStride = 1;
                m_Stencil = true;
                m_PpV = 1600;
                m_NumLights = 3;
                m_MaxTextureSize = 64;
            }
            m_AnisotropicFiltering = false;
            m_UseTessellation = false;
            m_UseMeshlets = true;
            m_Ztest = false;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
        case Gpu::GA100:
        {
            m_TexIdxFarOffset = 2;
            m_FarZMultiplier = 1;
            m_PpV = 325;
            m_Stencil = false;
            m_TxD = -0.5f;
            m_UseTessellation = false;
            m_Ztest = false;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU102)
        case Gpu::TU102:
        {
            m_TxD = -0.5f;
            m_UseTessellation = false;
            m_PpV = 300;
            m_FarZMultiplier = 1;
            m_MaxTextureSize = 1024;
            m_NumTextures = 8;
            m_TexIdxFarOffset = 2;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU104)
        case Gpu::TU104:
        {
            m_MaxTextureSize = 1024;
            m_NumTextures = 8;
            m_TexIdxFarOffset = 2;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU106)
        case Gpu::TU106:
        {
            m_MaxTextureSize = 1024;
            m_NumTextures = 8;
            m_TexIdxFarOffset = 2;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU116)
        case Gpu::TU116:
        {
            m_MaxTextureSize = 128;
            m_UseTessellation = false;
            m_TxD = -0.3f;
            break;
        }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU117)
        case Gpu::TU117:
        {
            m_TxD = 1.5f;
            m_TexIdxFarOffset = 2;
            break;
        }
#endif
        default:
            Printf(Tee::PriWarn, "VkPowerStress has not been tuned for %s\n",
                Gpu::DeviceIdToString(GetBoundGpuSubdevice()->DeviceId()).c_str());
            break;
    }
    return RC::OK;
}

// This function sets a PerfPoint that maximizes the GPU's temperature when
// running this test. It does this by setting the pstate with the lowest
// dramclk and highest gpcclk. Many SKUs have several pstates with the same
// max gpcclk but different dramclk. For those SKUs, it is better to run at
// the pstate with the lowest dramclk because we will draw less power
// through FB and more through the GPU itself, which will maximize GPU heat.
// This function is not supported on CheetAh or Windows VkTest.
RC VkStress::ApplyMaxHeatSettings()
{
    RC rc;
#ifndef VULKAN_STANDALONE
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    if (!pPerf->HasPStates() || GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriWarn, "%s cannot set optimal clock frequencies\n", GetName().c_str());
        return rc;
    }

    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));
    m_PStatesClaimed = true;

    vector<UINT32> pstates;
    CHECK_RC(pPerf->GetAvailablePStates(&pstates));

    UINT32 bestPState = pstates[0];
    Perf::ClkRange gpcclkRange;
    Perf::ClkRange dramclkRange;
    UINT32 maxGpcclkKHz = 0;
    UINT32 minDramclkKHz = (std::numeric_limits<UINT32>::max)();
    for (const auto pstate : pstates)
    {
        CHECK_RC(pPerf->GetClockRange(pstate, Gpu::ClkGpc, &gpcclkRange));
        if (gpcclkRange.MaxKHz > maxGpcclkKHz)
        {
            maxGpcclkKHz = gpcclkRange.MaxKHz;
        }
    }
    for (const auto pstate : pstates)
    {
        CHECK_RC(pPerf->GetClockRange(pstate, Gpu::ClkGpc, &gpcclkRange));
        CHECK_RC(pPerf->GetClockRange(pstate, Gpu::ClkM, &dramclkRange));
        if (gpcclkRange.MaxKHz == maxGpcclkKHz && dramclkRange.MinKHz <= minDramclkKHz)
        {
            if (dramclkRange.MinKHz == minDramclkKHz)
            {
                // If two pstates have the same dramclk/gpcclk, pick the
                // higher pstate since higher numbered pstates usually have
                // more low power features enabled (e.g. MSCG and lower PCIE speed).
                bestPState = max(pstate, bestPState);
            }
            else // dramclkRange.MinKHz < minDramclkKHz
            {
                bestPState = pstate;
                minDramclkKHz = dramclkRange.MinKHz;
            }
        }
    }
    CHECK_RC(pPerf->GetLwrrentPerfPoint(&m_OrigPerfPoint));
    Perf::PerfPoint maxHeatPP(bestPState, Perf::GpcPerf_MAX);

    VerbosePrintf("Overriding PerfPoint to %s\n", maxHeatPP.name(true).c_str());
    CHECK_RC(pPerf->SetPerfPoint(maxHeatPP));
    m_PerfPointOverriden = true;
#endif
    return rc;
}

#ifdef VULKAN_STANDALONE_KHR
void VkStress::PresentThread(void * vkStress)
{
    VkStress * pVkStress = static_cast<VkStress*>(vkStress);
    pVkStress->PresentThread();
}

void VkStress::PresentThread()
{
    Tasker::DetachThread detach;

    UINT64 presentIdx = 0;

    Printf(Tee::PriLow, "Present thread start\n");
    while (true)
    {
        UINT32 lwrrentSwapChainIdx = m_SwapChainKHR->GetNextSwapChainImage(m_SwapChainAcquireSems[0].GetSemaphore());
        const VkResult res = m_SwapChainKHR->PresentImage(lwrrentSwapChainIdx,
                                                          m_SwapChainAcquireSems[0].GetSemaphore(),
                                                          VK_NULL_HANDLE, // ignored by SwapChainKHR
                                                          true);          // ignored by SwapChainKHR
        if (res != VK_SUCCESS)
        {
            Printf(Tee::PriError, "PresentImage failed with VkResult=%d\n", res);
            break;
        }

        if (Tasker::WaitOnEvent(m_pThreadExitEvent, m_DetachedPresentMs) != RC::TIMEOUT_ERROR)
        {
            break;
        }

        presentIdx++;
    }
    Printf(Tee::PriLow, "Present thread end, done present %llu times\n", presentIdx);
}
#endif

//------------------------------------------------------------------------------------------------
// Queue Container class that is thread safe.
VkStress::CheckSurfaceQueue::CheckSurfaceQueue() : m_Mutex(nullptr)
{
    m_Mutex = Tasker::AllocMutex("CheckSurfaceQueue::m_Mutex", Tasker::mtxUnchecked);
}

VkStress::CheckSurfaceQueue::~CheckSurfaceQueue()
{
    Tasker::FreeMutex(m_Mutex);
}

void VkStress::CheckSurfaceQueue::Push(CheckSurfaceParams params)
{
    Tasker::MutexHolder lock(m_Mutex);

    m_Queue.push(params);
}

UINT32 VkStress::CheckSurfaceQueue::Size()
{
    Tasker::MutexHolder lock(m_Mutex);

    return static_cast<UINT32>(m_Queue.size());
}

// Returns -1 if the queue is empty, otherwise pops the front element and returns it
// as a single atomic event.
bool VkStress::CheckSurfaceQueue::GetParams(CheckSurfaceParams *pParams)
{
    Tasker::MutexHolder lock(m_Mutex);

    if (!m_Queue.empty())
    {
        *pParams = m_Queue.front();
        m_Queue.pop();
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------------------------
// This function will create a Raytracer object within the given VkStress object that is
// accessible to JS.
extern JSClass Raytracer_class;
RC VkStress::AddExtraObjects(JSContext *cx, JSObject *obj)
{
    RC rc;
#if !defined(VULKAN_STANDALONE)
    m_Rt = make_unique<Raytracer>(m_pVulkanDev);

    // Creates a static JSClass RaytracerClass
    JSObject *raytracerObject = JS_DefineObject(cx, obj, "Raytracer",
                                                &Raytracer_class, NULL, 0);
    if (!raytracerObject)
    {
        JS_ReportWarning(cx, "Unable to define Raytracer object");
        return RC::SOFTWARE_ERROR;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, raytracerObject, "Help", &C_Global_Help, 1, 0);

    if (!JS_SetPrivate(cx, raytracerObject, m_Rt.get()))
    {
        JS_ReportWarning(cx, "Unable to define Raytracer object");
        return RC::SOFTWARE_ERROR;
    }
    m_Rt->SetJSObject(raytracerObject);
    rc =  GpuTest::AddExtraObjects(cx, obj);
#endif
    return rc;
}

UINT64 VkStress::GenRandMask(UINT32 numBits, UINT32 numMaskBits)
{
    if (numBits > 64)
    {
        MASSERT(!"GenRandMask() numBits > 64");
        numBits = 64;
    }
    if (numMaskBits > 64)
    {
        MASSERT(!"GenRandMask() numMaskBits > 64");
        numMaskBits = 64;
    }

    UINT64 mask = 0;
    UINT32 vals[64];

    for (UINT32 ii = 0; ii < numMaskBits; ii++)
        vals[ii] = ii;

    GetFpContext()->Rand.Shuffle(numMaskBits, vals);

    for (UINT32 ii = 0; ii < numBits; ii++)
        mask |= 1ULL << static_cast<UINT64>(vals[ii]);

    return mask;
}

UINT32 VkStress::GetNumGfxSMs()
{
#ifdef VULKAN_STANDALONE
    return m_NumSMs;
#else
    GpuSubdevice* pGpuSub = GetBoundGpuSubdevice();
    if (!pGpuSub->HasFeature(Device::Feature::GPUSUB_SUPPORTS_COMPUTE_ONLY_GPC))
        return m_NumSMs;

    UINT32 gfxGpcMask = pGpuSub->GetFsImpl()->GfxGpcMask();
    UINT32 numGfxTPCs = 0;
    INT32 gpcIdx;
    UINT32 tpcMask;
    while ((gpcIdx = Utility::BitScanForward(gfxGpcMask)) >= 0)
    {
        gfxGpcMask ^= 1U << gpcIdx;
        tpcMask = pGpuSub->GetGraphicsTpcMaskOnGpc(static_cast<UINT32>(gpcIdx));
        numGfxTPCs += Utility::CountBits(tpcMask);
    }
    return numGfxTPCs * pGpuSub->GetMaxSmPerTpcCount();
#endif
}

#ifndef VULKAN_STANDALONE
VkStress::FMaxController::FMaxController
(
    PIDGains gains
)
: PIDDutyController(gains, Tee::PriNone)
{
}

FLOAT32 VkStress::FMaxController::CallwlateError()
{
    RC rc = m_pGpuSubdevice->GetClock(Gpu::ClkGpc, &m_LwrrentGpcClockHz, nullptr, nullptr, nullptr);
    if (rc != RC::OK)
    {
        Printf(Tee::PriWarn, "Failed to get GPC clock in the FMaxController, callwlating with 0 Hz\n");
        m_LwrrentGpcClockHz = 0;
    }

    if (m_FMaxPerfTarget == PERF_TARGET_NONE)
    {
        const UINT64 potentialNewTargetGpcClockHz =
            static_cast<UINT64>(m_FMaxFraction * m_LwrrentGpcClockHz);
        if (potentialNewTargetGpcClockHz > m_LwrrentTargetGpcClockHz)
        {
            m_LwrrentTargetGpcClockHz = potentialNewTargetGpcClockHz;
        }
    }
    else
    {
        LW2080_CTRL_PERF_LIMITS_GET_STATUS_V2_PARAMS statusParam = {0};
        statusParam.numLimits = 1;

        switch (m_FMaxPerfTarget)
        {
            case PERF_TARGET_RELIABILITY:
                statusParam.limitsList[0].limitId = LW2080_CTRL_PERF_LIMIT_RELIABILITY_LOGIC;
                break;
            default:
                Printf(Tee::PriWarn,
                    "FMaxController:: Unrecognized FMaxPerfTarget=%d, using RELIABILITY_LOGIC\n",
                    m_FMaxPerfTarget);
                statusParam.limitsList[0].limitId = LW2080_CTRL_PERF_LIMIT_RELIABILITY_LOGIC;
        }
        const RC getStatusRc = LwRmPtr()->ControlBySubdevice(m_pGpuSubdevice,
                                               LW2080_CTRL_CMD_PERF_LIMITS_GET_STATUS_V2,
                                               &statusParam,
                                               sizeof(statusParam));
        if (getStatusRc == RC::OK)
        {
            m_LwrrentTargetGpcClockHz = 1000LL * statusParam.limitsList[0].output.value;
        }
        else
        {
            Printf(Tee::PriWarn,
                "FMaxController: Perf get status failed - target not updated\n");
        }
    }

    return (1.0f*m_LwrrentGpcClockHz - 1.0f*m_LwrrentTargetGpcClockHz) / 1000000.0f;
}

const char * VkStress::FMaxController::GetUnits() const
{
    return "MHz";
}

void VkStress::FMaxThread(void *vkStress)
{
    VkStress * pVkStress = static_cast<VkStress*>(vkStress);
    pVkStress->FMaxThread();
}

void VkStress::FMaxThread()
{
    Utility::CleanupOnReturn<GpuTest, RC>
        stopPT(this, &GpuTest::StopPowerThread);

    if (m_FMaxWriteCsv)
    {
        if (GetPwrSampleIntervalMs() != 0) // Don't enable if disabled
        {
            SetPwrSampleIntervalMs(1000 / m_FMaxCtrlHz);
            StartPowerThread(true);
            Tasker::PollHw(GetTestConfiguration()->TimeoutMs(),
                [this]()->bool { return HasPowerThreadStarted(); },
                __FUNCTION__);
        }
    }

    Tasker::DetachThread detach;
    DutyWorkController* pWc = dynamic_cast<DutyWorkController*>(m_WorkController.get());
    if (pWc == nullptr)
    {
        Printf(Tee::PriWarn, "Failed to get DutyWorkController, exiting FMaxThread\n");
        return;
    }

    const PIDGains pidGains(m_FMaxGainP, m_FMaxGainI, m_FMaxGainD);
    m_FreqController = FMaxController(pidGains);
    m_FreqController.m_pGpuSubdevice = GetBoundGpuSubdevice();
    m_FreqController.m_FMaxFraction = m_FMaxFraction;
    m_FreqController.m_FMaxPerfTarget = static_cast<FMaxPerfTarget>(m_FMaxPerfTarget);
    m_FreqController.m_LwrrentTargetGpcClockHz = (m_FMaxConstTargetMHz != 0) ?
        1'000'000 * m_FMaxConstTargetMHz :
        static_cast<UINT64>(m_FMaxFraction * m_FMaxIdleGpc);
    if (static_cast<PulseMode>(m_PulseMode) == TILE_MASK)
    {
        // Allow the duty cycle for tile masking to reach 100%.
        // This will allow the test to activate all tiles when possible.
        m_FreqController.SetMaxDutyPct(100.0f);
        m_FreqController.SetDutyHint(20.0f);
    }
    else
    {
        m_FreqController.SetDutyHint(30.0f);
    }

    FileHolder csvFile;
    if (m_FMaxWriteCsv)
    {
        csvFile.Open(GetUniqueFilename("vkstress_fmax", "csv").c_str(), "w");
        fprintf(csvFile.GetFile(),
            "timestampSec, DutyPercent, TargetClockMHz, LwrrentClockMHz, tempC, powerW, "
            "FMaxConstTargetMHz=%d, "
            "FMaxCtrlHz=%d, "
            "FMaxForcedDutyInc=%f, "
            "FMaxFraction=%f, "
            "FMaxGainP=%f, "
            "FMaxGainI=%f, "
            "FMaxGainD=%f\n",
            m_FMaxConstTargetMHz,
            m_FMaxCtrlHz,
            m_FMaxForcedDutyInc,
            m_FMaxFraction,
            m_FMaxGainP,
            m_FMaxGainI,
            m_FMaxGainD);
    }

    while (true)
    {
        FLOAT64 newDutyCycle = m_FreqController.CallwlateDutyCycle();

        // Apply FMaxForcedDutyInc whenever the current gpcclk is greater than
        // or equal to the target. Allow 1MHz wiggle-room due to truncation.
        if (m_FreqController.m_LwrrentGpcClockHz >=
            (m_FreqController.m_LwrrentTargetGpcClockHz - 1000000ULL))
        {
            // Force increase of the current duty cycle to prevent staying at
            // 1% when the GPU might be cooling down and able take more load
            // at the same frequency.
            FLOAT64 lwrrentDuty = pWc->GetDutyPct();
            lwrrentDuty += m_FMaxForcedDutyInc;
            if (lwrrentDuty > m_FMaxAverageDutyPctMax)
            {
                // Can't go higher because of "Workload latency is too large\n"
                lwrrentDuty = m_FMaxAverageDutyPctMax;
            }
            pWc->SetDutyPct(lwrrentDuty);
        }
        else
        {
            pWc->SetDutyPct(newDutyCycle);
        }

        m_FMaxDutyPctSum += static_cast<UINT64>(100.0*pWc->GetDutyPct());
        m_FMaxDutyPctSamples++;

        if (csvFile.GetFile())
        {
            FLOAT32 intTemp = -300.0f;
            GetBoundGpuSubdevice()->GetThermal()->GetChipTempViaInt(&intTemp);
            fprintf(csvFile.GetFile(),
                "%.3f, %.1f, %d, %d, %.1f, %.1f\n",
                Tee::GetTimeFromStart() / 1000000000.0,
                pWc->GetDutyPct(),
                static_cast<UINT32>(m_FreqController.m_LwrrentTargetGpcClockHz/1000000),
                static_cast<UINT32>(m_FreqController.m_LwrrentGpcClockHz/1000000),
                intTemp,
                m_LwrrentPowerMilliWatts / 1000.0f);
        }
        if (m_Debug & DBG_FMAX)
        {
            Printf(Tee::PriNormal,
                "FMaxController: dutyCycle = %.1f%%, current gpc clk = %4.1f MHz, target = %.1f MHz\n",
                pWc->GetDutyPct(),
                m_FreqController.m_LwrrentGpcClockHz / 1000000.0f,
                m_FreqController.m_LwrrentTargetGpcClockHz / 1000000.0f);
        }

        if (Tasker::WaitOnEvent(m_pThreadExitEvent, 1000.0/m_FMaxCtrlHz) != RC::TIMEOUT_ERROR)
        {
            break;
        }
    }
}

RC VkStress::GetNotes(JsArray* val) const
{
    MASSERT(val);
    *val = m_JsNotes;
    return RC::OK;
}

RC VkStress::SetNotes(JsArray* val)
{
    MASSERT(val);
    m_JsNotes = *val;
    return RC::OK;
}

JS_CLASS_INHERIT(VkStress, GpuTest, "Stress the graphics/compute engine using Vulkan");

JS_STEST_BIND_NO_ARGS(VkStress, ApplyMaxPowerSettings,
    "Apply args to achieve maximum power draw");

CLASS_PROP_READWRITE(VkStress, FramesPerSubmit,       UINT32,
    "Number of frames per submit");
CLASS_PROP_READWRITE(VkStress, LoopMs,                    UINT32,
    "time to loop in msecs");
CLASS_PROP_READWRITE(VkStress, RuntimeMs,                 UINT32,
    "Requested time to run the active portion of the test in ms");
CLASS_PROP_READWRITE(VkStress, IndexedDraw,               bool,
    "Use indexed rendering");
CLASS_PROP_READWRITE(VkStress, SkipDirectDisplay,         bool,
    "Disable direct memory scanout on display");
CLASS_PROP_READWRITE(VkStress, SendColor,                 bool,
    "Send color attributes to vertex shader");
CLASS_PROP_READWRITE(VkStress, ApplyFog,                  bool,
    "Apply fog in fragment shader");
CLASS_PROP_READWRITE(VkStress, UseMipMapping,             bool,
    "Use texture mip-mapping");
CLASS_PROP_READWRITE(VkStress, ExponentialFog,            bool,
    "Use equation for exponential fog instead of linear fog");
CLASS_PROP_READWRITE(VkStress, DrawLines,                 bool,
    "Draw lines instead of triangles (debug feature)");
CLASS_PROP_READWRITE(VkStress, DrawTessellationPoints,    bool,
    "Draw tessellation points instead of filled figures (debug feature)");
CLASS_PROP_READWRITE(VkStress, Debug,                     UINT32,
    "Enable debug mode.");
CLASS_PROP_READWRITE(VkStress, Ztest,                      bool,
    "Enable Depth Test.");
CLASS_PROP_READWRITE(VkStress, Stencil,                    bool,
    "Enable Stencil Test.");
CLASS_PROP_READWRITE(VkStress, ColorLogic,                 bool,
    "Enable XOR color logic op");
CLASS_PROP_READWRITE(VkStress, ColorBlend,                 bool,
    "Enable color blending (takes precedence over color logic op and must use floating point display surface formats)"); //$
CLASS_PROP_READWRITE(VkStress, EnableValidation,           bool,
    "Enable/Disable validation layers (default = false)");
CLASS_PROP_READWRITE(VkStress, EnableErrorLogger,           bool,
    "Enable/Disable the ErrorLogger on validation failures (default = true)");
CLASS_PROP_READWRITE(VkStress, ShaderReplacement,          bool,
    "Shader replacement");
CLASS_PROP_READWRITE(VkStress, SampleCount,                UINT32,
    "Number of samples to take per fragment");
CLASS_PROP_READWRITE(VkStress, NumLights,                  UINT32,
    "Number of lights to use during shading");
CLASS_PROP_READWRITE(VkStress, NumTextures,                UINT32,
    "Number of textures to allocate on the GPU");
CLASS_PROP_READWRITE(VkStress, PpV,                        double,
    "Number of pixels per vertex (controls geometry density)");
CLASS_PROP_READWRITE(VkStress, UseMeshlets,                bool,
    "Enable meshlet shaders");
CLASS_PROP_READWRITE(VkStress, UseTessellation,            bool,
    "Enable tessellation evaluation/control shaders");
CLASS_PROP_READWRITE(VkStress, BoringXform,                bool,
    "Do a boring geometry transformation");
CLASS_PROP_READWRITE(VkStress, CameraScaleFactor,          FLOAT32,
    "Scales the camera's field of view");
CLASS_PROP_READWRITE(VkStress, MinTextureSize,             UINT32,
    "The minimum texture size for any texture (not used in TX_MODE_STATIC)");
CLASS_PROP_READWRITE(VkStress, MaxTextureSize,             UINT32,
    "The maximum texture size for any texture (size used in TX_MODE_STATIC)");
CLASS_PROP_READWRITE(VkStress, TxMode,                     UINT32,
    "Controls how textures are filled");
CLASS_PROP_READWRITE(VkStress, TxD,                        FLOAT32,
    "log2(texels/pixel)");
CLASS_PROP_READWRITE(VkStress, UseRandomTextureData,       bool,
    "Fill textures using random data instead of MATS patterns");
CLASS_PROP_READWRITE(VkStress, CorruptionStep,             UINT32,
    "Negative testing error injection at selected progress step");
CLASS_PROP_READWRITE(VkStress, CorruptionLocationOffsetBytes, UINT64,
    "Negative testing error injection location offset in bytes");
CLASS_PROP_READWRITE(VkStress, CorruptionLocationPercentOfSize, UINT32,
    "Negative testing error injection location in percent of image size");
CLASS_PROP_READWRITE(VkStress, PrintPerf,                  bool,
    "Print runtime performance statistics");
CLASS_PROP_READWRITE(VkStress, ClearColor,                vector<UINT32>,
    "Clear color values(LE_A8R8G8B8) to initialize the screen images with before testing");
CLASS_PROP_READWRITE(VkStress, DumpPngOnError,             bool,
    "Dump a .png on failing surface (default=true).");
CLASS_PROP_READWRITE(VkStress, DumpPngMask,                UINT32,
    "Dump a .png after every surface check(default = false).");
CLASS_PROP_READWRITE(VkStress, FarZMultiplier,             float,
    "Control amount of depth folding for the far plane(default=1.0).");
CLASS_PROP_READWRITE(VkStress, NearZMultiplier,            float,
    "Control amount of depth folding for the near plane(default=0.4).");
CLASS_PROP_READWRITE(VkStress, KeepRunning,                bool,
    "Run until KeepRunning becomes false");
CLASS_PROP_READWRITE(VkStress, BufferCheckMs,              UINT32,
    "How often to check the color & depth/stencil buffers(default=~0).");
CLASS_PROP_READWRITE(VkStress, UseCompute,                 bool,
    "Use Compute shaders to increase stress.");
CLASS_PROP_READWRITE_JSARRAY(VkStress, ComputeShaderSelection, JsArray,
                     "Array of indexes into ComputeProgs for each compute queue");
CLASS_PROP_READWRITE(VkStress, NumComputeQueues,           UINT32,
    "Number of compute conlwrrent exelwtion queues");
CLASS_PROP_READWRITE_JSARRAY(VkStress, ComputeInnerLoops,  JsArray,
    "Compute Callwlate loops, Debug(default= 20) for each compute queue");
CLASS_PROP_READWRITE_JSARRAY(VkStress, ComputeOuterLoops,  JsArray,
    "Debug(Compute shader loops, Debug(default= 1) for each compute queue");
CLASS_PROP_READWRITE_JSARRAY(VkStress, ComputeRuntimeClks, JsArray,
    "How long to run the Compute Shader in usecs(default= 0) for each compute queue");
CLASS_PROP_READWRITE(VkStress, ComputeDisableIMMA,            bool,
    "Do not use IMMA instructions with compute shader (default = false).");
CLASS_PROP_READWRITE(VkStress, AsyncCompute,               bool,
    "Do not synchronize compute workload with graphics draws (default = false).");
CLASS_PROP_READWRITE(VkStress, PrintTuningInfo,            bool,
    "Output the compute tuning info (default = false).");
CLASS_PROP_READWRITE(VkStress, PrintSCGInfo,               bool,
    "Output the SCG results (defautl = false)");
CLASS_PROP_READWRITE(VkStress, DrawJobTimeNs,              UINT64,
    "Target time for graphics work submissions when not pulsing");
CLASS_PROP_READWRITE(VkStress, PrintComputeCells,          bool,
    "Debug:Output the Compute Cells array (defautl = false)");
CLASS_PROP_READWRITE(VkStress, MovingAvgSampleWindow,      UINT64,
    "Moving average window size for work submissions");
CLASS_PROP_READWRITE(VkStress, PulseMode,                  UINT32,
    "Controls if/how VkStress pulses its workload");
CLASS_PROP_READWRITE(VkStress, LowHz,                      FLOAT64,
    "Lowest frequency to test when pulsing workload");
CLASS_PROP_READWRITE(VkStress, HighHz,                     FLOAT64,
    "Highest frequency to test when pulsing workload");
CLASS_PROP_READWRITE(VkStress, DutyPct,                    FLOAT64,
    "Workload duty cycle");
CLASS_PROP_READWRITE(VkStress, OctavesPerSecond,           FLOAT64,
    "How quickly to sweep the workload frequency");
CLASS_PROP_READWRITE(VkStress, StepHz,                     FLOAT64,
    "How much to increase workload frequency per step");
CLASS_PROP_READWRITE(VkStress, FMaxForcedDutyInc,          FLOAT32,
    "Value to add to the duty percent per control iteration when the clock is on target");
CLASS_PROP_READWRITE(VkStress, FMaxFraction,               FLOAT32,
    "Multiplier of highest observed GPC clock frequency to get the target frequency");
CLASS_PROP_READWRITE(VkStress, FMaxGainP,                  FLOAT32,
    "Proportional gain of the FMax controller");
CLASS_PROP_READWRITE(VkStress, FMaxGainI,                  FLOAT32,
    "Integral gain of the FMax controller");
CLASS_PROP_READWRITE(VkStress, FMaxGainD,                  FLOAT32,
    "Derivative gain of the FMax controller");
CLASS_PROP_READWRITE(VkStress, FMaxAverageDutyPctMax,      FLOAT32,
    "Fail the test when Average Duty Percent value is above this value");
CLASS_PROP_READWRITE(VkStress, FMaxAverageDutyPctMin,      FLOAT32,
    "Fail the test when Average Duty Percent value is below this value");
CLASS_PROP_READWRITE(VkStress, FMaxConstTargetMHz,         UINT32,
    "GPC clock value in MHz used as fixed target instead of self-adapting Fraction based");
CLASS_PROP_READWRITE(VkStress, FMaxCtrlHz,                 UINT32,
    "GPC clock measaure and control rate of the FMax controller");
CLASS_PROP_READWRITE(VkStress, FMaxPerfTarget,             UINT32,
    "Selection of perf data as source for target GPC clock value");
CLASS_PROP_READWRITE(VkStress, FMaxWriteCsv,               bool,
    "Enable FMax logging into csv files");
CLASS_PROP_READWRITE(VkStress, StepTimeUs,                 UINT64,
    "How long to stay at each workload frequency");
CLASS_PROP_READWRITE(VkStress, TileMask,                   UINT32,
    "User or GPC tile bitmask");
CLASS_PROP_READWRITE(VkStress, NumActiveGPCs,              UINT32,
    "Number of active GPCs in GPC shuffle mode");
CLASS_PROP_READWRITE(VkStress, SkipDrawChecks,             bool,
    "Do not report pixel errors");
CLASS_PROP_READWRITE(VkStress, SynchronousMode,            bool,
    "Synchronize workload pulsing across multiple GPUs");
CLASS_PROP_READWRITE(VkStress, WarpsPerSM,                 UINT32,
    "Number of compute warps(1-16) per SM. (default = 1)");
CLASS_PROP_READWRITE(VkStress, UseHTex,                    bool,
    "Enable corner sampled image(HTex)");
CLASS_PROP_READWRITE(VkStress, PrintExtensions,            bool,
    "Print the list of registered & enabled extensions(default = false)");
CLASS_PROP_READWRITE(VkStress, MaxHeat,                    bool,
    "Apply maximum heat settings");
CLASS_PROP_READWRITE_FULL(VkStress, AllowedMiscompares,    UINT32,
    "Number of allowed miscompares across all buffers (color, depth, stencil, and compute)",
    MODS_INTERNAL_ONLY | SPROP_VALID_TEST_ARG, 0);
CLASS_PROP_READWRITE(VkStress, UseRaytracing,              bool,
    "Enable raytracing pipeline");
CLASS_PROP_READWRITE(VkStress, DisplayRaytracing,          bool,
    "Display raytracing output(default = false)");
CLASS_PROP_READWRITE(VkStress, AnisotropicFiltering,       bool,
    "Enables texture sampler anisotropic filtering");
CLASS_PROP_READWRITE(VkStress, LinearFiltering,            bool,
    "Enables texture sampler linear filtering. Uses nearest neighbor if false.");
CLASS_PROP_READWRITE(VkStress, TexCompressionMask,         UINT32,
    "Bitmask which textures should be used as compressed");
CLASS_PROP_READWRITE(VkStress, TexReadsPerDraw,            UINT32,
    "Number of textures to read each draw call");
CLASS_PROP_READWRITE(VkStress, TexIdxStride,               UINT32,
    "Increments the texture index used for each quadframe");
CLASS_PROP_READWRITE(VkStress, TexIdxFarOffset,            UINT32,
    "Offsets the texture index used for the far frames in a quadframe");
CLASS_PROP_READONLY(VkStress,  Fps,                        double,
    "Frames per second in last run.");
//Debugging variables
CLASS_PROP_READWRITE(VkStress, ShowQueryResults,            bool,
    "Display the timestamps from all of the active query objects");
CLASS_PROP_READWRITE(VkStress, GenUniqueFilenames,          bool,
    "Generate timestamp based filenames for *.png/*.tga files");
CLASS_PROP_READWRITE_JSARRAY(VkStress, Notes,              JsArray,
    "Specifies a list of notes for VkStress to play");
CLASS_PROP_READWRITE(VkStress, RandMinFreqHz,              FLOAT64,
    "Specifies the minimum workload frequency when pulsing randomly");
CLASS_PROP_READWRITE(VkStress, RandMaxFreqHz,              FLOAT64,
    "Specifies the maximum workload frequency when pulsing randomly");
CLASS_PROP_READWRITE(VkStress, RandMinDutyPct,             FLOAT64,
    "Specifies the minimum duty cycle when pulsing randomly");
CLASS_PROP_READWRITE(VkStress, RandMaxDutyPct,             FLOAT64,
    "Specifies the maximum duty cycle when pulsing randomly");
CLASS_PROP_READWRITE(VkStress, RandMinLengthMs,            UINT64,
    "Specifies the minimum tone duration when pulsing randomly");
CLASS_PROP_READWRITE(VkStress, RandMaxLengthMs,            UINT64,
    "Specifies the maximum tone duration when pulsing randomly");

JS_CLASS(VkStressConst);
static SObject VkStressConst_Object
(
    "VkStressConst",
    VkStressConstClass,
    0,
    0,
    "VkStressConst JS Object"
);

PROP_CONST(VkStressConst, TX_MODE_STATIC, VkStress::TX_MODE_STATIC);
PROP_CONST(VkStressConst, TX_MODE_INCREMENT, VkStress::TX_MODE_INCREMENT);
PROP_CONST(VkStressConst, TX_MODE_RANDOM, VkStress::TX_MODE_RANDOM);
PROP_CONST(VkStressConst, NO_PULSE, VkStress::NO_PULSE);
PROP_CONST(VkStressConst, SWEEP, VkStress::SWEEP);
PROP_CONST(VkStressConst, SWEEP_LINEAR, VkStress::SWEEP_LINEAR);
PROP_CONST(VkStressConst, FMAX, VkStress::FMAX);
PROP_CONST(VkStressConst, TILE_MASK, VkStress::TILE_MASK);
PROP_CONST(VkStressConst, MUSIC, VkStress::MUSIC);
PROP_CONST(VkStressConst, RANDOM, VkStress::RANDOM);
PROP_CONST(VkStressConst, TILE_MASK_CONSTANT_DUTY, VkStress::TILE_MASK_CONSTANT_DUTY);
PROP_CONST(VkStressConst, TILE_MASK_USER, VkStress::TILE_MASK_USER);
PROP_CONST(VkStressConst, TILE_MASK_GPC, VkStress::TILE_MASK_GPC);
PROP_CONST(VkStressConst, TILE_MASK_GPC_SHUFFLE, VkStress::TILE_MASK_GPC_SHUFFLE);
PROP_CONST(VkStressConst, MAX_FREQ_POSSIBLE, VkStress::MAX_FREQ_POSSIBLE);
PROP_CONST(VkStressConst, BUFFERCHECK_MS_DISABLED, VkStress::BUFFERCHECK_MS_DISABLED);

PROP_CONST(VkStressConst, CHECK_COLOR_SURFACE,          VkUtil::CHECK_COLOR_SURFACE);
PROP_CONST(VkStressConst, CHECK_DEPTH_SURFACE,          VkUtil::CHECK_DEPTH_SURFACE);
PROP_CONST(VkStressConst, CHECK_STENCIL_SURFACE,        VkUtil::CHECK_STENCIL_SURFACE);
PROP_CONST(VkStressConst, CHECK_DEPTH_STENCIL_SURFACE,  VkUtil::CHECK_DEPTH_STENCIL_SURFACE);
PROP_CONST(VkStressConst, CHECK_COMPUTE_FP16,           VkUtil::CHECK_COMPUTE_FP16);
PROP_CONST(VkStressConst, CHECK_COMPUTE_FP32,           VkUtil::CHECK_COMPUTE_FP32);
PROP_CONST(VkStressConst, CHECK_COMPUTE_FP64,           VkUtil::CHECK_COMPUTE_FP64);
PROP_CONST(VkStressConst, CHECK_RT_SURFACE,             VkUtil::CHECK_RT_SURFACE);
PROP_CONST(VkStressConst, CHECK_ALL_SURFACES,           VkUtil::CHECK_ALL_SURFACES);

// Rename the CHECK_* const properties to DUMP_* to make the code more readable in JS
PROP_CONST(VkStressConst, DUMP_COLOR_SURFACE,           VkUtil::CHECK_COLOR_SURFACE);
PROP_CONST(VkStressConst, DUMP_DEPTH_SURFACE,           VkUtil::CHECK_DEPTH_SURFACE);
PROP_CONST(VkStressConst, DUMP_STENCIL_SURFACE,         VkUtil::CHECK_STENCIL_SURFACE);
PROP_CONST(VkStressConst, DUMP_DEPTH_STENCIL_SURFACE,   VkUtil::CHECK_DEPTH_STENCIL_SURFACE);
PROP_CONST(VkStressConst, DUMP_COMPUTE_FP16,            VkUtil::CHECK_COMPUTE_FP16);
PROP_CONST(VkStressConst, DUMP_COMPUTE_FP32,            VkUtil::CHECK_COMPUTE_FP32);
PROP_CONST(VkStressConst, DUMP_COMPUTE_FP64,            VkUtil::CHECK_COMPUTE_FP64);
PROP_CONST(VkStressConst, DUMP_RT_SURFACE,              VkUtil::CHECK_RT_SURFACE);
PROP_CONST(VkStressConst, DUMP_ALL_SURFACES,            VkUtil::CHECK_ALL_SURFACES);

#endif
