/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#if !defined(VULKAN_STANDALONE)
#include "core/include/script.h"
#endif

#include "core/include/utility.h"

#include "vkstressray.h"
#include "vulkan/vkutil.h"
#include "core/include/imagefil.h"

Raytracer::Raytracer(VulkanDevice* pVulkanDev)
    : m_pVulkanDev(pVulkanDev)
{
    if (pVulkanDev)
    {
        m_pVulkanInst = m_pVulkanDev->GetVulkanInstance();
    }
    m_FpCtx.LoopNum = 0;
    m_FpCtx.RestartNum = 0;
    m_FpCtx.pJSObj = 0;
}

//-------------------------------------------------------------------------------------------------
void Raytracer::SetVulkanDevice(VulkanDevice* pVulkanDev)
{
    m_pVulkanDev = pVulkanDev;
    if (pVulkanDev)
    {
        m_pVulkanInst = m_pVulkanDev->GetVulkanInstance();
    }
}

//-------------------------------------------------------------------------------------------------
Raytracer::~Raytracer()
{
    CleanupResources();
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::AddExtensions(vector<string>& extNames)
{
    // Add required extensions
    static const char* const extensions[] =
    {
        "VK_KHR_deferred_host_operations", // required by VK_KHR_acceleration_structure
        "VK_KHR_acceleration_structure",   // required by VK_KHR_ray_tracing_pipeline
        "VK_KHR_ray_tracing_pipeline",
        "VK_EXT_scalar_block_layout"
    };
    for (const auto ext : extensions)
    {
        if (!m_pVulkanDev->HasExt(ext))
        {
            Printf(Tee::PriNormal, "The required extension \"%s\" is not supported\n", ext);
            return RC::MODS_VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        extNames.emplace_back(ext);
    }
    return RC::OK;
}

//-------------------------------------------------------------------------------------------------
void Raytracer::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "Raytracing Js Properties:\n");

    const char * TF[] = { "false", "true" };
    Printf(pri, "\t%-32s %3.1f %3.1f %3.1f\n",   "CamPos:", m_CamPos[0], m_CamPos[1], m_CamPos[2]);
    Printf(pri, "\t%-32s 0x%x\n",       "ClearSceneColor:", m_ClearSceneColor);
    Printf(pri, "\t%-32s %s\n",         "DisableXORLogic:", TF[m_DisableXORLogic]);
    Printf(pri, "\t%-32s %u\n",         "FramesPerSubmit:", m_FramesPerSubmit);
    Printf(pri, "\t%-32s %u\n",         "NumSpheres:",      m_NumSpheres);
    Printf(pri, "\t%-32s %u %u %u\n",   "Rotate:",          m_Rotate[0], m_Rotate[1], m_Rotate[2]);
    Printf(pri, "\t%-32s %s\n",         "Scene:",           GetSceneStr());
    Printf(pri, "\t%-32s %s\n",         "UseComputeQueue:", TF[m_UseComputeQueue]);
}

//-------------------------------------------------------------------------------------------------
void Raytracer::AddDebugMessagesToIgnore()
{
    // There is a VK_PERF_WARNING message that is generated because we are setting the color
    // attachment initialLayout & finalLayout = VK_IMAGE_LAYOUT_GENERAL.  We have to do that
    // because the raytraying descriptor layout binding 0 which is a STORAGE_IMAGE requires
    // that it's format be VK_IMAGE_LAYOUT_GENERAL. Validation Layers 1.2.137
    m_pVulkanInst->AddDebugMessageToIgnore(
    {
        VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT
        ,0 // RESERVED_PARAMETER
        ,"Validation"
        ,"Layout for color attachment is GENERAL but should be COLOR_ATTACHMENT_OPTIMAL%c"
        ,'.'
    });
}

//-------------------------------------------------------------------------------------------------
// Allocate all of the required Vulkan resources to run a raytracing shaders in parallel with the
// graphics & compute pipelines.
void Raytracer::AllocateResources()
{
    VkPhysicalDevice vkDev = m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice();

    m_Properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2KHR properties2;
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    properties2.pNext = &m_Properties;
    // Calling this function is all that is needed to force both libspirv.so & librtcore.so to be
    // loaded and initialized.
    m_pVulkanInst->GetPhysicalDeviceProperties2KHR(vkDev, &properties2);

    Tee::Priority pri = Tee::PriLow;
    Printf(pri, "Raytracing properties:\n");
    Printf(pri, "\t%-39s %u\n",  "shaderGroupHandleSize:",    m_Properties.shaderGroupHandleSize);
    Printf(pri, "\t%-39s %u\n",  "maxRayRelwrsionDepth:",     m_Properties.maxRayRelwrsionDepth);
    Printf(pri, "\t%-39s %u\n",  "maxShaderGroupStride:",     m_Properties.maxShaderGroupStride);
    Printf(pri, "\t%-39s %u\n",  "shaderGroupBaseAlignment:", m_Properties.shaderGroupBaseAlignment); //$
    Printf(pri, "\t%-39s %u\n",  "shaderGroupHandleCaptureReplaySize:", m_Properties.shaderGroupHandleCaptureReplaySize);
    Printf(pri, "\t%-39s %u\n",  "maxRayDispatchIlwocationCount:", m_Properties.maxRayDispatchIlwocationCount);
    Printf(pri, "\t%-39s %u\n",  "shaderGroupHandleAlignment:", m_Properties.shaderGroupHandleAlignment);
    Printf(pri, "\t%-39s %u\n",  "maxRayHitAttributeSize:", m_Properties.maxRayHitAttributeSize);

    // Create a swapchain to display the raytracing image.
    m_SwapChain = make_unique<SwapChainMods>(m_pVulkanInst, m_pVulkanDev);
    if (m_ZeroFb)
    {
        m_SwapChain->SetImageMemoryType(VK_MEMORY_PROPERTY_SYSMEM_BIT);
    }

    m_CmdPool = make_unique<VulkanCmdPool>(m_pVulkanDev);
    m_ComputeCmdPool = make_unique<VulkanCmdPool>(m_pVulkanDev);
    //--------------------------------------------------------------
    // we need one set of resources for each draw job (Ping vs Pong)
    for (size_t ii = 0; ii < m_NumDrawJobs; ii++)
    {
        m_CmdBuffers.emplace_back(make_unique<VulkanCmdBuffer>(m_pVulkanDev));
        m_ComputeCmdBuffers.emplace_back(make_unique<VulkanCmdBuffer>(m_pVulkanDev));
    }

    m_ShaderRayGen      = make_unique<VulkanShader>(m_pVulkanDev);
    m_ShaderAnyHit      = make_unique<VulkanShader>(m_pVulkanDev);
    m_ShaderClosestHit  = make_unique<VulkanShader>(m_pVulkanDev);
    m_ShaderClosestHit2 = make_unique<VulkanShader>(m_pVulkanDev);
    m_ShaderMiss        = make_unique<VulkanShader>(m_pVulkanDev);
    m_ShaderShadowMiss  = make_unique<VulkanShader>(m_pVulkanDev);
    // Lwrrently not using this shader
    m_ShaderIntersection = make_unique<VulkanShader>(m_pVulkanDev);

    m_CameraBuf         = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_SceneBuf          = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_DescriptorInfo    = make_unique<DescriptorInfo>(m_pVulkanDev);
    m_Pipeline          = make_unique<VulkanPipeline>(m_pVulkanDev);
    m_SbtBuffer         = make_unique<VulkanBuffer>(m_pVulkanDev);
    m_RenderPass        = make_unique<VulkanRenderPass>(m_pVulkanDev);
}

//-------------------------------------------------------------------------------------------------
void Raytracer::CleanupResources()
{
    if (!m_pVulkanDev)
    {
        return;
    }
    if (m_CameraBuf.get())
    {
        m_CameraBuf->Cleanup();
    }
    if (m_SceneBuf.get())
    {
        m_SceneBuf->Cleanup();
    }
    if (m_SpheresBuf.get())
    {
        m_SpheresBuf->Cleanup();
    }
    if (m_SpheresAabbBuf.get())
    {
        m_SpheresAabbBuf->Cleanup();
    }
    if (m_SpheresMatColorBuf.get())
    {
        m_SpheresMatColorBuf->Cleanup();
    }
    if (m_SpheresMatIndexBuf.get())
    {
        m_SpheresMatIndexBuf->Cleanup();
    }

    m_CmdBuffers.clear();
    m_ComputeCmdBuffers.clear();

    if (m_CmdPool.get())
    {
        m_CmdPool->DestroyCmdPool();
    }
    if (m_ComputeCmdPool.get())
    {
        m_ComputeCmdPool->DestroyCmdPool();
    }
    if (m_DescriptorInfo.get())
    {
        m_DescriptorInfo->Cleanup();
    }
    if (m_Pipeline.get())
    {
        m_Pipeline->Cleanup();
    }
    if (m_SbtBuffer.get())
    {
        m_SbtBuffer->Cleanup();
    }
    if (m_RenderPass.get())
    {
        m_RenderPass->Cleanup();
    }
    for (const auto& fb : m_FrameBuffers)
    {
        if (fb.get())
        {
             fb->Cleanup();
        }
    }
    m_FrameBuffers.clear();

    if (m_SwapChain.get())
    {
        m_SwapChain->Cleanup();
    }
    for (auto& sem : m_Sems)
    {
        m_pVulkanDev->DestroySemaphore(sem);
    }
    m_Sems.clear();

    for (auto& sem : m_RpSems)
    {
        m_pVulkanDev->DestroySemaphore(sem);
    }
    m_RpSems.clear();

    for (const auto& fence : m_Fences)
    {
        m_pVulkanDev->DestroyFence(fence);
    }
    m_Fences.clear();
    m_TSQueries.clear();

    m_Builder.Cleanup();
    for (auto &model : m_ObjModels)
    {
        model.vertexBuffer->Cleanup();
        model.indexBuffer->Cleanup();
        model.matColorBuffer->Cleanup();
        model.matIndexBuffer->Cleanup();
    }
    m_ObjModels.clear();
    m_pVulkanDev = nullptr;
}

//-------------------------------------------------------------------------------------------------
// Vulkan Raytracer APIs
const char* const szRayCommonGLSL = HS_(R"glsl(
    struct hitPayload
    {
        vec3 hitValue;
    };

    struct Sphere
    {
        vec3  center;
        float radius;
    };

    struct Aabb
    {
        vec3 minimum;
        vec3 maximum;
    };

    #define KIND_SPHERE 0
    #define KIND_LWBE 1
)glsl");

const char* const szWavefrontGLSL = HS_(R"glsl(
    struct Vertex
    {
        vec3 pos;
        vec3 nrm;
        vec3 color;
        vec2 texCoord;
    };

    struct WaveFrontMaterial
    {
        vec3  ambient;
        vec3  diffuse;
        vec3  spelwlar;
        vec3  transmittance;
        vec3  emission;
        float shininess;
        float ior;       // index of refraction
        float dissolve;  // 1 == opaque; 0 == fully transparent
        int   illum;     // illumination model (see http://www.fileformat.info/format/material/)
        int   textureId;
    };

    struct sceneDesc
    {
        int  objId;
        int  txtOffset;
        mat4 transfo;
        mat4 transfoIT;
    };


    vec3 computeDiffuse(WaveFrontMaterial mat, vec3 lightDir, vec3 normal)
    {
        // Lambertian
        float dotNL = max(dot(normal, lightDir), 0.0);
        vec3  c     = mat.diffuse * dotNL;
        if(mat.illum >= 1)
            c += mat.ambient;
        return c;
    }

    vec3 computeSpelwlar(WaveFrontMaterial mat, vec3 viewDir, vec3 lightDir, vec3 normal)
    {
        if(mat.illum < 2)
            return vec3(0);

        // Compute spelwlar only if not in shadow
        const float kPi        = 3.14159265;
        const float kShininess = max(mat.shininess, 4.0);

        // Spelwlar
        const float kEnergyConservation = (2.0 + kShininess) / (2.0 * kPi);
        vec3        V         = normalize(-viewDir);
        vec3        R         = reflect(-lightDir, normal);
        float       spelwlar  = kEnergyConservation * pow(max(dot(V, R), 0.0), kShininess);

        return vec3(mat.spelwlar * spelwlar);
    }
)glsl");

//-------------------------------------------------------------------------------------------------
void Raytracer::SetupRayGenProgram()
{
    if (!m_ProgRayGen.empty())
        return;
    m_ProgRayGen = HS_(R"glsl(
    #version 460
    #extension GL_LW_ray_tracing : require
    #extension GL_GOOGLE_include_directive : enable
    )glsl");
        m_ProgRayGen += szRayCommonGLSL; //"#include "raycommon.glsl"
        m_ProgRayGen += HS_(R"glsl(

    layout(binding = 0, set = 0) uniform accelerationStructureLW topLevelAS;
    layout(binding = 1, set = 0, rgba8) uniform image2D image;

    layout(location = 0) rayPayloadLW hitPayload prd;

    layout(binding = 0, set = 1) uniform CameraProperties
    {
        mat4 view;
        mat4 proj;
        mat4 viewIlwerse;
        mat4 projIlwerse;
    }
    cam;

    void main()
    {
        const vec2 pixelCenter = vec2(gl_LaunchIDLW.xy) + vec2(0.5);
        const vec2 inUV        = pixelCenter / vec2(gl_LaunchSizeLW.xy);
        vec2       d           = inUV * 2.0 - 1.0;

        vec4 origin    = cam.viewIlwerse * vec4(0, 0, 0, 1);
        vec4 target    = cam.projIlwerse * vec4(d.x, d.y, 1, 1);
        vec4 direction = cam.viewIlwerse * vec4(normalize(target.xyz), 0);

        uint  rayFlags = gl_RayFlagsOpaqueLW;
        float tMin     = 0.001;
        float tMax     = 10000.0;

        traceLW(topLevelAS,     // acceleration structure
                rayFlags,       // rayFlags
                0xFF,           // lwllMask
                0,              // sbtRecordOffset
                0,              // sbtRecordStride
                0,              // missIndex
                origin.xyz,     // ray origin
                tMin,           // ray min range
                direction.xyz,  // ray direction
                tMax,           // ray max range
                0);             // payload (location = 0));
    )glsl");
    if (m_DisableXORLogic)
    {
        m_ProgRayGen += HS_(R"glsl(
        imageStore(image, ivec2(gl_LaunchIDLW.xy), vec4(prd.hitValue, 1.0));
    }
    )glsl");
    }
    else
    {
        m_ProgRayGen += HS_(R"glsl(
       // Finally, we write the resulting payload XOR'd with the current buffer
       // contents so we can create a self-guilding image. Every other frame
       // should produce a black (all zeros) image.
       uvec4 uhit = floatBitsToUint(vec4(prd.hitValue, 1.0));
       vec4 col = imageLoad(image, ivec2(gl_LaunchIDLW.xy));
       uvec4 ucol = floatBitsToUint(col);
       ucol ^= uhit;
       col = uintBitsToFloat(ucol);
       imageStore(image, ivec2(gl_LaunchIDLW.xy), col);
    }
    )glsl");
    }
}

//-------------------------------------------------------------------------------------------------
void Raytracer::SetupIntersectionProgram()
{
    if (!m_ProgIntersection.empty())
        return;
    if (m_Scene == RTS_SPHERES)
    {
        m_ProgIntersection = HS_(R"glsl(
    #version 460
    #extension GL_LW_ray_tracing : require
    #extension GL_EXT_nonuniform_qualifier : enable
    #extension GL_EXT_scalar_block_layout : enable
    #extension GL_GOOGLE_include_directive : enable
    )glsl");
        m_ProgIntersection += szRayCommonGLSL; //"#include "raycommon.glsl"
        m_ProgIntersection += szWavefrontGLSL; //"#include "wavefront.glsl"
        m_ProgIntersection += HS_(R"glsl(

    hitAttributeLW vec3 HitAttribute;

    layout(binding = 7, set = 1, scalar) buffer allSpheres_
    {
        Sphere i[];
    }
    allSpheres;

    struct Ray
    {
        vec3 origin;
        vec3 direction;
    };

    // Ray-Sphere intersection
    // http://viclw17.github.io/2018/07/16/raytracing-ray-sphere-intersection/
    float hitSphere(const Sphere s, const Ray r)
    {
        vec3  oc           = r.origin - s.center;
        float a            = dot(r.direction, r.direction);
        float b            = 2.0 * dot(oc, r.direction);
        float c            = dot(oc, oc) - s.radius * s.radius;
        float discriminant = b * b - 4 * a * c;
        if(discriminant < 0)
        {
            return -1.0;
        }
        else
        {
            return (-b - sqrt(discriminant)) / (2.0 * a);
        }
    }

    // Ray-AABB intersection
    float hitAabb(const Aabb aabb, const Ray r)
    {
        vec3  ilwDir = 1.0 / r.direction;
        vec3  tbot   = ilwDir * (aabb.minimum - r.origin);
        vec3  ttop   = ilwDir * (aabb.maximum - r.origin);
        vec3  tmin   = min(ttop, tbot);
        vec3  tmax   = max(ttop, tbot);
        float t0     = max(tmin.x, max(tmin.y, tmin.z));
        float t1     = min(tmax.x, min(tmax.y, tmax.z));
        return t1 > max(t0, 0.0) ? t0 : -1.0;
    }

    void main()
    {
        Ray ray;
        ray.origin    = gl_WorldRayOriginLW;
        ray.direction = gl_WorldRayDirectionLW;

        // Sphere data
        Sphere sphere = allSpheres.i[gl_PrimitiveID];

        float tHit    = -1;
        int   hitKind = gl_PrimitiveID % 2 == 0 ? KIND_SPHERE : KIND_LWBE;
        if(hitKind == KIND_SPHERE)
        {
            // Sphere intersection
            tHit = hitSphere(sphere, ray);
        }
        else
        {
            // AABB intersection
            Aabb aabb;
            aabb.minimum = sphere.center - vec3(sphere.radius);
            aabb.maximum = sphere.center + vec3(sphere.radius);
            tHit         = hitAabb(aabb, ray);
        }

      // Report hit point
        if(tHit > 0)
            reportIntersectionLW(tHit, hitKind);
    }
    )glsl");
    }
}

//-------------------------------------------------------------------------------------------------
void Raytracer::SetupAnyHitProgram()
{
    // TODO: Implement this shader
}

//-------------------------------------------------------------------------------------------------
void Raytracer::SetupClosestHitProgram()
{
    if (!m_ProgClosestHit.empty())
        return;
    m_ProgClosestHit = HS_(R"glsl(
    #version 460
    #extension GL_LW_ray_tracing : require
    #extension GL_EXT_nonuniform_qualifier : enable
    #extension GL_EXT_scalar_block_layout : enable
    #extension GL_GOOGLE_include_directive : enable
    )glsl");
    m_ProgClosestHit += szRayCommonGLSL; //"#include "raycommon.glsl"
    m_ProgClosestHit += szWavefrontGLSL; //"#include "wavefront.glsl"
    m_ProgClosestHit += HS_(R"glsl(

    hitAttributeLW vec2 attribs;

    // clang-format off
    layout(location = 0) rayPayloadInLW hitPayload prd;
    layout(location = 1) rayPayloadLW bool isShadowed;

    layout(binding = 0, set = 0) uniform accelerationStructureLW topLevelAS;

    layout(binding = 1, set = 1, scalar) buffer MatColorBufferObject
        { WaveFrontMaterial m[]; } materials[];
    layout(binding = 2, set = 1, scalar) buffer ScnDesc { sceneDesc i[]; } scnDesc;
    //layout(binding = 3, set = 1) uniform sampler2D textureSamplers[];
    layout(binding = 4, set = 1)  buffer MatIndexColorBuffer { int i[]; } matIndex[];
    layout(binding = 5, set = 1, scalar) buffer Vertices { Vertex v[]; } vertices[];
    layout(binding = 6, set = 1) buffer Indices { uint i[]; } indices[];

    // clang-format on

    layout(push_constant) uniform Constants
    {
        vec4  clearColor;
        vec3  lightPosition;
        float lightIntensity;
        int   lightType;
    }
    pushC;


    void main()
    {
        // Object of this instance
        uint objId = scnDesc.i[gl_InstanceID].objId;

        // Indices of the triangle
        ivec3 ind = ivec3(indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 0],   //
                          indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 1],   //
                          indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 2]);  //
        // Vertex of the triangle
        Vertex v0 = vertices[nonuniformEXT(objId)].v[ind.x];
        Vertex v1 = vertices[nonuniformEXT(objId)].v[ind.y];
        Vertex v2 = vertices[nonuniformEXT(objId)].v[ind.z];

        const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

        // Computing the normal at hit position
        vec3 normal = v0.nrm * barycentrics.x + v1.nrm * barycentrics.y +
                      v2.nrm * barycentrics.z;
        // Transforming the normal to world space
        normal = normalize(vec3(scnDesc.i[gl_InstanceID].transfoIT * vec4(normal, 0.0)));

        // Computing the coordinates of the hit position
        vec3 worldPos = v0.pos * barycentrics.x + v1.pos * barycentrics.y +
                        v2.pos * barycentrics.z;
        // Transforming the position to world space
        worldPos = vec3(scnDesc.i[gl_InstanceID].transfo * vec4(worldPos, 1.0));

        // Vector toward the light
        vec3  L;
        float lightIntensity = pushC.lightIntensity;
        float lightDistance  = 100000.0;
        // Point light
        if(pushC.lightType == 0)
        {
            vec3 lDir      = pushC.lightPosition - worldPos;
            lightDistance  = length(lDir);
            lightIntensity = pushC.lightIntensity / (lightDistance * lightDistance);
            L              = normalize(lDir);
        }
        else  // Directional light
        {
            L = normalize(pushC.lightPosition - vec3(0));
        }

        // Material of the object
        int               matIdx = matIndex[nonuniformEXT(objId)].i[gl_PrimitiveID];
        WaveFrontMaterial mat    = materials[nonuniformEXT(objId)].m[matIdx];


        // Diffuse
        vec3 diffuse = computeDiffuse(mat, L, normal);
        if(mat.textureId >= 0)
        {
            uint txtId = mat.textureId + scnDesc.i[gl_InstanceID].txtOffset;
            vec2 texCoord =
                v0.texCoord * barycentrics.x + v1.texCoord * barycentrics.y +
                v2.texCoord * barycentrics.z;
            //TODO: Implement textures, for now fake it.
            //diffuse *= texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;
        }

        vec3  spelwlar    = vec3(0);
        float attenuation = 1;

        // Tracing shadow ray only if the light is visible from the surface
        if(dot(normal, L) > 0)
        {
          float tMin   = 0.001;
          float tMax   = lightDistance;
          vec3  origin = gl_WorldRayOriginLW + gl_WorldRayDirectionLW * gl_HitTLW;
          vec3  rayDir = L;
          uint  flags =
              gl_RayFlagsTerminateOnFirstHitLW | gl_RayFlagsOpaqueLW |
              gl_RayFlagsSkipClosestHitShaderLW;
          isShadowed = true;
          traceLW(topLevelAS,  // acceleration structure
                  flags,       // rayFlags
                  0xFF,        // lwllMask
                  0,           // sbtRecordOffset
                  0,           // sbtRecordStride
                  1,           // missIndex
                  origin,      // ray origin
                  tMin,        // ray min range
                  rayDir,      // ray direction
                  tMax,        // ray max range
                  1);          // payload (location = 1));

          if(isShadowed)
          {
              attenuation = 0.3;
          }
          else
          {
            // Spelwlar
              spelwlar = computeSpelwlar(mat, gl_WorldRayDirectionLW, L, normal);
          }
        }

        prd.hitValue = vec3(lightIntensity * attenuation * (diffuse + spelwlar));
    }
    )glsl");
}

//-------------------------------------------------------------------------------------------------
// This shader gets ilwoked when a ray intersects with one of the AABBs, then depending on the
// gl_HitKindLW it will interpret the geometry as a sphere or a lwbe.
void Raytracer::SetupClosestHit2Program()
{
    if (!m_ProgClosestHit2.empty() || (m_Scene != RTS_SPHERES))
    {
        return;
    }
    m_ProgClosestHit2 = HS_(R"glsl(
    #version 460
    #extension GL_LW_ray_tracing : require
    #extension GL_EXT_nonuniform_qualifier : enable
    #extension GL_EXT_scalar_block_layout : enable
    #extension GL_GOOGLE_include_directive : enable
    )glsl");
    m_ProgClosestHit2 += szRayCommonGLSL; // replace "#include "raycommon.glsl"
    m_ProgClosestHit2 += szWavefrontGLSL; // replace"#include "wavefront.glsl"
    m_ProgClosestHit2 += HS_(R"glsl(

    hitAttributeLW vec2 attribs;

    // clang-format off
    layout(location = 0) rayPayloadInLW hitPayload prd;
    layout(location = 1) rayPayloadLW bool isShadowed;

    layout(binding = 0, set = 0) uniform accelerationStructureLW topLevelAS;

    layout(binding = 1, set = 1, scalar) buffer MatColorBufferObject
        { WaveFrontMaterial m[]; } materials[];
    layout(binding = 2, set = 1, scalar) buffer ScnDesc { sceneDesc i[]; } scnDesc;
    layout(binding = 3, set = 1) uniform sampler2D textureSamplers[];
    layout(binding = 4, set = 1) buffer MatIndexColorBuffer { int i[]; } matIndex[];
    layout(binding = 5, set = 1, scalar) buffer Vertices { Vertex v[]; } vertices[];
    layout(binding = 6, set = 1) buffer Indices { uint i[]; } indices[];
    layout(binding = 7, set = 1, scalar) buffer allSpheres_ { Sphere i[]; } allSpheres;

    // clang-format on

    layout(push_constant) uniform Constants
    {
        vec4  clearColor;
        vec3  lightPosition;
        float lightIntensity;
        int   lightType;
    }
    pushC;


    void main()
    {
        vec3 worldPos = gl_WorldRayOriginLW + gl_WorldRayDirectionLW * gl_HitTLW;

        Sphere instance = allSpheres.i[gl_PrimitiveID];

        // Computing the normal at hit position
        vec3 normal = normalize(worldPos - instance.center);

        // Computing the normal for a lwbe
        if(gl_HitKindLW == KIND_LWBE)  // Aabb
        {
            vec3  absN = abs(normal);
            float maxC = max(max(absN.x, absN.y), absN.z);
            normal     = (maxC == absN.x) ?
                         vec3(sign(normal.x), 0, 0) :
                         (maxC == absN.y) ? vec3(0, sign(normal.y), 0) :
                            vec3(0, 0, sign(normal.z));
        }

        // Vector toward the light
        vec3  L;
        float lightIntensity = pushC.lightIntensity;
        float lightDistance  = 100000.0;
        // Point light
        if(pushC.lightType == 0)
        {
            vec3 lDir      = pushC.lightPosition - worldPos;
            lightDistance  = length(lDir);
            lightIntensity = pushC.lightIntensity / (lightDistance * lightDistance);
            L              = normalize(lDir);
        }
        else  // Directional light
        {
            L = normalize(pushC.lightPosition - vec3(0));
        }

        // Material of the object
        int matIdx = matIndex[nonuniformEXT(gl_InstanceID)].i[gl_PrimitiveID];
        WaveFrontMaterial mat = materials[nonuniformEXT(gl_InstanceID)].m[matIdx];

        // Diffuse
        vec3  diffuse     = computeDiffuse(mat, L, normal);
        vec3  spelwlar    = vec3(0);
        float attenuation = 0.3;

        // Tracing shadow ray only if the light is visible from the surface
        if(dot(normal, L) > 0)
        {
            float tMin   = 0.001;
            float tMax   = lightDistance;
            vec3  origin = gl_WorldRayOriginLW + gl_WorldRayDirectionLW * gl_HitTLW;
            vec3  rayDir = L;
            uint  flags =
                gl_RayFlagsTerminateOnFirstHitLW | gl_RayFlagsOpaqueLW |
                gl_RayFlagsSkipClosestHitShaderLW;
            isShadowed = true;
            traceLW(topLevelAS,  // acceleration structure
                    flags,       // rayFlags
                    0xFF,        // lwllMask
                    0,           // sbtRecordOffset
                    0,           // sbtRecordStride
                    1,           // missIndex
                    origin,      // ray origin
                    tMin,        // ray min range
                    rayDir,      // ray direction
                    tMax,        // ray max range
                    1);          // payload (location = 1));

            if(isShadowed)
            {
                attenuation = 0.3;
            }
            else
            {
                attenuation = 1;
                // Spelwlar
                spelwlar = computeSpelwlar(mat, gl_WorldRayDirectionLW, L, normal);
            }
        }

        prd.hitValue = vec3(lightIntensity * attenuation * (diffuse + spelwlar));
    }
    )glsl");
}

//-------------------------------------------------------------------------------------------------
void Raytracer::SetupMissProgram()
{
    if (!m_ProgMiss.empty())
        return;
    m_ProgMiss = HS_(R"glsl(
    #version 460
    #extension GL_LW_ray_tracing : require
    #extension GL_GOOGLE_include_directive : enable
    )glsl");
    m_ProgMiss += szRayCommonGLSL; //#include "raycommon.glsl"
    m_ProgMiss += HS_(R"glsl(

    layout(location = 0) rayPayloadInLW hitPayload prd;

    layout(push_constant) uniform Constants
    {
        vec4 clearColor;
    };

    void main()
    {
        prd.hitValue = clearColor.xyz * 0.8;
    }
    )glsl");
}

//-------------------------------------------------------------------------------------------------
void Raytracer::SetupShadowMissProgram()
{
    if (!m_ProgShadowMiss.empty())
        return;
    m_ProgShadowMiss = HS_(R"glsl(
    #version 460
    #extension GL_LW_ray_tracing : require

    layout(location = 1) rayPayloadInLW bool isShadowed;

    void main()
    {
        isShadowed = false;
    }
    )glsl");
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::SetupShaders()
{
    RC rc;

    // mandatory shader, ilwoked to begin all raytracing work.
    SetupRayGenProgram();
    if (!m_ProgRayGen.empty())
        CHECK_VK_TO_RC(m_ShaderRayGen->CreateShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                                  ,m_ProgRayGen
                                                  ,"main"
                                                  ,m_ShaderReplacement));

    // mandatory shader, ilwoked when no intersection isfound for a given ray
    SetupMissProgram();
    if (!m_ProgMiss.empty())
    {
        CHECK_VK_TO_RC(m_ShaderMiss->CreateShader(VK_SHADER_STAGE_MISS_BIT_KHR
                                                  ,m_ProgMiss
                                                  ,"main"
                                                  ,m_ShaderReplacement));
    }

    // mandatory shader, ilwoked when a shadow ray misses the geometry
    SetupShadowMissProgram();
    if (!m_ProgShadowMiss.empty())
    {
        CHECK_VK_TO_RC(m_ShaderShadowMiss->CreateShader(VK_SHADER_STAGE_MISS_BIT_KHR
                                                  ,m_ProgShadowMiss
                                                  ,"main"
                                                  ,m_ShaderReplacement));
    }

    // mandatory shader, ilwoked only on the closest intersection point along the ray
    SetupClosestHitProgram();
    if (!m_ProgClosestHit.empty())
    {
        CHECK_VK_TO_RC(m_ShaderClosestHit->CreateShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                                                  ,m_ProgClosestHit
                                                  ,"main"
                                                  ,m_ShaderReplacement));
    }

    // mandatory shader(for spheres), ilwoked only on the closest intersection point
    SetupClosestHit2Program();
    if (!m_ProgClosestHit2.empty())
    {
        CHECK_VK_TO_RC(m_ShaderClosestHit2->CreateShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                                                  ,m_ProgClosestHit2
                                                  ,"main"
                                                  ,m_ShaderReplacement));
    }

    // mandadory shader(for spheres), not required for triangle primitives because this support
    // is built in.
    SetupIntersectionProgram();
    if (!m_ProgIntersection.empty())
    {
        CHECK_VK_TO_RC(m_ShaderIntersection->CreateShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR
                                                      ,m_ProgIntersection
                                                      ,"main"
                                                      ,m_ShaderReplacement));
    }

    // optional shader, ilwoked on all intersections of a ray with scene primitives.
    SetupAnyHitProgram();
    if (!m_ProgAnyHit.empty())
    {
        CHECK_VK_TO_RC(m_ShaderAnyHit->CreateShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR
                                                  ,m_ProgAnyHit
                                                  ,"main"
                                                  ,m_ShaderReplacement));
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
void Raytracer::CleanupAfterSetup() const
{
    m_ShaderRayGen->Cleanup();
    m_ShaderIntersection->Cleanup();
    m_ShaderAnyHit->Cleanup();
    m_ShaderClosestHit->Cleanup();
    m_ShaderClosestHit2->Cleanup();
    m_ShaderMiss->Cleanup();
    m_ShaderShadowMiss->Cleanup();
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::SetupRaytracing
(
    vector<unique_ptr<VulkanGoldenSurfaces>> &rGoldenSurfaces,
    VulkanCmdBuffer * pCheckSurfCmdBuf,
    VulkanCmdBuffer * pCheckSurfTransBuf
)
{
    RC rc;

    if ((m_FramesPerSubmit % 2) && !m_Debug)
    {
        Printf(Tee::PriError, "Raytracer.FramesPerSubmit must be a multiple of 2\n");
        return RC::ILWALID_ARGUMENT;
    }

    AllocateResources();

    if (m_DisableXORLogic)
    {
        Printf(Tee::PriWarn,
               "Disabling Raytracing XOR logic will disable surface check on RT images\n");
    }
    // Allocate 2 Command buffers for ping-pong processing.
    UINT32 compFamilyIdx = m_pVulkanDev->GetPhysicalDevice()->GetComputeQueueFamilyIdx();
    UINT32 gfxFamilyIdx  = m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx();

    CHECK_VK_TO_RC(m_CmdPool->InitCmdPool(gfxFamilyIdx, m_GraphicsQueueIdx));
    CHECK_VK_TO_RC(m_ComputeCmdPool->InitCmdPool(compFamilyIdx, m_ComputeQueueIdx));
    for (size_t ii = 0; ii < m_NumDrawJobs; ii++)
    {
        CHECK_VK_TO_RC(m_CmdBuffers[ii]->AllocateCmdBuffer(m_CmdPool.get()));
        CHECK_VK_TO_RC(m_ComputeCmdBuffers[ii]->AllocateCmdBuffer(m_ComputeCmdPool.get()));
    }

    CHECK_RC(LoadModel(m_Scene));
    SetupPushConstants();
    CHECK_RC(SetupMatrices());
    CHECK_RC(CreateSceneDescriptionBuffer());
    m_SwapChain->SetImageUsage(VK_IMAGE_USAGE_STORAGE_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    CHECK_VK_TO_RC(m_SwapChain->Init(m_Width
                                     ,m_Height
                                     ,static_cast<SwapChain::ImageMode>(m_SwapChainMode)
                                     ,gfxFamilyIdx
                                     ,m_GraphicsQueueIdx));

    const UINT32 swapChainSize = static_cast<UINT32>(m_SwapChain->GetNumImages());
    m_GoldenSurfaceIdx = static_cast<UINT32>(rGoldenSurfaces.size());
    for (UINT32 i =0; i < swapChainSize; i++)
    {
        VulkanImage *pImage = m_SwapChain->GetSwapChainImage(i);
        // Switch to general layout to support the descriptor binding 0.
        CHECK_VK_TO_RC(pImage->SetImageLayout(m_CmdBuffers[0].get(),
                               pImage->GetImageAspectFlags(),
                               VK_IMAGE_LAYOUT_GENERAL,
                               pImage->GetImageAccessMask(),
                               pImage->GetImageStageMask()));
        pImage->SetImageName("RtImage");

        rGoldenSurfaces.emplace_back(make_unique<VulkanGoldenSurfaces>(m_pVulkanInst));
    }

    // Add a golden surface for results checking.
    for (UINT32 i =0; i < swapChainSize; i++)
    {
        string name = Utility::StrPrintf("rtgs%1.1d", i);
        auto &gs = rGoldenSurfaces[m_GoldenSurfaceIdx + i];
        CHECK_VK_TO_RC(gs->AddSurface(name
                                      ,m_SwapChain->GetSwapChainImage(i)
                                      ,pCheckSurfCmdBuf
                                      ,pCheckSurfTransBuf));
    }

    // Create the render pass.
    VkAttachmentDescription colorAttachment =
    {
        0,
        m_SwapChain->GetSurfaceFormat(),
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        //storing is necessary for display:
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_GENERAL
    };
    m_RenderPass->PushAttachmentDescription(VkUtil::AttachmentType::COLOR, &colorAttachment);

    VkAttachmentReference colorReference[] =
    {{ //$
        0,
        VK_IMAGE_LAYOUT_GENERAL
    }};
    VkSubpassDescription subpassDesc =
    {
        0,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,
        nullptr,
        1,
        colorReference,
        nullptr,
        nullptr,
        0,
        nullptr
    };
    m_RenderPass->PushSubpassDescription(&subpassDesc);
    CHECK_VK_TO_RC(m_RenderPass->CreateRenderPass());


    m_FrameBuffers.reserve(m_SwapChain->GetNumImages());
    for (UINT32 i = 0; i < m_SwapChain->GetNumImages(); i++)
    {
        // Setup the framebuffer, raytracing only uses a color surface.
        vector<VkImageView> attachments;
        attachments.push_back(m_SwapChain->GetImageView(i));
        m_FrameBuffers.emplace_back(make_unique<VulkanFrameBuffer>(m_pVulkanDev));

        m_FrameBuffers[i]->SetWidth(m_SwapChain->GetSwapChainExtent().width);
        m_FrameBuffers[i]->SetHeight(m_SwapChain->GetSwapChainExtent().height);
        CHECK_VK_TO_RC(m_FrameBuffers[i]->CreateFrameBuffer(attachments,
                                                            m_RenderPass->GetRenderPass()));
    }


    //Geometry
    m_Builder.Setup(m_pVulkanDev, m_ComputeQueueIdx, compFamilyIdx);
    CHECK_RC(CreateBottomLevelAS());
    CHECK_RC(CreateTopLevelAS());
    CHECK_RC(CreateDescriptorSetLayout());
    CHECK_RC(InitializeFB());
    UpdateDescriptorSet(0);
    // Do we really need to wait for the device to go idle?
    CHECK_VK_TO_RC(m_pVulkanDev->DeviceWaitIdle());
    m_Pipeline->AddPushConstantRange(
    {
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR, // stageFlags
        0,                            // offset
        sizeof(m_PushConstants)    //
    });
    vector<VkDescriptorSetLayout> layouts;
    layouts.push_back(m_DescriptorInfo->GetDescriptorSetLayout(0));
    layouts.push_back(m_DescriptorInfo->GetDescriptorSetLayout(1));

    CHECK_RC(m_Pipeline->CreateRaytracingPipelineLayout(move(layouts)));

    CHECK_VK_TO_RC(m_CmdBuffers[0]->BeginCmdBuffer());
    VkDescriptorSet descSet[2] = {};
    descSet[0] = m_DescriptorInfo->GetDescriptorSet(0);
    descSet[1] = m_DescriptorInfo->GetDescriptorSet(1);
    m_pVulkanDev->CmdBindDescriptorSets(m_CmdBuffers[0]->GetCmdBuffer(),
                                        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, // bind point
                                        m_Pipeline->GetPipelineLayout(),      // layout
                                        0,                                    // first set
                                        2,                                    // set count
                                        descSet,                              // pDescriptorsSets
                                        0,                                    // dynamicOffsetCount
                                        nullptr);                             // pDynamicOffsets
    CHECK_VK_TO_RC(m_CmdBuffers[0]->EndCmdBuffer());
    CHECK_VK_TO_RC(m_CmdBuffers[0]->ExelwteCmdBuffer(true, true));
    CHECK_VK_TO_RC(m_pVulkanDev->DeviceWaitIdle());

    // create the Shader Binding Table (SBT) for all 5 shaders
    m_ShaderGroupHandleSize = ALIGN_UP(m_Properties.shaderGroupHandleSize,
                                       m_Properties.shaderGroupBaseAlignment);
    CHECK_VK_TO_RC(m_SbtBuffer->CreateBuffer(VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             5 * m_ShaderGroupHandleSize,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    DEFER
    {
        CleanupAfterSetup();
    };

    CHECK_RC(SetupShaders());
    // Setup the raytracing pipeline and binding table
    vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    vector<VkPipelineShaderStageCreateInfo> stages;
    MASSERT(!m_ProgRayGen.empty()     && !m_ProgMiss.empty() &&
            !m_ProgShadowMiss.empty() && !m_ProgClosestHit.empty());
    //shader=0
    stages.push_back(m_ShaderRayGen->GetShaderStageInfo());
    VkRayTracingShaderGroupCreateInfoKHR rgInfo =
    {
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        nullptr,
        VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        static_cast<uint32_t>(stages.size()-1),
        VK_SHADER_UNUSED_KHR,
        VK_SHADER_UNUSED_KHR,
        VK_SHADER_UNUSED_KHR
    };
    groups.push_back(rgInfo);

    //shader=1
    stages.push_back(m_ShaderMiss->GetShaderStageInfo());
    VkRayTracingShaderGroupCreateInfoKHR mgInfo =
    {
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        nullptr,
        VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        static_cast<uint32_t>(stages.size()-1),
        VK_SHADER_UNUSED_KHR,
        VK_SHADER_UNUSED_KHR,
        VK_SHADER_UNUSED_KHR
    };
    groups.push_back(mgInfo);

    //shader=2
    stages.push_back(m_ShaderShadowMiss->GetShaderStageInfo());
    mgInfo.generalShader = static_cast<uint32_t>(stages.size()-1);
    groups.push_back(mgInfo);

    //shader=3, Hit Group0 - Closest Hit
    stages.push_back(m_ShaderClosestHit->GetShaderStageInfo());
    VkRayTracingShaderGroupCreateInfoKHR hg0Info =
    {
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        nullptr,
        VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        VK_SHADER_UNUSED_KHR,
        static_cast<uint32_t>(stages.size()-1),
        VK_SHADER_UNUSED_KHR,
        VK_SHADER_UNUSED_KHR,
    };
    groups.push_back(hg0Info);

    if (m_Scene == RTS_SPHERES)
    {
        MASSERT(!m_ProgClosestHit2.empty() && !m_ProgIntersection.empty());
        // Hit Group1 - Closest Hit + Intersection (procedural)
        VkRayTracingShaderGroupCreateInfoKHR hg1Info =
        {
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            nullptr,
            VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR,
        };
        // shader = 4
        stages.push_back(m_ShaderClosestHit2->GetShaderStageInfo());
        hg1Info.closestHitShader   = static_cast<uint32_t>(stages.size()-1);

        //shader=5
        stages.push_back(m_ShaderIntersection->GetShaderStageInfo());
        hg1Info.intersectionShader = static_cast<uint32_t>(stages.size()-1);
        groups.push_back(hg1Info);
    }

    const size_t numGroups = groups.size();

    CHECK_VK_TO_RC(m_Pipeline->SetupRaytracingPipeline(
        {}, // Descriptor set layouts already created
        move(stages),
        move(groups),
        {}, // Push constant already set
        VulkanPipeline::CREATE_PIPELINE,
        6));

    //Update the Shader Binding Table with the shader handles
    const VkDeviceSize sbtSize = numGroups * m_ShaderGroupHandleSize;
    vector<char> sbtMemory(sbtSize, 0);

    for (UINT32 i = 0; i < numGroups; i++)
    {
        CHECK_VK_TO_RC(m_pVulkanDev->GetRayTracingShaderGroupHandlesKHR(
            m_Pipeline->GetPipeline(),
            i,                                          // firstGroup
            1,                                          // groupCount
            m_ShaderGroupHandleSize,                    // dataSize
            &sbtMemory[i * m_ShaderGroupHandleSize]));  // pData
    }

    CHECK_VK_TO_RC(m_CmdBuffers[0]->BeginCmdBuffer());
    CHECK_VK_TO_RC(VkUtil::UpdateBuffer(
        m_CmdBuffers[0].get(),                          // pCmdBuffer
        m_SbtBuffer.get(),                              // bDstBuffer
        0,                                              // offset
        sbtSize,                                        // size
        sbtMemory.data(),                               // pData
        VK_ACCESS_TRANSFER_WRITE_BIT,                   // srcAccess
        VK_ACCESS_SHADER_READ_BIT,                      // dstAccess
        VK_PIPELINE_STAGE_TRANSFER_BIT,                 // srcStageMask
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,    // dstStageMask
        VkUtil::SEND_BARRIER                            // sendBarrier
        ));

    CHECK_VK_TO_RC(m_CmdBuffers[0]->EndCmdBuffer());
    CHECK_VK_TO_RC(m_CmdBuffers[0]->ExelwteCmdBuffer(true, true));
    CHECK_VK_TO_RC(m_pVulkanDev->DeviceWaitIdle());

    // Setup the raytracing fences
    VkFenceCreateInfo fenceInfo =
    {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        nullptr,
        0
    };
    m_Fences.resize(2);
    for (auto& fence : m_Fences)
    {
        CHECK_VK_TO_RC(m_pVulkanDev->CreateFence(&fenceInfo, nullptr, &fence));
    }

    // Semaphores are created in the "unsignaled" state. There is no flag to initialize them
    // in the signaled state.
    VkSemaphoreCreateInfo semInfo =
    {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        nullptr,
        0
    };
    m_Sems.resize(2);
    for (auto& sem : m_Sems)
    {
        CHECK_VK_TO_RC(m_pVulkanDev->CreateSemaphore(&semInfo, nullptr, &sem));
    }
    m_RpSems.resize(2);
    for (auto& sem : m_RpSems)
    {
        CHECK_VK_TO_RC(m_pVulkanDev->CreateSemaphore(&semInfo, nullptr, &sem));
    }

    // Setup the query objects for raytracing
    m_TSQueries.reserve(m_NumTsQueries);
    for (UINT32 i = 0; i < m_NumTsQueries; i++)
    {
        m_TSQueries.emplace_back(make_unique<VulkanQuery>(m_pVulkanDev));
        CHECK_RC(m_TSQueries.back()->Init(VK_QUERY_TYPE_TIMESTAMP, 2, 0));
        CHECK_RC(m_TSQueries.back()->CmdReset(m_CmdBuffers[0].get(), 0, 2));
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::SendWorkToGpu(UINT64 drawJobIdx)
{
    RC rc;
    const size_t lwrDrawJobIdx  = static_cast<size_t>(drawJobIdx % 2);
    const size_t prevDrawJobIdx = static_cast<size_t>((drawJobIdx - 1) % 2);
    VkSemaphore waitSem = drawJobIdx ? m_Sems[prevDrawJobIdx] : static_cast<VkSemaphore>(VK_NULL_HANDLE);
    if (m_UseComputeQueue)
    {
        // Send the graphics work
        CHECK_VK_TO_RC(m_CmdBuffers[lwrDrawJobIdx]->ExelwteCmdBuffer(
            waitSem,                            // wait semaphore
            1,                                  // num signal sems
            &m_RpSems[lwrDrawJobIdx],           // signal sem
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            false,                              // don't wait on a fence
            VK_NULL_HANDLE));                   // don't signal a fence

        //Wait on graphics work and then send the compute work
        CHECK_VK_TO_RC(m_ComputeCmdBuffers[lwrDrawJobIdx]->ExelwteCmdBuffer(
            m_RpSems[lwrDrawJobIdx],           // wait on the renderpass semaphore above
            1,                                  // num signal sems
            &m_Sems[lwrDrawJobIdx],             // signal sem
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            false,                              // don't wait on a fence
            m_Fences[lwrDrawJobIdx]));          // signal fence
    }
    else
    {
        CHECK_VK_TO_RC(m_CmdBuffers[lwrDrawJobIdx]->ExelwteCmdBuffer(
            waitSem,                            // wait semaphore
            1,                                  // num signal sems
            &m_Sems[lwrDrawJobIdx],             // signal sem
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            false,                              // don't wait on a fence
            m_Fences[lwrDrawJobIdx]));          // signal fence

    }

    m_NumFrames += m_FramesPerSubmit;

    return rc;
}

//-------------------------------------------------------------------------------------------------
VkFence Raytracer::GetFence(UINT32 idx)
{
    if (idx < m_Fences.size())
    {
        return (m_Fences[idx]);
    }
    MASSERT(!"Raytracer::Invalid fence index!");
    return VK_NULL_HANDLE;
}

//-------------------------------------------------------------------------------------------------
SwapChain * Raytracer::GetSwapChain()
{
    return m_SwapChain.get();
}

//-------------------------------------------------------------------------------------------------
const char* Raytracer::GetSceneStr()
{
    switch (m_Scene)
    {
        case Raytracer::RTS_LWBE:
            return "Lwbe";
        case Raytracer::RTS_SPHERES:
            return "Spheres";
        default:
            return "Unknown";
    }
}

//-------------------------------------------------------------------------------------------------
UINT64 Raytracer::GetTSResults(UINT32 lwrIdx, UINT32 rsltIdx)
{
    MASSERT(lwrIdx < m_TSQueries.size() && rsltIdx < 2);
    const UINT64* const pResults =
        reinterpret_cast<const UINT64*>(m_TSQueries[lwrIdx]->GetResultsPtr());
    return pResults[rsltIdx];
}

//-------------------------------------------------------------------------------------------------
// Colwert the object primitive to the ray tracing geometry used by teh BLAS
Raytracer::ObjectInfo Raytracer::ObjectToVkGeometry(const ObjectModel& model)
{
    ObjectInfo obj = { };

    VkAccelerationStructureGeometryKHR& geometry = obj.geometry;

    geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureGeometryTrianglesDataKHR& triangles = geometry.geometry.triangles;

    // now initialize the Geometry object
    triangles.sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat              = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress  = model.vertexBuffer->GetBufferDeviceAddress();
    triangles.vertexStride              = sizeof(Vertex);
    triangles.maxVertex                 = model.numVertices - 1;
    triangles.indexType                 = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress   = model.indexBuffer->GetBufferDeviceAddress();
    triangles.transformData.hostAddress = nullptr;

    VkAccelerationStructureBuildRangeInfoKHR& buildOffsetInfo = obj.buildOffsetInfo;
    MASSERT((model.numIndices % 3) == 0);
    buildOffsetInfo.primitiveCount = model.numIndices / 3;

    return obj;
}

//-------------------------------------------------------------------------------------------------
inline float sign(float s)
{
    return (s < 0.f) ? -1.f : 1.f;
}

//-------------------------------------------------------------------------------------------------
// Orbit the camera around the center of interest. If ilwert is true then the camera stays in
// place and the point of interest orbits around the camera.
// TODO: put this code in the Camera class and colwert that class to use the glm or the lwMath
// library.
void Raytracer::Orbit
(
    float dx,           //degrees in the x-axis
    float dy,           //degrees in the y-axis
    bool ilwert,        //if true rotate the object around the camera
    glm::vec3 *pos,     //position (Eye) of the cameraProperties`
    glm::vec3 *poi,     //point of interest
    const glm::vec3& up //up vector
)
{
    // Full width will do a full turn
    dx = dx * float(glm::two_pi<float>()) / 360.0f;
    dy = dy * float(glm::two_pi<float>()) / 360.0f;
    // Get the camera
    glm::vec3 origin(ilwert ? *pos : *poi);
    glm::vec3 position(ilwert ? *poi : *pos);

    // Get the length of sight
    glm::vec3 centerToEye(position - origin);
    float radius = glm::length(centerToEye);
    centerToEye = glm::normalize(centerToEye);

    glm::mat4 rot_x, rot_y;

    // Find the rotation around the UP axis (Y)
    glm::vec3 axe_z(glm::normalize(centerToEye));
    rot_y = glm::rotate(-dx, up);

    // Apply the (Y) rotation to the eye-center vector
    glm::vec4 vect_tmp = rot_y * glm::vec4(centerToEye.x, centerToEye.y, centerToEye.z, 0);
    centerToEye = glm::vec3(vect_tmp.x, vect_tmp.y, vect_tmp.z);

    // Find the rotation around the X vector: cross between eye-center and up (X)
    glm::vec3 axe_x = glm::cross(up, axe_z);
    axe_x = glm::normalize(axe_x);
    rot_x = glm::rotate(-dy, axe_x);

    // Apply the (X) rotation to the eye-center vector
    vect_tmp = rot_x * glm::vec4(centerToEye.x, centerToEye.y, centerToEye.z, 0);
    glm::vec3 vect_rot(vect_tmp.x, vect_tmp.y, vect_tmp.z);
    if (sign(vect_rot.x) == sign(centerToEye.x))
    {
        centerToEye = vect_rot;
    }

    // Make the vector as long as it was originally
    centerToEye *= radius;

    // Finding the new position
    glm::vec3 newPosition = centerToEye + origin;

    if (!ilwert)
    {
        *pos = newPosition;  // Normal: change the position of the camera
    }
    else
    {
        *poi = newPosition;  // Ilwerted: change the interest point
    }
}

//-------------------------------------------------------------------------------------------------
// Create a storage buffer containing the description of the scene elements
// - Which geometry is used by which instance
// - Transformation
// - Offset of the texture TDB
RC Raytracer::CreateSceneDescriptionBuffer()
{
    RC rc;
    UINT32 buffSize = static_cast<UINT32>(sizeof(ObjInstance) * m_ObjInstances.size());
    // create a temporary staging buffer
    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    VulkanBuffer stage(m_pVulkanDev);
    CHECK_VK_TO_RC(stage.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, buffSize, memFlags));
    CHECK_VK_TO_RC(stage.SetData(m_ObjInstances.data(), buffSize, 0));

    // Copy to device memory
    CHECK_VK_TO_RC(m_SceneBuf->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            buffSize,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(), &stage, m_SceneBuf.get()));
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Each vertex has the following properties that are packed into a set of 3 vec4 structures.
// vec3 pos
// vec3 nrm
// vec3 color
// vec2 texCoord
RC Raytracer::GenLwbeModel()
{
    RC rc;
    ObjInstance instance;
    instance.objIndex       = static_cast<INT32>(m_ObjModels.size());
    instance.txtOffset      = 0; //TODO: implement textures for raytracing.
    instance.transform      = lwmath::mat4f(1);
    instance.transformIT    = lwmath::ilwert(instance.transform);
    m_ObjInstances.push_back(instance);

    Raytracer::Vertex lwbeVerts[36] =
    {   // pos                   nrml           col         txcd
        // face 0 Left (luminate with shadow)
        { { -0.5f, -0.5f, -0.5f }, {-1.0f,  0.0f,  0.0f }, {0.0f, 0.0f, 1.0f }, {0.0f, 1.0f } },
        { { -0.5f, -0.5f,  0.5f }, {-1.0f,  0.0f,  0.0f }, {0.0f, 0.0f, 1.0f }, {1.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, {-1.0f,  0.0f,  0.0f }, {0.0f, 0.0f, 1.0f }, {1.0f, 0.0f } },
        { { -0.5f, -0.5f, -0.5f }, {-1.0f,  0.0f,  0.0f }, {0.0f, 0.0f, 1.0f }, {0.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, {-1.0f,  0.0f,  0.0f }, {0.0f, 0.0f, 1.0f }, {1.0f, 0.0f } },
        { { -0.5f,  0.5f, -0.5f }, {-1.0f,  0.0f,  0.0f }, {0.0f, 0.0f, 1.0f }, {0.0f, 0.0f } },
        //face 1.0f Front (luminate with lighting)
        { { -0.5f, -0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, {0.0f, 1.0f, 0.0f }, {0.0f, 1.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, {0.0f, 1.0f, 0.0f }, {1.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, {0.0f, 1.0f, 0.0f }, {1.0f, 0.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, {0.0f, 1.0f, 0.0f }, {0.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, {0.0f, 1.0f, 0.0f }, {1.0f, 0.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, {0.0f, 1.0f, 0.0f }, {0.0f, 0.0f } },
        //face 2 Right (luminate with lighting)
        { {  0.5f, -0.5f,  0.5f }, { 1.0f,  0.0f,  0.0f }, {0.0f, 1.0f, 1.0f }, {0.0f, 1.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f,  0.0f,  0.0f }, {0.0f, 1.0f, 1.0f }, {1.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f,  0.0f,  0.0f }, {0.0f, 1.0f, 1.0f }, {1.0f, 0.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f,  0.0f,  0.0f }, {0.0f, 1.0f, 1.0f }, {0.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f,  0.0f,  0.0f }, {0.0f, 1.0f, 1.0f }, {1.0f, 0.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f,  0.0f,  0.0f }, {0.0f, 1.0f, 1.0f }, {0.0f, 0.0f } },
        //face 3 Back (luminate with shadow)
        { {  0.5f, -0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, {1.0f, 0.0f, 1.0f }, {0.0f, 1.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, {1.0f, 0.0f, 1.0f }, {1.0f, 1.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, {1.0f, 0.0f, 1.0f }, {1.0f, 0.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, {1.0f, 0.0f, 1.0f }, {0.0f, 1.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, {1.0f, 0.0f, 1.0f }, {1.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, {1.0f, 0.0f, 1.0f }, {0.0f, 0.0f } },
        //face 4 Bottom surface (luminate with shadow)
        { {  0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f,  0.0f }, {1.0f, 1.0f, 0.0f }, {0.0f, 0.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f,  0.0f }, {1.0f, 1.0f, 0.0f }, {0.0f, 1.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f,  0.0f }, {1.0f, 1.0f, 0.0f }, {1.0f, 1.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f,  0.0f }, {1.0f, 1.0f, 0.0f }, {1.0f, 0.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f,  0.0f }, {1.0f, 1.0f, 0.0f }, {0.0f, 1.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f,  0.0f }, {1.0f, 1.0f, 0.0f }, {1.0f, 0.0f } },
        //face 5 Top surface (luminate with lighting)
        { { -0.5f,  0.5f, -0.5f }, { 0.0f,  1.0f,  0.0f }, {1.0f, 1.0f, 1.0f }, {0.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 0.0f,  1.0f,  0.0f }, {1.0f, 1.0f, 1.0f }, {1.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 0.0f,  1.0f,  0.0f }, {1.0f, 1.0f, 1.0f }, {1.0f, 0.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 0.0f,  1.0f,  0.0f }, {1.0f, 1.0f, 1.0f }, {0.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 0.0f,  1.0f,  0.0f }, {1.0f, 1.0f, 1.0f }, {1.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 0.0f,  1.0f,  0.0f }, {1.0f, 1.0f, 1.0f }, {0.0f, 0.0f } },
    };

    UINT32 indices[36];
    for (UINT32 i = 0; i < 36; i++)
    {
        indices[i] = i;
    }
    //One material object for each each lwbe face
    MaterialObj materials[6] =
    {   //ambient   diffuse    spelwlar   trans      emission   shiny ior   dissolve illum texID
        { {0, 0, 1}, {0, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 1.0f, 0.0f, 1.0f,    1,    -1 },
        { {0, 1, 0}, {0, 1, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 1.0f, 0.0f, 1.0f,    1,    -1 },
        { {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 1.0f, 0.0f, 1.0f,    1,    -1 },
        { {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 1.0f, 0.0f, 1.0f,    1,    -1 },
        { {1, 1, 0}, {1, 1, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 1.0f, 0.0f, 1.0f,    1,    -1 },
        { {1, 1, 1}, {1, 1, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 1.0f, 0.0f, 1.0f,    1,    -1 },
    };
    UINT32 matIndices[12] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5 };

    UINT32 stageBuffSize = static_cast<UINT32>(max(sizeof(materials), sizeof(lwbeVerts)));

    // create a temporary staging buffer
    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    VulkanBuffer stage(m_pVulkanDev);
    CHECK_VK_TO_RC(stage.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stageBuffSize, memFlags));

    // Create the model object
    m_ObjModels.push_back(
        {
            0, 0,
            make_unique<VulkanBuffer>(m_pVulkanDev),
            make_unique<VulkanBuffer>(m_pVulkanDev),
            make_unique<VulkanBuffer>(m_pVulkanDev),
            make_unique<VulkanBuffer>(m_pVulkanDev),
        });
    ObjectModel& model = m_ObjModels.back();
    model.numVertices = 36;
    model.numIndices = 36;
    // copy the vertex data
    CHECK_VK_TO_RC(stage.SetData(lwbeVerts, sizeof(lwbeVerts), 0));
    model.vertexBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(model.vertexBuffer->CreateBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                    sizeof(lwbeVerts),
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(), &stage, model.vertexBuffer.get()));

    // Create and load the index data
    CHECK_VK_TO_RC(stage.SetData(indices, sizeof(indices), 0));
    model.indexBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(model.indexBuffer->CreateBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   sizeof(indices),
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(), &stage, model.indexBuffer.get()));

    // Create and load the material data
    CHECK_VK_TO_RC(stage.SetData(materials, sizeof(materials), 0));
    model.matColorBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(model.matColorBuffer->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      sizeof(materials),
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(), &stage, model.matColorBuffer.get()));

    // Create and load the material index buffer
    CHECK_VK_TO_RC(stage.SetData(matIndices, sizeof(matIndices), 0));
    model.matIndexBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(model.matIndexBuffer->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      sizeof(matIndices),
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(), &stage, model.matIndexBuffer.get()));
    return rc;
}

//-------------------------------------------------------------------------------------------------
// This code loosely mimics the HelloVulkan::loadModel() however the model contents are in code
// rather that in a binary file.
RC Raytracer::LoadModel(UINT32 scene)
{
    RC rc;
    if (scene == RTS_LWBE)
    {
        CHECK_RC(GenLwbeModel());
    }
    ObjInstance instance;
    instance.objIndex       = static_cast<INT32>(m_ObjModels.size());
    instance.txtOffset      = 0; //TODO: implement textures for raytracing.
    instance.transform      = lwmath::mat4f(1);
    instance.transformIT    = lwmath::ilwert(instance.transform);
    m_ObjInstances.push_back(instance);

    m_ObjModels.push_back(
    {
        0, 0,
        make_unique<VulkanBuffer>(m_pVulkanDev),
        make_unique<VulkanBuffer>(m_pVulkanDev),
        make_unique<VulkanBuffer>(m_pVulkanDev),
        make_unique<VulkanBuffer>(m_pVulkanDev),
    });
    ObjectModel& model = m_ObjModels.back();
    MaterialObj mat;
    UINT32 stageBuffSize = static_cast<UINT32>(max(sizeof(mat), 6*sizeof(Raytracer::Vertex)));

    // Create a large empty plane (2 triangles) to render the lwbe or spheres on.
    Raytracer::Vertex simplePlaneVerts[6] =
    {   // pos                     nrml           col         txcd
        { { -20.0,  0.0, -20.0 }, { 0,  1,  0 }, {1, 0, 0 }, {0, 1 }},
        { { -20.0,  0.0,  20.0 }, { 0,  1,  0 }, {0, 1, 0 }, {1, 1 }},
        { {  20.0,  0.0, -20.0 }, { 0,  1,  0 }, {0, 0, 1 }, {1, 0 }},
        { { -20.0,  0.0,  20.0 }, { 0,  1,  0 }, {1, 0, 0 }, {1, 1 }},
        { {  20.0,  0.0,  20.0 }, { 0,  1,  0 }, {0, 1, 0 }, {0, 0 }},
        { {  20.0,  0.0, -20.0 }, { 0,  1,  0 }, {0, 0, 1 }, {1, 0 }},
    };

    // create a temporary staging buffer
    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    VulkanBuffer stage(m_pVulkanDev);
    CHECK_VK_TO_RC(stage.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stageBuffSize, memFlags));

    // copy the vertex data
    CHECK_VK_TO_RC(stage.SetData(simplePlaneVerts, sizeof(simplePlaneVerts), 0));

    model.numIndices = 6;
    model.numVertices = 6;
    model.vertexBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(model.vertexBuffer->CreateBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                    sizeof(simplePlaneVerts),
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(), &stage, model.vertexBuffer.get()));

    // copy the index data
    UINT32 indices[6] = { 0, 1, 2, 3, 4, 5 };
    CHECK_VK_TO_RC(stage.SetData(indices, sizeof(indices), 0));
    model.indexBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(model.indexBuffer->CreateBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   sizeof(indices),
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(), &stage, model.indexBuffer.get()));

    // Create and load the material object
    // Note: we are using illumination model 2 (Highlight on) so the color values in the plane
    //       object are ignored and the material values below are used instead.
    mat.ambient         = lwmath::vec3f(1.0f, 0.0f, 0.0f);  // red
    mat.diffuse         = lwmath::vec3f(1.0f, 0.0f, 0.0f);  // red
    mat.spelwlar        = lwmath::vec3f(1.0f, 1.0f, 1.0f);
    mat.transmittance   = lwmath::vec3f(0.0f, 0.0f, 0.0f);
    mat.emission        = lwmath::vec3f(0.0f, 0.0f, 0.0f);
    mat.shininess       = 8.0f;
    mat.ior             = 0.0f;
    mat.dissolve        = 1.0f; // 0 = fully transparent, 1 = fully opaque
    mat.illum           = 2;    // Highlight on
    mat.textureID       = -1;
    CHECK_VK_TO_RC(stage.SetData(&mat, sizeof(mat), 0));
    model.matColorBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(model.matColorBuffer->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      sizeof(mat),
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(), &stage, model.matColorBuffer.get()));

    // Create and load the material index buffer
    indices[0] = indices[1] = 0;
    CHECK_VK_TO_RC(stage.SetData(indices, sizeof(UINT32)*2, 0));
    model.matIndexBuffer = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(model.matIndexBuffer->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      sizeof(UINT32)*2,
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(), &stage, model.matIndexBuffer.get()));
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::CreateSpheres()
{
    RC rc;

    if (!m_NumSpheres)
    {
        Printf(Tee::PriError, "Raytracer.NumSpheres must be greater than 0\n");
        return RC::ILWALID_ARGUMENT;
    }

    m_FpCtx.Rand.SeedRandom(m_Seed);
    Sphere s;
    m_Spheres.reserve(m_NumSpheres);
    for (uint i = 0; i < m_NumSpheres; i++)
    {
        s.center = lwmath::vec3f(m_FpCtx.Rand.GetRandomFloat(-18.0, +18.0),    //x-axis
                                 m_FpCtx.Rand.GetRandomFloat(0.0f, 5.0f),      //y-axis
                                 m_FpCtx.Rand.GetRandomFloat(-18.0f, 18.0f));   //z-axis
        s.radius = m_FpCtx.Rand.GetRandomFloat(0.05f, 0.2f);
        m_Spheres.push_back(s);
    }
    // now create the Axis-Aligned-Boundry-Box for each sphere
    vector<VkAabbPositionsKHR> aabbs;
    aabbs.reserve(m_NumSpheres);
    for (const auto& s : m_Spheres)
    {
        const lwmath::vec3f minimum = s.center - lwmath::vec3f(s.radius);
        const lwmath::vec3f maximum = s.center + lwmath::vec3f(s.radius);

        VkAabbPositionsKHR aabb = { };
        aabb.minX = minimum.x;
        aabb.minY = minimum.y;
        aabb.minZ = minimum.z;
        aabb.maxX = maximum.x;
        aabb.maxY = maximum.y;
        aabb.maxZ = maximum.z;

        aabbs.push_back(aabb);
    }

    // Create the two materials (one for spheres, one for lwbes)
    MaterialObj mat;
    mat.diffuse = lwmath::vec3f(0, 1, 1);
    vector<MaterialObj> materials;
    materials.push_back(mat);
    mat.diffuse = lwmath::vec3f(1, 1, 0);
    materials.push_back(mat);

    vector<int>         matIdx;
    matIdx.reserve(m_NumSpheres);
    for (size_t i = 0; i < m_Spheres.size(); i++)
    {
        matIdx.push_back(i % 2);
    }

    UINT32 bufferSize = UNSIGNED_CAST(UINT32, max(m_Spheres.size() * sizeof(Sphere), aabbs.size() * sizeof(VkAabbPositionsKHR)));
    bufferSize = max(bufferSize, UINT32(materials.size() * sizeof(MaterialObj)));

    // Create a staging buffer in host memory to copy the data from host to device memory.
    VulkanBuffer stageBuff(m_pVulkanDev);
    CHECK_VK_TO_RC(stageBuff.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                          ,bufferSize
                                          ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

    // Create all of the required buffers need by the GPU to render the objects.
    m_SpheresBuf = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(m_SpheresBuf->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                UNSIGNED_CAST(UINT32, m_Spheres.size() * sizeof(Sphere)),
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    stageBuff.SetData(m_Spheres.data(), m_Spheres.size() * sizeof(Sphere), 0);
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get()
                                      ,&stageBuff
                                      ,m_SpheresBuf.get()));

    m_SpheresAabbBuf = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(m_SpheresAabbBuf->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  UNSIGNED_CAST(UINT32, aabbs.size() * sizeof(VkAabbPositionsKHR)),
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    stageBuff.SetData(aabbs.data(), aabbs.size() * sizeof(VkAabbPositionsKHR), 0);
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(),
                                      &stageBuff,
                                      m_SpheresAabbBuf.get()));

    m_SpheresMatColorBuf = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(m_SpheresMatColorBuf->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                UNSIGNED_CAST(UINT32, materials.size() * sizeof(MaterialObj)),
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    stageBuff.SetData(materials.data(), materials.size() * sizeof(MaterialObj), 0);
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(),
                                      &stageBuff,
                                      m_SpheresMatColorBuf.get()));

    m_SpheresMatIndexBuf  = make_unique<VulkanBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(m_SpheresMatIndexBuf->CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                UNSIGNED_CAST(UINT32, matIdx.size() * sizeof(int)),
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    stageBuff.SetData(matIdx.data(), matIdx.size() * sizeof(int), 0);
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_CmdBuffers[0].get(),
                                      &stageBuff,
                                      m_SpheresMatIndexBuf.get()));

    return rc;
}

//-------------------------------------------------------------------------------------------------
// Initialize the FB contents with all zeros so the scene will display proper when XOR'd with
// itself.
// Note we intentionally do not use the clear screen color here belwase the initial frame would
// not be correct.
RC Raytracer::InitializeFB()
{
    RC rc;
    // Clear the image with the clear scree color
    CHECK_VK_TO_RC(m_CmdBuffers[0]->BeginCmdBuffer());
    VkClearColorValue color = {{ 0.0f, 0.0f, 0.0f, 0.0f }};

    for (unsigned i = 0; i < m_SwapChain->GetNumImages(); i++)
    {
        VkImageSubresourceRange colorRange =
        {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            VK_REMAINING_MIP_LEVELS,
            0,
            VK_REMAINING_ARRAY_LAYERS
        };

        VkImageMemoryBarrier imgBarrier =
        {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            0,
            0,
            m_SwapChain->GetImage(i),
            colorRange
        };
        m_pVulkanDev->CmdPipelineBarrier(m_CmdBuffers[0]->GetCmdBuffer(),
                                         VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                                         VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
                                         0,                              // dependencyFlags
                                         0,                              // memoryBarrierCount
                                         0,                              // pMemoryBarriers
                                         0,                              // buffMemoryBarrierCount
                                         0,                              // pBufferMemoryBarriers
                                         1,                              // imageMemoryBarrierCount
                                         &imgBarrier                     // pImageMemoryBarriers
                                         );

        m_pVulkanDev->CmdClearColorImage(m_CmdBuffers[0]->GetCmdBuffer(),
                                         m_SwapChain->GetImage(i),
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         &color,
                                         1,
                                         &colorRange);
    }
    CHECK_VK_TO_RC(m_CmdBuffers[0]->EndCmdBuffer());
    CHECK_VK_TO_RC(m_CmdBuffers[0]->ExelwteCmdBuffer(true, true));

    return rc;
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::CreateBottomLevelAS()
{
    RC rc;
    vector<VulkanRaytracingBuilder::BlasInput> blas;
    blas.reserve(m_ObjModels.size());

    for (const auto& model : m_ObjModels)
    {
        auto geo = ObjectToVkGeometry(model);
        VulkanRaytracingBuilder::BlasInput blasInput;
        blasInput.geometry.push_back(geo.geometry);
        blasInput.buildOffsetInfo.push_back(geo.buildOffsetInfo);
        blas.push_back(move(blasInput));
    }

    if (m_Scene == RTS_SPHERES)
    {
        CHECK_RC(CreateSpheres());
        auto geo = SphereToVkGeometry();
        VulkanRaytracingBuilder::BlasInput blasInput;
        blasInput.geometry.push_back(geo.geometry);
        blasInput.buildOffsetInfo.push_back(geo.buildOffsetInfo);
        blas.push_back(move(blasInput));
    }

    CHECK_VK_TO_RC(m_Builder.BuildBlas(blas));
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::CreateTopLevelAS()
{
    RC rc;
    vector<VulkanRaytracingBuilder::Instance> tlas;
    tlas.reserve(m_ObjInstances.size());
    for (unsigned i = 0; i < m_ObjInstances.size(); i++)
    {
        VulkanRaytracingBuilder::Instance rayInst;
        rayInst.transform        = m_ObjInstances[i].transform;   // Position of the instance
        rayInst.instanceLwstomId = i;                             // gl_InstanceID
        rayInst.blasId           = m_ObjInstances[i].objIndex;
        rayInst.hitGroupId       = 0; // we will use the same hitgroup for all objects.
        rayInst.flags            = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_LWLL_DISABLE_BIT_KHR;
        tlas.emplace_back(rayInst);
    }
    if (m_Scene == RTS_SPHERES)
    {
        // add the blas containing the spheres
        VulkanRaytracingBuilder::Instance rayInst;
        rayInst.transform        = m_ObjInstances[0].transform;
        rayInst.instanceLwstomId = static_cast<UINT32>(tlas.size());  // gl_InstanceID
        rayInst.blasId           = static_cast<UINT32>(m_ObjModels.size());
        rayInst.hitGroupId       = 1;            // we will use the same hitgroup for all objects
        rayInst.flags            = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_LWLL_DISABLE_BIT_KHR;
        tlas.emplace_back(rayInst);
    }

    CHECK_VK_TO_RC(m_Builder.BuildTlas(tlas));

    return rc;
}

//-------------------------------------------------------------------------------------------------
// Because we use the ping-pong method of submitting draw jobs we need 2 copies of the descriptor
// sets because we can't update a descriptor set while its still in use. So sets 0-1 are associated
// with even draw jobs and sets 2-3 are associated with odd draw jobs.
// The Vulkan spec states: All submitted commands that refer to any element of pDescriptorSets must
// have completed exelwtion
//(https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-vkFreeDescriptorSets-pDescriptorSets-00309) //$
RC Raytracer::CreateDescriptorSetLayout()
{
    RC rc;
    vector<VkDescriptorPoolSize> descPoolSizes;
    // For spheres we have to include the additional Sphere object model that is not in the
    // m_ObjModels vector.
    const UINT32 numBoundObjs = static_cast<UINT32>(m_Scene == RTS_LWBE ?
                                                    m_ObjModels.size() :
                                                    m_ObjModels.size()+1);
    // bindings 0-1, set 0
    {
        vector<VkDescriptorSetLayoutBinding> bindings;
        //layout(binding = 0, set = 0) uniform accelerationStructureLW topLevelAS;
        bindings.push_back(
            {
                0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr
            });
        //layout(binding = 1, set = 0, rgba32f) uniform image2D image;
        bindings.push_back(
            {
                1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr
            });

        const UINT32 size = static_cast<UINT32>(bindings.size());
        CHECK_VK_TO_RC(m_DescriptorInfo->CreateDescriptorSetLayout(0, size, bindings.data()));
    }
    // bindings 0-7, set 1
    {
        vector<VkDescriptorSetLayoutBinding> bindings;
        // Create a descriptor layout to match what is needed by the shader resources
        // Camera matrices (binding = 0)
        bindings.push_back(
            {
                0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr
            });
        // Materials (binding = 1)
        bindings.push_back(
            {
                1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numBoundObjs,
                VK_SHADER_STAGE_VERTEX_BIT |
                VK_SHADER_STAGE_FRAGMENT_BIT |
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr
            });
        // Scene description (binding = 2)
        bindings.push_back(
            {
                2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                VK_SHADER_STAGE_VERTEX_BIT |
                VK_SHADER_STAGE_FRAGMENT_BIT |
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr
            });
        // Textures (binding = 3) TODO: Implement more textures
        //UINT32 numBoundTxt = ??;
        //bindings.push_back(
        //    {
        //        3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numBoundTxt,
        //        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr
        //    });
        // Materials Index (binding = 4)
        bindings.push_back(
            {
                4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numBoundObjs,
                VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr
            });
        // Vertices (binding = 5)
        bindings.push_back(
            {
                5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<UINT32>(m_ObjModels.size()),
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr
            });
        // Indices (binding = 6)
        bindings.push_back(
            {
                6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<UINT32>(m_ObjModels.size()),
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr
            });

        if (m_Scene == RTS_SPHERES)
        {
            // Spheres (binding = 7)
            bindings.push_back(
                {
                    7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                     VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                     VK_SHADER_STAGE_INTERSECTION_BIT_KHR, nullptr
                });
        }
        const UINT32 size = static_cast<UINT32>(bindings.size());
        CHECK_VK_TO_RC(m_DescriptorInfo->CreateDescriptorSetLayout(1, size, bindings.data()));
    }

    // Create descriptor region and set to match the shader resources.
    //
    //                       .type,          .descriptorCount
    descPoolSizes.push_back(
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,  1 }); //set 0, binding 0 = TopLevelAS
    descPoolSizes.push_back(
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 });       //set 0, binding 1 = uniform image2D image
    descPoolSizes.push_back(                            //set 1, binding 0 = uniform Camera
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1 });
    descPoolSizes.push_back(                            //binding 1 = buffer Matreials[]
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numBoundObjs });
    descPoolSizes.push_back(                            //set 1, binding 2 = Scene Desc
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1 });
    //Texture are TBD
    //descPoolSizes.push_back(                          //set 1, binding 3 = buffer Textures[]
    //    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ?? });
    descPoolSizes.push_back(                            //set 1, binding 4 = buffer MatIndices[]
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numBoundObjs });
    descPoolSizes.push_back(                            //set 1, binding 5 = buffer Vertices[]
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<UINT32>(m_ObjModels.size()) });
    descPoolSizes.push_back(                            //set 1, binding 6 = buffer Indices[]
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<UINT32>(m_ObjModels.size()) });

    if (m_Scene == RTS_SPHERES)
    {
        descPoolSizes.push_back(                        //set 1, binding 7 = buffer allSpheres[]
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 });
    }
    CHECK_VK_TO_RC(m_DescriptorInfo->CreateDescriptorPool(4, descPoolSizes));
    CHECK_VK_TO_RC(m_DescriptorInfo->AllocateDescriptorSets(0, 2, 0));
    CHECK_VK_TO_RC(m_DescriptorInfo->AllocateDescriptorSets(2, 2, 0));
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::SetupMatrices()
{
    RC rc;
    // Matrices
    // Create a Uniform buffer object (UBO) to handle the matrices.
    CHECK_VK_TO_RC(m_CameraBuf->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             sizeof(UniformBufferObject),
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    if (!m_CameraSetByJS)
    {   // The spheres scene is rather large so move the camera back to get a better view.
        if (m_Scene == RTS_SPHERES)
        {
            m_CamPos[0] =  18.0f; //.x
            m_CamPos[1] =  22.0f; //.y
            m_CamPos[2] =  22.0f; //.z
        }
        else
        {
            m_CamPos[0] =  1.0f; //.x
            m_CamPos[1] =  1.0f; //.y
            m_CamPos[2] =  1.0f; //.z
        }
    }
    glm::vec3 pos(m_CamPos[0], m_CamPos[1], m_CamPos[2]);  // camera pos
    glm::vec3 poi(0, 0, 0);           // center point of interest
    constexpr glm::vec3 up(0, -1, 0); // y-axis is the up vector, flipped for vulkan.
    // Range check and adjust if necessary the rotation parameters.
    m_Rotate[0] %= 360;
    m_Rotate[1] %= 360;
    m_Rotate[2] %= 360;
    Orbit(static_cast<float>(m_Rotate[0]), //degreesX
            static_cast<float>(m_Rotate[1]), //degreesY
            false, &pos, &poi, up);
    m_Camera.view = glm::lookAt(pos, poi, up);
    const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
    m_Camera.proj = glm::perspective(glm::radians(65.0f), aspectRatio, 0.1f, 1000.0f);
    m_Camera.viewIlwerse = glm::ilwerse(m_Camera.view);
    m_Camera.projIlwerse = glm::ilwerse(m_Camera.proj);

    // Copy the data to device memory
    char *pUBO = nullptr;
    m_CameraBuf->Map(sizeof(m_Camera), 0, (void**)&pUBO);
    memcpy(pUBO, (char*)&m_Camera, sizeof(m_Camera));
    m_CameraBuf->Unmap();

    return rc;
}

//-------------------------------------------------------------------------------------------------
// This sets the backgroud color of the scene being raytraced. Any ray that doesn't hit either the
// scene plane or any geometry will paint the pixel this color.
// Note this is not a clear screen color, that color must always be black for the XOR logic to
// work properly in raytracing.
void Raytracer::SetupPushConstants()
{
    m_PushConstants.clearColor = lwmath::vec4f(
         ((m_ClearSceneColor >> 16) & 0xff) * (1.0F/255.0F), //red
         ((m_ClearSceneColor >>  8) & 0xff) * (1.0F/255.0F), //green
         ((m_ClearSceneColor >>  0) & 0xff) * (1.0F/255.0F), //blue
         ((m_ClearSceneColor >> 24) & 0xff) * (1.0F/255.0F));  //alpha
    m_PushConstants.lightPosition = lwmath::vec3f(10.0f, 15.0f, 8.0f);
    m_PushConstants.lightIntensity = 200.0f;
    m_PushConstants.lightType = 0; // 0=point, 1:infinite
}

//-------------------------------------------------------------------------------------------------
//Update Descriptor Set 0 (for Lwbes), or Descriptor Set 1 (for Spheres)
void Raytracer::UpdateDescriptorSet(UINT32 drawJobIdx)
{
    MASSERT(drawJobIdx < 2);
    const UINT32 set0_2 = 2 * drawJobIdx + 0;
    const UINT32 set1_3 = 2 * drawJobIdx + 1;

    // Update descriptor set 0 or 2
    {   // AS Info:
        // layout(binding = 0, set = 0) uniform accelerationStructureKHR topLevelAS;
        VkAccelerationStructureKHR tlas = m_Builder.GetAccelerationStructure();
        VkWriteDescriptorSetAccelerationStructureKHR asInfo =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            nullptr,
            1,
            &tlas
        };
        VkWriteDescriptorSet writeDescriptorSet =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            &asInfo,
            m_DescriptorInfo->GetDescriptorSet(set0_2),
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };
        m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
    }
    {
        //Image output
        //layout(binding = 1, set = 0, rgba32f) uniform image2D image;
        VkDescriptorImageInfo imageInfo =
        {
            VK_NULL_HANDLE,
            m_SwapChain->GetImageView(m_LwrrentSwapChainIdx),
            VK_IMAGE_LAYOUT_GENERAL
        };
        VkWriteDescriptorSet writeDescriptorSet =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            m_DescriptorInfo->GetDescriptorSet(set0_2),
            1,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            &imageInfo
        };
        m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
    }

    //Update descriptor set 1 or 3
    {   // Camera Info
        // layout(binding = 0, set = 1) uniform CameraProperties
        VkDescriptorBufferInfo camDesc =
        {
            m_CameraBuf->GetBuffer(),
            0,
            VK_WHOLE_SIZE
        };
        VkWriteDescriptorSet writeDescriptorSet =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            m_DescriptorInfo->GetDescriptorSet(set1_3),
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            nullptr,
            &camDesc,
            nullptr
        };
        m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
    }
    {   // Scene Info
        // layout(binding = 2, set = 1, scalar) buffer ScnDesc { sceneDesc i[]; } scnDesc;
        VkDescriptorBufferInfo sceneDesc =
        {
            m_SceneBuf->GetBuffer(),
            0,
            VK_WHOLE_SIZE
        };
        VkWriteDescriptorSet writeDescriptorSet =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            m_DescriptorInfo->GetDescriptorSet(set1_3),
            2,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr,
            &sceneDesc,
            nullptr
        };
        m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
    }

    vector<VkDescriptorBufferInfo> dbiMat;
    vector<VkDescriptorBufferInfo> dbiMatIdx;
    vector<VkDescriptorBufferInfo> dbiVert;
    vector<VkDescriptorBufferInfo> dbiVertIdx;
    for (auto& model : m_ObjModels)
    {
        dbiMat.push_back({ model.matColorBuffer->GetBuffer(), 0, VK_WHOLE_SIZE });
        dbiMatIdx.push_back({ model.matIndexBuffer->GetBuffer(), 0, VK_WHOLE_SIZE });
        dbiVert.push_back({ model.vertexBuffer->GetBuffer(), 0, VK_WHOLE_SIZE });
        dbiVertIdx.push_back({ model.indexBuffer->GetBuffer(), 0, VK_WHOLE_SIZE });
    }
    if (m_Scene == RTS_SPHERES)
    {
        dbiMat.push_back({ m_SpheresMatColorBuf->GetBuffer(), 0, VK_WHOLE_SIZE });
        dbiMatIdx.push_back({ m_SpheresMatIndexBuf->GetBuffer(), 0, VK_WHOLE_SIZE });
    }

    {   // Materials Info
        // layout(binding = 1, set = 1, scalar) buffer MatColorBufferObject
        VkWriteDescriptorSet writeDescriptorSet =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            m_DescriptorInfo->GetDescriptorSet(set1_3),
            1,
            0,
            static_cast<uint32_t>(dbiMat.size()),
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr,
            dbiMat.data(),
            nullptr
        };
        m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
    }
    {   // Material Index Info
        // layout(binding = 4, set = 1)  buffer MatIndexColorBuffer { int i[]; } matIndex[];
        VkWriteDescriptorSet writeDescriptorSet =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            m_DescriptorInfo->GetDescriptorSet(set1_3),
            4,
            0,
            static_cast<uint32_t>(dbiMatIdx.size()),
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr,
            dbiMatIdx.data(),
            nullptr
        };
        m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
    }
    {   // Vertex Info,
        // layout(binding = 5, set = 1, scalar) buffer Vertices { Vertex v[]; } vertices[];
        VkWriteDescriptorSet writeDescriptorSet =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            m_DescriptorInfo->GetDescriptorSet(set1_3),
            5,
            0,
            static_cast<uint32_t>(dbiVert.size()),
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr,
            dbiVert.data(),
            nullptr
        };
        m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
    }
    {   // Index info,
        // layout(binding = 6, set = 1) buffer Indices { uint i[]; } indices[];
        VkWriteDescriptorSet writeDescriptorSet =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            m_DescriptorInfo->GetDescriptorSet(set1_3),
            6,
            0,
            static_cast<uint32_t>(dbiVertIdx.size()),
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr,
            dbiVertIdx.data(),
            nullptr
        };
        m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
    }

    if (m_Scene == RTS_SPHERES)
    {   //Spheres data,
        //layout(binding = 7, set = 1, scalar) buffer allSpheres_
        VkDescriptorBufferInfo spheresDesc
        {
            m_SpheresBuf->GetBuffer(),
            0,
            VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet writeDescriptorSet =
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            m_DescriptorInfo->GetDescriptorSet(set1_3),
            7,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr,
            &spheresDesc,
            nullptr
        };
        m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
    }

    /* TODO: Implement textures for raytracing.
        {   // Textures,
            // layout(binding = 3, set = 1) uniform sampler2D textureSamplers[];
            VkDescriptorImageInfo textureInfo =
            {
                VK_NULL_HANDLE,
                VK_NULL_HANDLE,
                VK_IMAGE_LAYOUT_GENERAL
            };
            VkWriteDescriptorSet writeDescriptorSet =
            {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                nullptr,
                m_DescriptorInfo->GetDescriptorSet(set1_3),
                3,
                0,
                1,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                TBD
            };
            m_DescriptorInfo->UpdateDescriptorSets(&writeDescriptorSet, 1);
        }
    */

}

void Raytracer::CmdTraceRays(VulkanCmdBuffer* pCmdBuf)
{
    const VkDeviceAddress sbtAddr = m_SbtBuffer->GetBufferDeviceAddress();

    VkStridedDeviceAddressRegionKHR raygenRegion =
    {
        sbtAddr,
        m_ShaderGroupHandleSize,
        m_ShaderGroupHandleSize
    };

    VkStridedDeviceAddressRegionKHR missRegion =
    {
        sbtAddr + 1 * m_ShaderGroupHandleSize,
        m_ShaderGroupHandleSize,
        2 * m_ShaderGroupHandleSize
    };

    VkStridedDeviceAddressRegionKHR hitRegion =
    {
        sbtAddr + 3 * m_ShaderGroupHandleSize,
        m_ShaderGroupHandleSize,
        m_ShaderGroupHandleSize
    };

    VkStridedDeviceAddressRegionKHR callableRegion = { };

    m_pVulkanDev->CmdTraceRaysKHR(pCmdBuf->GetCmdBuffer(), // command buffer
                                  &raygenRegion,           // pRaygenShaderBindingTable
                                  &missRegion,             // pMissShaderBindingTable
                                  &hitRegion,              // pHitShaderBindingTable
                                  &callableRegion,         // pCallableShaderBindingTable
                                  m_Width,                 // width
                                  m_Height,                // height
                                  1);                      // depth
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::AddFramesToCmdBuffer
(
    VulkanCmdBuffer* pCmdBuf,
    UINT32 numFramesRendered,
    UINT32 drawJobIdx
)
{
    RC rc;
    auto& lwrrFb = m_FrameBuffers[m_LwrrentSwapChainIdx];
    // Rotate the image on every draw job to prevent the same memory locations from getting the
    // same two values.
    if (!(numFramesRendered % 2) && m_Rotate[2])
    {
        glm::vec3 poi(0, 0, 0); // point of interest
        glm::vec3 up(0, -1, 0); // flip the Y-axis for vulkan
        glm::vec3 pos = glm::vec3(m_CamPos[0], m_CamPos[1], m_CamPos[2]);
        m_Rotate[0] = (m_Rotate[0] + m_Rotate[2]) % 360;
        if (m_Debug & VkUtil::DBG_PRINT_ROTATION)
        {
            Printf(Tee::PriNormal, "Rotate %d degreesX\n", m_Rotate[0]);
        }
        Orbit(static_cast<float>(m_Rotate[0]), 0.0f, false, &pos, &poi, up);
        m_Camera.view = glm::lookAt(pos, poi, up);
        m_Camera.viewIlwerse = glm::ilwerse(m_Camera.view);
        // copy the data inline with a memory barrier
        CHECK_VK_TO_RC(VkUtil::UpdateBuffer(pCmdBuf,
                                            m_CameraBuf.get(),
                                            0,
                                            sizeof(m_Camera),
                                            &m_Camera,
                                            VK_ACCESS_TRANSFER_WRITE_BIT,
                                            VK_ACCESS_SHADER_READ_BIT,
                                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                            VkUtil::SEND_BARRIER
                                            ));
    }
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    {
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = m_RenderPass->GetRenderPass();
        renderPassBeginInfo.framebuffer = lwrrFb->GetVkFrameBuffer();
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = lwrrFb->GetWidth();
        renderPassBeginInfo.renderArea.extent.height = lwrrFb->GetHeight();
        renderPassBeginInfo.clearValueCount = 0;
        renderPassBeginInfo.pClearValues = nullptr;
    };

    m_pVulkanDev->CmdBeginRenderPass(pCmdBuf->GetCmdBuffer(),
                                     &renderPassBeginInfo,
                                     VK_SUBPASS_CONTENTS_INLINE);

    // Begin/end the RenderPass to apply the rays to imageView
    m_pVulkanDev->CmdBindPipeline(pCmdBuf->GetCmdBuffer(),
                                  VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                  m_Pipeline->GetPipeline());

    VkDescriptorSet descSet[2] = {};
    descSet[0] = m_DescriptorInfo->GetDescriptorSet(2 * drawJobIdx + 0); //bindings 0-1 set 0 or 2
    descSet[1] = m_DescriptorInfo->GetDescriptorSet(2 * drawJobIdx + 1); //bindings 0-7 set 1 or 3

    m_pVulkanDev->CmdBindDescriptorSets(pCmdBuf->GetCmdBuffer(),
                                        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, // bind point
                                        m_Pipeline->GetPipelineLayout(),    // layout
                                        0,                                    // first set
                                        2,                                    // set count
                                        descSet,                              // pDescriptorsSets
                                        0,                                    // dynamicOffsetCount
                                        nullptr);                             // pDynamicOffsets

    m_pVulkanDev->CmdPushConstants(pCmdBuf->GetCmdBuffer(),
                                   m_Pipeline->GetPipelineLayout(),
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                   VK_SHADER_STAGE_MISS_BIT_KHR,
                                   0,   //index into the push constant buffer
                                   sizeof(m_PushConstants),
                                   &m_PushConstants);
    m_pVulkanDev->CmdEndRenderPass(pCmdBuf->GetCmdBuffer());

    if (!m_UseComputeQueue)
    {
        for (UINT32 frame = 0; frame < m_FramesPerSubmit; frame++)
        {
            CmdTraceRays(pCmdBuf);

            // add memory barrier to prevent TraceRaysLw from overlapping in the machine.
            VkMemoryBarrier barrier
            {
                VK_STRUCTURE_TYPE_MEMORY_BARRIER
            };
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            m_pVulkanDev->CmdPipelineBarrier(pCmdBuf->GetCmdBuffer(),
                                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                             0, 1, &barrier, 0, nullptr, 0, nullptr);
        }
    }
    return (rc);
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::AddRtToCmdBuffer
(
    VulkanCmdBuffer* pCmdBuf,
    UINT32 numFramesRendered,
    UINT32 drawJobIdx
)
{
    RC rc;
    m_pVulkanDev->CmdBindPipeline(pCmdBuf->GetCmdBuffer(),
                                  VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                  m_Pipeline->GetPipeline());


    VkDescriptorSet descSet[2] = {};
    descSet[0] = m_DescriptorInfo->GetDescriptorSet(2 * drawJobIdx + 0); //bindings 0-1 set 0 or 2
    descSet[1] = m_DescriptorInfo->GetDescriptorSet(2 * drawJobIdx + 1);  //bindings 0-7 set 1 or 3

    m_pVulkanDev->CmdBindDescriptorSets(pCmdBuf->GetCmdBuffer(),
                                        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, // bind point
                                        m_Pipeline->GetPipelineLayout(),    // layout
                                        0,                                    // first set
                                        2,                                    // set count
                                        descSet,                              // pDescriptorsSets
                                        0,                                    // dynamicOffsetCount
                                        nullptr);                             // pDynamicOffsets

    m_pVulkanDev->CmdPushConstants(pCmdBuf->GetCmdBuffer(),
                                   m_Pipeline->GetPipelineLayout(),
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                   VK_SHADER_STAGE_MISS_BIT_KHR,
                                   0,   //index into the push constant buffer
                                   sizeof(m_PushConstants),
                                   &m_PushConstants);

    for (UINT32 frame = 0; frame < m_FramesPerSubmit; frame++)
    {
        CmdTraceRays(pCmdBuf);

        // add memory barrier to prevent TraceRaysLw from overlapping in the machine.
        VkMemoryBarrier barrier
        {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER
        };
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        m_pVulkanDev->CmdPipelineBarrier(pCmdBuf->GetCmdBuffer(),
                                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                         0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
// When building the draw job we have to put all of the RenderPass operations in a graphics queue
// and then we can optionally put the raytracing calls in a compute queue. However we need to
// add synchronization objects to insure the render pass happens before the raytracing ops.
// Note: The preferred method is to put the raytracing ops in a compute queue because the context
//       switches are more lightweight than having them in the graphics queue.
//
RC Raytracer::BuildDrawJob(UINT32 drawJobIdx, UINT32 queryIdx, UINT32 numFramesRendered)
{
    MASSERT(drawJobIdx < 2);
    RC rc;

    UpdateDescriptorSet(drawJobIdx);

    if (m_UseComputeQueue)
    {
        // Note: With query objects the spec states: "Timestamps may only be meaningfully compared
        //       if they are written by commands submitted to the same queue."
        //       So we may need to create another set of query object for the compute queue and
        //       merge the results in DumpQueryResults().
        // Graphics Operations
        MASSERT(drawJobIdx < m_CmdBuffers.size());
        auto& cmdBuf = m_CmdBuffers[drawJobIdx];
        CHECK_VK_TO_RC(cmdBuf->ResetCmdBuffer());
        CHECK_VK_TO_RC(cmdBuf->BeginCmdBuffer());
        m_TSQueries[queryIdx]->CmdReset(cmdBuf.get(), 0, 1);
        m_TSQueries[queryIdx]->CmdWriteTimestamp(cmdBuf.get(),
                                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                 0);
        // Do the required render pass, but no raytracing cmds.
        CHECK_RC(AddFramesToCmdBuffer(cmdBuf.get(), numFramesRendered, drawJobIdx));

        CHECK_VK_TO_RC(m_TSQueries[queryIdx]->CmdCopyResults(cmdBuf.get(), 0, 1,
                                             VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        CHECK_VK_TO_RC(cmdBuf->EndCmdBuffer());

        //Compute operations
        auto& computeBuf = m_ComputeCmdBuffers[drawJobIdx];
        CHECK_VK_TO_RC(computeBuf->ResetCmdBuffer());
        CHECK_VK_TO_RC(computeBuf->BeginCmdBuffer());
        m_TSQueries[queryIdx]->CmdReset(computeBuf.get(), 1, 1);

        // add the required number of raytracing calls.
        CHECK_RC(AddRtToCmdBuffer(computeBuf.get(), numFramesRendered, drawJobIdx));

        m_TSQueries[queryIdx]->CmdWriteTimestamp(computeBuf.get(),
                                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                 1);
        CHECK_VK_TO_RC(m_TSQueries[queryIdx]->CmdCopyResults(computeBuf.get(), 1, 1,
                                             VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        CHECK_VK_TO_RC(computeBuf->EndCmdBuffer());
    }
    else
    {   //Everything goes into the graphics queue
        MASSERT(drawJobIdx < m_CmdBuffers.size());
        auto& cmdBuf = m_CmdBuffers[drawJobIdx];
        CHECK_VK_TO_RC(cmdBuf->ResetCmdBuffer());
        CHECK_VK_TO_RC(cmdBuf->BeginCmdBuffer());
        m_TSQueries[queryIdx]->CmdReset(cmdBuf.get(), 0, 2);
        m_TSQueries[queryIdx]->CmdWriteTimestamp(cmdBuf.get(),
                                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                 0);
        CHECK_RC(AddFramesToCmdBuffer(cmdBuf.get(), numFramesRendered, drawJobIdx));
        m_TSQueries[queryIdx]->CmdWriteTimestamp(cmdBuf.get(),
                                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                 1);
        CHECK_VK_TO_RC(m_TSQueries[queryIdx]->CmdCopyResults(cmdBuf.get(), 0, 2,
                                             VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        CHECK_VK_TO_RC(cmdBuf->EndCmdBuffer());
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
Raytracer::ObjectInfo Raytracer::SphereToVkGeometry()
{
    ObjectInfo obj = { };

    VkAccelerationStructureGeometryKHR& geometry = obj.geometry;
    geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureGeometryAabbsDataKHR& aabbs = geometry.geometry.aabbs;
    aabbs.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    aabbs.data.deviceAddress = m_SpheresAabbBuf->GetBufferDeviceAddress();
    aabbs.stride             = sizeof(VkAabbPositionsKHR);

    VkAccelerationStructureBuildRangeInfoKHR& buildOffsetInfo = obj.buildOffsetInfo;
    buildOffsetInfo.primitiveCount = static_cast<UINT32>(m_Spheres.size());

    return obj;
}

//-------------------------------------------------------------------------------------------------
RC Raytracer::DumpImage
(
    int frame,
    VulkanGoldenSurfaces *pGS,
    bool bUseTimestamp,
    string filepath
)
{
    RC rc;
    pGS->Ilwalidate();
    UINT32 *pSrc = static_cast<UINT32*>(pGS->GetCachedAddress(
                     0                      //int surfNum,
                    ,Goldelwalues::opCpuDma //not used
                    ,0                      // not used
                    ,nullptr));             //vector<UINT08> *surfDumpBuffer

    string::size_type pos = filepath.find_last_of("/\\");
    if ((pos != string::npos) && (pos != (filepath.length()-1)))
    {
        filepath.push_back(filepath[pos]); // append the proper type of separator
    }

    string filename = Utility::StrPrintf("VkRt%s_orbit_%dx_%dy_frm%d",
                                         m_Scene == RTS_LWBE ? "Lwbe" : "Spheres",
                                         m_Rotate[0], m_Rotate[1], frame);
    if (bUseTimestamp)
    {
        filename = Utility::GetTimestampedFilename(filename.c_str(), "png");
    }
    else
    {
        filename += ".png";
    }
    filename.insert(0, filepath);

    CHECK_RC(ImageFile::WritePng(filename.c_str()
        ,VkUtil::ColorUtilsFormat(m_SwapChain->GetSwapChainImage(0)->GetFormat())
        ,pSrc
        ,pGS->Width(0)
        ,pGS->Height(0)
        ,pGS->Pitch(0)
        ,false      // AlphaToRgb
        ,false));   // YIlwerted
    return rc;
}

//-------------------------------------------------------------------------------------------------
void Raytracer::SetJSObject(JSObject * obj)
{
    m_FpCtx.pJSObj = obj;
}

//-------------------------------------------------------------------------------------------------
JSObject * Raytracer::GetJSObject()
{
    MASSERT(m_FpCtx.pJSObj);
    return m_FpCtx.pJSObj;
}

//-------------------------------------------------------------------------------------------------
// JavaScript VulkanRaytracer object and properties.

#if !defined(VULKAN_STANDALONE)
JSClass Raytracer_class =
{
    "Raytracer",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    JS_FinalizeStub
};

SObject Raytracer_Object
(
    "Raytracer",
    Raytracer_class,
    0,
    0,
    "Raytracing JS Object"
);

//-------------------------------------------------------------------------------------------------
// Class properties that are settable from JS.
CLASS_PROP_READWRITE(Raytracer, ClearSceneColor,            UINT32,
    "Clear color values(RGBA8) to initialize the screen images with before testing");
//Tuning params
CLASS_PROP_READWRITE(Raytracer, DisableXORLogic,            bool,
    "Diable XOR logic in the raygen shader");
CLASS_PROP_READWRITE(Raytracer, FramesPerSubmit,            UINT32,
    "Ray tracing frames per submit");
CLASS_PROP_READWRITE(Raytracer, NumSpheres,                 UINT32,
    "Number of spheres to render when RtScene=Spheres");
CLASS_PROP_READWRITE(Raytracer, Scene,                      UINT32,
    "Ray tracing scene to render (0=Lwbe, 1=Spheres");
CLASS_PROP_READWRITE(Raytracer, ShaderReplacement,          bool,
    "Shader replacement");
CLASS_PROP_READWRITE(Raytracer, UseComputeQueue,            bool,
    "Use compute queues when generating raytracing ops(default = true)");

template<class T> RC Raytracer::InitJsArray(const char * jsPropName, T *pData, bool * pSetByJs)
{
    RC rc;
    JavaScriptPtr pJs;
    jsval jsvData;
    MASSERT(jsPropName && pData);

    rc = pJs->GetPropertyJsval(GetJSObject(), jsPropName, &jsvData);
    if (RC::OK == rc)
    {
        if (JSVAL_IS_OBJECT(jsvData))
        {
            JsArray jsaData;
            CHECK_RC(pJs->FromJsval(jsvData, &jsaData));
            pData->clear();
            for (size_t i = 0; i < jsaData.size(); ++i)
            {
                typename T::value_type data;
                CHECK_RC(pJs->FromJsval(jsaData[i], &data));
                pData->push_back(data);
            }
            if (pSetByJs)
            {
                *pSetByJs = true;
            }
        }
        else
        {
            Printf(Tee::PriError, "Raytracer: Invalid JS specification of %s", jsPropName);
            return RC::ILWALID_ARGUMENT;
        }
    }
    else if (rc == RC::ILWALID_OBJECT_PROPERTY)
    {
        rc.Clear(); // ignore and go with defaults
    }
    return rc;
}

template RC Raytracer::InitJsArray<vector<float>>
(
    const char *,
    vector<float> *,
    bool * pSetByJs
);
template RC Raytracer::InitJsArray<vector<UINT32>>
(
    const char *,
    vector<UINT32> *,
    bool * pSetByJs
);

//------------------------------------------------------------------------------------------------
// Initialize the special "fake" properties from JS.
RC Raytracer::InitFromJs()
{
    // MODS doesn't support initializing JSArrays via the command line using the
    // CLASS_PROP_READWRITE_JSARRAY macro so we have to create a "fake" property and load it
    // manually here. This implementation supports setting the Rotate & CamPos properties directly
    // from the command line, the spec files, or the vkstress.js file.
    RC rc;
    //    "Set geometry rotation[x, y, inc] in degrees 0-360.");
    CHECK_RC(InitJsArray("Rotate", &m_Rotate, nullptr));
    if (m_Rotate.size() != 3)
    {
        Printf(Tee::PriError,
            "Raytracer : Invalid number of entries for Raytrace.Rotate, expected:3, actual:%u\n",
            static_cast<UINT32>(m_Rotate.size()));
        return RC::ILWALID_ARGUMENT;
    }

    //    "Set raytracing camera position[x, y, z]in world coordinates (-20.0...20.0).");
    CHECK_RC(InitJsArray("CamPos", &m_CamPos, &m_CameraSetByJS));
    if (m_CamPos.size() != 3)
    {
        Printf(Tee::PriError,
            "Raytracer : Invalid number of entries for CamPos, expected:3, actual:%u\n",
            static_cast<UINT32>(m_CamPos.size()));
        return RC::ILWALID_ARGUMENT;
    }
    return rc;
}
#endif
