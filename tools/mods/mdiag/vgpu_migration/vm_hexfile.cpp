/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Implements vGgu Migration save/restore hex file read/write.

#include "vm_hexfile.h"
#include "mdiag/sysspec.h"
#include "vgpu_migration.h"

//#define FB_COPY_FILE_HDR_8        "segment lines followed. Data line format: PA offset plus 8x 4-byte FB mem data in hex."
#define FB_COPY_FILE_HDR        "segment lines followed. Data line format: PA offset plus %u 4-byte FB mem data in hex."
// Prefix "0x" not required for hex data dump
#define FB_COPY_FILE_SEG        "%016llx,%016llx,segment PA offset,size."
// Line format: PA offset and 8x 4-byte mem data
#define FB_COPY_LINE_FMT        "%016llx %08x %08x %08x %08x %08x %08x %08x %08x"
// Line format: PA offset and 1x 4-byte mem data
#define FB_COPY_LINE_FMT_1        "%016llx %08x"

namespace MDiagVmScope
{
    StreamHexFile::StreamHexFile()
    {
        m_Cfg.bandwidth = FbCopyLine::NumDataBytes;
    }

    RC StreamHexFile::SaveMeta(const MemMeta& meta)
    {
        MASSERT(m_pFile != nullptr);

        fprintf(m_pFile, "%x," FB_COPY_FILE_HDR "\n",
            UINT32(meta.size()),
            GetNumData()
            );
        for (const auto& seg : meta)
        {
            MemSegDesc desc(seg);
            fprintf(m_pFile, FB_COPY_FILE_SEG "\n",
                desc.GetAddr(),
                desc.GetSize());
        }

        return OK;
    }

    static RC RestoreFbSegmentLines(FILE* pFile, size_t numSegs, MemMeta* pMeta)
    {
        RC rc;

        for (size_t i = 0; i < numSegs; i ++)
        {
            FbCopyLine fileLine;
            char* pRet = fgets(fileLine.GetLine(), UINT32(FbCopyLine::Len), pFile);
            if (!pRet && !feof(pFile))
            {
                // In case not EOF but file OP failed.
                ErrPrintf("%s, " vGpuMigrationTag " Failed to load FB segment from file, line (%s).\n",
                    __FUNCTION__,
                    fileLine.GetLine()
                    );
                return RC::SOFTWARE_ERROR;
            }

            PHYSADDR paOff = 0;
            UINT64 size = 0;
            const INT32 num = sscanf(fileLine.GetLine(), FB_COPY_FILE_SEG,
                    &paOff, &size);
            if (num != 2)
            {
                // In case read file line was a success but failed to parse the line.
                ErrPrintf("%s, " vGpuMigrationTag " Failed to parse FB segment from file, line (%s).\n",
                    __FUNCTION__,
                    fileLine.GetLine()
                    );
                return RC::SOFTWARE_ERROR;
            }
            pMeta->emplace_back(paOff, size);
            DebugPrintf("StreamHexFile::%s, " vGpuMigrationTag " loaded FB segment PA offset 0x%llx/%lluM size 0x%llx/%lluM from file, line (%s).\n",
                __FUNCTION__,
                paOff,
                paOff >> 20,
                size,
                size >> 20,
                fileLine.GetLine()
                );
        }

        return rc;
    }


    RC StreamHexFile::LoadMeta(MemMeta* pMeta)
    {
        MASSERT(m_pFile != nullptr);
        MASSERT(pMeta != nullptr);

        RC rc;

        FbCopyLine fileLine;
        char* pRet = fgets(fileLine.GetLine(), UINT32(FbCopyLine::Len), m_pFile);
        if (!pRet && !feof(m_pFile))
        {
            // In case not EOF but file OP failed.
            ErrPrintf("StreamHexFile::%s, " vGpuMigrationTag " Failed to load FB length from file.\n",
                __FUNCTION__
                );
            return RC::SOFTWARE_ERROR;
        }
        UINT32 numSegs = 0;
        UINT32 numData = 0;
        INT32 num = sscanf(fileLine.GetLine(), "%x," FB_COPY_FILE_HDR "\n", &numSegs, &numData);
        if (num < 1)
        {
            // In case read file line was a success but failed to parse the line.
            ErrPrintf("StreamHexFile::%s, " vGpuMigrationTag " Failed to load number of FB segments length from file, line (%s).\n",
                __FUNCTION__,
                fileLine.GetLine()
                );
            return RC::SOFTWARE_ERROR;
        }
        MASSERT(numSegs >= 1);
        DebugPrintf("StreamHexFile::%s, " vGpuMigrationTag " Loaded number of FB segments 0x%x from file, head (%s).\n",
            __FUNCTION__,
            numSegs,
            fileLine.GetLine()
            );

        CHECK_RC(RestoreFbSegmentLines(m_pFile, numSegs, pMeta));
        MASSERT(pMeta->size() == numSegs);

        return rc;
    }

    const MemStream::CliConfig* StreamHexFile::GetConfig() const
    {
        return &m_Cfg;
    }

    RC StreamHexFile::SaveData8(UINT64 addr, const UINT32* pBuf)
    {
        MASSERT(m_pFile != nullptr);
        MASSERT(pBuf != nullptr);

        //PA offset and 8x 4-byte mem data
        fprintf(m_pFile, FB_COPY_LINE_FMT "\n",
            addr,
            pBuf[0], pBuf[1], pBuf[2], pBuf[3],
            pBuf[4], pBuf[5], pBuf[6], pBuf[7]
            );

        return OK;
    }

    RC StreamHexFile::SaveData1(UINT64 addr, const UINT32* pBuf)
    {
        MASSERT(GetFile() != nullptr);
        MASSERT(pBuf != nullptr);

        //PA offset and 1x 4-byte mem data
        fprintf(m_pFile, FB_COPY_LINE_FMT_1 "\n",
            addr,
            pBuf[0]);

        return OK;
    }

    RC StreamHexFile::SaveData(UINT64 addr, const void* pData)
    {
        MASSERT(pData != nullptr);

        const UINT32* pBuf = static_cast<const UINT32*>(pData);
        switch(GetNumData())
        {
        case 8:
            return SaveData8(addr, pBuf);
        case 1:
            return SaveData1(addr, pBuf);
        default:
            ErrPrintf("StreamHexFile::%s, " vGpuMigrationTag " Not yet support for hex file dump %u num data per file line.\n",
                __FUNCTION__,
                GetNumData());
            break;
        }

        return RC::ILWALID_ARGUMENT;
    }

    RC StreamHexFile::LoadData8(const char* pLine, UINT64* pAddr, UINT32* pBuf, INT32* pNum)
    {
        MASSERT(m_pFile != nullptr);
        MASSERT(pBuf != nullptr);
        MASSERT(pNum != nullptr);

        RC rc;
        //PA offset and 8x 4-byte mem data
        *pNum = sscanf(pLine, FB_COPY_LINE_FMT,
                pAddr,
                &pBuf[0], &pBuf[1], &pBuf[2], &pBuf[3],
                &pBuf[4], &pBuf[5], &pBuf[6], &pBuf[7]
                );

        return rc;
    }

    RC StreamHexFile::LoadData1(const char* pLine, UINT64* pAddr, UINT32* pBuf, INT32* pNum)
    {
        MASSERT(m_pFile != nullptr);
        MASSERT(pBuf != nullptr);
        MASSERT(pNum != nullptr);

        RC rc;
        //PA offset and 1x 4-byte mem data
        *pNum = sscanf(pLine, FB_COPY_LINE_FMT_1,
                pAddr,
                &pBuf[0]);

        return rc;
    }

    RC StreamHexFile::LoadData(UINT64* pAddr, void* pData)
    {
        MASSERT(m_pFile != nullptr);
        MASSERT(pData != nullptr);

        RC rc;
        FbCopyLine fileLine;

        char* pRet = fgets(fileLine.GetLine(), UINT32(FbCopyLine::Len), m_pFile);
        if (!pRet && !feof(m_pFile))
        {
            // In case not EOF but file OP failed.
            ErrPrintf("StreamHexFile::%s, " vGpuMigrationTag " Failed to load FB mem data from file, line (%s).\n",
                __FUNCTION__,
                fileLine.GetLine()
                );
            return RC::SOFTWARE_ERROR;
        }

        UINT32* pBuf = static_cast<UINT32*>(pData);
        INT32 num = 0;
        switch(GetNumData())
        {
        case 8:
            CHECK_RC(LoadData8(fileLine.GetLine(), pAddr, pBuf, &num));
            break;
        case 1:
            CHECK_RC(LoadData1(fileLine.GetLine(), pAddr, pBuf, &num));
            break;
        default:
            ErrPrintf("StreamHexFile::%s, " vGpuMigrationTag " Not yet support for hex file dump %u num data per file line.\n",
                __FUNCTION__,
                GetNumData());
            return RC::ILWALID_ARGUMENT;
        }
        if (num != INT32(GetNumData()) + 1)
        {
            // In case read file line was a success but failed to parse the line.
            ErrPrintf("StreamHexFile::%s, " vGpuMigrationTag " Failed to load FB mem data from file, line (%s).\n",
                __FUNCTION__,
                fileLine.GetLine()
                );
            return RC::SOFTWARE_ERROR;
        }

        return rc;
    }

    void StreamHexFile::Flush()
    {
        if (NeedFlush())
        {
            MASSERT(m_pFile != nullptr);
            fflush(m_pFile);
        }
    }

    // The Creator
    
    StreamHexFile* HexFileStreamCreator::CreateHexFileStream(size_t numData)
    {
        StreamHexFile* pCli = new StreamHexFile;

        pCli->SetNumData(UINT32(numData));
        switch(numData)
        {
        case 1:
            {
                MemStream::CliConfig& cfg = pCli->GetConfigRef();
                cfg.bandwidth = 4;
            }
            break;
        default:
            break;
        }
        pCli->GetConfig()->BandwidthSanityCheck();

        return pCli;
    }

} // MDiagVmScope

