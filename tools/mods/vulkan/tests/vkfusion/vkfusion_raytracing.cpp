/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vkfusion_raytracing.h"
#include "core/include/imagefil.h"

namespace
{
    enum GeometryType
    {
        GEOM_TRIANGLES,
        GEOM_AABBS,
        GEOM_AABBS_TRIVIAL
    };

    enum GeometryLevel
    {
        LEVEL_PRIMITIVES,
        LEVEL_GEOMETRIES,
        LEVEL_INSTANCES
    };

    using Vertex = lwmath::vec3f;

    struct RayParams
    {
        lwmath::vec3f cameraOrigin;
        UINT32        rayPerturbSeed;
    };

    struct ShaderSpecialization
    {
        float cameraDirX;
        float cameraDirY;
        float cameraDirZ;
        float tMax;
        float nearT;
        float farT;
        int   check;
    };
}

static const char* const s_RayGenShader = HS_(R"glsl(
    #version 460 core
    #extension GL_EXT_ray_tracing : require

    // Color returned from traced ray
    layout(location = 0) rayPayloadEXT float rayColor;

    layout(constant_id = 0) const float cameraDirX   = 0;   // Camera direction vector
    layout(constant_id = 1) const float cameraDirY   = 0;
    layout(constant_id = 2) const float cameraDirZ   = 1;
    layout(constant_id = 3) const float tMax         = 300; // Maximum ray distance
    layout(constant_id = 4) const float nearT        = 5;   // Distance of closest object from origin
    layout(constant_id = 5) const float farT         = 100; // Distance of farthest object from origin
    layout(constant_id = 6) const int   check        = 0;   // 0=render, 1=check

    layout(binding = 0, set = 0)        uniform accelerationStructureEXT scene;
    layout(binding = 1, set = 0, r32f)  uniform image2D  img;
    layout(binding = 2, set = 0, r32ui) uniform uimage2D errors;

    layout(push_constant) uniform RayParams
    {
        vec3 cameraOrigin;
        uint rayPerturbSeed; // Initial seed for ray perturbation
    };

    vec2 ComputePerturbDelta(uint perturbSeed)
    {
        if (perturbSeed == 0)
        {
            return vec2(0);
        }

        const int dx = int(perturbSeed & 0x3FFFU);
        const int dy = int((perturbSeed >> 14) & 0x3FFFU);

        return vec2(ivec2(dx, dy) - ivec2(8192)) / 8192.0f;
    }

    vec3 ComputeDir(vec3 dir, uint perturbSeed)
    {
        const vec2  halfSize     = vec2(gl_LaunchSizeEXT) / 2;
        // Output surface's aspect ratio
        const float ratio        = min(halfSize.x, halfSize.y);
        const vec2  dirDelta     = vec2(gl_LaunchIDEXT) - halfSize;
        const vec2  perturbDelta = ComputePerturbDelta(perturbSeed);
        return normalize(dir + vec3((dirDelta + perturbDelta) / ratio, 0));
    }

    // Trivial Linear Congruential Generator
    uint AdvanceSeed(uint seed)
    {
        return (seed * 1103515245) + 12345;
    }

    float TraceRay(vec3 cameraOrigin, uint seed)
    {
        const vec3 cameraDir = vec3(cameraDirX, cameraDirY, cameraDirZ);
        const vec3 dir       = ComputeDir(cameraDir, seed);

        const uint rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsLwllBackFacingTrianglesEXT;

        traceRayEXT(scene,             // topLevel
                    rayFlags,          // rayFlags
                    0xFF,              // lwllMask
                    0,                 // sbtRecordOffset
                    0,                 // sbtRecordStride
                    0,                 // missIndex
                    cameraOrigin,      // origin
                    0.001,             // Tmin
                    dir,               // direction
                    tMax,              // Tmax
                    0);                // payload (location = 0)

        return rayColor;
    }

    uint PixelId()
    {
        return gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;
    }

    uint RandomizeSeed(uint seed)
    {
        if (seed == 0)
        {
            return 0;
        }

        const uint pixelSeed = gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;
        return AdvanceSeed(seed ^ pixelSeed);
    }

    void main()
    {
        float color = TraceRay(cameraOrigin, RandomizeSeed(rayPerturbSeed));
        color = (max(nearT, min(farT, color)) - nearT) / (farT - nearT);

        const ivec2 pixelId = ivec2(gl_LaunchIDEXT.xy);
        if (check != 0)
        {
            const float expected = imageLoad(img, pixelId).x;
            if (color != expected)
            {
                const uint numErrors = imageLoad(errors, pixelId).x;
                if (numErrors < 0xFFFFFFFFU)
                {
                    imageStore(errors, pixelId, uvec4(numErrors + 1, 0, 0, 0));
                }
                imageStore(img, pixelId, vec4(color, 0, 0, 0));
            }
        }
        else
        {
            imageStore(img, pixelId, vec4(color, 0, 0, 0));
            imageStore(errors, pixelId, uvec4(0, 0, 0, 0));
        }
    }
)glsl");

static const char* const s_IntersectAABBShader = HS_(R"glsl(
    #version 460 core
    #extension GL_EXT_ray_tracing         : require
    #extension GL_EXT_scalar_block_layout : enable

    struct Aabb
    {
        vec3 minimum;
        vec3 maximum;
    };

    // 'scalar' layout from GL_EXT_scalar_block_layout produces alignment
    // consistent with the host, e.g. vec3 is aligned on 4 bytes instead of 16.
    layout(binding = 3, set = 0, scalar) buffer Geometry
    {
        Aabb aabbs[];
    };

    float IntersectAABB(vec3 origin, vec3 dir, Aabb aabb)
    {
        const vec3 ilwDir = 1.0 / dir;

        const vec3 tMin1 = (aabb.minimum - origin) * ilwDir;
        const vec3 tMax1 = (aabb.maximum - origin) * ilwDir;

        const vec3 tMin2 = min(tMin1, tMax1);
        const vec3 tMax2 = max(tMin1, tMax1);

        const float t0 = max(tMin2.x, max(tMin2.y, tMin2.z));
        const float t1 = min(tMax2.x, min(tMax2.y, tMax2.z));

        return (t0 < t1)
                   ? ((t0 > 0) ? t0 : t1)
                   : -1;
    }

    void main()
    {
        const uint  idx  = gl_PrimitiveID + gl_GeometryIndexEXT;
        const float tHit = IntersectAABB(gl_ObjectRayOriginEXT,
                                         gl_ObjectRayDirectionEXT,
                                         aabbs[idx]);

        // Note: tHit values outside of the ray's [tMin,tMax] range
        // will be rejected, i.e. treated as a miss
        reportIntersectionEXT(tHit, 0);
    }
)glsl");

static const char* const s_SimpleIntersectShader = HS_(R"glsl(
    #version 460 core
    #extension GL_EXT_ray_tracing : require

    void main()
    {
        reportIntersectionEXT(gl_RayTmaxEXT, 0);
    }
)glsl");

static const char* const s_HitShader = HS_(R"glsl(
    #version 460 core
    #extension GL_EXT_ray_tracing : require

    layout(location = 0) rayPayloadInEXT float outColor;

    void main()
    {
        outColor = gl_HitTEXT;
    }
)glsl");

static const char* const s_MissShader = HS_(R"glsl(
    #version 460 core
    #extension GL_EXT_ray_tracing : require

    layout(location = 0) rayPayloadInEXT float outColor;

    void main()
    {
        outColor = 0;
    }
)glsl");

VkFusion::QueueCounts VkFusion::Raytracing::GetRequiredQueues() const
{
    QueueCounts counts = { 0, 1, 0 };
    return counts;
}

VkFusion::ExtensionList VkFusion::Raytracing::GetRequiredExtensions() const
{
    static const char* const extensions[] =
    {
        "VK_KHR_deferred_host_operations", // required by VK_KHR_acceleration_structure
        "VK_KHR_acceleration_structure",   // required by VK_KHR_ray_tracing_pipeline
        "VK_KHR_ray_tracing_pipeline",     // Raytracing extension
        "VK_EXT_scalar_block_layout"       // scalar layout in shaders
    };

    ExtensionList list = { extensions, static_cast<UINT32>(NUMELEMS(extensions)) };
    return list;
}

RC VkFusion::Raytracing::Setup()
{
    if (!m_SurfaceWidth || !m_SurfaceHeight)
    {
        Printf(Tee::PriError, "Surface size not set!\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (!m_NumLwbesXY || !m_NumLwbesZ || !m_BLASSize)
    {
        Printf(Tee::PriError, "Invalid number of boxes!\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    m_Random.SeedRandom(m_RandomSeed ^ 0x53B0DF22U); // Unique seed for this subtest

    // Check if raytracing is supported on this device
    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& rtFeatures =
        m_pVulkanDev->GetPhysicalDevice()->GetRayTracingPipelineFeatures();
    if (!rtFeatures.rayTracingPipeline)
    {
        Printf(Tee::PriError, "RayTracing is not supported on this GPU!\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    // Get raytracing properties from physical device
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR    rtProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
    VkPhysicalDeviceProperties2KHR props2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR };
    asProperties.pNext = &rtProperties;
    props2.pNext       = &asProperties;
    m_pVulkanInst->GetPhysicalDeviceProperties2KHR(m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice(),
                                                   &props2);
    m_RTProperties = rtProperties;
    m_ASProperties = asProperties;

    m_Jobs.resize(m_NumJobs);

    RC rc;

    // Setup command buffer pool
    m_CmdPool = VulkanCmdPool(m_pVulkanDev);
    MASSERT(m_Queues.size() == 1);
    CHECK_VK_TO_RC(m_CmdPool.InitCmdPool(m_Queues[0].family, m_Queues[0].idx));

    // Create auxiliary/temporary command buffer for setup functions
    VulkanCmdBuffer auxCmdBuf(m_pVulkanDev);
    CHECK_VK_TO_RC(auxCmdBuf.AllocateCmdBuffer(&m_CmdPool));

    for (UINT32 jobId = 0; jobId < m_NumJobs; jobId++)
    {
        CHECK_RC(SetupJobResources(jobId, auxCmdBuf));
    }
    CHECK_RC(SetupGeometry(auxCmdBuf));
    CHECK_RC(SetupPipeline(auxCmdBuf));

    // Record all command buffers (must be last!)
    for (UINT32 jobId = 0; jobId < m_NumJobs; jobId++)
    {
        UpdateDescriptorSets(jobId);
        CHECK_RC(BuildCmdBuffer(jobId, Render));
        CHECK_RC(BuildCmdBuffer(jobId, Check));
    }

    return RC::OK;
}

RC VkFusion::Raytracing::Cleanup()
{
    StickyRC rc;

    m_Builder.Cleanup();
    m_GeometryData.Cleanup();
    m_ShaderBindingTable.Cleanup();

    m_RayGenRenderShader.Cleanup();
    m_RayGenCheckShader.Cleanup();
    m_IntersectAABBShader.Cleanup();
    m_HitShader.Cleanup();
    m_MissShader.Cleanup();

    m_RTPipeline.Cleanup();
    rc = m_DescriptorInfo.Cleanup();

    m_Jobs.clear();
    m_CmdPool.DestroyCmdPool();

    return rc;
}

RC VkFusion::Raytracing::SetupJobResources(UINT32 jobId, VulkanCmdBuffer& auxCmdBuf)
{
    Job& job = m_Jobs[jobId];

    job.rayPerturbSeed = m_Random.GetRandom();

    RC rc;

    // Allocate target image for this job
    CHECK_VK_TO_RC(AllocateImage(&job.target,
                                 VK_FORMAT_R32_SFLOAT,
                                 m_SurfaceWidth,
                                 m_SurfaceHeight,
                                 1, // mipmapLevels
                                 VK_IMAGE_USAGE_STORAGE_BIT |
                                     (m_DumpPng ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0),
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    // Switch to image layout expected on command buffer entry
    CHECK_VK_TO_RC(job.target.SetImageLayout(&auxCmdBuf,
                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_ACCESS_SHADER_READ_BIT |
                                                 VK_ACCESS_SHADER_WRITE_BIT,
                                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR));

    // Allocate error surface for this job
    CHECK_VK_TO_RC(AllocateImage(&job.errors,
                                 VK_FORMAT_R32_UINT,
                                 m_SurfaceWidth,
                                 m_SurfaceHeight,
                                 1, // mipmapLevels
                                 VK_IMAGE_USAGE_STORAGE_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    // Switch to image layout expected on command buffer entry
    CHECK_VK_TO_RC(job.errors.SetImageLayout(&auxCmdBuf,
                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_ACCESS_SHADER_READ_BIT |
                                                 VK_ACCESS_SHADER_WRITE_BIT,
                                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR));

    // Allocate host image for dumping
    if (m_DumpPng)
    {
        CHECK_VK_TO_RC(AllocateImage(&job.dumpImage,
                                     VK_FORMAT_R32_SFLOAT,
                                     m_SurfaceWidth,
                                     m_SurfaceHeight,
                                     1, // mipmapLevels
                                     VK_IMAGE_USAGE_SAMPLED_BIT | // not used, but required by validation layers
                                         VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                     VK_IMAGE_TILING_LINEAR,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT));

        // Switch image layout to the state expected on command buffer entry,
        // which is the state in which it will be left after each frame.
        CHECK_VK_TO_RC(job.dumpImage.SetImageLayout(&auxCmdBuf,
                                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL,
                                                    VK_ACCESS_HOST_READ_BIT,
                                                    VK_PIPELINE_STAGE_HOST_BIT));
    }

    return RC::OK;
}

RC VkFusion::Raytracing::SetupGeometry(VulkanCmdBuffer& auxCmdBuf)
{
    // Our geometry is a bounch of boxes, made of either AABBs (axis-aligned bounding boxes) or triangles.
    // The process of setting up the geometry consists of:
    // 1. Create a host buffer
    // 2. Fill the host buffer with the boxes
    // 3. Create a device buffer
    // 4. Transfer the boxes to the device buffer
    // 5. Build bottom-level acceleration structure
    // 6. Build top-level acceleration structure

    PROFILE_ZONE(VULKAN)

    RC rc;

    // Determine how many boxes to generate in BLAS
    UINT32 blasSizeXY;
    UINT32 blasSizeZ;
    if ((m_GeometryLevel == LEVEL_INSTANCES) || (m_BLASSize <= 1))
    {
        blasSizeXY = 1;
        blasSizeZ  = 1;
    }
    else
    {
        blasSizeXY = min(m_NumLwbesXY, m_BLASSize);
        blasSizeZ  = min(m_NumLwbesZ,  m_BLASSize);
    }
    MASSERT(blasSizeXY > 0U);
    MASSERT(blasSizeZ  > 0U);
    const bool   oneLwbeInBlas = (blasSizeXY == 1U) && (blasSizeZ == 1U);
    const UINT32 tlasSizeXY = max(ALIGN_UP(m_NumLwbesXY, blasSizeXY) / blasSizeXY, 1U);
    const UINT32 tlasSizeZ  = max(ALIGN_UP(m_NumLwbesZ,  blasSizeZ)  / blasSizeZ,  1U);
    VerbosePrintf("Raytracing: BLAS size [%ux%ux%u], TLAS size [%ux%ux%u]\n",
                  blasSizeXY, blasSizeXY, blasSizeZ,
                  tlasSizeXY, tlasSizeXY, tlasSizeZ);
    const UINT64 numPrimitives = static_cast<UINT64>(blasSizeXY) *
                                 static_cast<UINT64>(blasSizeXY) *
                                 static_cast<UINT64>(blasSizeZ);
    if (numPrimitives > m_ASProperties.maxGeometryCount)
    {
        Printf(Tee::PriError,
               "Number of lwbes in BLAS %llu exceeds maximum geometry count %llu for this device\n",
               numPrimitives, static_cast<UINT64>(m_ASProperties.maxGeometryCount));
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    const UINT64 totalPrimitives = numPrimitives * (m_GeometryType == GEOM_TRIANGLES ? 12U : 1U) *
                                   tlasSizeXY * tlasSizeXY * tlasSizeZ;
    if (totalPrimitives > m_ASProperties.maxPrimitiveCount)
    {
        Printf(Tee::PriError,
               "Number of generated primitives %llu exceeds maximum primitive count %llu for this device\n",
               totalPrimitives, static_cast<UINT64>(m_ASProperties.maxPrimitiveCount));
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    const UINT64 totalInstances = static_cast<UINT64>(tlasSizeXY) *
                                  static_cast<UINT64>(tlasSizeXY) *
                                  static_cast<UINT64>(tlasSizeZ);
    if (totalInstances > m_ASProperties.maxInstanceCount)
    {
        Printf(Tee::PriError,
               "Number of generated instances %llu exceeds maximum instance count %llu for this device\n",
               totalInstances, static_cast<UINT64>(m_ASProperties.maxInstanceCount));
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    const auto GenerateScene = [&](auto generate, UINT32 xSize, UINT32 ySize, UINT32 zSize)
    {
        for (UINT32 z = 0; z < zSize; z++)
        {
            for (UINT32 y = 0; y < ySize; y++)
            {
                for (UINT32 x = 0; x < xSize; x++)
                {
                    const lwmath::vec3f vSize(m_Random.GetRandomFloat(m_MinLwbeSize, m_MaxLwbeSize));

                    const float xOffs = m_Random.GetRandomFloat(m_MinLwbeOffset, m_MaxLwbeOffset);
                    const float yOffs = m_Random.GetRandomFloat(m_MinLwbeOffset, m_MaxLwbeOffset);
                    const float zOffs = m_Random.GetRandomFloat(m_MinLwbeOffset, m_MaxLwbeOffset);

                    const lwmath::vec3f minCorner(x + xOffs, y + yOffs, z + zOffs);
                    const lwmath::vec3f maxCorner = minCorner + vSize;

                    generate(minCorner, maxCorner);
                }
            }
        }
    };

    CHECK_VK_TO_RC(auxCmdBuf.ResetCmdBuffer());

    // Create host buffer
    VulkanBuffer hostGeomData(m_pVulkanDev);
    VulkanBuffer hostIndexData(m_pVulkanDev);
    static constexpr UINT32 numVtxPerLwbe = 8;
    static constexpr UINT32 numTriPerLwbe = 6 * 2;
    static constexpr UINT32 numIdxPerLwbe = numTriPerLwbe * 3;

    // Fill the host buffer with geometry
    if (m_GeometryType == GEOM_TRIANGLES)
    {
        if (numVtxPerLwbe * numPrimitives > 0xFFFF'FFFFU)
        {
            Printf(Tee::PriError,
                   "Number of generated vertices %llu exceeds UINT32\n",
                   numVtxPerLwbe * numPrimitives);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        CHECK_VK_TO_RC(hostGeomData.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 numPrimitives * numVtxPerLwbe * sizeof(Vertex),
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

        CHECK_VK_TO_RC(hostIndexData.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  numPrimitives * numIdxPerLwbe * sizeof(UINT32),
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

        VulkanBufferView<Vertex> vtxView(hostGeomData);
        CHECK_VK_TO_RC(vtxView.Map());
        auto   vtxIter = vtxView.begin();
        UINT32 vtxIdx  = 0;

        VulkanBufferView<UINT32> idxView(hostIndexData);
        CHECK_VK_TO_RC(idxView.Map());
        auto idxIter = idxView.begin();

        const auto GenerateTriangles = [&](lwmath::vec3f minCorner, lwmath::vec3f maxCorner)
        {
            // Outer array: vertices of a lwbe (axis-aligned)
            // Inner array: 3 coordinates (x, y, z), for each coord choose near (min) or far (max) value
            static const int selectCoord[numVtxPerLwbe][3] =
            {
                { 0, 0, 0 },
                { 1, 0, 0 },
                { 1, 1, 0 },
                { 0, 1, 0 },
                { 0, 0, 1 },
                { 1, 0, 1 },
                { 1, 1, 1 },
                { 0, 1, 1 }
            };

            // Near (min) and far (max) coordinates of the AABB (lwbe)
            const lwmath::vec3f coord[2] = { minCorner, maxCorner };

            for (const auto& select : selectCoord)
            {
                *vtxIter = Vertex(coord[select[0]].x,
                                  coord[select[1]].y,
                                  coord[select[2]].z);
                ++vtxIter;
            }

            // Select vertices for each triangle in each face of the lwbe
            static const UINT32 faceIndicesCCW[numIdxPerLwbe] =
            {
                0, 1, 3,
                3, 1, 2,
                2, 1, 6,
                6, 1, 5,
                5, 1, 4,
                4, 1, 0,
                4, 0, 3,
                3, 7, 4,
                4, 7, 5,
                5, 7, 6,
                6, 7, 3,
                3, 2, 6
            };

            for (UINT32 idx : faceIndicesCCW)
            {
                *idxIter = vtxIdx + idx;
                ++idxIter;
            }

            vtxIdx += numVtxPerLwbe;
        };

        if (oneLwbeInBlas)
        {
            GenerateTriangles(lwmath::vec3f(0, 0, 0), lwmath::vec3f(1, 1, 1));
        }
        else
        {
            GenerateScene(GenerateTriangles, blasSizeXY, blasSizeXY, blasSizeZ);
        }

        MASSERT(vtxIter == vtxView.end());
        MASSERT(idxIter == idxView.end());
    }
    else
    {
        CHECK_VK_TO_RC(hostGeomData.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 numPrimitives * sizeof(VkAabbPositionsKHR),
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

        VulkanBufferView<VkAabbPositionsKHR> view(hostGeomData);
        CHECK_VK_TO_RC(view.Map());
        auto aabbIter = view.begin();

        const auto GenerateAABB = [&](lwmath::vec3f minCorner, lwmath::vec3f maxCorner)
        {
            aabbIter->minX = minCorner.x;
            aabbIter->minY = minCorner.y;
            aabbIter->minZ = minCorner.z;
            aabbIter->maxX = maxCorner.x;
            aabbIter->maxY = maxCorner.y;
            aabbIter->maxZ = maxCorner.z;
            ++aabbIter;
        };

        if (oneLwbeInBlas)
        {
            GenerateAABB(lwmath::vec3f(0, 0, 0), lwmath::vec3f(1, 1, 1));
        }
        else
        {
            GenerateScene(GenerateAABB, blasSizeXY, blasSizeXY, blasSizeZ);
        }

        MASSERT(aabbIter == view.end());
    }

    // Transfer geometry data to device
    m_GeometryData = VulkanBuffer(m_pVulkanDev);
    CHECK_VK_TO_RC(m_GeometryData.CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               hostGeomData.GetSize(),
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(&auxCmdBuf,
                                      &hostGeomData,
                                      &m_GeometryData,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR));
    VulkanBuffer indexData;
    if (m_GeometryType == GEOM_TRIANGLES)
    {
        indexData = VulkanBuffer(m_pVulkanDev);
        CHECK_VK_TO_RC(indexData.CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              hostIndexData.GetSize(),
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        CHECK_VK_TO_RC(VkUtil::CopyBuffer(&auxCmdBuf,
                                          &hostIndexData,
                                          &indexData,
                                          VK_ACCESS_TRANSFER_WRITE_BIT,
                                          VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR));
    }

    // Build bottom-level acceleration structures
    MASSERT(m_Queues.size() == 1);
    MASSERT(m_Queues[0].caps & VK_QUEUE_COMPUTE_BIT);
    m_Builder.Setup(m_pVulkanDev, m_Queues[0].idx, m_Queues[0].family);

    vector<VulkanRaytracingBuilder::BlasInput> blas(1);
    VulkanRaytracingBuilder::BlasInput& geoms = blas.back();

    const auto GenerateGeometry = [&](UINT64 numPrimitives)
    {
        const size_t idx = geoms.geometry.size();
        MASSERT(idx == geoms.buildOffsetInfo.size());

        geoms.geometry.emplace_back();
        VkAccelerationStructureGeometryKHR& geom = geoms.geometry.back();

        geoms.buildOffsetInfo.emplace_back();
        VkAccelerationStructureBuildRangeInfoKHR& buildOffsetInfo = geoms.buildOffsetInfo.back();

        geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geom.pNext = nullptr;
        geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        if (m_GeometryType == GEOM_TRIANGLES)
        {
            geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

            VkAccelerationStructureGeometryTrianglesDataKHR& triangles = geom.geometry.triangles;

            const UINT32 lowestIdx = static_cast<UINT32>(idx * numVtxPerLwbe);

            triangles.sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.pNext                     = nullptr;
            triangles.vertexFormat              = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.vertexData.deviceAddress  = m_GeometryData.GetBufferDeviceAddress();
            triangles.vertexStride              = sizeof(Vertex);
            triangles.maxVertex                 = lowestIdx - 1 + static_cast<UINT32>(numVtxPerLwbe * numPrimitives);
            triangles.indexType                 = VK_INDEX_TYPE_UINT32;
            triangles.indexData.deviceAddress   = indexData.GetBufferDeviceAddress();
            triangles.transformData.hostAddress = nullptr;

            buildOffsetInfo.primitiveCount  = static_cast<UINT32>(numTriPerLwbe * numPrimitives);
            buildOffsetInfo.primitiveOffset = static_cast<UINT32>(idx * numIdxPerLwbe * sizeof(UINT32));
            buildOffsetInfo.firstVertex     = 0;
            buildOffsetInfo.transformOffset = 0;
        }
        else
        {
            geom.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;

            VkAccelerationStructureGeometryAabbsDataKHR& aabbs = geom.geometry.aabbs;

            aabbs.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
            aabbs.pNext              = nullptr;
            aabbs.data.deviceAddress = m_GeometryData.GetBufferDeviceAddress();
            aabbs.stride             = sizeof(VkAabbPositionsKHR);

            buildOffsetInfo.primitiveCount  = static_cast<UINT32>(numPrimitives);
            buildOffsetInfo.primitiveOffset = static_cast<UINT32>(idx * sizeof(VkAabbPositionsKHR));
            buildOffsetInfo.firstVertex     = 0;
            buildOffsetInfo.transformOffset = 0;
        }
    };

    if (m_GeometryLevel == LEVEL_GEOMETRIES)
    {
        geoms.geometry.reserve(numPrimitives);
        geoms.buildOffsetInfo.reserve(numPrimitives);

        for (UINT64 i = 0; i < numPrimitives; i++)
        {
            GenerateGeometry(1);
        }
    }
    else
    {
        GenerateGeometry(numPrimitives);
    }

    CHECK_VK_TO_RC(m_Builder.BuildBlas(blas,
                                       VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                       VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR));

    // Build top-level acceleration structure
    vector<VulkanRaytracingBuilder::Instance> tlas;
    tlas.reserve(tlasSizeXY * tlasSizeXY * tlasSizeZ);

    const UINT32 geomFlags = (m_GeometryType == GEOM_TRIANGLES)
                           ? VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR
                           : VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_LWLL_DISABLE_BIT_KHR;

    const INT32 halfXYWidth = static_cast<INT32>(m_NumLwbesXY) / 2;

    const auto CreateInstance = [&](lwmath::mat4f transform)
    {
        tlas.emplace_back();
        VulkanRaytracingBuilder::Instance& inst = tlas.back();

        inst.transform = transform;
        inst.flags     = geomFlags;
    };

    const auto GenerateInstance = [&](lwmath::vec3f minCorner, lwmath::vec3f maxCorner)
    {
        const lwmath::vec3f fullBlasShift(-halfXYWidth, -halfXYWidth, 0);
        CreateInstance(lwmath::mult(lwmath::translation_mat4(minCorner + fullBlasShift),
                                    lwmath::scale_mat4(maxCorner - minCorner)));
    };

    if (oneLwbeInBlas)
    {
        GenerateScene(GenerateInstance, tlasSizeXY, tlasSizeXY, tlasSizeZ);
    }
    else
    {
        // Build TLAS without rescaling BLASes, only move them around
        for (UINT32 z = 0; z < tlasSizeZ; z++)
        {
            const UINT32 zShift = z * blasSizeZ;
            for (UINT32 y = 0; y < tlasSizeXY; y++)
            {
                const INT32 yShift = -halfXYWidth + y * blasSizeXY;
                for (UINT32 x = 0; x < tlasSizeXY; x++)
                {
                    const INT32 xShift = -halfXYWidth + x * blasSizeXY;

                    CreateInstance(lwmath::translation_mat4(lwmath::vec3f(xShift, yShift, zShift)));
                }
            }
        }
    }

    CHECK_VK_TO_RC(m_Builder.BuildTlas(tlas));

    if (m_GeometryType != GEOM_AABBS)
    {
        m_GeometryData.Cleanup();
    }

    return RC::OK;
}

RC VkFusion::Raytracing::SetupPipeline(VulkanCmdBuffer& auxCmdBuf)
{
    PROFILE_ZONE(VULKAN)

    RC rc;

    m_DescriptorInfo = DescriptorInfo(m_pVulkanDev);

    vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    vector<VkPipelineShaderStageCreateInfo>      stages;
    vector<VkPushConstantRange>                  pushConstantRanges;
    const bool                                   bindGeometry = m_GeometryType == GEOM_AABBS;
    const bool                                   triangles    = m_GeometryType == GEOM_TRIANGLES;
    UINT32                                       numDescSetLayouts = 0;

    // Specialization constants for shaders
    ShaderSpecialization shaderConstants = { };
    shaderConstants.cameraDirX   = 0;
    shaderConstants.cameraDirY   = 0;
    shaderConstants.cameraDirZ   = 1;
    shaderConstants.tMax         = static_cast<float>(m_NumLwbesZ * 3);
    shaderConstants.nearT        = 5;
    shaderConstants.farT         = shaderConstants.nearT + m_NumLwbesZ;
    shaderConstants.check        = 0;

    const VkSpecializationMapEntry constantMapEntries[] =
    {
        { 0, offsetof(ShaderSpecialization, cameraDirX),   sizeof(shaderConstants.cameraDirX)   },
        { 1, offsetof(ShaderSpecialization, cameraDirY),   sizeof(shaderConstants.cameraDirY)   },
        { 2, offsetof(ShaderSpecialization, cameraDirZ),   sizeof(shaderConstants.cameraDirZ)   },
        { 3, offsetof(ShaderSpecialization, tMax),         sizeof(shaderConstants.tMax)         },
        { 4, offsetof(ShaderSpecialization, nearT),        sizeof(shaderConstants.nearT)        },
        { 5, offsetof(ShaderSpecialization, farT),         sizeof(shaderConstants.farT)         },
        { 6, offsetof(ShaderSpecialization, check),        sizeof(shaderConstants.check)        },
    };

    VkSpecializationInfo specInfo = { };
    specInfo.pMapEntries   = &constantMapEntries[0];
    specInfo.mapEntryCount = static_cast<UINT32>(NUMELEMS(constantMapEntries));
    specInfo.pData         = &shaderConstants;
    specInfo.dataSize      = sizeof(shaderConstants);

    ////////////////////////////////////////////////////////////////////////////
    // Raygen shader (render)
    m_RayGenRenderShader = VulkanShader(m_pVulkanDev);
    CHECK_VK_TO_RC(m_RayGenRenderShader.CreateShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                                     s_RayGenShader,
                                                     "main",
                                                     m_ShaderReplacement,
                                                     &specInfo));

    stages.push_back(m_RayGenRenderShader.GetShaderStageInfo());

    {
        VkRayTracingShaderGroupCreateInfoKHR group = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
        group.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader      = static_cast<UINT32>(stages.size() - 1);
        group.closestHitShader   = VK_SHADER_UNUSED_KHR;
        group.anyHitShader       = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(group);
    }

    {
        VkPushConstantRange range = { };
        range.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        range.size       = sizeof(RayParams);
        pushConstantRanges.push_back(range);
    }

    ////////////////////////////////////////////////////////////////////////////
    // Raygen shader (check)
    ShaderSpecialization checkShaderConstants = shaderConstants;
    checkShaderConstants.check = 1;
    VkSpecializationInfo checkSpecInfo = specInfo;
    checkSpecInfo.pData = &checkShaderConstants;

    m_RayGenCheckShader = VulkanShader(m_pVulkanDev);
    CHECK_VK_TO_RC(m_RayGenCheckShader.CreateShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                                    s_RayGenShader,
                                                    "main",
                                                    m_ShaderReplacement,
                                                    &checkSpecInfo));

    stages.push_back(m_RayGenCheckShader.GetShaderStageInfo());

    {
        VkRayTracingShaderGroupCreateInfoKHR group = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
        group.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader      = static_cast<UINT32>(stages.size() - 1);
        group.closestHitShader   = VK_SHADER_UNUSED_KHR;
        group.anyHitShader       = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(group);
    }

    ////////////////////////////////////////////////////////////////////////////
    // Intersection shader
    const UINT32 intersectShaderId = triangles
                                   ? VK_SHADER_UNUSED_KHR
                                   : static_cast<UINT32>(stages.size());
    if (!triangles)
    {
        m_IntersectAABBShader = VulkanShader(m_pVulkanDev);
        const char* const intersectShader = (m_GeometryType == GEOM_AABBS_TRIVIAL)
                                          ? s_SimpleIntersectShader
                                          : s_IntersectAABBShader;
        CHECK_VK_TO_RC(m_IntersectAABBShader.CreateShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
                                                          intersectShader,
                                                          "main",
                                                          m_ShaderReplacement));

        stages.push_back(m_IntersectAABBShader.GetShaderStageInfo());
    }

    // Note on groups: intersection shader is in the same shader group as closest hit shader

    ////////////////////////////////////////////////////////////////////////////
    // Closest hit shader
    m_HitShader = VulkanShader(m_pVulkanDev);
    CHECK_VK_TO_RC(m_HitShader.CreateShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                            s_HitShader,
                                            "main",
                                            m_ShaderReplacement));

    stages.push_back(m_HitShader.GetShaderStageInfo());

    {
        VkRayTracingShaderGroupCreateInfoKHR group = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
        group.type               = triangles
                                    ? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR
                                    : VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
        group.generalShader      = VK_SHADER_UNUSED_KHR;
        group.closestHitShader   = static_cast<UINT32>(stages.size() - 1);
        group.anyHitShader       = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = intersectShaderId;
        groups.push_back(group);
    }

    ////////////////////////////////////////////////////////////////////////////
    // Miss shader
    m_MissShader = VulkanShader(m_pVulkanDev);
    CHECK_VK_TO_RC(m_MissShader.CreateShader(VK_SHADER_STAGE_MISS_BIT_KHR,
        s_MissShader, "main", m_ShaderReplacement));

    stages.push_back(m_MissShader.GetShaderStageInfo());

    {
        VkRayTracingShaderGroupCreateInfoKHR group = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
        group.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader      = static_cast<UINT32>(stages.size() - 1);
        group.closestHitShader   = VK_SHADER_UNUSED_KHR;
        group.anyHitShader       = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(group);
    }

    ////////////////////////////////////////////////////////////////////////////
    // Create descriptor set layouts and descriptor sets
    {
        const VkDescriptorSetLayoutBinding bindings[] =
        {
            {
                0,                                             // binding = 0
                VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, // descriptorType
                1,                                             // descriptorCount
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,                // stageFlags
                nullptr                                        // pImmutableSamplers
            },
            {
                1,                                             // binding = 1
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              // descriptorType
                1,                                             // descriptorCount
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,                // stageFlags
                nullptr                                        // pImmutableSamplers
            },
            {
                2,                                             // binding = 2
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              // descriptorType
                1,                                             // descriptorCount
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,                // stageFlags
                nullptr                                        // pImmutableSamplers
            },
            {
                3,                                             // binding = 3
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             // descriptorType
                1,                                             // descriptorCount
                VK_SHADER_STAGE_INTERSECTION_BIT_KHR,          // stageFlags
                nullptr                                        // pImmutableSamplers
            }
        };
        const UINT32 numBindings = static_cast<UINT32>(NUMELEMS(bindings))
                                 - (bindGeometry ? 0 : 1);
        CHECK_VK_TO_RC(m_DescriptorInfo.CreateDescriptorSetLayout(0, numBindings, bindings));
        ++numDescSetLayouts;
    }

    vector<VkDescriptorPoolSize> descPoolSizes =
    {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              2 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1 }
    };
    if (!bindGeometry)
    {
        descPoolSizes.resize(descPoolSizes.size() - 1);
    }
    CHECK_VK_TO_RC(m_DescriptorInfo.CreateDescriptorPool(static_cast<UINT32>(numDescSetLayouts * m_Jobs.size()),
                                                         descPoolSizes));

    for (UINT32 i = 0; i < m_Jobs.size(); i++)
    {
        CHECK_VK_TO_RC(m_DescriptorInfo.AllocateDescriptorSets(i * numDescSetLayouts,
                                                               numDescSetLayouts,
                                                               0));
    }

    ////////////////////////////////////////////////////////////////////////////
    // Setup pipeline
    m_RTPipeline = VulkanPipeline(m_pVulkanDev);

    const UINT32 numGroups = static_cast<UINT32>(groups.size());

    CHECK_VK_TO_RC(m_RTPipeline.SetupRaytracingPipeline(m_DescriptorInfo.GetAllDescriptorSetLayouts(),
                                                        move(stages),
                                                        move(groups),
                                                        move(pushConstantRanges),
                                                        VulkanPipeline::CREATE_PIPELINE_LAYOUT |
                                                        VulkanPipeline::CREATE_PIPELINE,
                                                        0));

    ////////////////////////////////////////////////////////////////////////////
    // Setup shader binding table

    // Create host buffer to fill out
    m_ShaderGroupHandleSize = ALIGN_UP(m_RTProperties.shaderGroupHandleSize,
                                       m_RTProperties.shaderGroupBaseAlignment);
    const UINT32 sbtSize = numGroups * m_ShaderGroupHandleSize;
    VulkanBuffer hostSbt(m_pVulkanDev);
    CHECK_VK_TO_RC(hostSbt.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        sbtSize,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

    // Get the shader handles into the SBT
    {
        VulkanBufferView<UINT08> view(hostSbt);
        CHECK_VK_TO_RC(view.Map());

        for (UINT32 i = 0; i < numGroups; i++)
        {
            CHECK_VK_TO_RC(m_pVulkanDev->GetRayTracingShaderGroupHandlesKHR(m_RTPipeline.GetPipeline(),
                                                                            i,
                                                                            1,
                                                                            m_ShaderGroupHandleSize,
                                                                            &view.data()[i * m_ShaderGroupHandleSize]));
        }
    }

    // Create final SBT buffer in device memory
    m_ShaderBindingTable = VulkanBuffer(m_pVulkanDev);
    CHECK_VK_TO_RC(m_ShaderBindingTable.CreateBuffer(VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                     sbtSize,
                                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    // Transfer the SBT from host to device
    CHECK_VK_TO_RC(auxCmdBuf.ResetCmdBuffer());
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(&auxCmdBuf,
                                      &hostSbt,
                                      &m_ShaderBindingTable,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_ACCESS_SHADER_READ_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR));

    return RC::OK;
}

void VkFusion::Raytracing::UpdateDescriptorSets(UINT32 jobId)
{
    const auto& allDescSets = m_DescriptorInfo.GetAllDescriptorSets();

    VkWriteDescriptorSetAccelerationStructureKHR asInfo = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures    = &m_Builder.GetAccelerationStructure();

    VkDescriptorImageInfo imageInfo = { };
    imageInfo.sampler     = VK_NULL_HANDLE;
    imageInfo.imageView   = m_Jobs[jobId].target.GetImageView();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo errorsInfo = { };
    errorsInfo.sampler     = VK_NULL_HANDLE;
    errorsInfo.imageView   = m_Jobs[jobId].errors.GetImageView();
    errorsInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorBufferInfo geomInfo = { };
    geomInfo.buffer = m_GeometryData.GetBuffer();
    geomInfo.offset = 0;
    geomInfo.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet updateDescSets[] =
    {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            &asInfo,
            allDescSets[jobId],
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            allDescSets[jobId],
            1,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            &imageInfo
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            allDescSets[jobId],
            2,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            &errorsInfo
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            allDescSets[jobId],
            3,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr,
            &geomInfo
        }
    };

    const UINT32 numDesc = static_cast<UINT32>(NUMELEMS(updateDescSets))
                         - ((m_GeometryType == GEOM_AABBS) ? 0 : 1);

    m_DescriptorInfo.UpdateDescriptorSets(&updateDescSets[0], numDesc);
}

RC VkFusion::Raytracing::BuildCmdBuffer(UINT32 jobId, Behavior behavior)
{
    Job&             job    = m_Jobs[jobId];
    VulkanCmdBuffer& cmdBuf = job.cmdBuf[behavior];

    RC rc;

    cmdBuf = VulkanCmdBuffer(m_pVulkanDev);
    CHECK_VK_TO_RC(cmdBuf.AllocateCmdBuffer(&m_CmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));

    CHECK_VK_TO_RC(cmdBuf.BeginCmdBuffer());

    CHECK_RC(FillCmdBuffer(cmdBuf, jobId, behavior));

    CHECK_VK_TO_RC(cmdBuf.EndCmdBuffer());

    return RC::OK;
}

RC VkFusion::Raytracing::FillCmdBuffer(VulkanCmdBuffer& cmdBuf, UINT32 jobId, Behavior behavior)
{
    Job& job = m_Jobs[jobId];

    m_pVulkanDev->CmdBindPipeline(cmdBuf.GetCmdBuffer(),
                                  VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                  m_RTPipeline.GetPipeline());

    const size_t numSets = m_DescriptorInfo.GetAllDescriptorSetLayouts().size();
    m_pVulkanDev->CmdBindDescriptorSets(cmdBuf.GetCmdBuffer(),
                                        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                        m_RTPipeline.GetPipelineLayout(),
                                        0,
                                        static_cast<UINT32>(numSets),
                                        &m_DescriptorInfo.GetAllDescriptorSets()[jobId * numSets],
                                        0,
                                        nullptr);

    const double angle = jobId;
    const double x0    = 1;
    const double camX  = (jobId == 0) ? 0.0 : (cos(angle) * x0);
    const double camY  = (jobId == 0) ? 0.0 : (sin(angle) * x0);

    RayParams rayParams = { };
    rayParams.cameraOrigin   = lwmath::vec3f(camX, camY, -10);
    rayParams.rayPerturbSeed = job.rayPerturbSeed;

    m_pVulkanDev->CmdPushConstants(cmdBuf.GetCmdBuffer(),
                                   m_RTPipeline.GetPipelineLayout(),
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                   0,
                                   sizeof(rayParams),
                                   &rayParams);

    const VkDeviceAddress sbtAddr = m_ShaderBindingTable.GetBufferDeviceAddress();

    VkStridedDeviceAddressRegionKHR raygenRegion =
    {
        sbtAddr + (behavior * m_ShaderGroupHandleSize),
        m_ShaderGroupHandleSize,
        m_ShaderGroupHandleSize
    };

    VkStridedDeviceAddressRegionKHR hitRegion =
    {
        sbtAddr + 2 * m_ShaderGroupHandleSize,
        m_ShaderGroupHandleSize,
        m_ShaderGroupHandleSize
    };

    VkStridedDeviceAddressRegionKHR missRegion =
    {
        sbtAddr + 3 * m_ShaderGroupHandleSize,
        m_ShaderGroupHandleSize,
        m_ShaderGroupHandleSize
    };

    VkStridedDeviceAddressRegionKHR callableRegion = { };

    m_pVulkanDev->CmdTraceRaysKHR(cmdBuf.GetCmdBuffer(), // command buffer
                                  &raygenRegion,         // pRaygenShaderBindingTable
                                  &missRegion,           // pMissShaderBindingTable
                                  &hitRegion,            // pHitShaderBindingTable
                                  &callableRegion,       // pCallableShaderBindingTable
                                  m_SurfaceWidth,        // width
                                  m_SurfaceHeight,       // height
                                  1);                    // depth

    RC rc;

    if (m_DumpPng)
    {
        CHECK_VK_TO_RC(job.target.SetImageLayout(&cmdBuf,
                                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                 VK_ACCESS_TRANSFER_READ_BIT,
                                                 VK_PIPELINE_STAGE_TRANSFER_BIT));

        CHECK_VK_TO_RC(job.dumpImage.SetImageLayout(&cmdBuf,
                                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                                    VK_PIPELINE_STAGE_TRANSFER_BIT));

        VkImageCopy copyRegion = { };
        copyRegion.srcSubresource.aspectMask = job.target.GetImageAspectFlags();
        copyRegion.dstSubresource.aspectMask = job.dumpImage.GetImageAspectFlags();
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.extent.width              = job.target.GetWidth();
        copyRegion.extent.height             = job.target.GetHeight();
        copyRegion.extent.depth              = 1;

        m_pVulkanDev->CmdCopyImage(cmdBuf.GetCmdBuffer(),
                                   job.target.GetImage(),
                                   job.target.GetImageLayout(),
                                   job.dumpImage.GetImage(),
                                   job.dumpImage.GetImageLayout(),
                                   1,
                                   &copyRegion);

        CHECK_VK_TO_RC(job.dumpImage.SetImageLayout(&cmdBuf,
                                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL,
                                                    VK_ACCESS_HOST_READ_BIT,
                                                    VK_PIPELINE_STAGE_HOST_BIT));
    }

    CHECK_VK_TO_RC(job.target.SetImageLayout(&cmdBuf,
                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_ACCESS_SHADER_READ_BIT |
                                                 VK_ACCESS_SHADER_WRITE_BIT,
                                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR));

    CHECK_VK_TO_RC(job.errors.SetImageLayout(&cmdBuf,
                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_ACCESS_SHADER_READ_BIT |
                                                 VK_ACCESS_SHADER_WRITE_BIT,
                                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR));

    return RC::OK;
}

void VkFusion::Raytracing::PreRun()
{
    m_TotalRays     = 0;
    m_RaysPerSecond = 0;

    for (auto& job : m_Jobs)
    {
        job.numRenders = 0;
    }
}

RC VkFusion::Raytracing::Execute
(
    UINT32            jobId,
    VulkanCmdBuffer** pCmdBufs,
    UINT32            numCmdBufs,
    VkSemaphore*      pJobWaitSema,
    VkSemaphore*      pPresentSema
)
{
    MASSERT(numCmdBufs == 1);

    Job& job = m_Jobs[jobId];

    const Behavior behavior = (job.numRenders == 0 || !m_Check) ? Render : Check;

    const VkCommandBuffer secondaryCmdBuf = job.cmdBuf[behavior].GetCmdBuffer();

    m_pVulkanDev->CmdExelwteCommands(pCmdBufs[0]->GetCmdBuffer(),
                                     1,
                                     &secondaryCmdBuf);

    ++job.numRenders;

    m_TotalRays += static_cast<UINT64>(m_SurfaceWidth) * static_cast<UINT64>(m_SurfaceHeight);

    return RC::OK;
}

RC VkFusion::Raytracing::CheckFrame(UINT32 jobId, UINT64 frameId, bool finalFrame)
{
    if (!finalFrame)
    {
        return RC::OK;
    }

    Job& job = m_Jobs[jobId];

    if (!job.numRenders)
    {
        return RC::OK;
    }

    RC rc;

    // Create auxiliary/temporary command buffer
    VulkanCmdBuffer auxCmdBuf(m_pVulkanDev);
    CHECK_VK_TO_RC(auxCmdBuf.AllocateCmdBuffer(&m_CmdPool));

    // Create host-readable buffer
    VulkanBuffer hostBuf(m_pVulkanDev);
    CHECK_VK_TO_RC(hostBuf.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        job.errors.GetWidth() * job.errors.GetHeight() *
                                            static_cast<UINT32>(sizeof(UINT32)),
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                            VK_MEMORY_PROPERTY_HOST_CACHED_BIT));

    CHECK_VK_TO_RC(auxCmdBuf.BeginCmdBuffer());

    CHECK_VK_TO_RC(job.errors.SetImageLayout(&auxCmdBuf,
                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                             VK_ACCESS_TRANSFER_READ_BIT,
                                             VK_PIPELINE_STAGE_TRANSFER_BIT));

    VkBufferImageCopy copyRegion = { };
    copyRegion.bufferOffset                = 0;
    copyRegion.bufferRowLength             = m_SurfaceWidth;
    copyRegion.bufferImageHeight           = m_SurfaceHeight;
    copyRegion.imageSubresource.aspectMask = job.errors.GetImageAspectFlags();
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent.width           = job.errors.GetWidth();
    copyRegion.imageExtent.height          = job.errors.GetHeight();
    copyRegion.imageExtent.depth           = 1;

    m_pVulkanDev->CmdCopyImageToBuffer(auxCmdBuf.GetCmdBuffer(),
                                       job.errors.GetImage(),
                                       job.errors.GetImageLayout(),
                                       hostBuf.GetBuffer(),
                                       1,
                                       &copyRegion);

    CHECK_VK_TO_RC(job.errors.SetImageLayout(&auxCmdBuf,
                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_ACCESS_SHADER_READ_BIT |
                                                 VK_ACCESS_SHADER_WRITE_BIT,
                                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR));

    CHECK_VK_TO_RC(auxCmdBuf.EndCmdBuffer());
    CHECK_VK_TO_RC(auxCmdBuf.ExelwteCmdBuffer(true, false));

    // Check error numbers
    UINT64 totalErrors = 0;
    VulkanBufferView<UINT32> view(hostBuf);
    CHECK_VK_TO_RC(view.Map());
    for (UINT32 pixelErrors : view)
    {
        totalErrors += pixelErrors;
    }

    if (totalErrors)
    {
        Printf(Tee::PriError, "VkFusion.Raytracing job %u found %llu pixel miscompares\n", jobId, totalErrors);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    return RC::OK;
}

RC VkFusion::Raytracing::Dump(UINT32 jobId, UINT64 frameId)
{
    RC rc;
    VulkanImage& dumpImage = m_Jobs[jobId].dumpImage;

    const string filename = Utility::StrPrintf("vkfusion_rt%llu.png", frameId);
    VerbosePrintf("Raytracing: Dumping %s\n", filename.c_str());

    const VkDeviceSize size = dumpImage.GetPitch() * dumpImage.GetHeight();
    void* ptr = nullptr;
    CHECK_VK_TO_RC(dumpImage.Map(size, 0, &ptr));
    DEFER { dumpImage.Unmap(); };

    CHECK_RC(ImageFile::WritePng(filename.c_str(),
                                 VkUtil::ColorUtilsFormat(dumpImage.GetFormat()),
                                 ptr,
                                 dumpImage.GetWidth(),
                                 dumpImage.GetHeight(),
                                 static_cast<UINT32>(dumpImage.GetPitch()),
                                 false,
                                 false));

    return RC::OK;
}

void VkFusion::Raytracing::UpdateAndPrintStats(UINT64 elapsedMs)
{
    m_RaysPerSecond = elapsedMs ? (m_TotalRays * 1000.0 / elapsedMs) : 0.0;

    VerbosePrintf("Raytracing: Traced %llu rays in %llu ms, %.1f Rays-Per-Second\n",
                  m_TotalRays, elapsedMs, m_RaysPerSecond);
}
