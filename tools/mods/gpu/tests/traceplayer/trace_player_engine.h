/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/types.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surffill.h"
#include "trace_loader.h"
#include "trace_crc.h"
#include "trace_surface_accessor.h"
#include "trace_player_surface_variant.h"

#include <functional>
#include <memory>
#include <set>
#include <vector>

class LwRm;
class GpuDevice;
class GpuTestConfiguration;
class Channel;

namespace MfgTracePlayer
{
    enum EngineType
    {
         engineDontCare,
         engine3D,
         engineCompute,
         engineCopy
    };

    class Engine
    {
        public:
            struct Config
            {
                LwRm*                 pLwRm          = nullptr;
                GpuDevice*            pGpuDevice     = nullptr;
                GpuTestConfiguration* pTestConfig    = nullptr;
                DisplayMgr::TestContext* pDisplayMgrTestContext = nullptr;
                FLOAT64               defTimeoutMs   = 0;
                bool                  cacheSurfaces  = false;
                bool                  cpuWfi         = false;
                UINT32                displayPauseMs = 0U;
                INT32                 pri            = Tee::PriLow;
                UINT32                crcTolerance   = 0U;
                set<UINT32>           skipCRCFrames;
                UINT32                surfaceAccessorSizeMB = 16;
            };

            struct Error
            {
                string surface;     ///< Name of the surface on which the error oclwrred
                UINT32 subdev;      ///< Subdevice index
                UINT32 expected;    ///< Expected CRC
                UINT32 actual;      ///< Actual CRC
            };

            using Variant = MfgTracePlayer::Variant;

            Engine() = default;
            ~Engine();

            Engine(const Engine&)            = delete;
            Engine& operator=(const Engine&) = delete;

            /// Determines whether the player supports the given GPU.
            static bool IsSupported(LwRm* pLwRm, GpuDevice* pGpuDevice);
            /// Initializes configuration for a given GPU.
            void Init(const Config& init);
            /// Used by the parser to indicate that parsing is about to start.
            RC StartParsing();
            /// Used by the parser to indicate that parsing is done.
            RC DoneParsing();
            /// Checks whether the class requirements are satisfied on this GPU.
            bool IsSupported() const;
            /// Allocates all channels.
            RC AllocResources();
            /// Exelwtes the trace.
            /// @return Returns the list of CRC errors, if any were encountered during playback.
            RC PlayTrace(vector<Error>* pErrors);
            /// Releases all resources.
            RC FreeResources();
            /// Request that a surface is dumped when a specific event is hit.
            /// Must be called after the trace is loaded, but before AllocResources().
            RC DumpSurfaceOnEvent(const string& surfName, const string& eventName);

            enum State
            {
                init,
                traceLoaded,
                resourcesAllocated,
                played
            };

            /// Returns the trace loader object.
            Loader& GetLoader();

            // Trace commands which set up the trace

            void CreateChannel(string ch, UINT32* pIndex);
            void CreateSubChannel(string subch, UINT32 chId, UINT32 subchNumber, UINT32* pIndex);
            RC   SetSubChannelClass(UINT32 subchId, const string& className);
            void CreateSurface(string surf, UINT32* pIndex, Variant variant);
            void SetSurfaceFile(UINT32 surfId, string file);
            void SetSurfaceCrc(UINT32 surfId, string file);
            void SetSurfaceCrcRange(UINT32 surfId, UINT32 begin, UINT32 end);
            void SetSurfaceFill(UINT32 surfId, UINT32 value);
            void ConfigFromAttr(UINT32 surfId, UINT32 value);
            void ConfigFromAttr2(UINT32 surfId, UINT32 value);
            void SetVirtSurface(UINT32 surfId, UINT32 virtId);
            void SetPhysSurface(UINT32 surfId, UINT32 physId);
            void SetSurfaceVirtOffs(UINT32 surfId, UINT64 offs);
            void SetSurfacePhysOffs(UINT32 surfId, UINT64 offs);
            void CreateEvent(string eventName, UINT32* pIndex);

            // Access functions

            template<typename T>
            void ForEachSurface2D(UINT32 surfId, T func) const;
            bool HasCrc(UINT32 surfId) const;
            bool HasVirt(UINT32 surfId) const;
            bool HasPhys(UINT32 surfId) const;
            bool HasMap(UINT32 surfId) const;

            // Trace commands which schedule an action for exelwtion during Play()

            void AddAllocSurface(UINT32 surfId);
            void AddFinishSurfaces();
            void AddFreeSurface(UINT32 surfId);
            void AddGpEntry(UINT32 chId, UINT32 surfId, UINT32 offset, UINT32 size);
            void AddWaitForIdle(vector<UINT32> chIds);
            void AddEvent(UINT32 eventId);
            RC   AddCheckSurface(UINT32 surfId, const string& suffix);
            void AddDisplayImage(UINT32             surfId,
                                 UINT32             offset,
                                 ColorUtils::Format colorFormat,
                                 Surface2D::Layout  layout,
                                 UINT32             width,
                                 UINT32             height,
                                 UINT32             pitch,
                                 UINT32             logBlockHeight,
                                 UINT32             numBlocksWidth,
                                 UINT32             aaSamples);

        private:
            void FreeSurfaces();
            RC AllocChannels();
            RC FreeChannels();
            void PrintStats();
            RC IdleChannels();
            static RC Find3DClass(LwRm* pLwRm, GpuDevice* pGpuDevice, UINT32* pClassId);
            RC CalcAndCheckSurfaceSizes();
            struct Surface;
            RC CheckMapOffsets(const Surface& surf) const;
            RC AllocSurfaceVAs();
            enum WhatToFree
            {
                DontFreeVa,
                FreeAllParts
            };
            void FreeSurface(UINT32 surfId, WhatToFree freeVA);
            UINT64 GetPageSize(const Surface& surf) const;
            bool CacheSurfaces() const;
            RC FlushSocGpu();
            string GetCrcSuffix() const;
            template<typename T>
            void ForEachSurface2D(const Surface& surf, T func) const;

            struct SubChannel
            {
                SubChannel(string name, UINT32 number)
                    : name(move(name)), number(number) { }

                string     name;
                UINT32     number;
                UINT32     classId    = ~0U;
                UINT32     hClass     = ~0U;
                EngineType engineType = engineDontCare;
            };

            struct Channel
            {
                explicit Channel(string name)
                    : name(move(name)) { }

                string             name;
                vector<SubChannel> subchannels;
                ::Channel*         pCh = nullptr;
            };

            struct SubChannelRef
            {
                SubChannelRef(UINT32 chId, UINT32 subchId)
                    : chId(chId), subchId(subchId) { }

                UINT32 chId;
                UINT32 subchId;
            };

            struct Surface
            {
                explicit Surface(string name)
                    : name(move(name))
                { }

                typedef unique_ptr<Surface2D> SurfPtr;

                string         name;
                string         file;
                string         crc;
                UINT32         crcBegin    = 0U;
                UINT32         crcEnd      = 0U;
                vector<string> crcSuffixes;
                UINT32         fill        = 0U;
                bool           pushbuffer  = false;
                bool           displayable = false;
                SurfPtr        pSurfVA;
                SurfPtr        pSurfPA;
                SurfPtr        pSurfMap;
                UINT32         attr        = 0U;
                UINT32         attr2       = 0U;
                UINT32         virtId      = ~0U;
                UINT32         physId      = ~0U;
                UINT64         virtOffs    = 0U;
                UINT64         physOffs    = 0U;
                UINT64         actualSize  = 0U;
            };

            struct Event
            {
                explicit Event(string name)
                    : name(move(name))
                { }

                string name;
                vector<function<RC()>> handlers;  // Filled by DumpSurfaceOnEvent()
            };

            struct Stats
            {
                UINT64 alloc = 0;   // Time it takes to allocate and load surfaces
                UINT64 free  = 0;   // Time it takes to free surfaces
                UINT64 wfi   = 0;   // Time it takes to wait for GPU to complete work
                UINT64 crc   = 0;   // Time it takes to CRC surfaces
                UINT64 total = 0;   // Total playback time
            };

            Config                 m_Config;
            INT32                  m_Pri               = Tee::PriLow;
            Loader                 m_Loader;
            SurfaceAccessor        m_SurfaceAccessor;
            Engine::State          m_State             = Engine::init;
            bool                   m_HasFree           = false;
            bool                   m_HasAllocAfterFree = false;
            bool                   m_NeedFinish        = false;
            UINT32                 m_DisplaySurfId     = ~0U;
            vector<Channel>        m_Channels;
            vector<SubChannelRef>  m_SubChannels;
            vector<Surface>        m_Surfaces;
            vector<Event>          m_Events;
            vector<function<RC()>> m_Commands;
            UINT32                 m_hVASpace          = 0U;
            CRCChain               m_CrcChain;
            vector<Error>          m_Errors;
            Stats                  m_Stats;
            UINT64                 m_TotalMem          = 0U;
            UINT64                 m_Begilwa           = ~0ULL;
            UINT64                 m_EndVa             = 0U;
            unique_ptr<SurfaceUtils::OptimalSurfaceFiller> m_pSurfaceFiller;
            Surface2D              m_WfiSema;
            UINT32                 m_WfiCount          = 0U;
    };

    template<typename T>
    void Engine::ForEachSurface2D(UINT32 surfId, T func) const
    {
        MASSERT(surfId < m_Surfaces.size());
        const Surface& surf = m_Surfaces[surfId];
        ForEachSurface2D(surf, func);
    }

    template<typename T>
    void Engine::ForEachSurface2D(const Surface& surf, T func) const
    {
        if (surf.pSurfVA.get())
        {
            func(surf.pSurfVA.get());
        }
        if (surf.pSurfPA.get())
        {
            func(surf.pSurfPA.get());
        }
        if (surf.pSurfMap.get())
        {
            func(surf.pSurfMap.get());
        }
    }
}
