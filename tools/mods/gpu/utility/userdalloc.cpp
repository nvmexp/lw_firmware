/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/userdalloc.h"
#include "core/include/channel.h"
#include "core/include/rc.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/memtypes.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "surf2d.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "gpu/include/gralloc.h"
#include "lwRmReg.h"
#include "core/include/registry.h"
#include "class/cl506f.h" // LW50_CHANNEL_GPFIFO
#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h" // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h" // HOPPER_CHANNEL_GPFIFO_A
#include <vector>
#include <queue>
#include <string>
#include <algorithm>

namespace
{
    TeeModule s_ModsUserdModule("ModsUserd");

    //--------------------------------------------------------------------------
    //! \brief Get the highest GPFIFO class supported on the specified GPU
    //! unless the class is explicitly specififed and the size of its USERD area
    //!
    //! \param pGpuDev      : Pointer to Gpu device to get GPFIFO information on
    //! \param pLwRm        : RM client to use when retrieving information
    //! \param pGpFifoClass : GPFIFO class to get USERD size for if not the
    //!                       default class, if default then the pointer to
    //!                       highest supported GPFIFO class is returned
    //! \param pUserdSize   : Pointer to returned USERD size of discovered class
    //! \param pNumChannels : Pointer to returned number of USERD spaces to
    //!                       allocate if set to default (if non-null)
    //!
    //! \return OK if successful, not OK otherwise
    RC GetGpFifoChannelInfo
    (
         GpuDevice *pGpuDev
        ,LwRm      *pLwRm
        ,UINT32    *pGpFifoClass
        ,UINT32    *pUserdSize
        ,UINT32    *pNumChannels
    )
    {
        RC rc;

        // If the GPFIFO class is default, then get the highest supported GpFifo
        // class on the specified GPU
        if (*pGpFifoClass == UserdManager::DEFAULT_GPFIFO_CLASS)
        {
            CHECK_RC(pLwRm->GetFirstSupportedClass(static_cast<UINT32>(
                                                   Channel::FifoClasses.size()),
                                                   &(Channel::FifoClasses[0]),
                                                   pGpFifoClass,
                                                   pGpuDev));
        }

        switch (*pGpFifoClass)
        {
            case GF100_CHANNEL_GPFIFO:
                *pUserdSize = sizeof(Lw906fControl);
                break;
            case KEPLER_CHANNEL_GPFIFO_A:
                *pUserdSize = sizeof(LwA06FControl);
                break;
            case KEPLER_CHANNEL_GPFIFO_B:
                *pUserdSize = sizeof(LwA16FControl);
                break;
            case KEPLER_CHANNEL_GPFIFO_C:
                *pUserdSize = sizeof(LwA26FControl);
                break;
            case MAXWELL_CHANNEL_GPFIFO_A:
                *pUserdSize = sizeof(Lwb06FControl);
                break;
            case PASCAL_CHANNEL_GPFIFO_A:
                *pUserdSize = sizeof(Lwc06fControl);
                break;
            case VOLTA_CHANNEL_GPFIFO_A:
                *pUserdSize = sizeof(Lwc36fControl);
                break;
            case TURING_CHANNEL_GPFIFO_A:
                *pUserdSize = sizeof(Lwc46fControl);
                break;
            case AMPERE_CHANNEL_GPFIFO_A:
                *pUserdSize = sizeof(Lwc56fControl);
                break;
            case HOPPER_CHANNEL_GPFIFO_A:
                *pUserdSize = sizeof(Lwc86fControl);
                break;
            default:
                Printf(Tee::PriError, s_ModsUserdModule.GetCode(),
                       "Unknown channel class : 0x%04x\n",
                       *pGpFifoClass);
                return RC::SOFTWARE_ERROR;
        }

        if (pNumChannels &&
            (*pNumChannels == UserdManager::DEFAULT_USERD_ALLOCS))
        {
            // Get max physical channel number supported from RM
            LW2080_CTRL_FIFO_GET_PHYSICAL_CHANNEL_COUNT_PARAMS params = { };
            CHECK_RC(pLwRm->ControlBySubdevice(pGpuDev->GetSubdevice(0),
                                               LW2080_CTRL_CMD_FIFO_GET_PHYSICAL_CHANNEL_COUNT,
                                               &params,
                                               sizeof(params)));
            *pNumChannels = params.physChannelCount;
        }
        return rc;
    }

    //--------------------------------------------------------------------------
    //! \brief Allocate USERD memory and create necessary GPU mappings
    //!
    //! \param pSurf2d      : Pointer surface to allocate USERD memory on
    //! \param pGpuDev      : GpuDevice to use for allocation
    //! \param pLwRm        : RM client to use for allocation
    //! \param channelCount : Number of channels to support in the allocation
    //! \param userdSize    : Size of USERD space per channel
    //! \param memLoc       : Memory location to for USERD space
    //! \param pAddresses   : Pointer to returned vector of CPU addresses
    //!
    //! \return OK if successful, not OK otherwise
    RC AllolwserdMemory
    (
         Surface2D        *pSurf2d
        ,GpuDevice        *pGpuDev
        ,LwRm             *pLwRm
        ,UINT32            channelCount
        ,UINT32            userdSize
        ,Memory::Location  memLoc
        ,vector<void *>   *pAddresses
    )
    {
        // Get the actual memory location first since the location of USERD
        // space will determine how per-subdevice allocations are handled.
        //
        // When there are multiple subdevices and the USERD space is in FB
        // memory, memory allocation is done automatically across all subdevices
        // within RM internally so the size of memory does not need to be
        // scaled by the number of subdevices
        //
        // When there are multiple subdevices and USERD space is not in FB
        // memory, all subdevices share the same single memory allocation (i.e.
        // RM does not mirror system memory allocations across multiple
        // subdevices within a device).  Therefore, need to scale the USERD size
        // by the number of subdevices
        memLoc = Surface2D::GetActualLocation(memLoc, pGpuDev->GetSubdevice(0));
#ifdef SIM_BUILD
        memLoc = pSurf2d->CheckForIndividualLocationOverride(memLoc,
            pGpuDev->GetSubdevice(0));
#endif
        if (memLoc != Memory::Fb)
        {
            userdSize *= pGpuDev->GetNumSubdevices();
        }

        pSurf2d->SetAddressModel(Memory::Paging);
        pSurf2d->SetWidth(userdSize * channelCount);
        pSurf2d->SetPitch(userdSize * channelCount);
        pSurf2d->SetHeight(1);
        pSurf2d->SetColorFormat(ColorUtils::Y8);
        pSurf2d->SetLocation(memLoc);
        pSurf2d->SetAlignment(pGpuDev->GetSubdevice(0)->GetUserdAlignment());
        pSurf2d->SetGpuCacheMode(Surface2D::GpuCacheOff);

        if (Platform::IsPhysFunMode() && (memLoc == Memory::Fb))
        {
            // Force USERD to be allocated from TOP to save the reserved VMMU
            // segment. Comments above vfSetting.fbReserveSize in
            // VmiopMdiagElwManager::ParseVfConfigFile() have more details.
            //
            // Host only can directly access PA below 2^40. There is a risk to allocate
            // PA above 2^40 after forcing PA reverse allocation. But now we don't have
            // any product supporting more than 1T vidmem, and also Host should fix
            // the limitation after 1T vidmem configuration is supported. So it looks
            // it's safe to force PA reverse flag here.
            pSurf2d->SetPaReverse(true);
        }

        // Only need a physical allocation for USERD no need to get a GPU VA
        // for it.
        pSurf2d->SetSpecialization(Surface2D::PhysicalOnly);

        RC rc;
        CHECK_RC(pSurf2d->Alloc(pGpuDev, pLwRm));

        UINT32 subdevMapCount = pGpuDev->GetNumSubdevices();
        UINT32 flags = 0;
        // If the memory is not in FB, then simply map the memory since the
        // same memory is shared between subdevices only a single CPU pointer
        // is needed
        //
        // Ensure that the mapping is forced to direct (it is invalid to map
        // USERD reflected)
        if (memLoc != Memory::Fb)
        {
            // This is a WAR until mapping sysmem via the below map memory
            // call can be debugged.  Surface2D seems to work
            pSurf2d->SetMemoryMappingMode(Surface2D::MapDirect);
            CHECK_RC(pSurf2d->Fill(0));
            CHECK_RC(pSurf2d->Map());
            pAddresses->push_back(pSurf2d->GetAddress());
            return rc;
        }

        // For memory that resides in FB, need to manually map the memory on
        // each subdevice since there needs to be a different CPU pointer per
        // subdevice that points to the memory on the specific subdevice
        // (surface 2D doesnt directly support multiple simultaneous CPU
        // mappings on different to different subdevices)
        for (UINT32 subdevInst = 0; subdevInst < subdevMapCount; subdevInst++)
        {
            void *pAddr;
            CHECK_RC(pSurf2d->Fill(0, subdevInst));
            CHECK_RC(pLwRm->MapMemory(pSurf2d->GetMemHandle(),
                                      0,
                                      pSurf2d->GetSize(),
                                      &pAddr,
                                      flags,
                                      pGpuDev->GetSubdevice(subdevInst)));
            pAddresses->push_back(pAddr);
        }
        return rc;
    }

    //--------------------------------------------------------------------------
    //! \brief Free USERD memory
    //!
    //! \param pSurf2d      : Pointer surface to free USERD memory on
    //! \param addresses    : CPU addresses that were mapped on the USERD space
    //! \param pLwRm        : RM client to use for the free
    void FreeUserdMemory
    (
        Surface2D            *pSurf2D
       ,const vector<void *> &addresses
       ,LwRm                 *pLwRm
    )
    {
        if (pSurf2D->GetLocation() != Memory::Fb)
        {
            pSurf2D->Unmap();
        }
        else
        {
            for (size_t subdevInst = 0;
                 subdevInst < addresses.size();
                 subdevInst++)
            {
                pLwRm->UnmapMemory(pSurf2D->GetMemHandle(),
                             addresses[subdevInst],
                             0,
                             pSurf2D->GetGpuDev()->GetSubdevice(static_cast<UINT32>(subdevInst)));
            }
        }

        if (pSurf2D->IsAllocated())
        {
            pSurf2D->Free();
        }
    }

    //--------------------------------------------------------------------------
    //! \brief Class which encasulates a GPFIFO channel USERD space allocation
    //! that is managed as part of the common USERD allocation
    //!
    class UserdAllocManaged : public UserdAlloc
    {
    public:
        UserdAllocManaged
        (
             UserdManager         *pManager
            ,LwRm::Handle          memHandle
            ,UINT64                offset
            ,UINT32                userdSize
            ,const vector<void *> &cpuAddresses
        );
        virtual ~UserdAllocManaged();

        virtual RC Alloc(Memory::Location loc);
        virtual RC Setup(LwRm::Handle hCh) { return OK; }
        virtual void Free();
        LwRm::Handle   GetMemHandle() const;
        virtual UINT64 GetMemOffset(UINT32 subdevInst) const;
        virtual void * GetAddress(UINT32 subdevInst) const;
        virtual Memory::Location GetLocation() const;
    private:
        UserdManager     *m_pManager;     //!< Pointer to UserdManager (used for
                                          //!< freeing the allocation)
        LwRm::Handle      m_MemHandle;    //!< Memory handle of the allocation
        UINT64            m_Offset;       //!< Offset within the memory where
                                          //!< the USERD allocation starts
        UINT32            m_UserdSize;    //!< Size of the USERD space
        vector<void *>    m_CpuAddresses; //!< CPU addresses of the USERD space
        Memory::Location  m_Location;     //!< Location of the USERD space
    };

    //--------------------------------------------------------------------------
    //! \brief Class which encasulates a GPFIFO channel USERD space allocation
    //! that is seperate from the managed USERD space
    //!
    class UserdAllocCreated : public UserdAlloc
    {
    public:
        UserdAllocCreated(UINT32 gpFifoClass, GpuDevice *pDev, LwRm *pLwRm);
        virtual ~UserdAllocCreated();
        virtual RC Alloc(Memory::Location loc);
        virtual RC Setup(LwRm::Handle hCh) { return OK; }
        virtual void Free();
        LwRm::Handle   GetMemHandle() const;
        virtual UINT64 GetMemOffset(UINT32 subdevInst) const;
        virtual void * GetAddress(UINT32 subdevInst) const;
        virtual Memory::Location GetLocation() const;

    private:
        UINT32            m_GpFifoClass;  //!< GPFIFO class for the USERD space
        GpuDevice        *m_pGpuDev;      //!< Gpu device where the USERD space
                                          //!< was allocated
        LwRm             *m_pLwRm;        //!< RM client where the USERD space
                                          //!< was allocated
        UINT32            m_UserdSize;    //!< Size of USERD space
        Surface2D         m_UserdMemory;  //!< Memory for the USERD space
        vector<void *>    m_CpuAddresses; //!< CPU addresses of the USERD space
    };

    //--------------------------------------------------------------------------
    //! \brief Class which encasulates a GPFIFO channel USERD space allocation
    //! that is created and managed by RM
    //!
    class UserdAllocRm : public UserdAlloc
    {
    public:
        UserdAllocRm(UINT32 gpFifoClass, GpuDevice *pDev, LwRm *pLwRm);
        virtual ~UserdAllocRm();
        virtual RC Alloc(Memory::Location loc) { return OK; }
        virtual RC Setup(LwRm::Handle hCh);
        virtual void Free();
        LwRm::Handle   GetMemHandle() const { return 0; }
        virtual UINT64 GetMemOffset(UINT32 subdevInst) const { return 0; }
        virtual void * GetAddress(UINT32 subdevInst) const;
        virtual Memory::Location GetLocation() const { return m_Location; }
    private:
        UINT32            m_GpFifoClass;  //!< GPFIFO class for the USERD space
        GpuDevice        *m_pGpuDev;      //!< Gpu device where the USERD space
                                          //!< was allocated
        LwRm             *m_pLwRm;        //!< RM client where the USERD space
                                          //!< was allocated
        vector<void *>    m_CpuAddresses; //!< CPU addresses of the USERD space
        Memory::Location  m_Location;     //!< Memory location of USERD space
    };
};

//--------------------------------------------------------------------------
//! \brief Constructor
//!
UserdAllocManaged::UserdAllocManaged
(
     UserdManager         *pManager
    ,LwRm::Handle          memHandle
    ,UINT64                offset
    ,UINT32                userdSize
    ,const vector<void *> &cpuAddresses
)
 :   m_pManager(pManager)
    ,m_MemHandle(memHandle)
    ,m_Offset(offset)
    ,m_UserdSize(userdSize)
    ,m_CpuAddresses(cpuAddresses)
    ,m_Location(Memory::Optimal)
{
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
/* virtual */ UserdAllocManaged::~UserdAllocManaged()
{
    Free();
}

//------------------------------------------------------------------------------
//! \brief Allocate the managed USERD space
//!
//! \param loc : Memory location of the managed USERD memory
//!
//! \return OK
/* virtual */ RC UserdAllocManaged::Alloc(Memory::Location loc)
{
    m_Location = loc;
    string allocString =
        Utility::StrPrintf("UserdAllocManaged : Successfully allocated USERD :\n"
                           "    Actual Location : %d\n"
                           "    Address(es)     : ",
                           m_Location);
    for (size_t i = 0; i < m_CpuAddresses.size(); i++)
    {
        if ((i != 0) && (i % 4) == 0)
        {
            allocString += "\n                      ";
        }
        allocString += Utility::StrPrintf("0x%llx ",
                                          reinterpret_cast<UINT64>(m_CpuAddresses[i]));
    }
    Printf(Tee::PriLow, s_ModsUserdModule.GetCode(), "%s\n",
           allocString.c_str());
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Free the managed USERD space
//!
//! \param loc : Memory location of the managed USERD memory
/* virtual */ void UserdAllocManaged::Free()
{
    // Ensure that double frees do not occur
    if (m_Offset != ~0ULL)
    {
        Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
               "UserdAllocManaged : Freeing USERD space\n");
        m_pManager->Release(m_Offset);
        m_Offset = ~0ULL;
    }
}

//------------------------------------------------------------------------------
//! \brief Get the memory handle of the USERD space
//!
//! \return Memory handle of the managed USERD space
/* virtual */ LwRm::Handle UserdAllocManaged::GetMemHandle() const
{
    MASSERT(m_Offset != ~0ULL);
    return m_MemHandle;
}

//------------------------------------------------------------------------------
//! \brief Get the offset from the start of the memory represented by the memory
//! handle for the USERD memory of the specified subdevice
//!
//! \param subdevInst : Subdevice instance to get the memory offset for
//!
//! \return Offset from the start of the memory represented by the memory
//! handle for the USERD memory of the specified subdevice
/* virtual */ UINT64 UserdAllocManaged::GetMemOffset(UINT32 subdevInst) const
{
    MASSERT(m_Offset != ~0ULL);
    UINT64 offset = m_Offset;

    // For non-FB USERD allocations the USERD for each subdevice resides in the
    // same system memory allocation offset by the subdevice instance.  For FB
    // allocations all subdevices reside at the same offset (since RM
    // automatically mirrors FB memory allocations across all subdevices)
    if (m_Location != Memory::Fb)
        offset += m_UserdSize * subdevInst;
    return offset;
}

//------------------------------------------------------------------------------
//! \brief Get CPU address of USERD memory for the specified subdevice
//!
//! \param subdevInst : Subdevice instance to get the CPU address for
//!
//! \return CPU address of USERD memory for the specified subdevice
/* virtual */ void * UserdAllocManaged::GetAddress(UINT32 subdevInst) const
{
    MASSERT(m_Offset != ~0ULL);
    UINT08 *pAddr;

    // For non-FB USERD allocations the USERD for each subdevice resides in the
    // same system memory allocation offset by the subdevice instance.  For FB
    // allocations all subdevices reside at the same offset (since RM
    // automatically mirrors FB memory allocations across all subdevices)
    if (m_Location != Memory::Fb)
    {
        MASSERT(m_CpuAddresses.size() == 1);

        // Sysmem only has a single mapping
        pAddr = static_cast<UINT08 *>(m_CpuAddresses[0]);
        pAddr += m_UserdSize * subdevInst;
    }
    else
    {
        MASSERT(subdevInst < m_CpuAddresses.size());
        pAddr = static_cast<UINT08 *>(m_CpuAddresses[subdevInst]);
    }
    return pAddr;
}

//------------------------------------------------------------------------------
//! \brief Get the memory location of the USERD memory
//!
//! \return Memory location of the USERD memory
/* virtual */ Memory::Location UserdAllocManaged::GetLocation() const
{
    MASSERT(m_Offset != ~0ULL);
    return m_Location;
}

//--------------------------------------------------------------------------
//! \brief Constructor
//!
UserdAllocCreated::UserdAllocCreated
(
     UINT32     gpFifoClass
    ,GpuDevice *pDev
    ,LwRm      *pLwRm
)
 :   m_GpFifoClass(gpFifoClass)
    ,m_pGpuDev(pDev)
    ,m_pLwRm(pLwRm)
    ,m_UserdSize(0)
{
    m_UserdMemory.SetName("_Userd");
}

//--------------------------------------------------------------------------
//! \brief Destructor
//!
/* virtual */ UserdAllocCreated::~UserdAllocCreated()
{
    Free();
}

//------------------------------------------------------------------------------
//! \brief Allocate the created USERD space
//!
//! \param loc : Memory location of the created USERD memory
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC UserdAllocCreated::Alloc(Memory::Location loc)
{
    RC rc;
    CHECK_RC(GetGpFifoChannelInfo(m_pGpuDev,
                                  m_pLwRm,
                                  &m_GpFifoClass,
                                  &m_UserdSize,
                                  nullptr));
    if (loc != Memory::Fb)
    {
        m_UserdSize = max(m_UserdSize,
                          m_pGpuDev->GetSubdevice(0)->GetUserdAlignment());
    }
    Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
           "UserdAllocCreated : Allocating USERD space :\n"
           "    Class    : 0x%04x\n"
           "    Size     : %u\n"
           "    Location : %d\n",
           m_GpFifoClass,
           m_UserdSize,
           loc);
    CHECK_RC(AllolwserdMemory(&m_UserdMemory,
                              m_pGpuDev,
                              m_pLwRm,
                              1,
                              m_UserdSize,
                              loc,
                              &m_CpuAddresses));
    string allocString =
        Utility::StrPrintf("UserdAllocCreated : Successfully allocated USERD :\n"
                           "    Actual Location : %d\n"
                           "    Address(es)     : ",
                           m_UserdMemory.GetLocation());
    for (size_t i = 0; i < m_CpuAddresses.size(); i++)
    {
        if ((i != 0) && (i % 4) == 0)
        {
            allocString += "\n                           ";
        }
        allocString += Utility::StrPrintf("0x%llx ",
                                          reinterpret_cast<UINT64>(m_CpuAddresses[i]));
        Platform::MemSet(m_CpuAddresses[i], 0, m_UserdMemory.GetSize());
    }
    Printf(Tee::PriLow, s_ModsUserdModule.GetCode(), "%s\n",
           allocString.c_str());

    return rc;
}
//------------------------------------------------------------------------------
//! \brief Free the created USERD space
//!
/* virtual */ void UserdAllocCreated::Free()
{
    if (m_UserdMemory.IsAllocated())
    {
        Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
               "UserdAllocCreated : Freeing USERD space\n");
        FreeUserdMemory(&m_UserdMemory, m_CpuAddresses, m_pLwRm);
        m_CpuAddresses.clear();
        m_UserdSize = 0;
    }
}

//------------------------------------------------------------------------------
//! \brief Get the memory handle of the USERD space
//!
//! \return Memory handle of the created USERD space
/* virtual */ LwRm::Handle UserdAllocCreated::GetMemHandle() const
{
    MASSERT(m_UserdMemory.IsAllocated());
    return m_UserdMemory.GetMemHandle();
}

//------------------------------------------------------------------------------
//! \brief Get the offset from the start of the memory for the USERD memory of
//! the specified subdevice
//!
//! \param subdevInst : Subdevice instance to get the memory offset for
//!
//! \return Offset from the start of the memory for the USERD memory of the
//! specified subdevice
/* virtual */ UINT64 UserdAllocCreated::GetMemOffset(UINT32 subdevInst) const
{
    MASSERT(m_UserdMemory.IsAllocated());

    // For non-FB USERD allocations the USERD for each subdevice resides in the
    // same system memory allocation offset by the subdevice instance.  For FB
    // allocations all subdevices reside at offset 0 (since RM
    // automatically mirrors FB memory allocations across all subdevices)
    UINT64 offset = 0;
    if (GetLocation() != Memory::Fb)
    {
        offset = m_UserdSize * subdevInst;
    }
    return offset;
}

//------------------------------------------------------------------------------
//! \brief Get CPU address of USERD memory for the specified subdevice
//!
//! \param subdevInst : Subdevice instance to get the CPU address for
//!
//! \return CPU address of USERD memory for the specified subdevice
/* virtual */ void * UserdAllocCreated::GetAddress(UINT32 subdevInst) const
{
    MASSERT(m_UserdMemory.IsAllocated());

    // For non-FB USERD allocations the USERD for each subdevice resides in the
    // same system memory allocation offset by the subdevice instance.  For FB
    // allocations all subdevices reside at offset 0 (since RM
    // automatically mirrors FB memory allocations across all subdevices)
    UINT08 *pAddr;
    if (GetLocation() != Memory::Fb)
    {
        MASSERT(m_CpuAddresses.size() == 1);
        pAddr = static_cast<UINT08 *>(m_CpuAddresses[0]);
        pAddr += m_UserdSize * subdevInst;
    }
    else
    {
        MASSERT(subdevInst < m_CpuAddresses.size());
        pAddr = static_cast<UINT08 *>(m_CpuAddresses[subdevInst]);
    }
    return pAddr;
}

//------------------------------------------------------------------------------
//! \brief Get the memory location of the USERD memory
//!
//! \return Memory location of the USERD memory
/* virtual */ Memory::Location UserdAllocCreated::GetLocation() const
{
    MASSERT(m_UserdMemory.IsAllocated());
    return m_UserdMemory.GetLocation();
}

//--------------------------------------------------------------------------
//! \brief Constructor
//!
UserdAllocRm::UserdAllocRm
(
     UINT32     gpFifoClass
    ,GpuDevice *pDev
    ,LwRm      *pLwRm
)
 :   m_GpFifoClass(gpFifoClass)
    ,m_pGpuDev(pDev)
    ,m_pLwRm(pLwRm)
    ,m_Location(Memory::Optimal)
{
}

//--------------------------------------------------------------------------
//! \brief Destructor
//!
/* virtual */ UserdAllocRm::~UserdAllocRm()
{
    Free();
}

//------------------------------------------------------------------------------
//! \brief Setup the USERD space if necessary
//!
//! \param Channel handle for the USERD space
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC UserdAllocRm::Setup(LwRm::Handle hCh)
{
    RC rc;
    UINT32 userdSize;
    CHECK_RC(GetGpFifoChannelInfo(m_pGpuDev, m_pLwRm, &m_GpFifoClass, &userdSize, nullptr));

    for (UINT32 sub = 0; sub < m_pGpuDev->GetNumSubdevices(); sub++)
    {
        void *pAddr;
        CHECK_RC(m_pLwRm->MapMemory(hCh,
                                    0,
                                    userdSize,
                                    &pAddr,
                                    0,
                                    m_pGpuDev->GetSubdevice(sub)));
        m_CpuAddresses.push_back(pAddr);
    }

    if (m_pGpuDev->GetSubdevice(0)->IsSOC())
    {
        m_Location = Memory::NonCoherent;
        return rc;
    }
    else if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        m_Location = Memory::Fb;
        return rc;
    }

    LW2080_CTRL_CMD_FIFO_GET_USERD_LOCATION_PARAMS params = { };
    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pGpuDev->GetSubdevice(0),
                                         LW2080_CTRL_CMD_FIFO_GET_USERD_LOCATION,
                                         &params,
                                         sizeof(params)));
    if (params.aperture == LW2080_CTRL_CMD_FIFO_GET_USERD_LOCATION_APERTURE_VIDMEM)
    {
        // RM API is lacking for Volta FB can actually be coherent when
        // mapped through LwLink
        m_Location = Memory::Fb;
    }
    else if (params.attribute == LW2080_CTRL_CMD_FIFO_GET_USERD_LOCATION_ATTRIBUTE_CACHED)
    {
        // RM API is lacking, could be cached but unsnooped and therefore
        // be CachedNonCoherent
        m_Location = Memory::Coherent;
    }
    else
    {
        m_Location = Memory::NonCoherent;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Free the created USERD space
//!
/* virtual */ void UserdAllocRm::Free()
{
    if (!m_CpuAddresses.empty())
    {
        Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
               "UserdAllocRm : Freeing USERD allocation\n");
        m_CpuAddresses.clear();
    }
}

//------------------------------------------------------------------------------
//! \brief Get CPU address of USERD memory for the specified subdevice
//!
//! \param subdevInst : Subdevice instance to get the CPU address for
//!
//! \return CPU address of USERD memory for the specified subdevice
/* virtual */ void * UserdAllocRm::GetAddress(UINT32 subdevInst) const
{
    MASSERT(!m_CpuAddresses.empty());
    MASSERT(subdevInst < m_CpuAddresses.size());
    return static_cast<UINT08 *>(m_CpuAddresses[subdevInst]);
}

//--------------------------------------------------------------------------
//! \brief Constructor
//!
UserdManager::UserdManager(GpuDevice *pDev, UINT32 numChannels, Memory::Location loc)
 :  m_pGpuDev(pDev)
   ,m_bRmAllocatesUserd(false)
   ,m_NumChannels(numChannels)
   ,m_Location(loc)
   ,m_GpFifoClass(DEFAULT_GPFIFO_CLASS)
   ,m_UserdSize(0)
   ,m_AllocMutex(nullptr)
{
    m_UserdMemory.SetName("_Userd");
}

//--------------------------------------------------------------------------
//! \brief Destructor
//!
UserdManager::~UserdManager()
{
    if (OK != Free())
    {
        Printf(Tee::PriError, s_ModsUserdModule.GetCode(),
               "Failed to free USERD allocations");
    }
}

//------------------------------------------------------------------------------
//! \brief Allocate the memory for managed USERD space
//!
//! \return OK if successful, not OK otherwise
RC UserdManager::Alloc()
{
    RC rc;

    m_AllocMutex = Tasker::AllocMutex("UserdManager::m_AllocMutex", Tasker::mtxUnchecked);
    if (m_AllocMutex == nullptr)
    {
        Printf(Tee::PriError, "Unable to allocate USERD mutex\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // USERD space is allocated internally by RM on GPUs that do not support
    // user mode allocations (i.e. Volta+ doorbells)
    UsermodeAlloc userModeAlloc;
    if (!userModeAlloc.IsSupported(m_pGpuDev) || m_pGpuDev->GetSubdevice(0)->IsDFPGA())
    {
        m_bRmAllocatesUserd = true;
        return OK;
    }

    CHECK_RC(GetGpFifoChannelInfo(m_pGpuDev,
                                  LwRmPtr().Get(),
                                  &m_GpFifoClass,
                                  &m_UserdSize,
                                  &m_NumChannels));

    m_UserdSize = max(m_UserdSize, m_pGpuDev->GetSubdevice(0)->GetUserdAlignment());
    m_Location = Surface2D::GetActualLocation(m_Location,
        m_pGpuDev->GetSubdevice(0));
#ifdef SIM_BUILD
    m_Location = m_UserdMemory.CheckForIndividualLocationOverride(m_Location,
        m_pGpuDev->GetSubdevice(0));
#endif

    UINT32 rmInstLoc;
    if (OK == Registry::Read("ResourceManager", LW_REG_STR_RM_INST_LOC, &rmInstLoc))
    {
        Memory::Location rmUserdLoc = m_Location;
        switch (DRF_VAL(_REG_STR_RM, _INST_LOC, _USERD, rmInstLoc))
        {
            case LW_REG_STR_RM_INST_LOC_USERD_DEFAULT:
                break;
            case LW_REG_STR_RM_INST_LOC_USERD_COH:
                rmUserdLoc = Memory::Coherent;
                break;
            case LW_REG_STR_RM_INST_LOC_USERD_NCOH:
                rmUserdLoc = Memory::NonCoherent;
                break;
            case LW_REG_STR_RM_INST_LOC_USERD_VID:
                rmUserdLoc = Memory::Fb;
                break;
        }

        // It is valid (although likely unintentional) for the MODS USERD location
        // and RM USERD location to differ (MODS core JS files all act to keep it
        // in sync)
        if (rmUserdLoc != m_Location)
        {
            Printf(Tee::PriWarn,
                   "USERD memory location mismatch between RM and MODS.\n"
                   "  RM   : %s\n"
                   "  MODS : %s\n"
                   "Ensure that GpuDevice.UserdLocation is used to control USERD location!\n",
                   Memory::GetMemoryLocationName(rmUserdLoc),
                   Memory::GetMemoryLocationName(m_Location));
        }
    }

    if (m_NumChannels == 0)
    {
        Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
               "UserdManager : No preallocated USERD space requested\n");
        return rc;
    }

    Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
           "UserdManager : Allocating USERD space :\n"
           "    Channels : %u\n"
           "    Class    : 0x%04x\n"
           "    Size     : %u\n"
           "    Location : %d\n",
           m_NumChannels,
           m_GpFifoClass,
           m_UserdSize,
           m_Location);
    CHECK_RC(AllolwserdMemory(&m_UserdMemory,
                              m_pGpuDev,
                              LwRmPtr().Get(),
                              m_NumChannels,
                              m_UserdSize,
                              m_Location,
                              &m_CpuAddresses));
    string allocString =
        Utility::StrPrintf("UserdManager : Successfully allocated USERD :\n"
                           "    Actual Location : %d\n",
                           m_UserdMemory.GetLocation());

    PHYSADDR physAddr;
    if (OK == m_UserdMemory.GetPhysAddress(0, &physAddr, LwRmPtr().Get()))
    {
        allocString += Utility::StrPrintf("    Phys address    : 0x%llx\n", physAddr);
    }
    allocString += "    Cpu Address(es) : ";

    for (size_t i = 0; i < m_CpuAddresses.size(); i++)
    {
        if ((i != 0) && (i % 4) == 0)
        {
            allocString += "\n                      ";
        }
        allocString += Utility::StrPrintf("0x%llx ",
                                          reinterpret_cast<UINT64>(m_CpuAddresses[i]));
    }
    Printf(Tee::PriLow, s_ModsUserdModule.GetCode(), "%s\n",
           allocString.c_str());

    for (UINT32 chIdx = 0; chIdx < m_NumChannels; chIdx++)
    {
        UINT64 offset = m_UserdSize * static_cast<UINT64>(chIdx);
        if (m_UserdMemory.GetLocation() != Memory::Fb)
            offset *= m_pGpuDev->GetNumSubdevices();
        m_FreeOffsets.push(offset);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Free the memory for managed USERD space
//!
//! \return OK if successful, not OK otherwise
RC UserdManager::Free()
{
    StickyRC rc;
    if (m_UserdMemory.IsAllocated())
    {
        if (static_cast<UINT32>(m_FreeOffsets.size()) != m_NumChannels)
        {
            Printf(Tee::PriError, s_ModsUserdModule.GetCode(),
                   "Freeing USERD memory with outstanding allocations!\n");
            rc = RC::SOFTWARE_ERROR;
        }

        Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
               "UserdManager : Freeing USERD space\n");
        FreeUserdMemory(&m_UserdMemory, m_CpuAddresses, LwRmPtr().Get());

        queue<UINT64> empty;
        swap(m_FreeOffsets, empty);
    }
    m_CpuAddresses.clear();

    if (m_AllocMutex)
    {
        Tasker::FreeMutex(m_AllocMutex);
        m_AllocMutex = nullptr;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Create a new unmanaged USERD space
//!
//! \param ppUserd     : Pointer to returned UserdAllocPtr
//! \param gpFifoClass : GPFIFO class to allocate the USERD on
//! \param loc         : Memory location for the allocation
//! \param pLwRm       : Pointer to RM client for the allocation RM
//!
//! \return OK if successful, not OK otherwise
RC UserdManager::Create
(
    UserdAllocPtr     *ppUserd
    ,UINT32            gpFifoClass
    ,Memory::Location  loc
    ,LwRm             *pLwRm
)
{
    if (m_bRmAllocatesUserd)
    {
        Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
               "UserdManager : Creating new RM managed USERD allocation\n");
        ppUserd->reset(new UserdAllocRm(gpFifoClass, m_pGpuDev, pLwRm));
        return OK;
    }

    Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
           "UserdManager : Creating new USERD allocation :\n"
           "    Class    : 0x%04x\n"
           "    Location : %d\n",
           gpFifoClass,
           loc);
    ppUserd->reset(new UserdAllocCreated(gpFifoClass, m_pGpuDev, pLwRm));
    return (*ppUserd)->Alloc(loc);
}

//------------------------------------------------------------------------------
//! \brief Get a managed USERD space
//!
//! \param ppUserd     : Pointer to returned UserdAllocPtr
//! \param gpFifoClass : GPFIFO class to get a USERD for
//! \param pLwRm       : Pointer to RM client for the allocation RM
//!
//! \return OK if successful, not OK otherwise
RC UserdManager::Get(UserdAllocPtr *ppUserd, UINT32 gpFifoClass, LwRm *pLwRm)
{
    MASSERT(m_AllocMutex);
    Tasker::MutexHolder lock(m_AllocMutex);

    bool bCreateNew = false;
    if (m_bRmAllocatesUserd)
    {
        bCreateNew = true;
    }
    else if (!m_UserdMemory.IsAllocated())
    {
        Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
               "UserdManager : Creating a new USERD alloc due to no preallocated space\n");
        bCreateNew = true;
    }
    else if (m_FreeOffsets.empty())
    {
        Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
               "UserdManager : Creating a new USERD alloc, no preallocated entries remain\n");
        bCreateNew = true;
    }
    else
    {
        if ((gpFifoClass != DEFAULT_GPFIFO_CLASS) && (gpFifoClass != m_GpFifoClass))
        {
            Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
                   "UserdManager : Creating a new USERD alloc due to class mismatch\n");
            bCreateNew = true;
        }

        if (pLwRm != LwRmPtr().Get())
        {
            Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
                   "UserdManager : Creating a new USERD alloc due to LwRm pointer mismatch\n");
            bCreateNew = true;
        }
    }

    if (bCreateNew)
    {
        return Create(ppUserd, gpFifoClass, m_Location, pLwRm);
    }

    Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
           "UserdManager : Getting new USERD allocation at offset %llu\n",
           m_FreeOffsets.front());

    // Add the starting offset to the CPU addresses provided to the managed
    // allocation
    vector<void *> offsetAddresses(m_CpuAddresses.size());
    for (size_t ii = 0; ii < m_CpuAddresses.size(); ii++)
    {
        offsetAddresses[ii] =
            static_cast<UINT08 *>(m_CpuAddresses[ii]) + m_FreeOffsets.front();
        if (m_UserdMemory.GetLocation() == Memory::Fb)
            Platform::MemSet(offsetAddresses[ii], 0, m_UserdSize);
    }
    if (m_UserdMemory.GetLocation() != Memory::Fb)
    {
        Platform::MemSet(offsetAddresses[0], 0, m_UserdSize *m_pGpuDev->GetNumSubdevices());
    }

    ppUserd->reset(new UserdAllocManaged(this,
                                         m_UserdMemory.GetMemHandle(),
                                         m_FreeOffsets.front(),
                                         m_UserdSize,
                                         offsetAddresses));
    m_FreeOffsets.pop();
    return (*ppUserd)->Alloc(m_UserdMemory.GetLocation());
}

//------------------------------------------------------------------------------
//! \brief Release a managed USERD space
//!
//! \param offset : Offset in the managed area to release
//!
//! \return OK
RC UserdManager::Release(UINT64 offset)
{
    MASSERT(m_AllocMutex);
    Tasker::MutexHolder lock(m_AllocMutex);
    Printf(Tee::PriLow, s_ModsUserdModule.GetCode(),
           "UserdManager : Releasing USERD allocation at offset %llu\n",
           offset);
    m_FreeOffsets.push(offset);
    return OK;
}
