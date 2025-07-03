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

#pragma once

#include "vulkan/vkasgen.h"

//-------------------------------------------------------------------------------------------------
// This class handles all of the raytracing functionality for VkStress.
//(see https://lwpro-samples.github.io/vk_raytracing_tutorial/vkrt_tuto_intersection.md.html)
// For how raytracing is implemented in VkStress. The example above is closely followed and the
// raytracing shaders are pulled from that example.
class Raytracer
{
public:
    enum SceneType
    {
        RTS_LWBE    = 0,    //Generate a single lwbe
        RTS_SPHERES = 1     //Generate a scene with 50% spheres & 50% lwbes
    };
    struct Vertex
    {
        FLOAT32 pos[3];     // { .xyz }
        FLOAT32 norm[3];    // { .xyz }
        FLOAT32 col[3];     // { .rgb }
        FLOAT32 texcd[2];   // { .st }
    };

    Raytracer(VulkanDevice* pVulkanDev);
    ~Raytracer();
    void                            AddDebugMessagesToIgnore();
    RC                              AddExtensions(vector<string>& extNames);
    RC                              BuildDrawJob
                                    (
                                        UINT32 drawJobIdx,
                                        UINT32 queryIdx,
                                        UINT32 numFramesRendered
                                    );
    void                            CleanupResources();
    RC                              DumpImage
                                    (
                                        int frame,
                                        VulkanGoldenSurfaces *pGS,
                                        bool bUseTimestamp,
                                        string filepath
                                    );
    VkFence                         GetFence(UINT32 idx);
    JSObject *                      GetJSObject();
    SwapChain *                     GetSwapChain();
    UINT64                          GetTSResults(UINT32 lwrIdx, UINT32 rsltIdx);
    RC                              InitFromJs();
    void                            PrintJsProperties(Tee::Priority pri);
    RC                              SendWorkToGpu(UINT64 drawJobIdx);
    void                            SetJSObject(JSObject * obj);
    RC                              SetupRaytracing
                                    (
                                        vector<unique_ptr<VulkanGoldenSurfaces>> &rGoldenSurfaces,
                                        VulkanCmdBuffer * pCheckSurfCmdBuf,
                                        VulkanCmdBuffer * pCheckSurfTransBuf
                                    );
    void                            SetVulkanDevice(VulkanDevice* pVulkanDev);
    // Properties we all to be settable by JS.
    SETGET_PROP(ClearSceneColor,    UINT32);    // background color for rays that don't hit any
                                                // geometry.
    SETGET_PROP(DisableXORLogic,    bool);      //debug: disables XOR logic in raygen shader.
    SETGET_PROP(FramesPerSubmit,    UINT32);    // number of times the scene is traced per drawJob
    SETGET_PROP(NumSpheres,         UINT32);    // number of spheres/lwbes to generate
    SETGET_PROP(Scene,              UINT32);
    SETGET_PROP(UseComputeQueue,    bool);
    // SetGet methods not settable from JS.
    SETGET_PROP(LwrrentSwapChainIdx, UINT32);
    SETGET_PROP(Debug,              UINT32);
    GET_PROP(GoldenSurfaceIdx,      UINT32);
    SETGET_PROP(Height,             UINT32);
    SETGET_PROP(NumFrames,          UINT32);
    SETGET_PROP(NumTsQueries,       UINT32);
    SETGET_PROP(SwapChainMode,      UINT32);
    SETGET_PROP(Width,              UINT32);
    SETGET_PROP(ComputeQueueIdx,    UINT32);
    SETGET_PROP(GraphicsQueueIdx,   UINT32);
    // used to seed the random generator for spheres
    SETGET_PROP(Seed,               UINT32);
    SETGET_PROP(ZeroFb,             bool);
    SETGET_PROP(ShaderReplacement,  bool);

private:
    struct ObjInstance
    {
        INT32           objIndex = 0;
        INT32           txtOffset = 0;           // offset in the m_Textures
        lwmath::mat4f   transform = { 1 };      // position of the instance
        lwmath::mat4f   transformIT = { 1 };    // ilwerse transpose of transform
    };

    // Note: this was pulled from obj_loader.h
    // (see https://lwpro-samples.github.io/vk_raytracing_tutorial/vkrt_tuto_intersection.md.html)
    // In the future we may want to bring in the entire tiny object loader into VkStress and
    // utilize more of the standard shading models.
    // Structure holding the material
    struct MaterialObj
    {
        lwmath::vec3f   ambient         = lwmath::vec3f(0.1f, 0.1f, 0.1f);
        lwmath::vec3f   diffuse         = lwmath::vec3f(0.7f, 0.7f, 0.7f);
        lwmath::vec3f   spelwlar        = lwmath::vec3f(1.0f, 1.0f, 1.0f);
        lwmath::vec3f   transmittance   = lwmath::vec3f(0.0f, 0.0f, 0.0f);
        lwmath::vec3f   emission        = lwmath::vec3f(0.0f, 0.0f, 0.10);
        float           shininess       = 0.f;
        float           ior             = 1.0f;  // index of refraction
        float           dissolve        = 1.f;   // 1 == opaque; 0 == fully transparent
        // illumination model (see http://www.fileformat.info/format/material/)
        int             illum           = 0; // Color on and Ambient off
        int             textureID       = -1;
    };
    struct ObjectModel
    {
        UINT32                      numIndices      = 0;
        UINT32                      numVertices     = 0;
        unique_ptr<VulkanBuffer>    vertexBuffer    = nullptr;
        unique_ptr<VulkanBuffer>    indexBuffer     = nullptr;
        unique_ptr<VulkanBuffer>    matColorBuffer  = nullptr;
        unique_ptr<VulkanBuffer>    matIndexBuffer  = nullptr;
    };
    struct PushConstant
    {
        lwmath::vec4f   clearColor;
        lwmath::vec3f   lightPosition;
        float           lightIntensity;
        int             lightType;
    };
    struct Sphere
    {
        lwmath::vec3f   center;
        float           radius;
    };
    struct UniformBufferObject
    {
        glm::mat4    view;
        glm::mat4    proj;
        glm::mat4    viewIlwerse;
        glm::mat4    projIlwerse;
    };
    struct ObjectInfo
    {
        VkAccelerationStructureGeometryKHR       geometry;
        VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo;
    };
    template<class T> RC InitJsArray(const char * jsPropName, T *pValues, bool *pSetByJS);

    VulkanRaytracingBuilder                 m_Builder;
    UniformBufferObject                     m_Camera                = {};
    unique_ptr<VulkanBuffer>                m_CameraBuf             = nullptr;
    vector<float>                           m_CamPos                = { 18.0, 22.0, 22.0 };
    bool                                    m_CameraSetByJS         = false;
    UINT32                                  m_ClearSceneColor       = 0xFF0000FF; //ARGB
    unique_ptr<VulkanCmdPool>               m_CmdPool               = nullptr;
    unique_ptr<VulkanCmdPool>               m_ComputeCmdPool        = nullptr;
    UINT32                                  m_ComputeQueueIdx       = 0;
    vector<unique_ptr<VulkanCmdBuffer>>     m_CmdBuffers;
    vector<unique_ptr<VulkanCmdBuffer>>     m_ComputeCmdBuffers;
    UINT32                                  m_LwrrentSwapChainIdx   = 0;
    UINT32                                  m_Debug                 = 0;
    unique_ptr<DescriptorInfo>              m_DescriptorInfo        = nullptr;
    bool                                    m_DisableXORLogic       = false;
    vector<unique_ptr<VulkanFrameBuffer>>   m_FrameBuffers;
    vector<VkFence>                         m_Fences;
    UINT32                                  m_FramesPerSubmit       = 40;
    FancyPicker::FpContext                  m_FpCtx;
    UINT32                                  m_GoldenSurfaceIdx      = 0;
    UINT32                                  m_GraphicsQueueIdx      = 0;
    UINT32                                  m_Height                = 768;
    const UINT64                            m_NumDrawJobs           = 2;
    UINT32                                  m_NumFrames             = 0;
    UINT32                                  m_NumSpheres            = 15000;
    UINT32                                  m_NumTsQueries          = 8;
    vector<ObjInstance>                     m_ObjInstances;
    vector<ObjectModel>                     m_ObjModels;
    unique_ptr<VulkanPipeline>              m_Pipeline              = nullptr;
    string                                  m_ProgAnyHit;
    string                                  m_ProgClosestHit;
    string                                  m_ProgClosestHit2;
    string                                  m_ProgIntersection;
    string                                  m_ProgMiss;
    string                                  m_ProgRayGen;
    string                                  m_ProgShadowMiss;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_Properties = { };
    UINT32                                  m_ShaderGroupHandleSize = 0;
    PushConstant                            m_PushConstants;
    VulkanInstance*                         m_pVulkanInst           = nullptr;
    VulkanDevice*                           m_pVulkanDev            = nullptr;
    unique_ptr<VulkanRenderPass>            m_RenderPass            = nullptr;
    vector<UINT32>                          m_Rotate                = { 0, 0, 5 };
    vector<VkSemaphore>                     m_RpSems;   // semaphore to sync the RenderPass in the
                                                        // graphics queue with the LwTraceRays in
                                                        // the compute queue.
    unique_ptr<VulkanBuffer>                m_SbtBuffer             = nullptr;
    UINT32                                  m_Scene                 = RTS_SPHERES;
    unique_ptr<VulkanBuffer>                m_SceneBuf              = nullptr;
    UINT32                                  m_Seed                  = 0x12345678;
    vector<VkSemaphore>                     m_Sems;
    // Raytracing Shaders
    unique_ptr<VulkanShader>                m_ShaderAnyHit          = nullptr;
    unique_ptr<VulkanShader>                m_ShaderClosestHit      = nullptr;
    unique_ptr<VulkanShader>                m_ShaderClosestHit2     = nullptr;
    unique_ptr<VulkanShader>                m_ShaderIntersection    = nullptr;
    unique_ptr<VulkanShader>                m_ShaderMiss            = nullptr;
    unique_ptr<VulkanShader>                m_ShaderRayGen          = nullptr;
    unique_ptr<VulkanShader>                m_ShaderShadowMiss      = nullptr;
    vector<Sphere>                          m_Spheres;
    unique_ptr<VulkanBuffer>                m_SpheresBuf            = nullptr;
    unique_ptr<VulkanBuffer>                m_SpheresAabbBuf        = nullptr;
    unique_ptr<VulkanBuffer>                m_SpheresMatColorBuf    = nullptr;
    unique_ptr<VulkanBuffer>                m_SpheresMatIndexBuf    = nullptr;
    UINT32                                  m_SwapChainMode         = SwapChain::SINGLE_IMAGE_MODE;
    vector<unique_ptr<VulkanQuery>>         m_TSQueries;
    bool                                    m_UseComputeQueue       = true;
    UINT32                                  m_Width                 = 1024;
    bool                                    m_ZeroFb                = false;
    unique_ptr<SwapChainMods>               m_SwapChain             = nullptr;
    bool                                    m_ShaderReplacement     = false;
    RC                                      AddFramesToCmdBuffer
                                            (
                                                VulkanCmdBuffer* pCmdBuf,
                                                UINT32 numFramesRendered,
                                                UINT32 drawJobIdx
                                            );
    RC                                      AddRtToCmdBuffer
                                            (
                                                VulkanCmdBuffer* pCmdBuf,
                                                UINT32 numFramesRendered,
                                                UINT32 drawJobIdx
                                            );
    void                                    AllocateResources();
    void                                    CleanupAfterSetup() const;
    RC                                      CreateBottomLevelAS();
    RC                                      CreateDescriptorSetLayout();
    RC                                      CreateSceneDescriptionBuffer();
    RC                                      CreateSpheres();
    RC                                      CreateTopLevelAS();
    RC                                      GenLwbeModel();
    const char*                             GetSceneStr();
    RC                                      InitializeFB();
    RC                                      LoadModel(UINT32 scene);
    ObjectInfo                              ObjectToVkGeometry(const ObjectModel& model);
    void                                    Orbit
                                            (
                                                float dx,
                                                float dy,
                                                bool ilwert,
                                                glm::vec3 *pos,
                                                glm::vec3 *poi,
                                                const glm::vec3 &up
                                            );
    void                                    SetupAnyHitProgram();
    void                                    SetupClosestHitProgram();
    void                                    SetupClosestHit2Program();
    void                                    SetupIntersectionProgram();
    RC                                      SetupMatrices();
    void                                    SetupMissProgram();
    void                                    SetupPushConstants();
    void                                    SetupRayGenProgram();
    RC                                      SetupShaders();
    void                                    SetupShadowMissProgram();
    ObjectInfo                              SphereToVkGeometry();
    void                                    UpdateDescriptorSet(UINT32 drawJobIdx);
    void                                    CmdTraceRays(VulkanCmdBuffer* pCmdBuf);
};
