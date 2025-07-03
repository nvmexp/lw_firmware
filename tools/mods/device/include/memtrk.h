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

//------------------------------------------------------------------------------
// utility memory/buffer tracking class
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

/**
 * @file   memtrk.h
 * @brief  Utility class for allocating, tracking and freeing memory
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_MEMTRK_H
#define INCLUDED_MEMTRK_H

#include "core/include/memtypes.h"
#include "core/include/types.h"
#include "core/include/memfrag.h"
#include "cheetah/include/controller.h"
#ifdef INCLUDE_PEATRANS
#ifndef INCLUDED_PEATMEM_H
    #include "cheetah/peatrans/peatmem.h"
    #include "core/include/platform.h"
#endif
#endif
#include <map>
#include <list>
#include <vector>

typedef struct
{
    void* m_pBaseVirtAddr;
    PHYSADDR m_BasePhysAddr;  //Note- No need to have this variable.
    UINT32 m_size;
    void* m_pDescriptor;
    bool m_IsReservd;
    vector<PHYSADDR> m_PageFrames;
} AllocList;

typedef struct
{
    UINT64 start;
    UINT32 size;
    UINT64 lwrStart;
} MemArea;

typedef struct
{
    UINT32 domain;
    UINT32 bus;
    UINT32 device;
    UINT32 function;
    bool isUsePciDev;
} MTPciDevInfo;

typedef struct
{
    bool IsIommuEnabled;
} DeviceMemProp;

typedef struct
{
    string   DevName;
    PHYSADDR Iova;
} MemDescInfo;

//! @class (MTInstance) MemoryTracker singleton instance
//!
//! Singleton implementation access through MemoryTracker namespace
class MTInstance
{
public:
    static MTInstance& Instance()
    {
        static MTInstance instance;
        return instance;
    }

    ~MTInstance();

    // Deleted copy constructor & assignment operator to prevent from creating new object
    MTInstance(const MTInstance&) = delete;
    MTInstance& operator=(const MTInstance&) = delete;

    RC AllocBuffer
    (
        const size_t NumBytes,
        void** ppReturnedVA,
        const bool Contiguous,
        const UINT32 AddressBits,
        const Memory::Attrib,
        Controller *pCtrl = nullptr
    );

    RC AllocBufferAligned
    (
        const size_t NumBytes,
        void** ppReturnedVA,
        const size_t PhysAlign,
        const UINT32 AddressBits,
        const Memory::Attrib Attrib,
        Controller *pCtrl = nullptr
    );

    void FreeBuffer
    (
        void *pReturnedVA
    );

    bool IsValidVirtualAddress
    (
        void *pVirtAddr,
        bool IsSuppressErr = false
    );

    bool IsValidPhysicalAddress
    (
        const PHYSADDR PhysAddr
    );

    void* PhysicalToVirtual
    (
        const PHYSADDR PhysAddr
    );

    RC FreePartialBuffer
    (
        void *pVirtAddr,
        const UINT32 Size = 0
    );

    void FreeAllBuffer();
    void DumpAllocList();
    void CheckAndFree();

    RC RsvdAddr(void);
    RC InitRsvd
    (
        const UINT64 Min,
        const UINT64 Max
    );

    RC Check64bitSetting();
    RC Check64bitAddr(UINT64 PhyAddr);

    //! \brief Get the Largest Physical Address in CheetAh Address space
    //! \param [out] The resulting address
    RC GetPhysLimit(PHYSADDR *PhysLimit);
    /**
     * \brief Set whether to allocate from the pcie numa node for 
     *        pcie device
     * 
     * @param pCtrl [in] : pointer to controller
     * @param Flag [in]  : whether allocate from pcie node
     */
    RC SetIsAllocFromPcieNode(Controller *pCtrl, bool Flag);
    /**
     * \brief Get the memory descriptor pointer from a virtual 
     *        address
     * 
     * @param pVirtAddr   [in] : virtual address for an allocated 
     *                  buffer address
     * @param pDescriptor [out]: pointer to the descritpor 
     * @param pOffset     [out]: pointer to the offset to the 
     *                    descriptor's base virtual address
     */
    RC GetDescriptor(void *pVirtAddr, void** pDescriptor, UINT32 *pOffset);
    /**
     * \brief Get the pcie mapped physical address for the virtual 
     *        address
     * 
     * @param pVirtAddr [in] : virtual address of the allocated 
     *                  buffer
     * @param pCtrl     [in] : controller class pointer to get the 
     *              pci info
     * @param pPhysAddr [out]: mapped physical address for the 
     *                  virtual address and controller
     */
    RC GetMappedPhysicalAddress(void * pVirtAddr, Controller *pCtrl, PHYSADDR * pPhysAddr);
    /**
     * \brief Get Iommu address if mapped, else physical address
     * @param pVirtAddr [in]  : virtual address of the allocated buffer
     * @return                : returned dma address, it's iommu 
     *                          mapped address if iommu is mapped
     *                          for the buffer, else return physical
     *                          address
     */
    PHYSADDR VirtualToDmaAddress(void* pVirtAddr);
    /**
     * \brief Get Iommu mapped address if mapped, else return error
     * @param pVirtAddr [in]  : virtual address of the allocated buffer
     * @param pPhysAddr [out] : returned dma address, it's iommu mapped address 
    */
    RC VirtualToIommuIova(void* pVirtAddr, PHYSADDR * pPhysAddr);
    /*!
     * \brief Set smmu enable flag for the controller, the controller pointer is used as id
     * 
     * \param pCtrl [in] : controller pointer
     * \param IsEn [in] : smmu enable flag
     */
    RC SetIsIommuEnable(Controller *pCtrl, bool IsEn);

private:

    //! Private constructor, assignment operator, copy constructor to
    //! force all clients to go through MemoryTracker namespace.
    MTInstance();

    RC AddToAllocList
    (
        void* pDescriptor,
        void* pBaseVirtAddr,
        const UINT32 Size
    );

    RC InitializePageFrameList
    (
        const list<AllocList>::iterator ListIter,
        const bool IsVerbose = true
    );

    RC IsValidVirtualAddr(void *pVirtAddr, list<AllocList>::iterator *pListIter = NULL, bool IsSuppressErr = false);
    RC IsValidPhysicalAddr(PHYSADDR PhysAddr, void **ppVirtAddr = NULL);

    void FreeBufferUseDescriptor(void *pDescriptor, bool IsResvd = false);
    bool IsDescriptorInAllocList(void *pDescriptor);
    RC RemoveFromAllocList(void *pVirtAddr, void **ppDescriptor = NULL, UINT32 Size = 0x00, bool *pIsResvd = NULL);

    RC AllocRsvd(size_t NumBytes, void ** ppReturnedVA, UINT32 Align = 0);
    RC AllocNode(size_t Size, void ** ppReturnedVA);

    UINT64 GetMTCfgId(const Controller *pCtrl);
    MTPciDevInfo* GetMTPciDevInfo(const Controller *pCtrl);

    /*!
     * \brief Check whether smmu is enabled for the controller pointer
     * 
     * \param pCtrl [in] : Pointer to the controller
     */
    bool IsIommuEnabled(Controller *pCtrl);

    map<void*,void*> m_Descriptor2Va;

    list<AllocList> m_pAllocList;

    // for memtrk to allocate  any prdt address
    UINT64 m_MinAddr;
    UINT64 m_MaxAddr;
    bool m_IsReservd;

    list <MemArea> m_NodeMem;
    list<void *> m_ResvdList;

    // For Linux 64bit test check
    UINT64 m_64PhysLimit;
    bool   m_IsChecked;

    // For thread safety, we lock this mutex in primarily all functions which
    // access MemoryTracker namespace. This mutex is locked in AllocBuffer(),
    // AllocBufferAligned(), FreePartialBuffer() and FreeAllBuffer(). It
    // protects all the internal data structures of this singleton object.
    void* m_pMutex;
    //custom memory setting for per controller control
    std::map<UINT64, MTPciDevInfo> m_MTLwstomCfgMap;
    //! \var a map from pointer of virtual address to MemDescInfo,
    //! for free buffer from a virtual address
    std::map<void*, MemDescInfo> m_MemDescMap;
    //! \var a map from virtual address to DeviceMemProp,
    //! to save smmu enable flag for controller pointer, for allocating buffer
    std::map<void*, DeviceMemProp> m_MemPropertyMap;
};

#ifdef INCLUDE_PEATRANS
    //! Peatrans implementation.
    class MemoryTracker
    {
    public:
        inline static RC AllocBuffer(size_t NumBytes, void ** ppReturnedVA, bool Contiguous, UINT32 AddressBits, Memory::Attrib Attrib, Controller *pCtrl = nullptr)
        {
            return PeatMem::AllocAlignedMemory(NumBytes, reinterpret_cast<UINT32**>(ppReturnedVA));
        }

        inline static RC AllocBufferAligned(size_t NumBytes, void ** ppReturnedVA, size_t PhysAlign, UINT32 AddressBits, Memory::Attrib Attrib, Controller *pCtrl = nullptr)
        {
            return PeatMem::AllocAlignedMemory(NumBytes, reinterpret_cast<UINT32**>(ppReturnedVA), PhysAlign);
        }

        inline static RC FreeBuffer(void *pReturnedVA)
        {
            PeatMem::FreeMemory(static_cast<UINT32>(reinterpret_cast<size_t>(pReturnedVA)));
            return OK;
        }

        inline static bool  IsValidVirtualAddress(void *pVirtAddr, bool IsSuppressErr = false)
        {
            return PeatMem::IsAddressValid(static_cast<UINT32>(reinterpret_cast<size_t>(pVirtAddr)));
        }

        inline static bool  IsValidPhysicalAddress(PHYSADDR PhysAddr)
        {
            return PeatMem::IsAddressValid(static_cast<UINT32>(PhysAddr));
        }

        inline static void* PhysicalToVirtual(PHYSADDR PhysAddr)
        {
            return Platform::PhysicalToVirtual(PhysAddr);
        }

        inline static RC VirtualToPhysical(void* pVirtAddr, PHYSADDR* PhysAddr)
        {
            *PhysAddr = Platform::VirtualToPhysical(pVirtAddr);
            return OK;
        }

        inline static RC FreePartialBuffer(void *pVirtAddr, UINT32 Size = 0)
        {
            Printf(Tee::PriError, "Function not supported on Peatrans\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        inline static void  FreeAllBuffer()
        {
            PeatMem::FreeAll();
        }

        inline static void  DumpAllocList()
        {
            PeatMem::DumpAllocList();
        }

        inline static RC  InitRsvd(UINT64 Min, UINT64 Max)
        {
            Printf(Tee::PriError, "Function not supported on Peatrans\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        inline static void CheckAndFree()
        {
            PeatMem::FreeAll();
        }

        inline static RC SetIsAllocFromPcieNode(Controller *pCtrl, bool Flag)
        {
            Printf(Tee::PriError, "Function not supported on Peatrans\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
        inline static RC GetDescriptor(void *pVirtAddr, void** pDescriptor, UINT32 *pOffset)
        {
            Printf(Tee::PriError, "Function not supported on Peatrans\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
        inline static RC GetMappedPhysicalAddress(void * pVirtAddr, Controller *pCtrl, PHYSADDR * pPhysAddr)
        {
            Printf(Tee::PriError, "Function not supported on Peatrans\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
        inline static PHYSADDR VirtualToDmaAddress(void* pVirtAddr)
        {
            Printf(Tee::PriError, "Function not supported on Peatrans\n");
            return 0;
        }
        inline static RC GetIommuIova(void* pVirtAddr, PHYSADDR * pPhysAddr)
        {
            Printf(Tee::PriError, "Function not supported on Peatrans\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    };
#else
//! Regular Implementation for Silicon
namespace MemoryTracker
{
    RC AllocBuffer(size_t NumBytes, void ** ppReturnedVA, bool Contiguous, UINT32 AddressBits, Memory::Attrib Attrib, Controller *pCtrl = nullptr);
    RC AllocBufferAligned(size_t NumBytes, void ** ppReturnedVA, size_t PhysAlign, UINT32 AddressBits, Memory::Attrib Attrib, Controller *pCtrl = nullptr);
    RC FreeBuffer(void *pReturnedVA);
    bool IsValidVirtualAddress(void *pVirtAddr, bool IsSuppressErr = false);
    bool IsValidPhysicalAddress(PHYSADDR PhysAddr);
    void* PhysicalToVirtual(PHYSADDR PhysAddr);
    RC VirtualToPhysical(void* pVirtAddr, PHYSADDR* PhysAddr);
    RC FreePartialBuffer(void *pVirtAddr, UINT32 Size = 0);
    void FreeAllBuffer();
    void DumpAllocList();
    RC  InitRsvd(UINT64 Min, UINT64 Max);
    void CheckAndFree();
    RC GetPhysLimit(PHYSADDR *pPhysLimit);
    RC SetIsAllocFromPcieNode(Controller *pCtrl, bool Flag);
    RC GetDescriptor(void *pVirtAddr, void** pDescriptor, UINT32 *pOffset);
    RC GetMappedPhysicalAddress(void * pVirtAddr, Controller *pCtrl, PHYSADDR * pPhysAddr);
    RC SetIsIommuEnable(Controller *pCtrl, bool Flag);
    PHYSADDR VirtualToDmaAddress(void* pVirtAddr);
    RC VirtualToIommuIova(void* pVirtAddr, PHYSADDR * pPhysAddr);
}

#endif  // NON-PEATRANS builds
#endif  // !INCLUDED_MEMTRK_H
