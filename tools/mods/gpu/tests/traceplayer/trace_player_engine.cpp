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

#include "trace_player_engine.h"
#include "core/include/abort.h"
#include "core/include/channel.h"
#include "core/include/cpu.h"
#include "core/include/fileholder.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gputestc.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surffill.h"

#include "class/cl90f1.h" // FERMI_VASPACE_A
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cla097.h" // KEPLER_A
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "class/cla0b5sw.h" // LWA0B5_ALLOCATION_PARAMETERS
#include "class/clb0b5.h" // MAXWELL_DMA_COPY_A
#include "class/clc0b5.h" // PASCAL_DMA_COPY_A
#include "class/clc1b5.h" // PASCAL_DMA_COPY_B
#include "class/clc3b5.h" // VOLTA_DMA_COPY_A
#include "class/clc5b5.h" // TURING_DMA_COPY_A
#include "class/clc6b5.h" // AMPERE_DMA_COPY_A
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // HOPPER_DMA_COPY_A
#include "class/cla0c0.h" // KEPLER_COMPUTE_A
#include "class/cla140.h" // KEPLER_INLINE_TO_MEMORY_B
#include "class/cla197.h" // KEPLER_B
#include "class/cla1c0.h" // KEPLER_COMPUTE_B
#include "class/cla297.h" // KEPLER_C
#include "class/clb097.h" // MAXWELL_A
#include "class/clb0c0.h" // MAXWELL_COMPUTE_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clb1c0.h" // MAXWELL_COMPUTE_B
#include "class/clc097.h" // PASCAL_A
#include "class/clc0c0.h" // PASCAL_COMPUTE_A
#include "class/clc197.h" // PASCAL_B
#include "class/clc1c0.h" // PASCAL_COMPUTE_B
#include "class/clc397.h" // VOLTA_A
#include "class/clc3c0.h" // VOLTA_COMPUTE_A
#include "class/clc597.h" // TURING_A
#include "class/clc5c0.h" // TURING_COMPUTE_A
#include "class/clc697.h" // AMPERE_A
#include "class/clc6c0.h" // AMPERE_COMPUTE_A
#include "class/clc797.h" // AMPERE_B
#include "class/clc7c0.h" // AMPERE_COMPUTE_B
#include "class/clc997.h" // ADA_A
#include "class/clc9c0.h" // ADA_COMPUTE_A
#include "class/clcb97.h" // HOPPER_A
#include "class/clcbc0.h" // HOPPER_COMPUTE_A
#include "class/clcc97.h" // HOPPER_B
#include "class/clccc0.h" // HOPPER_COMPUTE_B
#include "class/clcd97.h" // BLACKWELL_A
#include "class/clcdc0.h" // BLACKWELL_COMPUTE_A

namespace MfgTracePlayer
{
    static const struct ClassMap
    {
        const char* name;       // String found in the trace file
        UINT32      classId;    // Object class ID used in channels
        EngineType  engineType; // Type used to find a CE instance
    } s_ClassMap[] =
    {
        // 3D
        { "blackwell_a",               BLACKWELL_A,               engine3D       },
        { "hopper_b",                  HOPPER_B,                  engine3D       },
        { "hopper_a",                  HOPPER_A,                  engine3D       },
        { "ada_a",                     ADA_A,                     engine3D       },
        { "ampere_b",                  AMPERE_B,                  engine3D       },
        { "ampere_a",                  AMPERE_A,                  engine3D       },
        { "turing_a",                  TURING_A,                  engine3D       },
        { "volta_a",                   VOLTA_A,                   engine3D       },
        { "pascal_b",                  PASCAL_B,                  engine3D       },
        { "pascal_a",                  PASCAL_A,                  engine3D       },
        { "maxwell_b",                 MAXWELL_B,                 engine3D       },
        { "maxwell_a",                 MAXWELL_A,                 engine3D       },
        { "kepler_c",                  KEPLER_C,                  engine3D       },
        { "kepler_b",                  KEPLER_B,                  engine3D       },
        { "kepler_a",                  KEPLER_A,                  engine3D       },
        // Compute
        { "blackwell_compute_a",       BLACKWELL_COMPUTE_A,       engineCompute  },
        { "hopper_compute_b",          HOPPER_COMPUTE_B,          engineCompute  },
        { "hopper_compute_a",          HOPPER_COMPUTE_A,          engineCompute  },
        { "ada_compute_a",             ADA_COMPUTE_A,             engineCompute  },
        { "ampere_compute_b",          AMPERE_COMPUTE_B,          engineCompute  },
        { "ampere_compute_a",          AMPERE_COMPUTE_A,          engineCompute  },
        { "turing_compute_a",          TURING_COMPUTE_A,          engineCompute  },
        { "volta_compute_a",           VOLTA_COMPUTE_A,           engineCompute  },
        { "pascal_compute_b",          PASCAL_COMPUTE_B,          engineCompute  },
        { "pascal_compute_a",          PASCAL_COMPUTE_A,          engineCompute  },
        { "maxwell_compute_b",         MAXWELL_COMPUTE_B,         engineCompute  },
        { "maxwell_compute_a",         MAXWELL_COMPUTE_A,         engineCompute  },
        { "kepler_compute_b",          KEPLER_COMPUTE_B,          engineCompute  },
        { "kepler_compute_a",          KEPLER_COMPUTE_A,          engineCompute  },
        // 2D
        { "fermi_twod_a",              FERMI_TWOD_A,              engineDontCare },
        // Inline to memory
        { "kepler_inline_to_memory_b", KEPLER_INLINE_TO_MEMORY_B, engineDontCare },
        // Copy engine
        { "blackwell_dma_copy_a",      BLACKWELL_DMA_COPY_A,      engineCopy     },
        { "hopper_dma_copy_a",         HOPPER_DMA_COPY_A,         engineCopy     },
        { "ampere_dma_copy_b",         AMPERE_DMA_COPY_B,         engineCopy     },
        { "ampere_dma_copy_a",         AMPERE_DMA_COPY_A,         engineCopy     },
        { "turing_dma_copy_a",         TURING_DMA_COPY_A,         engineCopy     },
        { "volta_dma_copy_a",          VOLTA_DMA_COPY_A,          engineCopy     },
        { "pascal_dma_copy_b",         PASCAL_DMA_COPY_B,         engineCopy     },
        { "pascal_dma_copy_a",         PASCAL_DMA_COPY_A,         engineCopy     },
        { "maxwell_dma_copy_a",        MAXWELL_DMA_COPY_A,        engineCopy     },
        { "kepler_dma_copy_a",         KEPLER_DMA_COPY_A,         engineCopy     },
    };
}

namespace
{
    // Semaphore stride, ideally should be an entire cache line
    constexpr UINT32 s_SemSize = 256;

    // The lowest possible VA, according to RM
    constexpr UINT64 s_LowestVA = 64_MB;

    class Timer
    {
        public:
            explicit Timer(UINT64* pAclwm)
            : m_pAclwm(pAclwm)
            , m_Start(Xp::GetWallTimeMS())
            {
            }
            ~Timer()
            {
                const UINT64 time = Xp::GetWallTimeMS();
                *m_pAclwm += time - m_Start;
            }

        private:
            UINT64* const m_pAclwm;
            const UINT64  m_Start;
    };
}

MfgTracePlayer::Engine::~Engine()
{
    MASSERT(m_State == init || m_State == traceLoaded);

    FreeResources();

    // Release surfaces
    for (UINT32 surfIdx = 0; surfIdx < m_Surfaces.size(); surfIdx++)
    {
        Surface& surf = m_Surfaces[surfIdx];
        surf.pSurfMap.reset();
        surf.pSurfPA.reset();
        surf.pSurfVA.reset();
    }
}

bool MfgTracePlayer::Engine::IsSupported(LwRm* pLwRm, GpuDevice* pGpuDevice)
{
    UINT32 classId = 0;
    return pGpuDevice->GetNumSubdevices() == 1 &&
           OK == Find3DClass(pLwRm, pGpuDevice, &classId);
}

void MfgTracePlayer::Engine::Init(const Config& config)
{
    MASSERT(m_State == init);
    m_Config = config;
    m_Pri = config.pri;
}

RC MfgTracePlayer::Engine::StartParsing()
{
    MASSERT(m_State == init);

    Printf(m_Pri, "Trace player: Parsing trace\n");

    m_HasFree           = false;
    m_HasAllocAfterFree = false;
    m_Begilwa           = ~0ULL;
    m_EndVa             = 0;

    // Find first supported 3D class for CRC
    UINT32 classId = 0;
    RC rc = Find3DClass(m_Config.pLwRm, m_Config.pGpuDevice, &classId);
    if (rc != OK)
    {
        Printf(Tee::PriError, "No supported class found\n");
        return rc;
    }

    return m_CrcChain.Load(m_Loader, classId);
}

RC MfgTracePlayer::Engine::DoneParsing()
{
    MASSERT(m_State == init);
    m_State = traceLoaded;

    return CalcAndCheckSurfaceSizes();
}

MfgTracePlayer::Loader& MfgTracePlayer::Engine::GetLoader()
{
    return m_Loader;
}

void MfgTracePlayer::Engine::CreateChannel(string ch, UINT32* pIndex)
{
    MASSERT(m_State == init);

    *pIndex = static_cast<UINT32>(m_Channels.size());
    m_Channels.emplace_back(move(ch));
}

void MfgTracePlayer::Engine::CreateSubChannel(string subch, UINT32 chId, UINT32 subchNumber, UINT32* pIndex)
{
    MASSERT(m_State == init);
    MASSERT(chId < m_Channels.size());

    const UINT32 idx = static_cast<UINT32>(m_Channels[chId].subchannels.size());
    *pIndex = static_cast<UINT32>(m_SubChannels.size());
    m_SubChannels.emplace_back(chId, idx);

    m_Channels[chId].subchannels.emplace_back(move(subch), subchNumber);
}

namespace
{
    UINT32 MaybeGetClassId(const string& className)
    {
        const char* const begin = className.c_str();
        char* end = 0;

        const UINT64 number = Utility::Strtoull(begin, &end, 0);

        if (number < numeric_limits<UINT32>::max() &&
                end == begin + className.size())
            return static_cast<UINT32>(number);

        return ~0U;
    }
}

RC MfgTracePlayer::Engine::SetSubChannelClass(UINT32 subchId, const string& className)
{
    const UINT32 numClasses = sizeof(s_ClassMap) / sizeof(s_ClassMap[0]);
    const UINT32 classId = MaybeGetClassId(className);

    // Find the class identifier
    UINT32 idx = 0;
    for ( ; idx < numClasses; idx++)
    {
        if (className == s_ClassMap[idx].name ||
            classId   == s_ClassMap[idx].classId)
        {
            break;
        }
    }
    if (idx >= numClasses)
    {
        Printf(Tee::PriError, "Unsupported class: %s\n", className.c_str());
        return RC::BAD_TRACE_DATA;
    }

    MASSERT(m_State == init);
    MASSERT(subchId < m_SubChannels.size());

    const SubChannelRef& ref = m_SubChannels[subchId];
    SubChannel& subch = m_Channels[ref.chId].subchannels[ref.subchId];
    subch.classId    = s_ClassMap[idx].classId;
    subch.engineType = s_ClassMap[idx].engineType;
    return OK;
}

void MfgTracePlayer::Engine::CreateSurface(string surfName, UINT32* pIndex, Variant variant)
{
    MASSERT(m_State == init);
    const UINT32 index = static_cast<UINT32>(m_Surfaces.size());
    *pIndex = index;
    m_Surfaces.emplace_back(move(surfName));
    Surface& surf = m_Surfaces.back();
    surf.virtId = index;
    surf.physId = index;

    if (variant & Variant::Virtual)
    {
        surf.pSurfVA = make_unique<Surface2D>();
        surf.pSurfVA->SetSpecialization(Surface2D::VirtualOnly);
    }

    if (variant & Variant::Physical)
    {
        surf.pSurfPA = make_unique<Surface2D>();
        surf.pSurfPA->SetSpecialization(Surface2D::PhysicalOnly);
    }

    if (variant & Variant::Map)
    {
        surf.pSurfMap = make_unique<Surface2D>();
        surf.pSurfMap->SetSpecialization(Surface2D::MapOnly);
    }
}

void MfgTracePlayer::Engine::SetSurfaceFile(UINT32 surfId, string file)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    m_Surfaces[surfId].file = move(file);
}

void MfgTracePlayer::Engine::SetSurfaceCrc(UINT32 surfId, string file)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    m_Surfaces[surfId].crc = move(file);
}

void MfgTracePlayer::Engine::SetSurfaceCrcRange(UINT32 surfId, UINT32 begin, UINT32 end)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    m_Surfaces[surfId].crcBegin = begin;
    m_Surfaces[surfId].crcEnd   = end;
}

void MfgTracePlayer::Engine::SetSurfaceFill(UINT32 surfId, UINT32 value)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    m_Surfaces[surfId].fill = value;
}

void MfgTracePlayer::Engine::ConfigFromAttr(UINT32 surfId, UINT32 value)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    m_Surfaces[surfId].attr = value;
}

void MfgTracePlayer::Engine::ConfigFromAttr2(UINT32 surfId, UINT32 value)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    m_Surfaces[surfId].attr2 = value;
}

void MfgTracePlayer::Engine::SetVirtSurface(UINT32 surfId, UINT32 virtId)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    MASSERT(virtId < m_Surfaces.size());

    Surface&       surf   = m_Surfaces[surfId];
    const Surface& surfVA = m_Surfaces[virtId];

    surf.virtId = virtId;
    surf.attr   = surfVA.attr;
    surf.attr2  = surfVA.attr2;
    surf.pSurfMap->SetType(surfVA.pSurfVA->GetType());
}

void MfgTracePlayer::Engine::SetPhysSurface(UINT32 surfId, UINT32 physId)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    MASSERT(physId < m_Surfaces.size());
    m_Surfaces[surfId].physId = physId;
}

void MfgTracePlayer::Engine::SetSurfaceVirtOffs(UINT32 surfId, UINT64 offs)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    m_Surfaces[surfId].virtOffs = offs;
}

void MfgTracePlayer::Engine::SetSurfacePhysOffs(UINT32 surfId, UINT64 offs)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());
    m_Surfaces[surfId].physOffs = offs;
}

void MfgTracePlayer::Engine::CreateEvent(string eventName, UINT32* pIndex)
{
    MASSERT(m_State == init);
    *pIndex = static_cast<UINT32>(m_Events.size());
    m_Events.emplace_back(move(eventName));
}

bool MfgTracePlayer::Engine::HasCrc(UINT32 surfId) const
{
    MASSERT(surfId < m_Surfaces.size());
    return !m_Surfaces[surfId].crc.empty();
}

bool MfgTracePlayer::Engine::HasVirt(UINT32 surfId) const
{
    MASSERT(surfId < m_Surfaces.size());
    return m_Surfaces[surfId].pSurfVA != nullptr;
}

bool MfgTracePlayer::Engine::HasPhys(UINT32 surfId) const
{
    MASSERT(surfId < m_Surfaces.size());
    return m_Surfaces[surfId].pSurfPA != nullptr;
}

bool MfgTracePlayer::Engine::HasMap(UINT32 surfId) const
{
    MASSERT(surfId < m_Surfaces.size());
    return m_Surfaces[surfId].pSurfMap != nullptr;
}

namespace
{
    template<typename T>
    static RC FindIndex(const T& collection, const char* what, const string& name, UINT32* pIndex)
    {
        const auto idx = static_cast<UINT32>(
                distance(begin(collection),
                    find_if(begin(collection), end(collection),
                         [&](const typename T::value_type& elem)
                         {
                             return elem.name == name;
                         })));

        if (idx >= collection.size())
        {
            Printf(Tee::PriError, "Unknown %s: \"%s\"\n", what, name.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        *pIndex = idx;
        return OK;
    }
}

RC MfgTracePlayer::Engine::DumpSurfaceOnEvent
(
    const string& surfName,
    const string& eventName
)
{
    MASSERT(m_State == Engine::traceLoaded);

    RC rc;

    UINT32 surfId = 0;
    CHECK_RC(FindIndex(m_Surfaces, "surface", surfName, &surfId));

    UINT32 eventId = 0;
    CHECK_RC(FindIndex(m_Events, "event", eventName, &eventId));

    const auto   dotPos = surfName.rfind('.');
    const auto   prefix = surfName.substr(0, dotPos);
    const string suffix = dotPos == string::npos ? "" : surfName.substr(dotPos);

    m_Events[eventId].handlers.emplace_back([=]() -> RC
    {
        const UINT64 MAX_BUF_SIZE = 0x10000U;

        MASSERT(surfId < m_Surfaces.size());
        Surface& surf = m_Surfaces[surfId];

        if ((surf.pSurfMap != nullptr) && surf.pSurfMap->IsAllocated())
        {
            // Use a global tag to avoid collisions between multiple instances
            // (e.g. running on multiple GPUs) and between multiple test loops.
            static volatile INT32 dumpTag = 1;

            const INT32  count    = Cpu::AtomicAdd(&dumpTag, 1);
            const string countTag = Utility::StrPrintf("_%d", count);

            const auto numSubdevices = surf.pSurfMap->GetGpuDev()->GetNumSubdevices();

            for (UINT32 subdev = 0; subdev < numSubdevices; ++subdev)
            {
                const string gpuTag   = subdev == 0 ? "" : Utility::StrPrintf("_GPU%d", subdev);
                const string filename = prefix + countTag + gpuTag + suffix;

                Printf(this->m_Pri, "Dumping %s\n", filename.c_str());

                SurfaceUtils::MappingSurfaceReader reader(*surf.pSurfMap);
                reader.SetTargetSubdevice(subdev);

                FileHolder fh;
                RC rc;
                CHECK_RC(fh.Open(filename, "wb"));

                const UINT64 surfSize = surf.pSurfMap->GetSize();
                vector<UINT08> buf;
                for (UINT64 offset=0; offset < surfSize; offset += buf.size())
                {
                    const size_t chunkSize = min(surfSize - offset, MAX_BUF_SIZE);
                    buf.resize(chunkSize);
                    CHECK_RC(m_SurfaceAccessor.ReadBytes(surf.pSurfMap.get(),
                                                         subdev,
                                                         offset,
                                                         &buf[0],
                                                         buf.size()));
                    if (fwrite(&buf[0], buf.size(), 1, fh.GetFile()) != 1)
                    {
                        return RC::FILE_WRITE_ERROR;
                    }
                }
            }
        }
        return OK;
    });

    return rc;
}

void MfgTracePlayer::Engine::FreeSurfaces()
{
    Timer timer(&m_Stats.free);

    // Free mappings created using MAP trace command
    // This has to be done before releasing memory and VAs
    for (UINT32 surfId = 0; surfId < m_Surfaces.size(); surfId++)
    {
        Surface& surf = m_Surfaces[surfId];
        if (surf.pSurfMap.get() && !surf.pSurfVA.get())
        {
            FreeSurface(surfId, FreeAllParts);
        }
    }

    // Free remaining allocations
    for (UINT32 surfId = 0; surfId < m_Surfaces.size(); surfId++)
    {
        FreeSurface(surfId, FreeAllParts);
    }
}

RC MfgTracePlayer::Engine::AllocChannels()
{
    RC rc;

    for (UINT32 chId=0; chId < m_Channels.size(); chId++)
    {
        Channel& ch = m_Channels[chId];

        if (ch.pCh)
        {
            continue;
        }

        Printf(m_Pri, "Allocating channel %s\n", ch.name.c_str());

        GpuTestConfiguration* const pTestConfig = m_Config.pTestConfig;
        pTestConfig->SetAllowMultipleChannels(true);
        pTestConfig->SetChannelType(TestConfiguration::GpFifoChannel);
        UINT32 hCh = ~0U;
        CHECK_RC(pTestConfig->AllocateChannel(&ch.pCh,
                                              &hCh,
                                              nullptr,
                                              nullptr,
                                              m_hVASpace,
                                              0,
                                              LW2080_ENGINE_TYPE_GRAPHICS));

        // Determine if any channel uses graphics
        bool hasGraphics = false;
        for (UINT32 subchId=0; subchId < ch.subchannels.size(); subchId++)
        {
            const EngineType engineType = ch.subchannels[subchId].engineType;
            if (engineType == engine3D || engineType == engineCompute)
            {
                hasGraphics = true;
                break;
            }
        }

        for (UINT32 subchId=0; subchId < ch.subchannels.size(); subchId++)
        {
            SubChannel& subch = ch.subchannels[subchId];
            void* pParams = nullptr;

            Printf(m_Pri,
                   "Allocating class 0x%04x on channel %s subchannel %u\n",
                   subch.classId, ch.name.c_str(), subch.number);

            // Find CE instance which can work together with graphics
            LWA0B5_ALLOCATION_PARAMETERS params = { };
            if (subch.engineType == engineCopy && hasGraphics)
            {
                LW2080_CTRL_GPU_GET_ENGINE_PARTNERLIST_PARAMS partnerParams = { };
                partnerParams.engineType         = LW2080_ENGINE_TYPE_GRAPHICS;
                partnerParams.partnershipClassId = ch.pCh->GetClass();
                CHECK_RC(m_Config.pLwRm->ControlBySubdevice(
                            m_Config.pGpuDevice->GetSubdevice(0),
                            LW2080_CTRL_CMD_GPU_GET_ENGINE_PARTNERLIST,
                            &partnerParams, sizeof(partnerParams)));
                if (partnerParams.numPartners == 0)
                {
                    Printf(Tee::PriError, "Unable to match CE with graphics on this GPU\n");
                    return RC::UNSUPPORTED_DEVICE;
                }

                params.version = LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
                params.engineType = partnerParams.partnerList[0];
                pParams = &params;
            }

            CHECK_RC(m_Config.pLwRm->Alloc(
                    ch.pCh->GetHandle(), &subch.hClass, subch.classId, pParams));
            CHECK_RC(ch.pCh->SetObject(subch.number, subch.hClass));
        }
    }

    return rc;
}

RC MfgTracePlayer::Engine::FreeChannels()
{
    StickyRC rc;

    for (UINT32 chId=0; chId < m_Channels.size(); chId++)
    {
        Channel& ch = m_Channels[chId];
        if (ch.pCh)
        {
            Printf(m_Pri, "Freeing channel %s\n", ch.name.c_str());
            for (UINT32 subchId=0; subchId < ch.subchannels.size(); subchId++)
            {
                SubChannel& subch = ch.subchannels[subchId];
                m_Config.pLwRm->Free(subch.hClass);
            }
            rc = m_Config.pTestConfig->FreeChannel(ch.pCh);
            ch.pCh = nullptr;
        }
    }

    return rc;
}

void MfgTracePlayer::Engine::PrintStats()
{
    Printf(m_Pri, "Statistics: Alloc+Load=%llums Free=%llums WFI=%llums CRC=%llums Other=%llums\n",
           m_Stats.alloc,
           m_Stats.free,
           m_Stats.wfi,
           m_Stats.crc,
           m_Stats.total
                - m_Stats.alloc
                - m_Stats.free
                - m_Stats.wfi
                - m_Stats.crc);
}

RC MfgTracePlayer::Engine::IdleChannels()
{
    Timer timer(&m_Stats.wfi);
    RC rc;
    for (UINT32 chId=0; chId < m_Channels.size(); chId++)
    {
        auto* const pCh = m_Channels[chId].pCh;
        if (pCh)
        {
            CHECK_RC(pCh->WaitIdle(m_Config.defTimeoutMs * (m_WfiCount+1)));
        }
    }
    m_WfiCount = 0;
    return rc;
}

RC MfgTracePlayer::Engine::Find3DClass(LwRm* pLwRm, GpuDevice* pGpuDevice, UINT32* pClassId)
{
    // Please add new class in s_ClassMap at the top.

    const UINT32 numClasses = sizeof(s_ClassMap) / sizeof(s_ClassMap[0]);
    UINT32 classId = 0;
    for (UINT32 idx=0; idx < numClasses; idx++)
    {
        if (s_ClassMap[idx].engineType != engine3D)
        {
            continue;
        }
        const UINT32 lwrClassId = s_ClassMap[idx].classId;
        UINT32 supp = 0;
        if (OK == pLwRm->GetFirstSupportedClass(1, &lwrClassId, &supp, pGpuDevice))
        {
            classId = supp;
            break;
        }
    }
    if (classId == 0)
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    *pClassId = classId;
    return OK;
}

void MfgTracePlayer::Engine::FreeSurface(UINT32 surfId, WhatToFree freeVA)
{
    MASSERT(surfId < m_Surfaces.size());
    Surface& surf = m_Surfaces[surfId];

    if ((!surf.pSurfVA.get() || !surf.pSurfVA->IsAllocated()) &&
        (!surf.pSurfPA.get() || !surf.pSurfPA->IsAllocated()) &&
        (!surf.pSurfMap.get() || !surf.pSurfMap->IsAllocated()))
    {
        return;
    }

    // We have to preserve certain surface properties which are
    // set by the parser, in case the trace is looped and
    // the surface is allocated again, because Free() will
    // destroy/reset these settings.

    Surface2D* const pOrigSurf = surf.pSurfVA.get() ? surf.pSurfVA.get() :
                                 surf.pSurfMap.get() ? surf.pSurfMap.get() :
                                 surf.pSurfPA.get();

    const UINT32           type       = pOrigSurf->GetType();
    const Memory::Location location   = pOrigSurf->GetLocation();
    const Memory::Protect  protect    = pOrigSurf->GetProtect();
    const UINT64           arrayPitch = pOrigSurf->GetArrayPitch();
    const UINT64           virtAddr   = pOrigSurf->GetGpuVirtAddrHint();
    const UINT64           physAddr   = pOrigSurf->GetGpuPhysAddrHint();

    if (!CacheSurfaces())
    {
        if (surfId == m_DisplaySurfId)
        {
            MASSERT(m_Config.pDisplayMgrTestContext);
            m_Config.pDisplayMgrTestContext->DisplayImage(
                static_cast<Surface2D*>(nullptr), DisplayMgr::WAIT_FOR_UPDATE);
            m_DisplaySurfId = ~0U;
        }

        m_TotalMem     -= surf.actualSize;
        surf.actualSize = 0U;
        Printf(Tee::PriDebug, "Freeing surface %s (total 0x%llx bytes)\n",
               surf.name.c_str(), m_TotalMem);
        if (surf.pSurfMap.get())
        {
            surf.pSurfMap->Free();
        }
        if (surf.pSurfPA.get())
        {
            surf.pSurfPA->Free();
        }
        if (freeVA && surf.pSurfVA.get())
        {
            surf.pSurfVA->Free();
        }
    }

    ForEachSurface2D(surf, [=](Surface2D* pSurf)
    {
        pSurf->SetType(type);
        pSurf->SetLocation(location);
        pSurf->SetProtect(protect);
        pSurf->SetArrayPitch(arrayPitch);
        pSurf->SetGpuVirtAddrHint(virtAddr);
        pSurf->SetGpuPhysAddrHint(physAddr);
    });
}

UINT64 MfgTracePlayer::Engine::GetPageSize(const Surface& surf) const
{
    switch (DRF_VAL(OS32, _ATTR, _PAGE_SIZE, surf.attr))
    {
        case LWOS32_ATTR_PAGE_SIZE_BIG:
            return 64_KB;

        case LWOS32_ATTR_PAGE_SIZE_HUGE:
            return 2_MB;

        default:
            break;
    }
    return 4_KB;
}

bool MfgTracePlayer::Engine::CacheSurfaces() const
{
    return m_Config.cacheSurfaces && ! m_HasAllocAfterFree;
}

bool MfgTracePlayer::Engine::IsSupported() const
{
    MASSERT(m_State == traceLoaded);

    for (UINT32 chId=0; chId < m_Channels.size(); chId++)
    {
        auto& ch = m_Channels[chId];
        for (UINT32 subchId=0; subchId < ch.subchannels.size(); subchId++)
        {
            auto& subch = ch.subchannels[subchId];
            UINT32 supp = 0;
            if (OK != m_Config.pLwRm->GetFirstSupportedClass(
                        1, &subch.classId, &supp, m_Config.pGpuDevice)
                || supp != subch.classId)
            {
                Printf(m_Pri,
                       "Trace not supported on this GPU: class 0x%4x is not supported\n",
                       subch.classId);
                return false;
            }
        }
    }
    return true;
}

RC MfgTracePlayer::Engine::AllocResources()
{
    MASSERT(m_State == traceLoaded);

    Printf(m_Pri, "Trace player: Allocating resources\n");

    // Add some slack so we can allocate channels etc. and then
    // round up the end of VA space
    constexpr UINT64 slack        = 128_MB;
    constexpr UINT64 vasAlignment = 128_MB;
    const UINT64 endVa = Utility::AlignUp(m_EndVa + slack, vasAlignment);

    Printf(Tee::PriLow, "Last VA 0x%llx, requesting VA space size 0x%llx\n", m_EndVa, endVa);

    RC rc;

    LW_VASPACE_ALLOCATION_PARAMETERS params = { };
    params.vaSize = endVa;
    CHECK_RC(m_Config.pLwRm->Alloc(
                m_Config.pLwRm->GetDeviceHandle(m_Config.pGpuDevice),
                &m_hVASpace,
                FERMI_VASPACE_A,
                &params));

    Printf(Tee::PriLow, "Actual VA space limit is 0x%llx\n", params.vaSize);
    if (m_EndVa > params.vaSize)
    {
        Printf(Tee::PriError, "Maximum VA needed is 0x%llx, but VA space limit is 0x%llx\n",
               m_EndVa, params.vaSize);
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    // Immediately after the VA space object is allocated, we allocate VAs for fixed-address
    // surfaces, in order to reserve them to avoid collisions with non-fixed address
    // surfaces, such as channels, semaphores or GPFIFOs.
    CHECK_RC(AllocSurfaceVAs());

    m_pSurfaceFiller = make_unique<SurfaceUtils::OptimalSurfaceFiller>(m_Config.pGpuDevice);
    m_pSurfaceFiller->SetAutoWait(false);

    CHECK_RC(AllocChannels());

    m_SurfaceAccessor.Setup(m_Config.pTestConfig,
                            m_Config.pLwRm,
                            m_Config.pGpuDevice,
                            m_hVASpace,
                            m_Config.surfaceAccessorSizeMB);

    // Allocate dummy semaphore used for WFI command
    m_WfiSema.SetWidth(s_SemSize * static_cast<UINT32>(m_Channels.size()));
    m_WfiSema.SetHeight(1);
    m_WfiSema.SetPageSize(4);
    m_WfiSema.SetColorFormat(ColorUtils::Y8);
    m_WfiSema.SetLocation(Memory::Coherent);
    m_WfiSema.SetGpuVASpace(m_hVASpace);
    m_WfiSema.SetVASpace(Surface2D::GPUVASpace);
    CHECK_RC(m_WfiSema.Alloc(m_Config.pGpuDevice, m_Config.pLwRm));
    for (UINT32 chId=0; chId < m_Channels.size(); chId++)
    {
        CHECK_RC(m_WfiSema.BindGpuChannel(m_Channels[chId].pCh->GetHandle()));
    }

    m_State = resourcesAllocated;

    return rc;
}

RC MfgTracePlayer::Engine::CalcAndCheckSurfaceSizes()
{
    using VAMap = map<UINT64, const Surface*>;
    VAMap mapVAToSurf;

    for (Surface& surf : m_Surfaces)
    {
        Printf(Tee::PriDebug, "Callwlating size for surface %s\n", surf.name.c_str());

        // Get size of the surface from file
        size_t size = 0;
        if (!surf.file.empty())
        {
            RC rc = m_Loader.GetFileSize(surf.file, &size);
            // If the uncompressed file does not exist, check if the compressed version exists
            if (rc != RC::OK)
            {
                rc.Clear();
                rc = m_Loader.GetEncryptedFileSize(surf.file + ".e", &size);
                if (rc != RC::OK)
                {
                    rc.Clear();
                    rc = m_Loader.GetFileSize(surf.file + ".lz4", &size);
                    if (rc != RC::OK)
                    {
                        rc.Clear();
                        rc = m_Loader.GetEncryptedFileSize(surf.file + ".lz4e", &size);
                        if (rc == RC::FILE_DOES_NOT_EXIST)
                        {
                            Printf(Tee::PriError, "File \"%s\" not found\n",
                                   surf.file.c_str());
                        }
                        CHECK_RC(rc);
                    }
                }
            }

            ForEachSurface2D(surf, [size](Surface2D* pSurf)
            {
                pSurf->SetArrayPitch(UNSIGNED_CAST(UINT64, size));
            });
        }

        if (!surf.pSurfVA.get())
        {
            if (surf.pSurfMap.get())
            {
                RC rc;
                CHECK_RC(CheckMapOffsets(surf));
            }
            continue;
        }

        // Track the range of VAs used by the trace
        const UINT64 begilwa = surf.pSurfVA->GetGpuVirtAddrHint();
        const UINT64 endVa   = begilwa + surf.pSurfVA->GetArrayPitch();
        if (begilwa < s_LowestVA)
        {
            Printf(Tee::PriError, "VA 0x%llx for surface %s is too low, must be at least 0x%llx\n",
                   begilwa, surf.name.c_str(), s_LowestVA);
            return RC::BAD_TRACE_DATA;
        }
        if (!surf.pSurfVA->GetArrayPitch())
        {
            Printf(Tee::PriError, "Surface %s size is 0\n", surf.name.c_str());
            return RC::BAD_TRACE_DATA;
        }

        m_Begilwa = min(begilwa, m_Begilwa);
        m_EndVa   = max(endVa,   m_EndVa);

        // Check whether surface base is aligned on page size
        const UINT64 pageSize = GetPageSize(surf);
        if (begilwa & (pageSize - 1))
        {
            Printf(Tee::PriError,
                   "Surface %s start address 0x%llx is not aligned on page size 0x%llx\n",
                   surf.name.c_str(), begilwa, pageSize);
            return RC::BAD_TRACE_DATA;
        }

        // Check whether this surface overlaps any other surface
        // We want to do this check in the trace player, because if there is
        // any overlap, RM would just crash.
        const auto ReportOverlap = [](const Surface& s1, const Surface& s2)
        {
            Printf(Tee::PriError, "Surfaces %s and %s overlap\n",
                   s1.name.c_str(), s2.name.c_str());
        };
        const auto nextIt = mapVAToSurf.lower_bound(begilwa);
        if (nextIt != mapVAToSurf.end())
        {
            const UINT64 alignedEndVa = ALIGN_UP(endVa, pageSize);
            if (alignedEndVa > nextIt->first)
            {
                ReportOverlap(*nextIt->second, surf);
                return RC::BAD_TRACE_DATA;
            }
        }
        auto prevIt = nextIt;
        if (prevIt != mapVAToSurf.begin())
        {
            --prevIt;
        }
        if (prevIt != mapVAToSurf.begin())
        {
            const Surface& otherSurf       = *prevIt->second;
            const UINT64   otherEnd        = prevIt->first + otherSurf.pSurfVA->GetArrayPitch();
            const UINT64   otherPageSize   = GetPageSize(otherSurf);
            const UINT64   alignedOtherEnd = ALIGN_UP(otherEnd, otherPageSize);
            if (alignedOtherEnd > begilwa)
            {
                ReportOverlap(otherSurf, surf);
                return RC::BAD_TRACE_DATA;
            }
        }
        mapVAToSurf.emplace_hint(nextIt, VAMap::value_type(begilwa, &surf));
    }

    return RC::OK;
}

RC MfgTracePlayer::Engine::CheckMapOffsets(const Surface& surf) const
{
    MASSERT(surf.pSurfMap.get());
    MASSERT(!surf.pSurfVA.get());
    MASSERT(!surf.pSurfPA.get());

    MASSERT(surf.virtId < m_Surfaces.size());
    MASSERT(surf.physId < m_Surfaces.size());

    const Surface& surfVA = m_Surfaces[surf.virtId];
    const Surface& surfPA = m_Surfaces[surf.physId];

    MASSERT(surfVA.pSurfVA.get());
    MASSERT(surfPA.pSurfPA.get());

    const UINT64 vaSize  = surfVA.pSurfVA->GetArrayPitch();
    const UINT64 paSize  = surfPA.pSurfPA->GetArrayPitch();
    const UINT64 mapSize = surf.pSurfMap->GetArrayPitch();

    const UINT64 mapPageSize = GetPageSize(surf);
    const UINT64 vaPageSize  = GetPageSize(surfVA);
    const UINT64 paPageSize  = GetPageSize(surfPA);

    if (mapPageSize != vaPageSize)
    {
        Printf(Tee::PriError, "Page size %uKB for map %s does not match page size %uKB for virtual allocation %s\n",
               static_cast<unsigned>(mapPageSize / 1_KB), surf.name.c_str(),
               static_cast<unsigned>(vaPageSize / 1_KB), surfVA.name.c_str());
        return RC::BAD_TRACE_DATA;
    }

    if (mapPageSize != paPageSize)
    {
        Printf(Tee::PriError, "Page size %uKB for map %s does not match page size %uKB for physical allocation %s\n",
               static_cast<unsigned>(mapPageSize / 1_KB), surf.name.c_str(),
               static_cast<unsigned>(paPageSize / 1_KB), surfPA.name.c_str());
        return RC::BAD_TRACE_DATA;
    }

    if (surf.virtOffs & (mapPageSize - 1))
    {
        Printf(Tee::PriError,
               "Map %s virtual offset 0x%llx is not aligned on page size 0x%llx\n",
               surf.name.c_str(), surf.virtOffs, mapPageSize);
        return RC::BAD_TRACE_DATA;
    }

    if (surf.physOffs & (mapPageSize - 1))
    {
        Printf(Tee::PriError,
               "Map %s virtual offset 0x%llx is not aligned on page size 0x%llx\n",
               surf.name.c_str(), surf.physOffs, mapPageSize);
        return RC::BAD_TRACE_DATA;
    }

    if ((surf.virtOffs > vaSize) || (surf.virtOffs + mapSize > vaSize))
    {
        Printf(Tee::PriError, "Virtual offset 0x%llx for map %s size 0x%llx exceeds virtual allocation's %s size 0x%llx\n",
               surf.virtOffs, surf.name.c_str(), mapSize, surfVA.name.c_str(), vaSize);
        return RC::BAD_TRACE_DATA;
    }

    if ((surf.physOffs > paSize) || (surf.physOffs + mapSize > paSize))
    {
        Printf(Tee::PriError, "Physical offset 0x%llx for map %s size 0x%llx exceeds physical allocation's %s size 0x%llx\n",
               surf.physOffs, surf.name.c_str(), mapSize, surfPA.name.c_str(), paSize);
        return RC::BAD_TRACE_DATA;
    }

    return RC::OK;
}

RC MfgTracePlayer::Engine::AllocSurfaceVAs()
{
    for (Surface& surf : m_Surfaces)
    {
        // Setup Surface2D objects
        const UINT32 hVASpace = m_hVASpace;
        ForEachSurface2D(surf, [&surf, hVASpace](Surface2D* pSurf)
        {
            pSurf->ConfigFromAttr(surf.attr);
            pSurf->ConfigFromAttr2(surf.attr2);
            pSurf->SetGpuVASpace(hVASpace);
            pSurf->SetVASpace(Surface2D::GPUVASpace);
            pSurf->SetColorFormat(ColorUtils::Y8); // Set dummy color format
            pSurf->SetDisplayable(surf.displayable);
        });

        if (!surf.pSurfVA.get())
        {
            continue;
        }

        const UINT64 virtAddr = surf.pSurfVA->GetGpuVirtAddrHint();
        Printf(Tee::PriDebug,
               "Reserving VA 0x%llx for surface %s, 0x%llx bytes\n",
               virtAddr, surf.name.c_str(), surf.pSurfVA->GetArrayPitch());

        // Prevent RM from changing how the VA is allocated due to internal alignment
        surf.pSurfVA->SetVirtAlignment(1);

        // Reserve VA for the surface
        RC rc;
        CHECK_RC(surf.pSurfVA->Alloc(m_Config.pGpuDevice, m_Config.pLwRm));

        // Verify requested virtual address
        const UINT64 actualVirtAddr = surf.pSurfVA->GetCtxDmaOffsetGpu();
        if (virtAddr && (actualVirtAddr != virtAddr))
        {
            Printf(Tee::PriError,
                   "Requested surface VA 0x%llx but allocated at 0x%llx for surface %s\n",
                   virtAddr, actualVirtAddr, surf.name.c_str());
            return RC::ILWALID_ADDRESS;
        }
    }

    return RC::OK;
}

RC MfgTracePlayer::Engine::PlayTrace(vector<Error>* pErrors)
{
    MASSERT(pErrors);
    MASSERT(m_State == resourcesAllocated ||
            m_State == played);

    Tasker::DetachThread detach;

    Printf(m_Pri, "Trace player: Playing\n");

    if (CacheSurfaces())
    {
        Printf(m_Pri, "Surface caching: enabled\n");
    }
    else if (m_Config.cacheSurfaces && m_HasAllocAfterFree)
    {
        Printf(m_Pri, "Surface caching: disabled due to alloc after free\n");
    }
    else
    {
        Printf(m_Pri, "Surface caching: disabled\n");
    }

    // Traces like to leak surfaces which they allocate dynamically.
    // So free any dynamic surfaces that were allocated in the previous invocation.
    RC rc;
    CHECK_RC(IdleChannels());
    FreeSurfaces();

    // Ensure that channels are always allocated.
    // When VA spaces are in use, channels could have been freed at the end
    // of previous loop.
    CHECK_RC(AllocChannels());

    m_Errors.clear();
    pErrors->clear();

    m_Stats = Stats();

    {
        Timer timer(&m_Stats.total);
        for (const auto& cmd : m_Commands)
        {
            CHECK_RC(cmd());
        }
    }

    CHECK_RC(FreeChannels());

    PrintStats();

    pErrors->swap(m_Errors);
    m_State = played;

    return pErrors->empty() ? rc : RC::GOLDEN_VALUE_MISCOMPARE;
}

RC MfgTracePlayer::Engine::FreeResources()
{
    if (m_State == init ||
        m_State == traceLoaded)
    {
        return OK;
    }

    Printf(m_Pri, "Trace player: Freeing resources\n");

    // Remove all event handlers
    for (auto& event : m_Events)
    {
        event.handlers.clear();
    }

    StickyRC rc = IdleChannels();

    // Temporarily disable surface caching to force all surfaces
    // to be really freed.
    Utility::TempValue<bool> disableCaching(m_Config.cacheSurfaces, false);

    FreeSurfaces();

    m_WfiSema.Free();

    rc = m_SurfaceAccessor.Free();

    rc = FreeChannels();

    rc = m_pSurfaceFiller->Cleanup();
    m_pSurfaceFiller.reset();

    m_Config.pLwRm->Free(m_hVASpace);
    m_hVASpace = 0U;

    m_State = traceLoaded;

    return rc;
}

void MfgTracePlayer::Engine::AddAllocSurface(UINT32 surfId)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());

    Surface& surf = m_Surfaces[surfId];

    // Skip this command if the surface is a pure VA allocation
    // and does not have physical memory or is not a mapping,
    // because in this case there is nothing to allocate.
    if (!surf.pSurfPA.get() && !surf.pSurfMap.get())
    {
        return;
    }

    if (m_HasFree)
    {
        m_HasAllocAfterFree = true;
    }

    m_NeedFinish = true;

    m_Commands.emplace_back([=]() -> RC
    {
        Surface& surf = this->m_Surfaces[surfId];

        Timer timer(&this->m_Stats.alloc);

        const bool isAllocated = (surf.pSurfPA.get() && surf.pSurfPA->IsAllocated()) ||
                                 (surf.pSurfMap.get() && surf.pSurfMap->IsAllocated());

        if (! CacheSurfaces() || ! isAllocated)
        {
            RC rc;

            // Allocate physical memory
            if (surf.pSurfPA.get())
            {
                const UINT64 pageSize   = GetPageSize(surf);
                const UINT64 actualSize = ALIGN_UP(surf.pSurfPA->GetArrayPitch(), pageSize);
                surf.actualSize         = actualSize;
                this->m_TotalMem       += actualSize;

                Printf(Tee::PriDebug,
                       "Allocating memory for surface %s, 0x%llx bytes (total 0x%llx bytes)\n",
                       surf.name.c_str(), surf.pSurfPA->GetArrayPitch(), this->m_TotalMem);

                CHECK_RC(surf.pSurfPA->Alloc(this->m_Config.pGpuDevice,
                                             this->m_Config.pLwRm));
            }

            // Map the physical memory into a VA
            if (surf.pSurfMap.get())
            {
                const UINT32 virtId = surf.virtId;
                const UINT32 physId = surf.physId;

                MASSERT(virtId < this->m_Surfaces.size());
                MASSERT(physId < this->m_Surfaces.size());

                Surface& surfVA = (surfId == virtId) ? surf : this->m_Surfaces[virtId];
                Surface& surfPA = (surfId == physId) ? surf : this->m_Surfaces[physId];

                const UINT64 virtAddr = surfVA.pSurfVA->GetCtxDmaOffsetGpu() + surf.virtOffs;

                if ((surfId != virtId) || (surfId != physId))
                {
                    Printf(Tee::PriDebug,
                           "Mapping surface %s at 0x%llx in virt alloc %s from phys alloc %s, 0x%llx bytes\n",
                           surf.name.c_str(), virtAddr, surfVA.name.c_str(),
                           surfPA.name.c_str(), surf.pSurfMap->GetArrayPitch());
                }
                else
                {
                    Printf(Tee::PriDebug,
                           "Mapping surface %s at 0x%llx, 0x%llx bytes\n",
                           surf.name.c_str(), virtAddr, surf.pSurfMap->GetArrayPitch());
                }

                CHECK_RC(surf.pSurfMap->MapVirtToPhys(this->m_Config.pGpuDevice,
                                                      surfVA.pSurfVA.get(),
                                                      surfPA.pSurfPA.get(),
                                                      surf.virtOffs,
                                                      surf.physOffs,
                                                      this->m_Config.pLwRm,
                                                      true));

                // Check mapped VA
                const UINT64 actualVirtAddr = surf.pSurfMap->GetCtxDmaOffsetGpu();
                if (actualVirtAddr != virtAddr)
                {
                    Printf(Tee::PriError,
                           "Requested surface VA 0x%llx but mapped at 0x%llx for surface %s\n",
                           virtAddr, actualVirtAddr, surf.name.c_str());
                    return RC::ILWALID_ADDRESS;
                }

                // Load the surface from file
                if (!surf.file.empty())
                {
                    Printf(this->m_Pri, "Loading surface %s from file %s\n",
                           surf.name.c_str(), surf.file.c_str());

                    rc = this->m_Loader.LoadFile(surf.file,
                                                 surf.pSurfMap.get(),
                                                 &m_SurfaceAccessor);
                    // If the uncompressed file does not exist, check if the compressed version exists
                    if (rc != RC::OK)
                    {
                        rc.Clear();
                        rc = this->m_Loader.LoadEncryptedFile(surf.file + ".e",
                                                              surf.pSurfMap.get(),
                                                              &m_SurfaceAccessor);
                        if (rc != RC::OK)
                        {
                            rc.Clear();
                            rc = this->m_Loader.LoadFile(surf.file + ".lz4",
                                                         surf.pSurfMap.get(),
                                                         &m_SurfaceAccessor);
                            if (rc != RC::OK)
                            {
                                rc.Clear();
                                rc = this->m_Loader.LoadEncryptedFile(surf.file + ".lz4e",
                                                                      surf.pSurfMap.get(),
                                                                      &m_SurfaceAccessor);
                                if (rc == RC::FILE_DOES_NOT_EXIST)
                                {
                                    Printf(Tee::PriError, "File \"%s\" not found\n",
                                           surf.file.c_str());
                                }
                                CHECK_RC(rc);
                            }
                        }
                    }
                }
            }
        }
        else if (!surf.file.empty())
        {
            Printf(this->m_Pri, "Reusing cached surface %s\n", surf.name.c_str());
        }

        // Always fill non-loadable surface, even if caching is enabled
        RC rc;
        if (surf.file.empty() && surf.pSurfMap.get())
        {
            CHECK_RC(this->m_pSurfaceFiller->Fill08(surf.pSurfMap.get(), surf.fill));
        }

        CHECK_RC(Abort::Check());

        return OK;
    });
}

void MfgTracePlayer::Engine::AddFinishSurfaces()
{
    if (m_NeedFinish)
    {
        m_Commands.emplace_back([=]() -> RC
        {
            return this->m_pSurfaceFiller->Wait();
        });
        this->m_NeedFinish = false;
    }
}

void MfgTracePlayer::Engine::AddFreeSurface(UINT32 surfId)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());

    m_HasFree = true;

    AddFinishSurfaces();

    m_Commands.emplace_back([=]() -> RC
    {
        Surface& surf = this->m_Surfaces[surfId];

        if ((surf.pSurfVA.get() && surf.pSurfVA->IsAllocated()) ||
            (surf.pSurfPA.get() && surf.pSurfPA->IsAllocated()) ||
            (surf.pSurfMap.get() && surf.pSurfMap->IsAllocated()))
        {
            RC rc;
            CHECK_RC(IdleChannels());
            Timer timer(&this->m_Stats.free);
            FreeSurface(surfId, DontFreeVa);
        }
        return OK;
    });
}

void MfgTracePlayer::Engine::AddGpEntry(UINT32 chId, UINT32 surfId, UINT32 offset, UINT32 size)
{
    MASSERT(m_State == init);
    MASSERT(surfId < m_Surfaces.size());

    m_Surfaces[surfId].pushbuffer = true;
    MASSERT(m_Surfaces[surfId].pSurfMap.get());

    AddFinishSurfaces();

    m_Commands.emplace_back([=]() -> RC
    {
        MASSERT(surfId < this->m_Surfaces.size());
        MASSERT(chId < this->m_Channels.size());

        if (size == 0)
        {
            return OK;
        }

        Timer timer(&this->m_Stats.wfi);

        Printf(Tee::PriDebug, "GPENTRY chId=0x%x surfId=0x%x offset=0x%08x size=0x%08x\n",
                              chId, surfId, offset, size);

        const Surface& surf = this->m_Surfaces[surfId];
        MASSERT(surf.pushbuffer);
        const UINT64 virtAddr = surf.pSurfMap->GetCtxDmaOffsetGpu() + offset;

        RC rc;
        const Channel& ch = this->m_Channels[chId];
        CHECK_RC(ch.pCh->CallSubroutine(virtAddr, size));
        CHECK_RC(ch.pCh->Flush());
        return rc;
    });
}

void MfgTracePlayer::Engine::AddWaitForIdle(vector<UINT32> chIds)
{
    MASSERT(m_State == init);

    AddFinishSurfaces();

    m_Commands.emplace_back([=]() -> RC
    {
        Timer timer(&this->m_Stats.wfi);

        string strChIds;
        for (UINT32 chId : chIds)
        {
            strChIds += Utility::StrPrintf(" chId=0x%x", chId);
        }
        Printf(Tee::PriDebug, "WAIT_FOR_IDLE%s\n", strChIds.c_str());

        const UINT32 data = ++this->m_WfiCount;

        const auto GetSemOffs = [&](UINT32 chId) -> UINT64
        {
            return this->m_WfiSema.GetCtxDmaOffsetGpu(this->m_Config.pLwRm)
                   + s_SemSize * chId;
        };

        const UINT32 chId0    = chIds[0];
        auto&        ch0      = *this->m_Channels[chId0].pCh;
        UINT64       sem0Offs = GetSemOffs(chId0);
        RC           rc;

        // First channel will:
        //  1. Wait for all other channels to release semaphore
        //  2. Release its own semaphore with WFI
        // All other channels will:
        //  1. Release their own semaphore with WFI
        //  2. Wait for the first channel to release semaphore
        // The first channel serves as a gateway to make sure that WFI has
        // been performed on all selected channels before proceeding.

        for (UINT32 ich = 1; ich < chIds.size(); ich++)
        {
            const UINT32 chId = chIds[ich];

            MASSERT(chId < this->m_Channels.size());
            auto& ch = *this->m_Channels[chId].pCh;

            const UINT64 semOffs = GetSemOffs(chId);

            // Perform semaphore release with WFI on all selected channels
            // except the first one
            ch.SetSemaphoreReleaseFlags(::Channel::FlagSemRelWithWFI);
            CHECK_RC(ch.SetSemaphoreOffset(semOffs));
            CHECK_RC(ch.SemaphoreRelease(data));

            // Channels other than the first one will wait for the first
            // channel to write the semaphore value
            CHECK_RC(ch.SetSemaphoreOffset(sem0Offs));
            CHECK_RC(ch.SemaphoreAcquireGE(data));

            // The first channel will wait for all other channels
            // to write the semaphore value
            CHECK_RC(ch0.SetSemaphoreOffset(semOffs));
            CHECK_RC(ch0.SemaphoreAcquireGE(data));
        }

        // Finally, release the semaphore on the first channel
        // to let all the channels run
        ch0.SetSemaphoreReleaseFlags(::Channel::FlagSemRelWithWFI);
        CHECK_RC(ch0.SetSemaphoreOffset(sem0Offs));
        CHECK_RC(ch0.SemaphoreRelease(data));

        // Flush all semaphore methods
        for (UINT32 ich = 0; ich < chIds.size(); ich++)
        {
            const UINT32 chId = chIds[ich];

            MASSERT(chId < this->m_Channels.size());
            auto& ch = *this->m_Channels[chId].pCh;

            CHECK_RC(ch.Flush());
        }

        if (this->m_Config.cpuWfi)
        {
            CHECK_RC(ch0.WaitIdle(this->m_Config.defTimeoutMs));
        }

        return RC::OK;
    });
}

void MfgTracePlayer::Engine::AddEvent(UINT32 eventId)
{
    MASSERT(m_State == init);

    AddFinishSurfaces();

    m_Commands.emplace_back([=]() -> RC
    {
        MASSERT(eventId < this->m_Events.size());
        Event* pEvent = &this->m_Events[eventId];
        RC rc;

        // Ensure drawing is complete before ilwoking any handlers.
        // This is important esp. for DumpSurfaceOnEvent(), where we want
        // to dump the surface after it's been completed, without requiring
        // WAIT_FOR_IDLE in the trace, which also requires StrictWFI to
        // work properly.
        if (!pEvent->handlers.empty())
        {
            CHECK_RC(IdleChannels());
        }

        // Call the callbacks that were registered by RegisterEventHandler()
        for (const auto& handler : pEvent->handlers)
        {
            CHECK_RC(handler());
        }
        return OK;
    });
}

RC MfgTracePlayer::Engine::AddCheckSurface(UINT32 surfId, const string& suffix)
{
    MASSERT(m_State == init);

    MASSERT(surfId < m_Surfaces.size());
    Surface& surf = m_Surfaces[surfId];
    MASSERT(surf.pSurfMap.get());

    const UINT32 suffixId = suffix.empty() ? ~0U : static_cast<UINT32>(surf.crcSuffixes.size());
    if (!suffix.empty())
    {
        surf.crcSuffixes.emplace_back(suffix);
    }

    // Check for existence of CRC, so we can fail early without actually
    // running the trace, if the CRC is missing.
    UINT32 crc = 0;
    RC rc;
    CHECK_RC(m_CrcChain.GetCrc(surf.crc, suffix + GetCrcSuffix(), &crc));

    AddFinishSurfaces();

    m_Commands.emplace_back([=]() -> RC
    {
        MASSERT(surfId < this->m_Surfaces.size());
        const Surface& surf = this->m_Surfaces[surfId];
        MASSERT(suffixId == ~0U || suffixId < surf.crcSuffixes.size());

        RC rc;
        CHECK_RC(IdleChannels());
        CHECK_RC(FlushSocGpu());

        Timer timer(&this->m_Stats.crc);

        string suffix;
        if (suffixId != ~0U)
        {
            suffix += surf.crcSuffixes[suffixId];
        }
        suffix += GetCrcSuffix();

        UINT32 expectedCrc = 0;
        CHECK_RC(this->m_CrcChain.GetCrc(surf.crc, suffix, &expectedCrc));

        const UINT32 size = (surf.crcBegin < surf.crcEnd) ? (surf.crcEnd - surf.crcBegin)
            : static_cast<UINT32>(surf.pSurfMap->GetSize());

        MASSERT((size % sizeof(UINT32)) == 0);

        vector<UINT32> crcData;
        crcData.resize(size/sizeof(UINT32));
        CHECK_RC(m_SurfaceAccessor.ReadBytes(surf.pSurfMap.get(),
                                             0,
                                             surf.crcBegin,
                                             &crcData[0],
                                             size));

        UINT32 crc = 0;
        {
            const UINT08* srcPtr = reinterpret_cast<UINT08*>(&crcData[0]);

            // Apply CRC mask or skip CRC frames, if set
            if (this->m_Config.crcTolerance > 0 || !this->m_Config.skipCRCFrames.empty())
            {
                if (this->m_Config.crcTolerance > 0)
                {
                    const UINT32 crcMask = ~0U << this->m_Config.crcTolerance;
                    for (UINT32 i=0; i < crcData.size(); i++)
                    {
                        crcData[i] &= crcMask;
                    }
                }
                if (!this->m_Config.skipCRCFrames.empty())
                {
                    for (const auto& index : this->m_Config.skipCRCFrames)
                    {
                        if (index < crcData.size())
                        {
                            crcData[index] = 0;
                        }
                    }
                }
            }

            crc = CalcCRC(srcPtr, size);
        }

        Printf(this->m_Pri,
               "Surface %s CRC: 0x%08x (expected=0x%08x)\n",
               surf.name.c_str(), crc, expectedCrc);

        if (crc != expectedCrc)
        {
            Printf(Tee::PriError,
                   "CRC check failed for surface %s: expected=0x%08x, actual=0x%08x\n",
                   surf.name.c_str(), expectedCrc, crc);

            const Error error = { surf.name, 0, expectedCrc, crc };
            this->m_Errors.push_back(error);
        }
        return rc;
    });

    return rc;
}

void MfgTracePlayer::Engine::AddDisplayImage
(
    UINT32             surfId,
    UINT32             offset,
    ColorUtils::Format colorFormat,
    Surface2D::Layout  layout,
    UINT32             width,
    UINT32             height,
    UINT32             pitch,
    UINT32             logBlockHeight,
    UINT32             numBlocksWidth,
    UINT32             aaSamples
)
{
    MASSERT(m_State == init);

    MASSERT(surfId < m_Surfaces.size());
    m_Surfaces[surfId].displayable = true;
    MASSERT(m_Surfaces[surfId].pSurfMap.get());

    AddFinishSurfaces();

    m_Commands.emplace_back([=]() -> RC
    {
        if (this->m_Config.pDisplayMgrTestContext == nullptr)
        {
            return OK;
        }

        MASSERT(surfId < this->m_Surfaces.size());
        const Surface& surf = this->m_Surfaces[surfId];

        if (offset >= surf.pSurfMap->GetSize())
        {
            Printf(Tee::PriError, "DISPLAY_IMAGE offset 0x%x is outside of the surface!",
                   offset);
            return RC::BAD_TRACE_DATA;
        }

        GpuUtility::DisplayImageDescription desc;
        desc.hMem           = surf.pSurfMap->GetMemHandle();
        desc.CtxDMAHandle   = surf.pSurfMap->GetCtxDmaHandleIso();
        desc.Offset         = surf.pSurfMap->GetCtxDmaOffsetIso() + offset;
        desc.Height         = height;
        desc.Width          = width;
        desc.AASamples      = aaSamples;
        desc.Layout         = layout;
        desc.Pitch          = pitch;
        desc.LogBlockHeight = logBlockHeight;
        desc.NumBlocksWidth = numBlocksWidth;
        desc.ColorFormat    = colorFormat;

        RC rc;
        CHECK_RC(m_Config.pDisplayMgrTestContext->DisplayImage(&desc,
            DisplayMgr::DONT_WAIT_FOR_UPDATE));

        this->m_DisplaySurfId = surfId;

        if (this->m_Config.displayPauseMs)
        {
            Tasker::Sleep(this->m_Config.displayPauseMs);
        }
        return rc;
    });
}

RC MfgTracePlayer::Engine::FlushSocGpu()
{
    // The sysmembar flush is only needed for SOC GPUs
    if (!m_Config.pGpuDevice->GetSubdevice(0)->IsSOC())
    {
        return OK;
    }

    if (m_Channels.empty())
    {
        return OK;
    }

    if (m_Channels[0].subchannels.empty())
    {
        return OK;
    }

    // We can do this on any channel/subchannel - just pick the first one
    auto* const pCh = m_Channels[0].pCh;
    RC rc;

    // Flush GPU cache
    CHECK_RC(pCh->WriteL2FlushDirty());

    CHECK_RC(IdleChannels());

    return rc;
}

string MfgTracePlayer::Engine::GetCrcSuffix() const
{
    string suffix;
    if (m_Config.crcTolerance > 0)
    {
        MASSERT(m_Config.crcTolerance < 32);
        suffix += Utility::StrPrintf("_tol%u\n", m_Config.crcTolerance);
    }
    if (!m_Config.skipCRCFrames.empty())
    {
        UINT32 chksum = 0;
        for (const auto& index : m_Config.skipCRCFrames)
        {
            chksum += index;
        }
        suffix += Utility::StrPrintf("_skip%x\n", chksum);
    }
    return suffix;
}
