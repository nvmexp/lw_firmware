/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "class/cla06fsubch.h"      // Default subchannels (LWA06F_SUBCHANNEL_3D)
#include "class/clc597.h"           // Turing 3d engine (contains MME64)
#include "core/include/massert.h"
#include "gpu/tests/gputest.h"
#include "mme64manager.h"

using namespace Mme64;

//! Number of NOPs to insert in the channel after perfoming a R-M-W via the
//! Falcon engine
#define FALCON_PRI_RMW_NOP_COUNT 10

namespace
{
    //!
    //! \brief DMA channel subchannels
    //!
    enum Subchannel
    {
        SUBCH_3D = LWA06F_SUBCHANNEL_3D
    };
};

//!
//! \brief Constructor.
//!
//! \param size Number of dwords in MME64 instruction RAM.
//! \param pCh Open DMA channel to the MME64.
//!
InstructionRam::InstructionRam(UINT32 size, Channel* pCh)
    : m_RamSize(size)
    , m_pCh(pCh)
{
    MASSERT(pCh);

    m_Memory.emplace_back(0, m_RamSize, true);
}

//!
//! \brief Load the given groups into the MME instruction RAM.
//!
//! \param groups Macro groups to load.
//! \param pOutStartAddr OUT Start address of the allocated instructions
//! in the instruction RAM.
//!
//! \return RC::OK on success, RC::CANNOT_ALLOCATE_MEMORY if there is no memory
//! to allocate.
//!
RC InstructionRam::Load(const Macro::Groups& groups, Addr* pOutStartAddr)
{
    RC rc;
    MASSERT(pOutStartAddr);

    const UINT32 numGroups = static_cast<UINT32>(groups.size());
    MASSERT(numGroups == groups.size()); // Truncation check
    const UINT32 sizeInDwords = numGroups * Group<s_NumGroupAlus>::GetElementCount();

    // Find free memory
    chunkIter it = std::find_if(m_Memory.begin(), m_Memory.end(),
                                [=] (auto& chunk)
                                {
                                    return (sizeInDwords <= chunk.size)
                                        && chunk.isFree;
                                });
    if (it == m_Memory.end())
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // Allocate and load the memory
    CHECK_RC(LoadInstrMem(it->addr, groups));

    // Update memory record
    const Addr originalAddr = it->addr;
    CHECK_RC(RecordAlloc(&it, groups, sizeInDwords));
    if (originalAddr != it->addr)
    {
        MASSERT(!"Iter should be refreshed to point to same RAM chunk");
        return RC::SOFTWARE_ERROR;
    }

    *pOutStartAddr = it->addr;
    return rc;
}

//!
//! \brief Free object loaded into the MME Instruction RAM.
//!
//! \param addr Instruction RAM address to free.
//!
RC InstructionRam::Free(Addr addr)
{
    RC rc;

    // Find the allocated chunk
    //
    chunkIter freedChunkIt = std::find_if(m_Memory.begin(), m_Memory.end(),
                                          [=] (auto& chunk)
                                          {
                                              return chunk.addr == addr;
                                          });
    if (freedChunkIt == m_Memory.end())
    {
        Printf(Tee::PriError, "Attempt to free address not associated with an "
               "allocation: %u\n", addr);
        return RC::SOFTWARE_ERROR;
    }

    // Free the memory
    //
    // Coalesce neighbouring free chunks
    UINT32 startAddr = freedChunkIt->addr;
    UINT32 freeMemSize = 0;
    chunkIter it = freedChunkIt;

    // Check if previous chunk is free
    if (it != m_Memory.begin())
    {
        --it; // Move to previous chunk
        if (it->isFree)
        {
            startAddr = it->addr;
            freeMemSize += it->size;
            CHECK_RC(RemoveChunk(&it)); // Iterator ends pointing to next chunk
                                        // (chunk to free)
        }
        else
        {
            ++it; // Move back to chunk to free
        }
    }

    // Add chunk being freed. Chunk will be used to represent the coalesced
    // memory.
    MASSERT(it == freedChunkIt);   // Should be the chunk being freed
    freeMemSize += it->size;

    // Check if next chunk is free
    ++it;
    if (it != m_Memory.end() && it->isFree)
    {
        freeMemSize += it->size;
        CHECK_RC(RemoveChunk(&it));     // Iterator ends pointing to next chunk
    }

    // Record coalesced memory chunk
    //
    freedChunkIt->addr = startAddr;
    freedChunkIt->size = freeMemSize;
    freedChunkIt->isFree = true;

    return rc;
}

//!
//! \brief  Free all the allocated memory in the MME Instruction RAM.
//!
RC InstructionRam::FreeAll()
{
    RC rc;

    // Erase memory records
    m_Memory.clear();

    // Record all memory as free
    m_Memory.emplace_back(0, m_RamSize, true);

    return rc;
}

//!
//! \brief Load the given group package into the instruction ram at the
//! given starting address.
//!
RC InstructionRam::LoadInstrMem(Addr startAddr, const Macro::Groups& groups)
{
    RC rc;

    // NOTE: The mme_instruction_ram is set as follows:
    //   mme_instruction_ram[mme_instruction_ram_pointer]
    //       = LoadMmeInstructionRam.V;
    //   mme_instruction_ram_pointer++;

    // Set the pointer into the MME instruction RAM to the desired starting
    // location.
    //
    CHECK_RC(m_pCh->Write(SUBCH_3D,
                          LWC597_LOAD_MME_INSTRUCTION_RAM_POINTER,
                          startAddr));

    // Load the macro into the MME instruction RAM.
    //
    // TODO(stewarts): Putting group packs into instruction format is done here
    // and in the sim. Make a constructor for bitfield.h that will allocate the
    // bitfield based on memory location. It will remove the need for this
    // colwersion.
    for (const auto& group : groups)
    {
        // For each data element in group
        for (const auto& instr : group)
        {
            CHECK_RC(m_pCh->Write(SUBCH_3D,
                                  LWC597_LOAD_MME_INSTRUCTION_RAM,
                                  instr));
        }
    }

    return rc;
}

//!
//! \brief Record an allocation to track the current state of the MME64
//! Instruction RAM.
//!
//! \param fromChunk Iterator to the chunk we are allocating from. Must be an
//! iterator from m_Memory.
//! \param groups Macros's Groups we are allocating.
//! \param packSize Size of the package's groups in dwords.
//!
RC InstructionRam::RecordAlloc
(
    chunkIter* pFromChunkIter
    ,const Macro::Groups& groups
    ,UINT32 packSize
)
{
    RC rc;
    MASSERT((*pFromChunkIter) != m_Memory.end());
    RamChunk& fromChunk = **pFromChunkIter;
    const Addr originalChunkAddr = fromChunk.addr;
    const UINT32 originalChunkSize = fromChunk.size;

    // Update original chunk
    fromChunk.size = packSize;
    fromChunk.isFree = false;

    // Create node for new free space
    //
    // Coalesce with next chunk if it's free
    chunkIter nextChunkIter = *pFromChunkIter;
    ++nextChunkIter;
    if (nextChunkIter != m_Memory.end() && nextChunkIter->isFree)
    {
        // Can coalesce. Use this chunk to record the coalesced memory.
        nextChunkIter->addr = originalChunkAddr + packSize;
        nextChunkIter->size += originalChunkSize - packSize;
    }
    else if (originalChunkSize - packSize > 0)
    {
        // There is still free memory, create a new node to represent it
        MASSERT(m_Memory.size() <= _UINT32_MAX); // Truncation check
        const UINT32 vecIndex = static_cast<UINT32>(std::distance(m_Memory.begin(), *pFromChunkIter));

        // Insert the new node after the 'from' RAM chunk
        m_Memory.emplace(++(*pFromChunkIter), originalChunkAddr + packSize,
                         originalChunkSize - packSize, true/*isFree*/);

        // Update the now invalid iterator to point to the same RAM chunk
        *pFromChunkIter = m_Memory.begin();
        std::advance(*pFromChunkIter, vecIndex);
    }

    return rc;
}

//!
//! \brief Remove given RAM chunk. Updates the given iterator to point to the
//! next item in the RAM chunk list.
//!
//! \param[in,out] RAM chunk to remove. Updated to point to the chunk following
//! the removed chunk.
//!
RC InstructionRam::RemoveChunk(chunkIter* pChunkIter)
{
    RC rc;
    MASSERT(pChunkIter);

    MASSERT(m_Memory.size() <= _UINT32_MAX); // Truncation check
    const UINT32 index = static_cast<UINT32>(std::distance(m_Memory.begin(), *pChunkIter));

    // Remove chunk
    m_Memory.erase(*pChunkIter);

    // Update the now invalid iterator to point to the chunk following the
    // removed chunk
    *pChunkIter = m_Memory.begin();
    std::advance(*pChunkIter, index);

    return rc;
}

bool InstructionRam::RamChunk::operator==(const RamChunk& other) const
{
    return this->addr == other.addr
        && this->size == other.size
        && this->isFree == other.isFree;
}

bool InstructionRam::RamChunk::operator!=(const RamChunk& other) const
{
    return !(*this == other);
}

//------------------------------------------------------------------------------
MacroLoader::MacroLoader(GpuSubdevice* pSubdev, Channel* pCh)
    : m_DebugPrintPri(Tee::PriNone)
    , m_pCh(pCh)
    , m_pInstrRam(make_unique<InstructionRam>(
                      pSubdev->GetMmeInstructionRamSize(), m_pCh))
    , m_StartAddrRamSize(pSubdev->GetMmeStartAddrRamSize())
{
    MASSERT(pCh);
}

//!
//! \brief Load the given group pack into the MME64 and return the start
//! address pointer to the group.
//!
//! pMacro->startAddrPtr will be set to the mme_start_addr entry table
//! corresponding to the given group package.
//!
//! NOTE: The start address pointer is used to tell the MME64 which set of
//! macros to run.
//!
//! \param macro The Macro containing the Groups to load into the MME64.
//! \return RC::CANNOT_ALLOCATE_MEMORY when macro can't be allocated.
//!
RC MacroLoader::Load(Macro* pMacro)
{
    RC rc;
    MASSERT(pMacro);

    // Find free entry in the macro_start_addr table
    Macro::StartAddrPtr entry;
    CHECK_RC(FindAvailableStartAddrPtr(&entry));
    MASSERT(entry != Macro::NOT_LOADED); // Ensure no flag value collision

    // Load the macro into the mme_instruction_ram (MME64 instruction memory)
    // NOTE: This is expected to fail when the intruction ram is full. To avoid
    // the CHECK_RC verbose print, avoid using CHECK_RC here.
    InstructionRam::Addr instrRamAddr;
    RC loadRc = m_pInstrRam->Load(pMacro->groups, &instrRamAddr);
    if (loadRc != RC::OK)
    {
        if (loadRc == RC::CANNOT_ALLOCATE_MEMORY)
        {
            Printf(m_DebugPrintPri, "Insufficient MME64 instruction RAM memory available\n");
        }

        return loadRc;
    }

    // Select the mme_start_addr table entry to associate with the loaded macro
    CHECK_RC(m_pCh->Write(SUBCH_3D,
                          LWC597_LOAD_MME_START_ADDRESS_RAM_POINTER,
                          entry));

    // Set mme_start_addr table entry to the macro start address in the
    // mme_instruction_ram
    CHECK_RC(m_pCh->Write(SUBCH_3D,
                          LWC597_LOAD_MME_START_ADDRESS_RAM,
                          instrRamAddr));

    // Record the table entry that contains this macro
    MASSERT(m_AllocationMap.find(entry) == m_AllocationMap.end());
    m_AllocationMap.insert(std::make_pair(entry, instrRamAddr));
    pMacro->mmeTableEntry = entry;

    return rc;
}

//!
//! \brief Free the given macro in the MME.
//!
//! Sets the macro to the 'not loaded' state.
//!
RC MacroLoader::Free(Macro* pMacro)
{
    RC rc;
    MASSERT(pMacro);
    MASSERT(pMacro->IsLoaded());

    // Find allocation
    auto iter = m_AllocationMap.find(pMacro->mmeTableEntry);
    if (iter == m_AllocationMap.end())
    {
        Printf(Tee::PriError, "Attempt to free unallocated macro in MME\n");
        return RC::SOFTWARE_ERROR;
    }

    // Free macro in the mme instruction ram
    CHECK_RC(m_pInstrRam->Free(iter->second));

    // Remove the item from the allocation map
    m_AllocationMap.erase(iter);

    // Set macro to 'not loaded' state
    pMacro->mmeTableEntry = Macro::NOT_LOADED;

    return rc;
}

//!
//! \brief Free all the allocated memory for macros on the MME.
//!
//! Note that all macros are not updated to indicate they are not loaded. It is
//! up to the user of this function to treat all previously loaded macros as if
//! they are not loaded.
//!
RC MacroLoader::FreeAll()
{
    RC rc;

    // Reset macro loader
    m_AllocationMap.clear();

    // Reset the instruction ram
    m_pInstrRam->FreeAll();

    return rc;
}

//!
//! \brief Set the priority of debug prints.
//!
//! \param pri Debug print priority.
//!
void MacroLoader::SetDebugPrintPri(Tee::Priority pri)
{
    m_DebugPrintPri = pri;
}

//!
//! \brief Find an avilable macro_start_addr table entry (aka.
//! macro_start_addr_ptr).
//!
//! \param[out] pTableEntry An available entry in the macro_start_addr table.
//!
RC MacroLoader::FindAvailableStartAddrPtr(Macro::StartAddrPtr* pTableEntry)
{
    RC rc;

    // Ensure that we have not hit the maximum number of entries.
    if (m_AllocationMap.size() >= m_StartAddrRamSize)
    {
        Printf(Tee::PriError, "Maximum number of MME macro_start_addr table "
               "entries reached\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // Find first free entry
    Macro::StartAddrPtr entry = 0;
    for (const auto& it : m_AllocationMap)
    {
        if (it.first == entry)
        {
            // Check next entry
            ++entry;
        }
        else
        {
            // Found free entry
            break;
        }
    }

    *pTableEntry = entry;
    return rc;
}

//------------------------------------------------------------------------------
//!
//! \brief Return the usable Data RAM size in DWORDs when the Data FIFO is set
//! to the given size.
//!
//! \param totalDataRamSize Total size of the Data RAM.
//! \param dataFifoSize Size of the Data FIFO.
//!
/* static */ UINT32 DataRam::RamSizeWhenFifoSize
(
    UINT32 totalDataRamSize
    ,FifoSize dataFifoSize
)
{
    return totalDataRamSize - DataRam::FifoSizeInDwords(dataFifoSize);
}

//!
//! \brief Return the given Data FIFO size in DWORDs.
//!
//! \param fifoSize Data FIFO size enum value.
//!
/* static */ UINT32 DataRam::FifoSizeInDwords(FifoSize size)
{
    UINT32 dwordSize = 0;

    switch (size)
    {
    case FifoSize::_0KB:
        dwordSize = static_cast<UINT32>(0_KB / 4);
        break;

    case FifoSize::_4KB:
        dwordSize = static_cast<UINT32>(4_KB / 4);
        break;

    case FifoSize::_8KB:
        dwordSize = static_cast<UINT32>(8_KB / 4);
        break;

    case FifoSize::_12KB:
        dwordSize = static_cast<UINT32>(12_KB / 4);
        break;

    case FifoSize::_16KB:
        dwordSize = static_cast<UINT32>(16_KB / 4);
        break;

    default:
        MASSERT(!"Unknown Data FIFO size");
        Printf(Tee::PriWarn, "Unknown Data FIFO size\n");
    }

    return dwordSize;
}

//!
//! \brief Constructor.
//!
//! \param pSubdev GPU subdevice.
//! \param pCh 3D engine channel containing the MME.
//! \param totalRamSize Total size of the Data RAM (includes the section
//! allocated to the Data FIFO).
//!
DataRam::DataRam(Channel* pCh, UINT32 totalRamSize)
    : m_pCh(pCh)
    , m_TotalRamSize(totalRamSize)
      // Default. Ref: https://p4viewer.lwpu.com/get/hw/lwgpu/class/mfs/class/common/CommonMmeMethods.mfs //$
    , m_FifoSize(FifoSize::_4KB)
{}

//!
//! Sets the size of the Data FIFO (aka. Data RAM FIFO).
//!
//! Data FIFO is stored in the Data RAM. What value is set here is subtracted
//! from the available Data RAM size.
//!
//! NOTE: Data FIFO size must be set when the data FIFO is empty.
//!
//! \param size Size of the data FIFO.
//!
RC DataRam::SetDataFifoSize(FifoSize size)
{
    RC rc;

    // Set data fifo size
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_MME_DATA_FIFO_CONFIG,
                          static_cast<UINT32>(size)));

    // Record new fifo size
    m_FifoSize = size;

    return rc;
}

//!
//! \brief Fill the MME Data RAM.
//!
//! \param pSubdev GPU subdevice associated with the target MME.
//! \param dataRamValues Values to set the full usable data ram.
//! \param timeoutMs Time to wait for the operation to complete.
//!
RC DataRam::Fill
(
    GpuSubdevice* pSubdev
    ,const vector<UINT32>& dataRamValues
    ,FLOAT64 timeoutMs
)
{
    RC rc;
    MASSERT(pSubdev);
    MASSERT(dataRamValues.size() == GetRamSize());

    Surface2D surf;
    CHECK_RC(AllocRamSizedSurface(pSubdev, &surf));

    // Copy given data ram values into the surface
    //
    for (UINT32 n = 0; n < dataRamValues.size(); ++n)
    {
        surf.WritePixel(n, 0, dataRamValues.at(n));
    }

    // Flush the CPU writes to the GPU
    //
    surf.FlushCpuCache(Surface::Flush);

    // Populate data ram
    //
    const LwU64 offset = surf.GetCtxDmaOffsetGpu();
    const LwU32 upperOffset = LwU64_HI32(offset);
    const LwU32 lowerOffset = LwU64_LO32(offset);
    // SetMmeMemAddressA supports 8 bits of upper addr
    MASSERT(!(upperOffset & ~0xff));

    // Force all prior DMA reads, writes and reductions to memory
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_MME_DMA_SYSMEMBAR, 0));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_pCh->WaitIdle(timeoutMs));

    // Set source to the surface
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_MME_MEM_ADDRESS_A, upperOffset));
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_MME_MEM_ADDRESS_B, lowerOffset));

    // Set destination to first address in MME data ram
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_MME_DATA_RAM_ADDRESS, 0));

    // Copy the surface into the MME data ram
    const UINT32 readSize = static_cast<UINT32>(dataRamValues.size());
    MASSERT(readSize == dataRamValues.size()); // Truncation check
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_MME_DMA_READ, readSize));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_pCh->WaitIdle(timeoutMs));

    // Cleanup
    //
    surf.Unmap();
    surf.Free();

    return rc;
}

//!
//! \brief Copy the usable Data RAM into the provided surface.
//!
//! \param pSubdev GPU Subdevice to copy from.
//! \param[out] pBuf Surface to copy the Data RAM to.
//! \param timeoutMs Time to wait for the operation to complete.
//!
RC DataRam::Copy(GpuSubdevice* pSubdev, Surface2D* pBuf, FLOAT64 timeoutMs)
{
    RC rc;
    MASSERT(pSubdev);
    MASSERT(pBuf);
    // Surface should either not be allocated, or should be allocated and mapped.
    MASSERT(!pBuf->IsAllocated()
            || (pBuf->IsAllocated() && pBuf->IsGpuMapped()));

    if (pBuf->IsAllocated())
    {
        // Existing surface, make sure it is large enough
        if (!(pBuf->GetSize() / 4 == GetRamSize())) // bytes / 4 == DWORD
        {
            MASSERT(!"Provided buffer is not the same size as the Data RAM");
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        // Setup the surface
        CHECK_RC(AllocRamSizedSurface(pSubdev, pBuf));
    }

    // Copy data ram
    //
    const LwU64 offset = pBuf->GetCtxDmaOffsetGpu();
    const LwU32 upperOffset = LwU64_HI32(offset);
    const LwU32 lowerOffset = LwU64_LO32(offset);
    // SetMmeMemAddressA supports 8 bits of upper addr
    MASSERT(!(upperOffset & ~0xff));

    // Force all prior DMA reads, writes and reductions to memory
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_MME_DMA_SYSMEMBAR, 0));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_pCh->WaitIdle(timeoutMs));

    // Set destination to the surface
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_MME_MEM_ADDRESS_A, upperOffset));
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_MME_MEM_ADDRESS_B, lowerOffset));

    // Set destination to first address in MME data ram
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_MME_DATA_RAM_ADDRESS, 0));

    // Copy the MME data ram to the surface
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_MME_DMA_WRITE, GetRamSize()));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_pCh->WaitIdle(timeoutMs));

    return rc;
}

//!
//! \brief Return the total Data RAM size in DWORDs.
//!
UINT32 DataRam::GetTotalRamSize() const
{
    return m_TotalRamSize;
}

//!
//! \brief Return the usable Data RAM size in DWORDs.
//!
//! Excludes memory taken paritioned off for the data FIFO.
//!
//! Represents the addressable memory size from the MME through DREAD/DWRITE.
//!
UINT32 DataRam::GetRamSize() const
{
    UINT32 ramSize
        = DataRam::RamSizeWhenFifoSize(GetTotalRamSize(), m_FifoSize);
    MASSERT(ramSize == GetTotalRamSize() - GetFifoSize());
    return ramSize;
}

//!
//! \brief Return the Data FIFO size.
//!
UINT32 DataRam::GetFifoSize() const
{
    return DataRam::FifoSizeInDwords(m_FifoSize);
}

//!
//! \brief Allocate the given surface to be able to hold the usable Data RAM
//! size contiguously.
//!
//! \param pSubdev GPU Subdevice to allocate the surface on.
//! \param pSurf Surface to allocate.
//!
RC DataRam::AllocRamSizedSurface(GpuSubdevice* pSubdev, Surface2D* pSurf) const
{
    RC rc;
    MASSERT(pSubdev);
    MASSERT(pSurf);
    MASSERT(!pSurf->IsAllocated());

    pSurf->SetWidth(GetRamSize());
    pSurf->SetHeight(1);
    pSurf->SetPitch(pSurf->GetWidth() * 4); // Specified in bytes, 4 bytes per VOID32
    pSurf->SetColorFormat(ColorUtils::VOID32);
    pSurf->SetLocation(Memory::NonCoherent); // Not cached (only writing to surf)
    CHECK_RC(pSurf->Alloc(pSubdev->GetParentDevice()));
    CHECK_RC(pSurf->Map(pSubdev->GetSubdeviceInst()));

    MASSERT(pSurf->GetSize() / 4 == GetRamSize()); // bytes / 4 == DWORD

    return rc;
}


//------------------------------------------------------------------------------
Manager::Manager
(
    GpuTest* pGpuTest
    ,GpuTestConfiguration* pGpuTestConfig
    ,unique_ptr<ThreeDAlloc> pThreeDAlloc
)
    : m_pGpuTest(pGpuTest)
    , m_pSubdev(pGpuTest->GetBoundGpuSubdevice())
    , m_pGpuTestConfig(pGpuTestConfig)
    , m_pThreeDAlloc(std::move(pThreeDAlloc))
    , m_pCh(nullptr)
    , m_hCh(0)
    , m_pAtomChWrapper(nullptr)
    , m_pI2mBuf(nullptr)
    , m_SemaPayload(0)
    , m_OutputToInlineToMemoryEnabled(false)
{}

RC Manager::Setup()
{
    RC rc;

    // Allocate channel and engine
    CHECK_RC(m_pGpuTestConfig->AllocateChannelWithEngine(&m_pCh,
                                                         &m_hCh,
                                                         m_pThreeDAlloc.get()));

    // Macro loader
    m_pMacroLoader = make_unique<MacroLoader>(m_pSubdev, m_pCh);
    if (!m_pMacroLoader)
    {
        Printf(Tee::PriError, "Failed to construct MME64 macro loader\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // Data ram
    m_pDataRam = make_unique<DataRam>(m_pCh, m_pSubdev->GetMmeDataRamSize());
    if (!m_pDataRam)
    {
        Printf(Tee::PriError, "Failed to construct MME Data RAM object\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // Set the Data FIFO size to a known value
    m_pDataRam->SetDataFifoSize(DataRam::FifoSize::_4KB);

    // Setup 3D engine with MME
    CHECK_RC(m_pCh->SetObject(SUBCH_3D, m_pThreeDAlloc->GetHandle()));
    m_pAtomChWrapper = m_pCh->GetAtomChannelWrapper();
    MASSERT(m_pAtomChWrapper);

    // Get surface for I2M output
    m_pI2mBuf = m_pGpuTest->GetDisplayMgrTestContext()->GetSurface2D();
    CHECK_RC(m_pI2mBuf->Map(m_pSubdev->GetSubdeviceInst()));
    m_pI2mBuf->Fill(0, m_pSubdev->GetSubdeviceInst());

    // Setup 3D engine semaphore
    m_3dSemaphore.Setup(NotifySemaphore::ONE_WORD,
                        m_pGpuTestConfig->NotifierLocation(),
                        1/*num surfaces*/);
    CHECK_RC(m_3dSemaphore.Allocate(m_pCh, 0));

    // Put the shadow RAM in passthrough mode. All methods are passed along to
    // the method decode state, and the MME Shadow RAM is unchanged.
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_MME_SHADOW_RAM_CONTROL,
                          LWC597_SET_MME_SHADOW_RAM_CONTROL_MODE_METHOD_PASSTHROUGH));

    // Set MME to output over I2M
    CHECK_RC(SetOutputToInlineToMemory(true));

    return rc;
}

RC Manager::Cleanup()
{
    StickyRC firstRc;

    firstRc = FreeAll();

    // Set MME to output to pipeline
    firstRc = SetOutputToInlineToMemory(false);
    firstRc = m_pCh->Flush();
    firstRc = m_pCh->WaitIdle(1000/*ms*/); // Finish method exelwtion

    m_pThreeDAlloc->Free();
    m_3dSemaphore.Free();

    firstRc = m_pGpuTestConfig->FreeChannel(m_pCh);
    m_pCh = nullptr;
    m_hCh = 0;

    return firstRc;
}

RC Manager::LoadMacro(Macro* pMacro)
{
    return m_pMacroLoader->Load(pMacro);
}

RC Manager::FreeMacro(Macro* pMacro)
{
    return m_pMacroLoader->Free(pMacro);
}

RC Manager::FreeAll()
{
    return m_pMacroLoader->FreeAll();
}

//!
//! \brief Run the given Macro on the MME.
//!
//! If the macro has emissions, the MME will be set to I2M mode and will wait
//! for I2M transaction completion. Otherwise the methods are flushed and a
//! wait-for-idle is not performed.
//!
//! \param macro Macro to run.
//! \param pOutMacroOutput OUT Start virtual address of the inline-to-memory
//! output from the given macro. Can be nullptr.
//!
RC Manager::RunMacro(const Macro& macro, VirtualAddr* pOutMacroOutput)
{
    RC rc;
    MASSERT(macro.mmeTableEntry != Macro::NOT_LOADED); // Macro must be loaded

    I2mConfig i2mConfig;
    const bool isEmissions = macro.numEmissions > 0;

    // Start I2M transaction
    //
    if (isEmissions)
    {
        // Open a transfer between the MME64 and the I2M destination buf
        CHECK_RC(OpenI2mTransfer(macro.numEmissions, &i2mConfig));
    }

    // Run macros on the MME64
    //
    if (macro.inputData.size() > 0)
    {
        const UINT32 inputDataSize = static_cast<UINT32>(macro.inputData.size());
        MASSERT(inputDataSize == macro.inputData.size()); // Truncation check
        CHECK_RC(m_pCh->WriteIncOnce(SUBCH_3D,
                                     LWC597_CALL_MME_MACRO(macro.mmeTableEntry),
                                     inputDataSize, macro.inputData.data()));
    }
    else
    {
        CHECK_RC(m_pCh->Write(SUBCH_3D,
                              LWC597_CALL_MME_MACRO(macro.mmeTableEntry),
                              0));
    }

    // NOTE: CallMme method is part of what is considered an atomic method
    // group, or 'atom'. The definition for these atoms are in atomlist.h. The
    // push buffer logic has an underlying finite state machine (FSM) that
    // determines atoms in the method stream. Incomplete atoms are not sent
    // during a flush. The FSM cannot determine if the current method is the end
    // of an atom until the next method is recieved. Since CallMme is the start
    // of an atom and is the last method before a Flush, the FSM is unable to
    // determine that it alone is a complete atom and will not send it. An atom
    // can be closed by calling CancelAtom, qualifying CallMme to be sent with
    // the Flush.
    //
    // Channel::WaitIdle could also be used as it implicitly calls CancelAtom.
    // CancelAtom is used directly because WaitIdle waits for the engine to
    // complete the flushed methods. This increases the time between macro runs
    // when running multiple macros within the same I2M transaction.
    //
    m_pAtomChWrapper->CancelAtom();

    // End I2M transaction
    //
    if (isEmissions)
    {
        // Complete I2M transaction with dummy LoadInlineData calls if necessary
        CHECK_RC(SendI2mTransactionPadding(macro.numEmissions, i2mConfig));
    }

    // Send methods over DMA channel
    //
    CHECK_RC(m_pCh->Flush());

    // Wait on I2M transaction completion
    //
    // TODO(stewarts): We may run macros that have no emissions. In this case,
    // do we still need to wait for the MME to complete? Can we simply CALL_MME
    // right after flushing and it will be automatically queued? The payload
    // values we are writing are unique, so if we run n macros without ouput
    // followed by one with output, can we just wait on the I2M_SEMAPHORE?
    if (isEmissions)
    {
        // Wait on the I2M result
        CHECK_RC(m_3dSemaphore.Wait(m_pGpuTestConfig->TimeoutMs()));

        if (pOutMacroOutput)
        {
            *pOutMacroOutput = reinterpret_cast<VirtualAddr>(m_pI2mBuf->GetAddress());
        }
    }

    return rc;
}

//!
//! Sets the size of the Data FIFO (aka. Data RAM FIFO).
//!
//! Data FIFO is stored in the Data RAM. What value is set here is subtracted
//! from the available Data RAM size.
//!
//! NOTE: Data FIFO size must be set when the data FIFO is empty.
//!
//! \param size Size of the data FIFO.
//!
RC Manager::SetDataFifoSize(DataRam::FifoSize size)
{
    return m_pDataRam->SetDataFifoSize(size);
}

//!
//! \brief Fill the MME Data RAM from the given values
//!
//! \param dataRamValues Values to set the full usable Data RAM.
//!
RC Manager::FillDataRam(const vector<UINT32>& dataRamValues)
{
    return m_pDataRam->Fill(m_pSubdev, dataRamValues,
                            m_pGpuTestConfig->TimeoutMs());
}

//!
//! \brief Copy the MME Data RAM into the given buffer.
//!
//! \param pBuf Buffer to copy the Data RAM to. If the surface has been mapped
//! and allocated and is sufficiently large to hold the Data RAM, it will be
//! used. If the surface has not been allocated, it will be setup accordingly.
//!
RC Manager::CopyDataRam(Surface2D* pBuf)
{
    return m_pDataRam->Copy(m_pSubdev, pBuf, m_pGpuTestConfig->TimeoutMs());
}

//!
//! \brief Wait for the MME's channel to idle.
//!
RC Manager::WaitIdle()
{
    return m_pCh->WaitIdle(m_pGpuTestConfig->TimeoutMs());
}

//!
//! \brief Return the usable Data RAM size in DWORDs.
//!
//! Excludes memory taken paritioned off for the data FIFO.
//!
//! Represents the addressable memory size from the MME through DREAD/DWRITE.
//!
UINT32 Manager::GetUsableDataRamSize() const
{
    return m_pDataRam->GetRamSize();
}

//!
//! \brief Return the Data FIFO size.
//!
UINT32 Manager::GetDataFifoSize() const
{
    return m_pDataRam->GetFifoSize();
}

//!
//! \brief Return size of MME Instruction RAM in DWORDs.
//!
UINT32 Manager::GetInstrRamSize() const
{
    return m_pMacroLoader->GetInstrRamSize();
}

//!
//! \brief Return maximum number of macos that the be stored on the MME.
//!
UINT32 Manager::GetMaxNumMacros() const
{
    return m_pMacroLoader->GetMaxNumMacros();
}

//!
//! \brief Set the priority of debug prints.
//!
//! \param pri Debug print priority.
//!
void Manager::SetDebugPrintPri(Tee::Priority pri)
{
    m_pMacroLoader->SetDebugPrintPri(pri);
}

//!
//! \brief Send LoadInlineData methods to complete an open MME to
//! inline-to-memory (I2M) transaction.
//!
//! Assumes that the MME will produce the expected amount of LoadInlineData
//! methods.
//!
//! The MME may not send enough data to complete an open I2M transaction.
//! LoadInlineData methods will be generated to pad the output with zero until
//! the required transaction amount is completed.
//!
//! \param numEmissions Number of MME emissions.
//! \param i2mConfig Open I2M transaction configuration.
//!
//! \see OpenI2mTransfer
//!
RC Manager::SendI2mTransactionPadding(UINT32 numEmissions, const I2mConfig& i2mConfig)
{
    RC rc;

    const UINT32 numExpectedLoads
        = i2mConfig.lineCount * static_cast<UINT32>(std::ceil(i2mConfig.lineLengthIn / 4));
    const UINT32 numMmeLoads = numEmissions * I2mEmission::s_SizeInDwords;
    const UINT32 numPadLoads = numExpectedLoads - numMmeLoads;

    // Pad I2M loads until the end of the transaction
    for (UINT32 n = 0; n < numPadLoads; ++n)
    {
        CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_LOAD_INLINE_DATA, 0));
    }

    return rc;
}

//!
//! \brief Toggle MME output to inline-to-memory transfer.
//!
//! While enabled, the MME will send LoadInlineData methods containing its
//! output instead of sending the macros down the graphics pipeline.
//!
//! \param enable True to enable MME inline-to-memory transfer, false to
//! disable.
//!
//! \see OpenI2mTransfer
//!
RC Manager::SetOutputToInlineToMemory(bool enable)
{
    // NOTE: This is equivalent to writing to the PRI register directly to set
    // the MME to M2M Inline (I2M) mode. Writing the the register directly does
    // not trigger the MME to output via I2M. This is likely due to needing HULK
    // access to directly write the register. Using the Falcon is a way to
    // cirlwmvent this.

    RC rc;
    UINT32 offset, mask, data;

    m_pSubdev->GetMmeToMem2MemRegInfo(enable, &offset, &mask, &data);

    // Configure MME to I2M via Falcon
    //
    // Scratch location 0 is used as mailbox to sync between MME and Falcon.
    // Scratch location 1-127 can be used to pass parameters to the Falcon.
    // MME<->Falcon ref: https://p4viewer.lwpu.com/get/hw/lwgpu/class/mfs/class/3d/turing.mfs
    CHECK_RC(m_pCh->Write(SUBCH_3D,
                          LWC597_SET_MME_SHADOW_SCRATCH(0),
                          3,
                          0,        // Scratch[0]: Flag register to 0. Set to 1 when Falcon completes. //$
                          data,     // Scratch[1]: PRI data, MEM_TO_MEM_INLINE_[ENABLE, DISABLE]
                          mask));   // Scratch[2]: Mask of bits to change, MEM_TO_MEM_INLINE field mask //$
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_FALCON04,
                          offset)); // PRI register addr, MME_CONFIG_MEM_TO_MEM_INLINE

    // Emit NOP to backup the pipe and guarantee synchronization between MME and
    // Falcon.
    //
    // TODO: Remove this once ucode is updated to not require NOPS when a R-M-W
    // is done via the falcon engine
    for (UINT32 i = 0; i < FALCON_PRI_RMW_NOP_COUNT; ++i)
    {
        CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_NO_OPERATION, 0));
    }

    // Record the state of the MME output
    m_OutputToInlineToMemoryEnabled = enable;

    return rc;
}

//!
//! \brief Open a transfer for data from the MME64 to the I2M buffer.
//!
//! NOTE: This will initialize the transfer and will be waiting for the MME64 to
//! do its loads.
//!
//! \param numEmissions Number of MME emissions.
//! \param pOutI2mConfig OUT The I2M configuration settings for the transfer.
//!
RC Manager::OpenI2mTransfer(UINT32 numEmissions, I2mConfig* const pOutI2mConfig)
{
    RC rc;
    MASSERT(m_OutputToInlineToMemoryEnabled); // Must have MME in I2M mode
    MASSERT(pOutI2mConfig);
    const UINT32 bytesPerDword = 4;

    // Configure I2M
    //
    const UINT32 pitch = m_pI2mBuf->GetPitch();
    const UINT32 lineCount
        = ((numEmissions * I2mEmission::s_SizeInDwords * bytesPerDword) / pitch) + 1;

    *pOutI2mConfig = I2mConfig(m_pI2mBuf->GetCtxDmaOffsetGpu(),
                               pitch,
                               pitch,
                               lineCount);

    CHECK_RC(pOutI2mConfig->Apply(m_pCh));

    // Config semaphore
    //
    // Set the location I2M semaphore over the DMA channel
    const LwU64 threeDSemaOffset
        = static_cast<LwU64>(m_3dSemaphore.GetCtxDmaOffsetGpu(0/*surface id*/));
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_I2M_SEMAPHORE_A,
                          LwU64_HI32(threeDSemaOffset)));
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_I2M_SEMAPHORE_B,
                          LwU64_LO32(threeDSemaOffset)));

    // Set expected semaphore payload
    const UINT32 dummyPayload = ++m_SemaPayload;
    const UINT32 payload = ++m_SemaPayload;

    m_3dSemaphore.SetPayload(dummyPayload);
    m_3dSemaphore.SetTriggerPayload(payload);

    // Set payload to be written by I2M after the transaction has completed (ie.
    // the expected payload to wait on).
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_SET_I2M_SEMAPHORE_C,
                          payload));

    // Configure the LaunchDma
    //
    UINT32 launchVal = 0;

    // DstMemoryLayout: destination memory layout
    launchVal |= DRF_DEF(C597, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);
    // CompletionType: type of syncronization required at completion of copy
    launchVal |= DRF_DEF(C597, _LAUNCH_DMA, _COMPLETION_TYPE, _RELEASE_SEMAPHORE);
    // InterruptType: type of interrupt to signal copy completion
    launchVal |= DRF_DEF(C597, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE);
    // SemaphoreStructSize: size of semaphore struct (only required if
    //   CompletionType==RELEASE_SEMAPHORE)
    launchVal |= DRF_DEF(C597, _LAUNCH_DMA, _SEMAPHORE_STRUCT_SIZE, _ONE_WORD);

    // Launch the I2M transfer
    //
    CHECK_RC(m_pCh->Write(SUBCH_3D, LWC597_LAUNCH_DMA, launchVal));

    // NOTE: I2M transaction is now open. The assumption for the transfer is the
    // following:
    //   LoadInlineData N times, where N = LineCount * (ceil(LineLength/4))

    return rc;
}

Manager::I2mConfig::I2mConfig
(
    UINT64 outputBufDmaOffsetGpu
    ,UINT32 pitchOut
    ,UINT32 lineLengthIn
    ,UINT32 lineCount
)
    : outputBufDmaOffsetGpu(outputBufDmaOffsetGpu)
    , pitchOut(pitchOut)
    , lineLengthIn(lineLengthIn)
    , lineCount(lineCount)
{
    MASSERT(!((outputBufDmaOffsetGpu >> 32) & ~0x0001ffffU)); // 17 bit wide upper offset
    MASSERT(!(lineCount & ~0x0003ffffU)); // 18 bit field
    MASSERT(!(pitchOut  & ~0x007fffffU)); // 23 bit field
}

//!
//! \brief Send methods over the given channel to use this I2M configuration.
//!
//! The assumption for transfer is the following:
//!   LoadInlineData N times, where N = lineCount * (ceil(lineLength/4))
//!
RC Manager::I2mConfig::Apply(Channel* pCh)
{
    RC rc;
    MASSERT(pCh);
    const LwU64 offset = static_cast<LwU64>(outputBufDmaOffsetGpu);

    // Ref: //hw/lwgpu/class/mfs/class/2d/kepler_common_inline_to_memory_for_pascal.mfs //$
    // - OffsetOut: Output buffer GPU DMA offset.
    CHECK_RC(pCh->Write(SUBCH_3D, LWC597_OFFSET_OUT, LwU64_LO32(offset)));
    CHECK_RC(pCh->Write(SUBCH_3D, LWC597_OFFSET_OUT_UPPER, LwU64_HI32(offset)));
    // - LineLengthIn: If reading the next sample would exceed this value,
    //   output stops and notification is generated w/ semaphore. Specified in
    //   bytes.
    CHECK_RC(pCh->Write(SUBCH_3D, LWC597_LINE_LENGTH_IN, lineLengthIn));
    // - LineCount: Number of lines of linear data. If 0, no data transferred,
    //   but semaphore is still triggered according to LaunchDma config.
    CHECK_RC(pCh->Write(SUBCH_3D, LWC597_LINE_COUNT, lineCount));
    // - PitchOut: Delta in bytes between any two bytes with the same offset in
    //   adjacent line of the output buffer. Not used if lineCount.Value==1.
    CHECK_RC(pCh->Write(SUBCH_3D, LWC597_PITCH_OUT, pitchOut));

    return rc;
}

//!
//! \brief Print the I2M configuration.
//!
void Manager::I2mConfig::Print(Tee::Priority printLevel)
{
    Printf(printLevel,
           "Inline-to-memory Configuration:\n"
           "\tDestination parameters:\n"
           "\t\tOutput buf DMA offset GPU: %llu bytes\n"
           "\t\tPitch out: %u bytes\n"
           "\tTransfer parameters:\n"
           "\t\tLine length in: %u bytes\n"
           "\t\tLine count: %u\n",
           outputBufDmaOffsetGpu, pitchOut, lineLengthIn, lineCount);
}
