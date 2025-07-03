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
#pragma once

#include <MEM_Manager/mem_dev.h>  // MAL's interface class
#include <MEM_Manager/Parameters.h>

class GpuSubdevice;

namespace MATHS_memory_manager_ns
{
    /**
     * This class handles a block of contiguous physical memory managed by
     * /dev/mods (the mods kernel driver which interfaces with physical memory)
     * in the way that MAL expects it (implementing the IMemDevice interface)
     **/
    struct MemDevice final : public IMemDevice
    {
    public:
        using MemAddresses = decltype(Parameters::_initialMem);
        struct MemRequirements
        {
            size_t blockSizeBytes = 0;
            uint32_t numBlocks = 0;
        };

        MemDevice(std::string fn, size_t start, size_t len, size_t bidx);
        MemDevice(const MemDevice&);
        ~MemDevice();

        /**
         * \brief Allocate physically-contiguous memory as specified by memRequirements
         * \return A vector of memory addresses. Empty vector if allocation fails
         */
        static MemAddresses AllocateMemory
        (
            const MemRequirements& memRequirements,
            const GpuSubdevice *gpuSubdev = nullptr,
            bool isIOMMUEnabled = false
        );

        /**
         * \brief Free previously-allocated memory and clears container
         */
        static void Free
        (
            MemAddresses* pMem
        );

        size_t read(char* mem, size_t size, size_t offset) override;
        bool write(const char* mem, size_t size, size_t offset) override;
        bool write(const unsigned char* mem, size_t size, size_t offset) override;
        size_t DumpToFile(FILE *outfile, size_t size, size_t offset);

        void lock() override { m_Mutex.lock(); }
        void unlock() override { m_Mutex.unlock(); }

        unsigned char *getPtrForRead(size_t size, size_t offset) override {return nullptr;}
        unsigned char *getPtrForWrite(size_t size, size_t offset) override {return nullptr;}

        // Getters
        size_t start_addr() override { return m_PhysicalAddr; }
        size_t length() override { return m_Length; }
        std::string path() override { return "/dev/mods"; }

        // Unsupported (and now temporarily commented out in the interface???)
        // std::string to_str(bool) override { throw; }
        // void dump(std::string fn, bool) override { throw; }

    private:
        size_t m_PhysicalAddr = 0;
        size_t m_Length = 0;
        size_t m_Descriptor = 0;
        std::relwrsive_mutex m_Mutex;   // To make the class thread-safe
    };
};

