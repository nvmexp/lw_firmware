/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Implements utility functions & classes for PolicyManager

#include "core/include/refparse.h"
#include "core/include/channel.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/regexp.h"
#include "pmchan.h"
#include "pmchwrap.h"
#include "pmevent.h"
#include "pmevntmn.h"
#include "pmgilder.h"
#include "pmpotevt.h"
#include "pmsurf.h"
#include "mdiag/utils/mdiagsurf.h"
#include "mdiag/utils/mmufaultbuffers.h"
#include "mdiag/utils/utils.h"
#include "pmtest.h"
#include "pmtrigg.h"
#include "pmutils.h"
#include "gpu/utility/runlist.h"
#include "gpu/utility/runlwrap.h"
#include "class/clb069.h"
#include "class/cl2080.h"
#include "class/clc365.h"
#include "class/clc369.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrlb069.h"
#include "ctrl/ctrlc365.h"
#include "ctrl/ctrl2080/ctrl2080gpu.h"
#include "ctrl/ctrl2080/ctrl2080fifo.h"
#include "g00x/g000/dev_fault.h"
#include "mdiag/vaspace.h"
#include "mdiag/sysspec.h"
#include "mdiag/lwgpu.h"
#include "pmvaspace.h"
#include "pmvftest.h"
#include "pmsmcengine.h"

#include <boost/algorithm/string/predicate.hpp>
#include <algorithm>
#include <cstdio>

using boost::algorithm::starts_with;
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))

const vector<string> PmRegister::m_RegRestoreExclude = {
    "LW_VIRTUAL_FUNCTION_PRIV_MMU_FAULT_BUFFER_GET",
    "LW_VIRTUAL_FUNCTION_PRIV_MMU_FAULT_BUFFER_PUT",
    "LW_PRAMIN_DATA032"
};

//-----------------------------------------------------------------------------
//! \brief A namespace including utilites for fault handling
//!
namespace PmFaulting
{
    //-------------------------------------------------------------------------
    //! \brief return true if MMU fault buffer is supported by RM
    //!
    bool IsSupportMMUFaultBuffer()
    {
        LwRmPtr pLwRm;
        GpuDevice* pDev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice();
        return pLwRm->IsClassSupported(MMU_FAULT_BUFFER, pDev);
    }
    //-------------------------------------------------------------------------
    //! \brief A description of fault buffer
    //!
    //! class and notifier are described.
    class FaultBuffer
    {
    public:

        //---------------------------------------------------------------------
        //! \brief return faulting buffer class
        //!
        static UINT32 GetClass()
        {
            Init();
            return m_FaultBufferClass;
        }
        //---------------------------------------------------------------------
        //! \brief return faulting buffer notifier
        //!
        static UINT32 GetNotifier()
        {
            Init();
            return m_FaultBufferNotifier;
        }
        //---------------------------------------------------------------------
        //! \brief return faulting buffer size
        //!
        static UINT32 GetSize()
        {
            Init();
            if (m_FaultBufferClass == MMU_FAULT_BUFFER)
            {
                return LWC369_BUF_SIZE;
            }
            else
            {
                return LWB069_FAULT_BUF_SIZE;
            }
        }

    private:
        //---------------------------------------------------------------------
        //! \brief initialize faulting buffer class and notifier
        //!
        static void Init()
        {
            static bool init = false;

            if (init)
            {
                return;
            }

            if (IsSupportMMUFaultBuffer())
            {
                m_FaultBufferClass = MMU_FAULT_BUFFER;
                m_FaultBufferNotifier = LWC369_NOTIFIER_MMU_FAULT_REPLAYABLE;
            }
            else
            {
                m_FaultBufferClass = MAXWELL_FAULT_BUFFER_A;
                m_FaultBufferNotifier = LWB069_NOTIFIERS_REPLAYABLE_FAULT;
            }

            init = true;
        }
        static UINT32 m_FaultBufferClass;
        static UINT32 m_FaultBufferNotifier;
    };

    UINT32 FaultBuffer::m_FaultBufferClass = 0;
    UINT32 FaultBuffer::m_FaultBufferNotifier = 0;

    //-------------------------------------------------------------------------
    //! \brief A description of fault buffer packet
    //!
    //! FaultBufferEntry will parse each field of packet
    //! and provide interface to manipulate the packet
    class FaultBufferEntry
    {
    public:
        typedef shared_ptr<FaultBufferEntry> Pointer;
        //--------------------------------------------------------------------
        //! \brief FaultBufferEntry default constructor
        //!
        FaultBufferEntry() = default;
        //--------------------------------------------------------------------
        //! \brief FaultBufferEntry constructor
        //!
        FaultBufferEntry(const vector<UINT32>& data)
        : m_Data(data)
        {
        }
        //--------------------------------------------------------------------
        //! \brief FaultBufferEntry constructor, constructed by array
        //!
        FaultBufferEntry(const UINT32* pPos, int length)
        : m_Data(pPos, pPos + length)
        {
        }
        //--------------------------------------------------------------------
        //! \brief return faulting address
        //!
        virtual RC GetAddress(UINT64* pAddress) const = 0;
        //--------------------------------------------------------------------
        //! \brief return faulting address aperture
        //!
        virtual RC GetAddressAperture(UINT32* pAperture) const = 0;
        //--------------------------------------------------------------------
        //! \brief return the faulting type
        //!
        virtual RC GetType(UINT32* pType) const = 0;
        //--------------------------------------------------------------------
        //! \brief return the faulting client ID
        //!
        virtual RC GetClientID(UINT32* pClientID) const = 0;
        //--------------------------------------------------------------------
        //! \brief return the faulting timestamp
        //!
        virtual RC GetTimeStamp(UINT64* pTimeStamp) const = 0;
        //--------------------------------------------------------------------
        //! \brief return the faulting engine ID
        //!
        virtual RC GetEngineID(UINT32* pEngineID) const = 0;
        //--------------------------------------------------------------------
        //! \brief return the faulting GPC ID
        //!
        virtual RC GetGPCID(UINT32* pGpcID) const = 0;
        //--------------------------------------------------------------------
        //! \brief return the access type
        //!
        virtual RC GetAccessType(UINT32* pAccessType) const = 0;
        //--------------------------------------------------------------------
        //! \brief return true if valid bit is true
        //!
        virtual RC IsValid(bool* pValid) const = 0;
        //--------------------------------------------------------------------
        //! \brief set faulting valid bit to false
        //!
        virtual RC Ilwalidate() = 0;
        //--------------------------------------------------------------------
        //! \brief get faulting instrance memory physical address
        //!
        virtual RC GetInstPhysAddr(UINT64 * pInstPhysAddr) const = 0;
        //--------------------------------------------------------------------
        //! \brief get faulting instrance memory aperture
        //!
        virtual RC GetInstAperture(UINT32 * pInstAperture) const = 0;

        //--------------------------------------------------------------------
        //! \check whether this faulting address is accessed by virtual
        //!
        virtual RC IsVirtualAccess(bool * isVirtualAccess) const = 0;
        //--------------------------------------------------------------------
        //! \check whether this faulting address is recoverable it is a NR fault
        //!
        virtual RC IsRecoverable(bool *isRecoverable) const = 0;
        //--------------------------------------------------------------------
        //! \check whether this faulting address is for Access Counter Notify Buffer
        //!
        virtual UINT32 GetClass() const = 0;
        //--------------------------------------------------------------------
        //! \brief FaultBufferEntry destructor
        //!
        virtual ~FaultBufferEntry() = 0;
        //--------------------------------------------------------------------
        //! \brief return faulting buffer data at given index
        //!
        //! return UINT32&
        UINT32& operator[](size_t i)
        {
            MASSERT(i < m_Data.size());
            return m_Data[i];
        }
        //--------------------------------------------------------------------
        //! \brief return faulting buffer data at given index
        //!
        //! return const UINT32&
        const UINT32& operator[](size_t i) const
        {
            MASSERT(i < m_Data.size());
            return m_Data[i];
        }
    protected:
            vector<UINT32> m_Data;

    };

    FaultBufferEntry::~FaultBufferEntry()
    {}

    //-------------------------------------------------------------------------
    //! \brief A description of fault packet before Volta
    //!
    class MaxwellFaultBufferEntry : public FaultBufferEntry
    {
    public:
        MaxwellFaultBufferEntry
        (
            const vector<UINT32>& data
        ) :
            FaultBufferEntry(data)
        {}

        MaxwellFaultBufferEntry
        (
            const UINT32* pPos,
            int length
        ) :
            FaultBufferEntry(pPos, length)
        {
        }

        virtual RC GetAddress(UINT64* pAddress) const
        {
            RC rc;

            const UINT64 addrLo = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _ADDR_LO, &m_Data[0]);
            const UINT64 addrHi = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _ADDR_HI, &m_Data[0]);
            *pAddress = addrLo + (addrHi << 32);
            DebugPrintf("replayable fault address = 0x%llx\n", *pAddress);

            return rc;
        }

        virtual RC GetAddressAperture(UINT32 * pAperture) const
        {
            RC rc;

            ErrPrintf("Not support addres aperture before volta.\n");

            return RC::SOFTWARE_ERROR;
        }

        virtual RC GetType(UINT32* pType) const
        {
            RC rc;

            *pType = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _FAULT_TYPE, &m_Data[0]);
            DebugPrintf("replayable fault type = 0x%x\n", *pType);

            return rc;
        }

        virtual RC GetClientID(UINT32* pClientID) const
        {
            RC rc;

            *pClientID = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _CLIENT, &m_Data[0]);
            DebugPrintf("replayable fault ClientID = 0x%x\n", *pClientID);

            return rc;
        }

        virtual RC GetTimeStamp(UINT64* pTimeStamp) const
        {
            RC rc;

            *pTimeStamp = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _TIMESTAMP, &m_Data[0]);
            DebugPrintf("replayable fault timestamp = 0x%llx\n", *pTimeStamp);

            return rc;
        }

        virtual RC GetEngineID(UINT32* pEngineID) const
        {
            ErrPrintf("engine ID is not supported before Volta\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        virtual RC GetGPCID(UINT32* pGpcID) const
        {
            RC rc;

            *pGpcID = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _GPC_ID, &m_Data[0]);
            DebugPrintf("replayable fault GPCID = 0x%x\n", *pGpcID);

            return rc;
        }

        virtual RC GetAccessType(UINT32* pAccessType) const
        {
            RC rc;

            *pAccessType = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _ACCESS_TYPE, &m_Data[0]);
            DebugPrintf("replayable access type = 0x%x\n", *pAccessType);

            return rc;
        }

        virtual RC IsValid(bool* pValid) const
        {
            RC rc;

            *pValid = FLD_TEST_DRF_MW(B069, _FAULT_BUF_ENTRY, _VALID, _TRUE, &m_Data[0]);
            DebugPrintf("replayable valid = 0x%x\n", *pValid);

            return rc;
        }

        virtual RC Ilwalidate()
        {
            RC rc;

            // Clear the valid bit now that the entry has been processed.
            FLD_SET_DRF_DEF_MW(B069, _FAULT_BUF_ENTRY, _VALID, _FALSE, &m_Data[0]);
            DebugPrintf("replayable invalid fault packet\n");

            return rc;
        }

        virtual RC GetInstPhysAddr(UINT64 * pInstPhysAddr) const
        {
            RC rc;

            UINT64 addressLo = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _INST_LO, &m_Data[0]);
            UINT64 addressHi = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _INST_HI, &m_Data[0]);
            *pInstPhysAddr = (addressLo << 12) + (addressHi << 32);

            DebugPrintf("Faulting Instance Memory Physical Address = 0x%llx", *pInstPhysAddr);

            return rc;
        }

        virtual RC GetInstAperture(UINT32 * pInstAperture) const
        {
            RC rc;

            *pInstAperture = DRF_VAL_MW(B069, _FAULT_BUF_ENTRY, _INST_APERTURE, &m_Data[0]);

            DebugPrintf("Faulting Instance Memory Aperture = 0x%x", *pInstAperture);

            return rc;
        }

        virtual RC IsVirtualAccess(bool * isVirtualAccess) const override
        {
            // Before Volta, all access is virtual only
            *isVirtualAccess = true;
            return OK;
        }

        virtual RC IsRecoverable(bool * isRecoverable) const override
        {
            ErrPrintf("%s: Not supported in Maxwell fault buffer\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        virtual UINT32 GetClass() const override
        {
            return FaultBuffer::GetClass();
        }

        virtual ~MaxwellFaultBufferEntry()
        {}
    };

    //-------------------------------------------------------------------------
    //! \brief A description of fault buffer packet in Volta
    //!
    class MMUFaultBufferEntry : public FaultBufferEntry
    {
    public:
        MMUFaultBufferEntry() = default;

        MMUFaultBufferEntry
        (
            const vector<UINT32>& data
        ) :
            FaultBufferEntry(data)
        {}

        MMUFaultBufferEntry
        (
            const UINT32* pPos,
            int length
        ) :
            FaultBufferEntry(pPos, length)
        {
        }

        virtual RC GetAddress(UINT64* pAddress) const
        {
            RC rc;

            const UINT64 addrLo = DRF_VAL_MW(C369, _BUF_ENTRY, _ADDR_LO, &m_Data[0]);
            const UINT64 addrHi = DRF_VAL_MW(C369, _BUF_ENTRY, _ADDR_HI, &m_Data[0]);
            *pAddress = (addrLo << 12) + (addrHi << 32);
            DebugPrintf("MMU fault address = 0x%llx\n", *pAddress);

            return rc;
        }

        virtual RC GetAddressAperture(UINT32 * pAperture) const
        {
            RC rc;

            *pAperture = DRF_VAL_MW(C369, _BUF_ENTRY, _ADDR_PHYS_APERTURE, &m_Data[0]);

            DebugPrintf("MMU fault address aperture = 0x%x\n", *pAperture);

            return rc;
        }

        virtual RC GetType(UINT32* pType) const
        {
            RC rc;

            *pType = DRF_VAL_MW(C369, _BUF_ENTRY, _FAULT_TYPE, &m_Data[0]);
            DebugPrintf("MMU fault type = 0x%x\n", *pType);

            return rc;
        }

        virtual RC GetClientID(UINT32* pClientID) const
        {
            RC rc;

            *pClientID = DRF_VAL_MW(C369, _BUF_ENTRY, _CLIENT, &m_Data[0]);
            DebugPrintf("MMU fault ClientID = 0x%x\n", *pClientID);

            return rc;
        }

        virtual RC GetTimeStamp(UINT64* pTimeStamp) const
        {
            RC rc;

            *pTimeStamp = DRF_VAL_MW(C369, _BUF_ENTRY, _TIMESTAMP, &m_Data[0]);
            DebugPrintf("MMU fault timestamp = 0x%llx\n", *pTimeStamp);

            return rc;
        }

        virtual RC GetEngineID(UINT32* pEngineID) const
        {
            RC rc;

            *pEngineID = DRF_VAL_MW(C369, _BUF_ENTRY, _ENGINE_ID, &m_Data[0]);
            DebugPrintf("MMU fault engine ID = 0x%x\n", *pEngineID);

            return rc;
        }

        virtual RC GetGPCID(UINT32* pGpcID) const
        {
            RC rc;

            *pGpcID = DRF_VAL_MW(C369, _BUF_ENTRY, _GPC_ID, &m_Data[0]);
            DebugPrintf("MMU fault GPCID = 0x%x\n", *pGpcID);

            return rc;
        }

        virtual RC GetAccessType(UINT32* pAccessType) const
        {
            RC rc;

            *pAccessType = DRF_VAL_MW(C369, _BUF_ENTRY, _ACCESS_TYPE, &m_Data[0]);
            DebugPrintf("MMU access type = 0x%x\n", *pAccessType);

            return rc;
        }

        virtual RC IsValid(bool* pValid) const
        {
            RC rc;

            *pValid = FLD_TEST_DRF_MW(C369, _BUF_ENTRY, _VALID, _TRUE, &m_Data[0]);
            DebugPrintf("MMU valid = 0x%x\n", *pValid);

            return rc;
        }

        virtual RC Ilwalidate()
        {
            RC rc;

            // Clear the valid bit now that the entry has been processed.
            FLD_SET_DRF_DEF_MW(C369, _BUF_ENTRY, _VALID, _FALSE, &m_Data[0]);
            DebugPrintf("Ilwalidate MMU fault\n");

            return rc;
        }

        virtual RC GetInstPhysAddr(UINT64* pInstPhysAddr) const
        {
            RC rc;

            UINT64 addressLo = DRF_VAL_MW(C369, _BUF_ENTRY, _INST_LO, &m_Data[0]);
            UINT64 addressHi = DRF_VAL_MW(C369, _BUF_ENTRY, _INST_HI, &m_Data[0]);
            *pInstPhysAddr = (addressLo << 12) + (addressHi << 32);

            DebugPrintf("Faulting Instance Memory Physical Address = 0x%llx\n", *pInstPhysAddr);

            return rc;
        }

        virtual RC GetInstAperture(UINT32 * pInstAperture) const
        {
            RC rc;

            *pInstAperture = DRF_VAL_MW(C369, _BUF_ENTRY, _INST_APERTURE, &m_Data[0]);

            DebugPrintf("Faulting Instance Memory Aperture = 0x%x", *pInstAperture);

            return rc;
        }

        virtual RC IsVirtualAccess(bool * isVirtualAccess) const override
        {
            RC rc;

            UINT32 accessType = 0;
            CHECK_RC(GetAccessType(&accessType));

            *isVirtualAccess = false;
            switch (accessType)
            {
                case LW_PFAULT_ACCESS_TYPE_VIRT_READ:
                case LW_PFAULT_ACCESS_TYPE_VIRT_WRITE:
                case LW_PFAULT_ACCESS_TYPE_VIRT_ATOMIC_WEAK:
                case LW_PFAULT_ACCESS_TYPE_VIRT_ATOMIC_STRONG:
                case LW_PFAULT_ACCESS_TYPE_VIRT_PREFETCH:
                    *isVirtualAccess = true;
                    break;
                case LW_PFAULT_ACCESS_TYPE_PHYS_READ:
                case LW_PFAULT_ACCESS_TYPE_PHYS_WRITE:
                case LW_PFAULT_ACCESS_TYPE_PHYS_ATOMIC:
                case LW_PFAULT_ACCESS_TYPE_PHYS_PREFETCH:
                    *isVirtualAccess = false;
                    break;
                default:
                    ErrPrintf("%s: Not supported access Type.\n", __FUNCTION__);
                    return RC::SOFTWARE_ERROR;
            }
            return rc;
        }

        virtual RC IsRecoverable(bool *isRecoverable) const override
        {
            RC rc;

            UINT32 faultType;
            CHECK_RC(GetType(&faultType));

            switch (faultType)
            {
                case LW_PFAULT_FAULT_TYPE_PDE:
                case LW_PFAULT_FAULT_TYPE_PDE_SIZE:
                case LW_PFAULT_FAULT_TYPE_PTE:
                case LW_PFAULT_FAULT_TYPE_RO_VIOLATION:
                case LW_PFAULT_FAULT_TYPE_WO_VIOLATION:
                case LW_PFAULT_FAULT_TYPE_ATOMIC_VIOLATION:
                    *isRecoverable = true;
                    break;
                default:
                    *isRecoverable = false;
            }
            return rc;
        }

        virtual UINT32 GetClass() const override
        {
            return FaultBuffer::GetClass();
        }

        virtual ~MMUFaultBufferEntry()
        {}
    };

    //-------------------------------------------------------------------------
    //! \brief A description of fault packet in Volta
    //!
    class AccessCounterNotifyBufferEntry : public FaultBufferEntry
    {
    public:
        AccessCounterNotifyBufferEntry
        (
            const vector<UINT32>& data
        ) :
            FaultBufferEntry(data)
        {}

        AccessCounterNotifyBufferEntry
        (
            const UINT32* pPos,
            int length
        ) :
            FaultBufferEntry(pPos, length)
        {
        }

        virtual RC GetAddress(UINT64* pAddress) const
        {
            RC rc;

            const UINT64 addrLo = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _ADDR_LO, &m_Data[0]);
            const UINT64 addrHi = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _ADDR_HI, &m_Data[0]);
            *pAddress = addrLo + (addrHi << 32);
            DebugPrintf("access count fault address = 0x%llx\n", *pAddress);

            return rc;
        }

        virtual RC GetClientID(UINT32* pClientID) const
        {
            return RC::SOFTWARE_ERROR;
        }

        virtual RC GetTimeStamp(UINT64* pTimeStamp) const
        {
            return RC::SOFTWARE_ERROR;
        }

        virtual RC GetEngineID(UINT32* pEngineID) const
        {
            RC rc;

            *pEngineID = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _MMU_ENGINE_ID, &m_Data[0]);
            DebugPrintf("access count engine ID = 0x%x\n", *pEngineID);

            return rc;
        }

        virtual RC GetGPCID(UINT32* pGpcID) const
        {
            return RC::SOFTWARE_ERROR;
        }

        virtual RC IsValid(bool* pValid) const
        {
            return RC::SOFTWARE_ERROR;
        }

        virtual RC Ilwalidate()
        {
            return RC::SOFTWARE_ERROR;
        }

        virtual RC GetInstPhysAddr(UINT64* pInstPhysAddr) const
        {
            RC rc;

            UINT64 addressLo = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _INST_LO, &m_Data[0]);
            UINT64 addressHi = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _INST_HI, &m_Data[0]);

            *pInstPhysAddr = (addressLo << 12) + (addressHi << 32);

            DebugPrintf("Faulting access count Instance Memory Physical Address = 0x%llx\n", *pInstPhysAddr);

            return rc;
        }

        virtual RC GetAddressAperture(UINT32 * pAperture) const
        {
            *pAperture = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _APERTURE, &m_Data[0]);

            DebugPrintf("access counter aperture = 0x%x\n", *pAperture);

            return OK;
        }

        RC GetAccessType(UINT32 * accessType) const
        {
            ErrPrintf("access type not support at the access counter.\n");

            return RC::SOFTWARE_ERROR;
        }

        RC GetType(UINT32 * type) const
        {
            *type = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _TYPE, &m_Data[0]);

            DebugPrintf("access counter type = 0x%x\n", *type);

            return OK;
        }

        RC GetAddressType(UINT32 * addressType) const
        {
            *addressType = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _ADDR_TYPE, &m_Data[0]);

            DebugPrintf("access counter address type = 0x%x\n", *addressType);

            return OK;
        }

        RC GetCounterValue(UINT32 * counterValue) const
        {
            *counterValue = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _COUNTER_VAL, &m_Data[0]);

            DebugPrintf("access counter value = 0x%x\n", *counterValue);

            return OK;
        }

        RC GetPeerID(UINT32 * peerID) const
        {
            *peerID = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _PEER_ID, &m_Data[0]);

            DebugPrintf("access counter peer id = 0x%x\n", *peerID);

            return OK;
        }

        RC GetBank(UINT32 * bank) const
        {
            *bank = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _BANK, &m_Data[0]);

            DebugPrintf("access counter bank = 0x%x\n", *bank);

            return OK;
        }

        RC GetNotifyTag(UINT32 * notifyTag) const
        {
            *notifyTag = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _NOTIFY_TAG, &m_Data[0]);

            DebugPrintf("access counter notify tag = 0x%x\n", *notifyTag);

            return OK;
        }

        RC GetSubGranularity(UINT32 * subGranularity) const
        {
            *subGranularity =
                DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _SUB_GRANULARITY, &m_Data[0]);

            DebugPrintf("access counter subgranularity = 0x%x\n", *subGranularity);

            return OK;
        }

        virtual RC GetInstAperture(UINT32 * pInstAperture) const
        {
            RC rc;

            *pInstAperture = DRF_VAL_MW(C365, _NOTIFY_BUF_ENTRY, _INST_APERTURE, &m_Data[0]);

            DebugPrintf("Faulting Instance Memory Aperture = 0x%x", *pInstAperture);

            return rc;
        }

        virtual RC IsVirtualAccess(bool * isVirtualAccess) const override
        {
            RC rc;
            *isVirtualAccess = true;

            UINT32 addrType(0);
            CHECK_RC(GetAddressType(&addrType));

            if (addrType == LWC365_NOTIFY_BUF_ENTRY_ADDR_TYPE_GPA)
            {
                *isVirtualAccess = false;
            }
            else if (addrType == LWC365_NOTIFY_BUF_ENTRY_ADDR_TYPE_GVA)
            {
                *isVirtualAccess = true;
            }
            else
            {
                MASSERT(!"UnSupported address type");
                return RC::SOFTWARE_ERROR;
            }

            return rc;
        }

        virtual RC IsRecoverable(bool * isRecoverable) const override
        {
            ErrPrintf("%s: Not supported in Access counter notify buffer\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        virtual UINT32 GetClass() const override
        {
            return ACCESS_COUNTER_NOTIFY_BUFFER;
        }

        virtual ~AccessCounterNotifyBufferEntry()
        {}
    };

    //-------------------------------------------------------------------------
    //! \brief A description FaultBufferEntry adapter.
    //! Parse info from register and colwert it to MMU fault buffer packet
    //! Lwrrently since RM reset VALID bit before error notifier is sent to MODS
    //! FAULT register could be overwritten.
    //! The result is that fault info parsed from register could not be corresponding
    //! to current error notifier
    //!
    class MMUFaultPrivAdapter : public MMUFaultBufferEntry
    {
    public:
        typedef shared_ptr<MMUFaultPrivAdapter> Pointer;

        MMUFaultPrivAdapter
        (
            GpuSubdevice *pGpuSubdevice
        ) :
            m_pGpuSubdevice(pGpuSubdevice)
        {}

        virtual RC GetAddress(UINT64* pAddress) const
        {
            RC rc;

            const UINT64 addrLo = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_ADDR_LO_ADDR);
            const UINT64 addrHi = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_ADDR_HI_ADDR);

            *pAddress = (addrLo << 12) + (addrHi << 32);
            DebugPrintf("MMU Priv fault address = 0x%llx\n", *pAddress);

            return rc;
        }

        virtual RC GetAddressAperture(UINT32 * pAperture) const
        {
            RC rc;

            *pAperture = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_ADDR_LO_PHYS_APERTURE);

            DebugPrintf("MMU Priv fault address = 0x%x\n", *pAperture);

            return rc;
        }

        virtual RC GetType(UINT32* pType) const
        {
            RC rc;

            *pType = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_INFO_FAULT_TYPE);
            DebugPrintf("MMU Priv fault type = 0x%x\n", *pType);

            return rc;
        }

        virtual RC GetClientID(UINT32* pClientID) const
        {
            RC rc;

            *pClientID = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_INFO_CLIENT);
            DebugPrintf("MMU Priv fault ClientID = 0x%x\n", *pClientID);

            return rc;
        }

        virtual RC GetTimeStamp(UINT64* pTimeStamp) const
        {
            RC rc;

            *pTimeStamp = 0;
            InfoPrintf("MMU Priv fault timestamp is not available in Priv register\n");

            return rc;
        }

        virtual RC GetEngineID(UINT32* pEngineID) const
        {
            RC rc;

            *pEngineID = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_INST_LO_ENGINE_ID);
            DebugPrintf("MMU Priv fault engine ID = 0x%x\n", *pEngineID);

            return rc;
        }

        virtual RC GetGPCID(UINT32* pGpcID) const
        {
            RC rc;

            *pGpcID = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_INFO_GPC_ID);
            DebugPrintf("MMU Priv fault GPCID = 0x%x\n", *pGpcID);

            return rc;
        }

        virtual RC GetAccessType(UINT32* pAccessType) const
        {
            RC rc;

            *pAccessType = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_INFO_ACCESS_TYPE);
            DebugPrintf("MMU Priv fault access type = 0x%x\n", *pAccessType);

            return rc;
        }

        virtual RC IsValid(bool* pValid) const
        {
            RC rc;

            *pValid = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_INFO_VALID);
            DebugPrintf("MMU Priv fault valid = 0x%x\n", *pValid);

            return rc;
        }

        virtual RC Ilwalidate()
        {
            RC rc;

            m_pGpuSubdevice->Regs().Write32(MODS_PFB_PRI_MMU_FAULT_STATUS_VALID_CLEAR);
            DebugPrintf("Ilwalidate MMU Priv fault\n");

            return rc;
        }

        virtual RC GetInstPhysAddr(UINT64 *pInstPhysAddr) const
        {
            RC rc;

            UINT64 addressLo = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_INST_LO_ADDR);
            UINT64 addressHi = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_INST_HI_ADDR);
            *pInstPhysAddr = (addressLo << 12) + (addressHi << 32);

            DebugPrintf("MMU Priv faulting Instance Memory Physical Address = 0x%llx\n", *pInstPhysAddr);

            return rc;
        }

        virtual RC IsReplayable(bool *pIsReplayable) const
        {
            RC rc;
            const RegHal &regs = m_pGpuSubdevice->Regs();

            const UINT32 faultStatusReg = regs.Read32(MODS_PFB_PRI_MMU_FAULT_STATUS);
            *pIsReplayable = regs.GetField(faultStatusReg, MODS_PFB_PRI_MMU_FAULT_STATUS_REPLAYABLE);

            bool isNonReplayable = regs.GetField(faultStatusReg, MODS_PFB_PRI_MMU_FAULT_STATUS_NON_REPLAYABLE);

            if (*pIsReplayable || isNonReplayable)
            {
                ErrPrintf("fault reported by Priv is neither a replayable nor non-replayable fault!");
                return RC::SOFTWARE_ERROR;
            }

            const string str = *pIsReplayable ? "replayable" : "non-replayable";
            DebugPrintf("%s fault is captured in Priv\n", str.c_str());

            return rc;
        }

        virtual RC GetInstAperture(UINT32 * pInstAperture) const
        {
            RC rc;

            *pInstAperture = m_pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_FAULT_INST_LO_APERTURE);

            DebugPrintf("Faulting Instance Memory Aperture = 0x%x", *pInstAperture);

            return rc;
        }

        virtual ~MMUFaultPrivAdapter()
        {}

    private:
        GpuSubdevice *m_pGpuSubdevice;
    };

    FaultBufferEntry::Pointer CreateFaultBufferEntry(const vector<UINT32>& data)
    {
        if (FaultBuffer::GetClass() == MMU_FAULT_BUFFER)
        {
            return FaultBufferEntry::Pointer(new MMUFaultBufferEntry(data));
        }
        else
        {
            return FaultBufferEntry::Pointer(new MaxwellFaultBufferEntry(data));
        }
    }

    FaultBufferEntry::Pointer CreateFaultBufferEntry(const UINT32* pPos, int length)
    {
        if (FaultBuffer::GetClass() == MMU_FAULT_BUFFER)
        {
            return FaultBufferEntry::Pointer(new MMUFaultBufferEntry(pPos, length));
        }
        else
        {
            return FaultBufferEntry::Pointer(new MaxwellFaultBufferEntry(pPos, length));
        }
    }

    MMUFaultPrivAdapter::Pointer CreateFaultPrivAdapter(GpuSubdevice *pGpuSubdevice)
    {
        return MMUFaultPrivAdapter::Pointer(new MMUFaultPrivAdapter(pGpuSubdevice));
    }

    FaultBufferEntry::Pointer CreateAccessCounterNotifyBufferEntry
    (
         const UINT32 * pPos,
         int length
    )
    {
        return FaultBufferEntry::Pointer(new AccessCounterNotifyBufferEntry(pPos, length));
    }

    class FaultingVeid
    {
    public:
        FaultingVeid
        (
            const FaultBufferEntry * const faultEntry,
            GpuSubdevice * pGpuSubdevice,
            LwRm* pLwRm,
            UINT32 engineId
        ) :
            m_FaultEntry(faultEntry),
            m_pGpuSubdevice(pGpuSubdevice),
            m_VEID(SubCtx::VEID_END),
            m_pLwRm(pLwRm),
            m_engineId(engineId)
        {
            if (!(m_pGpuSubdevice->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID)))
                m_engineId = LW2080_ENGINE_TYPE_GRAPHICS;
            if (FaultBuffer::GetClass() == MMU_FAULT_BUFFER)
            {
                RC rc = m_FaultEntry->GetEngineID(&m_MMUEngineId);
                if (rc != 0)
                {
                    ErrPrintf("%s: Unavailable MMU engine ID.\n");
                }
            }
        }

    public:
        bool GetVeid(UINT32 * pVEID)
        {
            if(m_VEID != SubCtx::VEID_END)
            {
                *pVEID = m_VEID;
                return true;
            }

            PolicyManager * pPolicyManager = PolicyManager::Instance();

            if (m_pGpuSubdevice->IsSMCEnabled() &&
                (m_FaultEntry->GetClass() == ACCESS_COUNTER_NOTIFY_BUFFER))
            {
                m_VEID = m_MMUEngineId - MDiagUtils::GetCachedMMUEngineIdStart(m_pGpuSubdevice);
            }
            else
            {
                m_VEID = pPolicyManager->MMUEngineIdToVEID(m_MMUEngineId, m_pGpuSubdevice, m_pLwRm, m_engineId);
            }

            if (m_VEID == SubCtx::VEID_END)
                return false;

            DebugPrintf("%s: faulting veid = 0x%x\n", __FUNCTION__, m_VEID);
            *pVEID = m_VEID;
            return true;
        }

    private:
        const PmFaulting::FaultBufferEntry* const m_FaultEntry;
        GpuSubdevice * m_pGpuSubdevice;
        UINT32 m_MMUEngineId;
        UINT32 m_VEID;
        LwRm* m_pLwRm;
        UINT32 m_engineId;
    };

    const UINT32 g_BadVeid = 0xdeadbeaf;

    class FaultingInfo
    {
     public:
        FaultingInfo
        (
            const FaultBufferEntry * const faultEntry,
            PolicyManager * pPolicyManager,
            GpuSubdevice* pGpuSubdevice
        ) :
            m_FaultEntry(faultEntry),
            m_PolicyManager(pPolicyManager),
            m_GpuSubdevice(pGpuSubdevice),
            m_pChannel(NULL)
        {
            init();
        }

        bool IsFaultingChannelFound() const
        {
            return m_pChannel != NULL;
        }

        RC GetFaultingVas(LwRm::Handle * hVaSpace, LwRm** ppLwRm)
        {
            if (!IsFaultingChannelFound())
            {
                ErrPrintf("%s, Failed to get vas since faulting channel is not found.\n", __FUNCTION__);
                return RC::ILWALID_CHANNEL;
            }

            RC rc;

            if (m_FaultingSubCtx)
            {
                *hVaSpace = m_FaultingSubCtx->GetVaSpace()->GetHandle();
                *ppLwRm = m_FaultingSubCtx->GetLwRmPtr();
            }
            else
            {
                *hVaSpace = m_pChannel->GetVaSpaceHandle();
                *ppLwRm = m_pChannel->GetLwRmPtr();
            }

            DebugPrintf("%s, faulting vas handle: 0x%x\n", __FUNCTION__, *hVaSpace);

            return rc;
        }

        RC GetFaultingChannels(PmChannels *pPmChannels)
        {
            RC rc;

            if (!IsFaultingChannelFound())
            {
                InfoPrintf("%s, Faulting channels are not found.\n", __FUNCTION__);
                return rc;
            }

            const bool isChannelInTsg = (m_pChannel->GetTsgHandle() != 0);

            if (isChannelInTsg)
            {
                if (m_FaultingSubCtx)
                {
                    GetChannelInFaultingSubCtx(pPmChannels);
                }
                else
                {
                    UINT32 veid = 0;
                    const bool isValidVeid = m_pFaultingVeid->GetVeid(&veid);
                    // isValidVeid will be true if it is a graphics/compute channel
                    // then faulting channel is a compute/graphics channel and in a tsg
                    if (isValidVeid)
                    {
                        GetChannelInFaultingTsg(pPmChannels);
                    }
                    else
                    {
                        pPmChannels->push_back(m_pChannel);
                    }
                }
            }
            else
            {
                pPmChannels->push_back(m_pChannel);
            }

            for (PmChannels::iterator iter = pPmChannels->begin(); iter != pPmChannels->end(); ++iter)
            {
                DebugPrintf("%s, faulting channel: %s\n", __FUNCTION__, (*iter)->GetName().c_str());
            }

            return rc;
        }

        bool GetFaultingVeid(UINT32* pVeid)
        {
            return m_pFaultingVeid->GetVeid(pVeid);
        }

     private:
        void init()
        {
            m_pChannel = GetFaultingChannelByInstAddress();

            if (m_pChannel != NULL)
            {
                m_pFaultingVeid = make_unique<FaultingVeid>(m_FaultEntry, m_GpuSubdevice, m_pChannel->GetLwRmPtr(), m_pChannel->GetEngineId());
                UINT32 veid = 0;
                const bool isValidVeid = m_pFaultingVeid->GetVeid(&veid);

                if (isValidVeid)
                {
                    m_FaultingSubCtx = m_pChannel->GetSubCtxInTsg(veid);
                }
            }
        }

        PmChannel * GetFaultingChannelByInstAddress()
        {
            UINT64 faultingInstPa;
            UINT32 faultingInstAperture;

            RC rc = m_FaultEntry->GetInstPhysAddr(&faultingInstPa);

            if (rc != OK)
            {
                MASSERT(!"FaultingVas::GetFaultingChannelByInstAddress: "
                        "Can't get the legal faulting instance physcial address.");
                return NULL;
            }

            rc = m_FaultEntry->GetInstAperture(&faultingInstAperture);

            if (rc != OK)
            {
                MASSERT(!"FaultingVas::GetFaultingChannelByInstAddress: "
                        "Can't get the legal faulting instance aperture.");
                return NULL;
            }

            PmChannels channels = m_PolicyManager->GetActiveChannels();
            for (PmChannels::iterator ppChannel = channels.begin();
                    ppChannel != channels.end(); ++ppChannel)
            {
                if (((*ppChannel)->GetInstPhysAddr() == faultingInstPa) &&
                        ((*ppChannel)->GetInstAperture() == faultingInstAperture))
                {
                    return (*ppChannel);
                }
            }

            channels = m_PolicyManager->GetInactiveChannels();
            for (PmChannels::iterator ppChannel = channels.begin();
                    ppChannel != channels.end(); ++ppChannel)
            {
                if (((*ppChannel)->GetInstPhysAddr() == faultingInstPa) &&
                        ((*ppChannel)->GetInstAperture() == faultingInstAperture))
                {
                    return (*ppChannel);
                }
            }

            InfoPrintf("Faulting Instance Memory Physical Address 0x%llx doesn't match any channel in the test.\n",
                       faultingInstPa);
            return NULL;
        }

        void GetChannelInFaultingSubCtx(PmChannels *pPmChannels)
        {
            PmChannels allChannels = m_PolicyManager->GetActiveChannels();
            for (PmChannels::iterator ppChannel = allChannels.begin();
                    ppChannel != allChannels.end(); ++ppChannel)
            {
                if ((*ppChannel)->GetSubCtx() == m_FaultingSubCtx)
                {
                    pPmChannels->push_back(*ppChannel);
                }
            }
        }

        void GetChannelInFaultingTsg(PmChannels *pPmChannels)
        {
            PmChannels allChannels = m_PolicyManager->GetActiveChannels();
            for (PmChannels::iterator ppChannel = allChannels.begin();
                    ppChannel != allChannels.end(); ++ppChannel)
            {
                if ((*ppChannel)->GetTsgHandle() == m_pChannel->GetTsgHandle())
                {
                    pPmChannels->push_back(*ppChannel);
                }
            }
        }

        const PmFaulting::FaultBufferEntry* const m_FaultEntry;
        PolicyManager * m_PolicyManager;
        GpuSubdevice* m_GpuSubdevice;
        unique_ptr<FaultingVeid> m_pFaultingVeid;
        PmChannel *m_pChannel;
        shared_ptr<SubCtx> m_FaultingSubCtx;
    };

    class FaultingRange
    {
    public:
        FaultingRange
        (
            const FaultBufferEntry * const faultEntry,
            PolicyManager * pPolicyManager,
            GpuSubdevice * pSubDev
        ) :
            m_FaultEntry(faultEntry),
            m_PolicyManager(pPolicyManager),
            m_GpuSubdevice(pSubDev)
        {}

        //--------------------------------------------------------------------
        //! \brief Get the memory range that includes the notification address
        //!
        RC GetMemRange
        (
             PmMemRange *range
        )
        {
            RC rc;
            UINT64 address(0);
            LwRm::Handle hFaultingVaSpace(0);
            bool virtualAccess = true;
            LwRm* pLwRm = LwRmPtr().Get();

            CHECK_RC(m_FaultEntry->GetAddress(&address));
            CHECK_RC(m_FaultEntry->IsVirtualAccess(&virtualAccess));

            PmSubsurfaces subsurfaces = m_PolicyManager->GetActiveSubsurfaces();

            if (virtualAccess)
            {
                FaultingInfo faultingInfo(m_FaultEntry, m_PolicyManager, m_GpuSubdevice);
                CHECK_RC(faultingInfo.GetFaultingVas(&hFaultingVaSpace, &pLwRm));

                for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
                        ppSubsurface != subsurfaces.end(); ++ppSubsurface)
                {
                    UINT64 surfaceAddress = (*ppSubsurface)->GetGpuAddr();
                    UINT64 surfaceSize = (*ppSubsurface)->GetSize();
                    const LwRm::Handle hVaSpace = (*ppSubsurface)->GetMdiagSurf()->GetGpuVASpace();

                    if ((address >= surfaceAddress) &&
                            (address < surfaceAddress + surfaceSize) &&
                            (hFaultingVaSpace == hVaSpace) &&
                            (pLwRm == (*ppSubsurface)->GetMdiagSurf()->GetLwRmPtr()))
                    {
                        range->Set((*ppSubsurface)->GetSurface(),
                                address - surfaceAddress,  // Offset
                                1);                        // Size

                        return OK;
                    }
                }
            }
            else
            {
                // re-order pm surfaces and put the surface having memmapping in the front
                // so that if we can avoid to create memmappings as far as possible
                PmSubsurfaces sortedPmSubSurfaces(subsurfaces.size());
                int front = 0, tail = subsurfaces.size() - 1;
                for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
                        ppSubsurface != subsurfaces.end(); ++ppSubsurface)
                {
                    PmSurface *surface = (*ppSubsurface)->GetSurface();
                    PmMemMappings memMappings =
                        surface->GetMemMappingsHelper()->GetMemMappings(NULL);

                    // put surfaces not having memmappings in the tail
                    if (memMappings.size() == 0)
                    {
                        sortedPmSubSurfaces[tail--] =  *ppSubsurface;
                    }
                    else
                    {
                        sortedPmSubSurfaces[front++] = *ppSubsurface;
                    }
                }

                PmSurface *lastSurface = 0;
                for (PmSubsurfaces::iterator ppSubsurface = sortedPmSubSurfaces.begin();
                        ppSubsurface != sortedPmSubSurfaces.end(); ++ppSubsurface)
                {
                    PmSurface *surface = (*ppSubsurface)->GetSurface();

                    if (surface == lastSurface)
                    {
                        continue;
                    }
                    else
                    {
                        lastSurface = surface;
                    }

                    PmMemMappings memMappings =
                        surface->GetMemMappingsHelper()->GetMemMappings(NULL);

                    // if mem mappings are not initialized, call CreateMemMappings
                    if (memMappings.size() == 0)
                    {
                        CHECK_RC(surface->CreateMemMappings());
                    }

                    PmMemRange memRange(surface, 0, surface->GetSize());
                    PmMemMappings mappings = surface->GetMemMappingsHelper()->GetMemMappings(&memRange);
                    UINT32 aperture;

                    CHECK_RC(m_FaultEntry->GetAddressAperture(&aperture));

                    UINT32 lwmulativeOffset = 0;
                    PmMemMappings::iterator ppMappings;
                    for (ppMappings = mappings.begin(); ppMappings != mappings.end(); ++ppMappings)
                    {
                        UINT32 surfaceAperture = DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _APERTURE,
                                (*ppMappings)->GetPteFlags());
                        if (aperture == surfaceAperture)
                        {
                            UINT64 surfaceAddress = (*ppMappings)->GetPhysAddr();
                            UINT64 surfaceSize = (*ppMappings)->GetPhysRange()->GetSize();

                            if ((address >= surfaceAddress) &&
                                    (address < surfaceAddress + surfaceSize))
                            {
                                range->Set((*ppMappings)->GetSurface(),
                                        (address - surfaceAddress) + lwmulativeOffset,  // Offset
                                        1);                                             // Size

                                return OK;
                            }
                            else
                            {
                                lwmulativeOffset += surfaceSize;
                            }
                        }
                    }
                }
            }

            ErrPrintf("gpu virtual address 0x%llx doesn't match any surface in the test.\n", address);

            return RC::SOFTWARE_ERROR;
        }

    private:
            const FaultBufferEntry* const m_FaultEntry;
            PolicyManager * m_PolicyManager;
            GpuSubdevice* m_GpuSubdevice;
    };

    //--------------------------------------------------------------------
    //! \brief A description of mmu engine ID and type
    //!
    //! TODO: only one subdevice is supported. It need to
    //! be extended if SLI should be supported
    //!
    class MMUEngineDesc
    {
        public:
            MMUEngineDesc
            (
                UINT32 engineID,
                GpuSubdevice* pSubDev,
                LwRm* pLwRm
            ) :
                m_EngineID(engineID),
                m_SubDev(pSubDev),
                m_pLwRm(pLwRm)
            {}

            bool Type(UINT32 *pType)
            {
                if (m_MMUEngineID2Type.find(m_pLwRm) == m_MMUEngineID2Type.end())
                {
                    CreateEngine2TypeMapping(m_SubDev, m_pLwRm);
                }

                if (m_MMUEngineID2Type[m_pLwRm].count(m_EngineID) == 0)
                {
                    WarnPrintf("Engine id (0x%x) is not associated with any engine type.\n", m_EngineID);
                    return false;
                }

                *pType = m_MMUEngineID2Type[m_pLwRm][m_EngineID];
                return true;
            }

        private:
            static void CreateEngine2TypeMapping(GpuSubdevice* pSubDev, LwRm* pLwRm);
            static vector<UINT32> GetSupportedEngines(GpuSubdevice* pSubDev, LwRm* pLwRm);

            UINT32 m_EngineID;
            GpuSubdevice* m_SubDev;
            LwRm* m_pLwRm;
            static map<LwRm*, map<UINT32, UINT32>> m_MMUEngineID2Type;
    };

    //--------------------------------------------------------------------
    //! \brief create mapping between MMU engine ID to engine type
    //!
    void MMUEngineDesc::CreateEngine2TypeMapping(GpuSubdevice* pSubDev, LwRm* pLwRm)
    {
        vector<UINT32> engines = GetSupportedEngines(pSubDev, pLwRm);

        for (vector<UINT32>::iterator iter = engines.begin(); iter != engines.end(); ++iter)
        {
            const UINT32 engineType = *iter;
            LW2080_CTRL_GPU_GET_ENGINE_FAULT_INFO_PARAMS params = {0};
            params.engineType = engineType;
            RC rc = pLwRm->ControlBySubdevice(
                    pSubDev, LW2080_CTRL_CMD_GPU_GET_ENGINE_FAULT_INFO,
                    (void *)&params, sizeof(params));
            if (rc != OK)
            {
                WarnPrintf("Fail to ilwoke LW2080_CTRL_CMD_GPU_GET_ENGINE_FAULT_INFO. Engine type(0x%x)\n", engineType);
                // on CheetAh we can have host1x engines not in the GPU
                // so they do not have fault info, simply skip them
                if (Platform::IsTegra())
                {
                    continue;
                }
                else
                {
                    MASSERT(!"Failed to create mapping between MMU engine ID to engine type!");
                }

            }
            DebugPrintf("%s: Creating mapping engine ID (0x%x) -> engine type (0x%0x)\n",
                        __FUNCTION__, params.mmuFaultId, engineType);
            m_MMUEngineID2Type[pLwRm][params.mmuFaultId] = engineType;
        }
    }

    //--------------------------------------------------------------------
    //! \brief get supported engine list
    //!
    vector<UINT32> MMUEngineDesc::GetSupportedEngines(GpuSubdevice* pSubDev, LwRm* pLwRm)
    {
        RC rc;
        LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams;
        LwRm::Handle handle = pLwRm->GetSubdeviceHandle(pSubDev);
        memset(&engineParams, 0, sizeof(engineParams));
        rc = pLwRm->Control(handle,
                LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                &engineParams,
                sizeof(engineParams));
        if (rc != OK)
        {
            ErrPrintf("%s, failed to ilwoke LW2080_CTRL_CMD_GPU_GET_ENGINES_V2", __FUNCTION__);
            MASSERT(0);
        }

        // Get the list of supported engine
        vector<UINT32> engines(engineParams.engineCount);
        for (UINT32 i = 0; i < engineParams.engineCount; i++)
        {
            engines[i] = engineParams.engineList[i];
        }

        return engines;
    }

    map<LwRm*, map<UINT32, UINT32>> MMUEngineDesc::m_MMUEngineID2Type;
}

using namespace PmFaulting;

//--------------------------------------------------------------------
//! \brief STL comparator to sort GpuDevice* by DeviceInst
//!
bool CmpGpuDevicesByInst::operator()
(
    const GpuDevice *pLhs,
    const GpuDevice *pRhs
) const
{
    return pLhs->GetDeviceInst() < pRhs->GetDeviceInst();
}

//////////////////////////////////////////////////////////////////////
// PmPickCounter
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor.
//!
//! \param pFancyPicker This argument lets several PmPickCounters
//!     share the same FancyPicker.  If NULL, the PmPickCounter uses
//!     an internal FancyPicker.  Note that the internal FancyPicker
//!     is automatically deleted with the PmPickCounter, but an
//!     external FancyPicker is not.
//! \param bUseSeedString true to use the seed string, false uses
//!     the explicit seed
//! \param seed The seed that is used to initialize the FancyPicker's
//!     random-number generator (only used if bUseSeedString is false)
//! \param seedString This string is colwerted to a UINT32 via a
//!     simple hash, and used to initialize the FancyPicker's random-
//!     number generator.  We use a string as a seed because (a) we
//!     need different seeds so that the RNG's don't all return the
//!     same numbers, and (b) we need the seeds to be the same from
//!     run to run.  The easiest way to meet both criteria is to use
//!     the name of the trigger/actionblock/whatever as the seed.
//!     (only used if bUseSeedString is true)
//!
PmPickCounter::PmPickCounter
(
 FancyPicker  *pFancyPicker,
 bool          bUseSeedString,
 UINT32        seed,
 const string &seedString
 )
{
    // Init the members
    //
    if (pFancyPicker == NULL)
    {
        m_pFancyPicker = new FancyPicker();
        m_UsesExternalFancyPicker = false;
    }
    else
    {
        m_pFancyPicker = pFancyPicker;
        m_UsesExternalFancyPicker = true;
    }
    m_FpContext.LoopNum    = 0;
    m_FpContext.RestartNum = 0;
    m_FpContext.pJSObj     = 0;
    m_Counter   = 0;
    m_Done      = false;
    m_NextCount = 0;

    // Seed the random-number generator
    //
    if (bUseSeedString)
    {
        // Seed the random-number generator
        //
        UINT64 BIGGEST_32_BIT_PRIME = 0x00000000fffffffbUL;
        UINT64 strSeed = 0;
        for (size_t ii = 0; ii < seedString.length(); ++ii)
        {
            strSeed = (strSeed << 8) + (seedString[ii] & 0x00ff);
            strSeed %= BIGGEST_32_BIT_PRIME;
        }
        m_FpContext.Rand.SeedRandom(static_cast<UINT32>(strSeed));
    }
    else
    {
        m_FpContext.Rand.SeedRandom(seed);
    }

    // Pick the first increment
    //
    m_NextCount = PickIncrement() - 1;

    m_bCounterMatches = (m_NextCount == 0);
    if (m_bCounterMatches)
        m_NextCount += PickIncrement();
}

//--------------------------------------------------------------------
//! \brief Increment the counter
//!
void PmPickCounter::IncrCounter(UINT64 incr)
{
    // Ensure that the increment will not pass the next count value
    MASSERT(m_bCounterMatches || m_Done ||
            ((m_Counter + incr) <= m_NextCount));

    if (incr == 0)
        return;

    m_bCounterMatches = false;

    m_Counter += incr;
    if (m_Counter == m_NextCount && !m_Done)
    {
        m_NextCount += PickIncrement();
        m_bCounterMatches = true;
    }
}

//--------------------------------------------------------------------
//! \brief Return the number of counts until the next counter match
//!
UINT64 PmPickCounter::GetCountRemaining() const
{
    return m_Done ? 0 : (m_NextCount - m_Counter);
}

//--------------------------------------------------------------------
//! \brief Reset the counter to 0.  Increments the FpContext's RestartNum
//!
void PmPickCounter::Restart()
{
    m_FpContext.LoopNum = 0;
    ++m_FpContext.RestartNum;
    m_Counter   = 0;
    m_Done      = false;
    m_NextCount = PickIncrement() - 1;

    m_bCounterMatches = (m_NextCount == 0);
    if (m_bCounterMatches)
        m_NextCount += PickIncrement();
}

//--------------------------------------------------------------------
//! \brief Pick the next increment from the fancyPicker
//!
//! This method automatically sets up the FpContext, so that each
//! PickIncrement() returns a new pick.
//!
UINT64 PmPickCounter::PickIncrement()
{
    m_pFancyPicker->SetContext(&m_FpContext);

    UINT64 incr = m_pFancyPicker->Pick();
    ++m_FpContext.LoopNum;
    if (incr == 0)
        m_Done = true;
    return incr;
}

//////////////////////////////////////////////////////////////////////
// PmSurfaceDesc
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmSurfaceDesc::PmSurfaceDesc
(
 const string &id
 ) :
    m_Id(id),
    m_pGpuDesc(NULL),
    m_Faulting(false),
    m_pTestDesc(NULL)
{
}

//--------------------------------------------------------------------
//! \brief Set regular expression that the surface's name must not match
//!
RC PmSurfaceDesc::SetNotName(const string &str)
{
    return m_NotName.Set(str, RegExp::FULL | RegExp::ILWERT);
}

//--------------------------------------------------------------------
//! \brief Set regular expression that the surface's type must not match
//!
RC PmSurfaceDesc::SetNotType(const string &str)
{
    return m_NotType.Set(str, RegExp::FULL | RegExp::ILWERT);
}

//--------------------------------------------------------------------
//! \brief Return OK if the GpuDesc is set correctly
//!
RC PmSurfaceDesc::SetGpuDesc
(
    const PmGpuDesc* pGpuDesc
)
{
    RC rc;

    if (pGpuDesc->GetFaulting())
    {
        ErrPrintf("Gpu.Faulting is not supported inside Surface\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_pGpuDesc = pGpuDesc;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return OK if the TestDesc is set correctly
//!
RC PmSurfaceDesc::SetTestDesc
(
    const PmTestDesc * pTestDesc
)
{
    RC rc;

    if (pTestDesc->GetFaulting())
    {
        ErrPrintf("Test.Faulting is not supported inside Surface\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_pTestDesc = pTestDesc;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return OK if the surfaceDesc is supported in a trigger.
//!
RC PmSurfaceDesc::IsSupportedInTrigger() const
{
    RC rc;

    if (m_Faulting)
    {
        ErrPrintf("Surface.Faulting is not supported in triggers\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return OK if the surfaceDesc is supported in an action.
//! \param pTrigger The trigger that the action is triggered by.
//!
RC PmSurfaceDesc::IsSupportedInAction(const PmTrigger *pTrigger) const
{
    MASSERT(pTrigger != NULL);
    RC rc;

    if (m_Faulting && !pTrigger->HasMemRange())
    {
        ErrPrintf("Used Surface.Faulting in a trigger with no associated memory\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return true if the SurfaceDesc matches the event.  Used by
//! triggers.
//!
bool PmSurfaceDesc::Matches(const PmEvent *pEvent) const
{
    MASSERT(pEvent != NULL);
    PmSubsurfaces subsurfaces = pEvent->GetSubsurfaces();

    // Check each subsurface for a match
    //
    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        if (SimpleMatch(*ppSubsurface))
        {
            return true;
        }
    }

    // If we get this far, the event isn't a match
    //
    return false;
}

//--------------------------------------------------------------------
//! \brief Return all subsurfaces that match the SurfaceDesc.  Used by
//! actions.
//!
PmSubsurfaces PmSurfaceDesc::GetMatchingSubsurfaces
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);

    // Get the criteria we'll use to select the matching pages.
    //
    PmSubsurfaces        candidateSubsurfaces;
    const PmSurfaceDesc *pTriggerSurfaceDesc;
    const PmPageDesc    *pTriggerPageDesc;
    const PmMemRange    *pEventMemRange;

    if (m_Faulting)
    {
        candidateSubsurfaces = pEvent->GetSubsurfaces();
        pTriggerSurfaceDesc  = pTrigger->GetSurfaceDesc();
        pTriggerPageDesc     = pTrigger->GetPageDesc();
        pEventMemRange       = pEvent->GetMemRange();
    }
    else
    {
        candidateSubsurfaces =
            pTrigger->GetPolicyManager()->GetActiveSubsurfaces();
        pTriggerSurfaceDesc  = NULL;
        pTriggerPageDesc     = NULL;
        pEventMemRange       = NULL;
    }

    // Find the matching subsurfaces.
    //
    PmSubsurfaces matchingSubsurfaces;

    for (PmSubsurfaces::iterator ppSubsurface = candidateSubsurfaces.begin();
         ppSubsurface != candidateSubsurfaces.end(); ++ppSubsurface)
    {
        if (SimpleMatch(*ppSubsurface)
            && (pTriggerSurfaceDesc == NULL ||
                pTriggerSurfaceDesc->SimpleMatch(*ppSubsurface))
            && (pTriggerPageDesc == NULL ||
                pEventMemRange == NULL ||
                pTriggerPageDesc->SimpleMatch(*ppSubsurface,
                                              pEventMemRange)))
        {
            matchingSubsurfaces.push_back(*ppSubsurface);
        }
    }

    return matchingSubsurfaces;
}

//--------------------------------------------------------------------
//! \brief Return all subsurfaces that match the SurfaceDesc.  Used by
//! triggers.
//!
PmSubsurfaces PmSurfaceDesc::GetMatchingSubsurfacesInTrigger
(
    const PolicyManager *pPolicyManager
) const
{
    // Get the criteria we'll use to select the matching pages.
    //
    PmSubsurfaces candidateSubsurfaces =
        pPolicyManager->GetActiveSubsurfaces();

    // Find the matching subsurfaces.
    //
    PmSubsurfaces matchingSubsurfaces;

    for (PmSubsurfaces::iterator ppSubsurface = candidateSubsurfaces.begin();
         ppSubsurface != candidateSubsurfaces.end(); ++ppSubsurface)
    {
        if (SimpleMatch(*ppSubsurface))
        {
            matchingSubsurfaces.push_back(*ppSubsurface);
        }
    }

    return matchingSubsurfaces;
}

//--------------------------------------------------------------------
//! Private function that checks whether the surface matches
//! everything in this surfaceDesc except m_Faulting (which need some
//! extra work).
//!
bool PmSurfaceDesc::SimpleMatch(const PmSubsurface *pSubsurface) const
{
    MASSERT(pSubsurface != NULL);
    GpuDevice *pSurfaceGpuDevice = pSubsurface->GetMdiagSurf()->GetGpuDev();
    PmTest* pTest = pSubsurface->GetSurface()->GetTest();

    return (m_Name.Matches(pSubsurface->GetName()) &&
            m_NotName.Matches(pSubsurface->GetName()) &&
            m_Type.Matches(pSubsurface->GetTypeName()) &&
            m_NotType.Matches(pSubsurface->GetTypeName()) &&
            (m_pGpuDesc == NULL ||
             m_pGpuDesc->SimpleMatch(pSurfaceGpuDevice)) &&
            (m_pTestDesc == NULL || m_pTestDesc->SimpleMatch(pTest)));
}

//////////////////////////////////////////////////////////////////////
// PmPageDesc
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmPageDesc::PmPageDesc
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size
) :
    m_pSurfaceDesc(pSurfaceDesc),
    m_Offset(offset),
    m_Size(size)
{
    MASSERT(m_pSurfaceDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the pageDesc is supported in a trigger.
//!
RC PmPageDesc::IsSupportedInTrigger() const
{
    return m_pSurfaceDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! \brief Return OK if the pageDesc is supported in an action.
//! \param pTrigger The trigger that the action is triggered by.
//!
RC PmPageDesc::IsSupportedInAction(const PmTrigger *pTrigger) const
{
    return m_pSurfaceDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Return true if the PageDesc matches the event.  Used by
//! triggers.
//!
bool PmPageDesc::Matches(const PmEvent *pEvent) const
{
    MASSERT(pEvent != NULL);
    PmSubsurfaces     subsurfaces = pEvent->GetSubsurfaces();
    const PmMemRange *pMemRange   = pEvent->GetMemRange();

    // Check each subsurface for a match
    //
    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        if (SimpleMatch(*ppSubsurface, pMemRange))
        {
            return true;
        }
    }

    // If we get this far, the event isn't a match
    //
    return false;
}

//--------------------------------------------------------------------
//! \brief Return PmMemRanges containing all pages that match the
//! pageDesc.  Used by actions.
//!
PmMemRanges PmPageDesc::GetMatchingRanges
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);

    // Use PmSurfaceDesc to find the faulting/matching pages
    //
    PmSubsurfaces subsurfaces =
        m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);

    // Use m_Offset & m_Size to extract a range from each surface
    //
    PmMemRanges matchingRanges;

    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        PmMemRange range((*ppSubsurface)->GetSurface(),
                         (*ppSubsurface)->GetOffset() + m_Offset,
                         m_Size == 0 ? (*ppSubsurface)->GetSize() : m_Size);
        if (range.Overlaps(*ppSubsurface))
        {
            matchingRanges.push_back(range.GetIntersection(*ppSubsurface));
        }
    }

    return matchingRanges;
}

//--------------------------------------------------------------------
//! \brief Return PmMemRanges containing all pages that match the
//! pageDesc.  Used by triggers.
//!
PmMemRanges PmPageDesc::GetMatchingRangesInTrigger
(
    const PolicyManager *pPolicyManager
) const
{
    // Use PmSurfaceDesc to find the faulting/matching pages
    //
    PmSubsurfaces subsurfaces =
        m_pSurfaceDesc->GetMatchingSubsurfacesInTrigger(pPolicyManager);

    // Use m_Offset & m_Size to extract a range from each surface
    //
    PmMemRanges matchingRanges;

    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        PmMemRange range((*ppSubsurface)->GetSurface(),
                         (*ppSubsurface)->GetOffset() + m_Offset,
                         m_Size == 0 ? (*ppSubsurface)->GetSize() : m_Size);
        if (range.Overlaps(*ppSubsurface))
        {
            matchingRanges.push_back(range.GetIntersection(*ppSubsurface));
        }
    }

    return matchingRanges;
}

//--------------------------------------------------------------------
//! Private function that checks whether the subsurface/range pair
//! matches everything in this pageDesc except m_Faulting (which need
//! some extra work).
//!
bool PmPageDesc::SimpleMatch
(
    const PmSubsurface *pSubsurface,
    const PmMemRange *pMemRange
) const
{
    MASSERT(pSubsurface != NULL);
    if (!m_pSurfaceDesc->SimpleMatch(pSubsurface))
        return false;

    if (m_Offset < pSubsurface->GetSize())
    {
        UINT64 RangeSize = pSubsurface->GetSize() - m_Offset;
        if (m_Size != 0)
        {
            RangeSize = min(RangeSize, m_Size);
        }
        PmMemRange range(pSubsurface->GetSurface(),
                         pSubsurface->GetOffset() + m_Offset,
                         RangeSize);
        return range.Overlaps(pMemRange);
    }
    return false;
}

//////////////////////////////////////////////////////////////////////
// PmVfTestDesc
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmVfTestDesc::PmVfTestDesc
(
    const string &id
) :
    m_Id(id)
{
}

//--------------------------------------------------------------------
//! \brief Return all channels that match the channelDesc.  Used by actions.
//!
PmVfTests PmVfTestDesc::GetMatchingVfs
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);

    PmVfTests matchingVfs;

    PmVfTests allVfs =
        pTrigger->GetPolicyManager()->GetActiveVfs();
    for (PmVfTests::iterator ppVf = allVfs.begin();
            ppVf != allVfs.end(); ++ppVf)
    {
        if (SimpleMatch(*ppVf))
        {
            matchingVfs.push_back(*ppVf);
        }
    }

    return matchingVfs;
}

//--------------------------------------------------------------------
//! Private function that checks whether a Vf matches everything
//! in this VfDesc except m_Faulting (which requires extra work).
//!
bool PmVfTestDesc::SimpleMatch(const PmVfTest *pVf) const
{
    MASSERT(pVf != NULL);

    if (!m_SeqId.ToString().empty())
    {
        return m_SeqId.Matches(Utility::StrPrintf("%d", pVf->GetSeqId()));
    }
    else if (!m_GFID.ToString().empty())
    {
        return m_GFID.Matches(Utility::StrPrintf("%d", pVf->GetGFID()));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// PmSmcEngineDesc
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmSmcEngineDesc::PmSmcEngineDesc
(
    const string &id
) :
    m_Id(id),
    m_pTestDesc(nullptr)
{
}

//--------------------------------------------------------------------
//! \brief Return all smcengines that match the smcEngineDesc.  Used by actions.
//!
PmSmcEngines PmSmcEngineDesc::GetMatchingSmcEngines
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);

    PmSmcEngines matchingSmcEngines;

    if (m_pTestDesc)
    {
        PmTests matchingTests = m_pTestDesc->GetMatchingTests(pTrigger, pEvent, nullptr);
        for (auto const & pmTest : matchingTests)
        {
            // Gather unique SmcEngines
            if (pmTest->GetSmcEngine() &&
                find(matchingSmcEngines.begin(), matchingSmcEngines.end(), pmTest->GetSmcEngine()) == matchingSmcEngines.end())
            {
                matchingSmcEngines.push_back(pmTest->GetSmcEngine());
            }
        }
    }
    else
    {
        PmSmcEngines allSmcEngines =
            pTrigger->GetPolicyManager()->GetActiveSmcEngines();
        for (PmSmcEngines::iterator ppSmcEngine = allSmcEngines.begin();
                ppSmcEngine != allSmcEngines.end(); ++ppSmcEngine)
        {
            if (SimpleMatch(*ppSmcEngine))
            {
                matchingSmcEngines.push_back(*ppSmcEngine);
            }
        }
    }

    return matchingSmcEngines;
}

//--------------------------------------------------------------------
//! Private function that checks whether a SmcEngine matches everything
//! in this SmcEngineDesc
//!
bool PmSmcEngineDesc::SimpleMatch(const PmSmcEngine *pSmcEngine) const
{
    MASSERT(pSmcEngine != NULL);

    if (!m_SysPipe.ToString().empty())
    {
        return m_SysPipe.Matches(Utility::StrPrintf("%d", pSmcEngine->GetSysPipe()));
    }
    else if (!m_Name.ToString().empty())
    {
        return m_Name.Matches(pSmcEngine->GetName());
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// PmVaSpaceDesc
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmVaSpaceDesc::PmVaSpaceDesc
(
    const string &id
) :
    m_Id(id),
    m_pTestDesc(nullptr)
{
}

//--------------------------------------------------------------------
//! \brief Return all channels that match the channelDesc.  Used by actions.
//!
PmVaSpaces PmVaSpaceDesc::GetMatchingVaSpaces
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent,
    const LwRm      *pLwRm
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);

    PmVaSpaces matchingVaSpaces;

    PmVaSpaces allVaSpaces =
        pTrigger->GetPolicyManager()->GetActiveVaSpaces();
    for (PmVaSpaces::iterator ppVaSpace = allVaSpaces.begin();
            ppVaSpace != allVaSpaces.end(); ++ppVaSpace)
    {
        if (SimpleMatch(*ppVaSpace))
        {
            // If GpuPartiton's LwRm* has been specified (by using
            // Policy.SetSmcEngine) then match its LwRm* with the VaSpace's LwRm*
            if (pLwRm)
            {
                if ((*ppVaSpace)->GetLwRmPtr() == pLwRm)
                    matchingVaSpaces.push_back(*ppVaSpace);
            }
            else
                matchingVaSpaces.push_back(*ppVaSpace);
        }
    }

    return matchingVaSpaces;
}

//--------------------------------------------------------------------
//! Private function that checks whether a vaspace matches everything
//! in this vaspaceDesc except m_Faulting (which requires extra work).
//!
bool PmVaSpaceDesc::SimpleMatch(const PmVaSpace *pVaSpace) const
{
    MASSERT(pVaSpace != NULL);

    if (!m_Name.Matches(pVaSpace->GetName()))
        return false;

    if (m_pTestDesc && !m_pTestDesc->SimpleMatch(pVaSpace->GetTest()))
        return false;

    return true;
}
//////////////////////////////////////////////////////////////////////
// PmChannelDesc
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmChannelDesc::PmChannelDesc
(
    const string &id
) :
    m_Id(id),
    m_pGpuDesc(NULL),
    m_Faulting(false),
    m_pTestDesc(NULL)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the channelDesc is supported in a trigger.
//!
RC PmChannelDesc::IsSupportedInTrigger() const
{
    RC rc;
    if (m_Faulting)
    {
        ErrPrintf("Channel.Faulting is not supported in triggers\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return OK if the channelDesc is supported in an action.
//! \param pTrigger The trigger that the action is triggered by.
//!
RC PmChannelDesc::IsSupportedInAction(const PmTrigger *pTrigger) const
{
    MASSERT(pTrigger != NULL);
    RC rc;

    if (m_Faulting && !pTrigger->HasChannel())
    {
        ErrPrintf("Used Channel.Faulting in a trigger with no channel\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return OK if PmGpuDesc is set correctly
//!
RC PmChannelDesc::SetGpuDesc
(
    const PmGpuDesc *pGpuDesc
)
{
    RC rc;

    if (pGpuDesc->GetFaulting())
    {
        ErrPrintf("Gpu.Faulting is not supported inside Channel\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_pGpuDesc = pGpuDesc;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return OK if PmTestDesc is set correctly
//!
RC PmChannelDesc::SetTestDesc
(
    const PmTestDesc * pTestDesc
)
{
    RC rc;

    if (pTestDesc->GetFaulting())
    {
        ErrPrintf("Test.Faulting is not supported inside Channel\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_pTestDesc = pTestDesc;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return true if the channelDesc matches the event.  Used by
//! triggers.
//!
bool PmChannelDesc::Matches(const PmEvent *pEvent) const
{
    PmChannels pmChannels = pEvent->GetChannels();
    for (PmChannels::iterator ppChannel = pmChannels.begin();
         ppChannel != pmChannels.end(); ++ppChannel)
    {
        if (SimpleMatch(*ppChannel))
        {
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------
//! \brief Return all channels that match the channelDesc.  Used by actions.
//!
PmChannels PmChannelDesc::GetMatchingChannels
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);

    PmChannels matchingChannels;

    if (m_Faulting)
    {
        PmChannels channels = pEvent->GetChannels();
        for (PmChannels::iterator ppChannel = channels.begin();
             ppChannel != channels.end(); ++ppChannel)
        {
            if (SimpleMatch(*ppChannel))
            {
                matchingChannels.push_back(*ppChannel);
            }
        }
    }
    else
    {
        PmChannels allChannels =
            pTrigger->GetPolicyManager()->GetActiveChannels();
        for (PmChannels::iterator ppChannel = allChannels.begin();
             ppChannel != allChannels.end(); ++ppChannel)
        {
            if (SimpleMatch(*ppChannel))
            {
                matchingChannels.push_back(*ppChannel);
            }
        }
    }

    return matchingChannels;
}

//--------------------------------------------------------------------
//! \brief Return a subdevice mask that matches the channelDesc
//!
//! Return a subdevice mask that can be passed to
//! Channel::WriteSetSubdevice(), for all subdevices on the indicated
//! channel that match the channelDesc.
//!
//! Returns Channel::AllSubdevicesMask if all subdevices mask.  (This
//! is the usual case for non-SLI devices.)
//!
//! Used by actions.
//!
//! \param pChannel A channel that was returned by GetMatchingChannels()
//! \param pTrigger The trigger passed to Action::Execute()
//! \param pEvent   The event passed to Action::Execute()
//!
UINT32 PmChannelDesc::GetMatchingSubdevices
(
    const PmChannel *pChannel,
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent
) const
{
    UINT32 subdeviceMask;

    if (m_pGpuDesc == NULL)
    {
        subdeviceMask = Channel::AllSubdevicesMask;
    }
    else
    {
        subdeviceMask = 0;
        GpuDevice *pChannelGpuDevice = pChannel->GetGpuDevice();
        GpuSubdevices gpuSubdevices =
            m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

        for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevices.begin();
             ppGpuSubdevice != gpuSubdevices.end(); ++ppGpuSubdevice)
        {
            if ((*ppGpuSubdevice)->GetParentDevice() == pChannelGpuDevice)
            {
                subdeviceMask |= (1 << (*ppGpuSubdevice)->GetSubdeviceInst());
            }
        }

        if (subdeviceMask == pChannelGpuDevice->GetNumSubdevices() - 1)
        {
            subdeviceMask = Channel::AllSubdevicesMask;
        }
    }

    return subdeviceMask;
}

//--------------------------------------------------------------------
//! Return all subchannels that match the channelDesc for a channel.
//! Used by actions.
//!
//! \param pChannel A channel that was returned by GetMatchingChannels()
//! \param pTrigger The trigger passed to Action::Execute()
//! \param pEvent   The event passed to Action::Execute()
//!
vector<UINT32> PmChannelDesc::GetMatchingSubchannels
(
    const PmChannel *pChannel,
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent
) const
{
    vector<UINT32> subchannels;
    for (UINT32 subch = 0; subch < Channel::NumSubchannels; ++subch)
    {
        string subchannelName = pChannel->GetSubchannelName(subch);
        if (subchannelName != "" && m_Name.Matches(subchannelName))
        {
            subchannels.push_back(subch);
        }
    }
    return subchannels;
}

//--------------------------------------------------------------------
//! Private function that checks whether a channel matches everything
//! in this channelDesc except m_Faulting (which requires extra work).
//!
bool PmChannelDesc::SimpleMatch(const PmChannel *pChannel) const
{
    MASSERT(pChannel != NULL);

    if (!m_Name.Matches(pChannel->GetName()))
    {
        bool subchMatch = false;
        for (UINT32 subch = 0; subch < Channel::NumSubchannels; ++subch)
        {
            if (m_Name.Matches(pChannel->GetSubchannelName(subch)))
            {
                subchMatch = true;
                break;
            }
        }
        if (!subchMatch)
        {
            return false;
        }
    }

    if (!m_ChannelNumbers.empty())
    {
        auto iter = find(m_ChannelNumbers.begin(), m_ChannelNumbers.end(),
                         pChannel->GetChannelNumber());
        if (iter == m_ChannelNumbers.end())
        {
            return false;
        }
    }

    if (m_pGpuDesc && !m_pGpuDesc->SimpleMatch(pChannel->GetGpuDevice()))
    {
        return false;
    }

    if (m_pTestDesc && !m_pTestDesc->SimpleMatch(pChannel->GetTest()))
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// PmGpuDesc
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmGpuDesc::PmGpuDesc
(
    const string &id
) :
    m_Id(id),
    m_Faulting(false)
{
}

//--------------------------------------------------------------------
//! \brief Return a short descriptive string, for debugging
//!
string PmGpuDesc::ToString() const
{
    string str;

    if (m_Id != "")
    {
        str = m_Id;
    }
    else if (m_Faulting)
    {
        str = "Gpu.Fauting";
    }
    else
    {
        str = m_DevInst.ToString() == "" ? ".*" : m_DevInst.ToString();
        str += ":";
        str += m_SubdevInst.ToString() == "" ? ".*" : m_SubdevInst.ToString();
    }
    return str;
}

//--------------------------------------------------------------------
//! \brief Set regular expression that the device instance must match
//!
RC PmGpuDesc::SetDevInst(const string &str)
{
    return m_DevInst.Set(str, RegExp::FULL);
}

//--------------------------------------------------------------------
//! \brief Set regular expression that the subdevice instance must match
//!
RC PmGpuDesc::SetSubdevInst(const string &str)
{
    return m_SubdevInst.Set(str, RegExp::FULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the gpuDesc is supported in a trigger.
//!
RC PmGpuDesc::IsSupportedInTrigger() const
{
    RC rc;
    if (m_Faulting)
    {
        ErrPrintf("Gpu.Faulting is not supported in triggers\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return OK if the gpuDesc is supported in an action.
//! \param pTrigger The trigger that the action is triggered by.
//!
RC PmGpuDesc::IsSupportedInAction(const PmTrigger *pTrigger) const
{
    MASSERT(pTrigger != NULL);
    RC rc;

    if (m_Faulting && !pTrigger->HasGpuSubdevice() &&
        !pTrigger->HasGpuDevice())
    {
        ErrPrintf("Used Gpu.Faulting in a trigger with no gpu\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return true if the gpuDesc matches the event.  Used by
//! triggers.
//!
bool PmGpuDesc::Matches(const PmEvent *pEvent) const
{
    GpuSubdevice *pGpuSubdevice = pEvent->GetGpuSubdevice();
    if (pGpuSubdevice != NULL)
    {
        return SimpleMatch(pGpuSubdevice);
    }

    GpuDevice *pGpuDevice = pEvent->GetGpuDevice();
    if (pGpuDevice != NULL)
    {
        return SimpleMatch(pGpuDevice);
    }

    // If we get this far, the event cannot say which [sub]device it
    // oclwrred on.  Arguably, we should MASSERT, but let's just
    // return false for now.
    //
    return false;
}

//--------------------------------------------------------------------
//! \brief Return all GpuSubdevices that match the gpuDesc.  Used by
//! actions.
//!
GpuSubdevices PmGpuDesc::GetMatchingSubdevices
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);

    GpuSubdevices matchingSubdevices;

    if (m_Faulting)
    {
        GpuSubdevice *pGpuSubdevice;
        GpuDevice    *pGpuDevice;

        if ((pGpuSubdevice = pEvent->GetGpuSubdevice()) != NULL)
        {
            if (SimpleMatch(pGpuSubdevice))
            {
                matchingSubdevices.push_back(pGpuSubdevice);
            }
        }
        else if ((pGpuDevice = pEvent->GetGpuDevice()) != NULL)
        {
            for (UINT32 ii = 0; ii < pGpuDevice->GetNumSubdevices(); ++ii)
            {
                pGpuSubdevice = pGpuDevice->GetSubdevice(ii);
                if (SimpleMatch(pGpuSubdevice))
                {
                    matchingSubdevices.push_back(pGpuSubdevice);
                }
            }
        }
    }
    else
    {
        GpuSubdevices allSubdevices =
            pTrigger->GetPolicyManager()->GetGpuSubdevices();
        for (GpuSubdevices::iterator ppGpuSubdevice = allSubdevices.begin();
             ppGpuSubdevice != allSubdevices.end(); ++ppGpuSubdevice)
        {
            if (SimpleMatch(*ppGpuSubdevice))
            {
                matchingSubdevices.push_back(*ppGpuSubdevice);
            }
        }
    }

    return matchingSubdevices;
}

//--------------------------------------------------------------------
//! \brief Return all GpuSubdevices that match the gpuDesc.  Used by
//! triggers.
//!
GpuSubdevices PmGpuDesc::GetMatchingSubdevicesInTrigger
(
    const PolicyManager *pPolicyManager
) const
{
    GpuSubdevices matchingSubdevices;
    GpuSubdevices allSubdevices = pPolicyManager->GetGpuSubdevices();

    for (GpuSubdevices::iterator ppGpuSubdevice = allSubdevices.begin();
         ppGpuSubdevice != allSubdevices.end(); ++ppGpuSubdevice)
    {
        if (SimpleMatch(*ppGpuSubdevice))
        {
            matchingSubdevices.push_back(*ppGpuSubdevice);
        }
    }
    return matchingSubdevices;
}

//--------------------------------------------------------------------
//! \brief Return all GpuDevices that match the gpuDesc.  Used by
//! actions.
//!
GpuDevices PmGpuDesc::GetMatchingDevices
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);

    GpuDevices matchingDevices;

    if (m_Faulting)
    {
        GpuDevice *pGpuDevice = pEvent->GetGpuDevice();
        if (pGpuDevice != NULL && SimpleMatch(pGpuDevice))
        {
            matchingDevices.push_back(pGpuDevice);
        }
    }
    else
    {
        GpuDevices allDevices = pTrigger->GetPolicyManager()->GetGpuDevices();
        for (GpuDevices::iterator ppGpuDevice = allDevices.begin();
             ppGpuDevice != allDevices.end(); ++ppGpuDevice)
        {
            if (SimpleMatch(*ppGpuDevice))
            {
                matchingDevices.push_back(*ppGpuDevice);
            }
        }
    }

    return matchingDevices;
}

//--------------------------------------------------------------------
//! \brief Return all GpuDevices that match the gpuDesc.  Used by
//! triggers.
//!
GpuDevices PmGpuDesc::GetMatchingDevicesInTrigger
(
    const PolicyManager *pPolicyManager
) const
{
    GpuDevices matchingDevices;
    GpuDevices allDevices = pPolicyManager->GetGpuDevices();

    for (GpuDevices::iterator ppGpuDevice = allDevices.begin();
         ppGpuDevice != allDevices.end(); ++ppGpuDevice)
    {
        if (SimpleMatch(*ppGpuDevice))
        {
            matchingDevices.push_back(*ppGpuDevice);
        }
    }
    return matchingDevices;
}

//--------------------------------------------------------------------
//! Private function that checks whether a GpuSubdevice matches
//! everything in this gpuDesc except m_Faulting (which requires extra
//! work).
//!
bool PmGpuDesc::SimpleMatch(const GpuSubdevice *pGpuSubdevice) const
{
    MASSERT(pGpuSubdevice != NULL);
    char devInst[10];
    char subdevInst[10];

    sprintf(devInst, "%d", pGpuSubdevice->GetParentDevice()->GetDeviceInst());
    sprintf(subdevInst, "%d", pGpuSubdevice->GetSubdeviceInst());
    return m_DevInst.Matches(devInst) && m_SubdevInst.Matches(subdevInst);
}

//--------------------------------------------------------------------
//! Private function that checks whether a GpuDevice matches
//! everything in this gpuDesc except m_Faulting (which requires extra
//! work).
//!
bool PmGpuDesc::SimpleMatch(const GpuDevice *pGpuDevice) const
{
    MASSERT(pGpuDevice != NULL);

    // Check m_DevInst.  Fail if no match.
    //
    char devInst[10];
    sprintf(devInst, "%d", pGpuDevice->GetDeviceInst());
    if (!m_DevInst.Matches(devInst))
        return false;

    // Check m_SubdevInst against all subdevices.  Succeed if any
    // match, fail if none match.
    //
    char subdevInst[10];
    for (UINT32 ii = 0; ii < pGpuDevice->GetNumSubdevices(); ++ii)
    {
        sprintf(subdevInst, "%d",
                pGpuDevice->GetSubdevice(ii)->GetSubdeviceInst());
        if (m_SubdevInst.Matches(subdevInst))
        {
            return true;
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////
// PmRunlistDesc
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmRunlistDesc::PmRunlistDesc
(
    const string &id
) :
    m_Id(id),
    m_pChannelDesc(NULL),
    m_pGpuDesc(NULL)
{
}

//--------------------------------------------------------------------
//! \brief Set regular expression that the runlist name must match
//!
//! The runlist name is the same as the engine name.  The name is
//! case-insensitive.
//!
RC PmRunlistDesc::SetName(const string &str)
{
    return m_Name.Set(str, RegExp::FULL | RegExp::ICASE);
}

//--------------------------------------------------------------------
//! \brief Set the channel(s) that are (or will be) submitted to the runlist
//!
//! Implementation note: Mods figures out which runlist to submit the
//! channel to by snooping SetObject.  So matching runlists based on
//! the associated channel doesn't work well before SetObject is
//! called on the channel(s).
//!
RC PmRunlistDesc::SetChannelDesc(const PmChannelDesc *pChannelDesc)
{
    m_pChannelDesc = pChannelDesc;
    return OK;
}

//--------------------------------------------------------------------
//! \brief Set the gpu device(s) that the runlist(s) are on
//!
RC PmRunlistDesc::SetGpuDesc(const PmGpuDesc *pGpuDesc)
{
    m_pGpuDesc = pGpuDesc;
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return OK if the runlistDesc is supported in an action.
//! \param pTrigger The trigger that the action is triggered by.
//!
RC PmRunlistDesc::IsSupportedInAction(const PmTrigger *pTrigger) const
{
    MASSERT(pTrigger != NULL);
    RC rc;

    if ((m_pChannelDesc && (m_pChannelDesc->IsSupportedInAction(pTrigger) != OK)) ||
        (m_pGpuDesc && (m_pGpuDesc->IsSupportedInAction(pTrigger) != OK)))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get all Runlists that match the runlistDesc.  Used by
//! actions.
//!
RC PmRunlistDesc::GetMatchingRunlists
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent,
    Runlists *pRunlists
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);
    MASSERT(pRunlists != NULL);
    RC rc;

    // Sanity check: make sure runlists are supported.  (This code
    // can't be in IsSupportedInAction because that function is called
    // at parse-time, which can occur before the command lines are
    // fully processed.  So the -runlist arg may not have finished.)
    //
    if (!ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        ErrPrintf("Used Runlist in PolicyManager in non-runlist mode\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    pRunlists->clear();

    // Get all matching devices
    //
    GpuDevices MatchingDevices;

    if (m_pGpuDesc)
        MatchingDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);
    else
        MatchingDevices = pTrigger->GetPolicyManager()->GetGpuDevices();

    // Get the matching runlists
    //
    // We use two different algorithms, depending on whether
    // m_pChannelDesc is set or not.  In the first case, we loop
    // through the channels to check the runlist for each channel.  In
    // the second case, we loop throough the devices to check all
    // runlists on each device.
    //
    if (m_pChannelDesc)
    {
        set<Runlist*> RunlistSet;  // Prevents us from adding same
                                   // runlist twice to *pRunlists
        PmChannels Channels =
            m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

        for (PmChannels::iterator ppChannel = Channels.begin();
             ppChannel != Channels.end(); ++ppChannel)
        {
            RunlistChannelWrapper *pRunlistWrapper =
                (*ppChannel)->GetModsChannel()->GetRunlistChannelWrapper();
            if (pRunlistWrapper != NULL)
            {
                Runlist *pRunlist;
                CHECK_RC(pRunlistWrapper->GetRunlist(&pRunlist));
                if (RunlistSet.count(pRunlist) == 0 &&
                    SimpleMatch(MatchingDevices, pRunlist->GetGpuDevice(),
                                pRunlist->GetEngine()))
                {
                    RunlistSet.insert(pRunlist);
                    pRunlists->push_back(pRunlist);
                }
            }
        }
    }
    else // m_pChannelDesc == NULL
    {
        // Loop through all matching devices
        //
        for (GpuDevices::iterator ppGpuDevice = MatchingDevices.begin();
             ppGpuDevice != MatchingDevices.end(); ++ppGpuDevice)
        {
            vector<UINT32> Engines;

            // Loop through the engines on the device to get the runlists
            //
            CHECK_RC((*ppGpuDevice)->GetEngines(&Engines));
            for (vector<UINT32>::iterator pEngine = Engines.begin();
                 pEngine != Engines.end(); ++pEngine)
            {
                if (SimpleMatch(MatchingDevices, *ppGpuDevice, *pEngine))
                {
                    Runlist *pRunlist;
                    for (UINT32 i = 0;
                         i < LwRmPtr::GetValidClientCount(); i++)
                    {
                        CHECK_RC((*ppGpuDevice)->GetRunlist(*pEngine,
                                                            LwRmPtr(i).Get(),
                                                            true,
                                                            &pRunlist));
                        pRunlists->push_back(pRunlist);
                    }
                }
            }
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get all <GpuDevice*, LwRm*, EngineId> tuples for the
//! channels in runlistDesc. Used by actions.
//!
RC PmRunlistDesc::GetChannelsEngines
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent,
    vector< tuple<GpuDevice*, LwRm*, UINT32> > *pGpuDevicesEngines
) const
{
    RC rc;

    MASSERT(m_pChannelDesc);

    PmChannels channels = m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    // Map of GpuDevice : Set of partiton EngineIds (pair of LwRm+EngineId)
    map<GpuDevice*, set<pair<LwRm*, UINT32>>> devicesEnginesMap;
    for (auto const & channel : channels)
    {
        GpuDevice* pGpuDev = channel->GetGpuDevice();
        // EngineIds in channel is supported only from Ampere onwards
        if (!(pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID)))
        {
            continue;
        }
        LwRm* pLwRm = channel->GetLwRmPtr();
        UINT32 engineId = channel->GetEngineId();

        pair<LwRm*, UINT32> partitionEngine(pLwRm, engineId);

        // PartitonEngine for the GpuDevice does not exist in the map
        // then add to the map and the returning vector as well
        if (devicesEnginesMap[pGpuDev].count(partitionEngine) == 0)
        {
            devicesEnginesMap[pGpuDev].insert(partitionEngine);
            pGpuDevicesEngines->push_back(make_tuple(pGpuDev, pLwRm, engineId));
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get all <GpuDevice,Engine> pairs that match the
//! runlistDesc. Used by actions.
//!
RC PmRunlistDesc::GetMatchingEngines
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent,
    vector< pair<GpuDevice*, UINT32> > *pGpuDevicesEngines,
    LwRm* pLwRm,
    PmSmcEngines smcEngines
) const
{
    RC rc;

    GpuDevices MatchingDevices;
    if (m_pGpuDesc)
        MatchingDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);
    else
        MatchingDevices = pTrigger->GetPolicyManager()->GetGpuDevices();

    for (GpuDevices::iterator ppGpuDevice = MatchingDevices.begin();
         ppGpuDevice != MatchingDevices.end(); ++ppGpuDevice)
    {
        vector<UINT32> Engines;
        CHECK_RC((*ppGpuDevice)->GetEngines(&Engines, pLwRm));
        for (vector<UINT32>::iterator pEngine = Engines.begin();
             pEngine != Engines.end(); ++pEngine)
        {
            string engineName = (*ppGpuDevice)->GetEngineName(*pEngine);

            // This special handling is there for GR engines since:
            // For any of the Graphics (SMC) engines, user will only specify "GRAPHICS" not "GRAPHICS1" etc.
            // For SMC mode user will also use Policy.SetSmcEngine
            // User could also specify Policy.SetSmcEngine(SmcEngine.Test(Test.All())
            if (MDiagUtils::IsGraphicsEngine(*pEngine))
            {
                // For all GR engines use GR0's name ("GRAPHICS"),
                // Since user always specifies "GRAPHICS" only in the policy file
                // and uses Policy.SetSmcEngine in combination for SMC engines
                engineName = (*ppGpuDevice)->GetEngineName(LW2080_ENGINE_TYPE_GR0);
            }

            if (*pEngine != LW2080_ENGINE_TYPE_SW &&
                        m_Name.Matches(engineName))
            {
                pair<GpuDevice*, UINT32> DeviceEngine(*ppGpuDevice, *pEngine);
                pGpuDevicesEngines->push_back(DeviceEngine);
            }
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! Return true if an (device,engine) tuple is a matching runlist.
//!
//! \param MatchingDevices All devices that matched m_pGpuDesc.  This
//!     arg is assumed to be sorted by CmpGpuDevicesByInst, since
//!     PolicyManager::GetGpuDevices and PmGpuDesc::GetMatchingDevices
//!     both return the devices in that order.
//! \param pGpuDevice The device the runlist is on
//! \param Engine The engine the runlist is for.
//!
bool PmRunlistDesc::SimpleMatch
(
    const GpuDevices &MatchingDevices,
    const GpuDevice *pGpuDevice,
    UINT32 Engine
) const
{
    static CmpGpuDevicesByInst Cmp;

    // Return false if pGpuDevice isn't one of the MatchingDevices
    //
    if (!binary_search(MatchingDevices.begin(), MatchingDevices.end(),
                       pGpuDevice, Cmp))
    {
        return false;
    }

    // Return true if the engine name matches m_Names, and it's not
    // the SW engine (which doesn't support runlists).
    //
    return (Engine != LW2080_ENGINE_TYPE_SW &&
            m_Name.Matches(pGpuDevice->GetEngineName(Engine)));
}

//////////////////////////////////////////////////////////////////////
// PmTestDesc
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTestDesc::PmTestDesc
(
    const string &id
) :
    m_Id(id),
    m_pChannelDesc(NULL),
    m_pGpuDesc(NULL),
    m_Faulting(false),
    m_TestId(m_UnknonwTestId)
{
}

//--------------------------------------------------------------------
//! \brief Set the channel(s) that are part of the test(s)
//!
RC PmTestDesc::SetChannelDesc(const PmChannelDesc *pChannelDesc)
{
    RC rc;

    if (pChannelDesc->GetFaulting())
    {
        ErrPrintf("Channel.Faulting is not supported inside Test\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_pChannelDesc = pChannelDesc;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Set the gpu device(s) that the test(s) run on
//!
RC PmTestDesc::SetGpuDesc(const PmGpuDesc *pGpuDesc)
{
    RC rc;

    if (pGpuDesc->GetFaulting())
    {
        ErrPrintf("Gpu.Faulting is not supported inside Test\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_pGpuDesc = pGpuDesc;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return OK if the testDesc is supported in an action.
//! \param pTrigger The trigger that the action is triggered by.
//!
RC PmTestDesc::IsSupportedInAction(const PmTrigger *pTrigger) const
{
    MASSERT(pTrigger != NULL);
    RC rc;

    if ((m_pChannelDesc && (m_pChannelDesc->IsSupportedInAction(pTrigger) != OK)) ||
        (m_pGpuDesc && (m_pGpuDesc->IsSupportedInAction(pTrigger) != OK)) ||
        (m_Faulting && !pTrigger->HasChannel()))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get all active tests that match the testDesc.  Used by
//! actions.
//!
PmTests PmTestDesc::GetMatchingTests
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent,
    const PmSmcEngineDesc *pSmcEngineDesc
) const
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);
    PmTests candidateTests;
    PmTests matchingTests;

    // Start with a list of active tests
    //
    candidateTests = pTrigger->GetPolicyManager()->GetActiveTests();

    // PmEvent::GetTest() has already implemented how to get test from
    // channel and surfaces. So here just ask PmEvent to return test.
    if (m_Faulting)
    {
        PmTest* pPmTest = pEvent->GetTest();
        if (pPmTest)
        {
            candidateTests.clear();
            candidateTests.insert(pPmTest);
        }
    }

    for (auto pTest : candidateTests)
    {
        if (SimpleMatch(pTest, pSmcEngineDesc))
        {
            matchingTests.insert(pTest);
        }
    }

    return matchingTests;
}

//--------------------------------------------------------------------
//! Private function that checks whether a test matches everything
//! in this PmTestDesc except m_Faulting (which requires extra work).
//!
bool PmTestDesc::SimpleMatch
(
    PmTest* pTest,
    const PmSmcEngineDesc* pSmcEngineDesc
) const
{
    // an empty regex "" matches anything
    if (!m_Name.Matches(pTest->GetName()) || !m_Type.Matches(pTest->GetTypeName()))
    {
        return false;
    }

    // Filter out any tests that don't match m_pChannelDesc
    //
    if (m_pChannelDesc)
    {
        bool channelMatching = false;

        for (auto pChannel : pTest->GetPolicyManager()->GetActiveChannels())
        {
            if ((pChannel->GetTest() == pTest) && m_pChannelDesc->SimpleMatch(pChannel))
            {
                channelMatching = true;
                break;
            }
        }

        if (!channelMatching)
        {
            return false;
        }
    }

    // Filter out any tests that don't match m_pGpuDesc
    //
    if (m_pGpuDesc && !m_pGpuDesc->SimpleMatch(pTest->GetGpuDevice()))
    {
        return false;
    }

    // Filter out any tests that don't match m_TestId
    if ((m_TestId != m_UnknonwTestId) && (m_TestId != pTest->GetTestId()))
    {
        return false;
    }

    if (pSmcEngineDesc && !pSmcEngineDesc->SimpleMatch(pTest->GetSmcEngine()))
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// PmNonStallInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief PmNonStallInt constructor
//!
PmNonStallInt::PmNonStallInt
(
    const string &intName,
    PmChannel    *pChannel
) :
    m_IntName(intName),
    m_pChannel(pChannel),
    m_IsAllocated(false),
    m_Semaphore(),
    m_LastReceivedValue(0),
    m_LastRequestedValue(0)
{
    MASSERT(pChannel != NULL);
}

//--------------------------------------------------------------------
//! \brief PmNonStallInt destructor
//!
PmNonStallInt::~PmNonStallInt()
{
    PmNonStallInt *pThis = this;
    m_pChannel->GetPolicyManager()->GetEventManager()->UnhookResmanEvent(
        PmNonStallInt::HandleResmanEvent, &pThis, sizeof(pThis));
}

//--------------------------------------------------------------------
//! \brief Request a non-stalling interrupt from the channel
//!
RC PmNonStallInt::PrepareInterruptRequest
(
    UINT64 *pSemaphoreOffset,
    UINT64 *pPayload
)
{
    MASSERT(pSemaphoreOffset != NULL);
    MASSERT(pPayload != NULL);
    RC rc;

    // Allocate the semaphore, if needed
    //
    if (!m_IsAllocated)
    {
        m_Semaphore.SetWidth(1);
        m_Semaphore.SetHeight(1);
        m_Semaphore.SetColorFormat(ColorUtils::VOID32);
        m_Semaphore.SetLocation(Memory::Coherent);
        m_Semaphore.SetGpuVASpace(m_pChannel->GetVaSpaceHandle());
        CHECK_RC(m_Semaphore.Alloc(m_pChannel->GetGpuDevice(), m_pChannel->GetLwRmPtr()));
        CHECK_RC(m_Semaphore.BindGpuChannel(m_pChannel->GetHandle()));
        CHECK_RC(m_Semaphore.Map(Gpu::UNSPECIFIED_SUBDEV, m_pChannel->GetLwRmPtr()));
        Platform::VirtualWr32(m_Semaphore.GetAddress(), 0);

        m_IsAllocated = true;
    }

    // Send the potential event to the gilder
    //
    PmGilder *pGilder = m_pChannel->GetPolicyManager()->GetGilder();
    pGilder->StartPotentialEvent(new PmPotential_NonStallInt(this));

    // Return the requested offset/payload
    //
    ++m_LastRequestedValue;

    *pSemaphoreOffset = m_Semaphore.GetCtxDmaOffsetGpu();
    *pPayload = m_LastRequestedValue;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Check whether the int has oclwrred; launch PmEvent(s) if so.
//!
RC PmNonStallInt::HandleResmanEvent(void *ppThis)
{
    RC rc;
    PmNonStallInt *pThis = *static_cast<PmNonStallInt**>(ppThis);
    PmEventManager *pEventManager =
        pThis->m_pChannel->GetPolicyManager()->GetEventManager();

    while (pThis->m_LastReceivedValue !=
           Platform::VirtualRd32(pThis->m_Semaphore.GetAddress()))
    {
        PmEvent_NonStallInt event(pThis);
        CHECK_RC(pEventManager->HandleEvent(&event));
        ++pThis->m_LastReceivedValue;
    }

    if (pThis->m_LastReceivedValue == pThis->m_LastRequestedValue)
    {
        CHECK_RC(pEventManager->UnhookResmanEvent(
                     PmNonStallInt::HandleResmanEvent,
                     &pThis, sizeof(pThis)));
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return the non-stall int data in string form.  Used for gilding.
//!
string PmNonStallInt::ToString() const
{
    return m_IntName + " " + m_pChannel->GetName();
}

//////////////////////////////////////////////////////////////////////
// PmMethodInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief PmMethodInt constructor
//!
PmMethodInt::PmMethodInt
(
    PmChannel    *pChannel
) :
    m_pChannel(pChannel),
    m_IsAllocated(false),
    m_Semaphore(),
    m_LastInt((UINT32)~0)
{
    MASSERT(pChannel != NULL);
}

//--------------------------------------------------------------------
//! \brief PmMethodInt destructor
//!
PmMethodInt::~PmMethodInt()
{
    PmMethodInt *pThis = this;
    m_pChannel->GetPolicyManager()->GetEventManager()->UnhookMemoryEvent(
        PmMethodInt::RespondToSemaphore, &pThis, sizeof(pThis));
}

//--------------------------------------------------------------------
//! \brief Request a method interrupt from the channel
//!
RC PmMethodInt::RequestInterrupt(UINT32 methodCount,
                                 bool bWfiOnRelease,
                                 bool bWaitEventHandled)
{
    RC rc;

    // Only request an interrupt at any particular method once
    if (m_LastInt == methodCount)
        return OK;

    // Allocate the semaphore, if needed
    //
    if (!m_IsAllocated)
    {
        LwRm::Handle hVaSpace = m_pChannel->GetVaSpaceHandle();

        m_Semaphore.SetWidth(16);
        m_Semaphore.SetHeight(1);
        m_Semaphore.SetColorFormat(ColorUtils::VOID32);
        m_Semaphore.SetLocation(Memory::Coherent);

        m_Semaphore.SetGpuVASpace(hVaSpace);
        CHECK_RC(m_Semaphore.Alloc(m_pChannel->GetGpuDevice(), m_pChannel->GetLwRmPtr()));
        CHECK_RC(m_Semaphore.BindGpuChannel(m_pChannel->GetHandle()));
        CHECK_RC(m_Semaphore.Map(Gpu::UNSPECIFIED_SUBDEV, m_pChannel->GetLwRmPtr()));
        Platform::VirtualWr32(m_Semaphore.GetAddress(), METHOD_INT_COMPLETE);

        m_IsAllocated = true;
    }

    // Write the semaphore-release and the current method count to the channel
    PM_BEGIN_INSERTED_METHODS(m_pChannel);

    UINT32 flags = m_pChannel->GetSemaphoreReleaseFlags();
    if (bWfiOnRelease)
    {
        flags |= Channel::FlagSemRelWithWFI;
    }
    else
    {
        flags &= ~Channel::FlagSemRelWithWFI;
    }
    m_pChannel->SetSemaphoreReleaseFlags(flags);

    if (m_pChannel->GetType() != PmChannel::PIO)
        CHECK_RC(m_pChannel->WriteSetSubdevice(Channel::AllSubdevicesMask));
    CHECK_RC(m_pChannel->SetContextDmaSemaphore(m_Semaphore.GetCtxDmaHandleGpu()));
    CHECK_RC(m_pChannel->SetSemaphoreOffset(m_Semaphore.GetCtxDmaOffsetGpu()));
    CHECK_RC(m_pChannel->SemaphoreRelease(methodCount));

    if (bWaitEventHandled)
    {
        CHECK_RC(m_pChannel->SemaphoreAcquire(METHOD_INT_COMPLETE));
    }

    m_RespondIntList.push_back(methodCount);

    // Call RespondToSemaphore when the semaphore changes
    PmEventManager *pEventManager =
        m_pChannel->GetPolicyManager()->GetEventManager();
    PmMethodInt *pThis = this;
    CHECK_RC(pEventManager->HookMemoryEvent(
                 (UINT32*)m_Semaphore.GetAddress(), METHOD_INT_COMPLETE,
                 PmMethodInt::RespondToSemaphore, &pThis, sizeof(pThis)));

    // Save the method count for the interrupt so only one interrupt is
    // requested at any particular method (since a single interrupt will
    // cause all associated actions to fire)
    m_LastInt = methodCount;

    PM_END_INSERTED_METHODS();
    return rc;
}

//--------------------------------------------------------------------
//! \brief Check whether the int has oclwrred; launch PmEvent(s) if so.
//!
RC PmMethodInt::RespondToSemaphore(void *ppThis)
{
    PmMethodInt *pThis = *static_cast<PmMethodInt**>(ppThis);
    RC rc;

    // Flag the channel as stalled to avoid doing any channel operations
    // which may poll for methods to be processed
    PmChannelWrapper *pPmWrap =
        pThis->m_pChannel->GetModsChannel()->GetPmChannelWrapper();
    PmChannelWrapper::ChannelStalledHolder stallChannel(pPmWrap);
    PmEventManager *pEventManager =
        pThis->m_pChannel->GetPolicyManager()->GetEventManager();

    UINT32 methodCount = Platform::VirtualRd32(pThis->m_Semaphore.GetAddress());
    DebugPrintf("%s: method count read from semaphore surface = %d\n",
                __FUNCTION__, methodCount);

    vector<UINT32>::iterator it = pThis->m_RespondIntList.begin();
    while (it != pThis->m_RespondIntList.end())
    {
        // send events which have happened before the methodCount
        //
        if (*it <= methodCount)
        {
            // In case bWaitEventHandled = false in RequestInterrupt(),
            // host will not stop after releasing methodCount. So it's
            // possible that there are more than one event not handled
            // when RespondToSemaphore() is called.
            DebugPrintf("%s: Sending unhandled event: method count in PmEvent_MethodExelwte = %d\n",
                        __FUNCTION__, *it);

            PmEvent_MethodExelwte event(pThis->m_pChannel, *it);
            CHECK_RC(pEventManager->HandleEvent(&event));

            it = pThis->m_RespondIntList.erase(it);
        }
        else
        {
            ++ it;
        }
    }

    Platform::VirtualWr32(pThis->m_Semaphore.GetAddress(), METHOD_INT_COMPLETE);
    CHECK_RC(pEventManager->HookMemoryEvent(
                 (UINT32*)pThis->m_Semaphore.GetAddress(), METHOD_INT_COMPLETE,
                 PmMethodInt::RespondToSemaphore, &pThis, sizeof(pThis)));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return the non-stall int data in string form.  Used for gilding.
//!
string PmMethodInt::ToString() const
{
    return "PmMethodInt : " + m_pChannel->GetName();
}

//////////////////////////////////////////////////////////////////////
// PmContextSwitchInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief PmContextSwitchInt constructor
//!
PmContextSwitchInt::PmContextSwitchInt
(
    PmChannel* pChannel,
    bool blocking
) :
    m_pChannel(pChannel),
    m_IsAllocated(false),
    m_Semaphore(),
    m_IntCount(1),
    m_BlockingInt(blocking)
{
    MASSERT(m_pChannel != NULL);
}

//--------------------------------------------------------------------
//! \brief PmContextSwitch destructor
//!
PmContextSwitchInt::~PmContextSwitchInt()
{
    PmContextSwitchInt *pThis = this;
    PolicyManager::Instance()->GetEventManager()->UnhookMemoryEvent(
        PmContextSwitchInt::RespondToSemaphore, &pThis, sizeof(pThis));
}

//--------------------------------------------------------------------
//! \brief Request an interrupt from the channel
//!
RC PmContextSwitchInt::RequestInterrupt()
{
    RC rc;

    // Allocate the semaphore, if needed
    //
    if (!m_IsAllocated)
    {
        m_Semaphore.SetWidth(16);
        m_Semaphore.SetHeight(1);
        m_Semaphore.SetColorFormat(ColorUtils::VOID32);
        m_Semaphore.SetLocation(Memory::Fb);
        m_Semaphore.SetGpuVASpace(m_pChannel->GetVaSpaceHandle());

        CHECK_RC(m_Semaphore.Alloc(m_pChannel->GetGpuDevice(), m_pChannel->GetLwRmPtr()));
        CHECK_RC(m_Semaphore.BindGpuChannel(m_pChannel->GetHandle()));
        CHECK_RC(m_Semaphore.Map(Gpu::UNSPECIFIED_SUBDEV, m_pChannel->GetLwRmPtr()));
        Platform::VirtualWr32(m_Semaphore.GetAddress(), 0);

        m_IsAllocated = true;
    }

    // Write the semaphore-release and the current method count to the channel
    PM_BEGIN_INSERTED_METHODS(m_pChannel);

    if (m_pChannel->GetType() != PmChannel::PIO)
        CHECK_RC(m_pChannel->WriteSetSubdevice(Channel::AllSubdevicesMask));
    CHECK_RC(m_pChannel->SetContextDmaSemaphore(m_Semaphore.GetCtxDmaHandleGpu()));
    CHECK_RC(m_pChannel->SetSemaphoreOffset(m_Semaphore.GetCtxDmaOffsetGpu()));
    CHECK_RC(m_pChannel->SemaphoreRelease(m_IntCount));
    if (m_BlockingInt)
        CHECK_RC(m_pChannel->SemaphoreAcquire(CTXSW_INT_COMPLETE));

    // Call RespondToSemaphore when the semaphore changes
    PmEventManager *pEventManager =
        m_pChannel->GetPolicyManager()->GetEventManager();
    PmContextSwitchInt *pThis = this;
    CHECK_RC(pEventManager->HookMemoryEvent(
                 (UINT32*)m_Semaphore.GetAddress(), CTXSW_INT_COMPLETE,
                 PmContextSwitchInt::RespondToSemaphore,
                 &pThis, sizeof(pThis)));

    m_IntCount++;

    PM_END_INSERTED_METHODS();
    return rc;
}

//--------------------------------------------------------------------
//! \brief Check whether the int has oclwrred; launch PmEvent(s) if so.
//!
RC PmContextSwitchInt::RespondToSemaphore(void *ppThis)
{
    PmContextSwitchInt *pThis = *static_cast<PmContextSwitchInt**>(ppThis);
    RC rc;

    // For blocking interrupts, flag the channel as stalled to avoid doing any
    // channel operations which may poll for methods to be processed
    PmChannelWrapper *pPmWrap =
        pThis->m_pChannel->GetModsChannel()->GetPmChannelWrapper();
    PmChannelWrapper::ChannelStalledHolder stallChannel(pThis->m_BlockingInt
                                                        ? pPmWrap : NULL);

    PmEventManager *pEventManager =
        pThis->m_pChannel->GetPolicyManager()->GetEventManager();
    Platform::VirtualWr32(pThis->m_Semaphore.GetAddress(), CTXSW_INT_COMPLETE);
    CHECK_RC(pEventManager->HookMemoryEvent(
                 (UINT32*)pThis->m_Semaphore.GetAddress(), CTXSW_INT_COMPLETE,
                 PmContextSwitchInt::RespondToSemaphore,
                 &pThis, sizeof(pThis)));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return the int data in string form.  Used for gilding.
//!
string PmContextSwitchInt::ToString() const
{
    return "PmContextSwitchInt : " + m_pChannel->GetName();
}

//////////////////////////////////////////////////////////////////////
// PmReplayableInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief PmReplayableInt constructor
//!
PmReplayableInt::PmReplayableInt
(
    PolicyManager *policyManager,
    GpuSubdevice *gpuSubdevice
) :
    m_PolicyManager(policyManager),
    m_GpuSubdevice(gpuSubdevice),
    m_FaultBufferHandle(0),
    m_FaultBufferEntries(0),
    m_MaxFaultBufferEntries(0),
    m_GetIndex(0),
    m_PutIndex(0)
{
    MASSERT(policyManager != 0);
    MASSERT(gpuSubdevice != 0);
}

//--------------------------------------------------------------------
//! \brief PmReplayableInt destructor
//!
PmReplayableInt::~PmReplayableInt()
{
    LwRmPtr pLwRm;

    if (m_FaultBufferHandle != 0)
    {
        PmEventManager *eventManager = m_PolicyManager->GetEventManager();
        PmReplayableInt *pThis = this;

        eventManager->UnhookResmanEvent(
            PmReplayableInt::HandleResmanEvent,
            &pThis,
            sizeof(pThis));

        if (m_FaultBufferEntries != 0)
        {
            pLwRm->UnmapMemory(
                m_FaultBufferHandle,
                m_FaultBufferEntries,
                0,
                m_GpuSubdevice);
        }
    }
}

//--------------------------------------------------------------------
//! \brief Allocate a fault buffer object
//!
//! A fault buffer object is used to access the fault event buffer data.
//!
RC PmReplayableInt::PrepareForInterrupt()
{
    RC rc;
    LwRm* pLwRm = LwRmPtr().Get();

    // Allocate the fault buffer object.
    SubdeviceFaultBuffer *pFaultBuffer = SubdeviceFaultBuffer::GetFaultBuffer(m_GpuSubdevice);
    MASSERT(pFaultBuffer);
    m_FaultBufferHandle = pFaultBuffer->GetFaultBufferHandle();

    // Get the size of the fault buffer.
    CHECK_RC(pFaultBuffer->GetFaultBufferSize(&m_FaultBufferSize));

    const UINT32 faultBufferSize = FaultBuffer::GetSize();
    m_MaxFaultBufferEntries = m_FaultBufferSize / faultBufferSize;

    // Map the fault buffer to a pointer so it can be accessed.
    CHECK_RC(pFaultBuffer->MapFaultBuffer(0, m_FaultBufferSize, (void**)&m_FaultBufferEntries, 0, m_GpuSubdevice));

    // Read the fault buffer get pointer and save it in our internal copy.
    CHECK_RC(pFaultBuffer->GetFaultBufferGetIndex(&m_GetIndex));

    // Pass a callback function to RM for when a replayable fault happens.
    PmEventManager *eventManager = m_PolicyManager->GetEventManager();
    PmReplayableInt *pThis = this;

    CHECK_RC(eventManager->HookResmanEvent(
                 m_FaultBufferHandle,
                 FaultBuffer::GetNotifier(),
                 PmReplayableInt::HandleResmanEvent,
                 &pThis,
                 sizeof(pThis),
                 pLwRm));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Callback function for when a replayable fault happens.
//!
RC PmReplayableInt::HandleResmanEvent(void *ppThis)
{
    RC rc;
    if (MDiagUtils::IsFaultIndexRegisteredInUtl(FaultBuffer::GetNotifier()))
    {
        WarnPrintf("You cannot register handlers for the same fault in both UTL and PM.\n");
        return rc;
    }

    PmReplayableInt *pThis = *static_cast<PmReplayableInt**>(ppThis);
    LwRmPtr pLwRm;

    SubdeviceFaultBuffer *pFaultBuffer = SubdeviceFaultBuffer::GetFaultBuffer(pThis->m_GpuSubdevice);
    CHECK_RC(pFaultBuffer->GetFaultBufferPutIndex(&pThis->m_PutIndex));

    // If the fault buffer has overflowed, skip over any existing
    // fault buffer entries without processing them.  Then create
    // an overflow event so that the policy file can respond
    // to the overflow.
    if (pThis->m_GpuSubdevice->FaultBufferOverflowed())
    {
        DebugPrintf("fault buffer overflow detected");

        CHECK_RC(pThis->CreateFaultBufferOverflowEvent());

        pThis->m_GetIndex = pThis->m_PutIndex;
        CHECK_RC(pFaultBuffer->UpdateFaultBufferGetIndex(pThis->m_PutIndex));
    }
    else if (pThis->m_GetIndex != pThis->m_PutIndex)
    {
        MASSERT(pThis->m_PutIndex < pThis->m_MaxFaultBufferEntries);

        while (pThis->m_GetIndex != pThis->m_PutIndex)
        {
            FaultBufferEntry::Pointer faultEntry = pThis->GetFaultEntry();

            // If the valid bit in the fault buffer entry is false, that
            // means that the buffer entry is not completely written yet
            // and no further fault buffer entries should be processed.
            bool valid = false;
            CHECK_RC(faultEntry->IsValid(&valid));
            if (!valid)
            {
                // Modify the put index so that when the get index is
                // updated below, it doesn't go past the non-valid entries.
                pThis->m_PutIndex = pThis->m_GetIndex;
                break;
            }

            // Otherwise create a replayable fault event
            // based on the entry.
            else
            {
                DebugPrintf("reading fault buffer entry %u\n", pThis->m_GetIndex);

                CHECK_RC(pThis->CreateReplayableFaultEvent(faultEntry.get()));
            }

            if (pThis->m_GetIndex == pThis->m_PutIndex)
            {
                // GetIndex must be updated elsewhere, like ClearFaultBuffer
                // Supposed that valid bit of each entries must be cleared correctly
                break;
            }

            CHECK_RC(pThis->IlwalidFaultEntry(faultEntry.get()));

            ++pThis->m_GetIndex;

            if (pThis->m_GetIndex >= pThis->m_MaxFaultBufferEntries)
            {
                pThis->m_GetIndex = 0;
            }
        }

        // UpdateFaultBufferGetPointer could be called twice,
        // if ClearFaultBuffer was ilwoked before. But it looks harmness
        if (pThis->m_PolicyManager->GetUpdateFaultBufferGetPointer())
        {
            CHECK_RC(pFaultBuffer->UpdateFaultBufferGetIndex(pThis->m_PutIndex));
        }
    }

    // Before RM calls this function, it will turn off replayable fault
    // notifications in order to prevent multiple callbacks running at the same
    // time.  We need to re-enable notifications when we're done.
    CHECK_RC(pFaultBuffer->EnableNotificationsForFaultBuffer());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Create a replayable fault event based on a fault buffer entry.
//!
RC PmReplayableInt::CreateReplayableFaultEvent(const FaultBufferEntry* const faultEntry)
{
    RC rc;

    UINT32 GPCID;
    UINT32 clientID;
    UINT32 faultType;
    UINT64 timestamp;

    FaultingInfo faultingInfo(faultEntry, m_PolicyManager, m_GpuSubdevice);
    PmChannels faultingChannels;
    CHECK_RC(faultingInfo.GetFaultingChannels(&faultingChannels));
    MASSERT(!faultingChannels.empty());

    UINT32 faultingVeid = 0;
    if (!faultingInfo.GetFaultingVeid(&faultingVeid))
    {
        faultingVeid = g_BadVeid;
        InfoPrintf("VEID is not available in this replayable fault\n");
    }

    FaultingRange faultingRange(faultEntry, m_PolicyManager, m_GpuSubdevice);
    PmMemRange faultingMemRange;
    CHECK_RC(faultingRange.GetMemRange(&faultingMemRange));

    CHECK_RC(faultEntry->GetGPCID(&GPCID));
    CHECK_RC(faultEntry->GetClientID(&clientID));
    CHECK_RC(faultEntry->GetType(&faultType));
    CHECK_RC(faultEntry->GetTimeStamp(&timestamp));

    PmEvent_ReplayableFault event(m_GpuSubdevice, this, faultingMemRange, GPCID, clientID,
                 faultType, timestamp, faultingVeid, faultingChannels);
    PmEventManager *pEventManager = m_PolicyManager->GetEventManager();
    CHECK_RC(pEventManager->HandleEvent(&event));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Create a fault buffer overflow  event.
//!
RC PmReplayableInt::CreateFaultBufferOverflowEvent()
{
    RC rc;

    PmEvent_FaultBufferOverflow event(m_GpuSubdevice);
    PmEventManager *pEventManager = m_PolicyManager->GetEventManager();
    CHECK_RC(pEventManager->HandleEvent(&event));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Update the fault buffer get pointer to be equal to the
//! fault buffer put pointer.
//!
RC PmReplayableInt::ClearFaultBuffer()
{
    RC rc;
    LwRmPtr pLwRm;
    SubdeviceFaultBuffer *pFaultBuffer = SubdeviceFaultBuffer::GetFaultBuffer(m_GpuSubdevice);

    LWB069_CTRL_FAULTBUFFER_READ_PUT_PARAMS putParams = {0};
    putParams.faultBufferType = LWB069_CTRL_FAULT_BUFFER_REPLAYABLE;

    CHECK_RC(pLwRm->Control(m_FaultBufferHandle,
            LWB069_CTRL_CMD_FAULTBUFFER_READ_PUT,
            (void*)&putParams,
            sizeof(putParams)));

    m_PutIndex = putParams.faultBufferPutOffset;

    while (m_GetIndex <= m_PutIndex) {
        FaultBufferEntry::Pointer faultEntry = GetFaultEntry();
        CHECK_RC(IlwalidFaultEntry(faultEntry.get()));
        ++m_GetIndex;
    }

    m_GetIndex = m_PutIndex;
    CHECK_RC(pFaultBuffer->UpdateFaultBufferGetIndex(m_GetIndex));

    return rc;
}

FaultBufferEntry::Pointer PmReplayableInt::GetFaultEntry()
{
    const UINT32 entrySizeInDword = FaultBuffer::GetSize() / sizeof(UINT32);
    const UINT32 *faultEntryAddr =
        &(m_FaultBufferEntries[m_GetIndex * entrySizeInDword]);

    vector<UINT32> faultEntry(entrySizeInDword);
    for (UINT32 i = 0; i < faultEntry.size(); ++i)
    {
        faultEntry[i] = Platform::VirtualRd32(faultEntryAddr + i);
    }

    return CreateFaultBufferEntry(faultEntry);
}

void PmReplayableInt::SetfaultEntry(const FaultBufferEntry * const pFaultEntry)
{
    const UINT32 entrySizeInDword = LWB069_FAULT_BUF_SIZE / sizeof(UINT32);
    volatile UINT32 *faultEntryAddr =
        &(m_FaultBufferEntries[m_GetIndex * entrySizeInDword]);

    for (UINT32 i = 0; i < entrySizeInDword; ++i)
    {
        Platform::VirtualWr32(faultEntryAddr + i, (*pFaultEntry)[i]);
    }
}

RC PmReplayableInt::IlwalidFaultEntry(FaultBufferEntry *pFaultEntry)
{
    RC rc;

    CHECK_RC(pFaultEntry->Ilwalidate());
    SetfaultEntry(pFaultEntry);

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmNonReplayableInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief PmNonReplayableInt constructor
//!
PmNonReplayableInt::PmNonReplayableInt
(
    PolicyManager *policyManager,
    GpuSubdevice *gpuSubdevice
) :
    m_PolicyManager(policyManager),
    m_GpuSubdevice(gpuSubdevice),
    m_FaultBufferHandle(0),
    m_pSubdevShadowBuffer(NULL)
{
    MASSERT(policyManager != 0);
    MASSERT(gpuSubdevice != 0);
}

//--------------------------------------------------------------------
//! \brief PmNonReplayableInt destructor
//!
PmNonReplayableInt::~PmNonReplayableInt()
{
    if (m_FaultBufferHandle != 0)
    {
        PmEventManager *eventManager = m_PolicyManager->GetEventManager();
        PmNonReplayableInt *pThis = this;

        eventManager->UnhookResmanEvent(
            PmNonReplayableInt::HandleFaultInBuffer,
            &pThis,
            sizeof(pThis));

        eventManager->UnhookResmanEvent(
            PmNonReplayableInt::HandleFaultInPrivs,
            &pThis,
            sizeof(pThis));
    }
}

//--------------------------------------------------------------------
//! \brief Allocate a fault buffer object and register
//!
//! A fault buffer object is used to access the fault event buffer data.
//!
RC PmNonReplayableInt::PrepareForInterrupt()
{
    RC rc;
    LwRm* pLwRm = LwRmPtr().Get();

    if (!IsSupportMMUFaultBuffer())
    {
        ErrPrintf("MMU_FAULT_BUFFER is not supported.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Obtain a shadow buffer, which in turn will also allocate the underlying
    // fault buffer. Make sure GetQueue is non-NULL to make sure the underlying
    // data was successfully initialized.
    m_pSubdevShadowBuffer = SubdeviceShadowFaultBuffer::GetShadowFaultBuffer(m_GpuSubdevice);
    MASSERT(m_pSubdevShadowBuffer);
    m_FaultBufferHandle = m_pSubdevShadowBuffer->GetFaultBuffer()->GetFaultBufferHandle();

    // register callback for fault in buffer
    PmEventManager *eventManager = m_PolicyManager->GetEventManager();
    PmNonReplayableInt *pThis = this;
    CHECK_RC(eventManager->HookResmanEvent(
             m_FaultBufferHandle,
             LWC369_NOTIFIER_MMU_FAULT_NON_REPLAYABLE,
             PmNonReplayableInt::HandleFaultInBuffer,
             &pThis,
             sizeof(pThis),
             pLwRm));

    // register callback for fault in register
    CHECK_RC(eventManager->HookResmanEvent(
             m_FaultBufferHandle,
             LWC369_NOTIFIER_MMU_FAULT_NON_REPLAYABLE_IN_PRIV,
             PmNonReplayableInt::HandleFaultInPrivs,
             &pThis,
             sizeof(pThis),
             pLwRm));

    // register overflow handler callback
    CHECK_RC(eventManager->HookResmanEvent(
             m_FaultBufferHandle,
             LWC369_NOTIFIER_MMU_FAULT_ERROR,
             PmNonReplayableInt::HandleOverflowEvent,
             &pThis,
             sizeof(pThis),
             pLwRm));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Callback function for when a non-replayable fault in register happens.
//!
RC PmNonReplayableInt::HandleFaultInPrivs(void *ppThis)
{
    RC rc;
    if (MDiagUtils::IsFaultIndexRegisteredInUtl(LWC369_NOTIFIER_MMU_FAULT_NON_REPLAYABLE_IN_PRIV))
    {
        WarnPrintf("You cannot register handlers for the same fault in both UTL and PM.\n");
        return rc;
    }

    PmNonReplayableInt *pThis = *static_cast<PmNonReplayableInt**>(ppThis);

    DebugPrintf("%s, Handle non-replayable fault in register\n", __FUNCTION__);

    MMUFaultPrivAdapter::Pointer faultEntry = CreateFaultPrivAdapter(pThis->m_GpuSubdevice);

    //only for debug
    UINT32 engineId = 0;
    CHECK_RC(faultEntry->GetEngineID(&engineId));

    DebugPrintf("Find a non-replayable fault in Priv\n");
    pThis->CreateFaultEvent(faultEntry.get());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Callback function for when a non-replayable fault in buffer happens.
//! Today only CE fault will be written into shadow buffer in MODS
//!
RC PmNonReplayableInt::HandleFaultInBuffer(void *ppThis)
{
    RC rc;
    if (MDiagUtils::IsFaultIndexRegisteredInUtl(LWC369_NOTIFIER_MMU_FAULT_NON_REPLAYABLE))
    {
        WarnPrintf("You cannot register handlers for the same fault in both UTL and PM.\n");
        return rc;
    }

    PmNonReplayableInt *pThis = *static_cast<PmNonReplayableInt**>(ppThis);
    MASSERT(pThis->m_pSubdevShadowBuffer);
    SubdeviceShadowFaultBuffer::ShadowBufferQueue *pQueue =
        pThis->m_pSubdevShadowBuffer->GetQueue();
    QueueContext *pQueueContext = pThis->m_pSubdevShadowBuffer->GetQueueContext();

    DebugPrintf("%s, Handle non-replayable fault in buffer\n", __FUNCTION__);

    if (queueIsEmpty(pQueue))
    {
        DebugPrintf("%s, Shadow buffer is empty. This fault should be a fatal fault. "
            "RC notifier will be received.\n", __FUNCTION__);
    }

    SubdeviceShadowFaultBuffer::FaultData faultData;
    while (!queueIsEmpty(pQueue))
    {
        DebugPrintf("Find a non-replayable fault in fault buffer\n");
        queuePopAndCopyNonManaged(pQueue, pQueueContext, &faultData);
        FaultBufferEntry::Pointer faultEntry =
            CreateFaultBufferEntry(faultData.data, ARRAY_SIZE(faultData.data));
        pThis->CreateFaultEvent(faultEntry.get());
    }

    // Different from replayble fault, RM will not disable interrupt before error
    // notifier is sent to MODS.
    // Comment in gmmuServiceNonReplayableFault_GV100 indicates the reason is that
    // RM has already copied data from HW buffer to shadow buffer.
    // It is not necessary to re-enable notifier.

    return rc;
}

//--------------------------------------------------------------------
//! \brief Create various non-replay fault events based on a fault buffer entry.
//!
RC PmNonReplayableInt::CreateFaultEvent
(
    const FaultBufferEntry* const faultEntry
)
{
    RC rc;

    UINT32 GPCID;
    UINT32 clientID;
    UINT32 faultType;
    UINT64 timestamp;
    UINT32 engineID;
    bool isRecoverable = false;
    LwRm* pLwRm = LwRmPtr().Get();

    CHECK_RC(faultEntry->GetGPCID(&GPCID));
    CHECK_RC(faultEntry->GetClientID(&clientID));
    CHECK_RC(faultEntry->GetType(&faultType));
    CHECK_RC(faultEntry->GetTimeStamp(&timestamp));
    CHECK_RC(faultEntry->GetEngineID(&engineID));
    CHECK_RC(faultEntry->IsRecoverable(&isRecoverable));

    PmChannels faultingChannels;
    FaultingInfo faultingInfo(faultEntry, m_PolicyManager, m_GpuSubdevice);
    CHECK_RC(faultingInfo.GetFaultingChannels(&faultingChannels));

    FaultingRange faultingRange(faultEntry, m_PolicyManager, m_GpuSubdevice);
    PmMemRange faultingMemRange;
    // For BAR fault and etc. Can not find channel via inst memory address
    if (!faultingChannels.empty())
    {
        CHECK_RC(faultingRange.GetMemRange(&faultingMemRange));
        pLwRm = faultingChannels[0]->GetLwRmPtr();
    }

    if (faultingChannels.empty() || !IsCEEngineID(engineID, pLwRm) || !isRecoverable)
    {
        DebugPrintf("Create non-replay fatal fault. EngineID(0x%x)\n", engineID);
        PmEvent_NonReplayableFault event(m_GpuSubdevice, this, faultingMemRange, GPCID, clientID,
                                         faultType, timestamp, faultingChannels);
        PmEventManager *pEventManager = m_PolicyManager->GetEventManager();
        CHECK_RC(pEventManager->HandleEvent(&event));
    }
    else
    {
        MASSERT(!faultingChannels.empty());
        DebugPrintf("Create CE fault\n");
        const bool atsEnable = faultingChannels[0]->IsAtsEnabled();
        bool isVirtualAccess = false;

        CHECK_RC(faultEntry->IsVirtualAccess(&isVirtualAccess));

        if (isVirtualAccess || (!isVirtualAccess && atsEnable))
        {
            PmEvent_CeRecoverableFault event(m_GpuSubdevice, this, faultingMemRange, GPCID, clientID,
                                             faultType, timestamp, faultingChannels);
            PmEventManager *pEventManager = m_PolicyManager->GetEventManager();
            CHECK_RC(pEventManager->HandleEvent(&event));
        }
        else
        {
            DebugPrintf("Ignore physical access fault on ats disabled channel!");
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief determine if a given engine ID is assoicated with any CE.
//!
bool PmNonReplayableInt::IsCEEngineID(UINT32 engineID, LwRm* pLwRm) const
{
    MMUEngineDesc engine(engineID, m_GpuSubdevice, pLwRm);
    UINT32 type = LW2080_ENGINE_TYPE_LAST;
    if (engine.Type(&type))
    {
        return MDiagUtils::IsCopyEngine(type);
    }
    else
    {
        return false;
    }
}

//--------------------------------------------------------------------
//! \brief Callback function for when fault buffer overflow oclwrs.
//!
RC PmNonReplayableInt::HandleOverflowEvent(void *ppThis)
{
    RC rc;
    if (MDiagUtils::IsFaultIndexRegisteredInUtl(LWC369_NOTIFIER_MMU_FAULT_ERROR))
    {
        WarnPrintf("You cannot register handlers for the same fault in both UTL and PM.\n");
        return rc;
    }

    DebugPrintf("%s, Handle non-replayable buffer overflow\n", __FUNCTION__);

    PmNonReplayableInt *pThis = *static_cast<PmNonReplayableInt**>(ppThis);

    PmEvent_FaultBufferOverflow event(pThis->m_GpuSubdevice);
    PmEventManager *pEventManager = pThis->m_PolicyManager->GetEventManager();
    CHECK_RC(pEventManager->HandleEvent(&event));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Set GET = (PUT + 1) % SIZE. Then next fault will cause overflow
//!
RC PmNonReplayableInt::ForceNonReplayableFaultBufferOverflow()
{
    RC rc;
    UINT32 size = 0;
    UINT32 put = 0;
    CHECK_RC(m_pSubdevShadowBuffer->GetFaultBufferSize(&size));
    CHECK_RC(m_pSubdevShadowBuffer->GetFaultBufferPutPointer(&put));
    CHECK_RC(m_pSubdevShadowBuffer->UpdateFaultBufferGetPointer((put + 1) % size));
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
// PmPhysicalPageFaultInt
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief PmPhysicalPageFaultInt constructor
//!
PmPhysicalPageFaultInt::PmPhysicalPageFaultInt
(
    PolicyManager * pPolicyManager,
    GpuSubdevice * pGpuSubdevice
) :
    m_PolicyManager(pPolicyManager),
    m_pGpuSubdevice(pGpuSubdevice)
{
    MASSERT(pPolicyManager != 0);
    MASSERT(pGpuSubdevice != 0);
}

//-----------------------------------------------------------------------------
//! \brief PmPhysicalPageFaultInt destructor
//!
PmPhysicalPageFaultInt::~PmPhysicalPageFaultInt()
{
    PmEventManager * pEventManager = m_PolicyManager->GetEventManager();
    PmPhysicalPageFaultInt * pThis = this;

    pEventManager->UnhookResmanEvent(
        PmPhysicalPageFaultInt::HandleResmanEvent,
        &pThis,
        sizeof(pThis));

}

//-----------------------------------------------------------------------------
//! \brief Hook to the RM event
//!
RC PmPhysicalPageFaultInt::PrepareForInterrupt()
{
    RC rc;
    LwRm* pLwRm = LwRmPtr().Get();

    PmEventManager * pEventManager = m_PolicyManager->GetEventManager();
    PmPhysicalPageFaultInt * pThis = this;

    CHECK_RC(pEventManager->HookResmanEvent(
        m_pGpuSubdevice,
        LW2080_NOTIFIERS_PHYSICAL_PAGE_FAULT,
        PmPhysicalPageFaultInt::HandleResmanEvent,
        &pThis,
        sizeof(pThis),
        pLwRm));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Handle RM event
//!
//! register the OnPhysicalPageFault to the pmeventmanger
//!
RC PmPhysicalPageFaultInt::HandleResmanEvent(void *ppThis)
{
    RC rc;
    PmPhysicalPageFaultInt * pThis = *static_cast<PmPhysicalPageFaultInt**>(ppThis);
    PmEvent_OnPhysicalPageFault onPhysicalPageFault(pThis->m_pGpuSubdevice);
    PmEventManager * pEventManager = pThis->GetPolicyManger()->GetEventManager();
    CHECK_RC(pEventManager->HandleEvent(&onPhysicalPageFault));

    return rc;
}
//--------------------------------------------------------------------
//! \brief PmAccessCounterInt constructor
//!
PmAccessCounterInt::PmAccessCounterInt
(
    PolicyManager *policyManager,
    GpuSubdevice *gpuSubdevice,
    FLOAT64 timeoutMs
) :
    m_PolicyManager(policyManager),
    m_GpuSubdevice(gpuSubdevice),
    m_AccessCounterBufferHandle(0),
    m_AccessCounterBufferEntries(0),
    m_AccessCounterBufferSize(0),
    m_MaxAccessCounterBufferEntries(0),
    m_GetIndex(0),
    m_PutIndex(0),
    m_TimeoutMs(timeoutMs)
{
    MASSERT(policyManager != 0);
    MASSERT(gpuSubdevice != 0);

    DebugPrintf("%s, timeoutMs = %.3fms\n", __FUNCTION__, m_TimeoutMs);
}

//--------------------------------------------------------------------
//! \brief PmAccessCounterInt destructor
//!
PmAccessCounterInt::~PmAccessCounterInt()
{
    LwRmPtr pLwRm;

    if (m_AccessCounterBufferHandle != 0)
    {
        PmEventManager *eventManager = m_PolicyManager->GetEventManager();
        PmAccessCounterInt *pThis = this;

        eventManager->UnhookResmanEvent(
            PmAccessCounterInt::HandleResmanEvent,
            &pThis,
            sizeof(pThis));

        if (m_AccessCounterBufferEntries != 0)
        {
            pLwRm->UnmapMemory(
                m_AccessCounterBufferHandle,
                m_AccessCounterBufferEntries,
                0,
                m_GpuSubdevice);
        }

        pLwRm->Free(m_AccessCounterBufferHandle);
    }
}

//--------------------------------------------------------------------
//! \brief Allocate a access counter buffer object
//!
//! An access counter buffer object is used to access the access counter buffer data.
//!
RC PmAccessCounterInt::PrepareForInterrupt()
{
    RC rc;
    UINT32 accessCounterBufferAllocParams = 0;
    LwRm* pLwRm = LwRmPtr().Get();

    // Allocate the access counter buffer object.

    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(m_GpuSubdevice),
                 &m_AccessCounterBufferHandle,
                 ACCESS_COUNTER_NOTIFY_BUFFER,
                 &accessCounterBufferAllocParams));

    // Get the size of the access counter buffer.

    LWC365_CTRL_ACCESS_CNTR_BUFFER_GET_SIZE_PARAMS sizeParams = {0};

    CHECK_RC(pLwRm->Control(m_AccessCounterBufferHandle,
        LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_GET_SIZE,
        (void*)&sizeParams,
        sizeof(sizeParams)));

    m_AccessCounterBufferSize = sizeParams.accessCntrBufferSize;
    m_MaxAccessCounterBufferEntries = m_AccessCounterBufferSize / LWC365_NOTIFY_BUF_SIZE;

    // Map the access counter buffer to a pointer so it can be read.

    CHECK_RC(pLwRm->MapMemory(
        m_AccessCounterBufferHandle,
        0,
        m_AccessCounterBufferSize,
        (void**)&m_AccessCounterBufferEntries,
        0,
        m_GpuSubdevice));

    // Pass a callback function to RM which will be called when an
    // access counter notification happens.

    PmEventManager *eventManager = m_PolicyManager->GetEventManager();
    PmAccessCounterInt *pThis = this;

    CHECK_RC(eventManager->HookResmanEvent(
                 m_AccessCounterBufferHandle,
                 LWC365_NOTIFIERS_ACCESS_COUNTER,
                 PmAccessCounterInt::HandleResmanEvent,
                 &pThis,
                 sizeof(pThis),
                 pLwRm));

    LWC365_CTRL_ACCESS_CNTR_BUFFER_ENABLE_PARAMS notificationParams = {0};
    notificationParams.intrOwnership = LWC365_CTRL_ACCESS_COUNTER_INTERRUPT_OWNERSHIP_NO_CHANGE;
    notificationParams.enable = true;

    CHECK_RC(pLwRm->Control(m_AccessCounterBufferHandle,
        LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_ENABLE,
        (void*)&notificationParams,
        sizeof(notificationParams)));

    // Read the access counter buffer get pointer and save it.
    // Note:
    //     GET=PUT=0 reset in hardware happens on LO_EN goes from 0->1
    //     So get pointer read should happen after LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_ENABLE
    LWC365_CTRL_ACCESS_CNTR_BUFFER_READ_GET_PARAMS getParams = {0};

    CHECK_RC(pLwRm->Control(m_AccessCounterBufferHandle,
                 LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_READ_GET,
                 (void*)&getParams,
                 sizeof(getParams)));

    m_GetIndex = getParams.accessCntrBufferGetOffset;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Callback function for when an access counter notify happens.
//!
RC PmAccessCounterInt::HandleResmanEvent(void *ppThis)
{
    RC rc;
    PmAccessCounterInt *pThis = *static_cast<PmAccessCounterInt**>(ppThis);
    LwRmPtr pLwRm;
    LWC365_CTRL_ACCESS_CNTR_BUFFER_READ_PUT_PARAMS putParams = {0};

    CHECK_RC(pLwRm->Control(pThis->m_AccessCounterBufferHandle,
                 LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_READ_PUT,
                 (void*)&putParams,
                 sizeof(putParams)));

    pThis->m_PutIndex = putParams.accessCntrBufferPutOffset;

    if (pThis->m_GetIndex != pThis->m_PutIndex)
    {
        MASSERT(pThis->m_PutIndex < pThis->m_MaxAccessCounterBufferEntries);

        while (pThis->m_GetIndex != pThis->m_PutIndex)
        {
            DebugPrintf("%s, GetIndex = %u PutIndex = %u\n", __FUNCTION__, pThis->m_GetIndex, pThis->m_PutIndex);
            vector<UINT32> accessCounterEntry = pThis->GetAccessCounterEntry();

            // If the valid bit in the access counter buffer entry is false,
            // that means that the buffer entry is not completely written yet
            // MODS has to wait until it becomes valid otherwise it could be lost
            // if this is the last interrupt
            if (FLD_TEST_DRF_MW(C365, _NOTIFY_BUF_ENTRY, _VALID, _FALSE,
                    &accessCounterEntry[0]))
            {
                // The ctrl call here for FbFlush is temporary and might be
                // repalced with GpuSubdevice::FbFlush() when the issue in the
                // bug https://lwbugs/3394776 is resolved
                LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS flushParams = {};
                flushParams.flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY) |
                    DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FB_FLUSH, _YES);
                CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(pThis->GetGpuSubDevice()),
                    LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
                    &flushParams, sizeof(flushParams)));
                
                auto pollFunc =
                    [](void * pollArgs)
                    {
                        auto pThis = static_cast<PmAccessCounterInt*>(pollArgs);
                        vector<UINT32> accessCounterEntry = pThis->GetAccessCounterEntry();
                        auto isValid = FLD_TEST_DRF_MW(C365, _NOTIFY_BUF_ENTRY,_VALID, _TRUE, &accessCounterEntry[0]);
                        if (!isValid)
                        {
                            DebugPrintf("access counter entry %u is not valid\n", pThis->m_GetIndex);
                            RegHal& regs = pThis->GetGpuSubDevice()->Regs();
                            if (regs.IsSupported(MODS_PFB_FBHUB_ACCESS_COUNTER_NOTIFY_BUFFER_INFO))
                            {
                                const UINT32 value = regs.Read32(MODS_PFB_FBHUB_ACCESS_COUNTER_NOTIFY_BUFFER_INFO);
                                if (regs.TestField(value, MODS_PFB_FBHUB_ACCESS_COUNTER_NOTIFY_BUFFER_INFO_WRITE_NACK_TRUE))
                                {
                                    InfoPrintf("WRITE_NACK_TRUE is set so a fault must occur somewhere!\n");
                                    return true;
                                }
                            }
                            else
                            {
                                const UINT32 value = regs.Read32(MODS_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_INFO);
                                if (regs.TestField(value, MODS_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_INFO_WRITE_NACK_TRUE))
                                {
                                    InfoPrintf("WRITE_NACK_TRUE is set so a fault must occur somewhere!\n");
                                    return true;
                                }
                            }
                        }

                        return isValid;
                    };

                CHECK_RC(Tasker::Poll(pollFunc, pThis, pThis->m_TimeoutMs,__FUNCTION__, "PmAccessCounterInt::PollEntryValidBit"));

                accessCounterEntry = pThis->GetAccessCounterEntry();

                auto isValid = FLD_TEST_DRF_MW(C365, _NOTIFY_BUF_ENTRY,_VALID, _TRUE, &accessCounterEntry[0]);
                if (!isValid)
                {
                    // when _WRITE_NACK_TRUE is set, we never expect to see VALID bit will be set.
                    // so we intend to time out
                    return RC::TIMEOUT_ERROR;
                }

            }

            // Otherwise create a access counter notification event
            // based on the buffer entry.
            DebugPrintf("reading access counter entry %u\n", pThis->m_GetIndex);

            FaultBufferEntry::Pointer faultEntry =
                CreateAccessCounterNotifyBufferEntry(&accessCounterEntry[0], accessCounterEntry.size());
            CHECK_RC(pThis->CreateAccessCounterNotificationEvent(faultEntry.get()));

            if (pThis->m_GetIndex == pThis->m_PutIndex)
            {
                // GetIndex must be updated elsewhere, like ClearFaultBuffer
                // Supposed that valid bit of each entries must be cleared correctly
                break;
            }

            pThis->IlwalidateAccessCounterEntry(&accessCounterEntry);

            ++pThis->m_GetIndex;

            if (pThis->m_GetIndex >= pThis->m_MaxAccessCounterBufferEntries)
            {
                pThis->m_GetIndex = 0;
            }
        }

        CHECK_RC(pThis->UpdateAccessCounterBufferGetPointer(pThis->m_PutIndex));
    }

    // Before RM calls this function, it will turn off access counter
    // notifications in order to prevent multiple callbacks running at the same
    // time.  We need to re-enable notifications when we're done.

    LWC365_CTRL_ACCESS_CNTR_BUFFER_ENABLE_INTR_PARAMS notificationParams = {0};
    notificationParams.enable = true;

    CHECK_RC(pLwRm->Control(pThis->m_AccessCounterBufferHandle,
        LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_ENABLE_INTR,
        (void*)&notificationParams,
        sizeof(notificationParams)));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Update the access counter buffer get pointer to be equal to the
//! access counter  buffer put pointer.
//!
RC PmAccessCounterInt::ClearAccessCounterBuffer()
{
    RC rc;
    LwRmPtr pLwRm;
    LWC365_CTRL_ACCESS_CNTR_BUFFER_READ_PUT_PARAMS putParams = {0};

    CHECK_RC(pLwRm->Control(m_AccessCounterBufferHandle,
            LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_READ_PUT,
            (void*)&putParams,
            sizeof(putParams)));

    m_PutIndex = putParams.accessCntrBufferPutOffset;

    while (m_GetIndex <= m_PutIndex)
    {
        vector<UINT32> accessCounterEntry = GetAccessCounterEntry();
        IlwalidateAccessCounterEntry(&accessCounterEntry);
        ++m_GetIndex;
    }

    m_GetIndex = m_PutIndex;
    CHECK_RC(UpdateAccessCounterBufferGetPointer(m_GetIndex));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Create an access counter event based on a access counter buffer entry.
//!
RC PmAccessCounterInt::CreateAccessCounterNotificationEvent
(
    const FaultBufferEntry * const faultEntry
)
{
    RC rc;

    PmMemRange memRange;
    UINT64 instanceBlockPointer(0);
    UINT32 aperture(0);
    UINT32 type(0);
    UINT32 addressType(0);
    UINT32 counterValue(0);
    UINT32 peerID(0);
    UINT32 mmuEngineID(0);
    UINT32 bank(0);
    UINT32 notifyTag(0);
    UINT32 subGranularity(0);
    UINT32 instAperture(0);

    unique_ptr<FaultingRange> pFaultingRange(new FaultingRange(faultEntry, m_PolicyManager, m_GpuSubdevice));
    CHECK_RC(pFaultingRange->GetMemRange(&memRange));
    PmChannels faultingChannels;

    const AccessCounterNotifyBufferEntry * accessCounterFaultEntry = dynamic_cast<const AccessCounterNotifyBufferEntry *>(faultEntry);

    bool isVirtualAccess = false;
    CHECK_RC(accessCounterFaultEntry->IsVirtualAccess(&isVirtualAccess));

    if (isVirtualAccess)
    {
        FaultingInfo faultingInfo(faultEntry, m_PolicyManager, m_GpuSubdevice);
        CHECK_RC(faultingInfo.GetFaultingChannels(&faultingChannels));
        MASSERT(!faultingChannels.empty());
    }

    CHECK_RC(accessCounterFaultEntry->GetInstPhysAddr(&instanceBlockPointer));
    CHECK_RC(accessCounterFaultEntry->GetAddressAperture(&aperture));
    CHECK_RC(accessCounterFaultEntry->GetType(&type));
    CHECK_RC(accessCounterFaultEntry->GetAddressType(&addressType));
    CHECK_RC(accessCounterFaultEntry->GetCounterValue(&counterValue));
    CHECK_RC(accessCounterFaultEntry->GetPeerID(&peerID));
    CHECK_RC(accessCounterFaultEntry->GetEngineID(&mmuEngineID));
    CHECK_RC(accessCounterFaultEntry->GetBank(&bank));
    CHECK_RC(accessCounterFaultEntry->GetNotifyTag(&notifyTag));
    CHECK_RC(accessCounterFaultEntry->GetSubGranularity(&subGranularity));
    CHECK_RC(accessCounterFaultEntry->GetInstAperture(&instAperture));

    PmEvent_AccessCounterNotification event(
        m_GpuSubdevice, this, memRange, aperture, instanceBlockPointer,
        type, addressType, counterValue, peerID, mmuEngineID, bank,
        notifyTag, subGranularity, instAperture, isVirtualAccess, faultingChannels);
    PmEventManager *pEventManager = m_PolicyManager->GetEventManager();
    CHECK_RC(pEventManager->HandleEvent(&event));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Update the access counter buffer get pointer of the access counter event buffer.
//!
RC PmAccessCounterInt::UpdateAccessCounterBufferGetPointer(UINT32 newIndex)
{
    RC rc;
    LWC365_CTRL_ACCESS_CNTR_BUFFER_WRITE_GET_PARAMS writeGetParams = {0};
    writeGetParams.accessCntrBufferGetValue = newIndex;

    LwRmPtr pLwRm;
    CHECK_RC(pLwRm->Control(m_AccessCounterBufferHandle,
            LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_WRITE_GET,
            (void*)&writeGetParams,
            sizeof(writeGetParams)));

    return rc;
}

vector<UINT32> PmAccessCounterInt::GetAccessCounterEntry()
{
    const UINT32 entrySizeInDword = LWC365_NOTIFY_BUF_SIZE / sizeof(UINT32);
    const UINT32 *accessCounterEntryAddr =
        &(m_AccessCounterBufferEntries[m_GetIndex * entrySizeInDword]);

    vector<UINT32> accessCounterEntry(entrySizeInDword);
    for (UINT32 ii = 0; ii < accessCounterEntry.size(); ++ii)
    {
        accessCounterEntry[ii] = Platform::VirtualRd32(accessCounterEntryAddr + ii);
    }

    return accessCounterEntry;
}

void PmAccessCounterInt::SetAccessCounterEntry(const vector<UINT32> &accessCounterEntry)
{
    const UINT32 entrySizeInDword = LWC365_NOTIFY_BUF_SIZE / sizeof(UINT32);
    volatile UINT32 *accessCounterEntryAddr =
        &(m_AccessCounterBufferEntries[m_GetIndex * entrySizeInDword]);

    for (UINT32 ii = 0; ii < accessCounterEntry.size(); ++ii)
    {
        Platform::VirtualWr32(accessCounterEntryAddr + ii, accessCounterEntry[ii]);
    }
}

void PmAccessCounterInt::IlwalidateAccessCounterEntry(vector<UINT32> *pAccessCounterEntry)
{
    // Clear the valid bit now that the entry has been processed.
    FLD_SET_DRF_DEF_MW(C365, _NOTIFY_BUF_ENTRY, _VALID, _FALSE,
        &(*pAccessCounterEntry)[0]);
    SetAccessCounterEntry(*pAccessCounterEntry);
}

//--------------------------------------------------------------------
//! \brief PmErrorLoggerInt constructor
//!
PmErrorLoggerInt::PmErrorLoggerInt
(
    PolicyManager *policyManager,
    GpuSubdevice *gpuSubdevice,
    const PolicyManager::InterruptNames *pNames
) :
    m_PolicyManager(policyManager),
    m_GpuSubdevice(gpuSubdevice),
    m_LastIndex(0)
{
    MASSERT(policyManager != nullptr);
    MASSERT(gpuSubdevice != nullptr);
    MASSERT(pNames != nullptr);
    for (auto &name : *pNames)
    {
        m_Events.push_back(std::make_tuple(name, new Event()));
    }
}

//--------------------------------------------------------------------
//! \brief PmErrorLoggerInt destructor
//!
PmErrorLoggerInt::~PmErrorLoggerInt()
{
    for (const auto &event : m_Events)
    {
        delete std::get<1>(event);
    }

    PmEventManager *eventManager = m_PolicyManager->GetEventManager();
    PmErrorLoggerInt *pThis = this;
    eventManager->UnhookErrorLoggerEvent(
        PmErrorLoggerInt::HandleErrorLoggerEvent,
        &pThis,
        sizeof(pThis));
}

//--------------------------------------------------------------------
//! \brief Allocate an error logger interrupt object
//!
RC PmErrorLoggerInt::PrepareForInterrupt()
{
    RC rc;
    PmEventManager *eventManager = m_PolicyManager->GetEventManager();
    PmErrorLoggerInt *pThis = this;
    CHECK_RC(eventManager->HookErrorLoggerEvent(
        PmErrorLoggerInt::HandleErrorLoggerEvent,
        &pThis,
        sizeof(pThis)));

    // Set the index to the current error count so that we don't trigger on
    // anything that got logged before the test started.
    ErrorLogger::FlushLogBuffer();
    m_LastIndex = ErrorLogger::GetErrorCount();

    return rc;
}

RC PmErrorLoggerInt::HandleErrorLoggerEvent(void *ppThis)
{
    RC rc;
    PmErrorLoggerInt *pThis = *static_cast<PmErrorLoggerInt**>(ppThis);

    ErrorLogger::FlushLogBuffer();

    auto errorCount = ErrorLogger::GetErrorCount();

    for (auto &event : pThis->m_Events)
    {
        bool match = false;
        const std::string eventString = std::get<0>(event);
        for (UINT32 i = pThis->m_LastIndex; i < errorCount; i++)
        {
            string errString = ErrorLogger::GetErrorString(i);
            RegExp regExp;
            CHECK_RC(regExp.Set(eventString));
            if (regExp.Matches(errString))
            {
                std::get<1>(event)->Set();
                match = true;
            }
        }

        if (match)
        {
            PmEvent_OnErrorLoggerInterrupt event(pThis->m_GpuSubdevice, eventString);
            CHECK_RC(pThis->m_PolicyManager->GetEventManager()->HandleEvent(&event));
        }
    }

    pThis->m_LastIndex = errorCount;
    return OK;
}

///////////////////////////////////////////////////////////////////////////////
// PmNonfatalPoisonErrorInt
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief PmNonfatalPoisonErrorInt constructor
//!
PmNonfatalPoisonErrorInt::PmNonfatalPoisonErrorInt
(
    PolicyManager * pPolicyManager,
    GpuSubdevice * pGpuSubdevice
) :
    m_PolicyManager(pPolicyManager),
    m_pGpuSubdevice(pGpuSubdevice)
{
    MASSERT(pPolicyManager != 0);
    MASSERT(pGpuSubdevice != 0);
}

//-----------------------------------------------------------------------------
//! \brief PmNonfatalPoisonErrorInt destructor
//!
PmNonfatalPoisonErrorInt::~PmNonfatalPoisonErrorInt()
{
    PmEventManager * pEventManager = m_PolicyManager->GetEventManager();
    PmNonfatalPoisonErrorInt * pThis = this;

    pEventManager->UnhookResmanEvent(
        PmNonfatalPoisonErrorInt::HandleResmanEvent,
        &pThis,
        sizeof(pThis));

}

//-----------------------------------------------------------------------------
//! \brief Hook to the RM event
//!
RC PmNonfatalPoisonErrorInt::PrepareForInterrupt()
{
    RC rc;
    LwRm* pLwRm = LwRmPtr().Get();

    PmEventManager * pEventManager = m_PolicyManager->GetEventManager();
    PmNonfatalPoisonErrorInt * pThis = this;

    CHECK_RC(pEventManager->HookResmanEvent(
        m_pGpuSubdevice,
        LW2080_NOTIFIERS_POISON_ERROR_NON_FATAL,
        PmNonfatalPoisonErrorInt::HandleResmanEvent,
        &pThis,
        sizeof(pThis),
        pLwRm));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Handle RM event
//!
//! register the OnNonfatalPoisonError to the pmeventmanger
//!
RC PmNonfatalPoisonErrorInt::HandleResmanEvent(void *ppThis)
{
    RC rc;
    PmNonfatalPoisonErrorInt * pThis = *static_cast<PmNonfatalPoisonErrorInt**>(ppThis);

    LwNotification data;
    CHECK_RC(pThis->m_pGpuSubdevice->GetResmanEventData(LW2080_NOTIFIERS_POISON_ERROR_NON_FATAL, &data));
    PmEvent_OnNonfatalPoisonError onNonfatalPoisonError(pThis->m_pGpuSubdevice, data.info16);

    PmEventManager * pEventManager = pThis->GetPolicyManger()->GetEventManager();
    CHECK_RC(pEventManager->HandleEvent(&onNonfatalPoisonError));

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
// PmFatalPoisonErrorInt
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief PmFatalPoisonErrorInt constructor
//!
PmFatalPoisonErrorInt::PmFatalPoisonErrorInt
(
    PolicyManager * pPolicyManager,
    GpuSubdevice * pGpuSubdevice
) :
    m_PolicyManager(pPolicyManager),
    m_pGpuSubdevice(pGpuSubdevice)
{
    MASSERT(pPolicyManager != 0);
    MASSERT(pGpuSubdevice != 0);
}

//-----------------------------------------------------------------------------
//! \brief PmFatalPoisonErrorInt destructor
//!
PmFatalPoisonErrorInt::~PmFatalPoisonErrorInt()
{
    PmEventManager * pEventManager = m_PolicyManager->GetEventManager();
    PmFatalPoisonErrorInt * pThis = this;

    pEventManager->UnhookResmanEvent(
        PmFatalPoisonErrorInt::HandleResmanEvent,
        &pThis,
        sizeof(pThis));

}

//-----------------------------------------------------------------------------
//! \brief Hook to the RM event
//!
RC PmFatalPoisonErrorInt::PrepareForInterrupt()
{
    RC rc;
    LwRm* pLwRm = LwRmPtr().Get();

    PmEventManager * pEventManager = m_PolicyManager->GetEventManager();
    PmFatalPoisonErrorInt * pThis = this;

    CHECK_RC(pEventManager->HookResmanEvent(
        m_pGpuSubdevice,
        LW2080_NOTIFIERS_POISON_ERROR_FATAL,
        PmFatalPoisonErrorInt::HandleResmanEvent,
        &pThis,
        sizeof(pThis),
        pLwRm));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Handle RM event
//!
//! register the OnFatalPoisonError to the pmeventmanger
//!
RC PmFatalPoisonErrorInt::HandleResmanEvent(void *ppThis)
{
    RC rc;
    PmFatalPoisonErrorInt * pThis = *static_cast<PmFatalPoisonErrorInt**>(ppThis);

    LwNotification data;
    CHECK_RC(pThis->m_pGpuSubdevice->GetResmanEventData(LW2080_NOTIFIERS_POISON_ERROR_FATAL, &data));
    PmEvent_OnFatalPoisonError onFatalPoisonError(pThis->m_pGpuSubdevice, data.info16);

    PmEventManager * pEventManager = pThis->GetPolicyManger()->GetEventManager();
    CHECK_RC(pEventManager->HandleEvent(&onFatalPoisonError));

    return rc;
}

//--------------------------------------------------------------------
//! \brief PmRegister ctor
//!
PmRegister::PmRegister
(
    LWGpuResource* pGpuRes,
    UINT32 subdev,
    const string& regName,
    PmSmcEngine *pPmSmcEngine,
    const string& regSpace
) :
    PmRegister(pGpuRes, subdev, regName, {} , pPmSmcEngine, regSpace)
{
}
//--------------------------------------------------------------------
//! \brief PmRegister ctor with indexes
//!
PmRegister::PmRegister
(
    LWGpuResource* pGpuRes,
    UINT32 subdev,
    const string& regName,
    const vector<UINT32> &regIndexes,
    PmSmcEngine *pPmSmcEngine,
    const string& regSpace
) :
    m_pGpuRes(pGpuRes),
    m_Subdev(subdev),
    m_RegName(regName),
    m_RegIndexes(regIndexes),
    m_RegSpace(regSpace),
    m_pPmSmcEngine(pPmSmcEngine),
    m_pRefManualRegister(nullptr),
    m_RegHalAddress(MODS_REGISTER_ADDRESS_NULL),
    m_RegOffset(0),
    m_Data(0)
{

    GpuSubdevice *pGpuSubdev = m_pGpuRes->GetGpuSubdevice(m_Subdev);
    if (starts_with(m_RegName, "LW_"))
    {
        const RefManual *pRefManual = pGpuSubdev->GetRefManual();
        if (!pRefManual)
            MASSERT(!"ref manual is not found!");
        m_pRefManualRegister = pRefManual->FindRegister(m_RegName.c_str());
        if (!m_pRefManualRegister)
        {
            ErrPrintf("register(%s) is not found in ref manual!\n",
                      m_RegName.c_str());
            MASSERT(!"Nonexistent register!");
        }

        switch (m_RegIndexes.size())
        {
            case 0:
                m_RegOffset = m_pRefManualRegister->GetOffset();
                break;
            case 1:
                m_RegOffset = m_pRefManualRegister->GetOffset(m_RegIndexes[0]);
                break;
            case 2:
                m_RegOffset = m_pRefManualRegister->GetOffset(m_RegIndexes[0],
                                                              m_RegIndexes[1]);
                break;
            default:
                MASSERT(!"Too many indexes");
                m_RegOffset = m_pRefManualRegister->GetOffset(m_RegIndexes[0],
                                                              m_RegIndexes[1]);
                break;
        }

        LwRm *pLwRm = m_pPmSmcEngine ? m_pPmSmcEngine->GetLwRmPtr() : LwRmPtr().Get();
        m_RegOffset += MDiagUtils::GetDomainBase(m_RegName.c_str(), m_RegSpace.c_str(), pGpuSubdev, pLwRm);
    }
    else if (starts_with(m_RegName, "MODS_"))
    {
        m_RegHalAddress = RegHal::ColwertToAddress(m_RegName);
        if (m_RegHalAddress == MODS_REGISTER_ADDRESS_NULL)
        {
            ErrPrintf("register(%s) not found in RegHal!\n",
                      m_RegName.c_str());
            MASSERT(!"Nonexistent register!");
        }
        else if (!pGpuSubdev->Regs().IsSupported(m_RegHalAddress))
        {
            ErrPrintf("register(%s) not supported on %s!\n",
                      m_RegName.c_str(),
                      Gpu::DeviceIdToString(pGpuSubdev->DeviceId()).c_str());
            MASSERT(!"Unsupported register!");
        }
        m_RegOffset = pGpuSubdev->Regs().LookupAddress(m_RegHalAddress);
    }
    else
    {
        ErrPrintf("register(%s) must start with LW_ or MODS_!\n",
                  m_RegName.c_str());
        MASSERT(!"Bad register name!");
    }
}

//--------------------------------------------------------------------
//! \brief Return true if the register is readable
//!
bool PmRegister::IsReadable() const
{
    return (m_pRefManualRegister ?
            m_pRefManualRegister->IsReadable() :
            m_RegHalAddress != MODS_REGISTER_ADDRESS_NULL);
}

//--------------------------------------------------------------------
//! \brief Return true if the register is writeable
//!
bool PmRegister::IsWriteable() const
{
    return (m_pRefManualRegister ?
            m_pRefManualRegister->IsWriteable() :
            m_RegHalAddress != MODS_REGISTER_ADDRESS_NULL);
}

RC PmRegister::Read()
{
    if (!IsReadable())
    {
        ErrPrintf("register(%s) is not readable!\n", m_RegName.c_str());
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    SmcEngine *pSmcEngine = m_pPmSmcEngine ? m_pPmSmcEngine->GetSmcEngine() : nullptr;
    m_Data = m_pGpuRes->RegRd32(m_RegOffset, m_Subdev, pSmcEngine);
    return OK;
}

//--------------------------------------------------------------------
//! \brief Write data back into register
//!
RC PmRegister::Write()
{
    if (!IsWriteable())
    {
        ErrPrintf("register(%s) is not writeable!\n",
                  m_RegName.c_str());
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    bool bRegRestoreNeeded = true;
    UINT32 oldData = ~0U;

    for (auto& pRegExcl : m_RegRestoreExclude)
    {
        if (starts_with(m_RegName, pRegExcl))
        {
            bRegRestoreNeeded = false;
        }
    }

    if (m_pGpuRes->IsSMCEnabled() && m_pGpuRes->IsPgraphReg(m_RegOffset))
    {
        bRegRestoreNeeded = false;
    }

    SmcEngine *pSmcEngine = m_pPmSmcEngine ? m_pPmSmcEngine->GetSmcEngine() : nullptr;

    if (bRegRestoreNeeded)
    {
        oldData = m_pGpuRes->RegRd32(m_RegOffset, m_Subdev, pSmcEngine);
    }

    m_pGpuRes->RegWr32(m_RegOffset, m_Data, m_Subdev, pSmcEngine);

    if (bRegRestoreNeeded)
    {
        PolicyManager::Instance()->RegisterRegRestore(
                [regOffset = this->m_RegOffset, pGpuRes = this->m_pGpuRes,
                subdev = this->m_Subdev, oldData, pSmcEngine]() {
                pGpuRes->RegWr32(regOffset, oldData, subdev, pSmcEngine);});
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Set value into given field
//!
RC PmRegister::SetField
(
    const string& fieldName,
    UINT32 value
)
{
    RC rc;

    CHECK_RC(Read());

    if (m_pRefManualRegister)
    {
        const RefManualRegisterField* pRegField =
            m_pRefManualRegister->FindField(fieldName.c_str());

        if (!pRegField)
        {
            ErrPrintf("field %s is not found!\n", fieldName.c_str());
            return RC::REGISTER_READ_WRITE_FAILED;
        }
        if (!pRegField->IsWriteable())
        {
            ErrPrintf("field %s is not writeable!\n", fieldName.c_str());
            return RC::REGISTER_READ_WRITE_FAILED;
        }

        UINT32 mask = pRegField->GetMask();
        m_Data &= ~mask;
        m_Data |= ((value << pRegField->GetLowBit()) & mask);
    }
    else
    {
        GpuSubdevice *pGpuSubdev = m_pGpuRes->GetGpuSubdevice(m_Subdev);
        const ModsGpuRegField regField = RegHal::ColwertToField(fieldName);
        MASSERT(m_RegHalAddress == RegHal::ColwertToAddress(regField));
        if (!pGpuSubdev->Regs().IsSupported(regField))
        {
            ErrPrintf("field %s is not supported on %s!\n",
                      fieldName.c_str(),
                      Gpu::DeviceIdToString(pGpuSubdev->DeviceId()).c_str());
            return RC::REGISTER_READ_WRITE_FAILED;
        }
        pGpuSubdev->Regs().SetField(&m_Data, regField, value);
    }

    CHECK_RC(Write());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Set masked value into register
//!
//!  Intended for writing unnamed fields
//!  Only use this if SetField is unsuitable
//!
RC PmRegister::SetMasked
(
    UINT32 andMask,
    UINT32 value
)
{
    RC rc;

    CHECK_RC(Read());

    m_Data &= ~andMask;
    m_Data |= (value & andMask);

    CHECK_RC(Write());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Set register value, which is not written into GPU
//!
void PmRegister::SetRegValue(UINT32 val)
{
    m_Data = val;
}

//--------------------------------------------------------------------
//! \brief Get register offset
//!
UINT32 PmRegister::GetRegOffset() const
{
    return m_RegOffset;
}
