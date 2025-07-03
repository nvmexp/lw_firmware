/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "vkbuffer.h"
#include "vkdev.h"

//--------------------------------------------------------------------
VulkanBuffer::VulkanBuffer(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
{
}

//--------------------------------------------------------------------
VulkanBuffer::VulkanBuffer(VulkanBuffer&& vulkBuf) noexcept
{
    TakeResources(move(vulkBuf));
}

//--------------------------------------------------------------------
VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& vulkBuf)
{
    TakeResources(move(vulkBuf));
    return *this;
}

//--------------------------------------------------------------------
VulkanBuffer::~VulkanBuffer()
{
    Cleanup();
}

//--------------------------------------------------------------------
// Create the buffer object and the backing memory store for it.
//--------------------------------------------------------------------
VkResult VulkanBuffer::CreateBuffer
(
    VkBufferUsageFlags    usage,
    UINT64                bufferSizeBytes,
    VkMemoryPropertyFlags memoryPropertyFlags,
    VkSharingMode         sharingMode,
    UINT32                queueFamilyCount,
    UINT32*               pQueueFamilyIndices
)
{
    VkResult res = VK_SUCCESS;
    CHECK_VK_RESULT(CreateBuffer(usage,
                                 bufferSizeBytes,
                                 sharingMode,
                                 queueFamilyCount,
                                 pQueueFamilyIndices));

    const VkMemoryAllocateFlags memoryAllocateFlags =
        (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0;
    return AllocateAndBindMemory(memoryPropertyFlags, memoryAllocateFlags);
}

//--------------------------------------------------------------------
VkResult VulkanBuffer::CreateBuffer
(
    VkBufferUsageFlags usageFlag,
    UINT64             bufferSizeBytes,
    VkSharingMode      sharingMode,
    UINT32             queueFamilyCount,
    UINT32*            pQueueFamilyIndices
)
{
    MASSERT(m_Buffer == VK_NULL_HANDLE);

    MASSERT(bufferSizeBytes > 0 && usageFlag != 0);
    if (sharingMode == VK_SHARING_MODE_CONLWRRENT)
    {
        MASSERT(queueFamilyCount > 1 && pQueueFamilyIndices);
    }
    m_Size = bufferSizeBytes;
    m_Usage = usageFlag;

    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.size                  = bufferSizeBytes;
    bufferCreateInfo.usage                 = usageFlag;
    bufferCreateInfo.sharingMode           = sharingMode;
    bufferCreateInfo.queueFamilyIndexCount = queueFamilyCount;
    bufferCreateInfo.pQueueFamilyIndices   = pQueueFamilyIndices;

    return m_pVulkanDev->CreateBuffer(&bufferCreateInfo, nullptr, &m_Buffer);
}

//--------------------------------------------------------------------
VkDescriptorBufferInfo VulkanBuffer::GetBufferInfo() const
{
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = m_Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = m_Size;
    return bufferInfo;
}

//--------------------------------------------------------------------
VkDeviceAddress VulkanBuffer::GetBufferDeviceAddress() const
{
    VkBufferDeviceAddressInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    bufferInfo.buffer = m_Buffer;

    return m_pVulkanDev->GetBufferDeviceAddress(&bufferInfo);
}

//--------------------------------------------------------------------
VkResult VulkanBuffer::AllocateAndBindMemory(VkMemoryPropertyFlags memoryPropertyFlags,
                                             VkMemoryAllocateFlags memoryAllocateFlags)
{
    VkResult res = VK_SUCCESS;
    VkMemoryRequirements memoryRequirements;
    m_pVulkanDev->GetBufferMemoryRequirements(m_Buffer, &memoryRequirements);

    // check if this Buffer can be allocated where it is requested
    VkMemoryAllocateInfo memAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memAllocInfo.memoryTypeIndex = m_pVulkanDev->GetPhysicalDevice()->GetMemoryTypeIndex(
        memoryRequirements, memoryPropertyFlags);
    if (memAllocInfo.memoryTypeIndex == UINT32_MAX)
    {
        return VK_ERROR_VALIDATION_FAILED_EXT;
    }

    VkMemoryAllocateFlagsInfo flags = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
    if (memoryAllocateFlags)
    {
        flags.flags = memoryAllocateFlags;
        memAllocInfo.pNext = &flags;
    }

    memAllocInfo.allocationSize = memoryRequirements.size;
    CHECK_VK_RESULT(m_pVulkanDev->AllocateMemory(&memAllocInfo, NULL, &m_DeviceMemory));
    res = m_pVulkanDev->BindBufferMemory(m_Buffer, m_DeviceMemory, 0);
    if (res != VK_SUCCESS)
    {
        m_pVulkanDev->FreeMemory(m_DeviceMemory, nullptr);
        m_DeviceMemory = VK_NULL_HANDLE;
        return res;
    }
    return res;
}

bool VulkanBuffer::IsMapped() const
{
    return m_Mapped;
}

VkResult VulkanBuffer::Map(VkDeviceSize size, VkDeviceSize offset, void **pData)
{
    //Note: Device local memory can't be mapped (there are no pools that support both
    //      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT and VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT. There
    //      are two ways to update device local memory.
    //      1. Create a staging buffer on the host and execute a vkCmdCopyBuffer to copy from the
    //         host buffer to a device buffer.
    //      2. Use the vkCmdUpdateBuffer() API to do an inline copy from host memory to device
    //         buffer.
    //      see https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
    if (m_Mapped)
    {
        return VK_ERROR_TOO_MANY_OBJECTS;
    }
    *pData = nullptr;
    VkResult res =  m_pVulkanDev->MapMemory(m_DeviceMemory, offset, size, 0, pData);
    if (!*pData)
    {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    m_Mapped = true;
    return res;
}

VkResult VulkanBuffer::FlushMappedRange(VkDeviceSize offset, VkDeviceSize size)
{
    VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };

    range.memory = m_DeviceMemory;
    range.offset = offset;
    range.size   = size;

    return m_pVulkanDev->FlushMappedMemoryRanges(1, &range);
}

//-------------------------------------------------------------------------------------------------
// Unmap host visible memory. See notes in Map() above
VkResult VulkanBuffer::Unmap()
{
    if (!m_Mapped)
        return VK_ERROR_LAYER_NOT_PRESENT;

    m_pVulkanDev->UnmapMemory(m_DeviceMemory);
    m_Mapped = false;
    return VK_SUCCESS;
}

//--------------------------------------------------------------------
VkResult VulkanBuffer::SetData(const void *data, VkDeviceSize size, VkDeviceSize offset) const
{
    MASSERT(m_DeviceMemory != VK_NULL_HANDLE);
    void *mappedData = nullptr;
    VkResult res = m_pVulkanDev->MapMemory(m_DeviceMemory, offset, size, 0, &mappedData);
    if (!mappedData)
    {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    memcpy(mappedData, data, UNSIGNED_CAST(size_t, size));
    m_pVulkanDev->UnmapMemory(m_DeviceMemory);
    return res;
}

//--------------------------------------------------------------------
void VulkanBuffer::Cleanup()
{
    if (!m_pVulkanDev)
    {
        MASSERT(m_Buffer == VK_NULL_HANDLE);
        MASSERT(m_DeviceMemory == VK_NULL_HANDLE);
        return;
    }
    if (m_Mapped)
    {
        VkResult res = Unmap();
        if (res != VK_SUCCESS)
        {
            MASSERT(!"Unmap() failed");
        }
    }
    if (m_Buffer)
    {
        m_pVulkanDev->DestroyBuffer(m_Buffer);
        m_Buffer = VK_NULL_HANDLE;
    }
    if (m_DeviceMemory)
    {
        m_pVulkanDev->FreeMemory(m_DeviceMemory);
        m_DeviceMemory = VK_NULL_HANDLE;
    }
    m_pVulkanDev = nullptr;
}

//--------------------------------------------------------------------
void VulkanBuffer::TakeResources(VulkanBuffer&& vulkBuf)
{
    if (this != &vulkBuf)
    {
        Cleanup();

        m_pVulkanDev = vulkBuf.m_pVulkanDev;
        m_DeviceMemory = vulkBuf.GetDeviceMemory();
        m_Buffer = vulkBuf.GetBuffer();

        vulkBuf.m_pVulkanDev = nullptr;
        vulkBuf.m_DeviceMemory = VK_NULL_HANDLE;
        vulkBuf.m_Buffer = VK_NULL_HANDLE;

        m_Usage = vulkBuf.m_Usage;
        m_Size = vulkBuf.m_Size;
        m_Mapped = vulkBuf.m_Mapped;
    }
}

//--------------------------------------------------------------------
VkResult VulkanBufferViewBase::Map(VkDeviceSize size, VkDeviceSize offset)
{
    MASSERT(!m_Data);

    if (m_Data)
    {
        Printf(Tee::PriError, "VkBuffer already mapped!\n");
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    MASSERT(offset < m_Buffer.GetSize());

    if (size == NoSize)
    {
        size = m_Buffer.GetSize() - offset;
    }

    MASSERT(size <= m_Buffer.GetSize());
    MASSERT(offset + size <= m_Buffer.GetSize());

    const VkResult res = m_Buffer.Map(size, offset, &m_Data);

    if (res == VK_SUCCESS)
    {
        m_Offset = offset;
        m_Size   = size;
    }

    return res;
}

//--------------------------------------------------------------------
void VulkanBufferViewBase::Unmap()
{
    if (m_Data)
    {
        m_Buffer.Unmap();
        m_Data   = nullptr;
        m_Offset = 0;
        m_Size   = 0;
    }
}

//--------------------------------------------------------------------
VkResult VulkanBufferViewBase::Flush()
{
    return m_Buffer.FlushMappedRange(m_Offset);
}
