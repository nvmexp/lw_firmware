/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GPU_MAPFB_H
#define INCLUDED_GPU_MAPFB_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif

#ifndef INCLUDED_TEE_H
#include "core/include/tee.h"
#endif

#ifndef INCLUDED_GPU_UTILITY_H
#include "gpuutils.h"
#endif

class GpuSubdevice;

class MapFb
{
    public:
        MapFb();
        ~MapFb();
        void * MapFbRegion
        (
            GpuUtility::MemoryChunks *Chunks,
            UINT64 FbOffset,
            UINT64 Size,
            GpuSubdevice *GpuSubdev
        );
        void UnmapFbRegion();

    private:
        void DumpMappingInfo(Tee::Priority Pri, UINT64 Offset);
        inline void DumpMappingInfo(Tee::PriDebugStub, UINT64 offset)
        {
            DumpMappingInfo(Tee::PriSecret, offset);
        }

        bool   m_MappingActive;
        UINT32 m_MappingHandle;
        UINT64 m_MappingFbOffset;
        UINT64 m_MappingSize;
        GpuSubdevice *m_pMappingSubdev;
        void  *m_pMappingAddress;
        bool   m_EnableWARForBug1180705;
};

#endif // INCLUDED_GPU_MAPFB_H
