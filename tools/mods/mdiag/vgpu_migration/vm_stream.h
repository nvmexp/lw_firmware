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
#ifndef VM_STREAM_H
#define VM_STREAM_H

#include "mdiag/vgpu_migration/vm_mem.h"
// ALIGN_UP
#include "core/include/utility.h"

namespace MDiagVmScope
{
    // MemStream
    //     A mem Stream connects client(s) to mem for reading/writing.
    //
    // StreamClient:
    //     A StreamClient reads/writes memory through a MemStream.
    //
    // The MemStream to mem or a StreamClient to MemStream, each end
    // can have its own bandwidth, and MemStream deals with the
    // different read/write speed.
    // E.g., StreamClient side speed may be 4-byte or 32-byte per each read/write,
    // while MemStream to mem side could be 256-byte or 4K page size per
    // each read/write, etc.
    // This way a StreamClient can always read/write random bytes,
    // even when actual client side bandwidth is different from mem side one.
    // MemStream provides interface to read/write random number of
    // bytes from/to the mem, but not as stream bandwidth bytes.
    // 
    class MemStream
    {
    public:
        // Use 4K page size by default.
        static const size_t PageSize = 0x1000UL;
        static const size_t PageMask = PageSize - 1;
        // Pattern size. Pattern for verifying mem read/write.
        static const size_t PatterlwerifSize = 2 * sizeof(UINT64);

        // The max bandwidth limit for sanity check purpose.
        static const size_t MaxBandwidth = PageSize;
        static bool BandwidthSanityCheck(size_t bandwidth);
        // Mem copy a block.
        // The underlying actual copy uses below MemRW interface.
        static void MemCopy(volatile void *Dst, const volatile void *Src, size_t Count);

        // Global config.
        class GlobConfig
        {
        public:
            // Set to verification mode for each mem read/write transaction.
            void SetVerif(bool bVerif);
            bool NeedVerif() const { return m_bVerif; }

        private:
            bool m_bVerif = false;

            // Put other config items here.
        };

        // StreamClient config, each client can have its own bandwidth.
        // E.g. 4-byte or 32-byte, useful when print format required on
        // client side.
        struct CliConfig
        {
            size_t bandwidth = PageSize;
            bool BandwidthSanityCheck() const;
        };

        // Each MemStream to mem can have its own bandwidth.
        struct Config : public CliConfig
        {
            const GlobConfig* pGlobCfg;
        };

    public:
        const Config* GetConfig() const { return &m_Cfg; }
        // Open a MemStream.
        void Open(const FbMemSeg& fbSeg, bool bRead, size_t bandwidth);
        // Read some random length of mem data from the stream.
        // The stream hides backend mem read details such as read on a per
        // page basis, etc.
        bool ReadMem(void* pDataBuf, size_t *pBufSize);
        // Write some random length of mem data to the stream.
        // The stream hides backend mem write details such as read on a per
        // page basis, etc.
        bool WriteMem(const void* pDataBuf, size_t bufSize);
        // Flush wirte to mem which might still be remained in the stream.
        bool FlushWrites();

    private:
        // The interface for MemStream to actually read/write BAR1 mapped or
        // Surface mem.
        // It callwlates MemStream's buffer PA/VA mapping and deals with the client
        // side different read/write speed or bandwidth from stream to mem side.
        // It's aware of MemStream bandwidth only for debugging prupose.
        class MemRW
        {
        public:
            // The underlying actual copy/read/write could use Platform::MemCopy/VirtualRd/Wr
            // or another function as needed.
            // Wrapped memcopy/read/write to check MaxBandwidth.
            static void MemCopy(volatile void *Dst, const volatile void *Src, size_t Count);
            static void MemRd(const volatile void *Addr, void *Data, UINT32 Count);
            static void MemWr(volatile void *Addr, const void *Data, UINT32 Count);

            // Set up for a new transaction.
            void Setup(PHYSADDR paOff,
                void* pVA,
                size_t memSize,
                size_t bandwidth);
            // Read/write deal with speed different from each end.
            void Read(void *Data, UINT32 Count);
            void Write(const void *Data, UINT32 Count);

        private:
            // Get current PA offset at which now we reading/writing.
            UINT64 GetLwrPaOffset() const
            {
                return m_PaOff + m_LwrLoc;
            }
            // Get current VA offset at which now we reading/writing.
            UINT08* GetLwrVaOffset() const
            {
                return m_pVA + m_LwrLoc;
            }
            // If the input PA offset in the Stream covered range?
            bool IsPaInStream(UINT64 paOff) const;
            const UINT08* GetDataBufOffset(UINT64 paOff, const void* pDataBuf) const;

            //  Mem PA offset
            UINT64 m_PaOff = 0;
            // Mem VA
            UINT08* m_pVA = nullptr;
            // Mem length.
            size_t m_MemSize = 0;
            // Current read/write localtion offset into stream covered range.
            size_t m_LwrLoc = 0;

            size_t m_Bandwidth = 0;
        };

        MemRW* GetMemRW() { return &m_MemRW; }
        UINT08* GetDataBuf() { return &m_DataBuf[0]; }
        size_t DataBufSize() const { return m_DataBuf.size(); }
        size_t DataBufRemainSize() const
        {
            return DataBufSize() - m_DataBufLwr;
        }

        Config m_Cfg;
        MemRW m_MemRW;
        MemData m_DataBuf;
        size_t m_DataBufLwr = 0;
    };

    // The Stream Client type.
    class StreamClient
    {
    public:
        virtual ~StreamClient() {}

        // StreamClient might need to save/load mem segment meta info.
        virtual RC SaveMeta(const MemMeta& meta) = 0;
        virtual RC LoadMeta(MemMeta* pMeta) = 0;
        virtual const MemStream::CliConfig* GetConfig() const = 0;
        // Callback called by MemStream that the Stream started a new
        // mem segment read/write.
        virtual void OnStartSeg(const MemSegDesc& seg) = 0;
        // Data read from MemStream, passing over to the Client to save 
        virtual RC SaveData(PHYSADDR addr, const void* pData) = 0;
        // Data loaded by the Client, passing over to MemStream to write back to mem.
        virtual RC LoadData(PHYSADDR* pAddr, void* pData) = 0;
        virtual void GetStreamData(MemData* pOut) const = 0;
        virtual void SetStreamData(const MemData& data) = 0;
        virtual void OnResizeBandwidth() = 0;
        // E.g. save data to file might need fflush during file writes.
        virtual void Flush() = 0;       

    public:
        // Wrapped memcopy to check MaxBandwidth.
        // The underlying actual copy could be memcpy() or others else.
        static void MemCopy(void *Dst, const void *Src, size_t Count);

        MemStream::CliConfig& GetConfigRef()
        {
            return *const_cast<MemStream::CliConfig*>(GetConfig());
        }
    };

    // A StreamClient needn't use file to save/restore to passing across
    // multiple MODS runs.
    // Only useful when save/restore happening in single sim or single MODS run.
    class StreamLocal : public StreamClient
    {
    public:
        virtual ~StreamLocal() {}

        virtual RC SaveMeta(const MemMeta& meta) { return OK; }
        virtual RC LoadMeta(MemMeta* pMeta) { return OK; }
        virtual const MemStream::CliConfig* GetConfig() const { return &m_Cfg; }
        virtual void OnStartSeg(const MemSegDesc& seg);
        virtual RC SaveData(UINT64 addr, const void* pData)
        {
            GetConfig()->BandwidthSanityCheck();
            const UINT08* pIn = static_cast<const UINT08*>(pData);
            std::copy(pIn, pIn + GetConfig()->bandwidth, m_Buf.begin() + m_LwrLoc);
            UpdateLwrLoc();

            return OK;
        }
        virtual RC LoadData(UINT64* pAddr, void* pData)
        {
            GetConfig()->BandwidthSanityCheck();
            UINT08* pOut = static_cast<UINT08*>(pData);
            std::copy(m_Buf.begin() + m_LwrLoc, m_Buf.begin() + m_LwrLoc + GetConfig()->bandwidth, pOut);
            *pAddr = GetLwrAddr();
            UpdateLwrLoc();
            
            return OK;
        }
        virtual void GetStreamData(MemData* pOut) const;
        virtual void SetStreamData(const MemData& data);
        virtual void OnResizeBandwidth();
        virtual void Flush() {}

    private:
        PHYSADDR GetLwrAddr() const { return m_Addr + m_LwrLoc; }
        void UpdateLwrLoc()
        { 
            GetConfig()->BandwidthSanityCheck();
            m_LwrLoc += GetConfig()->bandwidth;
        }

        MemData m_Buf;
        MemStream::CliConfig m_Cfg;
        UINT64 m_Addr = 0;
        size_t m_LwrLoc = 0;
    };

    // The FbCopyStream maps each required FB mem segment, and connects
    // a StreamClient to a MemStream and coordinates them to get work done.
    class FbCopyStream
    {
    public:
        // Connecting to a StreamClient.
        void SetStreamClient(StreamClient* pCli);
        // The FB mem mapper.
        void SetMemAlloc(MemAlloc* pAlloc);
        void SetMemStreamBandwidth(size_t bandwidth);
        size_t GetMemStreamBandwidth() const { return m_MemStreamBandwidth; }
        void SetClientBandwidth(size_t bandwidth);

        // Driving the StreamClient to save or load mem segments meta info.
        RC SaveMeta(const MemMeta& meta);
        RC LoadMeta(MemMeta* pMeta);
        // Driving the MemStream and StreamClient read/write flow.
        RC SaveData(const MemDescs& descs);
        RC LoadData(const MemDescs& descs);

    private:
        // A tool to verify if FB mem read/write Stream working at correct
        // expected PA offset, or the data content read/written is correct as well.
        class Verif
        {
        public:
            Verif(UINT64 segSize, bool bVerif)
            {
                SetNeedVerif(bVerif);
                if (NeedVerif())
                {
                    m_VerifBufSize = CalcVerifDataSize(size_t(segSize));
                    m_pVerifBuf.reset(new UINT08[m_VerifBufSize]);
                    m_VerifLwrLoc = 0;
                }
            }
            bool NeedVerif() const { return m_bNeedVerif; }
            const void* GetBuf() const { return m_pVerifBuf.get(); }
            size_t GetBufSize() const { return m_VerifBufSize; }
            void AddData(const void* pData, size_t size);

        private:
            static size_t CalcVerifDataSize(size_t segSize)
            {
                const size_t pgSize = MemStream::PageSize;
                return std::min(segSize, pgSize);
            }
            void SetNeedVerif(bool bVerif) { m_bNeedVerif = bVerif; }
        
            unique_ptr<UINT08> m_pVerifBuf;
            size_t m_VerifBufSize = 0;
            size_t m_VerifLwrLoc = 0;
            bool m_bNeedVerif = false;
        };

        MemStream* GetMemStream () { return &m_MemStream; }
        bool VerifyData(UINT64 paOff, size_t size, const void* pBuf, const char* pPoint) const;
        // Overwite FB mem with pattern to check mem writes going at expected PA offset exactly.
        // Recover original mem data upon verification done.
        RC FillPatterlwerif(UINT64 paOff, size_t size) const;
    
        unique_ptr<StreamClient> m_pCli;
        MemAlloc* m_pAlloc = nullptr;
        MemStream m_MemStream;
        size_t m_MemStreamBandwidth  = MemStream::PageSize;
        
    };

    struct FbCopyLine
    {
        static const size_t NumData = 8;
        static const size_t NumDataBytes = NumData * 4;
        // Number of chars per line: bytes * 2 + 16 + 12.
        static const size_t Len = ALIGN_UP(UINT32(NumDataBytes * 2 + 28), UINT32(8));
        static const UINT32 DmaCopyChunkSize = 8U << 20;

        char* GetLine() { return m_Line; }

    protected:
        char m_Line[Len];
    };

} // namespace MDiagVmScope

#endif

