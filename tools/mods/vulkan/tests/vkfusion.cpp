/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vkfusion.h"
#include "vulkan/swapchain.h"
#include "core/include/abort.h"

#ifndef VULKAN_STANDALONE
namespace VkFusionRegisterJS
{
    using VkFusion = ::VkFusion::Test;

    JS_CLASS_INHERIT(VkFusion, GpuTest, "Vulkan-based stress test");

    CLASS_PROP_READWRITE(VkFusion, RuntimeMs,        UINT32, "How long to run the test for [ms]");
    CLASS_PROP_READWRITE(VkFusion, KeepRunning,      bool,   "Used for running the test in the background");
    CLASS_PROP_READWRITE(VkFusion, EnableValidation, bool,   "Enable Vulkan validation layers");
    CLASS_PROP_READWRITE(VkFusion, PrintExtensions,  bool,   "Print list of registered and enabled extensions");
    CLASS_PROP_READWRITE(VkFusion, Group0Us,         UINT32, "Duration for subtest group 0 [us]");
    CLASS_PROP_READWRITE(VkFusion, Group1Us,         UINT32, "Duration for subtest group 1 [us]");
    CLASS_PROP_READWRITE(VkFusion, Group2Us,         UINT32, "Duration for subtest group 2 [us]");
    CLASS_PROP_READWRITE(VkFusion, Group3Us,         UINT32, "Duration for subtest group 3 [us]");

    using Graphics = ::VkFusion::Graphics;

    JS_CLASS_WITH_FLAGS(C_VkFusionGraphics, JSCLASS_HAS_PRIVATE);
    SObject Graphics_Object("Graphics", C_VkFusionGraphicsClass, nullptr, 0, "Graphics config");

    CLASS_PROP_READWRITE(Graphics, RunMask,          UINT32, "Bitmask indicating in which groups to run the subtest");
    CLASS_PROP_READWRITE(Graphics, NumJobs,          UINT32, "Number of round-robin jobs");
    CLASS_PROP_READWRITE(Graphics, SurfaceWidth,     UINT32, "Width of target surface, also controlled with -maxwh");
    CLASS_PROP_READWRITE(Graphics, SurfaceHeight,    UINT32, "Height of target surface, also controlled with -maxwh");
    CLASS_PROP_READWRITE(Graphics, SurfaceBits,      UINT32, "Number of bits per pixel in target surface");
    CLASS_PROP_READWRITE(Graphics, NumLights,        UINT32, "Number of lights computed per pixel");
    CLASS_PROP_READWRITE(Graphics, DrawsPerFrame,    UINT32, "Number of times the torus is drawn for every frame");
    CLASS_PROP_READWRITE(Graphics, GeometryType,     UINT32, "How geometry is generated: 0 - CPU/vertex buffer, 1 - full tessellated patch");
    CLASS_PROP_READWRITE(Graphics, NumVertices,      UINT32, "Number of vertices around a slice of the torus");
    CLASS_PROP_READWRITE(Graphics, Fov,              UINT32, "Field of view in degrees, affects screen coverage");
    CLASS_PROP_READWRITE(Graphics, MajorR,           double, "Major radius of the torus");
    CLASS_PROP_READWRITE(Graphics, MinorR,           double, "Minor radius of the torus");
    CLASS_PROP_READWRITE(Graphics, TorusMajor,       bool,   "Draw strips around major radius instead of minor radius");
    CLASS_PROP_READWRITE(Graphics, Lwll,             bool,   "Enable back-face lwlling");
    CLASS_PROP_READWRITE(Graphics, Rotation,         double, "Initial torus rotation around X axis in degrees, 0 is edge on, 90 is big O like GLStressZ");
    CLASS_PROP_READWRITE(Graphics, RotationStep,     double, "Degrees of rotation between each draw, 0 means no intra-frame rotation");
    CLASS_PROP_READWRITE(Graphics, NumTexReads,      UINT32, "Number of texture reads per fragment");
    CLASS_PROP_READWRITE(Graphics, NumTextures,      UINT32, "Number of distinct textures");
    CLASS_PROP_READWRITE(Graphics, TexSize,          UINT32, "Width/height of each texture");
    CLASS_PROP_READWRITE(Graphics, MipLodBias,       double, "Mipmap LOD bias");
    CLASS_PROP_READWRITE(Graphics, DebugMipMaps,     bool,   "Fill mipmaps with colors");
    CLASS_PROP_READWRITE(Graphics, RaysPerPixel,     UINT32, "Number of rays traced per pixel");
    CLASS_PROP_READWRITE(Graphics, RTGeometryType,   UINT32, "Type of shapes in RT geometry: 0 - lwbes, 1 - toruses");
    CLASS_PROP_READWRITE(Graphics, RTTLASWidth,      UINT32, "Width of TLAS geometry for raytracing");
    CLASS_PROP_READWRITE(Graphics, RTXMask,          UINT32, "Mask for the screen X coordinate for which to trace rays, e.g. 0 is every pixel, 0x1F is every 32nd pixel");
    CLASS_PROP_READWRITE(Graphics, Check,            bool,   "Enable comparing outputs against first frame");
    CLASS_PROP_READWRITE(Graphics, ShaderReplacement, bool,  "Shader replacement");
    CLASS_PROP_READONLY( Graphics, TotalDraws,       UINT64, "Total draws produced during the test in the last run (read-only)");
    CLASS_PROP_READONLY( Graphics, TotalPixels,      UINT64, "Total pixels produced during the test in the last run (read-only)");
    CLASS_PROP_READONLY( Graphics, DrawsPerSecond,   double, "Draws-Per-Second in the last run (read-only)");
    CLASS_PROP_READONLY( Graphics, PixelsPerSecond,  double, "Pixels-Per-Second in the last run (read-only)");

    using Raytracing = ::VkFusion::Raytracing;

    JS_CLASS_WITH_FLAGS(C_VkFusionRaytracing, JSCLASS_HAS_PRIVATE);
    SObject Raytracing_Object("Raytracing", C_VkFusionRaytracingClass, nullptr, 0, "Raytracing config");

    CLASS_PROP_READWRITE(Raytracing, RunMask,          UINT32, "Bitmask indicating in which groups to run the subtest");
    CLASS_PROP_READWRITE(Raytracing, NumJobs,          UINT32, "Number of round-robin jobs");
    CLASS_PROP_READWRITE(Raytracing, SurfaceWidth,     UINT32, "Width of target surface, also controlled with -maxwh");
    CLASS_PROP_READWRITE(Raytracing, SurfaceHeight,    UINT32, "Height of target surface, also controlled with -maxwh");
    CLASS_PROP_READWRITE(Raytracing, NumLwbesXY,       UINT32, "Number of lwbes generated in both X and Y axis, in the image plane");
    CLASS_PROP_READWRITE(Raytracing, NumLwbesZ,        UINT32, "Number of lwbes generated in Z axis, away from the camera");
    CLASS_PROP_READWRITE(Raytracing, BLASSize,         UINT32, "Size of one dimension of BLAS, assuming BLAS is a lwbe");
    CLASS_PROP_READWRITE(Raytracing, MinLwbeSize,      double, "Minimum size of each lwbe");
    CLASS_PROP_READWRITE(Raytracing, MaxLwbeSize,      double, "Maximum size of each lwbe");
    CLASS_PROP_READWRITE(Raytracing, MinLwbeOffset,    double, "Minimum offset of each lwbe");
    CLASS_PROP_READWRITE(Raytracing, MaxLwbeOffset,    double, "Maximum offset of each lwbe");
    CLASS_PROP_READWRITE(Raytracing, GeometryType,     UINT32, "Geometry type, i.e. what the lwbes are made of:"
                                                               " 0 - triangles,"
                                                               " 1 - AABBs,"
                                                               " 2 - AABBs (trivial intersection)");
    CLASS_PROP_READWRITE(Raytracing, GeometryLevel,    UINT32, "Where to put each lwbe:"
                                                               " 0 - one instance, one geometry, many lwbes,"
                                                               " 1 - one instance, many lwbes with one geometry,"
                                                               " 2 - many instances with one geometry, one lwbe (forces BLASSize to 1)");
    CLASS_PROP_READWRITE(Raytracing, Check,            bool,   "Enables checking of results between frames");
    CLASS_PROP_READWRITE(Raytracing, ShaderReplacement, bool,  "Shader replacement");
    CLASS_PROP_READONLY( Raytracing, TotalRays,        UINT64, "Total rays produced during the test in the last run (read-only)");
    CLASS_PROP_READONLY( Raytracing, RaysPerSecond,    double, "Rays-Per-Second in the last run (read-only)");

    using Mats = ::VkFusion::Mats;

    JS_CLASS_WITH_FLAGS(C_VkFusionMats, JSCLASS_HAS_PRIVATE);
    SObject Mats_Object("Mats", C_VkFusionMatsClass, nullptr, 0, "Mats config");

    CLASS_PROP_READWRITE(Mats, RunMask,           UINT32, "Bitmask indicating in which groups to run the subtest");
    CLASS_PROP_READWRITE(Mats, NumJobs,           UINT32, "Number of round-robin jobs");
    CLASS_PROP_READWRITE(Mats, MaxFbMb,           UINT32, "Amount of memory to allocate for testing");
    CLASS_PROP_READWRITE(Mats, BoxSizeKb,         UINT32, "Box size in KB");
    CLASS_PROP_READWRITE(Mats, UseSysmem,         bool,   "Allocate sysmem instead of vidmem");
    CLASS_PROP_READWRITE(Mats, NumBlitLoops,      UINT32, "Number of blit loops per job");
    CLASS_PROP_READWRITE(Mats, NumGraphicsQueues, UINT32, "Number of graphics queues to use");
    CLASS_PROP_READWRITE(Mats, NumComputeQueues,  UINT32, "Number of compute queues to use");
    CLASS_PROP_READWRITE(Mats, NumTransferQueues, UINT32, "Number of transfer queues to use");
    CLASS_PROP_READONLY (Mats, TotalGB,           UINT32, "Total number of GB transferred (read-only)");
    CLASS_PROP_READONLY (Mats, GBPerSecond,       double, "GB transferred per second (read-only)");
}
#endif

#ifndef VerbosePrintf
void VkFusion::Subtest::VerbosePrintf(const char* format, ...) const
{
    va_list args;
    va_start(args, format);

    ModsExterlwAPrintf(m_VerbosePri,
                       Tee::GetTeeModuleCoreCode(),
                       Tee::SPS_NORMAL,
                       format,
                       args);

    va_end(args);
}
#endif

VkResult VkFusion::Subtest::AllocateImage
(
    VulkanImage*          pImage,
    VkFormat              format,
    UINT32                width,
    UINT32                height,
    UINT32                mipmapLevels,
    VkImageUsageFlags     usage,
    VkImageTiling         tiling,
    VkMemoryPropertyFlags location
) const
{
    *pImage = VulkanImage(m_pVulkanDev);

    if (format == VK_FORMAT_UNDEFINED)
        pImage->PickColorFormat();
    else
        pImage->SetFormat(format);

    VulkanImage::ImageType imageType = VulkanImage::ImageType::INVALID;
    switch (pImage->GetFormat())
    {
        case VK_FORMAT_D16_UNORM:          imageType = VulkanImage::ImageType::DEPTH; break;
        case VK_FORMAT_D32_SFLOAT:         imageType = VulkanImage::ImageType::DEPTH; break;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: imageType = VulkanImage::ImageType::DEPTH_STENCIL; break;
        case VK_FORMAT_D24_UNORM_S8_UINT:  imageType = VulkanImage::ImageType::DEPTH_STENCIL; break;
        case VK_FORMAT_D16_UNORM_S8_UINT:  imageType = VulkanImage::ImageType::DEPTH_STENCIL; break;
        default:                           imageType = VulkanImage::ImageType::COLOR; break;
    }

    pImage->SetImageProperties(width,
                               height,
                               mipmapLevels,
                               imageType);

    VkResult res = VK_SUCCESS;

    CHECK_VK_RESULT(pImage->CreateImage(static_cast<VkImageCreateFlags>(0),
                                        usage,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        tiling,
                                        VK_SAMPLE_COUNT_1_BIT,
                                        "Image"));

    CHECK_VK_RESULT(pImage->AllocateAndBindMemory(location));

    CHECK_VK_RESULT(pImage->CreateImageView());

    return VK_SUCCESS;
}

VkFusion::Test::SubtestData::SubtestData(SubtestData&& other)
: pSubtest(other.pSubtest),
  queues(move(other.queues)),
  nextFrame(other.nextFrame.load()),
  lastFrameChecked(other.lastFrameChecked),
  continuous(other.continuous),
  rc(other.rc.load())
{
}

VkFusion::Test::SubtestThread::SubtestThread(SubtestThread&& other)
: pSubtestData(other.pSubtestData),
  threadId(other.threadId),
  exitFlag(other.exitFlag.load())
{
}

VkFusion::Test::Test()
{
    SetName("VkFusion");
}

VkFusion::Test::~Test()
{
}

void VkFusion::Test::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    static const char* const tf[2] = { "false", "true" };

    static const char* const grGeomTypeStr[] =
    {
        "vertex buffer",
        "tessellated patch",
    };
    const size_t grGeomTypeIdx = min(static_cast<size_t>(m_Graphics.GetGeometryType()), NUMELEMS(grGeomTypeStr) - 1);
    const char* const grGeomTypeName = grGeomTypeStr[grGeomTypeIdx];

    static const char* const grRTGeomTypeStr[] =
    {
        "lwbes",
        "toruses",
    };
    const size_t grRTGeomTypeIdx = min(static_cast<size_t>(m_Graphics.GetRTGeometryType()), NUMELEMS(grRTGeomTypeStr) - 1);
    const char* const grRTGeomTypeName = grRTGeomTypeStr[grRTGeomTypeIdx];

    static const char* const rtGeomTypeStr[] =
    {
        "triangles",
        "AABBs",
        "AABBs (trivial intersection)"
    };
    const size_t rtGeomTypeIdx = min(static_cast<size_t>(m_Raytracing.GetGeometryType()), NUMELEMS(rtGeomTypeStr) - 1);
    const char* const rtGeomTypeName = rtGeomTypeStr[rtGeomTypeIdx];

    static const char* const rtGeomLevelStr[] =
    {
        "primitives",
        "geometries",
        "instances"
    };
    const size_t rtGeomLevelIdx = min(static_cast<size_t>(m_Raytracing.GetGeometryLevel()), NUMELEMS(rtGeomLevelStr) - 1);
    const char* const rtGeomLevelName = rtGeomLevelStr[rtGeomLevelIdx];

    Printf(pri, "VkFusion Js Properties:\n");
    Printf(pri, "\t%-32s%u\n",      "RuntimeMs:",                   m_RuntimeMs);
    Printf(pri, "\t%-32s%s\n",      "KeepRunning:",                 tf[m_KeepRunning]);
    Printf(pri, "\t%-32s%s\n",      "EnableValidation:",            tf[m_EnableValidation]);
    Printf(pri, "\t%-32s%s\n",      "PrintExtensions:",             tf[m_PrintExtensions]);
    Printf(pri, "\t%-32s%u\n",      "Group0Us:",                    m_Group0Us);
    Printf(pri, "\t%-32s%u\n",      "Group1Us:",                    m_Group1Us);
    Printf(pri, "\t%-32s%u\n",      "Group2Us:",                    m_Group2Us);
    Printf(pri, "\t%-32s%u\n",      "Group3Us:",                    m_Group3Us);
    Printf(pri, "\t%-32s%u\n",      "Graphics.RunMask:",            m_Graphics.GetRunMask());
    Printf(pri, "\t%-32s%u\n",      "Graphics.NumJobs:",            m_Graphics.GetNumJobs());
    Printf(pri, "\t%-32s%u\n",      "Graphics.SurfaceWidth:",       m_Graphics.GetSurfaceWidth());
    Printf(pri, "\t%-32s%u\n",      "Graphics.SurfaceHeight:",      m_Graphics.GetSurfaceHeight());
    Printf(pri, "\t%-32s%u\n",      "Graphics.SurfaceBits:",        m_Graphics.GetSurfaceBits());
    if (m_Graphics.GetDrawsPerFrame() == Graphics::defaultDrawsPerFrame)
    {
        Printf(pri, "\t%-32s%s\n",  "Graphics.DrawsPerFrame:",      "automatic");
    }
    else
    {
        Printf(pri, "\t%-32s%u\n",  "Graphics.DrawsPerFrame:",      m_Graphics.GetDrawsPerFrame());
    }
    Printf(pri, "\t%-32s%u\n",      "Graphics.DrawsPerFrame:",      m_Graphics.GetDrawsPerFrame());
    Printf(pri, "\t%-32s%u\n",      "Graphics.NumLights:",          m_Graphics.GetNumLights());
    Printf(pri, "\t%-32s%u (%s)\n", "Graphics.GeometryType:",       m_Graphics.GetGeometryType(), grGeomTypeName);
    Printf(pri, "\t%-32s%u\n",      "Graphics.NumVertices:",        m_Graphics.GetNumVertices());
    Printf(pri, "\t%-32s%u\n",      "Graphics.Fov:",                m_Graphics.GetFov());
    Printf(pri, "\t%-32s%f\n",      "Graphics.MajorR:",             m_Graphics.GetMajorR());
    Printf(pri, "\t%-32s%f\n",      "Graphics.MinorR:",             m_Graphics.GetMinorR());
    Printf(pri, "\t%-32s%s\n",      "Graphics.TorusMajor:",         tf[m_Graphics.GetTorusMajor()]);
    Printf(pri, "\t%-32s%s\n",      "Graphics.Lwll:",               tf[m_Graphics.GetLwll()]);
    Printf(pri, "\t%-32s%f\n",      "Graphics.Rotation:",           m_Graphics.GetRotation());
    Printf(pri, "\t%-32s%f\n",      "Graphics.RotationStep:",       m_Graphics.GetRotationStep());
    Printf(pri, "\t%-32s%u\n",      "Graphics.NumTexReads:",        m_Graphics.GetNumTexReads());
    Printf(pri, "\t%-32s%u\n",      "Graphics.NumTextures:",        m_Graphics.GetNumTextures());
    Printf(pri, "\t%-32s%u\n",      "Graphics.TexSize:",            m_Graphics.GetTexSize());
    Printf(pri, "\t%-32s%f\n",      "Graphics.MipLoadBias:",        m_Graphics.GetMipLodBias());
    Printf(pri, "\t%-32s%s\n",      "Graphics.DebugMipMaps:",       tf[m_Graphics.GetDebugMipMaps()]);
    Printf(pri, "\t%-32s%u\n",      "Graphics.RaysPerPixel:",       m_Graphics.GetRaysPerPixel());
    Printf(pri, "\t%-32s%u (%s)\n", "Graphics.RTGeometryType:",     m_Graphics.GetRTGeometryType(), grRTGeomTypeName);
    Printf(pri, "\t%-32s%u\n",      "Graphics.RTTLASWidth:",        m_Graphics.GetRTTLASWidth());
    Printf(pri, "\t%-32s%u\n",      "Graphics.RTXMask:",            m_Graphics.GetRTXMask());
    Printf(pri, "\t%-32s%s\n",      "Graphics.Check:",              tf[m_Graphics.GetCheck()]);
    Printf(pri, "\t%-32s%s\n",      "Graphics.ShaderReplacement:",  tf[m_Graphics.GetShaderReplacement()]);
    Printf(pri, "\t%-32s%u\n",      "Raytracing.RunMask:",          m_Raytracing.GetRunMask());
    Printf(pri, "\t%-32s%u\n",      "Raytracing.NumJobs:",          m_Raytracing.GetNumJobs());
    Printf(pri, "\t%-32s%u\n",      "Raytracing.SurfaceWidth:",     m_Raytracing.GetSurfaceWidth());
    Printf(pri, "\t%-32s%u\n",      "Raytracing.SurfaceHeight:",    m_Raytracing.GetSurfaceHeight());
    Printf(pri, "\t%-32s%u\n",      "Raytracing.NumLwbesXY:",       m_Raytracing.GetNumLwbesXY());
    Printf(pri, "\t%-32s%u\n",      "Raytracing.NumLwbesZ:",        m_Raytracing.GetNumLwbesZ());
    Printf(pri, "\t%-32s%u\n",      "Raytracing.BLASSize:",         m_Raytracing.GetBLASSize());
    Printf(pri, "\t%-32s%f\n",      "Raytracing.MinLwbeSize:",      m_Raytracing.GetMinLwbeSize());
    Printf(pri, "\t%-32s%f\n",      "Raytracing.MaxLwbeSize:",      m_Raytracing.GetMaxLwbeSize());
    Printf(pri, "\t%-32s%f\n",      "Raytracing.MinLwbeOffset:",    m_Raytracing.GetMinLwbeOffset());
    Printf(pri, "\t%-32s%f\n",      "Raytracing.MaxLwbeOffset:",    m_Raytracing.GetMaxLwbeOffset());
    Printf(pri, "\t%-32s%u (%s)\n", "Raytracing.GeometryType:",     m_Raytracing.GetGeometryType(), rtGeomTypeName);
    Printf(pri, "\t%-32s%u (%s)\n", "Raytracing.GeometryLevel:",    m_Raytracing.GetGeometryLevel(), rtGeomLevelName);
    Printf(pri, "\t%-32s%s\n",      "Raytracing.Check:",            tf[m_Raytracing.GetCheck()]);
    Printf(pri, "\t%-32s%s\n",      "Raytracing.ShaderReplacement:", tf[m_Raytracing.GetShaderReplacement()]);
    Printf(pri, "\t%-32s%u\n",      "Mats.RunMask:",                m_Mats.GetRunMask());
    Printf(pri, "\t%-32s%u\n",      "Mats.NumJobs:",                m_Mats.GetNumJobs());
    Printf(pri, "\t%-32s%f\n",      "Mats.MaxFbMb:",                m_Mats.GetMaxFbMb());
    Printf(pri, "\t%-32s%u\n",      "Mats.BoxSizeKb:",              m_Mats.GetBoxSizeKb());
    Printf(pri, "\t%-32s%u\n",      "Mats.UseSysmem:",              m_Mats.GetUseSysmem());
    Printf(pri, "\t%-32s%u\n",      "Mats.NumBlitLoops:",           m_Mats.GetNumBlitLoops());
    Printf(pri, "\t%-32s%u\n",      "Mats.NumGraphicsQueues:",      m_Mats.GetNumGraphicsQueues());
    Printf(pri, "\t%-32s%u\n",      "Mats.NumComputeQueues:",       m_Mats.GetNumComputeQueues());
    Printf(pri, "\t%-32s%u\n",      "Mats.NumTransferQueues:",      m_Mats.GetNumTransferQueues());
}

RC VkFusion::Test::AddExtraObjects(JSContext* cx, JSObject* obj)
{
#ifndef VULKAN_STANDALONE
    RC rc;

    CHECK_RC(GpuTest::AddExtraObjects(cx, obj));

    CHECK_RC(AddExtraObject(cx,
                            obj,
                            VkFusionRegisterJS::Graphics_Object.GetJSObject(),
                            "Graphics",
                            &VkFusionRegisterJS::C_VkFusionGraphicsClass,
                            &m_Graphics));

    CHECK_RC(AddExtraObject(cx,
                            obj,
                            VkFusionRegisterJS::Raytracing_Object.GetJSObject(),
                            "Raytracing",
                            &VkFusionRegisterJS::C_VkFusionRaytracingClass,
                            &m_Raytracing));

    CHECK_RC(AddExtraObject(cx,
                            obj,
                            VkFusionRegisterJS::Mats_Object.GetJSObject(),
                            "Mats",
                            &VkFusionRegisterJS::C_VkFusionMatsClass,
                            &m_Mats));
#endif
    return RC::OK;
}

RC VkFusion::Test::AddExtraObject
(
    JSContext*  cx,
    JSObject*   containerObj,
    JSObject*   protoObj,
    const char* memberName,
    JSClass*    classObj,
    Subtest*    pSubtest
)
{
    JSObject* pMember = JS_DefineObject(cx, containerObj, memberName, classObj, protoObj, 0);
    if (!pMember)
    {
        JS_ReportWarning(cx, "Failed to create member object");
        return RC::SOFTWARE_ERROR;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, pMember, "Help", &C_Global_Help, 1, 0);

    if (!JS_SetPrivate(cx, pMember, pSubtest))
    {
        JS_ReportWarning(cx, "Failed to create member object");
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

bool VkFusion::Test::IsSupported()
{
    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_VULKAN))
    {
        return false;
    }

    const auto NeedsRT = [&]() -> bool
    {
        if (m_Raytracing.GetRunMask())
        {
            return true;
        }

        if (m_Graphics.GetRunMask() && m_Graphics.GetRaysPerPixel())
        {
            return true;
        }

        return false;
    };

    if (NeedsRT())
    {
        const UINT32 numRtCores = GetBoundGpuSubdevice()->GetRTCoreCount();
        VerbosePrintf("Found %u RT cores\n", numRtCores);
        if (numRtCores == 0)
        {
            return false;
        }
    }

    return true;
}

RC VkFusion::Test::Setup()
{
    RC rc;

    ////////////////////////////////////////////////////////////
    // Boiler-plate initialization

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());

    CHECK_RC(m_MglTestCtx.SetProperties(GetTestConfiguration(),
                                        false,  // DoubleBuffer
                                        GetBoundGpuDevice(),
                                        GetDispMgrReqs(),
                                        0,      // ColorSurfaceFormatOverride
                                        false,  // ForceFbo
                                        false,  // RenderToSysmem
                                        0));    // NumLayersInFBO
    if (!m_MglTestCtx.IsSupported())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(m_MglTestCtx.Setup());

    ////////////////////////////////////////////////////////////
    // Vulkan initialization

    constexpr bool enableVkDebug = true;
    vector<string> extensionNames;

    // Initialize Vulkan instance and create non-initialized device
    CHECK_VK_TO_RC(m_VulkanInst.InitVulkan(enableVkDebug,
                                           GetBoundGpuSubdevice()->GetGpuInst(),
                                           extensionNames,
                                           false, // initializeDevice
                                           m_EnableValidation,
                                           m_PrintExtensions));
    VulkanInstance::ClearDebugMessagesToIgnore();
    m_pVulkanDev = m_VulkanInst.GetVulkanDevice();
    MASSERT(m_pVulkanDev);

#ifdef VULKAN_STANDALONE
    GetBoundGpuSubdevice()->SetDeviceId(m_pVulkanDev->GetPhysicalDevice()->GetVendorID(),
                                        m_pVulkanDev->GetPhysicalDevice()->GetDeviceID());
#endif

    ////////////////////////////////////////////////////////////
    // Prepare subtests

    const auto AddSubtest = [&](Subtest* pSubtest)
    {
        if (pSubtest->GetRunMask())
        {
            m_Subtests.emplace_back();
            m_Subtests.back().pSubtest = pSubtest;
        }
    };

    m_Subtests.reserve(3);
    AddSubtest(&m_Graphics);
    AddSubtest(&m_Raytracing);
    AddSubtest(&m_Mats);

    if (m_Subtests.empty())
    {
        Printf(Tee::PriError, "No VkFusion subtest selected!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (m_Mats.GetRunMask())
    {
        m_Mats.SetMemError(GetMemError(0));
    }

    for (const SubtestData& subtest : m_Subtests)
    {
        subtest.pSubtest->SetVerbosePri(GetVerbosePrintPri());
        subtest.pSubtest->SetTimeoutMs(static_cast<UINT32>(GetTestConfiguration()->TimeoutMs()));
        subtest.pSubtest->SetRandomSeed(GetTestConfiguration()->Seed());
        subtest.pSubtest->SetDumpPng(GetGoldelwalues()->GetDumpPng());
        subtest.pSubtest->SetInstance(&m_VulkanInst);
        subtest.pSubtest->SetWindowsSwapChain(m_Hinstance, m_HWindow);
        if (m_DetachedPresentMs)
        {
            subtest.pSubtest->ForceModsSwapChain();
        }
    }

    QueueCounts queues = { };
    CHECK_RC(SelectQueues(&queues));

    ////////////////////////////////////////////////////////////
    // Initialize device

    // Get list of required extensions from subtests
    vector<const char*> extensions;
    for (const SubtestData& subtest : m_Subtests)
    {
        const ExtensionList ext = subtest.pSubtest->GetRequiredExtensions();
        extensions.reserve(extensions.size() + ext.numExtensions);

        // Deduplicate extensions from subtests
        for (UINT32 i = 0; i < ext.numExtensions; i++)
        {
            const char* const lwrExt = ext.extensions[i];
            const auto foundIter = find_if(extensions.begin(), extensions.end(),
                    [lwrExt](const char* foundExt) { return strcmp(foundExt, lwrExt) == 0; });
            if (foundIter == extensions.end())
            {
                extensions.push_back(lwrExt);
            }
        }
    }

    // Check if extensions are supported
    for (const char* ext : extensions)
    {
        if (!m_pVulkanDev->HasExt(ext))
        {
            Printf(Tee::PriNormal, "The required extension \"%s\" is not supported\n", ext);
            return RC::MODS_VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        extensionNames.emplace_back(ext);
    }

    // Initialize device with required number of queues
    CHECK_VK_TO_RC(m_pVulkanDev->Initialize(extensionNames,
                                            m_PrintExtensions,
                                            queues.graphics,
                                            queues.transfer,
                                            queues.compute));

    m_pVulkanDev->SetTimeoutNs(static_cast<UINT64>(GetTestConfiguration()->TimeoutMs() * 1'000'000U));

    ////////////////////////////////////////////////////////////
    // Create swapchain
    // At the moment this is only used for tools like Nsight for vktest

    if (m_DetachedPresentQueueIdx != ~0U)
    {
        m_PresentFence = VulkanFence(m_pVulkanDev);
        CHECK_VK_TO_RC(m_PresentFence.CreateFence());
        m_pSwapChain = make_unique<FakeSwapChain>(&m_VulkanInst, m_pVulkanDev);
        CHECK_VK_TO_RC(m_pSwapChain->Init(m_Hinstance,
                                          m_HWindow,
                                          SwapChain::MULTI_IMAGE_MODE,
                                          m_PresentFence.GetFence(),
                                          m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx(),
                                          m_DetachedPresentQueueIdx));
    }

    ////////////////////////////////////////////////////////////
    // Setup subtests

    CHECK_RC(SetupSubtestGroups());
    for (SubtestData& subtest : m_Subtests)
    {
        PROFILE_NAMED_ZONE(VULKAN, "SubtestSetup")
        PROFILE_ZONE_SET_NAME(subtest.pSubtest->GetName(), strlen(subtest.pSubtest->GetName()))
        CHECK_RC(SetupJobQueues(subtest));
        CHECK_RC(subtest.pSubtest->Setup());
    }

    return RC::OK;
}

RC VkFusion::Test::SelectQueues(QueueCounts* pOutQueues)
{
    VulkanPhysicalDevice* const pPhysDev = m_pVulkanDev->GetPhysicalDevice();
    const QueueCounts availableQueues =
    {
        pPhysDev->GetMaxGraphicsQueues(),
        pPhysDev->GetMaxComputeQueues(),
        pPhysDev->GetMaxTransferQueues()
    };
    const UINT32 gfxFam     = pPhysDev->GetGraphicsQueueFamilyIdx();
    const UINT32 computeFam = pPhysDev->GetComputeQueueFamilyIdx();
    const UINT32 xferFam    = pPhysDev->GetTransferQueueFamilyIdx();

    // Subtests run in groups (batches).  For example, first group can include Graphics and
    // Raytracing and second group can include Mats (transfer).  If there is more than one
    // group, these groups run in a round-robin fashion, i.e. one group at a time.  Within
    // each group, all tests run conlwrrently.
    //
    // Find how many queues of each type are needed and assign queues to subtests.
    // A subtest running in multiple groups will always use the same queues, so tests generally
    // have to use different queues.  However, if there are two non-overlapping tests which never
    // run together, they can use the same queues.

    vector<QueueCounts> groupQueueMask; // Bitmask of queues already used in each group
    QueueCounts         numQueues;      // Number of queues allocated for each family

    for (const SubtestData& subtest : m_Subtests)
    {
        // A function to iterate over groups in which the current subtest will run
        const auto ForEachGroup = [&](auto func)
        {
            UINT32 runMask = subtest.pSubtest->GetRunMask();
            while (runMask)
            {
                const INT32 groupId = Utility::BitScanForward(runMask);

                func(groupId);

                runMask &= ~(1u << groupId);
            }
        };

        // A function to return queue mask for a specific group id
        // Also creates a new group if it does not exist
        const auto GetGroupQueueMask = [&](UINT32 groupId) -> QueueCounts&
        {
            if (groupId >= groupQueueMask.size())
            {
                groupQueueMask.resize(groupId + 1);
            }
            return groupQueueMask[groupId];
        };

        // Get the bitmask of queues already used by groups in which this test will run
        QueueCounts queueMask;
        ForEachGroup([&](UINT32 groupId)
        {
            if (groupId < groupQueueMask.size())
            {
                const QueueCounts& groupMask = groupQueueMask[groupId];
                queueMask.graphics |= groupMask.graphics;
                queueMask.compute  |= groupMask.compute;
                queueMask.transfer |= groupMask.transfer;
            }
        });

        // Allocate queues for this test
        QueueCounts requiredQueues = subtest.pSubtest->GetRequiredQueues();
        vector<Queue> foundQueues;
        foundQueues.reserve(requiredQueues.graphics + requiredQueues.compute + requiredQueues.transfer);
        string assignedQueues;

        VerbosePrintf("Subtest %s, group mask 0x%x, requires queues: %u gr, %u co, %u tr\n",
                      subtest.pSubtest->GetName(), subtest.pSubtest->GetRunMask(),
                      requiredQueues.graphics, requiredQueues.compute, requiredQueues.transfer);

        UINT32 queueIdx = 0;
        while (requiredQueues.graphics)
        {
            if (queueIdx >= availableQueues.graphics)
            {
                Printf(Tee::PriError,
                       "Insufficient number of graphics queues on this device to run subtest %s\n",
                       subtest.pSubtest->GetName());
                return RC::MODS_VK_ERROR_FEATURE_NOT_PRESENT;
            }

            const UINT32 queueBit = 1u << queueIdx;

            if (!(queueMask.graphics & queueBit))
            {
                foundQueues.emplace_back(gfxFam, queueIdx,
                        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
                assignedQueues += Utility::StrPrintf("%sgr %u",
                                                     assignedQueues.empty() ? "" : ", ", queueIdx);
                ForEachGroup([&](UINT32 groupId)
                {
                    GetGroupQueueMask(groupId).graphics |= queueBit;
                });
                --requiredQueues.graphics;
            }

            ++queueIdx;
        }
        numQueues.graphics = max(numQueues.graphics, queueIdx);

        queueIdx = 0;
        while (requiredQueues.compute)
        {
            // TODO promote to graphics queue if we run out of compute queues?
            if (queueIdx >= availableQueues.compute)
            {
                Printf(Tee::PriError,
                       "Insufficient number of compute queues on this device to run subtest %s\n",
                       subtest.pSubtest->GetName());
                return RC::MODS_VK_ERROR_FEATURE_NOT_PRESENT;
            }

            const UINT32 queueBit = 1u << queueIdx;

            if (!(queueMask.compute & queueBit))
            {
                foundQueues.emplace_back(computeFam, queueIdx,
                        VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
                assignedQueues += Utility::StrPrintf("%sco %u",
                                                     assignedQueues.empty() ? "" : ", ", queueIdx);
                ForEachGroup([&](UINT32 groupId)
                {
                    GetGroupQueueMask(groupId).compute |= queueBit;
                });
                --requiredQueues.compute;
            }

            ++queueIdx;
        }
        numQueues.compute = max(numQueues.compute, queueIdx);

        queueIdx = 0;
        while (requiredQueues.transfer)
        {
            if (queueIdx >= availableQueues.transfer)
            {
                Printf(Tee::PriError,
                       "Insufficient number of transfer queues on this device to run subtest %s\n",
                       subtest.pSubtest->GetName());
                return RC::MODS_VK_ERROR_FEATURE_NOT_PRESENT;
            }

            const UINT32 queueBit = 1u << queueIdx;

            if (!(queueMask.transfer & queueBit))
            {
                foundQueues.emplace_back(xferFam, queueIdx, VK_QUEUE_TRANSFER_BIT);
                assignedQueues += Utility::StrPrintf("%str %u",
                                                     assignedQueues.empty() ? "" : ", ", queueIdx);
                ForEachGroup([&](UINT32 groupId)
                {
                    GetGroupQueueMask(groupId).transfer |= queueBit;
                });
                --requiredQueues.transfer;
            }

            ++queueIdx;
        }
        numQueues.transfer = max(numQueues.transfer, queueIdx);

        subtest.pSubtest->SetQueues(foundQueues);

        VerbosePrintf("Subtest %s assigned queues: %s\n",
                      subtest.pSubtest->GetName(), assignedQueues.c_str());
    }

    m_Groups.resize(groupQueueMask.size());

#ifdef VULKAN_STANDALONE_KHR
    if ((numQueues.graphics < availableQueues.graphics) && m_DetachedPresentMs)
    {
        m_DetachedPresentQueueIdx = numQueues.graphics++;
        VerbosePrintf("Fake present assigned queue: gr %u\n", m_DetachedPresentQueueIdx);
    }
#endif

    *pOutQueues = numQueues;
    return RC::OK;
}

RC VkFusion::Test::SetupJobQueues(SubtestData& subtest)
{
    constexpr UINT32 measureSubmissions = 8;

    const vector<Queue>& selQueues = subtest.pSubtest->GetQueues();

    subtest.queues.resize(selQueues.size());

    for (UINT32 qIdx = 0; qIdx < selQueues.size(); qIdx++)
    {
        JobQueue& jobQ = subtest.queues[qIdx];

        RC rc;
        jobQ.cmdPool = VulkanCmdPool(m_pVulkanDev);
        CHECK_VK_TO_RC(jobQ.cmdPool.InitCmdPool(selQueues[qIdx].family, selQueues[qIdx].idx));

        jobQ.jobs.resize(subtest.pSubtest->GetNumJobs());
        for (Job& job : jobQ.jobs)
        {
            CHECK_VK_TO_RC(job.Initialize(m_pVulkanDev,
                                          &jobQ.cmdPool,
                                          measureSubmissions));
        }
    }

    return RC::OK;
}

RC VkFusion::Test::SetupSubtestGroups()
{
    MASSERT(m_Groups.size() <= sizeof(UINT32) * 8);
    const UINT32 allGroupsMask = (1u << m_Groups.size()) - 1u;

    for (UINT32 groupId = 0; groupId < m_Groups.size(); groupId++)
    {
        const UINT32 groupMask = 1u << groupId;

        for (SubtestData& subtest : m_Subtests)
        {
            const UINT32 runMask = subtest.pSubtest->GetRunMask();

            if (runMask == allGroupsMask)
            {
                subtest.continuous = true;
                m_ContinuousTests.emplace_back(&subtest);
            }
            else if (runMask & groupMask)
            {
                m_Groups[groupId].subtests.push_back(&subtest);
            }
        }

        UINT32 targetUs = 0;
        switch (groupId % 4u)
        {
            case 0: targetUs = m_Group0Us; break;
            case 1: targetUs = m_Group1Us; break;
            case 2: targetUs = m_Group2Us; break;
            case 3: targetUs = m_Group3Us; break;
        }
        m_Groups[groupId].targetTimeUs = targetUs;
    }

    UINT32 numEmptyGroups = 0;
    for (const SubtestGroup& group : m_Groups)
    {
        if (group.subtests.empty())
        {
            ++numEmptyGroups;
        }
    }
    if (numEmptyGroups == m_Groups.size())
    {
        // All tests running continuoulsy
        m_Groups.clear();
        numEmptyGroups = 0;
    }

    // If there are empty groups, we need to add one continuous test to
    // all groups, we don't have any other way to create gaps between groups
    if (!m_ContinuousTests.empty() && (numEmptyGroups > 0))
    {
        SubtestData* pSubtestData = m_ContinuousTests.back().pSubtestData;
        pSubtestData->continuous = false;
        m_ContinuousTests.pop_back();

        for (SubtestGroup& group : m_Groups)
        {
            group.subtests.push_back(pSubtestData);
        }
    }

    // Print information about configuration of subtests
    if (m_Groups.empty())
    {
        VerbosePrintf("No subtest groups scheduled\n");
    }

    for (UINT32 groupId = 0; groupId < m_Groups.size(); groupId++)
    {
        const SubtestGroup& group = m_Groups[groupId];

        string testNames;
        for (const SubtestData* pSubtestData : group.subtests)
        {
            if (!testNames.empty())
            {
                testNames += ", ";
            }
            testNames += pSubtestData->pSubtest->GetName();
        }

        VerbosePrintf("Group %u mask 0x%x target %u us subtests: %s\n",
                      groupId, 1u << groupId, group.targetTimeUs, testNames.c_str());
    }

    if (m_ContinuousTests.empty())
    {
        VerbosePrintf("No continuous subtests scheduled\n");
    }
    else
    {
        string testNames;
        for (const SubtestThread& thread : m_ContinuousTests)
        {
            if (!testNames.empty())
            {
                testNames += ", ";
            }
            testNames += thread.pSubtestData->pSubtest->GetName();
        }
        VerbosePrintf("Continuous subtests: %s\n", testNames.c_str());
    }

    return RC::OK;
}

RC VkFusion::Test::Cleanup()
{
    StickyRC rc;

    for (SubtestData& subtest : m_Subtests)
    {
        rc = subtest.pSubtest->Cleanup();
    }

    m_Groups.clear();
    m_Subtests.clear();
    m_pSwapChain.reset(nullptr);
    m_PresentCompleteSem.Cleanup();
    m_PresentFence.Cleanup();

    if (m_pVulkanDev)
    {
        m_pVulkanDev->Shutdown();
        m_pVulkanDev = nullptr;
    }

    m_VulkanInst.DestroyVulkan();
    rc = m_MglTestCtx.Cleanup();
    rc = GpuTest::Cleanup();

    return rc;
}

RC VkFusion::Test::Run()
{
    if (m_Groups.empty() && m_ContinuousTests.empty())
    {
        Printf(Tee::PriError, "No subtests were selected to run\n");
        return RC::NO_TESTS_RUN;
    }

    const UINT64 startTimeMs = Xp::GetWallTimeMS();
    const UINT64 endTimeMs   = startTimeMs + m_RuntimeMs;
    const UINT64 numLoops    = GetTestConfiguration()->Loops();

    DEFER
    {
        StopContinuousSubtests();
        WaitForAllJobs();
        StopPresentThread();
    };

    Tasker::DetachThread detach;

    for (SubtestData& subtest : m_Subtests)
    {
        subtest.rc = RC::OK;
        subtest.pSubtest->PreRun();
    }

    RC rc;
    CHECK_RC(StartPresentThread());

    CHECK_RC(SpawnContinuousSubtests());
    if (m_Groups.empty())
    {
        VerbosePrintf("Running %s subtest on foreground thread\n",
                      m_ContinuousTests.front().pSubtestData->pSubtest->GetName());
    }

    while (m_KeepRunning ||
           (Xp::GetWallTimeMS() < endTimeMs) ||
           (!m_RuntimeMs && (GetNumScheduledFrames() < numLoops)))
    {
        if (m_Groups.empty())
        {
            MASSERT(!m_ContinuousTests.empty());
            CHECK_RC(RenderNextFrame(*m_ContinuousTests.front().pSubtestData));
        }
        else
        {
            CHECK_RC(ScheduleGroups());
        }

        CHECK_RC(Abort::Check());

        if (GetGoldelwalues()->GetStopOnError())
        {
            CHECK_RC(CheckSubtestErrors());
        }
    }

    CHECK_RC(StopContinuousSubtests());
    CHECK_RC(WaitForAllJobs());
    StopPresentThread();

    const UINT64 elapsedMs = Xp::GetWallTimeMS() - startTimeMs;

    for (SubtestData& subtest : m_Subtests)
    {
        VerbosePrintf("%s: ran %llu jobs\n", subtest.pSubtest->GetName(), subtest.nextFrame.load());
        for (const JobQueue& jobQ : subtest.queues)
        {
            Job::Stats stats;
            for (const Job& job : jobQ.jobs)
            {
                stats += job.GetDurationStats();
            }

            if (stats.numSamples)
            {
                const VulkanPhysicalDevice* const pPhysDev = m_pVulkanDev->GetPhysicalDevice();
                const UINT32 famIdx = jobQ.cmdPool.GetQueueFamilyIdx();
                const char* const queueName =
                    (famIdx == pPhysDev->GetGraphicsQueueFamilyIdx()) ? "gr" :
                    (famIdx == pPhysDev->GetComputeQueueFamilyIdx())  ? "co" :
                    (famIdx == pPhysDev->GetTransferQueueFamilyIdx()) ? "tr" : "??";
                VerbosePrintf("%s queue %s %u: average %llu ns, min %llu ns, max %llu ns\n",
                              subtest.pSubtest->GetName(),
                              queueName,
                              jobQ.cmdPool.GetQueueIdx(),
                              stats.GetAverage(),
                              stats.minimum,
                              stats.maximum);
            }
        }
        subtest.pSubtest->UpdateAndPrintStats(elapsedMs);
    }

    StickyRC finalRc;
    for (SubtestData& subtest : m_Subtests)
    {
        finalRc = CheckFinalFrames(subtest);
    }

    finalRc = CheckSubtestErrors();
    CHECK_RC(finalRc);

    return RC::OK;
}

UINT64 VkFusion::Test::GetNumScheduledFrames() const
{
    UINT64 maxFrames   = 0;
    bool   unscheduled = false;

    for (const SubtestData& subtest : m_Subtests)
    {
        const UINT64 numFrames = subtest.nextFrame;

        maxFrames = max(maxFrames, numFrames);

        if (!numFrames)
        {
            unscheduled = true;
        }
    }

    // If any subtest did not schedule any frames yet, return 0 to make sure that
    // every subtest gets a chance to run
    return unscheduled ? 0ull : maxFrames;
}

RC VkFusion::Test::CheckSubtestErrors() const
{
    for (const SubtestData& subtest : m_Subtests)
    {
        if (subtest.rc != RC::OK)
        {
            return RC(subtest.rc);
        }
    }

    return RC::OK;
}

RC VkFusion::Test::SpawnContinuousSubtests()
{
    Test* const pTest = this;

    for (UINT32 idx = 0; idx < m_ContinuousTests.size(); idx++)
    {
        // If there are no groups running, the first continuous test will run
        // on the main test thread
        if ((idx == 0) && m_Groups.empty())
        {
            continue;
        }

        SubtestThread& thread = m_ContinuousTests[idx];

        thread.exitFlag = false;

        SubtestThread* const pThread = &thread;

        string name = "VkFusion.";
        name += thread.pSubtestData->pSubtest->GetName();
        thread.threadId = Tasker::CreateThread(name.c_str(), [=]
        {
            pTest->RunContinuousSubtest(pThread);
        });
    }

    return RC::OK;
}

RC VkFusion::Test::StopContinuousSubtests()
{
    PROFILE_ZONE(VULKAN)

    for (SubtestThread& thread : m_ContinuousTests)
    {
        thread.exitFlag = true;
    }

    StickyRC rc;

    for (SubtestThread& thread : m_ContinuousTests)
    {
        if (thread.threadId != Tasker::NULL_THREAD)
        {
            rc = Tasker::Join(thread.threadId);

            thread.threadId = Tasker::NULL_THREAD;
        }
    }

    return rc;
}

void VkFusion::Test::RunContinuousSubtest(SubtestThread* pThread)
{
    VerbosePrintf("Start background subtest: %s\n", pThread->pSubtestData->pSubtest->GetName());

    Tasker::DetachThread detach;

    // Make sure the thread is accounted for in the GL driver
    dglThreadAttach();
    DEFER
    {
        dglThreadDetach();
    };

    while (!pThread->exitFlag)
    {
        const RC rc = RenderNextFrame(*pThread->pSubtestData);
        pThread->pSubtestData->rc = rc.Get();
    }

    pThread->pSubtestData->rc = WaitForAllJobs(*pThread->pSubtestData).Get();
    VerbosePrintf("Finished background subtest: %s\n", pThread->pSubtestData->pSubtest->GetName());
}

void VkFusion::Test::GetGroupSemaphores(const SubtestGroup&  group,
                                      vector<VkSemaphore> *pSemaphores)
{
    pSemaphores->clear();

    for (const SubtestData* pSubtestData : group.subtests)
    {
        UINT64 frameIdx = pSubtestData->nextFrame;
        if (!frameIdx)
        {
            pSemaphores->clear();
            break;
        }
        --frameIdx;

        const UINT32 numJobs = pSubtestData->pSubtest->GetNumJobs();
        const UINT32 jobIdx  = frameIdx % numJobs;

        for (const JobQueue& jobQueue : pSubtestData->queues)
        {
            const Job& job = jobQueue.jobs[jobIdx];
            pSemaphores->push_back(job.GetSemaphore());
        }
    }
}

RC VkFusion::Test::ScheduleGroups()
{
    vector<VkSemaphore> prevGroupSemaphores;
    GetGroupSemaphores(m_Groups.back(), &prevGroupSemaphores);

    for (SubtestGroup& group : m_Groups)
    {
        const UINT32       numSemaphores   = static_cast<UINT32>(prevGroupSemaphores.size());
        const VkSemaphore* pWaitSemaphores = numSemaphores ? &prevGroupSemaphores[0] : nullptr;

        for (SubtestData* pSubtestData : group.subtests)
        {
            const UINT32 numSubmissions = AdjustRunTime(*pSubtestData);

            for (UINT32 i = 0; i < numSubmissions; i++)
            {
                RC rc;
                CHECK_RC(RenderNextFrame(*pSubtestData,
                                         i == numSubmissions - 1,
                                         (i == 0) ? pWaitSemaphores : nullptr,
                                         (i == 0) ? numSemaphores : 0));
            }
        }

        GetGroupSemaphores(group, &prevGroupSemaphores);
    }

    return RC::OK;
}

UINT32 VkFusion::Test::AdjustRunTime(SubtestData& subtest)
{
    // TODO
    return 1;
}

RC VkFusion::Test::RenderNextFrame(SubtestData&       subtest,
                                 bool               signalSemaphore,
                                 const VkSemaphore* pWaitSemaphores,
                                 UINT32             numWaitSemaphores)
{
    const UINT64 frameIdx = subtest.nextFrame++;
    const UINT32 numJobs  = subtest.pSubtest->GetNumJobs();
    const UINT32 jobIdx   = frameIdx % numJobs;

    vector<VulkanCmdBuffer*> buffers;
    buffers.reserve(subtest.queues.size());

    // Wait for the job in this slot to finish on all queues,
    // then prepare new primary command buffers
    RC rc;
    for (JobQueue& jobQueue : subtest.queues)
    {
        PROFILE_NAMED_ZONE(VULKAN, "JobWait")

        Job& job = jobQueue.jobs[jobIdx];
        CHECK_VK_TO_RC(job.Wait(GetTestConfiguration()->TimeoutMs()));

        VulkanCmdBuffer* pCmdBuf = nullptr;
        CHECK_VK_TO_RC(job.Begin(&pCmdBuf));
        buffers.push_back(pCmdBuf);
    }

    PROFILE_ZONE(VULKAN)
#ifdef TRACY_ENABLE
    string zoneName = Utility::StrPrintf("%s job %u", subtest.pSubtest->GetName(), jobIdx);
    PROFILE_ZONE_SET_NAME(zoneName.data(), zoneName.size())
#endif

    const bool   havePrevFrame = frameIdx >= numJobs;
    const UINT64 prevFrameIdx  = havePrevFrame ? (frameIdx - numJobs) : ~0ULL;

    // Dump previous frame, unless this is the first submission for this job index
    if (havePrevFrame && GetGoldelwalues()->GetDumpPng())
    {
        if ((prevFrameIdx > subtest.lastFrameChecked) || (subtest.lastFrameChecked == ~0ULL))
        {
            CHECK_RC(subtest.pSubtest->Dump(jobIdx, prevFrameIdx));
        }
    }

    // Check results for the previous frame (only some subtests may support this)
    if (havePrevFrame)
    {
        subtest.rc = subtest.pSubtest->CheckFrame(jobIdx, prevFrameIdx, false).Get();
        subtest.lastFrameChecked = prevFrameIdx;
        if ((subtest.rc != RC::OK) && GetGoldelwalues()->GetStopOnError())
        {
            return RC(subtest.rc);
        }
    }

    // Ask the subtest to fill the primary command buffer
    VkSemaphore waitSema    = VK_NULL_HANDLE;
    VkSemaphore presentSema = VK_NULL_HANDLE;
    {
        PROFILE_VK_ZONE("ExelwteSubtest", *buffers.front()) // TODO remove this for transfer queue
        CHECK_RC(subtest.pSubtest->Execute(jobIdx,
                                           buffers.data(),
                                           static_cast<UINT32>(buffers.size()),
                                           &waitSema,
                                           &presentSema));
        PROFILE_VK_ZONE_COLLECT(*buffers.front())
    }

    vector<VkSemaphore> waitSemaphores;
    if (waitSema)
    {
        if (numWaitSemaphores)
        {
            waitSemaphores.insert(waitSemaphores.end(), pWaitSemaphores, pWaitSemaphores + numWaitSemaphores);
        }

        waitSemaphores.push_back(waitSema);
        pWaitSemaphores = waitSemaphores.data();
        ++numWaitSemaphores;
    }

    // Submit all primary command buffers for all queues of this subtest
    for (JobQueue& jobQueue : subtest.queues)
    {
        Job& job = jobQueue.jobs[jobIdx];
        CHECK_VK_TO_RC(job.Execute(signalSemaphore, presentSema, pWaitSemaphores, numWaitSemaphores));
    }

    CHECK_RC(subtest.pSubtest->Present(presentSema));

    return RC::OK;
}

RC VkFusion::Test::WaitForAllJobs(SubtestData& subtest)
{
    PROFILE_ZONE(VULKAN)
    PROFILE_ZONE_SET_NAME(subtest.pSubtest->GetName(), strlen(subtest.pSubtest->GetName()))

    StickyRC rc;

    for (JobQueue& jobQueue : subtest.queues)
    {
        for (Job& job : jobQueue.jobs)
        {
            rc = VkUtil::VkErrorToRC(job.Wait(GetTestConfiguration()->TimeoutMs()));
        }
    }

    return rc;
}

RC VkFusion::Test::WaitForAllJobs()
{
    StickyRC rc;

    for (SubtestData& subtest : m_Subtests)
    {
        rc = WaitForAllJobs(subtest);
    }

    return rc;
}

RC VkFusion::Test::CheckFinalFrames(SubtestData& subtest)
{
    PROFILE_ZONE(VULKAN)
    PROFILE_ZONE_SET_NAME(subtest.pSubtest->GetName(), strlen(subtest.pSubtest->GetName()))

    StickyRC rc;

    const UINT32 numJobs   = subtest.pSubtest->GetNumJobs();
    const UINT64 numFrames = subtest.nextFrame;
    for (UINT64 frameIdx = subtest.lastFrameChecked + 1; frameIdx < numFrames; frameIdx++)
    {
        const UINT32 jobIdx = frameIdx % numJobs;
        rc = subtest.pSubtest->CheckFrame(jobIdx, frameIdx, true);

        if (GetGoldelwalues()->GetDumpPng())
        {
            rc = subtest.pSubtest->Dump(jobIdx, frameIdx);
        }

        subtest.lastFrameChecked = frameIdx;
    }

    return rc;
}

RC VkFusion::Test::StartPresentThread()
{
    if (m_DetachedPresentQueueIdx == ~0U)
    {
        return RC::OK;
    }

    m_PresentCompleteSem = VulkanSemaphore(m_pVulkanDev);
    RC rc;
    CHECK_VK_TO_RC(m_PresentCompleteSem.CreateBinarySemaphore());

    MASSERT(m_PresentThread == Tasker::NULL_THREAD);

    m_PresentThreadQuitEvent = Tasker::AllocEvent("PresentQuit");
    MASSERT(m_PresentThreadQuitEvent);
    if (m_PresentThreadQuitEvent == nullptr)
    {
        return RC::SOFTWARE_ERROR;
    }

    m_PresentThread = Tasker::CreateThread("Present", [&]
    {
        Tasker::DetachThread detach;

        UINT64 presentIdx = 0;

        for (;; ++presentIdx)
        {
            const VkSemaphore presentSem = this->m_PresentCompleteSem.GetSemaphore();
            const UINT32 lwrSwapChainIdx = this->m_pSwapChain->GetNextSwapChainImage(presentSem);

            const VkResult res = m_pSwapChain->PresentImage(lwrSwapChainIdx,
                                                            presentSem,
                                                            VK_NULL_HANDLE,
                                                            true);
            if (res != VK_SUCCESS)
            {
                Printf(Tee::PriError, "PresentImage failed with VkResult=%d\n", res);
                break;
            }

            if (Tasker::WaitOnEvent(this->m_PresentThreadQuitEvent, this->m_DetachedPresentMs) == RC::OK)
            {
                break;
            }
        }

        Printf(Tee::PriLow, "Performed %llu presents\n", presentIdx);
    });
    MASSERT(m_PresentThread != Tasker::NULL_THREAD);
    if (m_PresentThread == Tasker::NULL_THREAD)
    {
        return RC::SOFTWARE_ERROR;
    }
    return RC::OK;
}

void VkFusion::Test::StopPresentThread()
{
    if (m_PresentThread != Tasker::NULL_THREAD)
    {
        Tasker::SetEvent(m_PresentThreadQuitEvent);
        Tasker::Join(m_PresentThread);
        m_PresentThread = Tasker::NULL_THREAD;

        Tasker::FreeEvent(m_PresentThreadQuitEvent);
        m_PresentThreadQuitEvent = nullptr;

        m_PresentCompleteSem.Cleanup();
    }
}
