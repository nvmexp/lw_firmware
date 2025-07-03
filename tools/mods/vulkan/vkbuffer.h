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

#pragma once
#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include "vkmain.h"

class VulkanDevice;

//--------------------------------------------------------------------
class VBBindingAttributeDesc
{
public:
    VBBindingAttributeDesc() = default;
    void Reset()
    {
        m_BindingDesc.clear();
        m_AttributesDesc.clear();
    }

    void AddBindingDescription(VkVertexInputBindingDescription bindingDesc)
    {
        m_BindingDesc.push_back(bindingDesc);
    }
    void AddAttributeDescription(VkVertexInputAttributeDescription attDesc)
    {
        m_AttributesDesc.push_back(attDesc);
    }
    vector<VkVertexInputBindingDescription>   m_BindingDesc;
    vector<VkVertexInputAttributeDescription> m_AttributesDesc;
};

//--------------------------------------------------------------------
class VulkanBuffer
{
public:

    VulkanBuffer() = default;
    explicit VulkanBuffer(VulkanDevice* pVulkanDev);
    VulkanBuffer(VulkanBuffer&& vulkBuf) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& vulkBuf);
    VulkanBuffer(const VulkanBuffer& vulkBuf) = delete;
    VulkanBuffer& operator=(const VulkanBuffer& vulkBuf) = delete;
    ~VulkanBuffer();

    VkResult                AllocateAndBindMemory(VkMemoryPropertyFlags memoryPropertyFlags,
                                                  VkMemoryAllocateFlags memoryAllocateFlags = 0);
    void                    Cleanup();
    // Create the buffer object and the backing memory store.
    VkResult                CreateBuffer(VkBufferUsageFlags    usage,
                                         UINT64                bufferSizeBytes,
                                         VkMemoryPropertyFlags memoryPropertyFlags,
                                         VkSharingMode         sharingMode         = VK_SHARING_MODE_EXCLUSIVE,
                                         UINT32                queueFamilyCount    = 0,
                                         UINT32*               pQueueFamilyIndices = nullptr);
    // Create the buffer object
    VkResult                CreateBuffer(VkBufferUsageFlags usage,
                                         UINT64             bufferSizeBytes,
                                         VkSharingMode      sharingMode         = VK_SHARING_MODE_EXCLUSIVE,
                                         UINT32             queueFamilyCount    = 0,
                                         UINT32*            pQueueFamilyIndices = nullptr);

    VkDescriptorBufferInfo  GetBufferInfo() const;
    VkDeviceAddress         GetBufferDeviceAddress() const;
    VkDeviceMemory          GetDeviceMemory() { return m_DeviceMemory; }
    VkResult                Map(VkDeviceSize size, VkDeviceSize offset, void **pData);
    bool                    IsMapped() const;
    VkResult                SetData
                            (
                                const void *data
                                ,VkDeviceSize size
                                ,VkDeviceSize offset
                            ) const;
    VkResult                FlushMappedRange(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    VkResult                Unmap();

    GET_PROP(Usage, VkBufferUsageFlags);
    GET_PROP(Size, UINT64);
    GET_PROP(DeviceMemory, VkDeviceMemory);
    GET_PROP(Buffer, VkBuffer);

protected:
    void SetDeviceMemory(VkDeviceMemory devMem) { m_DeviceMemory = devMem; }
    void SetBuffer(VkBuffer vkBuf) { m_Buffer = vkBuf; }

private:
    VulkanDevice           *m_pVulkanDev = nullptr;

    VkDeviceMemory          m_DeviceMemory = VK_NULL_HANDLE;
    VkBuffer                m_Buffer = VK_NULL_HANDLE;

    VkBufferUsageFlags      m_Usage = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    UINT64                  m_Size = 0;
    bool                    m_Mapped = false;

    void                   TakeResources(VulkanBuffer&& vulkBuf);
};

class VulkanBufferViewBase
{
    public:
        VulkanBufferViewBase() = delete;
        ~VulkanBufferViewBase() { Unmap(); }

        VulkanBufferViewBase(const VulkanBufferViewBase&) = delete;
        VulkanBufferViewBase& operator=(const VulkanBufferViewBase&) = delete;

        static constexpr VkDeviceSize NoSize = ~static_cast<VkDeviceSize>(0);

        VkResult Map(VkDeviceSize size = NoSize, VkDeviceSize offset = 0);
        void Unmap();

        VkResult Flush();

    protected:
        VulkanBufferViewBase(VulkanBuffer& buffer)
            : m_Buffer(buffer) { }
        void*  GetData() const { return m_Data; }
        size_t GetSize() const { return m_Size; }

    private:
        VulkanBuffer& m_Buffer;
        void*         m_Data   = nullptr;
        VkDeviceSize  m_Offset = 0;
        size_t        m_Size   = 0;
};

template<typename T>
class VulkanBufferView: public VulkanBufferViewBase
{
    public:
        VulkanBufferView() = delete;
        VulkanBufferView(VulkanBuffer& buffer)
            : VulkanBufferViewBase(buffer) { }

        VulkanBufferView(const VulkanBufferView&) = delete;
        VulkanBufferView& operator=(const VulkanBufferView&) = delete;

        using iterator = T*;
        using const_iterator = const T*;

        iterator data() { return static_cast<T*>(GetData()); }
        const_iterator data() const { return static_cast<const T*>(GetData()); }
        size_t size() const { return GetSize() / sizeof(T); }

        iterator begin()              { return data(); }
        iterator end()                { return data() + size(); }
        const_iterator begin() const  { return data(); }
        const_iterator end()   const  { return data() + size(); }
        const_iterator cbegin() const { return data(); }
        const_iterator cend()   const { return data() + size(); }
};

#endif
