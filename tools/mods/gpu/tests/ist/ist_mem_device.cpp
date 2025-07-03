/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "device/interface/pcie.h"
#include "ist_mem_device.h"

namespace
{
    /**
     * \brief RAII class to map, read/write and unmap a memory block
     */
    class MemMapper final
    {
        using Error = MATHS_memory_manager_ns::mem_manager_error;

    public:
        enum MODE : uint32_t
        {
            RDONLY = 0,
            RDWR   = 1
        };

        MemMapper
        (
            void* descriptor,
            size_t offset,
            size_t length,
            MODE flags /* RDONLY, RDWR */
        )
        {
            const size_t size = length;
            const Memory::Protect readMode = (flags == RDONLY) ?
                                              Memory::Protect::Readable :
                                              Memory::Protect::ReadWrite;
            const RC rc = Platform::MapPages(
                    &m_VirtualAddress,  //void **pReturnedVirtualAddress,
                    descriptor,
                    offset,
                    size,
                    readMode
            );
            if (rc != RC::OK)
            {
                throw Error(Utility::StrPrintf(
                    "%s - Failed to map pages at address %llx", MODS_FUNCTION,
                    reinterpret_cast<UINT64>(m_VirtualAddress)));
            }
        }

        ~MemMapper()
        {
            Platform::UnMapPages(m_VirtualAddress);
        }

        void* wrPtr()
        {
            return m_VirtualAddress;
        }

        const void* rdPtr() const
        {
            return m_VirtualAddress;
        }

    private:
        void* m_VirtualAddress = nullptr;
    };

    struct MemBlock
    {
        MemBlock(size_t sizeBytes_, const void* descriptor_)
            : sizeBytes(sizeBytes_)
            , descriptor(descriptor_)
        {}

        const size_t sizeBytes = 0;
        const void* descriptor = nullptr;
    };
    struct ContiguousBlock
    {
        ContiguousBlock(PHYSADDR startingPhysicalAddress, size_t sizeBytes, const void* descriptor)
            : startingPhysicalAddress(startingPhysicalAddress)
            , sizeBytes(sizeBytes)
            , descriptors(1, descriptor)
        {}

        PHYSADDR startingPhysicalAddress = 0;
        size_t sizeBytes = 0;
        vector<const void*> descriptors;
    };

    ///
    /// Group all memory blocks into blocks large enough to fulfil the
    /// allocation request (/requestedSizeOneBlock/ bytes).
    /// The map /memoryBlocks/ is sorted by physical address.
    ///
    /// In the first iteration we create a new block with the physical address
    /// of the first allocation
    /// In every subsequent iteration, we add to this block if the addresses are
    /// contiguous or create a new block if they are not
    /// We also close a block if its size reaches the requested size (/desiredBlockSize/ bytes).
    ///
    uint32_t GroupBlocks
    (
        vector<ContiguousBlock>* pContiguousBlocks,
        const std::map<size_t, MemBlock>& memoryBlocks,
        const size_t desiredBlockSize
    )
    {
        vector<ContiguousBlock>& contiguousBlocks = *pContiguousBlocks;
        contiguousBlocks.clear();

        bool newBlock = true;
        PHYSADDR lastEndAddr = 0;
        uint32_t numFullBlocks = 0;
        for (const auto& memBlockPair : memoryBlocks)
        {
            const PHYSADDR physicalAddr = memBlockPair.first;
            const size_t sizeBytes = memBlockPair.second.sizeBytes;
            const void* descriptor = memBlockPair.second.descriptor;

            if (newBlock)
            {
                contiguousBlocks.emplace_back(physicalAddr, sizeBytes, descriptor);
                newBlock = false;
            }
            else if (physicalAddr == lastEndAddr)
            {
                contiguousBlocks.back().sizeBytes += sizeBytes;
                contiguousBlocks.back().descriptors.push_back(descriptor);
            }
            else
            {
                contiguousBlocks.emplace_back(physicalAddr, sizeBytes, descriptor);
            }

            if (contiguousBlocks.back().sizeBytes >= desiredBlockSize)
            {
                ++numFullBlocks;
                newBlock = true;
            }

            lastEndAddr = physicalAddr + UNSIGNED_CAST(PHYSADDR, sizeBytes);
        }

        return numFullBlocks;
    }
}

// MAL's namespace for memory-related classes
namespace MATHS_memory_manager_ns
{

MemDevice::MemDevice(std::string fn, size_t start, size_t len, size_t bidx)
    : m_PhysicalAddr(start),
      m_Length(len),
      m_Descriptor(bidx)
{}

MemDevice::MemDevice(const MemDevice& other)
    : m_PhysicalAddr(other.m_PhysicalAddr),
      m_Length(other.m_Length),
      m_Descriptor(other.m_Descriptor)
{}

MemDevice::~MemDevice()
{
    // needed?
}

/*static*/ MemDevice::MemAddresses MemDevice::AllocateMemory
(
    const MemRequirements& memRequirements,
    const GpuSubdevice *gpuSubdev,
    bool isIOMMUEnabled
)
{
    using Utility::StrPrintf;

    // 2MB is the largest the mods kernel driver can allocate at once (linux kernel limitation)
    constexpr size_t maxModsBlockSize = 1 << 21;
    constexpr UINT32 gpuInst = 0;   // enforced: only one GPU in system

    const uint32_t numBlocksRequested = memRequirements.numBlocks;
    const size_t requestedSizeOneBlock = memRequirements.blockSizeBytes;
    const size_t totalRequestedMem = requestedSizeOneBlock * numBlocksRequested;
    //const size_t allocationSize = std::min(requestedSizeOneBlock, maxModsBlockSize);  // FIXME: if using this, the code below has to query the actual allocated size because the memory is allocated in pages //$
    const size_t allocationSize = maxModsBlockSize;
    unsigned int domain = 0;
    unsigned int bus = 0;
    unsigned int device = 0;
    unsigned int function = 0;

    if (isIOMMUEnabled && gpuSubdev)
    {
        domain = gpuSubdev->GetInterface<Pcie>()->DomainNumber();
        bus = gpuSubdev->GetInterface<Pcie>()->BusNumber();
        device = gpuSubdev->GetInterface<Pcie>()->DeviceNumber();
        function = gpuSubdev->GetInterface<Pcie>()->FunctionNumber();
    }

    std::map<size_t, MemBlock> memoryBlocks;
    vector<ContiguousBlock> contiguousBlocks;
    contiguousBlocks.reserve(numBlocksRequested);

    string error;
    size_t allocatedSize = 0;
    uint32_t numFullBlocks = 0;
    while (numFullBlocks < numBlocksRequested)
    {
        // Allocate through the mods kernel driver
        void* descriptor = nullptr;
        RC rc = Platform::AllocPages(
            allocationSize, &descriptor,
            true /*contiguous*/,
            sizeof(PHYSADDR) * CHAR_BIT,    // memory addressing
            Memory::Attrib::WC, gpuInst,
            Memory::Protect::ReadWrite);

        // Potential TODO: Look for smaller chunks if this fails
        // (tested with 320MB and it works fine so far)

        if (rc != RC::OK)
        {
            error = StrPrintf("%s - Failed to allocate %zu bytes of contiguous "
                        "physical memory (%zu/%zu bytes already allocated)[RC=%d]",
                        MODS_FUNCTION, allocationSize, allocatedSize,
                        totalRequestedMem, static_cast<INT32>(rc));
            break;
        }

        // Get physical address of allocated block
        {
            PHYSADDR physicalAddr = 0x0;
            if (isIOMMUEnabled)
            {
                physicalAddr = Platform::GetMappedPhysicalAddress(
                             domain,
                             bus,
                             device,
                             function,
                             descriptor,
                             0);
            }
            else
            {
                physicalAddr = Platform::GetPhysicalAddress(descriptor, 0 /*offset*/);
            }

            MASSERT(memoryBlocks.count(physicalAddr) == 0 &&
                    "Two allocations at the same address???");
            memoryBlocks.emplace(physicalAddr, MemBlock(allocationSize, descriptor));

            // Do the error checking after adding to the map so that we are sure
            // to free all allocated descriptors, even if we encounter an error
            if (physicalAddr == 0)
            {
                error = StrPrintf("%s - Failed to retrieve physical address of "
                            "descriptor %zu with size %zu bytes and offset 0",
                            MODS_FUNCTION, reinterpret_cast<size_t>(descriptor),
                            allocationSize);
                break;
            }
        }

        // No need to check contiguity if we haven't even allocated
        // enough total memory yet
        allocatedSize += allocationSize;
        if (allocatedSize < totalRequestedMem)
        {
            continue;
        }

        numFullBlocks = GroupBlocks(&contiguousBlocks, memoryBlocks, requestedSizeOneBlock);
    }

    // If allocation failed at some point, free all the allocated memory
    if (!error.empty())
    {
        MASSERT(numFullBlocks < numBlocksRequested);
        for (const auto& memBlock : memoryBlocks)
        {
            Platform::FreePages(const_cast<void*>(memBlock.second.descriptor));
        }
        throw mem_manager_error(error);
        return {};
    }

    // Merge all sub-blocks into single blocks
    // Do it separately from the next loop so that we can handle failures
    for (ContiguousBlock& block : contiguousBlocks)
    {
        if (block.sizeBytes >= requestedSizeOneBlock)
        {
            RC rc;
            void* mergedDescriptor = nullptr;
            rc = Platform::MergePages(&mergedDescriptor,
                const_cast<void**>(block.descriptors.data()),
                const_cast<void**>(block.descriptors.data() + block.descriptors.size()));

            if (rc != RC::OK)
            {
                error = StrPrintf("%s - Failed to merge %zu blocks with total "
                    "size %zu bytes",
                    MODS_FUNCTION, block.descriptors.size(), block.sizeBytes);
                break;
            }

            PHYSADDR physicalAddr = 0x0;
            if (isIOMMUEnabled)
            {
                physicalAddr = Platform::GetMappedPhysicalAddress(
                             domain,
                             bus,
                             device,
                             function,
                             mergedDescriptor,
                             0);
            }
            else
            {
                physicalAddr = Platform::GetPhysicalAddress(mergedDescriptor, 0 /*offset*/);
            }

            if (physicalAddr == 0)
            {
                error = StrPrintf("%s - Failed to retrieve physical address of "
                    "merged descriptor %zu with size %zu bytes",
                    MODS_FUNCTION, reinterpret_cast<size_t>(mergedDescriptor),
                    block.sizeBytes);
                break;
            }

            // If the merge succeeded, we only have one valid descriptor left.
            block = ContiguousBlock(physicalAddr, block.sizeBytes,
                                    static_cast<const void*>(mergedDescriptor));
        }
    }

    // TODO : we could potentially recover here if we only failed on a merge
    // It depends how important this would be and how often we hit this issue.
    // For now, if we fail, we free all the memory and error out

    MemAddresses memAddresses;
    memAddresses.reserve(numFullBlocks);
    bool failed = !error.empty();
    // Save full chunks and free the rest
    for (const ContiguousBlock& block : contiguousBlocks)
    {
        if ((!failed) && (block.sizeBytes >= requestedSizeOneBlock))
        {
            // Save it as a complete block
            memAddresses.emplace_back(
                reinterpret_cast<size_t>(block.descriptors.front()),
                block.startingPhysicalAddress,
                block.sizeBytes);
        }
        else
        {
            for (const void* descriptor : block.descriptors)
            {
                // If the merge process failed, it might have set some of the
                // descriptors to nullptr
                if (descriptor)
                {
                    Platform::FreePages(const_cast<void*>(descriptor));
                }
            }
        }
    }

    if (failed)
    {
        throw mem_manager_error(error);
        return {};
    }

    return memAddresses;
}

/*static*/ void MemDevice::Free
(
    MemAddresses* pMem
)
{
    MASSERT(pMem);
    for (const auto& block : *pMem)
    {
        Platform::FreePages(reinterpret_cast<void*>(block.id));
    }
    pMem->clear();
}

size_t MemDevice::read(char* mem, size_t size, size_t offset)
{
    MemMapper reader(reinterpret_cast<void*>(m_Descriptor),
                     offset, size,  MemMapper::MODE::RDONLY);
    Platform::MemCopy(mem, reader.rdPtr(), size);
    return size;
}

bool MemDevice::write(const char* mem, size_t size, size_t offset)
{
    MemMapper writer(reinterpret_cast<void*>(m_Descriptor),
                     offset, size, MemMapper::MODE::RDWR);
    Platform::MemCopy(writer.wrPtr(), mem, size);
    return size;
}

bool MemDevice::write(const unsigned char* mem, size_t size, size_t offset)
{
    return this->write(reinterpret_cast<const char*>(mem), size, offset);
}

size_t MemDevice::DumpToFile(FILE *outfile, size_t size, size_t offset)
{
    vector<char>localBuffer(size, 0);

    read(localBuffer.data(), size, offset);
    return fwrite(localBuffer.data(), sizeof(char), size, outfile);
}

};
