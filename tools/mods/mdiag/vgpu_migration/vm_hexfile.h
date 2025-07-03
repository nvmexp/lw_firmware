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
#ifndef VM_HEXFILE_H
#define VM_HEXFILE_H

#include "mdiag/vgpu_migration/vm_stream.h"

namespace MDiagVmScope
{
    struct HexFileStreamCreator;

    // A StreamClient dump hex format mem data into a file.
    //
    // Needed when save/restore happening in different MODS runs
    // by passing over dump file across MODS runs.
    // Hexfile format FbCopyLine::NumData per line by default.
    //
    class StreamHexFile : public StreamClient
    {
    public:
        virtual ~StreamHexFile() {}

        virtual RC SaveMeta(const MemMeta& meta);
        virtual RC LoadMeta(MemMeta* pMeta);
        virtual const MemStream::CliConfig* GetConfig() const;
        virtual void OnStartSeg(const MemSegDesc& seg) {}
        virtual RC SaveData(UINT64 addr, const void* pData);
        virtual RC LoadData(UINT64* pAddr, void* pData);
        virtual void GetStreamData(MemData* pOut) const {}
        virtual void SetStreamData(const MemData& data) {}
        virtual void OnResizeBandwidth() {}
        virtual void Flush();

    public:
        void SetFile(FILE* pFile) { m_pFile = pFile; }
        UINT32 GetNumData() const { return m_NumData; }
        void SetNeedFlush(bool bNeedFlush) { m_bNeedFlush = bNeedFlush; }

    protected:
        friend struct MDiagVmScope::HexFileStreamCreator;
        StreamHexFile();
        void SetNumData(UINT32 num) { m_NumData = num; }
        FILE* GetFile() { return m_pFile; }

    private:
        bool NeedFlush() const { return m_bNeedFlush; }
        // File format: eight 4-byte data per line.
        RC SaveData8(UINT64 addr, const UINT32* pBuf);
        RC LoadData8(const char* pLine, UINT64* pAddr, UINT32* pBuf, INT32* pNum);
        // File format: one 4-byte data per line.
        RC SaveData1(UINT64 addr, const UINT32* pBuf);
        RC LoadData1(const char* pLine, UINT64* pAddr, UINT32* pBuf, INT32* pNum);

        MemStream::CliConfig m_Cfg;
        FILE* m_pFile = nullptr;

        UINT32 m_NumData = 8;
        bool m_bNeedFlush = true;
    };

    // The StreamClient creator.
    struct HexFileStreamCreator
    {
        static StreamHexFile* CreateHexFileStream(size_t numData);
    };

} // namespace MDiagVmScope

#endif

