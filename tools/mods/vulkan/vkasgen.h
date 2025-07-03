/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "vkmain.h"
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
#include "vkquery.h"
#include "vkasgen.h"
#include "vkutil.h"
#include "util.h" //GLMatrix
#include "core/include/fpicker.h"
#include "vkgoldensurfaces.h"
#include "vkdescriptor.h"

#if !defined(VULKAN_STANDALONE)
#include "core/include/setget.h"
#endif

#include "vulkan/shared_sources/lwmath/lwmath.h"

#include "vulkan/shared_sources/lwmath/lwmath.h"
#include "gtc/matrix_ilwerse.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtx/hash.hpp"
#include "gtx/rotate_vector.hpp"
#include "glm.hpp"

struct AccelerationStorage
{
    VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
    VulkanBuffer               buffer;

    AccelerationStorage() = default;
    AccelerationStorage(const AccelerationStorage&) = delete;
    AccelerationStorage& operator=(const AccelerationStorage&) = delete;
    AccelerationStorage(AccelerationStorage&& other) { TakeResources(move(other)); }
    AccelerationStorage& operator=(AccelerationStorage&& other) { TakeResources(move(other)); return *this; }

private:
    void TakeResources(AccelerationStorage&& other);
};

static_assert(sizeof(VkAccelerationStructureInstanceKHR) == 64,
            "VkAccelerationStructureInstanceKHR structure compiles to incorrect size");

// This is a helper class to build the TLAS & BLAS used in raytracing.
class VulkanRaytracingBuilder
{
public:
    // This is an instance of a Bottom Level AS
    struct Instance
    {
        UINT32        blasId             = 0;    // Index of the BLAS in m_blas
        UINT32        instanceLwstomId   = 0;    // Instance Index (gl_InstanceLwstomIndexEXT)
        UINT32        hitGroupId         = 0;    // Hit group index in the SBT
        UINT32        mask               = 0xFF; // Visibility mask, will be AND-ed with ray mask
        VkGeometryInstanceFlagsKHR flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_LWLL_DISABLE_BIT_KHR;
        lwmath::mat4f transform          = lwmath::mat4f(1); // Identity
    };

    VulkanRaytracingBuilder(VulkanRaytracingBuilder const&) = delete;
    VulkanRaytracingBuilder& operator=(VulkanRaytracingBuilder const &) = delete;
    VulkanRaytracingBuilder() = default;
    ~VulkanRaytracingBuilder();
    void Cleanup();
    void Setup(VulkanDevice* pVulkanDev, UINT32 queueIdx, UINT32 familyIdx)
    {
        m_pVulkanDev = pVulkanDev;
        m_QueueIndex = queueIdx;
        m_FamilyIndex = familyIdx;
    }

    // Return the constructed top-level acceleration structure
    const VkAccelerationStructureKHR& GetAccelerationStructure() const
    {
        return m_Tlas.as.accel;
    }

    struct BlasInput
    {
        vector<VkAccelerationStructureGeometryKHR>       geometry;
        vector<VkAccelerationStructureBuildRangeInfoKHR> buildOffsetInfo;
    };

    //---------------------------------------------------------------------------------------------
    // Create all the BLAS from the vector of object
    // - There will be one BLAS per object, i.e. per element of the input vector
    // - There will be the same number of BLAS as there are items in the toplevel objects vector
    // - Eacho object has multiple geometries.  Each geometry has multiple primitives.
    // - The resulting BLASes are stored in m_Blas
    //
    VkResult BuildBlas
    (
        const std::vector<BlasInput>&        objects,
        VkBuildAccelerationStructureFlagsKHR flags =
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
    );

    VkResult BuildTlas
    (
        const std::vector<Instance>&         instances,
        VkBuildAccelerationStructureFlagsKHR flags =
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
    );

private:
    VulkanDevice *  m_pVulkanDev = nullptr;
    UINT32          m_QueueIndex = 0;
    UINT32          m_FamilyIndex = 0;

    // Bottom Level AS
    struct Blas
    {
        AccelerationStorage                         as;
        VkAccelerationStructureBuildGeometryInfoKHR asInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    };

    // Top-level acceleration structure
    struct Tlas
    {
        AccelerationStorage                         as;
        VkAccelerationStructureBuildGeometryInfoKHR asInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    };

    //---------------------------------------------------------------------------------------------
    // Vector containing all the BLASes built and referenced by the TLAS
    std::vector<Blas> m_Blas;
    // Top-level acceleration structure
    Tlas m_Tlas;
    // Instance buffer containing the matrices and BLAS ids
    VulkanBuffer m_InstBuffer;

    VkResult CreateAcceleration
    (
        VkAccelerationStructureCreateInfoKHR& accel,
        AccelerationStorage*                  pResultAccel
    );

    VkAccelerationStructureInstanceKHR InstanceToVkGeometryInstance(const Instance& instance);
};
