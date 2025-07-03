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
#pragma once

#include "vkmain.h"
#include "vkbuffer.h"

class VulkanDevice;
class VulkanCmdBuffer;

class VulkanQueryPool
{
public:
    VulkanQueryPool() = default;
    explicit VulkanQueryPool(VulkanDevice* pVulkanDev);
    ~VulkanQueryPool();
    VulkanQueryPool(VulkanQueryPool&& vulkQueryPool) noexcept;
    VulkanQueryPool& operator=(VulkanQueryPool&& vulkQueryPool);
    VulkanQueryPool(const VulkanQueryPool& vulkQueryPool) = delete;
    VulkanQueryPool& operator=(const VulkanQueryPool& vulkQueryPool) = delete;

    VkResult InitQueryPool
    (
        VkQueryType queryType
        ,UINT32     queryCount
        ,VkQueryPipelineStatisticFlags pipelineStats
    );

    void       DestroyQueryPool();
    GET_PROP(QueryPool, VkQueryPool);

private:
    VulkanDevice* m_pVulkanDev = nullptr;
    VkQueryPool   m_QueryPool  = VK_NULL_HANDLE;

    void TakeResources
    (
        VulkanQueryPool&& other
    );
};

//------------------------------------------------------------------------------------------------
// VulkanQuery object is a thin wrapper around the Vulkan APIs to perform the various types of
// queries on the graphics system. There are 3 general types of Queries, Oclwllusion, Pipeline
// statistics, & Timestamp. Each type of query requires its own QueryPool to manage the query
// results.
// To perform a query on the Graphics system you need to do the following steps:
// a) Construct a VulkanQuery object
// b) Initialize the query object with the type and number of Queries to perform.
// c) Reset all of the queries for use
// d) Begin a query on a specific VkCmdBuffer
// e) End the query on a specific VkCmdBuffer
// f) Retrieve the results.
//
class VulkanQuery
{
public:
    VulkanQuery() = default;
    explicit VulkanQuery(VulkanDevice* pVulkanDev);
    VulkanQuery(const VulkanQuery& query) = delete;
    VulkanQuery& operator=(const VulkanQuery& query) = delete;
    VulkanQuery(VulkanQuery&& query) noexcept = default;
    VulkanQuery& operator=(VulkanQuery&& query) = default;
    ~VulkanQuery();

    VkResult Init
    (
        VkQueryType queryType
        ,UINT32 queryCount
        ,VkQueryPipelineStatisticFlags pipelineStats
    );

    VkResult CmdReset
    (
        VulkanCmdBuffer * pCmdBuffer
        ,UINT32 firstQuery
        ,UINT32 queryCount
    );

    VkResult CmdBegin
    (
        VulkanCmdBuffer * pCmdBuffer
        ,UINT32 query
        ,VkQueryControlFlags flags
    );

    VkResult CmdEnd
    (
        VulkanCmdBuffer * pCmdBuffer
        ,UINT32 query
    );

    // Return a pointer to a host mapped VkBuffer
    void* GetResultsPtr() const { return m_pResults; }
    VulkanBuffer* GetResultsBuffer() { return &m_Results; }
    const VulkanBuffer* GetResultsBuffer() const { return &m_Results; }

    // Write the query results from the query pool directly to host memory.
    VkResult GetResults
    (
        UINT32 firstQuery
        ,UINT32 queryCount
        ,size_t dataSize
        ,void * pData
        ,VkDeviceSize stride
        ,VkQueryResultFlags flags
    );

    // Copy the query results from the query pool to the internal VkBuffer.
    VkResult CmdCopyResults
    (
        VulkanCmdBuffer * pCmdBuffer
        ,UINT32 firstQuery
        ,UINT32 queryCount
        ,VkQueryResultFlags flags
    );

    // Copy the query results from the query pool to a VkBuffer.
    VkResult CmdCopyResults
    (
        VulkanCmdBuffer * pCmdBuffer
        ,UINT32 firstQuery
        ,UINT32 queryCount
        ,VulkanBuffer * pDstBuffer
        ,VkDeviceSize dstOffset
        ,VkDeviceSize stride
        ,VkQueryResultFlags flags
    );

    // Write the timestamp query object to the query pool.
    VkResult CmdWriteTimestamp
    (
        VulkanCmdBuffer * pCmdBuffer
        ,VkPipelineStageFlagBits pipelineStage
        ,UINT32 query
    );

    VkResult Cleanup();

private:
    VkDeviceSize    m_Stride      = sizeof(UINT64);
    VulkanDevice*   m_pVulkanDev  = nullptr;
    VkQueryType     m_QueryType   = VK_QUERY_TYPE_TIMESTAMP;
    void*           m_pResults    = nullptr;
    UINT32          m_BitsEnabled = 0;
    VulkanQueryPool m_QueryPool; // a pool resource to allocate the query objects
    VulkanBuffer    m_Results;   // a VkBuffer to hold the query results.
};
