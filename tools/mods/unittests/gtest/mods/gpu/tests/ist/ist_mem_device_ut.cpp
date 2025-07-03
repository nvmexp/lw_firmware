/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/utility.h"
#include "core/include/tasker.h"
#include "gpu/tests/ist/ist_mem_device.h"  // dut

#define WE_NEED_LINUX_XP
#include "xp_linux_internal.h"
#include "core/include/xp.h"

#include "gtest/gtest.h"

#include <vector>
#include <chrono>               // For exelwtion time

using namespace MATHS_memory_manager_ns;
using namespace std;

namespace Xp
{
    extern RC InitZeroFd();
}

// An RAII class to start and stop MODS components correctly
class ModsBackgroundTasks
{
public:
    ModsBackgroundTasks() {}

    ~ModsBackgroundTasks()
    {
        CleanupMods();
    }

    // Light MODS init (Tasker and Kernel driver only)
    // Returns success
    bool InitMods()
    {
        if (m_ModsInitialized)
        {
            return true;
        }
        m_ModsInitialized = true;

        // Start our cooperative multithreader.
        if (RC::OK != Tasker::Initialize())
        {
            cout << "Tasker::Initialize failed!" << endl;
            return false;
        }

        // Open the MODS kernel driver
        vector<string> args(1, "./mods");
        Xp::InitZeroFd();
        if (RC::OK != Xp::LinuxInternal::OpenDriver(args))
        {
            cout << ("Failed opening driver") << endl;
            return false;
        }

        return true;
    }

private:
    void CleanupMods()
    {
        if (!m_ModsInitialized)
        {
            return;
        }
        m_ModsInitialized = false;

        Tasker::Cleanup();

        if (RC::OK != Xp::LinuxInternal::CloseModsDriver())
        {
            cout << ("Failed closing MODS driver") << endl;
        }
    }

    bool m_ModsInitialized = false;
};
ModsBackgroundTasks g_ModsBackgroundTasks;

namespace
{
    template<typename T, int size>
    constexpr int ArraySize(T(&)[size]) { return size; }

    // Get access to private features
    struct Parameters_Mock : public Parameters
    {
    public:
        using MemAddresses = decltype(Parameters::_initialMem);
    };
    using MemAddresses = Parameters_Mock::MemAddresses;
    using MemAddress = MemAddresses::value_type;

    void Debug(const string& str)
    {
        constexpr bool s_PrintDebug = true; // Toggle manually to see/hide progress prints
        if (s_PrintDebug)
        {
            cout << str << '\n';
        }
    }

    // Allocate multiple chunks of a given size
    MemAddresses Allocate(const uint32_t numChunks, const size_t chunkSize_bytes)
    {
        using namespace std::chrono;
        auto start = high_resolution_clock::now();

        MemDevice::MemRequirements memParams = { chunkSize_bytes, numChunks };
        MemAddresses out = MemDevice::AllocateMemory(memParams);
        if (out.size() != numChunks) throw;

        auto end = high_resolution_clock::now();
        uint64_t millis = duration_cast<std::chrono::milliseconds>(end - start).count();
        Debug("=> " + to_string(millis) + "ms taken to allocate " + to_string(numChunks) + " " +
              to_string(static_cast<float>(chunkSize_bytes) / static_cast<float>(1_MB)) +
              " MB chunk(s)\n");

        return out;
    }

    // Allocate one contiguous chunk only
    MemAddress Allocate(const size_t chunkSize_bytes)
    {
        const MemAddresses& out = Allocate(1, chunkSize_bytes);
        return out[0];
    }
}

// NOTE: Disabling all tests in this test set by default because these cannot
// be run on the DVS machines (no sudo, no kernel driver), so they can only be
// run locally (in which case you can re-enable them)
TEST(MemDevice, SimpleAllocations)
{
    // Init MODS (Tasker and Kernel driver)
    if (!g_ModsBackgroundTasks.InitMods())
    {
        return;
    }

    // Test getters
    {
        SCOPED_TRACE("Testing getters");
        constexpr size_t numBytes = 10_MB;
        MemDevice memDevice("unused", 0, numBytes, 0);
        ASSERT_TRUE(memDevice.start_addr() == 0);
        ASSERT_TRUE(memDevice.length() == numBytes);
        ASSERT_TRUE(memDevice.path() == "/dev/mods");
        Debug("+ MemDevice getters are working");
    }

    // Test small allocation + write + read
    {
        SCOPED_TRACE("Testing small allocations");
        constexpr char data[] = "abcd";
        constexpr size_t numChars = ArraySize(data)-1;

        constexpr size_t numBytes = 10;
        MemAddress chunk(0, 0, 0);
        ASSERT_NO_THROW(chunk = Allocate(numBytes));
        DEFER
        {
            MemAddresses chunks(1, chunk);
            MemDevice::Free(&chunks);
            Debug("+ Chunk with id " + to_string(chunk.id) +
                  " freed successfully");
        };
        MemDevice memDevice("", chunk.physicalAddress, chunk.sizeBytes, chunk.id);
        ASSERT_TRUE(memDevice.start_addr() != 0);
        Debug("+ Allocating " + to_string(numBytes) + " bytes (chunk id = " +
              to_string(chunk.id) + ") and assigning them to a MemDevice worked");

        // Test write
        ASSERT_TRUE(memDevice.write(data, numChars, 0));
        Debug("+ Writing " + to_string(numChars) +
              " chars to a small MemDevice worked");

        // Test partial read
        {
            char buff[numChars + 1] = { 0 };
            ASSERT_EQ(numChars, memDevice.read(buff, numChars, 0))
                << "read = " << numChars << " chars; buff = " << buff;
            ASSERT_STREQ(data, buff) << "buff = " << buff;
            Debug("+ Reading back " + to_string(numChars) +
                  " chars ('" + buff + "') worked");
        }

        // Test read of entire buffer
        {
            char buff[numBytes] = { 0 };
            ASSERT_EQ(numBytes, memDevice.read(buff, numBytes, 0))
                << "read = " << numBytes << " chars; buff = " << buff;
            buff[numChars] = 0;
            ASSERT_STREQ(data, buff) << "buff = " << buff;
            Debug("+ Reading back the entire " + to_string(numBytes) +
                  " bytes ('" + buff + "') also worked");
        }
    }

    // Test larger allocation and write offset
    {
        SCOPED_TRACE("Testing larger allocations");
        constexpr size_t numBytes = 10_MB;
        MemAddress chunk(0, 0, 0);
        ASSERT_NO_THROW(chunk = Allocate(numBytes));
        DEFER
        {
            MemAddresses chunks(1, chunk);
            MemDevice::Free(&chunks);
            Debug("+ Chunk with id " + to_string(chunk.id) +
                  " freed successfully");
        };
        MemDevice memDevice("", chunk.physicalAddress, chunk.sizeBytes, chunk.id);
        Debug("+ Allocating " + to_string(numBytes) + " bytes (chunk id = " +
              to_string(chunk.id) + ") and assigning them to a MemDevice worked");

        // Clear the memory
        vector<char> fullSizeBuff(numBytes, 0);
        ASSERT_TRUE(memDevice.write(fullSizeBuff.data(), numBytes, 0));
        Debug("+ Writing " + to_string(numBytes) + " zeros to clear that memory block");

        // Write something at an arbitrary offset
        constexpr char data[] = "abcd";
        constexpr size_t offset = 10_KB + 123;
        constexpr size_t numChars = ArraySize(data) - 1;
        ASSERT_TRUE(memDevice.write(data, numChars, offset));
        Debug("+ Writing " + to_string(numChars) +
              " chars at offset " + to_string(offset) + " worked");

        // Test read at offset
        {
            char buff[numChars + 1] = { 0 };
            ASSERT_EQ(numChars, memDevice.read(buff, numChars, offset))
                << "read = " << numChars << " chars; buff = " << buff;
            ASSERT_STREQ(data, buff) << "buff = " << buff;
            Debug("+ Reading back " + to_string(numChars) + " chars at offset "
                  + to_string(offset) + " worked ('" + buff + "')");
        }

        // Test partial read from start address
        {
            constexpr size_t numBytesRead = offset + numChars;
            char buff[numBytesRead + 1] = { 0 };
            ASSERT_EQ(numBytesRead, memDevice.read(buff, numBytesRead, 0))
                << "read = " << numBytesRead << " chars; buff = " << buff;
            ASSERT_STREQ(&(buff[offset]), data) << "buff = " << &(buff[offset]);
            Debug("+ Reading from the start of the block (no offset) also "
                  " worked ('" + string(&(buff[offset])) + "')");
        }

        // Make sure there is nothing before or after the written memory
        {
            ASSERT_EQ(numBytes, memDevice.read(fullSizeBuff.data(), numBytes, 0))
                << "read = " << numBytes << " chars; (buff is too large to display)";
            for (size_t i = 0; i < offset; ++i)
            {
                ASSERT_TRUE(fullSizeBuff[i] == 0) << " at index " << i;
            }
            for (size_t i = offset + numChars; i < numBytes; ++i)
            {
                ASSERT_TRUE(fullSizeBuff[i] == 0) << " at index " << i;
            }
            Debug("  ... and it is confirmed that all other bytes are still 0");
        }
    }
}

TEST(MemDevice, ComplexAllocations)
{
    if (!g_ModsBackgroundTasks.InitMods())
    {
        return;
    }

    // Test allocating multiple chunks
    {
        SCOPED_TRACE("Testing multiple chunks");

        constexpr size_t numBytes = 32_MB;
        constexpr size_t numChunks = 5;
        MemAddresses chunks;
        ASSERT_NO_THROW(chunks = Allocate(numChunks, numBytes));
        DEFER
        {
            const size_t size = chunks.size();
            MemDevice::Free(&chunks);
            Debug("+ " + to_string(size) + " chunks freed successfully");
        };

        vector<MemDevice> memDevices;
        for (MemAddress& chunk : chunks)
        {
            memDevices.emplace_back("", chunk.physicalAddress, chunk.sizeBytes, chunk.id);
        }
        Debug("+ Allocating " + to_string(numChunks) + " chunks of " +
              to_string(numBytes) + " bytes and assigning them to MemDevices worked");

        constexpr char data[] = "agjkdsfzg143543asfg";
        constexpr size_t offset = 4_MB - 2; // crossing allocations
        constexpr size_t numChars = ArraySize(data) - 1;
        for (MemDevice& memDevice : memDevices)
        {
            ASSERT_TRUE(memDevice.start_addr() != 0);

            // Test write
            ASSERT_TRUE(memDevice.write(data, numChars, offset));
            char buff[numChars + 1] = { 0 };

            // Test partial read
            ASSERT_EQ(numChars, memDevice.read(buff, numChars, offset));
            ASSERT_STREQ(data, buff);

            // Test read of entire buffer
            vector<char> fullSizeBuff(numBytes, 0);
            ASSERT_EQ(numBytes, memDevice.read(fullSizeBuff.data(), numBytes, 0));
            fullSizeBuff[offset + numChars] = 0;
            ASSERT_STREQ(data, fullSizeBuff.data() + offset);
        }
        Debug("+ Writing and reading back " + to_string(numChars) + " chars at offset "
                + to_string(offset) + " worked for every memDevice");
    }
}
TEST(MemDevice, VeryLargeAllocations)
{
    // Init MODS (Tasker and Kernel driver)
    if (!g_ModsBackgroundTasks.InitMods())
    {
        return;
    }

    // 1GB
    {
        SCOPED_TRACE("Testing 1GB allocation");

        constexpr size_t numBytes = 1_GB;
        MemAddress chunk(0, 0, 0);
        ASSERT_NO_THROW(chunk = Allocate(numBytes));
        DEFER
        {
            MemAddresses chunks(1, chunk);
            MemDevice::Free(&chunks);
            Debug("+ Chunk with id " + to_string(chunk.id) +
                  " freed successfully");
        };
        MemDevice memDevice("", chunk.physicalAddress, chunk.sizeBytes, chunk.id);
        ASSERT_TRUE(memDevice.start_addr() != 0);
        Debug("+ Allocating " + to_string(numBytes) + " bytes (chunk id = " +
              to_string(chunk.id) + ") and assigning them to a MemDevice worked");
    }
}
TEST(DISABLED_MemDevice, ErrorWR)
{
    // IMPLEMENTME
    // Test Read/Write corner-cases like buffer overflows, out-of-bounds, etc
}
