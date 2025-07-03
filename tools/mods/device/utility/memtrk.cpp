/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "device/include/memtrk.h"
#include "core/include/platform.h"
#include "lwtypes.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "cheetah/include/logmacros.h"
#include "cheetah/include/tegradrf.h"
#include "core/include/tasker.h"
#include <string.h>
#include <ctype.h>

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "MemoryTracker"

MTInstance::MTInstance()
{
   m_pAllocList.clear();
   m_Descriptor2Va.clear();
   m_MinAddr = 0;
   m_MaxAddr = 0;
   m_IsReservd = false;
   m_IsChecked = false;
   m_64PhysLimit = ~(UINT64)0;
   m_pMutex = Tasker::AllocMutex("MTMutex", Tasker::mtxLast);
   m_MTLwstomCfgMap.clear();
}

MTInstance::~MTInstance()
{
    if (!m_Descriptor2Va.empty())
    {
       Printf(Tee::PriError, "MTInstance destructor called while some allocations remain un-freed!\n");
       Printf(Tee::PriError, "Call CheckAndFree() and check for memory leak\n");
       // don't call platform memory de-alloc functions since s_MemoryMapping might has been destroyed
    }

    // release reserved memory if exists
    list<void *>::iterator resIter = m_ResvdList.begin();

    for (; resIter != m_ResvdList.end(); ++resIter)
    {
        Printf(Tee::PriDebug, "free resvd virt " PRINT_PHYS "\n", PRINT_FMT_PTR(*resIter));
        free(*resIter);
    }

    m_ResvdList.clear();
    m_NodeMem.clear();

    Tasker::FreeMutex(m_pMutex);
    m_pMutex = nullptr;
}

void MTInstance::CheckAndFree()
{
    if (!m_Descriptor2Va.empty())
    {
       Printf(Tee::PriWarn, "Some allocations remain un-freed. Check for memory leak!\n");
       FreeAllBuffer();
       m_Descriptor2Va.clear();
    }
}

RC MTInstance::InitRsvd
(
    const UINT64 Min,
    const UINT64 Max
)
{
    Printf(Tee::PriHigh, "min 0x%llx , max 0x%llx\n", Min, Max);

    if ((Min == Max) && (Min == 0))
    {
        return OK;
    }

    if ((Min == Max) || (Min > Max))
    {
        Printf(Tee::PriError, "Min address can't be greater than max address\n");
        return RC::BAD_PARAMETER;
    }

    m_MinAddr = Min;
    m_MaxAddr = Max;

    if (m_MaxAddr < m_MinAddr)
    {
        Printf(Tee::PriError, "Address range is wrong\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    return RsvdAddr();
 }

RC MTInstance::RsvdAddr()
{
    LOG_ENT();
    RC rc = RC::CANNOT_ALLOCATE_MEMORY;

    map<void *, PHYSADDR> allocAddrMap;
    INT32 allocSize = 0x10000000;// 256M
    UINT32 totalSize = 0;

    while (allocSize)
    {
        void *p = NULL;
        p = malloc(allocSize); //virtual addr only for dos.
        if (!p)
        {
            if (allocSize > 0x80000)
                allocSize -= 0x80000; // 512k
            else
                allocSize -= 0x2000;// 8k

            continue;
        }

        UINT64 phys = Platform::VirtualToPhysical(p);

        UINT64 end = phys + allocSize;
        if (end <= m_MinAddr  || phys >= m_MaxAddr)
        {
            allocAddrMap[p] = phys;
            continue;
        }

        Printf(Tee::PriDebug, "\nfind  phys_start " PRINT_PHYS " phys_end " PRINT_PHYS " "
                              "virt addr " PRINT_PHYS " size 0x%x\n",
               phys, phys + allocSize, PRINT_FMT_PTR(p), allocSize);

        MemArea area;
        UINT64 realStart = (phys >= m_MinAddr) ? phys : m_MinAddr;
        UINT64 realEnd = (end < m_MaxAddr) ? end : m_MaxAddr;
        area.start = realStart;
        area.size = realEnd - realStart;
        totalSize += area.size;
        Printf(Tee::PriDebug, "saved phys " PRINT_PHYS " end " PRINT_PHYS " size 0x%x \n",
               realStart, realStart + area.size, area.size);
        area.lwrStart = realStart;
        m_NodeMem.push_back(area);
        m_ResvdList.push_back(p);
        if (totalSize == m_MaxAddr - m_MinAddr)
            break;
     }

    //free
    Printf(Tee::PriDebug, "free temporary list\n");
    map<void *, PHYSADDR>::iterator freeIter = allocAddrMap.begin();
    for (;freeIter != allocAddrMap.end(); ++freeIter)
    {
        Printf(Tee::PriDebug, "free virt " PRINT_PHYS ", phys " PRINT_PHYS "\n",
               PRINT_FMT_PTR(freeIter->first), freeIter->second);
        free(freeIter->first);
    }
    allocAddrMap.clear();

    if (m_NodeMem.empty())
    {
        Printf(Tee::PriError, "Can't find requested physcial address range\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    Printf(Tee::PriHigh, "found:\n");
    UINT64 minPhys = ~0ULL;
    UINT64 maxPhys = 0;
    list<MemArea>::iterator resIter = m_NodeMem.begin();
    for (; resIter != m_NodeMem.end(); resIter++)
    {
        Printf(Tee::PriHigh, "phys " PRINT_PHYS ", virt " PRINT_PHYS ", size 0x%x\n",
               PRINT_FMT_PTR(resIter->start), Platform::VirtualToPhysical((void*)resIter->start),
             resIter->size);
        if (resIter->start > maxPhys)
            maxPhys = resIter->start;

        if (resIter->start < minPhys)
            minPhys = resIter->start;
    }

    Printf(Tee::PriHigh, "MemArea founded: min phys " PRINT_PHYS " , max phys " PRINT_PHYS "\n",
           minPhys, maxPhys);

    CHECK_RC(Platform::SetMemRange(minPhys, maxPhys - minPhys + Xp::GetPageSize(), Memory::UC));

    m_IsReservd = true;
    return OK;
}

RC MTInstance::AllocNode
(
    const size_t Size,
    void** ppReturnedVA
)
{
    LOG_ENT();
    void *p = NULL;
    list <MemArea>::iterator nodeIter = m_NodeMem.begin();

    for (int i = 0; nodeIter !=  m_NodeMem.end(); nodeIter++, i++)
    {
        UINT64 remain = nodeIter->start + nodeIter->size - nodeIter->lwrStart;
        if (remain < Size)
            continue;
        //found
        Printf(Tee::PriDebug, "Node %d phys " PRINT_PHYS  " size 0x%x \n",
               i,  nodeIter->lwrStart, (UINT32)Size);
        p = (void *)Platform::PhysicalToVirtual(nodeIter->lwrStart);
        nodeIter->lwrStart = nodeIter->lwrStart + Size;
        *ppReturnedVA = p;
        return OK;
    }

    LOG_EXT();
    Printf(Tee::PriError, "Can't allocate memory\n");
    return RC::CANNOT_ALLOCATE_MEMORY;
}

UINT64 MTInstance::GetMTCfgId(const Controller *pCtrl)
{
    return reinterpret_cast<UINT64>(pCtrl);
}

MTPciDevInfo* MTInstance::GetMTPciDevInfo(const Controller *pCtrl)
{
    UINT64 ctrlMtId = 0;
    if (pCtrl == nullptr)
    {
        return nullptr;
    }
    ctrlMtId = GetMTCfgId(pCtrl);
    if(m_MTLwstomCfgMap.find(ctrlMtId) != m_MTLwstomCfgMap.end())
    {
        return &m_MTLwstomCfgMap[ctrlMtId];
    }
    else
    {
        return nullptr;
    }
}


RC MTInstance::AllocRsvd(size_t NumBytes, void ** ppReturnedVA, UINT32 Align)
{
        LOG_ENT();
        const UINT32 pageSize = Xp::GetPageSize();
        UINT32 size = (NumBytes + pageSize - 1)  & ~(pageSize -1);
        if (0 != (Align & (Align-1)))
        {
            Printf(Tee::PriHigh, "PhysAlign must be a power of 2,\n");
            LOG_ENT();
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        void *pVA = NULL;
        AllocNode(size + Align, &pVA);
        if (!pVA)
        {
            Printf(Tee::PriError, "Can't alloc remain buffer\n");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        UINT64 phys = Platform::VirtualToPhysical(pVA);
        if (Align)
        {
            phys = (phys + Align - 1) & ~(Align - 1);
            pVA = Platform::PhysicalToVirtual(phys);
        }

        AllocList alloc;
        alloc.m_pBaseVirtAddr = pVA;
        alloc.m_BasePhysAddr = phys;
        alloc.m_pDescriptor = pVA;
        alloc.m_size = NumBytes;
        alloc.m_IsReservd = true;
        m_Descriptor2Va[pVA] = pVA;
        m_pAllocList.push_back(alloc);

        *ppReturnedVA = pVA;
        Printf(Tee::PriDebug, "Reserved. AddToAllocList(desc "
                              "" PRINT_PHYS ", virt " PRINT_PHYS ",  phy " PRINT_PHYS " size 0x%.8x)\n",
               PRINT_FMT_PTR(pVA),
               PRINT_FMT_PTR(pVA),
               PRINT_FMT_PTR(alloc.m_BasePhysAddr),
               alloc.m_size );

        InitializePageFrameList(--m_pAllocList.end(), false);
        LOG_ENT();
        return OK;
}

/*
An example for /proc/iomem

00000000-0009fbff : System RAM
  00000000-00000000 : Crash kernel
0009fc00-0009ffff : reserved
000a0000-000bffff : Video RAM area
000c0000-000cddff : Video ROM
000ce000-000cefff : Adapter ROM
000cf000-000d07ff : Adapter ROM
000f0000-000fffff : System ROM
00100000-dbf9ffff : System RAM
  00400000-00624d67 : Kernel code
  00624d68-0073a563 : Kernel data
dbfa0000-dbfadfff : ACPI Tables
dbfae000-dbfdffff : ACPI Non-volatile Storage
...
...
...
fee01000-feefffff : pnp 00:07
fefe0000-fefe01ff : pnp 00:07
fefe1000-fefe1fff : pnp 00:07
100000000-10fffffff : 0000:00:10.0

*/

RC MTInstance::Check64bitSetting()
{
    LOG_ENT();

    // Only run this function under Linux
    if ((Xp::GetOperatingSystem() != Xp::OS_LINUX) &&
        (Xp::GetOperatingSystem() != Xp::OS_ANDROID))
    {
        Printf(Tee::PriError, "Please choose Linux to test 64bit capability\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
     }
    //check maxAddress in /proc/iomem
    if (m_IsChecked)
        return OK;

    FILE  *fp = fopen("/proc/iomem", "r");
    if (!fp)
    {
        Printf(Tee::PriError, "Failed to open /proc/iomem\n");
        return RC::CANNOT_OPEN_FILE; // return immediately
    }

    char  memStr[128];
    string str;

    while (1)
    {
        memset(memStr, 0, sizeof(memStr));

        if (fgets(memStr, 128,fp))
        {
            str += memStr;
            continue;
        }
            break;
    }

    Printf(Tee::PriDebug, "/proc/iomem:\n%s\n", str.c_str());
    string::size_type index = 0;
    UINT64 maxAddr = 0;
    while (1)
    {
         index = str.find('-', index + 1);
         if (index == string::npos)
            break;

        index++;
        if (!isxdigit(str[index]))
            continue;

        string::size_type end = str.find_first_of(' ', index);;
        if (end == string::npos)
            break;

        string digitStr = str.substr(index, end - index);
        UINT64 addr = Utility::Strtoull(digitStr.c_str(),NULL, 16);
        if (addr > maxAddr)
            maxAddr = addr;
    }
    fclose(fp);
    fp = NULL;
    Printf(Tee::PriHigh, "/proc/iomem MaxPhyAddr: 0x%llx\n", maxAddr);
    if ((maxAddr >> 30)  < 4)
    { // if less than 4G, prompt to add more memory
        Printf(Tee::PriWarn, "64 bit memory allocation needs at least 4G memory installed\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
        // continue to check mem= option
    }

    m_64PhysLimit = maxAddr;
    m_IsChecked = true;
    return OK;
}

RC MTInstance::Check64bitAddr(UINT64 PhyAddr)
{
    // 4G < PhyAddr < limit is needed
    if ((PhyAddr <= m_64PhysLimit) && (PhyAddr >> 32))
        return OK;

    Printf(Tee::PriHigh, "found wrong phyAddress:0x%llx not in the range [4G, %llx]\n",
           PhyAddr, m_64PhysLimit);
    return RC::CANNOT_ALLOCATE_MEMORY;
}

RC MTInstance::AllocBuffer
(
    const size_t NumBytes,
    void** ppReturnedVA,
    const bool Contiguous,
    UINT32 AddressBits,
    const Memory::Attrib Attrib,
    Controller *pCtrl
)
{
    LOG_ENT();

    RC rc;
    void* pDescriptor = nullptr;
    void* pVA = nullptr;
    *ppReturnedVA = nullptr;
    PHYSADDR pa = 0x00;

    DEFER
    {
        if (pVA)
        {
            Platform::UnMapPages(pVA);
        }
        if (pDescriptor)
        {
            Platform::FreePages(pDescriptor);
        }
    };

    if (0 == NumBytes)
    {
        Printf(Tee::PriError, "Can't allocate memory of 0 Byte\n");
        return RC::BAD_PARAMETER;
    }

    Tasker::MutexHolder lock(m_pMutex);

    MTPciDevInfo* pPciInfo = GetMTPciDevInfo(pCtrl);

    if (m_IsReservd)
    {
        CHECK_RC(AllocRsvd(NumBytes, &pVA));
        *ppReturnedVA = pVA;
        LOG_EXT();
        return OK;
    }

    if (AddressBits == 64 && !m_IsChecked)
    {
        if (OK != Check64bitSetting())
        {
            Printf(Tee::PriLow, "Colwerting 64bit request to 32 bit address request\n");
            AddressBits = 32;
        }
    }

    if (pPciInfo && pPciInfo->isUsePciDev)
    {
        CHECK_RC(Platform::AllocPages(NumBytes, &pDescriptor, Contiguous, AddressBits, Attrib,
                             pPciInfo->domain, pPciInfo->bus, pPciInfo->device, pPciInfo->function));
        if (Xp::CanEnableIova(pPciInfo->domain, pPciInfo->bus, pPciInfo->device, pPciInfo->function))
        {
            CHECK_RC(Platform::DmaMapMemory(pPciInfo->domain, pPciInfo->bus, pPciInfo->device, pPciInfo->function, pDescriptor));
        }
    }
    else
    {
        CHECK_RC(Platform::AllocPages(NumBytes, &pDescriptor, Contiguous, AddressBits, Attrib, Platform::UNSPECIFIED_GPUINST));
    }
    MASSERT(pDescriptor);

    CHECK_RC(Platform::MapPages(&pVA, pDescriptor, 0 /* offset */, NumBytes, Memory::ReadWrite));
    MASSERT(pVA);

    pa = Platform::GetPhysicalAddress(pDescriptor, 0 /* offset */);
    MASSERT(pa);

#ifndef INCLUDE_PEATRANS
    if (IsIommuEnabled(pCtrl))
    {
        MemDescInfo desc;
        string smmuDevName = pCtrl->GetSmmuDevName();
        PHYSADDR iova = 0;
        CHECK_RC(Xp::IommuDmaMapMemory(pDescriptor, smmuDevName, &iova));
        desc.DevName = smmuDevName;
        desc.Iova = iova;
        m_MemDescMap[pDescriptor] = desc;
        Printf(Tee::PriDebug, "MTInstance::AllocBuffer: pBufferStart = %p, \
                pDescriptor = %p, isSmmuMap = %d, smmuName = %s, iova = 0x%llx\n",
                pVA, pDescriptor, true, smmuDevName.c_str(), iova);
    }
#endif

    // casting NumBytes to unsigned because the length modifer 'z' isn't recognized
    Printf(Tee::PriDebug, "allocate %d bytes at va " PRINT_PHYS " and pa " PRINT_PHYS " \n",
           (unsigned int)NumBytes,  PRINT_FMT_PTR(pVA), pa);
    m_Descriptor2Va[pDescriptor] = pVA;

    *ppReturnedVA = pVA;
    CHECK_RC(AddToAllocList(pDescriptor, pVA, static_cast<UINT32>(NumBytes)));
    pVA = nullptr;
    pDescriptor = nullptr;

    LOG_EXT();
    return OK;
}

RC MTInstance::AllocBufferAligned
(
    const size_t NumBytes,
    void** ppReturnedVA,
    const size_t PhysAlign,
    UINT32 AddressBits,
    const Memory::Attrib Attrib,
    Controller *pCtrl
)
{
    LOG_ENT();
    RC rc;
    void *pDescriptor = NULL;
    PHYSADDR pa = 0x00;
    void *pVA = NULL;
    *ppReturnedVA = NULL;

    if (0 == NumBytes)
    {
        Printf(Tee::PriError, "Can't allocate memory of 0 Byte\n");
        return RC::BAD_PARAMETER;
    }

    Tasker::MutexHolder lock(m_pMutex);

    MTPciDevInfo* pPciInfo = GetMTPciDevInfo(pCtrl);

    if (m_IsReservd)
    {
        CHECK_RC(AllocRsvd(NumBytes, &pVA, PhysAlign));
        *ppReturnedVA = pVA;
        LOG_EXT();
        return OK;
    }

    if (AddressBits == 64 && !m_IsChecked)
    {
        if (OK != Check64bitSetting())
        {
            Printf(Tee::PriLow, "Colwerting 64bit Address request to 32 bit address request\n");
            AddressBits = 32;
        }
    }

    if (pPciInfo && pPciInfo->isUsePciDev)
    {
        CHECK_RC(Platform::AllocPages(NumBytes, &pDescriptor, true, AddressBits, Attrib,
                             pPciInfo->domain, pPciInfo->bus, pPciInfo->device, pPciInfo->function));
        if (Xp::CanEnableIova(pPciInfo->domain, pPciInfo->bus, pPciInfo->device, pPciInfo->function))
        {
            CHECK_RC(Platform::DmaMapMemory(pPciInfo->domain, pPciInfo->bus, pPciInfo->device, pPciInfo->function, pDescriptor));
        }
    }
    else
    {
        CHECK_RC(Platform::AllocPagesAligned(NumBytes,
                                             &pDescriptor,
                                             PhysAlign,
                                             AddressBits,
                                             Attrib,
                                             Platform::UNSPECIFIED_GPUINST));
    }
    MASSERT(pDescriptor);

    CHECK_RC(Platform::MapPages(&pVA,
                                pDescriptor,
                                0 /* offset */,
                                NumBytes,
                                Memory::ReadWrite));
    MASSERT(pVA);

    pa = Platform::GetPhysicalAddress( pDescriptor, 0 /* offset */);
    MASSERT(pa);

#ifndef INCLUDE_PEATRANS
    if (pCtrl && IsIommuEnabled(pCtrl))
    {
        MemDescInfo desc;
        string smmuDevName = pCtrl->GetSmmuDevName();
        PHYSADDR iova = 0;
        CHECK_RC(Xp::IommuDmaMapMemory(pDescriptor, smmuDevName, &iova));
        desc.DevName = smmuDevName;
        desc.Iova    = iova;
        m_MemDescMap[pDescriptor] = desc;
        Printf(Tee::PriDebug, "MTInstance::AllocBufferAligned: pBufferStart = %p, \
                pDescriptor = %p, isSmmuMap =%d, smmuName=%s, iova = 0x%llx\n",
                pVA, pDescriptor, true, smmuDevName.c_str(), iova);
    }
#endif

    // casting NumBytes and PhysAlign to unsigned because the length modifer 'z' isn't recognized
    Printf(Tee::PriDebug, "allocate %d bytes at va " PRINT_PHYS " and pa " PRINT_PHYS " (aligned to 0x%0x)\n",
                           (unsigned int)NumBytes, PRINT_FMT_PTR(pVA), pa, (unsigned int)PhysAlign);
    m_Descriptor2Va[pDescriptor] = pVA;

    *ppReturnedVA = pVA;
    CHECK_RC(AddToAllocList(pDescriptor, pVA, static_cast<UINT32>(NumBytes)));
    LOG_EXT();
    return OK;
}

void MTInstance::FreeBuffer
(
    void* pReturnedVA
)
{
   FreePartialBuffer(pReturnedVA);
}

// Using Descriptor as an argument
void MTInstance::FreeBufferUseDescriptor
(
    void* pDescriptor,
    const bool IsResvd
)
{
   if(m_Descriptor2Va.find(pDescriptor) == m_Descriptor2Va.end())
   {
       Printf(Tee::PriError, "Error:: Invalid descriptor " PRINT_PHYS "\n", PRINT_FMT_PTR(pDescriptor));
       return;
   }

   void *pVA = m_Descriptor2Va[pDescriptor];
   Printf(Tee::PriDebug, "freeing descriptor " PRINT_PHYS "\n", PRINT_FMT_PTR(pDescriptor));
   Printf(Tee::PriDebug, "freeing va " PRINT_PHYS " \n", PRINT_FMT_PTR(pVA));

#ifndef INCLUDE_PEATRANS
   auto itr = m_MemDescMap.find(pDescriptor);
   if (itr != m_MemDescMap.end())
   {
       MemDescInfo desc = m_MemDescMap[pDescriptor];
       Xp::IommuDmaUnmapMemory(pDescriptor, desc.DevName);
       m_MemDescMap.erase(itr);
       Printf(Tee::PriDebug, "MTInstance::FreeBufferUseDescriptor: \
                pBufferStart = %p, pDescriptor = %p, ioSmmuMap = %d\n",
                pVA, pDescriptor, true);
   }
#endif

   if (IsResvd == false)
   {
        Platform::UnMapPages( pVA );
        Platform::FreePages( pDescriptor );
   }

   m_Descriptor2Va.erase(pDescriptor);
}

// Description: Create unique structure with Descriptor, BaseVirtAddr and Size and
//              Insert it at the end of the list.
// Look At    : AllocBuffer, AllocBufferAligned, RemoveFromAllocList
RC MTInstance::AddToAllocList
(
    void* pDescriptor,
    void* pBaseVirtAddr,
    const UINT32 Size
)
{
    Printf(Tee::PriDebug, "AddToAllocList(" PRINT_PHYS ", " PRINT_PHYS ", 0x%.8x)\n",
           PRINT_FMT_PTR(pDescriptor), PRINT_FMT_PTR(pBaseVirtAddr), Size);

    AllocList newList = {0};

    newList.m_pDescriptor    = pDescriptor;
    newList.m_size           = Size;
    newList.m_pBaseVirtAddr  = pBaseVirtAddr;

    m_pAllocList.push_back(newList);
    InitializePageFrameList(--m_pAllocList.end());

    return OK;
}

// Description: Callwlate Virtual Page to Physical Page Mapping and insert it into vector.
//              Assumption:
//                 *Virtual Pages should be contigous.
//                 *Used Platform::VirtualToPhysical to get Physical Address from Virtual Address
//                 *AllocList structure variable m_BaseVirtAddr, m_size, m_Descriptor should be
//                  correctly programmed
//
//              *Handles contigous/noncontigous Physical Pages.
//              *Handles Aligned/nonaligned Physical/Virtual Pages.
//
// Look At    : AddToAllocList, RemoveFromAllocList, Platform::VirtualToPhysical

RC MTInstance::InitializePageFrameList
(
    list<AllocList>::iterator ListIter,
    const bool IsVerbose
)
{
    UINT32   size_covered = 0x00;
    void    *pLwrVirtAddr = ListIter->m_pBaseVirtAddr;

    const LwUPtr pageSize = Xp::GetPageSize();
    const LwUPtr pageMask = ~(pageSize - 1);

    //Initial Setup
    ListIter->m_PageFrames.clear();
    ListIter->m_BasePhysAddr = Platform::VirtualToPhysical((void *) pLwrVirtAddr);

    while(size_covered < ListIter->m_size)
    {
        PHYSADDR lwrPhysAddr = Platform::VirtualToPhysical((void *)pLwrVirtAddr);
        MASSERT(lwrPhysAddr); // Assuming assertion if it is zero.
        ListIter->m_PageFrames.push_back(lwrPhysAddr);

        if (IsVerbose)
            Printf(Tee::PriDebug, " Virtual Address " PRINT_PHYS " maps to Physical Address " PRINT_PHYS " \n",
                   PRINT_FMT_PTR(pLwrVirtAddr), lwrPhysAddr );

        UINT32 offset = ((LwUPtr)pLwrVirtAddr & pageMask) + pageSize - LwUPtr(pLwrVirtAddr);
        size_covered = size_covered + offset;
        pLwrVirtAddr = (UINT08 *)pLwrVirtAddr + offset;
    }

    return OK;
}

// Description: Verify If It's Valid Virtual Address.
//              Return boolean type
// Look At    : IsValidVirtualAddr
bool MTInstance::IsValidVirtualAddress
(
    void *pVirtAddr,
    bool IsSuppressErr
)
{
    if(IsValidVirtualAddr(pVirtAddr, nullptr, IsSuppressErr) == OK)
        return true;

    return false;
}

// Description: Verify If It's Valid Virtual Address.
//              Return the pointer to the List Iterator Position.
//              Private Function.
// Look At    : RemoveFromAllocList, IsValidVirtualAddress
RC MTInstance::IsValidVirtualAddr(void * pVirtAddr, list<AllocList>::iterator *pListIter, bool IsSuppressErr)
{
   Tasker::MutexHolder lock(m_pMutex);

   if (m_pAllocList.empty())
   {
       if ( !IsSuppressErr )
       {
           Printf(Tee::PriError, "IsValidVirtualAddr(" PRINT_PHYS "): no elements in theAllocList \n",
                  PRINT_FMT_PTR(pVirtAddr));
       }
       return RC::SOFTWARE_ERROR;
   }

   list<AllocList>::iterator listIter;
   listIter = m_pAllocList.begin();

   while(listIter !=  m_pAllocList.end())
   {
       void *pBaseVirtAddr  = listIter->m_pBaseVirtAddr;
       void *pLimitVirtAddr = (char *)pBaseVirtAddr + listIter->m_size;

       if( (pBaseVirtAddr <= pVirtAddr) && (pVirtAddr < pLimitVirtAddr) )
       {
           Printf(Tee::PriDebug, "IsValidVirtualAddr(" PRINT_PHYS "):: TRUE  \n",
                  PRINT_FMT_PTR(pVirtAddr));
           if(pListIter != NULL)
               *pListIter = listIter;
           return OK;
       }
       ++listIter;
   }

   if ( !IsSuppressErr )
   {
       Printf(Tee::PriError, "IsValidVirtualAddr(" PRINT_PHYS "):: FALSE  \n",
              PRINT_FMT_PTR(pVirtAddr));
   }
   return RC::ILWALID_ADDRESS;
}

// Description: Verify If It's Valid Physical Address.
//              Return boolean type
// Look At    : IsValidPhysicalAddr
bool MTInstance::IsValidPhysicalAddress(PHYSADDR PhysAddr)
{
    if(IsValidPhysicalAddr(PhysAddr) == OK)
        return true;

    return false;
}

// Description: Verify If It's Valid Physical Address.
//              Return Valid Virtual Address
//              Similar to Platform::PhysicalToVirtual
// Look At    : IsValidVirtualAddr,
// Note       : Recommended To Use This function in place of Platform::PhysicalToVirtual.
void* MTInstance::PhysicalToVirtual
(
    const PHYSADDR PhysAddr
)
{
    void *pVirtAddr = NULL;
    if(IsValidPhysicalAddr(PhysAddr, &pVirtAddr) == OK)
    {
        return pVirtAddr;
    }

    return NULL;
}

// Description: Verify If It's Valid Physical Address.
//              Return Virtual Address Mapped To Physical Address.
//              Private Function.
// Look At    : IsValidPhysicalAddress, PhysicalToVirtual
RC MTInstance::IsValidPhysicalAddr
(
    const PHYSADDR PhysAddr,
    void** ppVirtAddr
)
{
   Tasker::MutexHolder lock(m_pMutex);

   if (m_pAllocList.empty())
   {
       Printf(Tee::PriError, "IsValidPhysicalAddr(" PRINT_PHYS "): no elements in theAllocList \n",
              PhysAddr);
       return RC::SOFTWARE_ERROR;
   }

   const UINT32 pageSize = Xp::GetPageSize();
   const PHYSADDR pageMask = ~static_cast<PHYSADDR>(pageSize - 1);

   list<AllocList>::iterator listIter;
   listIter = m_pAllocList.begin();

   while( listIter !=  m_pAllocList.end())
   {
       if(listIter->m_PageFrames.empty())
       {
           ++listIter;
           Printf(Tee::PriError, " This case should not come with present logic \n");
           continue;
       }

       UINT32 size_covered = 0;

       vector<PHYSADDR>::iterator vectorIter;
       vectorIter = listIter->m_PageFrames.begin();

       while( vectorIter !=  listIter->m_PageFrames.end())
       {
           UINT32 size_left = listIter->m_size - size_covered;
           PHYSADDR limitPhysAddr = 0;

           if(size_left >= Xp::GetPageSize())
               limitPhysAddr = (*vectorIter & pageMask) + pageSize;
           else
               limitPhysAddr = *vectorIter + size_left;

           if( ( PhysAddr < (limitPhysAddr)) && (PhysAddr >= *vectorIter ))
           {
               UINT32 offset = PhysAddr - *vectorIter;
               if(ppVirtAddr != NULL)
               {
                   *ppVirtAddr = (char *)listIter->m_pBaseVirtAddr + size_covered + offset; //Got the VAddr
               }
               Printf(Tee::PriDebug, "IsValidPhysicalAddr( " PRINT_PHYS " )\n", PhysAddr);
               if (ppVirtAddr != NULL)
               {
                   Printf(Tee::PriDebug, " Mapped To  Virtual Address " PRINT_PHYS "\n",
                          PRINT_FMT_PTR(*ppVirtAddr));
               }
               return OK;
           }
           else
           {
               size_covered += (*vectorIter & pageMask) + pageSize - *vectorIter;
               ++vectorIter;
           }
       }//while( vectorIter !=  listIter->m_PageFrames.end())

       ++listIter;
   }//while( listIter !=  m_pAllocList.end())

   Printf(Tee::PriError, "IsValidPhysicalAddr( " PRINT_PHYS " ) doesn't Mapped to Any Virtual Address: FALSE\n",
          PhysAddr);

   return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
}

// Description: Verify if Unique Descriptor to Buffer Is present in Allocatin List
// Look At    : FreeAllBuffer, FreePartialBuffer
bool MTInstance::IsDescriptorInAllocList
(
    void *pDescriptor
)
{
   Printf(Tee::PriDebug, " IsDescriptorInAllocList (" PRINT_PHYS ") \n", PRINT_FMT_PTR(pDescriptor ));
   list<AllocList>::iterator listIter;

   listIter = m_pAllocList.begin();
   while(listIter != m_pAllocList.end())
   {
       if(listIter->m_pDescriptor == pDescriptor)
          return true;

       ++listIter;
   }

   return false;
}

// Description: Free All Buffer Based on Descriptor
//              Check for descriptor is required as avoid unnecessary call to FreeBuffer function.
// Look At    : IsDescriptorInAllocList, FreeBufferUseDescriptor
void MTInstance::FreeAllBuffer()
{
   Tasker::MutexHolder lock(m_pMutex);

   list<AllocList>::iterator listIter = m_pAllocList.begin();

   while( listIter != m_pAllocList.end())
   {
       void *pDescriptor = listIter->m_pDescriptor;
       listIter->m_PageFrames.clear();
       bool isRsvd =  listIter->m_IsReservd;

       list<AllocList>::iterator removeIter = listIter++;
       m_pAllocList.erase(removeIter);

       if(!IsDescriptorInAllocList(pDescriptor))
       {
           FreeBufferUseDescriptor(pDescriptor, isRsvd);
       }
   }
   m_pAllocList.clear();
   Printf(Tee::PriDebug, " FreeAllBuffer() - Exit \n");
}

// Description: Remove Partial Allocation from Allocation List.
//              Calls FreeBufferUseDescriptor Only if that descriptor is not present in Alloc List
// Look At    : RemoveFromAllocList

RC MTInstance::FreePartialBuffer
(
    void* pVirtAddr,
    const UINT32 Size
)
{
    RC rc;
    Printf(Tee::PriDebug, "FreePartialBuffer(" PRINT_PHYS ", 0x%08x) \n",
                           PRINT_FMT_PTR(pVirtAddr),
                           Size); //--debug

    Tasker::MutexHolder lock(m_pMutex);

    if (m_pAllocList.empty())
    {
        Printf(Tee::PriError, "FreePartialBuffer(" PRINT_PHYS ", 0x%08x): Trying to "
                              "free unallocated structure \n",
                               PRINT_FMT_PTR(pVirtAddr), Size);

        return RC::SOFTWARE_ERROR;
    }

    void* pDescriptor = NULL;
    bool isRsvd = false;
    CHECK_RC(RemoveFromAllocList(pVirtAddr, &pDescriptor, Size, &isRsvd));

    // Descriptor might be same in two different Alloc List Node.
    // (If both of them created by one Platform::AllocBuffer Call)
    if(!IsDescriptorInAllocList(pDescriptor))
    {
        FreeBufferUseDescriptor(pDescriptor, isRsvd);
    }

    return OK;
}

// Description: Remove Partial Allocation from Allocation List.
// Look At    : FreePartialBuffer, IsValidVirtualAddr

RC MTInstance::RemoveFromAllocList
(
    void* pVirtAddr,
    void** ppDescriptor,
    UINT32 Size,
    bool *pIsResvd
)
{
    Printf(Tee::PriDebug, "RemoveFromAllocList(" PRINT_PHYS ", 0x%08x) \n",
                           PRINT_FMT_PTR(pVirtAddr), Size);

    if (m_pAllocList.empty())
    {
        Printf(Tee::PriError, "RemoveFromAllocList(" PRINT_PHYS ", 0x%08x): "
                              "Trying to free unallocated structure \n",
                               PRINT_FMT_PTR(pVirtAddr), Size);

        return RC::SOFTWARE_ERROR;
    }

    list<AllocList>::iterator listIter;

    if(IsValidVirtualAddr(pVirtAddr, &listIter)!= OK)
    {
        Printf(Tee::PriError, "RemoveFromAllocList(" PRINT_PHYS ", 0x%08x) not found in theAllocList \n",
                                                    PRINT_FMT_PTR(pVirtAddr), Size);
        return RC::ILWALID_ADDRESS;
    }

    if(listIter != m_pAllocList.end()) // If Virtual Address is valid and it's not at the end of list.
    {
        if(!Size) //If Size=0 it means just delete the whole block where Virtual Address lies.
        {
            pVirtAddr = listIter->m_pBaseVirtAddr;
            Size     = listIter->m_size;
        }

        if (listIter->m_IsReservd && pIsResvd)
        {
            *pIsResvd = true;
        }

        void *pBaseVirtAddr =  listIter->m_pBaseVirtAddr;
        UINT32 size = listIter->m_size;
        void *pLimitVirtAddr = (char *)pBaseVirtAddr + size;
        void *pDescriptor = listIter->m_pDescriptor;

        // If size argument is greater than the actual size value in node.
        if((pBaseVirtAddr <= ((char *)pVirtAddr + Size)) && ( ((char *)pVirtAddr + Size) <= pLimitVirtAddr) )
        {
            Printf(Tee::PriDebug, "RemoveFromAllocList(" PRINT_PHYS ", 0x%08x) \n",
                   PRINT_FMT_PTR(pVirtAddr), Size);
            if((pBaseVirtAddr == pVirtAddr) && (size == Size)) // remove complete node.
            {
                listIter->m_PageFrames.clear();
                m_pAllocList.erase(listIter);
                if(ppDescriptor)
                    *ppDescriptor = pDescriptor;
                return OK;
            }
            else // need to either break up this element, or modify bounds
            {
                if(pBaseVirtAddr == pVirtAddr) // modify bounds, base changed
                {
                    Printf(Tee::PriDebug, "RemoveFromAllocList(" PRINT_PHYS ", 0x%08x) existing "
                                          "from (" PRINT_PHYS ", 0x%08x)\n",
                           PRINT_FMT_PTR(pVirtAddr), Size,  PRINT_FMT_PTR(pBaseVirtAddr), size);

                    listIter->m_size -= Size;
                    listIter->m_pBaseVirtAddr = (char *)listIter->m_pBaseVirtAddr + Size;

                    InitializePageFrameList(listIter);
                    if(ppDescriptor)
                        *ppDescriptor = pDescriptor;
                    return OK;
                }
                else if (((char *)pVirtAddr + Size) == pLimitVirtAddr) // modify bounds, limit changed
                {
                    Printf(Tee::PriDebug, "RemoveFromAllocList(" PRINT_PHYS ", 0x%08x) existing "
                                          "from (" PRINT_PHYS ", 0x%08x)\n",
                           PRINT_FMT_PTR(pVirtAddr), Size,  PRINT_FMT_PTR(pBaseVirtAddr), size);

                    listIter->m_size -= Size;

                    InitializePageFrameList(listIter);  // It will clear the page frame and reinitialize everything.
                    if(ppDescriptor)
                        *ppDescriptor = pDescriptor;

                    return OK;
                }
                else // break up the node
                {

                     UINT32 sizeOfFirstBlock = (LwUPtr)pVirtAddr - (LwUPtr)listIter->m_pBaseVirtAddr;
                     UINT32 sizeOfSecondBlock = listIter->m_size - (sizeOfFirstBlock + Size);

                     AddToAllocList(listIter->m_pDescriptor, listIter->m_pBaseVirtAddr, sizeOfFirstBlock);
                     AddToAllocList(listIter->m_pDescriptor, ((char *)pVirtAddr + Size), sizeOfSecondBlock);

                     Printf(Tee::PriDebug, "RemoveFromAllocList(" PRINT_PHYS ", 0x%08x): "
                                           "Created two new list elements with (" PRINT_PHYS ", 0x%08x) and (" PRINT_PHYS ", 0x%08x)\n",
                            PRINT_FMT_PTR(pVirtAddr), Size,
                            PRINT_FMT_PTR(listIter->m_pBaseVirtAddr), sizeOfFirstBlock,
                            PRINT_FMT_PTR((char *)pVirtAddr + Size), sizeOfSecondBlock);

                     if(ppDescriptor)
                          *ppDescriptor = pDescriptor;
                     listIter->m_PageFrames.clear();
                     m_pAllocList.erase(listIter);

                     return OK;

                } // break up the node
            }// if( baseVirtAddr == pVirtAddr),  node modification
        } // error check
        else
        {
            Printf(Tee::PriError, "RemoveFromAllocList(" PRINT_PHYS ", 0x%08x) : busted "
                                   "implementation: should fit in (" PRINT_PHYS ", 0x%.8x)\n",
                   PRINT_FMT_PTR(pVirtAddr), Size,
                   PRINT_FMT_PTR(pBaseVirtAddr), size);

            return RC::MEMORY_SIZE_MISMATCH;
        }

    }// if(listIter != m_pAllocList.end())

    Printf(Tee::PriError, "RemoveFromAllocList(" PRINT_PHYS ", 0x%08x) not found in theAllocList \n",
           PRINT_FMT_PTR(pVirtAddr), Size);

    return RC::ILWALID_ADDRESS;

}

// Description: Dump Allocation List Content
// Look At    :

void MTInstance::DumpAllocList()
{
   list<AllocList>::iterator listIter = m_pAllocList.begin();

   char s[100] = "-";
   const char *st = "----";

   while(listIter != m_pAllocList.end())
   {
       Printf(Tee::PriDebug, " %s Virtual Address  = " PRINT_PHYS " \n",
              s, PRINT_FMT_PTR(listIter->m_pBaseVirtAddr));
       Printf(Tee::PriDebug, " %s Physical Address = " PRINT_PHYS " \n",
              s, listIter->m_BasePhysAddr);
       Printf(Tee::PriDebug, " %s Size             = 0x%08x \n",
              s, listIter->m_size);
       Printf(Tee::PriDebug, " %s Descriptor       = " PRINT_PHYS "     \n",
              s, PRINT_FMT_PTR(listIter->m_pDescriptor));

       vector<PHYSADDR>::iterator vectorIter = listIter->m_PageFrames.begin();
       while( vectorIter != listIter->m_PageFrames.end())
       {
           Printf(Tee::PriDebug, " %s **** Page Frames = " PRINT_PHYS " \n", s, *vectorIter);
           ++vectorIter;
       }
       strcat(s,st);
       ++listIter;
   }
}

RC MTInstance::GetPhysLimit(PHYSADDR *pPhysLimit)
{
    RC rc = OK;      
    CHECK_RC(Check64bitSetting());
    *pPhysLimit = m_64PhysLimit;
    return rc;
}

RC MTInstance::SetIsAllocFromPcieNode(Controller *pCtrl, bool Flag)
{
    RC rc = OK;
    Tasker::MutexHolder lock(m_pMutex);
    if (pCtrl == nullptr)
    {
        return RC::BAD_PARAMETER;
    }
    if (Flag)
    {
        struct SysUtil::PciInfo pciInfo;
        rc = pCtrl->GetPciInfo(&pciInfo);
        if (rc == OK)
        {
            UINT64 ctrlMtId = GetMTCfgId(pCtrl);
            MTPciDevInfo mtDevInfo;
            mtDevInfo.domain = pciInfo.DomainNumber;
            mtDevInfo.bus = pciInfo.BusNumber;
            mtDevInfo.device = pciInfo.DeviceNumber;
            mtDevInfo.function = pciInfo.FunctionNumber;
            mtDevInfo.isUsePciDev = true;
            m_MTLwstomCfgMap[ctrlMtId] = mtDevInfo;
        }
    }
    else
    {
        UINT64 ctrlMtId = GetMTCfgId(pCtrl);
        std::map<UINT64, MTPciDevInfo>::iterator itr = m_MTLwstomCfgMap.find(ctrlMtId);
        if (itr != m_MTLwstomCfgMap.end())
        {
            m_MTLwstomCfgMap.erase(itr);
        }
    }
    return rc;
}

RC MTInstance::GetDescriptor(void *pVirtAddr, void** pDescriptor, UINT32 *pOffset)
{
    RC rc = OK;
    list<AllocList>::iterator itr;
    CHECK_RC(IsValidVirtualAddr(pVirtAddr, &itr));
    if (pDescriptor)
    {
        *pDescriptor = itr->m_pDescriptor;
        if (pOffset)
        {
            *pOffset = (UINT64)pVirtAddr - (UINT64)itr->m_pBaseVirtAddr;
        }
        return rc;
    }
    else
    {
        Printf(Tee::PriError, "m_pDescriptor is nullptr\n");
        return RC::BAD_PARAMETER;
    }
}

RC MTInstance::GetMappedPhysicalAddress(void * pVirtAddr, Controller *pCtrl, PHYSADDR * pPhysAddr)
{
    void * pDescriptor = nullptr;
    UINT32 offset = 0;
    RC rc = OK;
    CHECK_RC(GetDescriptor(pVirtAddr, &pDescriptor, &offset));
    if (pCtrl && pPhysAddr)
    {
        struct SysUtil::PciInfo pciInfo;
        CHECK_RC(pCtrl->GetPciInfo(&pciInfo));
        *pPhysAddr = Platform::GetMappedPhysicalAddress(pciInfo.DomainNumber, pciInfo.BusNumber, pciInfo.DeviceNumber, pciInfo.FunctionNumber, pDescriptor, offset);
        return OK;
    }
    else
        return RC::BAD_PARAMETER;
}

PHYSADDR MTInstance::VirtualToDmaAddress(void* pVirtAddr)
{
    PHYSADDR dmaAddr = 0;
    RC rc = OK;

    if(pVirtAddr == nullptr)
        return RC::BAD_PARAMETER;

    rc = VirtualToIommuIova(pVirtAddr, &dmaAddr);
    if (rc != OK)
    {
        dmaAddr = Platform::VirtualToPhysical(pVirtAddr);
    }

    return dmaAddr;
}

RC MTInstance::VirtualToIommuIova(void *pVirtAddr, PHYSADDR *pPhysAddr)
{
    void *pDescriptor = nullptr;
    UINT32 offset = 0;
    RC rc = OK;
    PHYSADDR dmaAddr = 0;

    if(pVirtAddr == nullptr || pPhysAddr == nullptr)
        return RC::BAD_PARAMETER;

    CHECK_RC(GetDescriptor(pVirtAddr, &pDescriptor, &offset));
    auto itr = m_MemDescMap.find(pDescriptor);
    if (itr != m_MemDescMap.end())
    {
       MemDescInfo desc = m_MemDescMap[pDescriptor];
       if (desc.Iova)
       {
           dmaAddr = desc.Iova + offset;
           *pPhysAddr = dmaAddr;
       }
    }
    return (dmaAddr == 0) ? RC::ILWALID_ADDRESS : rc;
}

RC MTInstance::SetIsIommuEnable(Controller *pCtrl, bool IsEn)
{
    DeviceMemProp memProp;
    memProp.IsIommuEnabled = IsEn;
    if (pCtrl != nullptr)
        m_MemPropertyMap[pCtrl] = memProp;
    Printf(Tee::PriLow, "MTInstance::SetIsIommuEnable(pCtrl=%p, Flag=%s)\n", pCtrl, IsEn ? "true" : "false");
    return OK;
}

bool MTInstance::IsIommuEnabled(Controller *pCtrl)
{
    bool isEnabled = false;
    auto itr = m_MemPropertyMap.find(pCtrl);
    if (itr != m_MemPropertyMap.end())
    {
        DeviceMemProp memProp = itr->second;
        isEnabled = memProp.IsIommuEnabled;
    }
    return isEnabled;
}

namespace MemoryTracker
{
    RC AllocBuffer(size_t NumBytes, void ** ppReturnedVA, bool Contiguous, UINT32 AddressBits, Memory::Attrib Attrib, Controller *pCtrl)
    {
        return  MTInstance::Instance().AllocBuffer(NumBytes, ppReturnedVA, Contiguous, AddressBits, Attrib, pCtrl);
    }

    RC AllocBufferAligned(size_t NumBytes, void ** ppReturnedVA, size_t PhysAlign, UINT32 AddressBits, Memory::Attrib Attrib, Controller *pCtrl)
    {
        return  MTInstance::Instance().AllocBufferAligned(NumBytes, ppReturnedVA, PhysAlign, AddressBits, Attrib, pCtrl);
    }

    RC FreeBuffer(void *pReturnedVA)
    {
        MTInstance::Instance().FreeBuffer(pReturnedVA);
        return OK;
    }

    bool IsValidVirtualAddress(void *pVirtAddr, bool IsSuppressErr)
    {
        return MTInstance::Instance().IsValidVirtualAddress(pVirtAddr, IsSuppressErr);
    }

    bool IsValidPhysicalAddress(PHYSADDR PhysAddr)
    {
        return MTInstance::Instance().IsValidPhysicalAddress(PhysAddr);
    }

    void* PhysicalToVirtual(PHYSADDR PhysAddr)
    {
        return MTInstance::Instance().PhysicalToVirtual(PhysAddr);
    }

    RC VirtualToPhysical(void* pVirtAddr, PHYSADDR* PhysAddr)
    {
        *PhysAddr = Platform::VirtualToPhysical(pVirtAddr);
        return OK;
    }

    RC FreePartialBuffer(void *pVirtAddr, UINT32 Size)
    {
        return MTInstance::Instance().FreePartialBuffer(pVirtAddr, Size);
    }

    void FreeAllBuffer()
    {
        MTInstance::Instance().FreeAllBuffer();
    }

    void DumpAllocList()
    {
        MTInstance::Instance().DumpAllocList();
    }

    RC  InitRsvd(UINT64 Min, UINT64 Max)
    {
        return MTInstance::Instance().InitRsvd(Min, Max);
    }

    void CheckAndFree()
    {
        return MTInstance::Instance().CheckAndFree();
    }

    RC GetPhysLimit(PHYSADDR *pPhysLimit)
    {
        RC rc = OK;      
        CHECK_RC(MTInstance::Instance().GetPhysLimit(pPhysLimit));
        return rc;
    }

    RC SetIsAllocFromPcieNode(Controller *pCtrl, bool Flag)
    {
        RC rc = OK;
        CHECK_RC(MTInstance::Instance().SetIsAllocFromPcieNode(pCtrl, Flag));
        return rc;
    }

    RC GetDescriptor(void *pVirtAddr, void** pDescriptor, UINT32 *pOffset)
    {
        return MTInstance::Instance().GetDescriptor(pVirtAddr, pDescriptor, pOffset);
    }

    RC GetMappedPhysicalAddress(void * pVirtAddr, Controller *pCtrl, PHYSADDR * pPhysAddr)
    {
        return MTInstance::Instance().GetMappedPhysicalAddress(pVirtAddr, pCtrl, pPhysAddr);
    }

    PHYSADDR VirtualToDmaAddress(void* pVirtAddr)
    {
        return MTInstance::Instance().VirtualToDmaAddress(pVirtAddr);
    }

    RC VirtualToIommuIova(void* pVirtAddr, PHYSADDR * pPhysAddr)
    {
        return MTInstance::Instance().VirtualToIommuIova(pVirtAddr, pPhysAddr);
    }

    RC SetIsIommuEnable(Controller *pCtrl, bool IsEn)
    {
        return MTInstance::Instance().SetIsIommuEnable(pCtrl, IsEn);
    }
}
