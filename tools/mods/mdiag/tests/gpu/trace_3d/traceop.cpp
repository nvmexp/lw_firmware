/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   traceop.cpp
 * @brief  various TraceOp classes.
 *
 */
#include "traceop.h"

#include "class/cl9096.h" // GF100_ZBC_CLEAR
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc397.h" // VOLTA_A
#include "class/clc3c0.h" // VOLTA_COMPUTE_A
#include "class/clc8b5.h" // HOPPER_COPY_DMA_A

#include "core/include/channel.h"
#include "gpu/utility/atomwrap.h"
#include "core/include/chiplibtracecapture.h"
#include "core/include/gpu.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/tasker.h"
#include "ctrl/ctrl208f.h"
#include "ctrl/ctrl85b6.h"
#include "ctrl/ctrl9096.h"

#include "device/interface/pcie.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/perf/pmusub.h"
#include "gputrace.h"

#include "mdiag/advschd/pmevent.h"
#include "mdiag/advschd/pmevntmn.h"
#include "mdiag/advschd/pmtest.h"
#include "mdiag/advschd/policymn.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/mdiag_xml.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/utils/sharedsurfacecontroller.h"
#include "mdiag/utils/surfutil.h"
#include "mdiag/utl/utl.h"

#include "lwos.h"
#include "teegroups.h"
#include "trace_3d.h"
#include "tracerel.h"

#include <boost/noncopyable.hpp>
#include <boost/functional/hash.hpp>
#include <utility>
#include <functional>

#define CTXSYNCID() T3D_MSGID(ContextSync)

namespace
{
    // TraceOpRunSemaphore is used to sync trace_3d threads
    class TraceOpRunSemaphore : private boost::noncopyable
    {
    public:
        static TraceOpRunSemaphore* Instance()
        {
            static TraceOpRunSemaphore instance;
            return &instance;
        }

        RC WaitForAllWorkerSubctx(UINT32 workerNum)
        {
            RC rc;
            for (UINT32 i = 0; i < workerNum; i++)
            {
                CHECK_RC(Tasker::AcquireSemaphore(m_Sema, Tasker::NO_TIMEOUT));
            }
            return rc;
        }

        void NotifyCoordinatorSubctx()
        {
            Tasker::ReleaseSemaphore(m_Sema);
        }

    private:
        TraceOpRunSemaphore()
        : m_Sema(Tasker::CreateSemaphore(0, "TraceOpRunSemaphore"))
        {}
        SemaID m_Sema;
    };

    // GpuBackendSemaphoreSurf manages semaphore surfaces.
    // The surface will be mapped into different VAS so that
    // different subctx can shared it
    class GpuBackendSemaphoreSurf : private boost::noncopyable
    {
    public:
        GpuBackendSemaphoreSurf(GpuDevice* pGpuDev, LwRm* pLwRm)
        : m_SemaVirtual(100 /*minium bucket count*/,
                        [](const AllocData& key) -> size_t
                        {
                            size_t seed = 0;
                            boost::hash_combine(seed, key.m_pLwRm);
                            boost::hash_combine(seed, key.m_VasHandle);
                            return seed;
                        },
                        [](const AllocData& lhs, const AllocData& rhs)
                        {
                            return (lhs.m_pLwRm == rhs.m_pLwRm) && (lhs.m_VasHandle == rhs.m_VasHandle);
                        }),
          m_SemaMap(100 /*minium bucket count*/,
                    [](const AllocData& key) -> size_t
                    {
                        size_t seed = 0;
                        boost::hash_combine(seed, key.m_pLwRm);
                        boost::hash_combine(seed, key.m_VasHandle);
                        return seed;
                    },
                    [](const AllocData& lhs, const AllocData& rhs)
                    {
                        return (lhs.m_pLwRm == rhs.m_pLwRm) && (lhs.m_VasHandle == rhs.m_VasHandle);
                    }),
          m_pGpuDev(pGpuDev), m_pLwRm(pLwRm), m_pCpuAddr(NULL)
        {
            AllocPhysSurf();
        }
        ~GpuBackendSemaphoreSurf()
        {
            Free();
        }

        void Zero()
        {
            MEM_WR32((uintptr_t)m_pCpuAddr, 0);
        }

        // export Map to let client can do surface map before the region
        // so that no cpu overhead will be introduced
        void Map(LwRm::Handle vasHandle, LwRm* pLwRm)
        {
            AllocData allocData(pLwRm, vasHandle);

            if (m_SemaMap.count(allocData))
            {
                return ;
            }

            // always duplicate physical memory handle because it is harmless
            // surf2d will return it directly if it is already allocated
            if (m_PhysSema.DuplicateSurface(pLwRm) != OK)
            {
                ErrPrintf(CTXSYNCID(), "Failed to duplicate backend semaphore physical "
                          "memory across RM client(%u)!\n", pLwRm->GetClientHandle());
                MASSERT(0);
            }

            MdiagSurf *surfVirtual = new MdiagSurf();
            Init(surfVirtual);
            surfVirtual->SetGpuVASpace(vasHandle);
            surfVirtual->SetSpecialization(Surface2D::VirtualOnly);

            if (surfVirtual->Alloc(m_pGpuDev, pLwRm) != OK)
            {
                ErrPrintf(CTXSYNCID(), "Failed to alloc virtual surface for backend semaphore on VAS(%u) "
                          "RM client(%u)!\n", vasHandle, pLwRm->GetClientHandle());
                MASSERT(0);
            }
            m_SemaVirtual[allocData] = surfVirtual;
            PHYSADDR phyAddr;
            RC rc = m_PhysSema.GetPhysAddress(0, &phyAddr);
            if (rc != OK)
            {
                ErrPrintf("Failed to get backend semaphore physical address!\n");
                MASSERT(0);
            }
            DebugPrintf(CTXSYNCID(), "Allocate backend semaphore (physical address 0x%llx) "
                        "on virtual address 0x%llx vas 0x%x!\n",
                        phyAddr, surfVirtual->GetCtxDmaOffsetGpu(), vasHandle);

            MdiagSurf *surfMap = new MdiagSurf();
            Init(surfMap);
            surfMap->SetSpecialization(Surface2D::MapOnly);
            if (surfMap->MapVirtToPhys(m_pGpuDev, surfVirtual, &m_PhysSema, 0, 0, pLwRm) != OK)
            {
                ErrPrintf(CTXSYNCID(), "Failed to map surface for backend semaphore on VAS(%u) RM client(%u)!\n,",
                          vasHandle, pLwRm->GetClientHandle());
                 MASSERT(0);
            }
            m_SemaMap[allocData] = surfMap;
        }

        UINT64 GetVA(LwRm::Handle vasHandle, LwRm* pLwRm)
        {
            AllocData allocData(pLwRm, vasHandle);

            if (m_SemaMap.count(allocData))
            {
                return m_SemaMap[allocData]->GetCtxDmaOffsetGpu();
            }

            Map(vasHandle, pLwRm);

            return m_SemaMap[allocData]->GetCtxDmaOffsetGpu();
        }
    private:
        void Init(MdiagSurf* pSurf)
        {
            pSurf->SetWidth(4);
            pSurf->SetPitch(4);
            pSurf->SetHeight(1);
            pSurf->SetColorFormat(ColorUtils::Y8);
            pSurf->SetLocation(Memory::Fb);
            pSurf->SetAlignment(4);
        }

        void AllocPhysSurf()
        {
            Init(&m_PhysSema);
            m_PhysSema.SetSpecialization(Surface2D::PhysicalOnly);
            if (m_PhysSema.Alloc(m_pGpuDev, m_pLwRm) != OK)
            {
                ErrPrintf(CTXSYNCID(), "Failed to allocate backend semaphore for PM sync on RM client(%u)!\n",
                          m_pLwRm->GetClientHandle());
                MASSERT(0);
            }
            RC rc;
            PHYSADDR phyAddr;
            rc = m_PhysSema.GetPhysAddress(0, &phyAddr);
            if (rc != OK)
            {
                ErrPrintf("Failed to get backend semaphore physical address!\n");
                MASSERT(0);
            }
            DebugPrintf(CTXSYNCID(), "Allocate backend semaphore on physical address 0x%llx!\n",
                        phyAddr);

            rc = m_pLwRm->MapMemory(m_PhysSema.GetMemHandle(),
                                  0,
                                  m_PhysSema.GetSize(),
                                  &m_pCpuAddr,
                                  0,
                                  m_pGpuDev->GetSubdevice(0));
            DebugPrintf(CTXSYNCID(), "Map backend semaphore on cpu address 0x%llx!\n",
                        m_pCpuAddr);

            if (rc != OK)
            {
                ErrPrintf(CTXSYNCID(), "Failed to map backend semaphore on CPU address space!\n");
                MASSERT(0);
            }
        }

        void Free()
        {
            for (auto& i : m_SemaMap)
            {
                delete i.second;
            }
            for (auto& i : m_SemaVirtual)
            {
                delete i.second;
            }
            m_pLwRm->UnmapMemory(m_PhysSema.GetMemHandle(),
                               m_pCpuAddr,
                               0,
                               m_pGpuDev->GetSubdevice(0));
        }

        struct AllocData
        {
            AllocData(LwRm* pLwRm, LwRm::Handle handle)
            : m_pLwRm(pLwRm), m_VasHandle(handle)
            {}

            LwRm* m_pLwRm;
            LwRm::Handle m_VasHandle;
        };

        unordered_map<AllocData, MdiagSurf*, function<size_t(const AllocData&)>,
                      function<bool(const AllocData&, const AllocData&)>> m_SemaVirtual;
        unordered_map<AllocData, MdiagSurf*, function<size_t(const AllocData&)>,
                      function<bool(const AllocData&, const AllocData&)>> m_SemaMap;
        MdiagSurf m_PhysSema;
        GpuDevice* m_pGpuDev;
        LwRm* m_pLwRm;  // used to allocate physical memory
        void* m_pCpuAddr;
    };

    // SyncEventSemaphoreSurf manages semaphore surfaces used by
    // SYNC_EVENT ON/OFF
    class SyncEventSemaphoreSurf : private boost::noncopyable
    {
    public:
        enum Type {SYNC_ON = 0, SYNC_OFF, PM_SYNC_ON, PM_SYNC_OFF, SYNC_SURF_SIZE};
        static SyncEventSemaphoreSurf* Instance()
        {
            static SyncEventSemaphoreSurf instance;
            return &instance;
        }

        void Alloc(Type type, GpuDevice* pGpuDev, LwRm* pLwRm)
        {
            if (type >= SYNC_SURF_SIZE)
            {
                MASSERT(!"Unknown semaphore type!");
            }

            Tasker::MutexHolder mh(m_Mutex);
            if (m_SyncSemaSurf[type] == nullptr)
            {
                m_SyncSemaSurf[type] = new GpuBackendSemaphoreSurf(pGpuDev, pLwRm);
            }
            m_SyncSemaRef[type]++;
        }

        void Free(Type type)
        {
            if (type >= SYNC_SURF_SIZE)
            {
                MASSERT(!"Unknown semaphore type!");
            }

            Tasker::MutexHolder mh(m_Mutex);

            if (m_SyncSemaSurf[type])
            {
                m_SyncSemaRef[type]--;

                if (m_SyncSemaRef[type] == 0)
                {
                    delete m_SyncSemaSurf[type];
                    m_SyncSemaSurf[type] = nullptr;
                }
            }
        }

        GpuBackendSemaphoreSurf* Get(Type type)
        {
            if (type >= SYNC_SURF_SIZE)
            {
                MASSERT(!"Unknown semaphore type!");
            }

            if (m_SyncSemaSurf[type] == nullptr)
            {
                MASSERT(!"semaphore is not allocated!");
            }

            return m_SyncSemaSurf[type];
        }

    private:
        SyncEventSemaphoreSurf()
        : m_SyncSemaSurf(SYNC_SURF_SIZE, nullptr),
          m_SyncSemaRef(SYNC_SURF_SIZE, 0),
          m_Mutex(NULL)
        {
            m_Mutex = Tasker::AllocMutex("SyncEventSemaphoreSurf::m_Mutex",
                                         Tasker::mtxUnchecked);
        }
        ~SyncEventSemaphoreSurf()
        {
            for (auto& pSurf : m_SyncSemaSurf)
            {
                delete pSurf;
                pSurf = nullptr;
            }
            Tasker::FreeMutex(m_Mutex);
        }
        vector<GpuBackendSemaphoreSurf*> m_SyncSemaSurf;
        vector<int> m_SyncSemaRef;
        void* m_Mutex; // surface allocation could cause an implicit yield
    };

    // SubctxInbandSemaphore helps to insert FE/CE release and host acquire methods
    // into semaphore surfaces
    class SubctxInbandSemaphore
    {
    public:
        SubctxInbandSemaphore(GpuBackendSemaphoreSurf* sema, TraceChannel* channel,
                   TraceSubChannel* subchannel, UINT32 payload)
        : m_BackendSema(sema), m_pChannel(channel),
          m_pSubChannel(subchannel), m_Payload(payload)
        {

        }
        ~SubctxInbandSemaphore()
        {
        }
        RC Release()
        {
            RC rc;

            vector<UINT32> semData(3);
            const UINT64 virtualAddress =
                m_BackendSema->GetVA(m_pChannel->GetVASpaceHandle(), m_pChannel->GetLwRmPtr());
            semData[0] = static_cast<UINT32>(virtualAddress>>32);
            semData[1] = static_cast<UINT32>(virtualAddress);
            semData[2] = m_Payload;
            
            // FE release methods do not work for async ce tests.
            if (m_pSubChannel->CopySubChannel())
            {
                DebugPrintf(CTXSYNCID(), "Begin to send CE Release methods...\n");

                CHECK_RC(m_pSubChannel->GetSubCh()->MethodWriteN_RC(LWC8B5_SET_SEMAPHORE_A,
                    static_cast<UINT32>(semData.size()),&semData[0]));

                const UINT32 data = 0;
                CHECK_RC(m_pSubChannel->GetSubCh()->MethodWriteN_RC(LWC8B5_LINE_LENGTH_IN,
                    1, &data));
                CHECK_RC(m_pSubChannel->GetSubCh()->MethodWriteN_RC(LWC8B5_LINE_COUNT,
                    1, &data));

                UINT32 dmaData = 0;
                dmaData |= DRF_DEF(C8B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);
                dmaData |= DRF_DEF(C8B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE);
                dmaData |= DRF_DEF(C8B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE);
                dmaData |= DRF_DEF(C8B5, _LAUNCH_DMA, _SEMAPHORE_REDUCTION_ENABLE, _TRUE);
                dmaData |= DRF_DEF(C8B5, _LAUNCH_DMA, _SEMAPHORE_REDUCTION, _INC);
                dmaData |= DRF_DEF(C8B5, _LAUNCH_DMA, _SEMAPHORE_REDUCTION_SIGN, _UNSIGNED);

                CHECK_RC(m_pSubChannel->GetSubCh()->MethodWriteN_RC(LWC8B5_LAUNCH_DMA,
                                                                    1, &dmaData));

                DebugPrintf(CTXSYNCID(), "Sending CE Release methods is completed\n");
            }
            else
            {
                DebugPrintf(CTXSYNCID(), "Begin to send FE Release methods...\n");
                CHECK_RC(m_pSubChannel->GetSubCh()->MethodWriteN_RC(LWC397_SET_I2M_SEMAPHORE_A,
                                                        static_cast<UINT32>(semData.size()),
                                                        &semData[0]));

                const UINT32 data = 0;
                CHECK_RC(m_pSubChannel->GetSubCh()->MethodWriteN_RC(LWC397_LINE_LENGTH_IN,
                                                                    1, &data));
                CHECK_RC(m_pSubChannel->GetSubCh()->MethodWriteN_RC(LWC397_LINE_COUNT,
                                                                    1, &data));

                UINT32 dmaData = 0;
                dmaData |= DRF_DEF(C397, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);
                dmaData |= DRF_DEF(C397, _LAUNCH_DMA, _COMPLETION_TYPE, _RELEASE_SEMAPHORE);
                dmaData |= DRF_DEF(C397, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE);
                dmaData |= DRF_DEF(C397, _LAUNCH_DMA, _SEMAPHORE_STRUCT_SIZE, _ONE_WORD);
                dmaData |= DRF_DEF(C397, _LAUNCH_DMA, _REDUCTION_ENABLE, _TRUE);
                dmaData |= DRF_DEF(C397, _LAUNCH_DMA, _REDUCTION_OP, _RED_INC);
                dmaData |= DRF_DEF(C397, _LAUNCH_DMA, _REDUCTION_FORMAT, _UNSIGNED_32);
                dmaData |= DRF_DEF(C397, _LAUNCH_DMA, _SYSMEMBAR_DISABLE, _FALSE);

                CHECK_RC(m_pSubChannel->GetSubCh()->MethodWriteN_RC(LWC397_LAUNCH_DMA,
                                                                    1, &dmaData));

                DebugPrintf(CTXSYNCID(), "Sending FE Release methods is completed\n");
            }

            return rc;
        }
        RC Acquire()
        {
            RC rc;

            DebugPrintf(CTXSYNCID(), "Begin to send Host Acquire methods...\n");

            vector<UINT32> semData(5);
            const UINT64 virtualAddress =
                m_BackendSema->GetVA(m_pChannel->GetVASpaceHandle(), m_pChannel->GetLwRmPtr());
            semData[0] = static_cast<UINT32>(virtualAddress);
            semData[1] = static_cast<UINT32>(virtualAddress>>32);
            semData[2] = m_Payload;
            semData[3] = 0; /*Not need for 32-bit payload*/
            semData[4] |= DRF_DEF(C36F, _SEM_EXELWTE, _OPERATION, _ACQUIRE);
            semData[4] |= DRF_DEF(C36F, _SEM_EXELWTE, _ACQUIRE_SWITCH_TSG, _DIS);
            semData[4] |= DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _32BIT);
            semData[4] |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION_FORMAT, _UNSIGNED);
            // Host will not check subchannel number. Call LWGpuSubChannel::MethodWriteN_RC
            // instead of LWGpuChannel::MethodWriteN_RC because former one will print methods
            // written message.
            CHECK_RC(m_pSubChannel->GetSubCh()->MethodWriteN_RC(LWC36F_SEM_ADDR_LO,
                                                          static_cast<UINT32>(semData.size()),
                                                          &semData[0]));

            DebugPrintf(CTXSYNCID(), "Sending Host Acquire methods is completed\n");

            return rc;
        }
    private:
        GpuBackendSemaphoreSurf* m_BackendSema;
        TraceChannel* m_pChannel;
        TraceSubChannel* m_pSubChannel;
        UINT32 m_Payload;
    };
}

//------------------------------------------------------------------------------
// TraceOp: abstract base class, represents a single operation from a parsed
// test.hdr file.
//
TraceOp::TraceOp()
:   m_TraceOpId(0),
    m_TraceOpStatus(TRACEOP_STATUS_DEFAULT),
    m_GpuTrace(nullptr),
    m_TriggerPoint(TraceOpTriggerPoint::Before),
    m_Skip(false),
    m_IsDefaultDependency(true),
    m_TraceOpExecOpNum(~0U)
{
}

void TraceOp::InitTraceOpDependency(GpuTrace* gpuTrace)
{
    m_GpuTrace = gpuTrace;
    m_TraceOpId = m_GpuTrace->GetLineNumber();
    if (m_GpuTrace->GetTraceOps().size() > 0)
    {
        // Set default traceop dependency: current traceop depend on the last one.
        m_DependentTraceOpIds.push_back(m_GpuTrace->GetTraceOps().rbegin()->first);
    }
}

void TraceOp::AddTraceOpDependency(UINT32 dependentTraceOpId)
{
    // Delete default traceop dependency
    if (m_IsDefaultDependency)
    {
        m_DependentTraceOpIds.pop_front();
        m_IsDefaultDependency = false;
    }

    // Set user defined traceop dependency
    m_DependentTraceOpIds.push_back(dependentTraceOpId);
}

bool TraceOp::CheckTraceOpDependency()
{
    // Those below lines can't be remove since bug 200589972
    if (!m_GpuTrace->ContainExelwtionDependency()) 
        return true;

    while (m_DependentTraceOpIds.size() > 0)
    {
        TraceOp* pTraceOp = m_GpuTrace->GetTraceOp(m_DependentTraceOpIds.front());
        if (pTraceOp == nullptr)
        {
            m_DependentTraceOpIds.pop_front();
        }
        else if (pTraceOp->GetTraceOpStatus() == TRACEOP_STATUS_DONE)
        {
            m_DependentTraceOpIds.pop_front();
        }
        else
        {
            return false;
        }
    }
    return true;
}

const char* TraceOp::GetOpName(const TraceOpType type)
{
    switch (type)
    {
    case TraceOpType::AllocSurface: return "AllocSurface";
    case TraceOpType::AllocVirtual: return "AllocVirtual";
    case TraceOpType::AllocPhysical: return "AllocPhysical";
    case TraceOpType::Map: return "Map";
    case TraceOpType::SurfaceView: return "SurfaceView";
    case TraceOpType::FreeSurface: return "FreeSurface";
    case TraceOpType::UpdateFile: return "UpdateFile";
    case TraceOpType::CheckDynamicSurface: return "CheckDynamicSurface";
    case TraceOpType::Reloc64: return "Reloc64";
    case TraceOpType::Unknown:
    default:
        break;
    }
    return "UnknownTraceOp";
}

string TraceOp::GetTriggerPointStr(UINT32 point)
{
    if (static_cast<UINT32>(TraceOpTriggerPoint::None) == point)
    {
        return "None";
    }
    string str;
    if (HasTriggerPoint(point, TraceOpTriggerPoint::Parse))
    {
        str += "Parse";
    }
    if (HasTriggerPoint(point, TraceOpTriggerPoint::Before))
    {
        if (!str.empty())
        {
            str += "|";
        }
        str += "Before";
    }
    if (HasTriggerPoint(point, TraceOpTriggerPoint::After))
    {
        if (!str.empty())
        {
            str += "|";
        }
        str += "After";
    }
    return str;
}

/*virtual*/ TraceOp::~TraceOp()
{
}

/*virtual*/ bool TraceOp::HasMethods() const
{
    return false;
}

/*virtual*/ bool TraceOp::WillPolling() const
{
    return false;
}

/*static*/ const char * TraceOp::BoolFuncToString(TraceOp::BoolFunc bf)
{
    const char * bfNames[] =
    {
        "==",
        "!=",
        ">",
        "<",
        ">=",
        "<="
    };
    MASSERT(bf < BfNUM_ITEMS);
    return bfNames[bf];
}

//------------------------------------------------------------------------------
// TraceOpSendMethods: writes part or all of the methods TraceModule to the
// channel it is associated with.
TraceOpSendMethods::TraceOpSendMethods
(
    TraceModule * pTraceModule,
    UINT32 start,
    UINT32 size
)
:   m_pTraceModule(pTraceModule),
    m_Start(start),
    m_Size(size)
{
    m_SegmentIsValid = (OK ==
            m_pTraceModule->AddMethodSegment(start, size, &m_SegmentH));
}
bool TraceOpSendMethods::SegmentIsValid() const
{
    return m_SegmentIsValid;
}

/*virtual*/ TraceOpSendMethods::~TraceOpSendMethods()
{
}

/*virtual*/ RC TraceOpSendMethods::Run()
{
    MASSERT(m_SegmentIsValid);
    return m_pTraceModule->SendMethodSegment(m_SegmentH);
}

/*virtual*/ bool TraceOpSendMethods::HasMethods() const
{
    return true;
}

/*virtual*/ void TraceOpSendMethods::Print() const
{
    RawPrintf("SendMethods from %s @%04x..%04x-1 to channel %s\n",
            m_pTraceModule->GetName().c_str(), m_Start, m_Start + m_Size,
            m_pTraceModule->GetChannelName().c_str());
}

void TraceOpSendMethods::GetTraceopProperties( const char * * pName,
                                               map<string,string> & properties ) const
{
    *pName = "TraceOpSendMethods";

    char buf[ 100 ];

    snprintf( buf, sizeof(buf), "%d", m_Start );
    properties[ "start" ] = buf;

    snprintf( buf, sizeof(buf), "%d", m_Size );
    properties[ "size" ] = buf;

    snprintf( buf, sizeof(buf), "%llu", (UINT64)m_SegmentH );
    properties[ "segment" ] = buf;

    snprintf( buf, sizeof(buf), "%d", m_SegmentIsValid );
    properties[ "valid" ] = buf;

    properties[ "channelName" ] = m_pTraceModule->GetChannelName();
    properties[ "pushbufferName" ] = m_pTraceModule->GetName();
}

//------------------------------------------------------------------------------
// TraceOpSendGpEntry: writes a GPFIFO entry to the channel
TraceOpSendGpEntry::TraceOpSendGpEntry
(
    TraceChannel * pTraceChannel,
    TraceModule  * pTraceModule,
    UINT64         offset,
    UINT32         size,
    bool           sync,
    UINT32         repeat_num
)
:   m_pTraceChannel(pTraceChannel),
    m_pTraceModule(pTraceModule),
    m_Offset(offset),
    m_Size(size),
    m_Sync(sync),
    m_RepeatNum(repeat_num)
{
}

/*virtual*/ TraceOpSendGpEntry::~TraceOpSendGpEntry()
{
}

RC TraceOpSendGpEntry::EscWriteGpEntryInfo()
{
    RC rc;

    //
    // Bug 838804
    // Amodel need some information to dump meta data
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        const string name = m_pTraceModule->GetName();
        UINT32 size = sizeof(UINT64) + sizeof(UINT64) + name.size() + 1;
        if (m_pTraceChannel->GetGpuResource()->IsMultiVasSupported())
        {
            //vas id
            size += sizeof(UINT32);
        }
        vector<UINT08> escwriteBuffer(size);
        UINT08* buf = &escwriteBuffer[0];

        // offset
        *(UINT64*)buf = m_pTraceModule->GetOffset() + m_Offset;
        buf += sizeof(UINT64);

        // size
        *(UINT64*)buf = static_cast<UINT64>(m_Size);
        buf += sizeof(UINT64);

        // surface name. end with null to avoid accident in amodel
        memcpy(buf, name.c_str(), name.size());
        buf += name.size();
        *buf = '\0';
        ++buf;
        if (m_pTraceChannel->GetGpuResource()->IsMultiVasSupported())
        {
            LwRm* pLwRm = m_pTraceChannel->GetLwRmPtr();
            LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS params = {0};
            params.hVASpace = m_pTraceModule->GetVASpaceHandle();
            CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(m_pTraceModule->GetGpuDev()),
                LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS, &params, sizeof (params)));
            *(UINT32 *)buf = static_cast<UINT32>(params.vaSpaceId);
        }

        // key = "GPEntry"
        const char* key = "GPEntry";
        GpuDevice* gpudev = m_pTraceModule->GetGpuDev();
        for (UINT32 subdev = 0; subdev < gpudev->GetNumSubdevices(); subdev++)
        {
            // it doesn't matter to ignore the return value of escapewrite
            GpuSubdevice* gpuSubdev = gpudev->GetSubdevice(subdev);
            gpuSubdev->EscapeWriteBuffer(key, 0, size,
                                         &escwriteBuffer[0]);
        }
    }

    return rc;
}

/*virtual*/ RC TraceOpSendGpEntry::Run()
{
    RC rc;
    const ArgReader* params = m_pTraceModule->GetTest()->GetParam();
    if (params && params->ParamPresent("-skip_gpentry") > 0)
        return rc;

    CHECK_RC(EscWriteGpEntryInfo());

    UINT64 get = m_pTraceModule->GetOffset() + m_Offset;

    if (m_Sync)
        CHECK_RC(m_pTraceChannel->GetCh()->GetModsChannel()->SetSyncEnable(m_Sync));
    // CallSubroutine() really does not care which subchannel, choose first one
    // Bug 740555: need to retain m_RepeatNum's value in case Run() is called multiple times.
    UINT32 repeatNum = m_RepeatNum;
    while (repeatNum-- > 0)
    {
        // If a channel is host only (has no subchannels) call the subroutine using the channel itself
        if (m_pTraceChannel->IsHostOnlyChannel())
        {
            CHECK_RC(m_pTraceChannel->GetCh()->GetModsChannel()->CallSubroutine(get, m_Size));
        }
        else
        {
            CHECK_RC(m_pTraceChannel->GetSubChannel("")->GetSubCh()->CallSubroutine(get, m_Size));
        }
    }
    if (m_Sync)
        CHECK_RC(m_pTraceChannel->GetCh()->GetModsChannel()->SetSyncEnable(false));
    return rc;
}

/*virtual*/ void TraceOpSendGpEntry::Print() const
{
    RawPrintf("SendGpEntry from %s @%04llx..%04llx-1 to channel %s\n",
            m_pTraceModule->GetName().c_str(), m_Offset, m_Offset + m_Size,
            m_pTraceChannel->GetName().c_str());
}

/*virtual*/ RC TraceOpSendGpEntry::ConsolidateGpEntry
(
    UINT32 Size
)
{
    m_Size += Size;

    return OK;
}

/*virtual*/ TraceOpSendGpEntry* TraceOpSendGpEntry::CreateNewContGpEntryOp
(
    UINT32 Size
)
{

    return new TraceOpSendGpEntry(
        m_pTraceChannel,
        m_pTraceModule,
        m_Offset+m_Size,
        Size,
        m_Sync,
        m_RepeatNum);
}

/*virtual*/ bool TraceOpSendGpEntry::HasMethods() const
{
    return true;
}

void TraceOpSendGpEntry::GetTraceopProperties( const char * * pName,
                                               map<string,string> & properties ) const
{
    *pName = "TraceOpSendGpEntry";

    properties[ "channel" ] =  m_pTraceChannel->GetName();
    properties[ "module" ] =  m_pTraceModule->GetName();
    properties[ "offset" ] = Utility::StrPrintf( "%08x", m_Offset );
    properties[ "size" ] = Utility::StrPrintf( "%08x", m_Size );
    properties[ "sync" ] = Utility::StrPrintf( "%d", m_Sync ? 1 : 0 );
}

//------------------------------------------------------------------------------
// TraceOpFlush: flush a channel
TraceOpFlush::TraceOpFlush
(
    TraceChannel * pTraceChannel
)
:   m_pTraceChannel(pTraceChannel)
{
}

/*virtual*/ TraceOpFlush::~TraceOpFlush()
{
}

/*virtual*/ RC TraceOpFlush::Run()
{
    RC rc;

    // If the trace requested an explicit FLUSH then cancel atomicity so that 
    // all pending atomic methods are also sent
    // See: https://confluence.lwpu.com/display/ArchMods/Multi-channel+traces+WFI+issue
    AtomChannelWrapper *pAtomWrap = m_pTraceChannel->GetCh()->GetModsChannel()->GetAtomChannelWrapper();
    if (pAtomWrap)
    {
        CHECK_RC(pAtomWrap->CancelAtom());
    }
    return m_pTraceChannel->GetCh()->MethodFlushRC();
}

/*virtual*/ void TraceOpFlush::Print() const
{
    RawPrintf("Flush channel %s\n",
            m_pTraceChannel->GetName().c_str());
}

/*virtual*/ void TraceOpFlush::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpFlush";
}

//------------------------------------------------------------------------------
// TraceOpWaitIdle: wait for a channel to go idle
TraceOpWaitIdle::TraceOpWaitIdle()
:   m_HaveInsertedSem(false)
{
}

/*virtual*/ TraceOpWaitIdle::~TraceOpWaitIdle()
{
}

/*virtual*/ RC TraceOpWaitIdle::Run()
{
    RC rc;

    SetTraceOpStatus(TRACEOP_STATUS_DEFAULT);

    const GpuTrace* pGpuTrace = GetGpuTrace();
    MASSERT(pGpuTrace);

    // Flush all channels before idle polling
    if (!m_HaveInsertedSem)
    {
        auto it = m_WaitIdleTraceChannels.begin();
        auto end = m_WaitIdleTraceChannels.end();
        for (; it != end; ++it)
        {
            if (pGpuTrace->ContainExelwtionDependency())
            {
                CHECK_RC(it->TraceChannelPtr->GetCh()->InsertSemMethodsForNonblockingPoll());
                it->TraceChannelPayloadValue = it->TraceChannelPtr->GetCh()->GetPayloadValue();
                DebugPrintf("NON_BLOCKING EXELWTION: %s: insert semaphore. \n", __FUNCTION__);
            }
            CHECK_RC(it->TraceChannelPtr->GetCh()->MethodFlushRC());
        }
        m_HaveInsertedSem = true;
    }

    // Idle each channel one by one
    auto it = m_WaitIdleTraceChannels.begin();
    auto end = m_WaitIdleTraceChannels.end();
    for (; it != end; ++it)
    {
        if (pGpuTrace->ContainExelwtionDependency())
        {
            if (it->TraceChannelPtr->GetCh()->CheckSemaphorePayload(it->TraceChannelPayloadValue))
            {
                CHECK_RC(it->TraceChannelPtr->GetCh()->CheckForErrorsRC());
            }
            else 
            {
                // Set traceop status as pending,
                // It means that this traceop will run again,
                // to check semaphore payload.
                SetTraceOpStatus(TRACEOP_STATUS_PENDING);
                return OK;
            }
        }
        else
        {
            InfoPrintf("Calling wait for idle from traceOp Id %u.\n", GetTraceOpId());
            CHECK_RC(it->TraceChannelPtr->GetCh()->WaitForIdleRC());
        }
    }

    return rc;
}

/*virtual*/ void TraceOpWaitIdle::Print() const
{
    RawPrintf("WaitForIdle channel");
    auto it = m_WaitIdleTraceChannels.begin();
    auto end = m_WaitIdleTraceChannels.end();
    for (; it != end; ++it)
    {
        RawPrintf(" %s", it->TraceChannelPtr->GetName().c_str());
    }
    RawPrintf("\n");
}

/*virtual*/ bool TraceOpWaitIdle::WillPolling() const
{
    // This function does not mean whether this is blocked wfi or non-blocking wfi.
    return true;
}

void TraceOpWaitIdle::GetTraceopProperties( const char * * pName,
                                            map<string,string> & properties ) const
{
    *pName = "TraceOpWaitIdle";

    string chNameList;
    for (const auto &ch : m_WaitIdleTraceChannels)
    {
        chNameList += Utility::StrPrintf("%s ", ch.TraceChannelPtr->GetName().c_str()); 
    }
    chNameList.erase(chNameList.find_last_not_of(" ") + 1);

    properties[ "channelNameList" ] = chNameList; 
}

RC TraceOpWaitIdle::AddWFITraceChannel(TraceChannel * pTraceChannel)
{
    if (m_HaveInsertedSem)
    {
        // Suppose that all wfi TraceChannel have been added 
        // before traceop insert semaphore for every wfi TraceChannel.
        // After polling semaphore has been inserted, this traceop is ready to run.
        // It's dangerous to add new wfi TraceChannel again during traceop runs.
        ErrPrintf("Not allow to add WFI TraceChannel after polling semaphore methods have been inserted. \n");
        return RC::SOFTWARE_ERROR;
    }

    if (!pTraceChannel)
    {
        ErrPrintf("Invalid TraceChannel pointer\n");
        return RC::LWRM_ILWALID_ARGUMENT;
    }

    auto start = m_WaitIdleTraceChannels.begin();
    auto end = m_WaitIdleTraceChannels.end();
    auto it = find_if(start, end, 
                      [pTraceChannel](const WaitIdleTraceChannel& i)
                      {return i.TraceChannelPtr == pTraceChannel;});

    if (it == end)
    {
        m_WaitIdleTraceChannels.push_back(WaitIdleTraceChannel(pTraceChannel, 0));
    }
    else
    {
        ErrPrintf("Same channel is specified more than once in WAIT_FOR_IDLE\n");
        return RC::LWRM_ILWALID_ARGUMENT;
    }

    return OK;
}

//------------------------------------------------------------------------------
// TraceOpWaitValue32: wait for a 32-bit value in a surface to satisfy a boolean
TraceOpWaitValue32::TraceOpWaitValue32
(
    TraceModule * pTraceModule,
    UINT32 surfOffset,
    BoolFunc compare,
    UINT32 value,
    UINT32 timeoutMs
)
:   m_pTraceModule(pTraceModule),
    m_SurfOffset(surfOffset),
    m_Compare(compare),
    m_Value(value),
    m_TimeoutMs(timeoutMs),
    m_pDmaBuffer(nullptr)
{
}

/*virtual*/ TraceOpWaitValue32::~TraceOpWaitValue32()
{
}

inline bool TraceOpWaitValue32::Poll()
{
    UINT32 i = MEM_RD32((uintptr_t)m_pDmaBuffer->GetAddress());

    return TraceOp::Compare(i, m_Value, m_Compare);
}

bool TraceOpWaitValue32_PollFunc(void * vpthis)
{
    TraceOpWaitValue32 * pthis = (TraceOpWaitValue32 *)vpthis;
    return pthis->Poll();
}

/*virtual*/ RC TraceOpWaitValue32::Run()
{
    RC rc;

    SetTraceOpStatus(TRACEOP_STATUS_DEFAULT);

    if (m_pDmaBuffer == nullptr)
    {
        m_pDmaBuffer = m_pTraceModule->GetDmaBufferNonConst();

        // Map a 4096-byte chunk of this surface to the CPU's address space.
        UINT64 offsetWithinDmaBuf = m_SurfOffset + m_pTraceModule->GetOffsetWithinDmaBuf();
        CHECK_RC( m_pDmaBuffer->MapRegion(offsetWithinDmaBuf, sizeof(UINT32)) );
    }

    const GpuTrace* pGpuTrace = GetGpuTrace();
    MASSERT(pGpuTrace);
    MASSERT(m_pDmaBuffer);
    if (pGpuTrace->ContainExelwtionDependency())
    {
        if (TraceOpWaitValue32_PollFunc((void *)this))
        {
            m_pDmaBuffer->Unmap();
        }
        else
        {
            // Set traceop status as pending,
            // It means that this traceop will run again to check value.
            SetTraceOpStatus(TRACEOP_STATUS_PENDING);
        }
    }
    else
    {
        if (m_pDmaBuffer->GetLocation() == Memory::Fb)
            rc = POLLWRAP_HW( &TraceOpWaitValue32_PollFunc, (void *)this, m_TimeoutMs);
        else
            rc = POLLWRAP( &TraceOpWaitValue32_PollFunc, (void *)this, m_TimeoutMs);

        m_pDmaBuffer->Unmap();
    }

    return rc;
}

/*virtual*/ void TraceOpWaitValue32::Print() const
{
    RawPrintf("WaitValue32 %s[0x%x] %s 0x%x  within %dms\n",
            m_pTraceModule->GetName().c_str(),
            m_SurfOffset,
            TraceOp::BoolFuncToString(m_Compare),
            m_Value,
            m_TimeoutMs);
}

/*virtual*/ bool TraceOpWaitValue32::WillPolling() const
{
    // This function does not mean whether this is blocked wfv or non-blocking wfv.
    return true;
}

/*virtual*/ void TraceOpWaitValue32::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpWaitValue32";
}

//------------------------------------------------------------------------------
TraceOpUpdateFile::TraceOpUpdateFile
(
    TraceModule *  pTraceModule,
    UINT32         moduleOffset,
    TraceFileMgr * pTraceFileMgr,
    string         filename,
    bool           flushCache,
    GpuDevice    * gpuDevice,
    UINT32         timeoutMs
)
:   m_pTraceModule(pTraceModule),
    m_ModuleOffset(moduleOffset),
    m_pTraceFileMgr(pTraceFileMgr),
    m_FileName(filename),
    m_FlushCache(flushCache),
    m_GpuDevice(gpuDevice),
    m_TimeoutMs(timeoutMs)
{
}

/*virtual*/ TraceOpUpdateFile::~TraceOpUpdateFile()
{
}

/*virtual*/ RC TraceOpUpdateFile::Run()
{
    if (m_FlushCache)
    {
        for (UINT32 subdeviceId = 0;
             subdeviceId < m_GpuDevice->GetNumSubdevices();
             ++subdeviceId)
        {
            GpuSubdevice* gpuSubdevice =
                m_GpuDevice->GetSubdevice(subdeviceId);

            gpuSubdevice->FbFlush(m_TimeoutMs);
            gpuSubdevice->IlwalidateL2Cache(0);
        }
    }

    return m_pTraceModule->Update(m_ModuleOffset, m_FileName.c_str(), m_pTraceFileMgr);
}

/*virtual*/ void TraceOpUpdateFile::Print() const
{
    RawPrintf("UpdateFile module %s @%x with file %s\n",
            m_pTraceModule->GetName().c_str(),
            m_ModuleOffset,
            m_FileName.c_str());
}

void TraceOpUpdateFile::GetTraceopProperties( const char * * pName,
                                              map<string,string> & properties ) const
{
    *pName = "TraceOpUpdateFile";

    properties[ "file" ] = m_pTraceModule->GetName();

    char buf[ 100 ];
    snprintf( buf, sizeof(buf), "0x%08x", m_ModuleOffset );
    properties[ "offset" ] = buf;

    properties[ "newDataFile" ] = m_FileName;
}

//------------------------------------------------------------------------------
TraceOpUpdateReloc64::TraceOpUpdateReloc64
(
    string         name,
    TraceModule *  pTraceModule,
    UINT64         mask_out,
    UINT64         mask_in,
    UINT32         offset,
    bool           bswap,
    bool           badd,
    UINT32         peerNum,
    UINT32         subdevNum,
    UINT64         signExtendMask,
    string         signExtendMode
)
:   m_SurfName(name),
    m_pTraceModule(pTraceModule),
    m_MaskOut(mask_out),
    m_MaskIn(mask_in),
    m_Offset(offset),
    m_Swap(bswap),
    m_Add(badd),
    m_PeerNum(peerNum),
    m_SubdevNum(subdevNum),
    m_SignExtendMask(signExtendMask),
    m_SignExtendMode(signExtendMode)
{
}

/*virtual*/ TraceOpUpdateReloc64::~TraceOpUpdateReloc64()
{
}

/*virtual*/ RC TraceOpUpdateReloc64::Run()
{
    TraceRelocation64 reloc(m_MaskOut, m_MaskIn, m_Offset, m_Swap, m_SurfName, false,
                            m_Add, 0, 0, m_PeerNum, USE_DEFAULT_RM_MAPPING, m_SignExtendMask, m_SignExtendMode);

    // SLI support is added here
    for (UINT32 i = 0; i < m_SubdevNum; i++)
    {
        reloc.DoOffset(m_pTraceModule, i);
    }

    return OK;
}

/*virtual*/ void TraceOpUpdateReloc64::Print() const
{
    RawPrintf("Update RELOC64 %s mask_out=0x%08llx mask_in = 0x%08llx offset=0x%x %s\n",
            m_pTraceModule->GetName().c_str(),
            m_MaskOut,
            m_MaskIn,
            m_Offset,
            m_Swap ? "SWAP" : "NO_SWAP");
}

/*virtual*/ void TraceOpUpdateReloc64::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpUpdateReloc64";
}

//------------------------------------------------------------------------------
TraceOpUpdateRelocConst32::TraceOpUpdateRelocConst32
(
    TraceModule *  pTraceModule,
    UINT32         value,
    UINT32         offset
)
:   m_pTraceModule(pTraceModule),
    m_Value(value),
    m_Offset(offset)
{
}

/*virtual*/ TraceOpUpdateRelocConst32::~TraceOpUpdateRelocConst32()
{
}

/*virtual*/ RC TraceOpUpdateRelocConst32::Run()
{
    TraceRelocationConst32 reloc(m_Offset, m_Value);
    // TODO(gsaggese): This function is not SLI aware, since DoOffset works
    // only for subdevice 0.
    reloc.DoOffset(m_pTraceModule, 0);
    return OK;
}

/*virtual*/ void TraceOpUpdateRelocConst32::Print() const
{
    RawPrintf("Update RELOC_CONST32 %s ofset=0x%x value=0x%x\n",
            m_pTraceModule->GetName().c_str(),
            m_Offset,
            m_Value);
}

/*virtual*/ void TraceOpUpdateRelocConst32::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpUpdateRelocConst32";
}

//------------------------------------------------------------------------------
TraceOpSec::TraceOpSec
(
    TraceModule   *pTraceModule,
    UINT32        moduleOffset,
    bool          isContent,
    const char    *contentKey,
    UINT32         subdev
)
:   m_pTraceModule(pTraceModule),
    m_ModuleOffset(moduleOffset),
    m_IsContent(isContent),
    m_Subdev(subdev)
{
    m_ContentKey = new char [strlen(contentKey)+1] ;
    strcpy(m_ContentKey, contentKey) ;
}

/*virtual*/ TraceOpSec::~TraceOpSec()
{
    delete m_ContentKey ;
}

#include "ctrl/ctrl0080.h"

int TraceOpSec::getContentKey(unsigned char *out, const char *inStr)
{
    char bte[] = "00";
    int i, off ;
    UINT32 val ;
    if(inStr==NULL) return 1 ;
    int len = strlen(inStr) ;
    /// Colwert by bte Little Endian:
    if(len==0) return 1 ;
    if (len != (2 * SKEY_SIZE_BYTES) ) {
        ErrPrintf("INVALID Content Key string Length: %d. Should be %d !\n", len, SKEY_SIZE_BYTES*2) ;
        return 2 ;
    }
    for(i=len-2,off=0; i>=0; i-=2, off++) {
        bte[0] = inStr[i] ;
        bte[1] = inStr[i+1] ;
        sscanf(bte,"%x", &val) ;
        out[off] = (LwU8) val ;
    }
    return 0 ;
}

/*virtual*/ RC TraceOpSec::Run()
{
    RC rc;
    LwRm* pLwRm = m_pTraceModule->GetTest()->GetLwRmPtr();
    UINT32 *pData ;
    LW2080_CTRL_CMD_CIPHER_SESSION_KEY_PARAMS sessionParams;
    LW2080_CTRL_CMD_CIPHER_AES_ENCRYPT_PARAMS contentParams;

    memset(&sessionParams, 0, sizeof(sessionParams));
    memset(&contentParams, 0, sizeof(contentParams));
    if( !m_IsContent ) { /// Query RM for Session Key
        CHECK_RC_MSG( pLwRm->Control(m_Subdev,
                                     LW2080_CTRL_CMD_CIPHER_SESSION_KEY,
                                     &sessionParams,
                                     sizeof (sessionParams)), "TraceOpSec::Run(): pLwRm->Control failed for LW2080_CTRL_CMD_CIPHER_SESSION_KEY.") ;

        InfoPrintf("TraceOpSec::Run(): SESSION_KEY:  0x%08x%08x%08x%08x \n",
                       *((UINT32*)(sessionParams.sKey+0)), *((UINT32*)(sessionParams.sKey+4)), *((UINT32*)(sessionParams.sKey+8)), *((UINT32*)(sessionParams.sKey+12)) ) ;
        pData = (UINT32*)sessionParams.sKey ;
    } else { /// Query RM to wrap Content Key

// If there is no content key, no need to massage session and content keys.
        if( getContentKey( contentParams.pt, m_ContentKey) != 0 ) {
            ErrPrintf("TraceOpSec::Run() Unable to massage Wrapped Content Key. Bad content key specified!\n") ;
            return false ;
        }
        CHECK_RC_MSG(pLwRm->Control(m_Subdev,
                                    LW2080_CTRL_CMD_CIPHER_AES_ENCRYPT,
                                    &contentParams,
                                    sizeof (contentParams)), "TraceOpSec::Run(): pLwRm->Control failed for LW2080_CTRL_CMD_CIPHER_AES_ENCRYPT") ;

        InfoPrintf("TraceOpSec::Run(): AES_ENCRYPT_PARAMS: In: 0x%08x%08x%08x%08x \n",
                   *((UINT32*)(contentParams.pt+0)), *((UINT32*)(contentParams.pt+4)), *((UINT32*)(contentParams.pt+8)), *((UINT32*)(contentParams.pt+12)) ) ;
        InfoPrintf("TraceOpSec::Run(): AES_ENCRYPT_PARAMS: Out: 0x%08x%08x%08x%08x \n",
                   *((UINT32*)(contentParams.ct+0)), *((UINT32*)(contentParams.ct+4)), *((UINT32*)(contentParams.ct+8)), *((UINT32*)(contentParams.ct+12)) ) ;
        pData = (UINT32*)contentParams.ct ;
    }
    // Now use m_pTraceModule and m_ModuleOffset to plug data into the
    // command-stream before it is sent to the channel. Since these are not block-of methods, need to add

    if (! m_pTraceModule->IsFrozenAt(m_ModuleOffset) )
    {
        UINT32 WordInterleave = 1;
        if (m_pTraceModule->GetFileType() == GpuTrace::FT_PUSHBUFFER)
        {
            // G98 SEC traces put keys in pushbuffer, with
            // data strictly alternating with method headers.
            WordInterleave = 2;
        }
        else
        {
            // In a non-pushbuffer surface, don't interleave the data words.
            // Also, free our cached copy of the surface, so the updates hit FB itself,
            // as the surface was already downloaded to GPU.
            m_pTraceModule->ReleaseCachedSurface(m_Subdev);
        }

        for (UINT32 WordIdx = 0; WordIdx < 4; WordIdx++)
        {
            m_pTraceModule->Put032(m_ModuleOffset +
                (WordInterleave*WordIdx*sizeof(UINT32)),
                *(pData+WordIdx), m_Subdev);
        }
    }

    return rc;
}

/*virtual*/ void TraceOpSec::Print() const
{
    RawPrintf("Sec module %s @%x Type:%s (Key:%s)\n",
              m_pTraceModule->GetName().c_str(),
              m_ModuleOffset,
              m_IsContent ? "CONTENT" : "SESSION",
              m_IsContent ? m_ContentKey : "None") ;
}

/*virtual*/ void TraceOpSec::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpSec";
}

//------------------------------------------------------------------------------
static void PrintSubdevices(GpuDevice *pGpuDev, UINT32 subdevMask)
{
    bool bFirstPrinted = false;

    RawPrintf("[ ");
    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); subdev++)
    {
        if (subdevMask & (1 << subdev))
        {
            if (bFirstPrinted)
                RawPrintf(", ");
            RawPrintf("%d", subdev);
            bFirstPrinted = true;
        }
    }
    RawPrintf(" ]");
}
//------------------------------------------------------------------------------
TraceOpEscapeWrite::TraceOpEscapeWrite
(
    GpuDevice *pGpuDev,
    UINT32     subdevMask,
    string     path,
    UINT32     index,
    UINT32     size,
    UINT32     data
)
:   m_pGpuDev(pGpuDev),
    m_SubdevMask(subdevMask),
    m_Path(path),
    m_Index(index),
    m_Size(size),
    m_Data(data)
{
}

/* virtual */ TraceOpEscapeWrite::~TraceOpEscapeWrite()
{
}

/* virtual */ RC TraceOpEscapeWrite::Run()
{
    if (m_SubdevMask != 0)
    {
        for (UINT32 subdev = 0; subdev < m_pGpuDev->GetNumSubdevices(); subdev++)
        {
            if (m_SubdevMask & (1 << subdev))
            {
                GpuSubdevice *pSubdev = m_pGpuDev->GetSubdevice(subdev);
                pSubdev->EscapeWrite(m_Path.c_str(), m_Index, m_Size, m_Data);
            }
        }
    }
    else
    {
        Platform::EscapeWrite(m_Path.c_str(), m_Index, m_Size, m_Data);
    }
    return OK;
}

/* virtual */ void TraceOpEscapeWrite::Print() const
{
    RawPrintf("EscapeWrite %s index=%d size=%d data=0x%0*x on subdevice(s) ",
              m_Path.c_str(),
              m_Index,
              m_Size,
              m_Size * 2,
              m_Data);
    PrintSubdevices(m_pGpuDev, m_SubdevMask);
    RawPrintf("\n");
}

/*virtual*/ void TraceOpEscapeWrite::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEscapeWrite";
}

//------------------------------------------------------------------------------
TraceOpEscapeWriteBuffer::TraceOpEscapeWriteBuffer
(
    GpuDevice *pGpuDev,
    UINT32     subdevMask,
    string     path,
    UINT32     index,
    UINT32     size,
    const char *data
)
:   m_pGpuDev(pGpuDev),
    m_SubdevMask(subdevMask),
    m_Path(path),
    m_Index(index),
    m_Size(size)
{
    if (m_SubdevMask == 0)
    {
        // all subdev
        m_SubdevMask = (1 << m_pGpuDev->GetNumSubdevices()) - 1;
    }

    MASSERT(0 != data);
    m_Data = new char[m_Size + 1];
    memcpy(m_Data, data, m_Size);
    m_Data[m_Size] = 0;
}

/* virtual */ TraceOpEscapeWriteBuffer::~TraceOpEscapeWriteBuffer()
{
    delete[] m_Data;
}

/* virtual */ RC TraceOpEscapeWriteBuffer::Run()
{
    RC rc;

    if (Platform::GetSimulationMode() != Platform::Amodel)
    {
        // NOP for non-amodel
        return OK;
    }

    for (UINT32 subdev = 0; subdev < m_pGpuDev->GetNumSubdevices(); subdev++)
    {
        if (m_SubdevMask & (1 << subdev))
        {
            GpuSubdevice *pSubdev = m_pGpuDev->GetSubdevice(subdev);
            if (pSubdev->EscapeWriteBuffer(m_Path.c_str(), m_Index,
                                           m_Size, m_Data))
            {
                // rc = RC::UNSUPPORTED_FUNCTION;
                // This is only a debugging feature
                // Igoring the failure returned will not hurt anything.
                WarnPrintf("%s: Ignore failure returned by"
                           " EscapeWriteBuffer(\"%s\").\n",
                           __FUNCTION__, m_Path.c_str());
            }
        }
    }

    return rc;
}

/* virtual */ void TraceOpEscapeWriteBuffer::Print() const
{
    RawPrintf("EscapeWriteBuffer %s index=%d size=%d data=0x%p on subdevice(s) ",
              m_Path.c_str(),
              m_Index,
              m_Size,
              m_Data);
    PrintSubdevices(m_pGpuDev, m_SubdevMask);
    RawPrintf("\n");
}

/*virtual*/ void TraceOpEscapeWriteBuffer::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEscapeWriteBuffer";
}

//------------------------------------------------------------------------------
TraceOpEscapeCompare::TraceOpEscapeCompare
(
    GpuDevice *pGpuDev,
    UINT32     subdevMask,
    string     path,
    UINT32     index,
    UINT32     size,
    BoolFunc   compare,
    UINT32     desiredValue,
    UINT32     timeoutMs
)
:   m_pGpuDev(pGpuDev),
    m_SubdevMask(subdevMask),
    m_Path(path),
    m_Index(index),
    m_Size(size),
    m_Compare(compare),
    m_DesiredValue(desiredValue),
    m_TimeoutMs(timeoutMs)
{
}

/* virtual */ TraceOpEscapeCompare::~TraceOpEscapeCompare()
{
}

inline bool TraceOpEscapeCompare::Poll()
{
    UINT32 value;

    if (m_SubdevMask != 0)
    {
        for (UINT32 subdev = 0; subdev < m_pGpuDev->GetNumSubdevices(); subdev++)
        {
            if (m_SubdevMask & (1 << subdev))
            {
                GpuSubdevice *pSubdev = m_pGpuDev->GetSubdevice(subdev);
                pSubdev->EscapeRead(m_Path.c_str(), m_Index, m_Size, &value);
                if (!TraceOp::Compare(value, m_DesiredValue, m_Compare))
                    return false;
            }
        }
    }
    else
    {
        Platform::EscapeRead(m_Path.c_str(), m_Index, m_Size, &value);
        return TraceOp::Compare(value, m_DesiredValue, m_Compare);
    }

    return true;
}

bool TraceOpEscapeCompare_PollFunc(void * vpthis)
{
    TraceOpEscapeCompare * pthis = (TraceOpEscapeCompare *)vpthis;
    return pthis->Poll();
}

/*virtual*/ RC TraceOpEscapeCompare::Run()
{
    if (m_TimeoutMs == TraceOp::TIMEOUT_IMMEDIATE)
    {
        return Poll() ? OK : RC::TIMEOUT_ERROR;
    }
    return POLLWRAP( &TraceOpEscapeCompare_PollFunc, (void *)this, m_TimeoutMs);
}

/* virtual */ void TraceOpEscapeCompare::Print() const
{

    RawPrintf("EscapeCompare %s index=%d size=%d %s %d",
              m_Path.c_str(),
              m_Index,
              m_Size,
              TraceOp::BoolFuncToString(m_Compare),
              m_DesiredValue);

    if (m_TimeoutMs != TraceOp::TIMEOUT_IMMEDIATE)
    {
        RawPrintf(" within %dms",
                  m_TimeoutMs);
    }
    RawPrintf(" on subdevice(s) ");
    PrintSubdevices(m_pGpuDev, m_SubdevMask);
    RawPrintf("\n");
}

/*virtual*/ bool TraceOpEscapeCompare::WillPolling() const
{
    return true;
}

/*virtual*/ void TraceOpEscapeCompare::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEscapeCompare";
}

//------------------------------------------------------------------------------
TraceOpProgramZBCColor::TraceOpProgramZBCColor
(
    Trace3DTest    *test,
    UINT32          colorFormat,
    string          colorFormatString,
    const UINT32   *colorFB,
    const UINT32   *colorDS,
    bool            bSkipL2Table
)
:   m_Test(test),
    m_colorFormat(colorFormat),
    m_colorFormatString(colorFormatString),
    m_colorFB(colorFB, colorFB + ZBCEntriesNum),
    m_colorDS(colorDS, colorDS + ZBCEntriesNum)
{
}

/* virtual */ TraceOpProgramZBCColor::~TraceOpProgramZBCColor()
{
}

/* virtual */ void TraceOpProgramZBCColor::Print() const
{
    RawPrintf("\nProgramming ZBC Color:");
    RawPrintf("\nColor format:%s(0x%08x), \nDS_COLOR_R:0x%08x, DS_COLOR_G:0x%08x, DS_COLOR_B:0x%08x, DS_COLOR_A:0x%08x",
            m_colorFormatString.c_str(),
            m_colorFormat,
            m_colorDS[0],
            m_colorDS[1],
            m_colorDS[2],
            m_colorDS[3]);
    RawPrintf("\nLTC_COLOR_0:0x%08x, LTC_COLOR_1:0x%08x, LTC_COLOR_2:0x%08x, LTC_COLOR_3:0x%08x\n",
            m_colorFB[0],
            m_colorFB[1],
            m_colorFB[2],
            m_colorFB[3]);
}

/*virtual*/ void TraceOpProgramZBCColor::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpProgramZBCColor";
}

/* virtual */ RC TraceOpProgramZBCColor::Run()
{
    LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS colorParams;
    RC     rc = OK;

    memset(&colorParams, 0, sizeof(colorParams));

    for (size_t i = 0; i <  ZBCEntriesNum; ++i)
    {
        colorParams.colorFB[i] = m_colorFB[i];
        colorParams.colorDS[i] = m_colorDS[i];
    }

    colorParams.format = m_colorFormat;

    LwRm* pLwRm = m_Test->GetLwRmPtr();
    LwRm::Handle subdevHandle = pLwRm->GetSubdeviceHandle(
        m_Test->GetBoundGpuDevice()->GetSubdevice(0));
    LwRm::Handle hZBCClearHandle;
    CHECK_RC(pLwRm->Alloc(subdevHandle, &hZBCClearHandle, GF100_ZBC_CLEAR, NULL));

    rc = pLwRm->Control(hZBCClearHandle,
            LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR,
            &colorParams,
            sizeof(LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS));

    pLwRm->Free(hZBCClearHandle);
    return rc;
}

//------------------------------------------------------------------------------
TraceOpProgramZBCDepth::TraceOpProgramZBCDepth
(
    Trace3DTest    *test,
    UINT32          depthFormat,
    string          depthFormatString,
    UINT32          depth,
    bool            bSkipL2Table
)
:   m_Test(test),
    m_depthFormat(depthFormat),
    m_depthFormatString(depthFormatString),
    m_depth(depth)
{
}

/* virtual */ TraceOpProgramZBCDepth::~TraceOpProgramZBCDepth()
{
}

/* virtual */ void TraceOpProgramZBCDepth::Print() const
{
    RawPrintf("\nProgramming ZBC Depth:");
    RawPrintf("\nDepth format:%s(0x%08x), depth:0x%08x\n",
            m_depthFormatString.c_str(),
            m_depthFormat,
            m_depth);
}

/*virtual*/ void TraceOpProgramZBCDepth::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpProgramZBCDepth";
}

/* virtual */ RC TraceOpProgramZBCDepth::Run()
{
    LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS depthParams;
    RC     rc = OK;

    memset(&depthParams, 0, sizeof(depthParams));

    depthParams.depth = m_depth;
    depthParams.format = m_depthFormat;

    LwRm* pLwRm = m_Test->GetLwRmPtr();
    LwRm::Handle subdevHandle = pLwRm->GetSubdeviceHandle(
        m_Test->GetBoundGpuDevice()->GetSubdevice(0));
    LwRm::Handle hZBCClearHandle;
    CHECK_RC(pLwRm->Alloc(subdevHandle, &hZBCClearHandle, GF100_ZBC_CLEAR, NULL));

    rc = pLwRm->Control(hZBCClearHandle,
            LW9096_CTRL_CMD_SET_ZBC_DEPTH_CLEAR,
            &depthParams,
            sizeof(LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS));

    pLwRm->Free(hZBCClearHandle);
    return rc;
}

TraceOpProgramZBCStencil::TraceOpProgramZBCStencil
(
    Trace3DTest    *test,
    UINT32          stencilFormat,
    string          stencilFormatString,
    UINT32          stencil,
    bool            bSkipL2Table
)
:   m_Test(test),
    m_stencilFormat(stencilFormat),
    m_stencilFormatString(stencilFormatString),
    m_stencil(stencil)
{
}

/* virtual */ TraceOpProgramZBCStencil::~TraceOpProgramZBCStencil()
{
}

/* virtual */ void TraceOpProgramZBCStencil::Print() const
{
    RawPrintf("\nProgramming ZBC stencil:");
    RawPrintf("\nstencil format:%s(0x%08x), stencil:0x%08x\n",
            m_stencilFormatString.c_str(),
            m_stencilFormat,
            m_stencil);
}

/*virtual*/ void TraceOpProgramZBCStencil::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpProgramZBCStencil";
}

/*virtual*/ RC TraceOpProgramZBCStencil::Run()
{
    RC rc;
    LW9096_CTRL_SET_ZBC_STENCIL_CLEAR_PARAMS stencilParams;

    memset(&stencilParams, 0, sizeof(stencilParams));

    stencilParams.stencil = m_stencil;
    stencilParams.format = m_stencilFormat;

    LwRm* pLwRm = m_Test->GetLwRmPtr();
    LwRm::Handle subdevHandle = pLwRm->GetSubdeviceHandle(
        m_Test->GetBoundGpuDevice()->GetSubdevice(0));
    LwRm::Handle hZBCClearHandle;
    CHECK_RC(pLwRm->Alloc(subdevHandle, &hZBCClearHandle, GF100_ZBC_CLEAR, NULL));

    rc = pLwRm->Control(hZBCClearHandle,
            LW9096_CTRL_CMD_SET_ZBC_STENCIL_CLEAR,
            &stencilParams,
            sizeof(LW9096_CTRL_SET_ZBC_STENCIL_CLEAR_PARAMS));

    pLwRm->Free(hZBCClearHandle);
    return rc;
}

//------------------------------------------------------------------------------
TraceOpReg::TraceOpReg
(
    GpuDevice *pGpuDev,
    UINT32     subdevMask,
    string     reg,
    UINT32     mask,
    UINT32     data
)
:   m_pGpuDev(pGpuDev),
    m_SubdevMask(subdevMask),
    m_Reg(reg),
    m_Mask(mask),
    m_Data(data)
{
}

TraceOpReg::TraceOpReg
(
    GpuDevice *pGpuDev,
    UINT32     subdevMask,
    string     reg,
    string     field,
    UINT32     data
)
:   m_pGpuDev(pGpuDev),
    m_SubdevMask(subdevMask),
    m_Reg(reg),
    m_Field(field),
    m_Data(data)
{
}

/* virtual */ TraceOpReg::~TraceOpReg()
{
    map<UINT08,PriReg *>::iterator iter;
    for (iter = m_SubdevRegMap.begin(); iter != m_SubdevRegMap.end(); iter++)
    {
        delete iter->second;
    }
    m_SubdevRegMap.clear();
}

RC TraceOpReg::GetRegIndexes(int *pDimension, int *pI, int *pJ)
{
    if (m_Reg.find('(') != string::npos)
    {
        string Index = m_Reg.substr(m_Reg.find('('));
        if ((2 != (*pDimension = sscanf(Index.c_str(), "(%i,%i)", pI, pJ))) &&
            (1 != (*pDimension = sscanf(Index.c_str(), "(%i)", pI))))
        {
            ErrPrintf("Invalid register index specification : \n", Index.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    return OK;
}

RC TraceOpReg::SetupRegs()
{
    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
    RefManual *Manual;
    const RefManualRegister * pReg;
    const RefManualRegisterField *pRegField;
    UINT32 data;
    UINT32 offset;
    UINT32 mask = m_Mask;
    UINT08 subdevCount = 0;
    int    dimension = 0;
    int    indexI = 0;
    int    indexJ = 0;
    string regname = m_Reg.substr(0, m_Reg.find('('));
    RC     rc = OK;

    CHECK_RC(GetRegIndexes(&dimension, &indexI, &indexJ));

    MASSERT(pGpuDevMgr);
    for (GpuSubdevice *pSubdev = pGpuDevMgr->GetFirstGpu();
         pSubdev != NULL; pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
    {
        UINT32 subdev = pSubdev->GetGpuInst();
        if (m_SubdevMask & (1 << subdev))
        {
            Manual = pSubdev->GetRefManual();
            MASSERT(Manual);
            pReg = Manual->FindRegister(regname.c_str());
            if (NULL == pReg)
            {
                ErrPrintf("SetupPriRegs : Invalid register name %s\n",
                          regname.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }

            if (pReg->GetDimensionality() < dimension)
            {
                ErrPrintf("Register %s on subdev %d, %d max dimensions, "
                          "%d specified\n", regname.c_str(), subdev,
                          pReg->GetDimensionality(), dimension);
                return RC::ILWALID_FILE_FORMAT;
            }

            if ((dimension > 0) &&
                ((indexI >= pReg->GetArraySizeI()) ||
                 (indexJ >= pReg->GetArraySizeJ())))
            {
                ErrPrintf("Register %s on subdev %d, index out of range\n",
                          regname.c_str(), subdev);
                return RC::ILWALID_FILE_FORMAT;
            }

            offset = pReg->GetOffset(indexI, indexJ);
            data = m_Data;
            if (!m_Field.empty())
            {
                pRegField = pReg->FindField(m_Field.c_str());
                if (NULL == pRegField)
                {
                    ErrPrintf("SetupPriRegs : Invalid field name %s\n",
                              m_Field.c_str());
                    return RC::ILWALID_FILE_FORMAT;
                }
                mask = pRegField->GetMask();
                data = (m_Data << pRegField->GetLowBit()) & mask;
            }

            PriReg * pPriReg = new PriReg;
            pPriReg->Offset = offset;
            pPriReg->Mask = mask;
            pPriReg->Data = data;
            m_SubdevRegMap[subdev] = pPriReg;
            subdevCount++;
        }
    }

    if (subdevCount == 0)
    {
        ErrPrintf("SetupPriRegs : No matching subdevices found\n",
                  regname.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    return rc;
}

//------------------------------------------------------------------------------
TraceOpPriWrite::TraceOpPriWrite
(
    Trace3DTest *pTest,
    UINT32     subdevMask,
    string     reg,
    UINT32     mask,
    UINT32     data
)
:   TraceOpReg(pTest->GetBoundGpuDevice(), subdevMask, reg, mask, data),
    m_pMod(NULL),
    m_Test(pTest)
{
}

TraceOpPriWrite::TraceOpPriWrite
(
    Trace3DTest *pTest,
    UINT32     subdevMask,
    string     reg,
    string     field,
    UINT32     data
)
:   TraceOpReg(pTest->GetBoundGpuDevice(), subdevMask, reg, field, data),
    m_pMod(NULL),
    m_Test(pTest)
{
}

TraceOpPriWrite::TraceOpPriWrite
(
    Trace3DTest *pTest,
    UINT32       subdevMask,
    string       reg,
    string       type,
    TraceModule *pMod
)
:   TraceOpReg(pTest->GetBoundGpuDevice(), subdevMask, reg, 0xFFFFFFFF, 0),
    m_TypeStr(type),
    m_pMod(pMod),
    m_Test(pTest)
{
}

/* virtual */ TraceOpPriWrite::~TraceOpPriWrite()
{
}

/* virtual */ RC TraceOpPriWrite::Run()
{
    map<UINT08,PriReg *>::iterator iter;
    UINT64         lwrValue = 0;
    RC             rc;

    // Peer offsets are subdevice dependant, so wait until the subdevice
    // loop for peer surfaces to get the offset
    if (m_TypeStr == "OFFSET_HI")
    {
        if (!m_pMod->IsPeer())
            lwrValue = m_pMod->GetOffset() >> 32;
    }
    else if (m_TypeStr == "OFFSET_LO")
    {
        if (!m_pMod->IsPeer())
            lwrValue = m_pMod->GetOffset();
    }
    else if (m_TypeStr == "SIZE")
    {
        lwrValue = m_pMod->GetSize();
    }
    else if (m_TypeStr == "WIDTH")
    {
        lwrValue = m_pMod->GetDmaBuffer()->GetWidth();
    }
    else if (m_TypeStr == "HEIGHT")
    {
        lwrValue = m_pMod->GetDmaBuffer()->GetHeight();
    }
    else if (m_TypeStr == "BUF_OFFSET")
    {
        lwrValue = m_pMod->GetOffsetWithinDmaBuf();
    }
    else if (!m_TypeStr.empty() &&
             (m_TypeStr != "PMU_OFFSET_HI") &&
             (m_TypeStr != "PMU_OFFSET_LO"))
    {
        MASSERT(!"Unknown PRI_WRITE type\n");
        return RC::SOFTWARE_ERROR;
    }

    LWGpuResource *pGpuRes = m_Test->GetGpuResource();

    for (iter = m_SubdevRegMap.begin(); iter != m_SubdevRegMap.end(); iter++)
    {

        // Get the peer offset per subdevice
        // TODO : Add capability to specify which "local" subdevice (subdevice
        // on the device that contains the actual FB allocation) to retrieve
        // the offset from since the offsets are different depending upon
        // which subdevice is targeted
        if ((m_TypeStr == "OFFSET_HI") && m_pMod->IsPeer())
        {
            lwrValue = m_pMod->GetOffset(0, iter->first) >> 32;
        }
        else if ((m_TypeStr == "OFFSET_LO") && m_pMod->IsPeer())
        {
            lwrValue = m_pMod->GetOffset(0, iter->first);
        }
        else if ((m_TypeStr == "PMU_OFFSET_HI") ||
                 (m_TypeStr == "PMU_OFFSET_LO"))
        {
            UINT64 pmuOffset;

            CHECK_RC(m_pMod->GetOffsetPmu(iter->first, &pmuOffset));

            lwrValue = (m_TypeStr == "PMU_OFFSET_HI") ?
                                (pmuOffset >> 32) :
                                pmuOffset;
        }

        if (m_TypeStr.empty())
        {
            if (iter->second->Mask != 0xFFFFFFFF)
            {
                lwrValue = (pGpuRes->RegRd32(iter->second->Offset, iter->first, m_Test->GetSmcEngine())
                            & ~iter->second->Mask);
                lwrValue |= iter->second->Data;
            }
            else
            {
                lwrValue = iter->second->Data;
            }
        }

        pGpuRes->RegWr32(iter->second->Offset, (UINT32)lwrValue, iter->first, m_Test->GetSmcEngine());
    }
    return OK;
}

/* virtual */ void TraceOpPriWrite::Print() const
{
    if (m_Field.empty())
    {
        if (m_TypeStr.empty())
        {
            RawPrintf("PriWrite %s mask=0x%08x data=0x%08x on subdevice(s) ",
                      m_Reg.c_str(), m_Mask, m_Data);
        }
        else
        {
            RawPrintf("PriWrite %s %s of %s on subdevice(s) ",
                      m_Reg.c_str(), m_TypeStr.c_str(), m_pMod->GetName().c_str());
        }
    }
    else
    {
        RawPrintf("PriWrite %s[%s]=0x%x on subdevice(s) ",
                  m_Reg.c_str(), m_Field.c_str(), m_Data);
    }
    PrintSubdevices(m_pGpuDev, m_SubdevMask);
    RawPrintf("\n");
}

/*virtual*/ void TraceOpPriWrite::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpPriWrite";
}

//------------------------------------------------------------------------------
TraceOpPriCompare::TraceOpPriCompare
(
    Trace3DTest *pTest,
    UINT32      subdevMask,
    string      reg,
    UINT32      mask,
    BoolFunc    compare,
    UINT32      desiredData,
    UINT32      timeoutMs
)
:   TraceOpReg(pTest->GetBoundGpuDevice(), subdevMask, reg, mask, desiredData),
    m_Compare(compare),
    m_TimeoutMs(timeoutMs),
    m_Test(pTest)
{
}

TraceOpPriCompare::TraceOpPriCompare
(
    Trace3DTest *pTest,
    UINT32      subdevMask,
    string      reg,
    string      field,
    BoolFunc    compare,
    UINT32      desiredData,
    UINT32      timeoutMs
)
:   TraceOpReg(pTest->GetBoundGpuDevice(), subdevMask, reg, field, desiredData),
    m_Compare(compare),
    m_TimeoutMs(timeoutMs),
    m_Test(pTest)
{
}

/* virtual */ TraceOpPriCompare::~TraceOpPriCompare()
{
}

inline bool TraceOpPriCompare::Poll()
{
    map<UINT08,PriReg *>::iterator iter;
    UINT32         data;
    LWGpuResource *pGpuRes = m_Test->GetGpuResource();

    for (iter = m_SubdevRegMap.begin(); iter != m_SubdevRegMap.end(); iter++)
    {
        data = (pGpuRes->RegRd32(iter->second->Offset, iter->first, m_Test->GetSmcEngine())
                & iter->second->Mask);
        if (!TraceOp::Compare(data, iter->second->Data, m_Compare))
            return false;
    }

    return true;
}

bool TraceOpPriCompare_PollFunc(void * vpthis)
{
    TraceOpPriCompare * pthis = (TraceOpPriCompare *)vpthis;
    return pthis->Poll();
}

/*virtual*/ RC TraceOpPriCompare::Run()
{
    if (m_TimeoutMs == TraceOp::TIMEOUT_IMMEDIATE)
    {
        return Poll() ? OK : RC::TIMEOUT_ERROR;
    }
    return POLLWRAP_HW( &TraceOpPriCompare_PollFunc, (void *)this, m_TimeoutMs);
}

/* virtual */ void TraceOpPriCompare::Print() const
{
    if (m_Field.empty())
    {
        RawPrintf("PriCompare (%s & 0x%08x) %s 0x%08x",
                  m_Reg.c_str(), m_Mask,
                  TraceOp::BoolFuncToString(m_Compare), m_Data);
    }
    else
    {
        RawPrintf("PriCompare %s[%s] %s 0x%x",
                  m_Reg.c_str(), m_Field.c_str(),
                  TraceOp::BoolFuncToString(m_Compare), m_Data);
    }

    if (m_TimeoutMs != TraceOp::TIMEOUT_IMMEDIATE)
    {
        RawPrintf(" within %dms",
                  m_TimeoutMs);
    }
    RawPrintf(" on subdevice(s) ");
    PrintSubdevices(m_pGpuDev, m_SubdevMask);
    RawPrintf("\n");
}

/* virtual */ bool TraceOpPriCompare::WillPolling() const
{
    return true;
}

/*virtual*/ void TraceOpPriCompare::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpPriCompare";
}

//------------------------------------------------------------------------------
// TraceOpCfgWrite
//
TraceOpCfgWrite::TraceOpCfgWrite
(
    GpuDevice   *pGpuDev,
    UINT32       subdevMask,
    string       reg,
    UINT32       mask,
    UINT32       data
)
:   TraceOpReg(pGpuDev, subdevMask, reg, mask, data)
{
}

TraceOpCfgWrite::TraceOpCfgWrite
(
    GpuDevice   *pGpuDev,
    UINT32       subdevMask,
    string       reg,
    string       field,
    UINT32       data
)
:   TraceOpReg(pGpuDev, subdevMask, reg, field, data)
{
}

/* virtual */ TraceOpCfgWrite::~TraceOpCfgWrite()
{
}

/* virtual */ RC TraceOpCfgWrite::Run()
{

    map<UINT08,PriReg *>::iterator iter;
    GpuSubdevice * pSubdev;
    UINT32         lwrValue = 0;
    RC             rc;

    for (iter = m_SubdevRegMap.begin(); iter != m_SubdevRegMap.end(); iter++)
    {
        pSubdev = m_pGpuDev->GetSubdevice(iter->first);

        auto pGpuPcie = pSubdev->GetInterface<Pcie>();
        if (iter->second->Mask != 0xFFFFFFFF)
        {
            CHECK_RC(Platform::PciRead32(pGpuPcie->DomainNumber(),
                                         pGpuPcie->BusNumber(),
                                         pGpuPcie->DeviceNumber(),
                                         pGpuPcie->FunctionNumber(),
                                         iter->second->Offset,
                                         &lwrValue));
            lwrValue &= ~iter->second->Mask;
            lwrValue |= iter->second->Data;
        }
        else
        {
            lwrValue = iter->second->Data;
        }

        CHECK_RC(Platform::PciWrite32(pGpuPcie->DomainNumber(),
                                      pGpuPcie->BusNumber(),
                                      pGpuPcie->DeviceNumber(),
                                      pGpuPcie->FunctionNumber(),
                                      iter->second->Offset,
                                      lwrValue));
    }

    return OK;
}

/* virtual */ void TraceOpCfgWrite::Print() const
{

    if (m_Field.empty())
    {
        RawPrintf("CfgWrite %s mask=0x%08x data=0x%08x on subdevice(s) ",
                  m_Reg.c_str(), m_Mask, m_Data);
    }
    else
    {
        RawPrintf("CfgWrite %s[%s]=0x%x on subdevice(s) ",
                  m_Reg.c_str(), m_Field.c_str(), m_Data);
    }
    PrintSubdevices(m_pGpuDev, m_SubdevMask);
    RawPrintf("\n");
}

/*virtual*/ void TraceOpCfgWrite::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpCfgWrite";
}

//------------------------------------------------------------------------------
// TraceOpCfgCompare
TraceOpCfgCompare::TraceOpCfgCompare
(
    GpuDevice   *pGpuDev,
    UINT32       subdevMask,
    string       reg,
    UINT32       mask,
    BoolFunc     compare,
    UINT32       desiredData,
    UINT32       timeoutMs
)
:   TraceOpReg(pGpuDev, subdevMask, reg, mask, desiredData),
    m_Compare(compare),
    m_TimeoutMs(timeoutMs)
{
}

TraceOpCfgCompare::TraceOpCfgCompare
(
    GpuDevice   *pGpuDev,
    UINT32       subdevMask,
    string       reg,
    string       field,
    BoolFunc     compare,
    UINT32       desiredData,
    UINT32       timeoutMs
)
:   TraceOpReg(pGpuDev, subdevMask, reg, field, desiredData),
    m_Compare(compare),
    m_TimeoutMs(timeoutMs)
{
}

/* virtual */ TraceOpCfgCompare::~TraceOpCfgCompare()
{
}

inline bool TraceOpCfgCompare::Poll()
{
    map<UINT08,PriReg *>::iterator iter;
    GpuSubdevice * pSubdev;
    UINT32         data;

    for (iter = m_SubdevRegMap.begin(); iter != m_SubdevRegMap.end(); iter++)
    {
        pSubdev = m_pGpuDev->GetSubdevice(iter->first);
        auto pGpuPcie = pSubdev->GetInterface<Pcie>();
        m_PollRc = Platform::PciRead32(pGpuPcie->DomainNumber(),
                                       pGpuPcie->BusNumber(),
                                       pGpuPcie->DeviceNumber(),
                                       pGpuPcie->FunctionNumber(),
                                       iter->second->Offset,
                                       &data);
        if (m_PollRc == OK)
        {
            data &= iter->second->Mask;
            if (!TraceOp::Compare(data, iter->second->Data, m_Compare))
                return false;
        }
        else
        {
            // If the read fails flag the poll as complete so that polling
            // will stop and the error can be reported
            return true;
        }
    }

    return true;
}

bool TraceOpCfgCompare_PollFunc(void * vpthis)
{
    TraceOpCfgCompare * pthis = (TraceOpCfgCompare *)vpthis;
    return pthis->Poll();
}

/*virtual*/ RC TraceOpCfgCompare::Run()
{
    RC rc;

    m_PollRc = OK;

    if (m_TimeoutMs == TraceOp::TIMEOUT_IMMEDIATE)
    {
        rc = Poll() ? OK : RC::TIMEOUT_ERROR;
    }
    else
    {
        rc = POLLWRAP_HW(&TraceOpCfgCompare_PollFunc,
                       (void *)this, m_TimeoutMs);
    }

    return (rc == OK) ? m_PollRc : rc;
}

/* virtual */ bool TraceOpCfgCompare::WillPolling() const
{
    return true;
}

/* virtual */ void TraceOpCfgCompare::Print() const
{
    if (m_Field.empty())
    {
        RawPrintf("CfgCompare (%s & 0x%08x) %s 0x%08x",
                  m_Reg.c_str(), m_Mask,
                  TraceOp::BoolFuncToString(m_Compare), m_Data);
    }
    else
    {
        RawPrintf("CfgCompare %s[%s] %s 0x%x",
                  m_Reg.c_str(), m_Field.c_str(),
                  TraceOp::BoolFuncToString(m_Compare), m_Data);
    }

    if (m_TimeoutMs != TraceOp::TIMEOUT_IMMEDIATE)
    {
        RawPrintf(" within %dms",
                  m_TimeoutMs);
    }
    RawPrintf(" on subdevice(s) ");
    PrintSubdevices(m_pGpuDev, m_SubdevMask);
    RawPrintf("\n");
}

/*virtual*/ void TraceOpCfgCompare::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpCfgCompare";
}

TraceOpWaitPmuCmd::TraceOpWaitPmuCmd(UINT32 cmdId, UINT32 timeoutMs, LwRm* pLwRm)
: m_CmdId(cmdId),
  m_TimeoutMs(timeoutMs),
  m_pLwRm(pLwRm),
  m_bSetup(false),
  m_PollRc(OK),
  m_pSubdev(0)
{
}

TraceOpWaitPmuCmd::~TraceOpWaitPmuCmd()
{
}

bool TraceOpWaitPmuCmd::Poll()
{
    PMU *pPmu;

    m_PollRc = m_pSubdev->GetPmu(&pPmu);
    if (m_PollRc != OK)
        return true;

    m_PollRc = RC::SOFTWARE_ERROR;
    ErrPrintf("PMU Command sequence descriptor %d does not exist!!!\n",
              0xffffffff);
    return true;
}

/*virtual*/ RC TraceOpWaitPmuCmd::Run()
{
    if (!m_bSetup)
    {
        ErrPrintf("TraceOpWaitPmuCmd : Command Sequence not set!!!\n");
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        InfoPrintf("TraceOpWaitPmuCmd : No PMU Command expected "
                   "for command ID %d!!!\n", m_CmdId);
        return OK;
    }
}

/* virtual */ void TraceOpWaitPmuCmd::Print() const
{
    if (m_bSetup)
    {
        RawPrintf("WaitPmuCmd cmdId=%d, seqDesc=%d",
                  m_CmdId, 0xffffffff);
    }
    else
    {
        RawPrintf("WaitPmuCmd cmdId=%d, seqDesc=NOT SET",
                  m_CmdId);
    }

    if (m_TimeoutMs == TraceOp::TIMEOUT_NONE)
    {
        RawPrintf(" (no timeout)");
    }
    else if (m_TimeoutMs != TraceOp::TIMEOUT_IMMEDIATE)
    {
        RawPrintf(" within %dms",
                  m_TimeoutMs);
    }
    RawPrintf("\n");
}

/*virtual*/ bool TraceOpWaitPmuCmd::WillPolling() const
{
    return true;
}

/*virtual*/ void TraceOpWaitPmuCmd::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpWaitPmuCmd";
}

void TraceOpWaitPmuCmd::Setup(GpuSubdevice *pSubdev)
{
    m_pSubdev = pSubdev;
    m_bSetup = true;
}

//------------------------------------------------------------------------------
// TraceOpId: abstract base class, represents a single operation from a parsed
// test.hdr file that contains a particular operation Id.
//
TraceOpPmu::TraceOpPmu(UINT32 CmdId)
:   m_CmdId(CmdId),
    m_pWaitOp(0)
{
}

/*virtual*/ TraceOpPmu::~TraceOpPmu()
{
}

TraceOpSysMemCacheCtrl::TraceOpSysMemCacheCtrl
(
    UINT32 writeMode,
    UINT32 powerState,
    UINT32 rcmState,
    UINT32 vgaMode,
    GpuSubdevice *gpuSubdev,
    UINT32 cmdId,
    LwRm* pLwRm
)
:   TraceOpPmu(cmdId),
    m_WriteMode(writeMode),
    m_PowerState(powerState),
    m_RCMState(rcmState),
    m_VGAMode(vgaMode),
    m_GpuSubdev(gpuSubdev),
    m_pLwRm(pLwRm)
{
}

/* virtual */ TraceOpSysMemCacheCtrl::~TraceOpSysMemCacheCtrl()
{
}

/* virtual */ RC TraceOpSysMemCacheCtrl::Run()
{
    RC rc = OK;
    LwRm::Handle m_hSubdevDiag;

    LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS param = {0};
    param.writeMode = m_WriteMode;
    param.powerState = m_PowerState;
    param.rcmState = m_RCMState;
    param.vgaCacheMode = m_VGAMode;

    m_hSubdevDiag = m_GpuSubdev->GetSubdeviceDiag();

    rc = m_pLwRm->Control(m_hSubdevDiag,
        LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
        &param,
        sizeof(LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS));

    if (rc != OK)
    {
        ErrPrintf("TraceOpSysMemCacheCtrl::Run: Set cache status via RM call fails\n");
    }
    else if (GetWaitOp())
    {
        GetWaitOp()->Setup(m_GpuSubdev);
    }

    return rc;
}

/* virtual */ void TraceOpSysMemCacheCtrl::Print() const
{
    string powerState[3] = {"Default", "Enabled", "Disabled"};
    string writeMode[3]  = {"Default", "WriteThrough", "WriteBack"};
    string rcmState[4]   = {"Default", "Full", "Reduced", "Zero_Cache"};
    string vgaMode[3]    = {"Default", "Enabled", "Disabled"};

    RawPrintf("SysMem Cache status: powerstate = %s, writemode = %s, rcmstate = %s, vgamode = %s",
        powerState[m_PowerState].c_str(), writeMode[m_WriteMode].c_str(),
        rcmState[m_RCMState].c_str(), vgaMode[m_VGAMode].c_str());

    if (GetWaitOp())
    {
        RawPrintf(" (cmdId = %d)", GetCmdId());
    }
    RawPrintf("\n\n");
}

/*virtual*/ void TraceOpSysMemCacheCtrl::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpSysMemCacheCtrl";
}

TraceOpFlushCacheCtrl::TraceOpFlushCacheCtrl
(
    string &Surf,
    UINT32 WriteBack,
    UINT32 Ilwalidate,
    UINT32 Mode,
    Trace3DTest *Test,
    GpuSubdevice *gpuSubdev
)
:   m_Surf(Surf),
    m_Aperture(0), // Get it from querying surface info
    m_WriteBack(WriteBack),
    m_Ilwalidate(Ilwalidate),
    m_Mode(Mode),
    m_Test(Test),
    m_GpuSubdev(gpuSubdev)
{
}

TraceOpFlushCacheCtrl::~TraceOpFlushCacheCtrl()
{
}

RC TraceOpFlushCacheCtrl::Run()
{
    RC rc = OK;
    LwRm* pLwRm = m_Test->GetLwRmPtr();

    TraceModule::SomeKindOfSurface skos = {0};
    MdiagSurf *surfObj = 0;
    GenericTraceModule *tmp = new GenericTraceModule(m_Test, "noname_for_tmp_use",
        GpuTrace::FT_TEXTURE, 0x400);
    tmp->StringToSomeKindOfSurface(0, Gpu::MaxNumSubdevices, m_Surf, &skos);
    surfObj = tmp->SKOS2LWSurfPtr(&skos);
    delete tmp;
    if (skos.CtxDmaOffset==0 || skos.Size==0 || surfObj==0)
    {
        ErrPrintf("TraceOpFlushCacheCtrl::Run: Can't find surface %s\n", m_Surf.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    UINT32 size = skos.Size;
    UINT64 address = 0;
    if (skos.Location == Memory::Fb)
    {
        m_Aperture = LW2080_CTRL_FB_FLUSH_GPU_CACHE_FLAGS_APERTURE_VIDEO_MEMORY;
        address = surfObj->GetVidMemOffset();
    }
    else
    {
        m_Aperture = LW2080_CTRL_FB_FLUSH_GPU_CACHE_FLAGS_APERTURE_SYSTEM_MEMORY;
        // Query physical address for system memory
        LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS param;
        memset(&param, 0, sizeof(param));
        RC rc = pLwRm->Control(surfObj->GetMemHandle(), LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
            &param, sizeof(param));
        if (rc != OK)
        {
            ErrPrintf("%s\n", rc.Message());
            MASSERT(0);
        }
        address = param.memOffset;
    }

    LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS param;
    memset(&param, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
    // Now RM aligns the size and address
    param.addressArray[0] = address;
    param.addressArraySize = 1;
    param.addressAlign = 1;
    param.memBlockSizeBytes = size;
    param.flags = DRF_NUM(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE,   m_Aperture)   |
                  DRF_NUM(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, m_WriteBack)  |
                  DRF_NUM(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, m_Ilwalidate) |
                  DRF_NUM(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, m_Mode);
    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_GpuSubdev),
                        LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
                        &param,
                        sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
    if (rc != OK)
    {
        ErrPrintf("TraceOpFlushCacheCtrl::Run: Flush memory fails\n");
    }

    return rc;
}

void TraceOpFlushCacheCtrl::Print() const
{
    RawPrintf("Flush surface %s with flag aperture: %s, writeback: %s, ilwalidate: %s, mode: %s\n",
        m_Surf.c_str(),
        m_Aperture == 0 ? "VIDEO" : "SYSTEM",
        m_WriteBack == 0 ? "NO" : "YES",
        m_Ilwalidate == 0 ? "No" : "YES",
        m_Mode == 0 ? "ADDRESS_ARRAY" : "FULL_CACHE");
}

/*virtual*/ void TraceOpFlushCacheCtrl::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpFlushCacheCtrl";
}

TraceOpAllocSurface::TraceOpAllocSurface
(
    SurfaceTraceModule *module,
    Trace3DTest *test,
    GpuTrace *trace,
    TraceFileMgr *traceFileMgr
)
:   m_Module(module),
    m_Test(test),
    m_TraceFileMgr(traceFileMgr)
{
    MASSERT(module);
    SetType(module->GetTraceOpType());
}

TraceOpAllocSurface::~TraceOpAllocSurface()
{
}

RC TraceOpAllocSurface::EscWriteAllocSurfaceInfo()
{
    RC rc;

    //
    // Bug 838804
    // Amodel need some information to dump meta data
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        const string name = m_Module->GetName();
        UINT32 size = sizeof(UINT64) + sizeof(UINT64) + name.size() + 1;
        if (m_Test->GetGpuResource()->IsMultiVasSupported())
        {
            // vas id
            size += sizeof(UINT32);
        }
        vector<UINT08> escwriteBuffer(size);
        UINT08* buf = &escwriteBuffer[0];

        // virtual address
        MdiagSurf *surface = m_Module->GetDmaBufferNonConst();
        *(UINT64*)buf = surface->GetCtxDmaOffsetGpu() +
                        surface->GetExtraAllocSize();
        buf += sizeof(UINT64);

        // module size
        *(UINT64*)buf = static_cast<UINT64>(m_Module->GetSize());
        buf += sizeof(UINT64);

        // surface name. end with null to avoid accident in amodel
        memcpy(buf, name.c_str(), name.size());
        buf += name.size();
        *buf = '\0';
        ++buf;
        if (m_Test->GetGpuResource()->IsMultiVasSupported())
        {
            LwRm* pLwRm = m_Test->GetLwRmPtr();
            LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS params = {0};
            params.hVASpace = m_Module->GetVASpaceHandle();
            CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(m_Module->GetGpuDev()),
                LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS, &params, sizeof (params)));
            const UINT32 vaSpaceId = static_cast<UINT32>(params.vaSpaceId);
            *(UINT32 *)buf = vaSpaceId;

             surface->SetVASpaceId(params.vaSpaceId);
        }

        // key = "ALLOC_SURFACE"
        const char* key = "ALLOC_SURFACE";
        GpuDevice* gpudev = m_Test->GetBoundGpuDevice();
        for (UINT32 subdev = 0; subdev < gpudev->GetNumSubdevices(); subdev++)
        {
            // it doesn't matter to ignore the return value of escapewrite
            GpuSubdevice* gpuSubdev = gpudev->GetSubdevice(subdev);
            gpuSubdev->EscapeWriteBuffer(key, 0, size, &escwriteBuffer[0]);
        }

        return rc;
    }

    return rc;
}

RC TraceOpAllocSurface::Run()
{
    ChiplibOpScope newScope("Alloc dynamic surface " + m_Module->GetName(), NON_IRQ,
                            ChiplibOpScope::SCOPE_COMMON, NULL);

    RC rc = OK;

    if (!m_Module->IsDynamic())
    {
        ErrPrintf("TraceOpAllocSurface::Run : module %s is not dynamic\n",
            m_Module->GetName().c_str());

        return RC::SOFTWARE_ERROR;
    }

    // Color and Z surfaces need to be enabled in the surface manager.
    if (m_Module->IsColorOrZ())
    {
        MdiagSurf *surface = m_Test->GetSurfaceMgr()->EnableSurface(
            m_Module->GetSurfaceType(), m_Module->GetParameterSurface(),
            m_Module->GetLoopback(), m_Module->GetPeerIDs());

        m_Module->SaveTrace3DSurface(surface);
    }

    rc = m_Module->LoadFromFile(m_TraceFileMgr);

    if (rc != OK)
    {
        ErrPrintf("TraceOpAllocSurface::Run : module->LoadFromFile() failed\n");
        return rc;
    }

    if (!m_Module->IsSurfaceView())
    {
        // Color and Z surfaces should be allocated by the surface manager.
        if (m_Module->IsColorOrZ())
        {
            rc = m_Test->GetSurfaceMgr()->AllocateSurface(
                m_Module->GetSurfaceType(), m_Test->GetBuffInfo());

            if (rc != OK)
            {
                ErrPrintf("TraceOpAllocSurface::Run : %s allocation failed\n",
                    m_Module->GetName().c_str());

                return rc;
            }
        }
        else
        {
            MdiagSurf *surface = m_Module->GetDmaBufferNonConst();

            rc = m_Test->SetIndividualBufferAttr(surface);
            if (rc != OK)
            {
                ErrPrintf("TraceOpAllocSurface::Run : m_Test->SetIndividualBufferAttr() failed\n");
                return rc;
            }

            if (m_Module->IsShared())
            {
                LWGpuResource * pGpuResource = m_Test->GetGpuResource();
                SharedSurfaceController * pSurfController = pGpuResource->GetSharedSurfaceController();
                LwRm::Handle hVaSpace = surface->GetGpuVASpace();
                LwRm * pLwRm = surface->GetLwRmPtr();
                MdiagSurf * sharedSurf = pSurfController->GetSharedSurf(surface->GetName().c_str(), hVaSpace, pLwRm); 
                if (sharedSurf)
                {
                    // Prevent ALLOC_SURFACE and ALLOC_PHYSICAL has same name and same lwrm client
                    if ((sharedSurf->HasMap() && surface->HasMap()) || 
                            (sharedSurf->HasPhysical() && surface->HasPhysical()))
                    {
                        // ToDo:: Whethre need to check the gpudevice is same
                        *surface = *sharedSurf;
                        m_Module->GetCachedSurface()->SetWasDownloaded();
                        surface->SetDmaBufferAlloc(true); // no need to call Surface3D::Free
                        return OK;
                    }
                }
            }

            if (Utl::HasInstance())
            {
                Utl::Instance()->AddTestSurface(m_Test, surface);
                Utl::Instance()->TriggerSurfaceAllocationEvent(surface, m_Test);
            }

            rc = m_Module->AllocateSurface(surface, m_Test->GetBoundGpuDevice());
            if (m_Module->IsShared())
            {
                surface->SetDmaBufferAlloc(true); // no need to call Surface3D::Free
            }

            if (rc != OK)
            {
                ErrPrintf("TraceOpAllocSurface::Run : surface->Alloc() failed\n");
                return rc;
            }
            if (surface->HasVirtual())
            {
                TraceChannel *pTraceCh = 0;
                if (surface->IsHostReflectedSurf())
                {
                    string chName = surface->GetGpuManagedChName();
                    pTraceCh = m_Test->GetChannel(chName, GpuTrace::GPU_MANAGED);
                    if (!pTraceCh || !pTraceCh->GetCh())
                    {
                        ErrPrintf("TraceOpAllocSurface::Run : "
                                "Invalid Gpu managed channel(%s) for host reflected surface\n",
                                chName.c_str());

                        return RC::SOFTWARE_ERROR;
                    }
                }
                else
                {
                    pTraceCh = m_Module->GetTraceChannel();
                    if (pTraceCh == 0)
                    {
                        WarnPrintf("TraceOpAllocSurface::Run : Module %s doesn't have a channel!\n",
                                m_Module->GetName().c_str());
                        pTraceCh = m_Test->GetDefaultChannelByVAS(m_Module->GetVASpaceHandle());
                        CHECK_RC(m_Module->SetTraceChannel(pTraceCh));
                    }
                }

                rc = surface->BindGpuChannel(pTraceCh->GetCh()->ChannelHandle());
            }

            if (rc != OK)
            {
                ErrPrintf("TraceOpAllocSurface::Run : surface->BindGpuChannel() failed\n");
                return rc;
            }
        }
    }

    rc = m_Module->Allocate();

    if (rc != OK)
    {
        ErrPrintf("TraceOpAllocSurface::Run : module->Allocate() failed\n");
        return rc;
    }

    if(!m_Test->GetParam()->ParamPresent("-silence_surface_info"))
        m_Test->GetBuffInfo()->Print(nullptr, m_Test->GetBoundGpuDevice());

    // Color/z surface views need to be handled after the
    // ViewTraceModule::Allocate call so that their parent info
    // is present.
    if (m_Module->IsSurfaceView() && m_Module->IsColorOrZ())
    {
        rc = m_Test->GetSurfaceMgr()->AllocateSurface(
            m_Module->GetSurfaceType(), m_Test->GetBuffInfo());

        if (rc != OK)
        {
            ErrPrintf("TraceOpAllocSurface::Run : %s allocation failed\n",
                m_Module->GetName().c_str());

            return rc;
        }
    }

    if (!m_Module->IsSurfaceView())
    {
        MdiagSurf *surface = m_Module->GetDmaBufferNonConst();

        const UINT64 offset = surface->GetCtxDmaOffsetGpu() +
            surface->GetExtraAllocSize();

        // If the buffer has a fixed virtual address constraint, make sure
        // that it was allocated at the requested address.
        if (surface->HasFixedVirtAddr() > 0)
        {
            if (offset != surface->GetFixedVirtAddr())
            {
                ErrPrintf("%s allocation failed: requested address is "
                    "0x%llx, obtained address is 0x%llx\n",
                    m_Module->GetName().c_str(),
                    surface->GetFixedVirtAddr(),
                    offset);

                return RC::SOFTWARE_ERROR;
            }
        }

        // If there is a virtual address range constraint, verify that the
        // surface was allocated within the specified virtual address range.
        else if (surface->HasVirtAddrRange())
        {
            if ((offset < surface->GetVirtAddrRangeMin()) ||
                ((offset + (m_Module->GetSize() - 1)) >
                    surface->GetVirtAddrRangeMax()))
            {
                ErrPrintf("%s allocation failed: requested address is "
                    "between 0x%llx and 0x%llx, "
                    "obtained address is 0x%llx\n",
                    m_Module->GetName().c_str(),
                    surface->GetVirtAddrRangeMin(),
                    surface->GetVirtAddrRangeMax(),
                    offset);

                return RC::SOFTWARE_ERROR;
            }
        }
    }

    rc = m_Module->Download();

    if (rc != OK)
    {
        ErrPrintf("TraceOpAllocSurface::Run : module->Dowload() failed\n");
        return rc;
    }

    // Color and Z surfaces need to be enabled in the surface manager.
    if (m_Module->IsColorOrZ())
    {
        m_Test->AddRenderTargetVerif(m_Module->GetSurfaceType());
    }

    CHECK_RC(EscWriteAllocSurfaceInfo());

    return rc;
}

void TraceOpAllocSurface::Print() const
{
    RawPrintf("Alloc buffer for module %s\n", m_Module->GetName().c_str());
}

void TraceOpAllocSurface::GetTraceopProperties( const char * * pName,
                                                map<string,string> & properties ) const
{
    *pName = "TraceOpAllocSurface";

    properties[ "module" ] = m_Module->GetName();
    properties[ "file" ] = m_Module->GetFilename();
    properties[ "type" ] = GpuTrace::GetFileTypeData( m_Module->GetFileType() ).Description;
    properties[ "size" ] = Utility::StrPrintf( "0x%llx", m_Module->GetSize() );
}

TraceOpCheckDynSurf::TraceOpCheckDynSurf
(
    TraceModule* module,
    Trace3DTest* test,
    const string& sufixName,
    const vector<TraceChannel*>& wfiChannels
)
:   m_Module(module),
    m_Test(test),
    m_SufixName(sufixName)
{
    MASSERT(m_Test != 0);
    MASSERT(m_Module != 0);

    m_WfiChannels = wfiChannels;
}

TraceOpCheckDynSurf::~TraceOpCheckDynSurf()
{
}

RC TraceOpCheckDynSurf::Run()
{
    RC rc = OK;

    TraceModule::ModCheckInfo checkInfo;
    GpuVerif *gpuVerif = 0;
    UINT32 subDevNum = m_Test->GetBoundGpuDevice()->GetNumSubdevices();

    if (m_Module->IsColorOrZ())
    {
        TraceChannel *grChannel = m_Test->GetGrChannel();

        if (grChannel != 0)
        {
            gpuVerif = grChannel->GetGpuVerif();
        }
    }
    else
    {
        if (!m_Module->GetCheckInfo(checkInfo))
        {
            ErrPrintf("%s: No CRC checkInfo attached to the surface %s.\n",
                __FUNCTION__, m_Module->GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }

        MASSERT(checkInfo.CheckMethod != NO_CHECK);
        MASSERT(checkInfo.pTraceCh != 0);

        gpuVerif = checkInfo.pTraceCh->GetGpuVerif();
    }

    if (gpuVerif == 0)
    {
        ErrPrintf("%s: No GpuVerif attached to the surface %s.\n",
            __FUNCTION__, m_Module->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    ITestStateReport* report = m_Test->GetTestIStateReport();

    vector<TraceChannel*>::const_iterator cit = m_WfiChannels.begin();
    for (; cit != m_WfiChannels.end(); cit++)
    {
        CHECK_RC((*cit)->GetCh()->WaitForIdleRC());
    }

    for (UINT32 subdev = 0; subdev < subDevNum; subdev++)
    {
        TestEnums::TEST_STATUS testStatus;

        string checkName;

        if (m_Module->IsColorOrZ())
        {
            MdiagSurf *surface = m_Module->GetDmaBufferNonConst();
            checkName = surface->GetName();
        }
        else
        {
            checkName = checkInfo.CheckFilename;
        }

        testStatus = gpuVerif->DoSurfaceCrcCheck(report, checkName,
            m_SufixName, subdev);

        if (testStatus != TestEnums::TEST_SUCCEEDED)
        {
            // Even some dynamic CRC checking fail, we don't interrupt
            // the running, one warning should be enough
            WarnPrintf("Dynamic CRC checking fails on %s for subdev %d\n",
                checkName.c_str(), subdev);
        }
        if (TestEnums::TEST_NOT_STARTED == (TestEnums::TEST_STATUS)m_Test->GetStatus() ||
            TestEnums::TEST_SUCCEEDED   == (TestEnums::TEST_STATUS)m_Test->GetStatus())
        {
            // Override the status initially or when test fails. Since dynamic CRC check
            // may have more than one failures, then only the first error will be recorded
            m_Test->SetStatus(testStatus);
        }
    }

    return OK;
}

void TraceOpCheckDynSurf::Print() const
{
    RawPrintf("Check dynamic surface for module %s\n", m_Module->GetName().c_str());
}

void TraceOpCheckDynSurf::GetTraceopProperties( const char * * pName,
                                                map<string,string> & properties ) const
{
    *pName = "TraceOpCheckDynSurf";

    properties["module"] = m_Module->GetName();
}

TraceOpRelocSurfaceProperty::TraceOpRelocSurfaceProperty
(
    UINT32 offset,
    UINT32 mask_out,
    UINT32 mask_in,
    bool   is_add,
    string &property,
    const string &surfaceName,
    SurfaceType surface_type,
    TraceModule *trace_module,
    bool   swap,
    bool   progZtAsCt0,
    TraceModule::SubdevMaskDataMap *sub_dev_map
)
:   m_Offset(offset),
    m_MaskOut(mask_out),
    m_MaskIn(mask_in),
    m_IsAdd(is_add),
    m_Property(property),
    m_SurfaceName(surfaceName),
    m_SurfaceType(surface_type),
    m_TraceModule(trace_module),
    m_Swap(swap),
    m_ProgZtAsCt0(progZtAsCt0),
    m_SubDevMap(sub_dev_map)
{
}

/*virtual*/ TraceOpRelocSurfaceProperty::~TraceOpRelocSurfaceProperty()
{
}

/*virtual*/ RC TraceOpRelocSurfaceProperty::Run()
{
    TraceRelocationSurfaceProperty reloc
    (
        m_MaskOut,
        m_MaskIn,
        m_Offset,
        m_Property,
        m_SurfaceName,
        m_SurfaceType,
        0,
        m_IsAdd,
        m_Swap,
        m_ProgZtAsCt0,
        m_SubDevMap
    );

    // TODO: Need SLI support
    reloc.DoOffset(m_TraceModule, 0);

    return OK;
}

/*virtual*/ void TraceOpRelocSurfaceProperty::Print() const
{
    RawPrintf("Update RELOC_SURFACE_PROPERTY for %s, offset = 0x%x, property = %s, mask_out = 0x%x, mask_in = 0x%x, write_mode = %s\n",
        m_TraceModule->GetName().c_str(),
        m_Offset,
        m_Property.c_str(),
        m_MaskOut,
        m_MaskIn,
        m_IsAdd ? "ADD" : "REPLACE");
}

/*virtual*/ void TraceOpRelocSurfaceProperty::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpRelocSurfaceProperty";
}

TraceOpFreeSurface::TraceOpFreeSurface
(
    TraceModule *module,
    Trace3DTest *test,
    bool dumpBeforeFree
)
:   m_Module(module), m_Test(test), m_DumpBeforeFree(dumpBeforeFree)
{
}

TraceOpFreeSurface::~TraceOpFreeSurface()
{
}

RC TraceOpFreeSurface::Run()
{
    RC rc = OK;

    if (m_DumpBeforeFree)
    {
        InfoPrintf("Dumping surface %s before free.\n", m_Module->GetName().c_str());

        GpuVerif *gpuVerif = 0;
        UINT32 subDevNum = m_Test->GetBoundGpuDevice()->GetNumSubdevices();

        if (m_Module->IsColorOrZ())
        {
            TraceChannel *grChannel = m_Test->GetGrChannel();

            if (grChannel != 0)
            {
                gpuVerif = grChannel->GetGpuVerif();
            }
        }
        else
        {
            TraceChannel *ch = m_Module->GetTraceChannel();
            MASSERT(ch != 0);
            gpuVerif = ch->GetGpuVerif();
        }
        MdiagSurf *dma = m_Module->GetDmaBufferNonConst();
        if (dma)
        {
            string fname = m_Module->GetName();

            for (UINT32 subdev = 0; subdev < subDevNum; ++subdev)
            {
                CHECK_RC(gpuVerif->DumpSurfOrBuf(dma, fname, subdev));
            }
        }
    }

    if(!m_Test->GetParam()->ParamPresent("-silence_surface_info"))
        InfoPrintf("Freeing %s\n", m_Module->GetName().c_str());

    // Notify any objects dependent on this module that its
    // buffer is about to be freed.
    m_Module->Release();

    MdiagSurf *surface = m_Module->GetDmaBufferNonConst();

    if (Utl::HasInstance())
    {
        Utl::Instance()->RemoveTestSurface(m_Test, surface);
    }

    // Update the BuffInfo class current buffer memory total.
    m_Test->GetBuffInfo()->FreeBufferMemory(surface->GetSize());

    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        surface->SendFreeAllocRangeEscapeWrite();
    }

    // Free the surface.
    surface->Free();

    return OK;
}

void TraceOpFreeSurface::Print() const
{
    RawPrintf("Delete buffer for module %s\n", m_Module->GetName().c_str());
}

/*virtual*/ void TraceOpFreeSurface::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpFreeSurface";

    properties[ "module" ] = m_Module->GetName();
    properties[ "type" ] = GpuTrace::GetFileTypeData( m_Module->GetFileType() ).Description;
    properties[ "size" ] = Utility::StrPrintf( "0x%llx", m_Module->GetSize() );
}

TraceOpDynTex::TraceOpDynTex
(
    TraceModule *target,
    string surfname,
    TextureMode mode,
    UINT32 index,
    bool no_offset,
    bool no_surf_attr,
    UINT32 peer,
    bool center_spoof,
    TextureHeader::HeaderFormat headerFormat,
    bool l1_promote
)
:   m_Target(target),
    m_SurfName(surfname),
    m_Mode(mode),
    m_Index(index),
    m_NoOffset(no_offset),
    m_NoSurfAttr(no_surf_attr),
    m_Peer(peer),
    m_CenterSpoof(center_spoof),
    m_TextureHeaderFormat(headerFormat),
    m_L1Promote(l1_promote)
{
}

TraceOpDynTex::~TraceOpDynTex()
{
}

RC TraceOpDynTex::Run()
{
    // Texture header pool file cannot be peered, so patch sudev 0 is OK
    TraceRelocationDynTex reloc(m_SurfName, m_Mode, m_Index, m_NoOffset,
        m_NoSurfAttr, 0, m_Peer, m_CenterSpoof, m_TextureHeaderFormat, m_L1Promote);
    reloc.DoOffset(m_Target, 0);
    return OK;
}

void TraceOpDynTex::Print() const
{
    RawPrintf("Patch texture header pool. SurfName: %s, TextureMode: %d, Index: %d, "
        "No_Offset: %s, No_Surf_Attr: %s, PeerNum: %d, OptimalL1Promote: %s\n",
        m_SurfName.c_str(), m_Mode, m_Index, m_NoOffset?"true":"false",
        m_NoSurfAttr?"true":"false", m_Peer, m_L1Promote?"true":"false");
}

/*virtual*/ void TraceOpDynTex::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpDynTex";
}

TraceOpFile::TraceOpFile
(
    TraceModule *target,
    UINT32 offset,
    TraceModule *module,
    UINT32 array_index,
    UINT32 lwbemap_face,
    UINT32 miplevel,
    UINT32 x,
    UINT32 y,
    UINT32 z,
    UINT32 peerNum,
    UINT64 mask_out,
    UINT64 mask_in,
    bool swap
)
:   m_Target(target),
    m_Offset(offset),
    m_Module(module),
    m_ArrayIndex(array_index),
    m_LwbemapFace(lwbemap_face),
    m_Miplevel(miplevel),
    m_X(x),
    m_Y(y),
    m_Z(z),
    m_PeerNum(peerNum),
    m_MaskOut(mask_out),
    m_MaskIn(mask_in),
    m_Swap(swap)
{
}

TraceOpFile::~TraceOpFile()
{
}

RC TraceOpFile::Run()
{
    TraceRelocationFile reloc(m_Offset, m_Module, m_MaskOut, m_MaskIn, m_Swap, m_ArrayIndex,
        m_LwbemapFace, m_Miplevel, m_X, m_Y, m_Z, 0, m_PeerNum);
    reloc.DoOffset(m_Target, 0); // Not SLI aware
    return OK;
}

void TraceOpFile::Print() const
{
    RawPrintf("Relocating a texture info. Target: %s, offset: 0x%x, module: %s,"
    " array_index: %d, lwbemap_face: %d, miplevel: %d, x: %d, y: %d, z: %d, "
    "peerNum: %d\n", m_Target->GetName().c_str(), m_Offset,
    m_Module->GetName().c_str(), m_ArrayIndex, m_LwbemapFace,
    m_Miplevel, m_X, m_Y, m_Z, m_PeerNum);
}

/*virtual*/ void TraceOpFile::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpFile";
}

TraceOpSurface::TraceOpSurface
(
    TraceModule *target,
    UINT32 offset,
    string &surf,
    UINT32 array_index,
    UINT32 lwbemap_face,
    UINT32 miplevel,
    UINT32 x,
    UINT32 y,
    UINT32 z,
    UINT32 peerNum,
    GpuDevice *gpudev,
    UINT64 mask_out,
    UINT64 mask_in,
    bool swap
)
:   m_Target(target),
    m_Offset(offset),
    m_Surf(surf),
    m_ArrayIndex(array_index),
    m_LwbemapFace(lwbemap_face),
    m_Miplevel(miplevel),
    m_X(x),
    m_Y(y),
    m_Z(z),
    m_PeerNum(peerNum),
    m_GpuDev(gpudev),
    m_MaskOut(mask_out),
    m_MaskIn(mask_in),
    m_Swap(swap)
{
}

TraceOpSurface::~TraceOpSurface()
{
}

RC TraceOpSurface::Run()
{
    TraceRelocationSurface reloc(m_Offset, m_Surf, m_ArrayIndex,
        m_LwbemapFace, m_Miplevel, m_X, m_Y, m_Z, m_Swap, 0,  m_PeerNum, m_GpuDev, m_MaskOut, m_MaskIn);
    reloc.DoOffset(m_Target, 0); // Not SLI aware
    return OK;
}

void TraceOpSurface::Print() const
{
    RawPrintf("Relocating a surface info. Target: %s, offset: 0x%x, surface: %s"
    " array_index: %d, lwbemap_face: %d, miplevel: %d, x: %d, y: %d, z: %d, "
    "peerNum: %d\n", m_Target->GetName().c_str(), m_Offset, m_Surf.c_str(), m_ArrayIndex,
    m_LwbemapFace, m_Miplevel, m_X, m_Y, m_Z, m_PeerNum);
}

/*virtual*/ void TraceOpSurface::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpSurface";
}

//------------------------------------------------------------------------------
// TraceOpEnableChannel: Enable a channel
TraceOpSetChannel::TraceOpSetChannel
(
    TraceChannel * pTraceChannel,
    bool           enable
)
:   m_pTraceChannel(pTraceChannel),
    m_SetChEnable(enable)
{
    MASSERT(pTraceChannel != NULL);
}

/*virtual*/ TraceOpSetChannel::~TraceOpSetChannel()
{
}

/*virtual*/ RC TraceOpSetChannel::Run()
{
    RC rc;
    LWGpuChannel * pCh = m_pTraceChannel->GetCh();

    if (pCh != NULL)
    {
        if (m_SetChEnable && !pCh->GetEnabled())
        {
            pCh->SetEnabled(true);
        }
        else if (!m_SetChEnable && pCh->GetEnabled())
        {
            pCh->SetEnabled(false);
        }
    }
    else
    {
        // Invalid channel attached.
        rc =  RC::SOFTWARE_ERROR;
    }

    return rc;
}

/*virtual*/ void TraceOpSetChannel::Print() const
{
    RawPrintf("%s channel %s\n",
            m_SetChEnable?"Enable":"Disable",
            m_pTraceChannel->GetName().c_str());
}

/*virtual*/ void TraceOpSetChannel::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpSetChannel";
}

//------------------------------------------------------------------------------
// TraceOpEvent: handle different kinds of EVENT from ACE2.0 traces
TraceOpEvent::TraceOpEvent
(
    Trace3DTest *test,
    string       name
)
:   m_Test(test),
    m_Name(name)
{
}

/*virtual*/ TraceOpEvent::~TraceOpEvent()
{
}

/*virtual*/ void TraceOpEvent::Print() const
{
    RawPrintf("Handling event: %s\n", m_Name.c_str());
}

/*virtual*/ void TraceOpEvent::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEvent";
}

TraceOpEventMarker::TraceOpEventMarker
(
    Trace3DTest *test,
    string       name,
    string       type,
    TraceChannel *pChannel,
    bool         afterTraceEvent
)
:   TraceOpEvent(test, name),
    m_Type(type),
    m_pChannel(pChannel),
    m_AfterTraceEvent(afterTraceEvent)
{
}

TraceOpEventMarker::~TraceOpEventMarker()
{
}

RC TraceOpEventMarker::Run()
{
    RC rc;

    // Send the event to PolicyManager
    PolicyManager *policyManager = PolicyManager::Instance();
    if (policyManager->IsInitialized())
    {
        PmEventManager *eventManager = policyManager->GetEventManager();
        PmTest* pTest = policyManager->GetTestFromUniquePtr(GetTest());
        LwRm::Handle chHandle = m_pChannel? m_pChannel->GetCh()->ChannelHandle():0;
        PmEvent_TraceEventCpu traceEventCpuEvent(m_Name, pTest, chHandle, m_AfterTraceEvent);
        CHECK_RC(eventManager->HandleEvent(&traceEventCpuEvent));
    }

    // run hooked TraceOp
    for (vector<TraceOp*>::iterator it = m_TraceOperations.begin();
         it != m_TraceOperations.end(); ++ it)
    {
        CHECK_RC((*it)->Run());
    }

    return OK;
}

void TraceOpEventMarker::Print() const
{
    RawPrintf("%s marker (%s) (%s TraceEvent)\n", m_Type.c_str(),
        m_Name.c_str(), m_AfterTraceEvent?"After":"Before");

    for (vector<TraceOp*>::const_iterator it = m_TraceOperations.begin();
         it != m_TraceOperations.end(); ++ it)
    {
        (*it)->Print();
    }
}

void TraceOpEventMarker::GetTraceopProperties(const char **pName,
    map<string,string> &properties) const
{
    *pName = "TraceOpEventMarker";
    properties["eventName"] = m_Name;
    properties["eventType"] = m_Type;
    properties["eventTriggerType"] = m_AfterTraceEvent? "After" : "Before";
}

TraceOpEventPMStart::TraceOpEventPMStart
(
    Trace3DTest *  test,
    TraceChannel * channel,
    string         name
)
:   TraceOpEvent(test, name),
    m_pTraceChannel(channel)
{
}

/*virtual*/ TraceOpEventPMStart::~TraceOpEventPMStart()
{
}

/*virtual*/ RC TraceOpEventPMStart::Run()
{
    RC rc;
    const ArgReader* params = m_Test->GetParam();
    if (params && params->ParamPresent("-pm_sb_file") > 0)
    {
        LWGpuChannel * pCh = m_pTraceChannel->GetCh();
        TraceSubChannel *ptracesubch = m_pTraceChannel->GetSubChannel("");
        UINT32 class_num = ptracesubch->GetClass();
        LWGpuSubChannel * pSubch = ptracesubch->GetSubCh();
        LWGpuResource * lwgpu = m_Test->GetGpuResource();

        pCh->MethodFlush();
        pCh->WaitForIdle();

        MASSERT(gXML);
        XD->XMLStartElement("<PmTrigger");
        XD->outputAttribute("name",  m_Name.c_str());
        XD->endAttributes();

        lwgpu->PerfMonStart(pCh, pSubch->subchannel_number(), class_num);
    }
    else
    {
        // l2 ilwalidation wil be handled in perfmon for HWPM
        // here we will only take care of VPM tests. Most likely they are ACE traces.
        if (params && params->ParamPresent("-pm_ilwalidate_cache") > 0)
        {
            LWGpuChannel * pCh = m_pTraceChannel->GetCh();
            pCh->MethodFlush();
            pCh->WaitForIdle();

            auto pGpuDevice = m_Test->GetBoundGpuDevice();
            for (UINT32 i = 0; i < pGpuDevice->GetNumSubdevices(); i++)
            {
                InfoPrintf("%s, ilwalidate l2 in subdev(%u) with L2_ILWALIDATE_CLEAN before PM trigger\n", __FUNCTION__, i);
                
                auto gpuSubdevice = pGpuDevice->GetSubdevice(i);
                CHECK_RC(gpuSubdevice->IlwalidateL2Cache(GpuSubdevice::L2_ILWALIDATE_CLEAN));
            } 
        }
    }

    return rc;
}

/*virtual*/ void TraceOpEventPMStart::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEventPMStart";
}

TraceOpEventPMStop::TraceOpEventPMStop
(
    Trace3DTest *  test,
    TraceChannel * channel,
    string         name
)
:   TraceOpEvent(test, name),
    m_pTraceChannel(channel)
{
}

/*virtual*/ TraceOpEventPMStop::~TraceOpEventPMStop()
{
}

/*virtual*/ RC TraceOpEventPMStop::Run()
{
    const ArgReader* params = m_Test->GetParam();
    if (params && params->ParamPresent("-pm_sb_file") > 0)
    {
        LWGpuChannel * pCh = m_pTraceChannel->GetCh();
        TraceSubChannel *ptracesubch = m_pTraceChannel->GetSubChannel("");
        UINT32 class_num = ptracesubch->GetClass();
        LWGpuSubChannel * pSubch = ptracesubch->GetSubCh();
        LWGpuResource * lwgpu = m_Test->GetGpuResource();

        pCh->MethodFlush();
        pCh->WaitForIdle();

        lwgpu->PerfMonEnd(pCh, pSubch->subchannel_number(), class_num);

        MASSERT(gXML);
        XD->XMLEndLwrrent();
    }

    return OK;
}

/*virtual*/ void TraceOpEventPMStop::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEventPMStop";
}

TraceOpEventSyncStart::TraceOpEventSyncStart
(
    Trace3DTest*  test,
    string name
)
:  TraceOpEventSyncStart(test, {}, {}, move(name))
{
}

TraceOpEventSyncStart::TraceOpEventSyncStart
(
    Trace3DTest*  test,
    vector<TraceChannel*> channels,
    vector<TraceSubChannel*> subChannels,
    string name
)
:   TraceOpEvent(test, name),
    m_pChannels(move(channels)),
    m_pSubChannels(move(subChannels)),
    m_Sync(false)
{
    const ArgReader* params = m_Test->GetParam();
    MASSERT(params);

    if (m_Test->ConlwrrentRunTestNum() > 1 && params->ParamPresent("-ctxswTimeSlice") == 0)
    {
        ErrPrintf(CTXSYNCID(), "%s requires -ctxswitch and -ctxswTimeSlice", GetEventName().c_str());
        MASSERT(0);
    }

    m_Sync = m_Test->ConlwrrentRunTestNum() > 1 &&
             params->ParamPresent("-ctxswTimeSlice");
    if (m_Sync)
    {
        // before TraceOpEventSyncStart::Run, we want to create semaphore surface
        // then it can avoid cpu overhead inside region
        // and also help to reuse semaphore for different region
        SyncEventSemaphoreSurf::Instance()->Alloc(SyncEventSemaphoreSurf::SYNC_ON,
                                                  m_Test->GetBoundGpuDevice(), m_Test->GetLwRmPtr());
    }
}

/*virtual*/ TraceOpEventSyncStart::~TraceOpEventSyncStart()
{
    if (m_Sync)
    {
        SyncEventSemaphoreSurf::Instance()->Free(SyncEventSemaphoreSurf::SYNC_ON);
    }
}

/*virtual*/ RC TraceOpEventSyncStart::Run()
{
    RC rc;

    RegisterChannels();
    WaitForIdle();
    PrepareSync();
    CHECK_RC(WaitAllTests());

    if (m_Test->IsCoordinatorSyncCtx())
    {
        DebugPrintf(CTXSYNCID(), "%s, coordinator ctx %s wait for worker ctx\n", __FUNCTION__, m_Test->GetTestName());
        //https://confluence.lwpu.com/display/ArchMods/Multiple+Traces+Sync+Mechanism
        TraceOpRunSemaphore::Instance()->WaitForAllWorkerSubctx(m_Test->ConlwrrentRunTestNum() - 1);
        CHECK_RC(WriteRelease());
    }
    else
    {
        CHECK_RC(WriteHostAcquire());
    }

    return OK;
}

void TraceOpEventSyncStart::RegisterChannels()
{
    for (auto iter = m_Test->GetChannels()->begin(); iter != m_Test->GetChannels()->end(); iter++)
    {
        m_pChannels.push_back(*iter);
        m_pSubChannels.push_back((*iter)->GetSubChannel(""));
    }
}

void TraceOpEventSyncStart::PrepareSync()
{
    if (!m_Sync)
    {
        return;
    }

    DebugPrintf(CTXSYNCID(), "%s, zero semaphore surface\n", __FUNCTION__);
    SyncEventSemaphoreSurf::Instance()->Get(SyncEventSemaphoreSurf::SYNC_ON)->Zero();
    SyncEventSemaphoreSurf::Instance()->Get(SyncEventSemaphoreSurf::SYNC_OFF)->Zero();

    // Allocate surface before PM region is to reduce unnecessary CPU overhead in side
    // PM region

    for (auto pChannel : m_pChannels)
    {
        SyncEventSemaphoreSurf::Instance()->Get(SyncEventSemaphoreSurf::SYNC_ON)->Map(
            pChannel->GetVASpaceHandle(), pChannel->GetLwRmPtr());
        SyncEventSemaphoreSurf::Instance()->Get(SyncEventSemaphoreSurf::SYNC_OFF)->Map(
            pChannel->GetVASpaceHandle(), pChannel->GetLwRmPtr());
    }
}

void TraceOpEventSyncStart::WaitForIdle()
{
    MASSERT(m_Test->GetParam());

    if (!m_Sync)
    {
        return;
    }

    WaitForIdleImpl();
}

void TraceOpEventSyncStart::WaitForIdleImpl()
{
    for (auto pChannel : m_pChannels)
    {
        LWGpuChannel * pCh = pChannel->GetCh();
        pCh->MethodFlush();
        pCh->WaitForIdle();
        DebugPrintf(CTXSYNCID(), "%s, test %s all the works before sync point are completed\n",
                    __FUNCTION__, m_Test->GetTestName());
    }
}

RC TraceOpEventSyncStart::WaitAllTests()
{
    RC rc;

    if (!m_Sync)
    {
        return rc;
    }

    LWGpuResource* lwgpu = m_Test->GetGpuResource();
    LWGpuContexScheduler::Pointer lwgpuSked = lwgpu->GetContextScheduler();
    lwgpuSked->SyncAllRegisteredCtx(SYNCPOINT(GetEventName() + " begins..."));
    return rc;
}

RC TraceOpEventSyncStart::WriteRelease()
{
    RC rc;
    if (!m_Sync)
    {
        return rc;
    }

    if (m_pChannels.size())
    {
        TraceChannel* pChannel = m_pChannels[0];
        TraceSubChannel* pSubChannel = m_pSubChannels[0];
        DebugPrintf(CTXSYNCID(), "%s, coordinator ctx %s inject Release on address 0x%llx vas 0x%x\n",
                    __FUNCTION__, m_Test->GetTestName(),
                    SyncEventSemaphoreSurf::Instance()->Get(
                        SyncEventSemaphoreSurf::SYNC_ON)->GetVA(pChannel->GetVASpaceHandle(), pChannel->GetLwRmPtr()),
                        pChannel->GetVASpaceHandle());
        SubctxInbandSemaphore inbandSync(SyncEventSemaphoreSurf::Instance()->Get(SyncEventSemaphoreSurf::SYNC_ON),
                pChannel, pSubChannel, 1 /*payload*/);
        CHECK_RC(inbandSync.Release());
    }
    return rc;
}

RC TraceOpEventSyncStart::WriteHostAcquire()
{
    RC rc;
    if (!m_Sync)
    {
        return rc;
    }

    for (int i = 0; i < static_cast<int>(m_pChannels.size()); i++)
    {
        TraceChannel* pChannel = m_pChannels[i];
        TraceSubChannel* pSubChannel = m_pSubChannels[i];
        DebugPrintf(CTXSYNCID(), "%s, worker ctx %s inject Host Acquire on address 0x%llx vas(0x%x)\n",
                    __FUNCTION__, m_Test->GetTestName(),
                    SyncEventSemaphoreSurf::Instance()->Get(
                        SyncEventSemaphoreSurf::SYNC_ON)->GetVA(pChannel->GetVASpaceHandle(), pChannel->GetLwRmPtr()),
                        pChannel->GetVASpaceHandle());
        SubctxInbandSemaphore inbandSync(SyncEventSemaphoreSurf::Instance()->Get(SyncEventSemaphoreSurf::SYNC_ON),
                pChannel, pSubChannel, 1 /*payload*/);
        CHECK_RC(inbandSync.Acquire());
    }

    return rc;
}

/*virtual*/ string TraceOpEventSyncStart::GetEventName() const
{
    return "EVENT_SYNC ON " + m_Name;
}

/*virtual*/ void TraceOpEventSyncStart::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEventSyncStart";
}

TraceOpEventSyncStop::TraceOpEventSyncStop
(
    Trace3DTest *  test,
    vector<TraceChannel*> channels,
    vector<TraceSubChannel*> subChannels,
    string name
)
:   TraceOpEvent(test, name),
    m_pChannels(move(channels)),
    m_pSubChannels(move(subChannels)),
    m_Sync(false)
{
    const ArgReader* params = m_Test->GetParam();
    MASSERT(params);

    if (m_Test->ConlwrrentRunTestNum() > 1 && params->ParamPresent("-ctxswTimeSlice") == 0)
    {
        ErrPrintf(CTXSYNCID(), "%s requires -ctxswitch and -ctxswTimeSlice", GetEventName().c_str());
        MASSERT(0);
    }

    m_Sync = m_Test->ConlwrrentRunTestNum() > 1 &&
             params->ParamPresent("-ctxswTimeSlice");
    if (m_Sync)
    {
        SyncEventSemaphoreSurf::Instance()->Alloc(SyncEventSemaphoreSurf::SYNC_OFF,
                                                  m_Test->GetBoundGpuDevice(), m_Test->GetLwRmPtr());
    }
}

TraceOpEventSyncStop::TraceOpEventSyncStop
(
    Trace3DTest *  test,
    string name
)
:   TraceOpEventSyncStop(test, {}, {}, move(name))
{
}

void TraceOpEventSyncStop::RegisterChannels()
{
    for (auto iter = m_Test->GetChannels()->begin(); iter != m_Test->GetChannels()->end(); iter++)
    {
        m_pChannels.push_back(*iter);
        m_pSubChannels.push_back((*iter)->GetSubChannel(""));
    }
}

/*virtual*/ TraceOpEventSyncStop::~TraceOpEventSyncStop()
{
    if (m_Sync)
    {
        SyncEventSemaphoreSurf::Instance()->Free(SyncEventSemaphoreSurf::SYNC_OFF);
    }
}

/*virtual*/ RC TraceOpEventSyncStop::Run()
{
    RC rc;

    DebugPrintf(CTXSYNCID(), "%s, ctx %s flush methods\n", __FUNCTION__, m_Test->GetTestName());
    for (auto pChannel : m_pChannels)
    {
        pChannel->GetCh()->MethodFlush();
    }

    if (!m_Test->IsCoordinatorSyncCtx())
    {
        DebugPrintf(CTXSYNCID(), "%s, worker ctx %s notify coordinator ctx\n", __FUNCTION__, m_Test->GetTestName());
        TraceOpRunSemaphore::Instance()->NotifyCoordinatorSubctx();
    }

    //WaitForIdle();
    //CHECK_RC(WaitAllTests());

    return OK;
}

void TraceOpEventSyncStop::WaitForIdle()
{
    MASSERT(m_Test->GetParam());

    if (!m_Sync)
    {
        return;
    }

    WaitForIdleImpl();
}

void TraceOpEventSyncStop::WaitForIdleImpl()
{
    for (auto pChannel : m_pChannels)
    {
        LWGpuChannel * pCh = pChannel->GetCh();
        pCh->MethodFlush();
        pCh->WaitForIdle();
    }
    DebugPrintf(CTXSYNCID(), "%s, ctx %s: all the works across all the tests are completed\n",
                __FUNCTION__, m_Test->GetTestName());
}

RC TraceOpEventSyncStop::WaitAllTests()
{
    RC rc;

    if (!m_Sync)
    {
        return rc;
    }

    LWGpuResource* lwgpu = m_Test->GetGpuResource();
    LWGpuContexScheduler::Pointer lwgpuSked = lwgpu->GetContextScheduler();
    lwgpuSked->SyncAllRegisteredCtx(SYNCPOINT(GetEventName() + " exits..."));
    return rc;
}

string TraceOpEventSyncStop::GetEventName() const
{
    return "EVENT_SYNC OFF " + m_Name;
}

/*virtual*/ void TraceOpEventSyncStop::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEventSyncStop";
}

TraceOpEventSyncPMStart::TraceOpEventSyncPMStart
(
    Trace3DTest *  test,
    TraceChannel * channel,
    TraceSubChannel * subchannel,
    string         name
)
:   TraceOpEventSyncStart(test, {channel}, {subchannel}, move(name))
{
    if (m_Sync && m_Test->GetParam()->ParamPresent("-enable_sync_event"))
    {
        ErrPrintf(CTXSYNCID(), "%s PMTRIGGER_SYNC_EVENT is not compatible with -enable_sync_event!");
        MASSERT(0);
    }
}

/*virtual*/ TraceOpEventSyncPMStart::~TraceOpEventSyncPMStart()
{
}

/*virtual*/ RC TraceOpEventSyncPMStart::Run()
{
    RC rc;

    WaitForIdle();
    PrepareSync();
    CHECK_RC(WaitAllTests());

    if (m_Test->IsCoordinatorSyncCtx())
    {
        DebugPrintf(CTXSYNCID(), "%s, coordinator subctx %s wait for worker subctx\n", __FUNCTION__, m_Test->GetTestName());
        // Coordinator subctx waits on event when all worker subctxs flush GPENTRY inside PM region.
        // The reason is that PmTrigger on/off methods should wrap all the GPENTRYs across all the subctxs
        // including worker subctx.
        // The solution is that worker subctx's GPENTRYs will be flushed at first but Host Acquire method
        // will be inserted in the begin of method stream so all the following methods will be blocked
        // in the host.
        // After all the worker subctxs flush GPENTRY in PM region, coordinator subctx will insert PmTrigger and
        // then insert FE release so that all the worker subctx's method steam have a chance to be processed
        // by FE.
        //
        // coordinator subctx                    worker subctx
        // PmTrigger
        // FE Release                       Host Acquire
        // PM region                        Pm region
        // Host Acquire                     FE Release
        // Host WFI_ALL
        // PmTrigger
        TraceOpRunSemaphore::Instance()->WaitForAllWorkerSubctx(m_Test->ConlwrrentRunTestNum() - 1);
        StartPerf();
        CHECK_RC(WritePmTrigger());
        CHECK_RC(WriteRelease());
    }
    else
    {
        CHECK_RC(WriteHostAcquire());
    }

    return OK;
}

void TraceOpEventSyncPMStart::WaitForIdle()
{
    const ArgReader* params = m_Test->GetParam();
    MASSERT(params);
    if (params->ParamPresent("-pm_sb_file") == 0 && !m_Sync)
    {
        return;
    }

    TraceOpEventSyncStart::WaitForIdleImpl();
}

void TraceOpEventSyncPMStart::StartPerf()
{
    DebugPrintf(CTXSYNCID(), "%s, coordinator ctx %s start perf\n", __FUNCTION__, m_Test->GetTestName());

    const ArgReader* params = m_Test->GetParam();
    MASSERT(params);
    if (params->ParamPresent("-pm_sb_file") == 0)
    {
        return;
    }

    LWGpuResource* lwgpu = m_Test->GetGpuResource();
    const UINT32 classNum = m_pSubChannels[0]->GetClass();
    const LWGpuSubChannel * pSubch = m_pSubChannels[0]->GetSubCh();
    LWGpuChannel * pCh = m_pChannels[0]->GetCh();
    MASSERT(gXML);
    XD->XMLStartElement("<PmTrigger");
    XD->outputAttribute("name",  m_Name.c_str());
    XD->endAttributes();
    lwgpu->PerfMonStart(pCh, pSubch->subchannel_number(), classNum);
}

RC TraceOpEventSyncPMStart::WritePmTrigger()
{
    RC rc;
    DebugPrintf(CTXSYNCID(), "%s, coordinator subctx %s inject PmTrigger On\n", __FUNCTION__, m_Test->GetTestName());
    UINT32 data = 0;
    CHECK_RC(m_pSubChannels[0]->GetSubCh()->MethodWriteN_RC(LWC397_PM_TRIGGER, 1, &data));
    return rc;
}

/*virtual*/ string TraceOpEventSyncPMStart::GetEventName() const
{
    return "PMTRIGGER_SYNC_EVENT OFF " + m_Name;
}

/*virtual*/ void TraceOpEventSyncPMStart::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEventSyncPMStart";
}

TraceOpEventSyncPMStop::TraceOpEventSyncPMStop
(
    Trace3DTest *  test,
    TraceChannel * channel,
    TraceSubChannel * subchannel,
    string name
)
:   TraceOpEventSyncStop(test, {channel}, {subchannel}, move(name))
{
    if (m_Sync && m_Test->GetParam()->ParamPresent("-enable_sync_event"))
    {
        ErrPrintf(CTXSYNCID(), "%s PMTRIGGER_SYNC_EVENT is not compatible with -enable_sync_event!");
        MASSERT(0);
    }
}

/*virtual*/ TraceOpEventSyncPMStop::~TraceOpEventSyncPMStop()
{
}

/*virtual*/ RC TraceOpEventSyncPMStop::Run()
{
    RC rc;

    if (m_Test->IsCoordinatorSyncCtx())
    {
        CHECK_RC(WriteHostAcquire());
        // Need to inject FE NOP before WFI_ALL
        // in order to avoid HOST to filter out WFI
        CHECK_RC(WriteNop());
        CHECK_RC(WriteWFI());
        CHECK_RC(WritePmTrigger());
        WaitForIdle();
        StopPerf();
    }
    else
    {
        CHECK_RC(WriteRelease());
        DebugPrintf(CTXSYNCID(), "%s, worker subctx %s flush methods\n", __FUNCTION__, m_Test->GetTestName());
        m_pChannels[0]->GetCh()->MethodFlush();
        DebugPrintf(CTXSYNCID(), "%s, worker subctx %s notify coordinator subctx\n", __FUNCTION__, m_Test->GetTestName());
        TraceOpRunSemaphore::Instance()->NotifyCoordinatorSubctx();
    }

    CHECK_RC(WaitAllTests());

    return OK;
}

void TraceOpEventSyncPMStop::WaitForIdle()
{
    const ArgReader* params = m_Test->GetParam();
    MASSERT(params);
    if (params->ParamPresent("-pm_sb_file") == 0 && !m_Sync)
    {
        return;
    }
    WaitForIdleImpl();
}

void TraceOpEventSyncPMStop::StopPerf()
{
    DebugPrintf("%s, coordinator subctx %s stop perf\n", __FUNCTION__, m_Test->GetTestName());

    const ArgReader* params = m_Test->GetParam();
    MASSERT(params);
    if (params->ParamPresent("-pm_sb_file") == 0)
    {
        return;
    }
    LWGpuResource* lwgpu = m_Test->GetGpuResource();
    const UINT32 classNum = m_pSubChannels[0]->GetClass();
    const LWGpuSubChannel * pSubch = m_pSubChannels[0]->GetSubCh();
    LWGpuChannel * pCh = m_pChannels[0]->GetCh();
    lwgpu->PerfMonEnd(pCh, pSubch->subchannel_number(), classNum);
    MASSERT(gXML);
    XD->XMLEndLwrrent();
}

RC TraceOpEventSyncPMStop::WriteWFI()
{
    RC rc;

    DebugPrintf(CTXSYNCID(), "%s, coordinator subctx %s inject WFI_ALL\n", __FUNCTION__, m_Test->GetTestName());
    UINT32 data = DRF_DEF(C36F, _WFI, _SCOPE, _ALL);
    CHECK_RC(m_pSubChannels[0]->GetSubCh()->MethodWriteN_RC(LWC36F_WFI, 1, &data));

    return rc;
}

RC TraceOpEventSyncPMStop::WritePmTrigger()
{
    RC rc;

    DebugPrintf(CTXSYNCID(), "%s, coordinator subctx %s inject PmTrigger Off\n", __FUNCTION__, m_Test->GetTestName());
    UINT32 data = 0;
    CHECK_RC(m_pSubChannels[0]->GetSubCh()->MethodWriteN_RC(LWC397_PM_TRIGGER, 1, &data));

    return rc;
}

RC TraceOpEventSyncPMStop::WriteRelease()
{
    RC rc;
    if (!m_Sync)
    {
        return rc;
    }
    DebugPrintf(CTXSYNCID(), "%s, worker subctx %s inject Release on address 0x%llx vas 0x%x\n",
                __FUNCTION__, m_Test->GetTestName(),
                SyncEventSemaphoreSurf::Instance()->Get(
                    SyncEventSemaphoreSurf::SYNC_OFF)->GetVA(m_pChannels[0]->GetVASpaceHandle(), m_pChannels[0]->GetLwRmPtr()));
    SubctxInbandSemaphore inbandSync(SyncEventSemaphoreSurf::Instance()->Get(SyncEventSemaphoreSurf::SYNC_OFF),
            m_pChannels[0], m_pSubChannels[0], m_Test->ConlwrrentRunTestNum() - 1 /*payload*/);
    CHECK_RC(inbandSync.Release());

    // LaunchDma and LoadInlineData should be submitted together.
    // So MODS will not flush LaunchDma w/o LoadInlineData.
    // Inject NOP to break that atomic sequence
    CHECK_RC(WriteNop());

    return rc;
}

RC TraceOpEventSyncPMStop::WriteNop()
{
    RC rc;
    if (!m_Sync)
    {
        return rc;
    }

    UINT32 data = 0;
    CHECK_RC(m_pSubChannels[0]->GetSubCh()->MethodWriteN_RC(LWC397_NO_OPERATION,
                                                             1, &data));

    return rc;
}

RC TraceOpEventSyncPMStop::WriteHostAcquire()
{
    RC rc;
    if (!m_Sync)
    {
        return rc;
    }
    DebugPrintf(CTXSYNCID(), "%s, coordinator subctx %s inject Host Acquire on address 0x%llx vas(0x%x)\n",
                __FUNCTION__, m_Test->GetTestName(),
                SyncEventSemaphoreSurf::Instance()->Get(
                    SyncEventSemaphoreSurf::SYNC_OFF)->GetVA(m_pChannels[0]->GetVASpaceHandle(), m_pChannels[0]->GetLwRmPtr()));
    SubctxInbandSemaphore inbandSync(SyncEventSemaphoreSurf::Instance()->Get(SyncEventSemaphoreSurf::SYNC_OFF),
            m_pChannels[0], m_pSubChannels[0], m_Test->ConlwrrentRunTestNum() - 1 /*payload*/);
    CHECK_RC(inbandSync.Acquire());

    return rc;
}

string TraceOpEventSyncPMStop::GetEventName() const
{
    return "PMTRIGGER_SYNC_EVENT OFF " + m_Name;
}

/*virtual*/ void TraceOpEventSyncPMStop::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEventSyncPMStop";
}

map<string, map<string, UINT32> > TraceOpEventDump::s_CountMap =
    map<string, map<string, UINT32> >();

TraceOpEventDump::TraceOpEventDump
(
    Trace3DTest*  test,
    TraceChannel* channel,
    GpuTrace*     trace,
    string        name,
    string        moduleName
)
:   TraceOpEvent(test, name), m_Channel(channel), m_Trace(trace),
    m_ModuleName(moduleName)
{
}

/*virtual*/ TraceOpEventDump::~TraceOpEventDump()
{
}

/*virtual*/ RC TraceOpEventDump::Run()
{
    RC rc = OK;
    GpuVerif *gpuverif = m_Channel->GetGpuVerif();
    UINT32 subDevNum = m_Test->GetBoundGpuDevice()->GetNumSubdevices();
    // We have to find the module here since module might be defined behind event
    TraceModule *module = m_Trace->ModFind(m_ModuleName.c_str());
    if (0 == module)
    {
        ErrPrintf("Can't find module %s\n", m_ModuleName.c_str());
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 subdev = 0; subdev < subDevNum; subdev++)
    {
        TraceModule::SomeKindOfSurface skos = {0};
        MdiagSurf *surfObj = 0;

        module->StringToSomeKindOfSurface(subdev, Gpu::MaxNumSubdevices,
        module->GetName(), &skos);
        switch (skos.SurfaceType)
        {
            case TraceModule::SURF_TYPE_LWSURF:
                surfObj = module->SKOS2LWSurfPtr(&skos);
                break;
            case TraceModule::SURF_TYPE_DMABUFFER:
                surfObj = static_cast<MdiagSurf*>(module->SKOS2DmaBufPtr(&skos));
                break;
            case TraceModule::SURF_TYPE_PEER:
                surfObj = const_cast<MdiagSurf*>(module->SKOS2ModPtr(&skos)->GetDmaBuffer());
                break;
            default:
                MASSERT(!"Unknown surface type!\n");
                return RC::SOFTWARE_ERROR;
        }
        if (skos.Size==0 || surfObj==0)
        {
            ErrPrintf("TraceOpEventDump::Run: Can't find surface %s\n", m_ModuleName.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
        else if (skos.CtxDmaOffset==0)
        {
            WarnPrintf("TraceOpEventDump::Run: surface %s not allocated, skip dumping "
                "for event %s\n", m_ModuleName.c_str(), m_Name.c_str());
            return OK;
        }

        // Saved image file looks like: imagename_eventName_count#.tga.gz
        // Saved buffer file looks like: buffername_eventName_count#.bin
        string dumpName = m_ModuleName;
        string eventTag = "_" + m_Name;
        string countTag = Utility::StrPrintf("_%d", s_CountMap[m_Name][m_ModuleName]++);
        string gpuTag   = subdev==0 ? "" : Utility::StrPrintf("_GPU%d", subdev);
        string tag      = eventTag + countTag + gpuTag;
        string::size_type posloc = dumpName.rfind('.');
        if (posloc == string::npos) posloc = dumpName.size();
        dumpName.insert(posloc, tag);

        m_Channel->GetCh()->WaitForIdle();

        rc = gpuverif->DumpSurfOrBuf(surfObj, dumpName, subdev);
        if (OK != rc)
        {
            ErrPrintf("Failed to dump buffer %s\n", dumpName.c_str());
            return rc;
        }
    }

    return OK;
}

/*virtual*/ void TraceOpEventDump::GetTraceopProperties( const char * * pName,
    map<string,string> & properties ) const
{
    *pName = "TraceOpEventDump";
}

//-----------------------------------------------------------------------------
TraceOpAllocChannel::TraceOpAllocChannel
(
    Trace3DTest* pTest,
    TraceChannel* pTraceChannel
):
    m_Test(pTest),
    m_pTraceChannel(pTraceChannel)
{
    MASSERT(m_pTraceChannel != 0 &&
            "Invalid TraceChannel pointer");
}

TraceOpAllocChannel::~TraceOpAllocChannel()
{
}

RC TraceOpAllocChannel::Run()
{
    RC rc;

    ChiplibOpScope newScope(string(m_Test->GetTestName()) + " Alloc channel " + m_pTraceChannel->GetName(),
                            NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

    // Sanity check
    LWGpuResource * lwgpu = m_pTraceChannel->GetGpuResource();
    if (m_pTraceChannel->GetWfiMethod() == WFI_INTR &&
        lwgpu->IsHandleIntrTest())
    {
        ErrPrintf("-wfi_intr is not compatible with -handleIntr\n");
        return RC::SOFTWARE_ERROR;
    }

    // Create channel
    const ArgReader* params = m_Test->GetParam();
    CHECK_RC(m_pTraceChannel->AllocChannel(params));

    // Add created channel to list
    CHECK_RC(m_Test->AddChannel(m_pTraceChannel));
    // print new added channel info
    m_Test->PrintChannelAllocInfo();

    // Print buffer info
    BuffInfo buffInfo;
    m_pTraceChannel->GetCh()->GetBuffInfo(&buffInfo, m_pTraceChannel->GetName());
    buffInfo.Print(nullptr, m_Test->GetBoundGpuDevice());

    // Setup channel properties
    CHECK_RC(m_Test->SetupCtxswZlwll(m_pTraceChannel));
    CHECK_RC(m_Test->SetupCtxswPm(m_pTraceChannel));
    CHECK_RC(m_Test->SetupCtxswSmpc(m_pTraceChannel));

    // Setup channel properties
    if (EngineClasses::IsGpuFamilyClassOrLater(
        TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
    {
        CHECK_RC(m_Test->SetupCtxswPreemption(m_pTraceChannel));
    }

    // Setup crc checker
    if (!m_pTraceChannel->IsHostOnlyChannel())
    {
        CHECK_RC(m_Test->SetupChannelCrcChecker(m_pTraceChannel, false));
    }

    // Set method count for ctxswitch
    CHECK_RC(m_pTraceChannel->SetMethodCount());

    // Begin channel switch
    m_pTraceChannel->BeginChannelSwitch();

    // Massage push buffer
    for (TraceModules::iterator iMod = m_pTraceChannel->ModBegin();
        iMod != m_pTraceChannel->ModEnd(); ++iMod)
    {
        CHECK_RC((*iMod)->MassagePushBuffer(0, m_Test->GetTrace(), params));
    }

    // Apply SLI scissor
    m_Test->ApplySLIScissorToChannel(m_pTraceChannel);

    // Register channel to Policy Manager
    PolicyManager *pPM = PolicyManager::Instance();
    if(pPM->IsInitialized())
    {
        PmTest *pTest = pPM->GetTestFromUniquePtr(m_Test);
        CHECK_RC(m_Test->RegisterChannelToPolicyManager(pPM, pTest, m_pTraceChannel));
    }
    // Set register control for dynamic alloc channel
    CHECK_RC(m_Test->HandleOptionalCtxRegisters(m_pTraceChannel));


    return rc;
}

void TraceOpAllocChannel::Print() const
{
    RawPrintf("Create %s managed channel %s\n",
        m_pTraceChannel->IsGpuManagedChannel()? "GPU" : "CPU",
        m_pTraceChannel->GetName().c_str());
}

void TraceOpAllocChannel::GetTraceopProperties
(
    const char * * pName,
    map<string,string> & properties
) const
{
    *pName = "TraceOpAllocChannel";
}

//-----------------------------------------------------------------------------
TraceOpAllocSubChannel::TraceOpAllocSubChannel
(
    Trace3DTest* pTest,
    TraceChannel* pTraceChannel,
    TraceSubChannel* pTraceSubChannel
):
    m_Test(pTest),
    m_pTraceChannel(pTraceChannel),
    m_pTraceSubChannel(pTraceSubChannel)
{
    MASSERT(m_pTraceChannel != 0 &&
            m_pTraceSubChannel != 0 &&
            "Invalid TraceChannel or TraceSubChannel pointer");
}

TraceOpAllocSubChannel::~TraceOpAllocSubChannel()
{
}

RC TraceOpAllocSubChannel::Run()
{
    RC rc;

    // Create channel object
    CHECK_RC(m_pTraceSubChannel->AllocObject(m_pTraceChannel->GetChHelper()));

    // Setup channel
    if (!m_pTraceChannel->IsGpuManagedChannel())
    {
        const ArgReader* params = m_Test->GetParam();

        // Setup subchannel
        CHECK_RC(m_pTraceChannel->SetupSubChannelClass(m_pTraceSubChannel));
        if (params->ParamPresent("-tsg_name") == 0)
        {
            // Same logic as TraceChannel::SetupClass().
            // When multiple trace3d tests running together in TSG mode,
            // this WFI can cause channel switching, which is undesirable
            // for TSG tests.
            CHECK_RC(m_pTraceChannel->GetCh()->WaitForIdleRC());
        }

        // Used for wfi at the end of trace_3d test;
        // Useless if it's freeed before reaching the end.
        CHECK_RC(m_pTraceSubChannel->AllocNotifier(
            params, m_pTraceChannel->GetName()));

        if ((params->ParamPresent("-insert_method") > 0) ||
            (params->ParamPresent("-insert_class_method") > 0) ||
            (params->ParamPresent("-insert_subch_method") > 0))
        {
            CHECK_RC(m_pTraceChannel->InjectMassageMethods());
        }
    }
    else
    {
        LWGpuChannel* lwgpuCh = m_pTraceChannel->GetCh();
        if (lwgpuCh)
        {
            CHECK_RC(lwgpuCh->ScheduleChannel(true));
        }
    }

    return rc;
}

void TraceOpAllocSubChannel::Print() const
{
    RawPrintf("Create subchannel %s for %s managed channel %s\n",
        m_pTraceSubChannel->GetName().c_str(),
        m_pTraceChannel->IsGpuManagedChannel()? "GPU" : "CPU",
        m_pTraceChannel->GetName().c_str());
}

void TraceOpAllocSubChannel::GetTraceopProperties
(
    const char * * pName,
    map<string,string> & properties
) const
{
    *pName = "TraceOpAllocSubChannel";
}

//-----------------------------------------------------------------------------
TraceOpFreeChannel::TraceOpFreeChannel
(
    Trace3DTest* pTest,
    TraceChannel* pTraceChannel
):
    m_Test(pTest),
    m_pTraceChannel(pTraceChannel)
{
    MASSERT(m_pTraceChannel != 0 &&
            "Invalid TraceChannel pointer");
}

TraceOpFreeChannel::~TraceOpFreeChannel()
{
}

RC TraceOpFreeChannel::Run()
{
    RC rc;

    if (m_pTraceChannel)
    {
        m_pTraceChannel->EndChannelSwitch();

        m_pTraceChannel->Free();

        CHECK_RC(m_Test->RemoveChannel(m_pTraceChannel));
    }

    return rc;
}

void TraceOpFreeChannel::Print() const
{
    RawPrintf("Free %s managed channel %s\n",
        m_pTraceChannel->IsGpuManagedChannel()? "GPU" : "CPU",
        m_pTraceChannel->GetName().c_str());
}

void TraceOpFreeChannel::GetTraceopProperties
(
    const char * * pName,
    map<string,string> & properties
) const
{
    *pName = "TraceOpFreeChannel";
}

//-----------------------------------------------------------------------------
TraceOpFreeSubChannel::TraceOpFreeSubChannel
(
    Trace3DTest* pTest,
    TraceChannel* pTraceChannel,
    TraceSubChannel* pTraceSubChannel
):
    m_pTraceChannel(pTraceChannel),
    m_pTraceSubChannel(pTraceSubChannel)
{
    MASSERT(m_pTraceChannel != 0 &&
            m_pTraceSubChannel != 0 &&
            "Invalid TraceSubChannel or TraceChannel pointer");
}

TraceOpFreeSubChannel::~TraceOpFreeSubChannel()
{
}

RC TraceOpFreeSubChannel::Run()
{
    RC rc;

    if (m_pTraceSubChannel != 0)
    {
        CHECK_RC(m_pTraceChannel->RemoveSubChannel(m_pTraceSubChannel));
    }

    return rc;
}

void TraceOpFreeSubChannel::Print() const
{
    RawPrintf("Free subchannel %s for %s managed channel %s\n",
        m_pTraceSubChannel->GetName().c_str(),
        m_pTraceChannel->IsGpuManagedChannel()? "GPU" : "CPU",
        m_pTraceChannel->GetName().c_str());
}

void TraceOpFreeSubChannel::GetTraceopProperties
(
    const char * * pName,
    map<string,string> & properties
) const
{
    *pName = "TraceOpFreeSubChannel";
}

TraceOpUtlRunScript::TraceOpUtlRunScript
(
    Trace3DTest * pTest,
    string scriptPath,
    string scriptArgs,
    string scopeName
) :
    m_pTest(pTest),
    m_ScriptPath(scriptPath),
    m_ScriptArgs(scriptArgs)
{
    m_ScopeName = scopeName.empty() ? "__default__" : scopeName;
}

/*virtual*/ TraceOpUtlRunScript::~TraceOpUtlRunScript()
{
}

/*virtual*/ RC TraceOpUtlRunScript::Run()
{
    if (Utl::HasInstance())
    {
        auto utl = Utl::Instance();
        
        utl->ReadTestSpecificScript(m_pTest, m_ScriptPath + " " + m_ScriptArgs, 
                m_ScopeName);
        Utl::Instance()->TriggerTestInitEvent(m_pTest);
    }
    else
    {
        ErrPrintf("UTL_SCRIPT can only work with utl enable scenario.\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

/*virtual*/ void TraceOpUtlRunScript::Print() const
{
    RawPrintf(" UTL_SCRIPT info: Script path %s with argumentList %s\n",
            m_ScriptPath.c_str(),
            m_ScriptArgs.c_str());
}

/*virtual*/ void TraceOpUtlRunScript::GetTraceopProperties(const char * * pName,
            map<string,string> & properties ) const
{
    *pName = "TraceOpUtlRunScript";
}


TraceOpUtlRunFunc::TraceOpUtlRunFunc
(
    Trace3DTest * pTest,
    string expression,
    string scopeName
) :
    m_pTest(pTest),
    m_Expression(expression)
{
    m_ScopeName = scopeName.empty() ? "__default__" : scopeName;
}

/*virtual*/ TraceOpUtlRunFunc::~TraceOpUtlRunFunc()
{
}

/*virtual*/ RC TraceOpUtlRunFunc::Run()
{
    RC rc;

    if (Utl::HasInstance())
    {
        auto utl = Utl::Instance();
        
        CHECK_RC(utl->Eval(m_Expression, m_ScopeName, 
                    m_pTest));
    }
    else
    {
        ErrPrintf("UTL_RUN can only work with utl enable scenario.\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

/*virtual*/ void TraceOpUtlRunFunc::Print() const
{
    RawPrintf(" UTL_RUN info: expression is %s under the module %s in test %s\n",
            m_Expression.c_str(), m_ScopeName.c_str(),
            m_pTest->GetTestName());
}

/*virtual*/ void TraceOpUtlRunFunc::GetTraceopProperties(const char * * pName,
            map<string,string> & properties ) const
{
    *pName = "TraceOpUtlRunFunc";
}

TraceOpUtlRunInline::TraceOpUtlRunInline
(
    Trace3DTest*  pTest,
    string expression,
    string scopeName
) :
    m_pTest(pTest),
    m_Expression(expression)
{
    m_ScopeName = scopeName.empty() ? "__default__" : scopeName;
}

/*virtual*/ TraceOpUtlRunInline::~TraceOpUtlRunInline()
{
}

/*virtual*/ RC TraceOpUtlRunInline::Run()
{
    RC rc;

    if (Utl::HasInstance())
    {
        auto utl = Utl::Instance();
        
        CHECK_RC(utl->Exec(m_Expression, m_ScopeName, m_pTest));
    }
    else
    {
        ErrPrintf("UTL_RUN_BEGIN/UTL_RUN_END can only work with utl enable scenario.\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

/*virtual*/ void TraceOpUtlRunInline::Print() const
{
    RawPrintf(" UTL_RUN_BEGIN info: Utl inline code has been launched.\n");
}

/*virtual*/ void TraceOpUtlRunInline::GetTraceopProperties( const char * * pName,
            map<string,string> & properties ) const
{
    *pName = "TraceOpUtlRunInline";
}

