/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009,2013-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mapfb.h"
#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jscript.h"

MapFb::MapFb()
{
   m_pMappingAddress = NULL;
   m_MappingActive   = false;
   m_MappingHandle   = 0;
   m_MappingFbOffset = 0;
   m_MappingSize     = 0;
   m_pMappingSubdev  = NULL;
   m_pMappingAddress = NULL;
   m_EnableWARForBug1180705 = false;
   JavaScriptPtr pJs;
   pJs->GetProperty(pJs->GetGlobalObject(), "g_RunConlwrrentDevices",
       &m_EnableWARForBug1180705);
}

MapFb::~MapFb()
{
   MASSERT(!m_MappingActive);
}

static void WARForBug1180705()
{
    // As discovered in bug 1056104 exelwting a number of
    // MapMemory calls without yielding to other threads causes
    // interrupt servicing starvation (because of a lock shared
    // between interrupt handler and MapMemory API calls).
    // This in turn stalls work on some GPUs, detected as
    // timeouts by MODS.
    Tasker::Yield();
}

void * MapFb::MapFbRegion
(
    GpuUtility::MemoryChunks *pChunks,
    UINT64 fbOffset,
    UINT64 size,
    GpuSubdevice *pGpuSubdev
)
{
    MASSERT(pChunks);

    LwRmPtr pLwRm;

    if (m_MappingActive)
    {
        if ( (fbOffset >= m_MappingFbOffset) &&
             ( (fbOffset + size) <= (m_MappingFbOffset + m_MappingSize) ) &&
             ( pGpuSubdev == m_pMappingSubdev ) )
        {
            return (UINT08*)m_pMappingAddress + (fbOffset - m_MappingFbOffset);
        }

        Printf(Tee::PriDebug,
               "MapFb: Request DID NOT fit within current mapping\n");
        UnmapFbRegion();
    }

    UINT64 const MappingAlignSize = 0x10000;

    UINT64 max_mapping_size =
        (size + MappingAlignSize - 1) & ~(MappingAlignSize-1);

    Printf(Tee::PriDebug, "MapFb: Maximum mapping size: 0x%08llx bytes\n",
           max_mapping_size);

    for (GpuUtility::MemoryChunks::iterator chunk_iter = pChunks->begin();
         chunk_iter != pChunks->end();
         chunk_iter++)
    {
        if (chunk_iter->fbOffset > fbOffset)
            continue;

        UINT64 offset_within_chunk = fbOffset - chunk_iter->fbOffset;

        if ( (offset_within_chunk + size) > chunk_iter->size )
            continue;

        m_MappingHandle = chunk_iter->hMem;

        if (chunk_iter->size <= max_mapping_size)
        {
            // map whole chunk
            Printf(Tee::PriDebug, "MapFb: Mapping whole chunk\n");

            m_MappingFbOffset = chunk_iter->fbOffset;
            m_MappingSize     = chunk_iter->size;
            m_pMappingSubdev  = pGpuSubdev;

            if (pLwRm->MapMemory(m_MappingHandle,
                                 chunk_iter->mapOffset,
                                 m_MappingSize,
                                 &m_pMappingAddress,
                                 0, m_pMappingSubdev) != OK)
            {
                Printf(Tee::PriHigh, "MapFb: ERROR! Unable to map fb region\n");
                MASSERT(!"MapFb: Unable to map fb region");
                return NULL;
            }

            m_MappingActive   = true;

            DumpMappingInfo(Tee::PriDebug, offset_within_chunk);

            if (m_EnableWARForBug1180705)
                WARForBug1180705();

            return (UINT08*)m_pMappingAddress + offset_within_chunk;
        }
        else
        {
            // map only a part of chunk that fits into max_mapping_size
            Printf(Tee::PriDebug, "MapFb: Mapping part of a chunk\n");
            m_MappingFbOffset = fbOffset;
            m_MappingSize     = max_mapping_size;
            m_pMappingSubdev  = pGpuSubdev;

            if ( (offset_within_chunk + m_MappingSize) > chunk_iter->size)
            {
                // The window needs to be moved so that a mapping of size
                // max_mapping_size will not be outside of a mem chunk
                offset_within_chunk = chunk_iter->size - m_MappingSize;
                m_MappingFbOffset = chunk_iter->fbOffset + offset_within_chunk;
            }

            // Try to map at aligned offset within a chunk if possible:
            UINT64 alignedMappingFbOffset =
                m_MappingFbOffset & ~(MappingAlignSize-1);
            // After align we need to be still within the chunk
            MASSERT (alignedMappingFbOffset >= chunk_iter->fbOffset);
            UINT64 alignedOffsetWithinChunk =
                alignedMappingFbOffset - chunk_iter->fbOffset;
            UINT64 alignedRequestedOffsetWithinMapping =
                fbOffset - alignedMappingFbOffset;

            if (alignedRequestedOffsetWithinMapping + size <= max_mapping_size)
            {
                offset_within_chunk = alignedOffsetWithinChunk;
                m_MappingFbOffset = alignedMappingFbOffset;
            }
            // End Try

            if (pLwRm->MapMemory(m_MappingHandle,
                                 offset_within_chunk + chunk_iter->mapOffset,
                                 m_MappingSize,
                                 &m_pMappingAddress,
                                 0, m_pMappingSubdev
                                 ) != OK)
            {
                MASSERT(!"Unable to map fb region");
                return NULL;
            }

            m_MappingActive   = true;

            UINT64 offset_from_base = fbOffset - m_MappingFbOffset;

            DumpMappingInfo(Tee::PriDebug, offset_from_base);

            if (m_EnableWARForBug1180705)
                WARForBug1180705();

            return (UINT08*)m_pMappingAddress + offset_from_base;
        }
    }

    MASSERT(!"MapFbRegion can't find a chunk to accommodate request");
    return NULL;
}

void MapFb::UnmapFbRegion()
{
    if (m_MappingActive)
    {
        LwRmPtr()->UnmapMemory(m_MappingHandle, m_pMappingAddress, 0,
                               m_pMappingSubdev);
        m_MappingActive = false;
        Printf(Tee::PriDebug, "MapFb: Unmapped active FB region\n");
    }
    else
    {
        Printf(Tee::PriDebug,
               "MapFb: No mapping active UnmapFbRegion is a NOP\n");
    }
}

//------------------------------------------------------------------------------
void MapFb::DumpMappingInfo(Tee::Priority pri, UINT64 offset)
{
    Printf(pri,
           "MapFb: Mapped memory from offset 0x%08llx for 0x%08llx bytes\n",
           m_MappingFbOffset, m_MappingSize);

    Printf(pri,
           "MapFb: Base of mapping: %p, offset within chunk: 0x%08llx\n",
           m_pMappingAddress, offset);
}
