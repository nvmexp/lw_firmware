/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "vkquery.h"
#include "vkdev.h"
#include "vkcmdbuffer.h"
#include "vkbuffer.h"
#include "core/include/coreutility.h"
//-------------------------------------------------------------------------------------------------
VulkanQueryPool::VulkanQueryPool(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
{
    MASSERT(m_pVulkanDev);
}

//-------------------------------------------------------------------------------------------------
VulkanQueryPool::~VulkanQueryPool()
{
    DestroyQueryPool();
}

//-------------------------------------------------------------------------------------------------
VulkanQueryPool::VulkanQueryPool(VulkanQueryPool&& other) noexcept
{
    TakeResources(move(other));
}

//-------------------------------------------------------------------------------------------------
VulkanQueryPool& VulkanQueryPool::operator=(VulkanQueryPool&& other)
{
    TakeResources(move(other));
    return *this;
}

//--------------------------------------------------------------------
void VulkanQueryPool::TakeResources(VulkanQueryPool&& other)
{
    if (this != &other)
    {
        DestroyQueryPool();

        m_pVulkanDev = other.m_pVulkanDev;
        m_QueryPool  = other.m_QueryPool;

        other.m_pVulkanDev = nullptr;
        other.m_QueryPool  = VK_NULL_HANDLE;
    }
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanQueryPool::InitQueryPool
(
    VkQueryType queryType   // Occlusion, pipeline stats, or timestamp
    ,UINT32     queryCount  // number of queries managed by the pool.
    ,VkQueryPipelineStatisticFlags pipelineStats  // bitmask of the type of counters returned.
)
{
    MASSERT(m_QueryPool == VK_NULL_HANDLE);

    VkQueryPoolCreateInfo poolCreateInfo =
    {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO
        ,nullptr        //pNext
        ,0              //flags (reserved for future use)
        ,queryType
        ,queryCount
        ,pipelineStats
    };

    return m_pVulkanDev->CreateQueryPool(&poolCreateInfo, nullptr, &m_QueryPool);
}

//--------------------------------------------------------------------
void VulkanQueryPool::DestroyQueryPool()
{
    if (m_QueryPool != VK_NULL_HANDLE)
    {
        m_pVulkanDev->DestroyQueryPool(m_QueryPool);
        m_QueryPool = VK_NULL_HANDLE;
    }
}

//-------------------------------------------------------------------------------------------------
// VulkanQuery implementation
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
VulkanQuery::VulkanQuery(VulkanDevice *pVulkanDev):
    m_pVulkanDev(pVulkanDev)
{
    MASSERT(m_pVulkanDev);
}

//-------------------------------------------------------------------------------------------------
VulkanQuery::~VulkanQuery()
{
    Cleanup();
}

//-------------------------------------------------------------------------------------------------
// Initialize all of the internal resources to manage all types of queries.
VkResult VulkanQuery::Init(VkQueryType queryType
                           ,UINT32 queryCount
                           ,VkQueryPipelineStatisticFlags pipelineStats)
{
    MASSERT(m_QueryPool.GetQueryPool() == VK_NULL_HANDLE);

    VkResult res = VK_SUCCESS;
    m_QueryPool = VulkanQueryPool(m_pVulkanDev);
    CHECK_VK_RESULT(m_QueryPool.InitQueryPool(queryType, queryCount, pipelineStats));

    // Allocate a buffer large enough to hold either a 32 or 64 bit elements.
    m_QueryType = queryType;
    UINT32 sizeInBytes = 0;
    switch (queryType)
    {
        case VK_QUERY_TYPE_OCCLUSION:
        case VK_QUERY_TYPE_TIMESTAMP:
            sizeInBytes = sizeof(UINT64) * queryCount;
            m_BitsEnabled = 1;
            break;

        //each bit set represents a query result.
        case VK_QUERY_TYPE_PIPELINE_STATISTICS:
            m_BitsEnabled = Utility::CountBits(static_cast<UINT64>(pipelineStats));
            sizeInBytes = sizeof(UINT64) * queryCount * m_BitsEnabled;
            break;

        default:
            MASSERT(!"Unknown queryType");
            return VK_INCOMPLETE;
    }

    m_Results = VulkanBuffer(m_pVulkanDev);
    CHECK_VK_RESULT(m_Results.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeInBytes));
    CHECK_VK_RESULT(m_Results.AllocateAndBindMemory(
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    CHECK_VK_RESULT(m_Results.Map(sizeInBytes, 0, &m_pResults));

    UINT64 * pData = reinterpret_cast<UINT64*>(m_pResults);
    for (UINT32 i = 0; i < sizeInBytes / sizeof(UINT64); i++)
    {
        pData[i] = 0x00;
    }

    return res;
}

//-------------------------------------------------------------------
// Reset the query objects for use. After allocating space in the QueryPool you need to reset
// all of the query objects before they can be used.
VkResult VulkanQuery::CmdReset
(
    VulkanCmdBuffer * pCmdBuffer
    ,UINT32 firstQuery
    ,UINT32 queryCount)
{
    VkResult res = VK_SUCCESS;
    const bool isCmdBufferActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }
    m_pVulkanDev->CmdResetQueryPool(pCmdBuffer->GetCmdBuffer(),
                                    m_QueryPool.GetQueryPool(),
                                    firstQuery,
                                    queryCount);
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return VK_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanQuery::CmdBegin
(
    VulkanCmdBuffer * pCmdBuffer
    ,UINT32 query
    ,VkQueryControlFlags flags
)
{
    VkResult res = VK_SUCCESS;
    const bool isCmdBufferActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }
    m_pVulkanDev->CmdBeginQuery(pCmdBuffer->GetCmdBuffer(),
                                m_QueryPool.GetQueryPool(),
                                query,
                                flags);
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return VK_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanQuery::CmdEnd
(
    VulkanCmdBuffer * pCmdBuffer
    ,UINT32 query)
{
    VkResult res = VK_SUCCESS;
    const bool isCmdBufferActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }

    m_pVulkanDev->CmdEndQuery(pCmdBuffer->GetCmdBuffer(),
                              m_QueryPool.GetQueryPool(),
                              query);
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return VK_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
// Write the query results from the query pool directly to host memory.
VkResult VulkanQuery::GetResults
(
    UINT32 firstQuery
    ,UINT32 queryCount
    ,size_t dataSize
    ,void * pData
    ,VkDeviceSize stride
    ,VkQueryResultFlags flags
)
{
    return m_pVulkanDev->GetQueryPoolResults(m_QueryPool.GetQueryPool(),
                                             firstQuery,
                                             queryCount,
                                             dataSize,
                                             pData,
                                             stride,
                                             flags);
}

//-------------------------------------------------------------------------------------------------
// Copy the query results from the query pool to the internal VkBuffer.
VkResult VulkanQuery::CmdCopyResults
(
    VulkanCmdBuffer * pCmdBuffer
    ,UINT32 firstQuery
    ,UINT32 queryCount
    ,VkQueryResultFlags flags
)
{
    VkDeviceSize stride = (static_cast<VkDeviceSize>(m_BitsEnabled) *
                           ((flags & VK_QUERY_RESULT_64_BIT) ? 8 : 4));
    return CmdCopyResults(pCmdBuffer,
                          firstQuery,
                          queryCount,
                          &m_Results,
                          firstQuery * stride, //offset
                          stride,
                          flags);

}

//-------------------------------------------------------------------------------------------------
// Copy the query results from the query pool to a user supplied VkBuffer.
VkResult VulkanQuery::CmdCopyResults
(
    VulkanCmdBuffer * pCmdBuffer
    ,UINT32 firstQuery
    ,UINT32 queryCount
    ,VulkanBuffer * pDstBuffer
    ,VkDeviceSize dstOffset
    ,VkDeviceSize stride
    ,VkQueryResultFlags flags
)
{
    VkResult res = VK_SUCCESS;
    const bool isCmdBufferActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }

    m_pVulkanDev->CmdCopyQueryPoolResults(pCmdBuffer->GetCmdBuffer(),
                                          m_QueryPool.GetQueryPool(),
                                          firstQuery,
                                          queryCount,
                                          pDstBuffer->GetBuffer(),
                                          dstOffset,
                                          stride,
                                          flags);

    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return VK_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
// Write the timestamp query object to the query pool.
VkResult VulkanQuery::CmdWriteTimestamp
(
    VulkanCmdBuffer * pCmdBuffer
    ,VkPipelineStageFlagBits pipelineStage
    ,UINT32 query
)
{
    VkResult res = VK_SUCCESS;
    const bool isCmdBufferActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }

    m_pVulkanDev->CmdWriteTimestamp(pCmdBuffer->GetCmdBuffer(),
                                    pipelineStage,
                                    m_QueryPool.GetQueryPool(),
                                    query);

    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return VK_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
// Cleanup any resources used by the Query object.
VkResult VulkanQuery::Cleanup()
{
    m_QueryPool.DestroyQueryPool();
    const VkResult res = m_Results.Unmap();
    m_pResults = nullptr;
    m_Results.Cleanup();

    return res;
}
