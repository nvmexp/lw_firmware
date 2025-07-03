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
#if !defined(VULKAN_STANDALONE)
#include "core/include/script.h"
#endif

#include "core/include/utility.h"

#include "vkasgen.h"
#include "vkutil.h"
#include "core/include/imagefil.h"

using namespace VkUtil;

//-------------------------------------------------------------------------------------------------
VulkanRaytracingBuilder::~VulkanRaytracingBuilder()
{
    Cleanup();
}

//-------------------------------------------------------------------------------------------------
void VulkanRaytracingBuilder::Cleanup()
{
    if (m_pVulkanDev)
    {
        for (const auto& blas : m_Blas)
        {
            if (blas.as.accel)
            {
                m_pVulkanDev->DestroyAccelerationStructureKHR(blas.as.accel, nullptr);
            }
        }
        m_Blas.clear();

        if (m_Tlas.as.accel)
        {
            m_pVulkanDev->DestroyAccelerationStructureKHR(m_Tlas.as.accel, nullptr);
            m_Tlas.as.accel = VK_NULL_HANDLE;
        }
        m_Tlas.as.buffer.Cleanup();

        m_InstBuffer.Cleanup();
        m_pVulkanDev = nullptr;
    }
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanRaytracingBuilder::CreateAcceleration
(
    VkAccelerationStructureCreateInfoKHR& accel,
    AccelerationStorage*                  pResultAccel
)
{
    VkResult res = VK_SUCCESS;

    pResultAccel->buffer = VulkanBuffer(m_pVulkanDev);
    CHECK_VK_RESULT(pResultAccel->buffer.CreateBuffer(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                      UNSIGNED_CAST(UINT32, accel.size),
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    accel.buffer = pResultAccel->buffer.GetBuffer();

    CHECK_VK_RESULT(m_pVulkanDev->CreateAccelerationStructureKHR(&accel,
                                                                 nullptr,
                                                                 &pResultAccel->accel));

    return VK_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanRaytracingBuilder::BuildBlas
(
    const std::vector<BlasInput>&        objects,
    VkBuildAccelerationStructureFlagsKHR flags
)
{
    const size_t numObjects = objects.size();

    MASSERT(m_Blas.empty());
    m_Blas.resize(numObjects);

    VkResult res = VK_SUCCESS;

    VkDeviceSize maxScratch = 0;

    // Is compaction requested?
    const bool doCompaction = !!(flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
    std::vector<VkDeviceSize> originalSizes;
    originalSizes.resize(numObjects);

    // Iterate over the groups of geometries, creating one BLAS for each group
    vector<UINT32> maxPrimCount;
    for (size_t i = 0; i < numObjects; i++)
    {
        MASSERT(objects[i].geometry.size() == objects[i].buildOffsetInfo.size());

        Blas& blas = m_Blas[i];
        const auto& geometry        = objects[i].geometry;
        const auto& buildOffsetInfo = objects[i].buildOffsetInfo;

        const size_t numGeoms = geometry.size();

        // Set the geometries that will be part of the BLAS
        MASSERT(blas.asInfo.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR);
        blas.asInfo.geometryCount = static_cast<uint32_t>(numGeoms);
        blas.asInfo.pGeometries   = geometry.data();
        blas.asInfo.flags         = flags;
        blas.asInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        blas.asInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        blas.asInfo.srcAccelerationStructure = VK_NULL_HANDLE;

        // Query the size of the finished acceleration structure and the amount of
        // scratch memory needed
        maxPrimCount.resize(numGeoms);
        for (UINT32 j = 0; j < numGeoms; j++)
        {
            maxPrimCount[j] = buildOffsetInfo[j].primitiveCount;
        }
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        m_pVulkanDev->GetAccelerationStructureBuildSizesKHR(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                            &blas.asInfo,
                                                            maxPrimCount.data(),
                                                            &sizeInfo);

        // Create an acceleration structure identifier and allocate memory to store the
        // resulting structure data
        VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        CHECK_VK_RESULT(CreateAcceleration(createInfo, &blas.as));

        blas.asInfo.flags = flags;

        originalSizes[i] = sizeInfo.accelerationStructureSize;

        maxScratch = std::max(maxScratch, sizeInfo.buildScratchSize);
    }
    maxPrimCount = vector<UINT32>();

    VulkanBuffer scratchBuffer(m_pVulkanDev);

    // Allocate the scratch buffers holding the temporary data of the AS builder
    CHECK_VK_RESULT(scratchBuffer.CreateBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                               UNSIGNED_CAST(UINT32, maxScratch),
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    const VkDeviceAddress scratchAddress = scratchBuffer.GetBufferDeviceAddress();

    // Query size of compact BLAS
    VkQueryPoolCreateInfo qpci = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    qpci.queryCount = static_cast<UINT32>(numObjects);
    qpci.queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;

    VkQueryPool queryPool = { };
    CHECK_VK_RESULT(m_pVulkanDev->CreateQueryPool(&qpci, nullptr, &queryPool));

    // Create a command buffer containing all the BLAS builds
    // TODO provide command buffer as a parameter, instead of managing it here!
    VulkanCmdPool cmdPool(m_pVulkanDev);
    CHECK_VK_RESULT(cmdPool.InitCmdPool(m_FamilyIndex, m_QueueIndex));

    VulkanCmdBuffer cmdBuf(m_pVulkanDev);

    CHECK_VK_RESULT(cmdBuf.AllocateCmdBuffer(&cmdPool));
    CHECK_VK_RESULT(cmdBuf.BeginCmdBuffer());
    vector<const VkAccelerationStructureBuildRangeInfoKHR*> rangePtrs;
    for (uint32_t i = 0; i < numObjects; i++)
    {
        Blas& blas = m_Blas[i];
        const auto& buildOffsetInfo = objects[i].buildOffsetInfo;

        blas.asInfo.scratchData.deviceAddress = scratchAddress;
        blas.asInfo.dstAccelerationStructure  = blas.as.accel;

        rangePtrs.resize(buildOffsetInfo.size());
        for (UINT32 j = 0; j < rangePtrs.size(); j++)
        {
            rangePtrs[j] = &buildOffsetInfo[j];
        }

        m_pVulkanDev->CmdBuildAccelerationStructuresKHR(cmdBuf.GetCmdBuffer(),
                                                        1,
                                                        &blas.asInfo,
                                                        rangePtrs.data());

        // Since the scratch buffer is reused across builds, we need a barrier to ensure one build
        // is finished before starting the next one
        VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

        m_pVulkanDev->CmdPipelineBarrier(cmdBuf.GetCmdBuffer(),
                                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                         0, 1, &barrier, 0, nullptr, 0, nullptr);

        // Query the compact size
        if (doCompaction)
        {
            m_pVulkanDev->CmdWriteAccelerationStructuresPropertiesKHR(
                cmdBuf.GetCmdBuffer(), 1, &blas.as.accel,
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, i);
        }
    }
    rangePtrs.clear();
    CHECK_VK_RESULT(cmdBuf.EndCmdBuffer());
    CHECK_VK_RESULT(cmdBuf.ExelwteCmdBuffer(WAIT_ON_FENCE, RESET_AFTER_EXEC));

    // Compacting all BLAS
    if (doCompaction)
    {
        // Get the size result back
        std::vector<VkDeviceSize> compactSizes(numObjects);
        m_pVulkanDev->GetQueryPoolResults(queryPool, 0, (uint32_t)numObjects,
                                          numObjects * sizeof(VkDeviceSize),
                                          compactSizes.data(), sizeof(VkDeviceSize),
                                          VK_QUERY_RESULT_WAIT_BIT);

        // Compacting
        std::vector<AccelerationStorage> cleanupAS(numObjects);
        uint32_t totOriginalSize = 0;
        uint32_t totCompactSize  = 0;
        CHECK_VK_RESULT(cmdBuf.BeginCmdBuffer());
        for (unsigned i = 0; i < numObjects; i++)
        {
            Printf(Tee::PriLow, "Reducing %i, from %ld to %ld \n",
                   i, originalSizes[i], compactSizes[i]);
            totOriginalSize += (uint32_t)originalSizes[i];
            totCompactSize  += (uint32_t)compactSizes[i];

            // Creating a compact version of the AS
            VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
            createInfo.size = compactSizes[i];
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

            AccelerationStorage compactedAS;
            CHECK_VK_RESULT(CreateAcceleration(createInfo, &compactedAS));

            // Copy the original BLAS to a compact version
            VkCopyAccelerationStructureInfoKHR copyInfo = { VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
            copyInfo.src  = m_Blas[i].as.accel;
            copyInfo.dst  = compactedAS.accel;
            copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;

            m_pVulkanDev->CmdCopyAccelerationStructureKHR(cmdBuf.GetCmdBuffer(), &copyInfo);

            cleanupAS[i] = move(m_Blas[i].as);
            m_Blas[i].as = move(compactedAS);
        }
        CHECK_VK_RESULT(cmdBuf.EndCmdBuffer());
        CHECK_VK_RESULT(cmdBuf.ExelwteCmdBuffer(WAIT_ON_FENCE, RESET_AFTER_EXEC));

        // Destroying the previous version
        for (const auto& as : cleanupAS)
        {
            m_pVulkanDev->DestroyAccelerationStructureKHR(as.accel, 0);
        }
        cleanupAS.clear();

        Printf(Tee::PriLow, "------------------\n");
        Printf(Tee::PriLow, "Total: %d -> %d = %d (%2.2f%s smaller) \n",
               totOriginalSize, totCompactSize, totOriginalSize - totCompactSize,
               (totOriginalSize - totCompactSize) / float(totOriginalSize) * 100.f, "%%");
    }

    m_pVulkanDev->DestroyQueryPool(queryPool, nullptr);
    return res;
}

//-------------------------------------------------------------------------------------------------
// Colwert an Instance object into a VkAccelerationStructureInstanceKHR struct.
VkAccelerationStructureInstanceKHR VulkanRaytracingBuilder::InstanceToVkGeometryInstance
(
    const Instance& instance
)
{
    MASSERT(instance.blasId < m_Blas.size());
    Blas& blas = m_Blas[instance.blasId];

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    addressInfo.accelerationStructure = blas.as.accel;

    const VkDeviceAddress blasAddress = m_pVulkanDev->GetAccelerationStructureDeviceAddressKHR(&addressInfo);

    VkAccelerationStructureInstanceKHR geoInst = { };
    // The matrices for the instance transforms are row-major, instead of
    // column-major in the rest of the application
    lwmath::mat4f transp = lwmath::transpose(instance.transform);
    // The geoInst.transform value only contains 12 values, corresponding to a 4x3
    // matrix, hence saving the last row that is anyway always (0,0,0,1). Since
    // the matrix is row-major, we simply copy the first 12 values of the
    // original 4x4 matrix
    memcpy(&geoInst.transform, &transp, sizeof(geoInst.transform));
    geoInst.instanceLwstomIndex                    = instance.instanceLwstomId;
    geoInst.mask                                   = instance.mask;
    geoInst.instanceShaderBindingTableRecordOffset = instance.hitGroupId;
    geoInst.flags                                  = instance.flags;
    geoInst.accelerationStructureReference         = blasAddress;
    return geoInst;
}

//-------------------------------------------------------------------------------------------------
// Creating the top-level acceleration structure from the vector of Instance
// - See struct of Instance
// - The resulting TLAS will be stored in m_Tlas
VkResult VulkanRaytracingBuilder::BuildTlas
(
    const std::vector<Instance>&         instances,
    VkBuildAccelerationStructureFlagsKHR flags
)
{
    MASSERT(m_Tlas.as.accel == VK_NULL_HANDLE);

    VkResult res = VK_SUCCESS;

    vector<VkAccelerationStructureInstanceKHR> geoInstances;
    geoInstances.reserve(instances.size());
    for (const auto& inst : instances)
    {
        geoInstances.push_back(InstanceToVkGeometryInstance(inst));
    }

    // Create buffer to hold instance data
    VulkanBuffer xferBuffer(m_pVulkanDev);
    CHECK_VK_RESULT(xferBuffer.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            UNSIGNED_CAST(UINT32, geoInstances.size() * sizeof(VkAccelerationStructureInstanceKHR)),
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_RESULT(xferBuffer.SetData(geoInstances.data(),
                                       geoInstances.size() * sizeof(VkAccelerationStructureInstanceKHR),
                                       0));
    m_InstBuffer = VulkanBuffer(m_pVulkanDev);
    CHECK_VK_RESULT(m_InstBuffer.CreateBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              UNSIGNED_CAST(UINT32, geoInstances.size() * sizeof(VkAccelerationStructureInstanceKHR)),
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    // Determine how much memory is needed
    VkAccelerationStructureGeometryInstancesDataKHR instData = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
    instData.arrayOfPointers    = VK_FALSE;
    instData.data.deviceAddress = m_InstBuffer.GetBufferDeviceAddress();

    VkAccelerationStructureGeometryKHR topASGeometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    topASGeometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topASGeometry.geometry.instances = instData;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR} ;
    buildInfo.flags         = flags;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &topASGeometry;
    buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    UINT32 count = static_cast<UINT32>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    m_pVulkanDev->GetAccelerationStructureBuildSizesKHR(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                        &buildInfo,
                                                        &count,
                                                        &sizeInfo);

    // Create the AS object and allocate the memory required to hold the TLAS data
    VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    CHECK_VK_RESULT(CreateAcceleration(createInfo, &m_Tlas.as));

    // Allocate the scratch buffer holding the temporary data of the AS builder
    VulkanBuffer scratchBuffer(m_pVulkanDev);

    CHECK_VK_RESULT(scratchBuffer.CreateBuffer(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                               UNSIGNED_CAST(UINT32, sizeInfo.buildScratchSize),
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    // Update build info
    buildInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure  = m_Tlas.as.accel;
    buildInfo.scratchData.deviceAddress = scratchBuffer.GetBufferDeviceAddress();

    // Create a command buffer containing all the TLAS builds
    VulkanCmdPool cmdPool(m_pVulkanDev);
    CHECK_VK_RESULT(cmdPool.InitCmdPool(m_FamilyIndex, m_QueueIndex));
    VulkanCmdBuffer cmdBuf(m_pVulkanDev);
    CHECK_VK_RESULT(cmdBuf.AllocateCmdBuffer(&cmdPool));

    CHECK_VK_RESULT(cmdBuf.BeginCmdBuffer());
    CHECK_VK_RESULT(VkUtil::CopyBuffer(&cmdBuf,
                                       &xferBuffer,
                                       &m_InstBuffer,
                                       VK_ACCESS_TRANSFER_WRITE_BIT,
                                       VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_LW));

    // Build the TLAS
    VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo = { };
    buildOffsetInfo.primitiveCount = static_cast<UINT32>(instances.size());
    VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

    m_pVulkanDev->CmdBuildAccelerationStructuresKHR(cmdBuf.GetCmdBuffer(),
                                                    1,
                                                    &buildInfo,
                                                    &pBuildOffsetInfo);

    CHECK_VK_RESULT(cmdBuf.EndCmdBuffer());
    CHECK_VK_RESULT(cmdBuf.ExelwteCmdBuffer(WAIT_ON_FENCE, RESET_AFTER_EXEC));

    return res;
}

void AccelerationStorage::TakeResources(AccelerationStorage&& other)
{
    if (this != &other)
    {
        accel = other.accel;
        other.accel = VK_NULL_HANDLE;

        buffer = move(other.buffer);
    }
}
