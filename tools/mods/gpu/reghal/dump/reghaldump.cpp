/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "../reghaltable.h"
#include "g_lwconfig.h"
#include "core/include/zlib.h"
#include <algorithm>
#include <assert.h>
#include <string.h>
#include <vector>

namespace RegHalTableInfo
{
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
        extern const RegHalTable::Table g_ ## GpuId;
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU

#define DEFINE_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, ...) \
        extern const RegHalTable::Table g_ ## LwId;
#define DEFINE_OBS_LWL_DEV(...) // don't care
#include "core/include/lwlinklist.h"
#undef DEFINE_LWL_DEV
}

namespace
{
    struct Table
    {
        const RegHalTable::Table* pOriginalTable = nullptr;
        std::vector<UINT08>       compressedTable;
    };

    typedef std::map<UINT32, Table>  Tables;
    typedef std::map<UINT32, UINT32> DeviceRemap;

    void AddDeviceTable(Tables*                   pTables,
                        DeviceRemap*              pRemap,
                        UINT32                    devId,
                        const RegHalTable::Table& reghal)
    {
        // Deduplicate identical RegHal tables
        for (const auto& uniqueTable : *pTables)
        {
            const RegHalTable::Table& uniqueReghal = *uniqueTable.second.pOriginalTable;
            const size_t tableBytes = reghal.size * sizeof(*reghal.pElements);
            if ((reghal.size == uniqueReghal.size) &&
                (0 == memcmp(reghal.pElements, uniqueReghal.pElements, tableBytes)))
            {
                // If the current table is the same as another one,
                // write the device id of the other table, which indicates
                // it's a duplicate.
                (*pRemap)[devId] = uniqueTable.first;
                return;
            }
        }

        // Write the device id of the current table.  When the index of the
        // entry is the same as the target, it means that it's a unique table
        // which was not deduplicated.
        (*pRemap)[devId]                 = devId;
        (*pTables)[devId].pOriginalTable = &reghal;
    }

    void GetDeviceTables(Tables* pTables, DeviceRemap* pRemap)
    {
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
        AddDeviceTable(pTables, pRemap, Device::GpuId, RegHalTableInfo::g_ ## GpuId);
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU

#define DEFINE_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, ...) \
        AddDeviceTable(pTables, pRemap, Device::LwId, RegHalTableInfo::g_ ## LwId);
#include "core/include/lwlinklist.h"
#undef DEFINE_LWL_DEV
    }

    void AppendBytes(std::vector<UINT08>* pBuf, const void* ptr, size_t size)
    {
        const UINT08* const pData = static_cast<const UINT08*>(ptr);
        pBuf->insert(pBuf->end(), pData, pData + size);
    }

    template<size_t size, typename T>
    class DeltaEncoder
    {
        public:
            explicit DeltaEncoder(std::vector<UINT08>* pBuf) : m_Buf(*pBuf) { }

            void Append(const T& value)
            {
                static_assert(sizeof(T) == size, "bad size");

                const T delta = value - m_Prev;
                m_Prev = value;

                const UINT08* const pValue = static_cast<const UINT08*>(static_cast<const void*>(&delta));
                AppendBytes(&m_Buf, pValue, size);
            }

        private:
            std::vector<UINT08>& m_Buf;
            T                    m_Prev = 0;
    };

    bool CreateCompressedTable(UINT32                    devId,
                               const RegHalTable::Table& table,
                               std::vector<UINT08>*      pOutput,
                               bool                      useGzip)
    {
        // Before we compress the RegHal table with gzip, we rearrange the
        // data to significantly improve compression rates:
        //
        // * We dump every field individually.  So first we dump the regIndex
        //   field for all entries, then we dump arrayIndexes[0], and so on.
        //   This way conselwtive values belong to the same field.
        //   A single field varies rarely between elements, which is easier to compress.
        // * We write deltas between conselwtive field values.  Fields for subsequent
        //   table entries increase monotonically or don't increase at all, so
        //   the delta encoding improves compression rates, because it results
        //   in a lot of identical data being compressed.

        const RegHalTable::Element*       pElement = table.pElements;
        const RegHalTable::Element* const pEnd     = pElement + table.size;

        std::vector<UINT08> buf;

        {
            DeltaEncoder<4, UINT32> encoder(&buf);
            for (pElement = table.pElements; pElement < pEnd; ++pElement)
            {
                encoder.Append(pElement->regIndex);
            }
        }

        static_assert(RegHalTable::MAX_INDEXES == 4, "unexpected MAX_INDEXES");

        for (UINT32 i = 0; i < RegHalTable::MAX_INDEXES; i++)
        {
            DeltaEncoder<2, UINT16> encoder(&buf);
            for (pElement = table.pElements; pElement < pEnd; ++pElement)
            {
                encoder.Append(pElement->arrayIndexes[i]);
            }
        }

        {
            DeltaEncoder<4, UINT32> encoder(&buf);
            for (pElement = table.pElements; pElement < pEnd; ++pElement)
            {
                encoder.Append(static_cast<UINT32>(pElement->domain));
            }
        }
        {
            DeltaEncoder<4, UINT32> encoder(&buf);
            for (pElement = table.pElements; pElement < pEnd; ++pElement)
            {
                encoder.Append(pElement->capabilityId);
            }
        }
        {
            DeltaEncoder<4, UINT32> encoder(&buf);
            for (pElement = table.pElements; pElement < pEnd; ++pElement)
            {
                encoder.Append(pElement->flags);
            }
        }
        {
            DeltaEncoder<4, UINT32> encoder(&buf);
            for (pElement = table.pElements; pElement < pEnd; ++pElement)
            {
                encoder.Append(pElement->regValue);
            }
        }
        const UINT32 decompressedSize = static_cast<UINT32>(buf.size());
        const UINT32 numElements      = static_cast<UINT32>(table.size);

        // Compress the reghal table with gzip
        if (useGzip)
        {
            pOutput->resize(buf.size());
            unsigned long destSize = pOutput->size();
            if (Z_OK != compress2(&(*pOutput)[0], &destSize, &buf[0], buf.size(), Z_BEST_COMPRESSION))
            {
                fprintf(stderr, "Compression failed\n");
                return false;
            }
            pOutput->resize(destSize);
        }
        else
        {
            *pOutput = move(buf);
        }
        const UINT32 compressedSize = static_cast<UINT32>(pOutput->size());

        // Write the header at the end of the buffer
        // The header consists of:
        // - MODS device id, UINT32, little-endian
        // - Compressed body size, UINT32, little-endian
        // - Uncompressed body size, UINT32, little-endian
        // - Number of entries in the RegHal table, UINT32, little-endian
        AppendBytes(pOutput, &devId, 4U);
        AppendBytes(pOutput, &compressedSize, 4U);
        AppendBytes(pOutput, &decompressedSize, 4U);
        AppendBytes(pOutput, &numElements, 4U);

        // Swap the header and the body in-place, so that the header comes first
        std::rotate(pOutput->begin(), pOutput->begin() + compressedSize, pOutput->end());

        return true;
    }

    bool CreateCompressedTables(Tables* pTables, bool useGzip)
    {
        for (auto& table : *pTables)
        {
            if (!CreateCompressedTable(table.first,
                                       *table.second.pOriginalTable,
                                       &table.second.compressedTable,
                                       useGzip))
            {
                return false;
            }
        }
        return true;
    }

    bool Write(FILE* file, const void* ptr, size_t size)
    {
        if (fwrite(ptr, 1, size, file) != size)
        {
            perror(nullptr);
            return false;
        }
        return true;
    }

    bool EmitHexBytes(FILE* file, const std::vector<UINT08>& buf)
    {
        static constexpr unsigned numPerLine = 32;
        char output[numPerLine * 4U + 1U];

        size_t pos = 0;
        while (pos < buf.size())
        {
            int outputSize = 0;
            const size_t lineEnd = std::min(pos + numPerLine, buf.size());
            for (; pos < lineEnd; ++pos)
            {
                const int numPrinted = snprintf(&output[outputSize],
                                                sizeof(output) - outputSize,
                                                "%u,",
                                                buf[pos]);
                outputSize += numPrinted;
            }
            assert(outputSize < sizeof(output));
            output[outputSize++] = '\n';

            if (!Write(file, output, outputSize))
            {
                return false;
            }
        }

        return true;
    }

    bool DumpTablesToFile(FILE* file, const Tables& tables, const DeviceRemap& remap)
    {
        static const char header[] =
            "namespace RegHalTableInfo\n"
            "{\n"
            "    static const unsigned char g_CompressedRegHalTables[] = {\n";

        if (!Write(file, header, sizeof(header) - 1))
        {
            return false;
        }

        std::vector<UINT08> dupTable;

        unsigned numUniqueChips    = 0;
        unsigned numDuplicateChips = 0;
        unsigned totalSize         = 0;

        for (const auto& chip : remap)
        {
            // Dump unique table
            if (chip.first == chip.second)
            {
                ++numUniqueChips;

                const auto tableIt = tables.find(chip.first);
                assert(tableIt != tables.end());
                const Table& table = tableIt->second;

                static constexpr size_t origElemSize = sizeof(*table.pOriginalTable->pElements);

                printf("RegHal table for chip 0x%x: %u elements, "
                       "uncompressed %u bytes, compressed %u bytes\n",
                       chip.first,
                       static_cast<unsigned>(table.pOriginalTable->size),
                       static_cast<unsigned>(table.pOriginalTable->size * origElemSize),
                       static_cast<unsigned>(table.compressedTable.size()));

                totalSize += static_cast<unsigned>(table.compressedTable.size());

                if (!EmitHexBytes(file, table.compressedTable))
                {
                    return false;
                }
            }
            // Dump remap index for duplicate table
            else
            {
                ++numDuplicateChips;

                const UINT32 remapId  = ~0U;

                printf("RegHal table for chip 0x%x mapped to chip 0x%x\n",
                       chip.first, chip.second);

                // This is a duplicate table, only write the header consisting of:
                // - MODS device id for this table, UINT32, little-endian
                // - Four bytes 0xFF which indicate this is a remapped entry
                // - MODS device id of the actual table that should be used, UINT32, little-endian
                dupTable.clear();
                AppendBytes(&dupTable, &chip.first,  4U);
                AppendBytes(&dupTable, &remapId,     4U);
                AppendBytes(&dupTable, &chip.second, 4U);

                totalSize += static_cast<unsigned>(dupTable.size());

                if (!EmitHexBytes(file, dupTable))
                {
                    return false;
                }
            }
        }

        static const char footer[] = "};\n}\n";

        if (!Write(file, footer, sizeof(footer) - 1))
        {
            return false;
        }

        printf("Found %u unique and %u duplicate RegHal tables\n",
               numUniqueChips, numDuplicateChips);
        printf("Total RegHal size: %u bytes\n", totalSize);

        return true;
    }
}

int main(int argc, char* argv[])
{
    if ((argc < 2) || (argc > 3))
    {
        fprintf(stderr, "Invalid number of parameters: %d, expected 1\n", argc - 1);
        fprintf(stderr, "Usage: reghaldump <OUTPUT_FILE> [--gzip=<true|false>]\n");
        return EXIT_FAILURE;
    }

    bool useGzip = true;
    if (argc == 3)
    {
        if (strcmp(argv[2], "--gzip=false") == 0)
        {
            useGzip = false;
        }
        else if (strcmp(argv[2], "--gzip=true") != 0)
        {
            fprintf(stderr,
                    "Invalid second parameter, expected --gzip=<true|false> but got %s\n",
                    argv[2]);
            return EXIT_FAILURE;
        }
    }

    Tables      tables;
    DeviceRemap remap;

    GetDeviceTables(&tables, &remap);

    if (!CreateCompressedTables(&tables, useGzip))
    {
        return EXIT_FAILURE;
    }

    FILE* file = fopen(argv[1], "w+");
    if (!file)
    {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    const int ret = DumpTablesToFile(file, tables, remap) ? EXIT_SUCCESS : EXIT_FAILURE;

    fclose(file);

    return ret;
}
