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

//! \file
//! \brief Implements vGgu Migration save/restore ops over pipe.

#include "vm_stream.h"
// LW_MMU_PTE_KIND_GENERIC_MEMORY*, drivers/common/inc/hwref/ampere/ga100/dev_mmu.h
#include "ampere/ga100/dev_mmu.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/utils.h"
#include "vgpu_migration.h"

namespace MDiagVmScope
{
    // MemRW
    void MemStream::MemRW::MemCopy(volatile void *Dst, const volatile void *Src, size_t Count)
    {
        MASSERT(MemStream::BandwidthSanityCheck(Count));
        Platform::MemCopy(Dst, Src, Count);
    }

    void MemStream::MemRW::MemRd(const volatile void *Addr, void *Data, UINT32 Count)
    {
        MASSERT(MemStream::BandwidthSanityCheck(Count));
        Platform::VirtualRd(Addr, Data, Count);
    }

    void MemStream::MemRW::MemWr(volatile void *Addr, const void *Data, UINT32 Count)
    {
        MASSERT(MemStream::BandwidthSanityCheck(Count));
        Platform::VirtualWr(Addr, Data, Count);
    }

    void MemStream::MemRW::Setup(UINT64 paOff,
        void* pVA,
        size_t memSize,
        size_t bandwidth)
    {
        MASSERT(pVA != 0 && memSize > 0);

        m_PaOff = paOff;
        m_pVA = static_cast<UINT08*>(pVA);
        m_MemSize = memSize;
        m_LwrLoc = 0;
        
        // More sanity check
        MASSERT(GetLwrPaOffset() == m_PaOff);
        MASSERT(GetLwrVaOffset() == m_pVA);
        MASSERT(MemStream::BandwidthSanityCheck(bandwidth));
        m_Bandwidth = bandwidth;
    }

    void MemStream::MemRW::Read(void *Data, UINT32 Count)
    {
        MASSERT(m_pVA && m_MemSize);
        MASSERT(m_LwrLoc + Count <= m_MemSize);
        MemRd(GetLwrVaOffset(), Data, Count);
        MASSERT(GetMDiagVmResources()->GetMemStreamGlobConfig() != 0);
        m_LwrLoc += Count;
    }

    void MemStream::MemRW::Write(const void *Data, UINT32 Count)
    {
        MASSERT(m_pVA && m_MemSize);
        MASSERT(m_LwrLoc + Count <= m_MemSize);
        MemWr(GetLwrVaOffset(), Data, Count);
        MASSERT(GetMDiagVmResources()->GetMemStreamGlobConfig() != 0);
        m_LwrLoc += Count;
    }

    const UINT08* MemStream::MemRW::GetDataBufOffset(UINT64 paOff, const void* pDataBuf) const
    {
        MASSERT(IsPaInStream(paOff));
        const UINT08* pIn = static_cast<const UINT08*>(pDataBuf);
        return pIn + (paOff - GetLwrPaOffset());
    }

    bool MemStream::MemRW::IsPaInStream(UINT64 paOff) const
    {
        return paOff >= GetLwrPaOffset() && paOff < GetLwrPaOffset() + m_Bandwidth;
    }

    ////////////////////////MemStream/////////////////////////

    bool MemStream::BandwidthSanityCheck(size_t bandwidth)
    {
        MASSERT(bandwidth > 0 && bandwidth <= MaxBandwidth);
        return bandwidth > 0 && bandwidth <= MaxBandwidth;
    }

    void MemStream::MemCopy(volatile void *Dst, const volatile void *Src, size_t Count)
    {
        MemRW::MemCopy(Dst, Src, Count);
    }

    void MemStream::GlobConfig::SetVerif(bool bVerif)
    {
        m_bVerif = bVerif;
        DebugPrintf("MemStream config, " vGpuMigrationTag " Set pipe read/write verify to %s.\n",
            m_bVerif ? "True" : "False");
    }

    void MemStream::Open(const FbMemSeg& fbSeg, bool bRead, size_t bandwidth)
    {
        const UINT64 paOff = fbSeg.GetAddr();
        void* pVA = fbSeg.pCpuPtr;
        const size_t memSize = size_t(fbSeg.GetSize());
        MASSERT(pVA && memSize);
        m_Cfg.bandwidth = bandwidth;
        // Sanity check
        GetConfig()->BandwidthSanityCheck();
        MASSERT(memSize >= GetConfig()->bandwidth);
        DebugPrintf("MemStream::%s, " vGpuMigrationTag " memSize 0x%llx bandwidth 0x%llx or 0x%llx.\n",
            __FUNCTION__,
            UINT64(memSize),
            GetConfig()->bandwidth,
            m_Cfg.bandwidth);
        GetMemRW()->Setup(paOff, pVA, memSize, bandwidth);
        if (DataBufSize() != bandwidth)
        {
            m_DataBuf.resize(bandwidth);
        }
        if (bRead)
        {
            m_DataBufLwr = DataBufSize();
        }
        else
        {
            m_DataBufLwr = 0;
        }
    }

    bool MemStream::ReadMem(void* pDataBuf, size_t* pBufSize)
    {
        MASSERT(pDataBuf && pBufSize && DataBufSize() >= *pBufSize);
        if (DataBufRemainSize() > 0)
        {
            if (*pBufSize > DataBufRemainSize())
            {
                *pBufSize = DataBufRemainSize();
            }
        }
        else
        {
            GetMemRW()->Read(GetDataBuf(), UINT32(DataBufSize()));
            m_DataBufLwr = 0;
            MASSERT(DataBufRemainSize() == DataBufSize());
        }
        MASSERT(*pBufSize <= DataBufRemainSize());
        StreamClient::MemCopy(pDataBuf, GetDataBuf() + m_DataBufLwr, *pBufSize);
        m_DataBufLwr += *pBufSize;
        
        return true;
    }

    bool MemStream::WriteMem(const void* pDataBuf, size_t bufSize)
    {
        MASSERT(DataBufSize() >= bufSize);
        size_t inLwr = 0;
        if (0 == DataBufRemainSize() || DataBufRemainSize() < bufSize)
        {
            if (DataBufRemainSize() > 0)
            {
                StreamClient::MemCopy(GetDataBuf() + m_DataBufLwr, static_cast<const UINT08*>(pDataBuf) + inLwr, DataBufRemainSize());
                inLwr += DataBufRemainSize();
            }
            MASSERT(DataBufRemainSize() == 0);
            GetMemRW()->Write(GetDataBuf(), UINT32(DataBufSize()));
            m_DataBufLwr = 0;
            MASSERT(DataBufRemainSize() == DataBufSize());
        }
        MASSERT(inLwr < bufSize);
        StreamClient::MemCopy(GetDataBuf() + m_DataBufLwr, static_cast<const UINT08*>(pDataBuf) + inLwr, bufSize - inLwr);
        m_DataBufLwr += bufSize - inLwr;

        return true;
    }

    bool MemStream::FlushWrites()
    {
        if (m_DataBufLwr > 0)
        {
            MASSERT(m_DataBufLwr == DataBufSize());
            GetMemRW()->Write(GetDataBuf(), UINT32(DataBufSize()));
            m_DataBufLwr = 0;
        }
        MASSERT(DataBufRemainSize() == DataBufSize());
        return true;
    }

    bool MemStream::CliConfig::BandwidthSanityCheck() const
    {
        // Usual check.
        bool ret = MemStream::BandwidthSanityCheck(bandwidth);
        MASSERT(ret);

        // Put client side specific other sanity check here.

        return ret;
    }

    void StreamClient::MemCopy(void *Dst, const void *Src, size_t Count)
    {
        MASSERT(MemStream::BandwidthSanityCheck(Count));
        memcpy(Dst, Src, Count);
    }

    ////////////////StreamLocal///////////////////

    void StreamLocal::GetStreamData(MemData* pOut) const
    {
        DebugPrintf("StreamLocal::%s, " vGpuMigrationTag " m_Buf.size 0x%llx.\n",
            __FUNCTION__,
            UINT64(m_Buf.size())
            );
        *pOut = m_Buf;
    }

    void StreamLocal::SetStreamData(const MemData& data)
    {
        DebugPrintf("StreamLocal::%s, " vGpuMigrationTag " in.size 0x%llx m_Buf.size 0x%llx.\n",
            __FUNCTION__,
            UINT64(data.size()),
            UINT64(m_Buf.size())
            );
        MASSERT(data.size() > 0);
        m_Buf = data;
    }

    void StreamLocal::OnResizeBandwidth()
    {
        GetConfig()->BandwidthSanityCheck();
        const size_t size = std::max(m_Buf.size(), GetConfig()->bandwidth);
        if (size != m_Buf.size())
        {
            m_Buf.resize(size);
        }
    }

    void StreamLocal::OnStartSeg(const MemSegDesc& seg)
    {
        const PHYSADDR addr = seg.GetAddr();
        const UINT64 size = seg.GetSize();
        DebugPrintf("StreamLocal::%s, " vGpuMigrationTag " addr 0x%llx size 0x%llx config.bandwidth 0x%llx.\n",
            __FUNCTION__,
            addr,
            size,
            UINT64(GetConfig()->bandwidth));

        MASSERT(size > 0);
        if (size != m_Buf.size())
        {
            m_Buf.resize(std::max(size_t(size), GetConfig()->bandwidth));
        }
        m_LwrLoc = 0;
        m_Addr = addr;
    }

    //////////////////////FbCopyStream//////////////////////////

    void FbCopyStream::Verif::AddData(const void* pData, size_t size)
    {
        if (!NeedVerif())
        {
            return;
        }
        if (m_VerifLwrLoc < m_VerifBufSize)
        {
            if (m_VerifLwrLoc + size <= m_VerifBufSize)
            {
            }
            else
            {
                size = m_VerifBufSize - m_VerifLwrLoc;
            }
            StreamClient::MemCopy(m_pVerifBuf.get() + m_VerifLwrLoc, pData, size);
            m_VerifLwrLoc += size;
            MASSERT(m_VerifLwrLoc <= m_VerifBufSize);
        }
    }

    void FbCopyStream::SetStreamClient(StreamClient* pCli)
    {
        MASSERT(pCli != nullptr);
        m_pCli.reset(pCli);
    }

    void FbCopyStream::SetMemAlloc(MemAlloc* pAlloc)
    {
        MASSERT(pAlloc != nullptr);
        m_pAlloc = pAlloc;
    }

    void FbCopyStream::SetMemStreamBandwidth(size_t bandwidth)
    {
        DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " To set stream bandwidth 0x%llx, old m_bandwidth 0x%llx.\n",
            __FUNCTION__,
            UINT64(bandwidth),
            UINT64(m_MemStreamBandwidth)
            );
        if (bandwidth >= MemStream::PageSize)
        {
            m_MemStreamBandwidth = MemStream::PageSize;
        }
        else
        {
            m_MemStreamBandwidth = bandwidth;
        }
        MASSERT(MemStream::BandwidthSanityCheck(m_MemStreamBandwidth));
        DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " New m_bandwidth 0x%llx.\n",
            __FUNCTION__,
            UINT64(m_MemStreamBandwidth)
            );
    }

    RC FbCopyStream::SaveMeta(const MemMeta& meta)
    {
        MASSERT(m_pCli != 0);
        return m_pCli->SaveMeta(meta);
    }

    RC FbCopyStream::LoadMeta(MemMeta* pMeta)
    {
        MASSERT(m_pCli != 0);
        return m_pCli->LoadMeta(pMeta);
    }

    RC FbCopyStream::SaveData(const MemDescs& descs)
    {
        MASSERT(m_pCli != 0);
        m_pCli->GetConfig()->BandwidthSanityCheck();
        MASSERT(m_pAlloc != 0);

        RC rc;
        MDiagvGpuMigration* pVm = GetMDiagvGpuMigration();
        MemDescs::DescList memList;
        descs.GetDescList(&memList);
        for (const auto& seg : memList)
        {
            const size_t bandwidth = m_pCli->GetConfig()->bandwidth;
            DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " Seg addr 0x%llx size 0x%llx, pipe bandwidth 0x%llx.\n",
                __FUNCTION__,
                seg.GetAddr(),
                seg.GetSize(),
                UINT64(bandwidth)
                );
            MASSERT(seg.GetSize() >= UINT64(bandwidth));
            MASSERT(seg.GetSize() >= UINT64(GetMemStreamBandwidth()));
            m_pCli->OnStartSeg(seg);
            FbMemSeg fbSeg(seg);
            
            m_pAlloc->SetFbMemSeg(&fbSeg);
            if (m_pAlloc->NeedKind())
            {
                m_pAlloc->SetKind(LW_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE);
            }
            CHECK_RC(m_pAlloc->Map());
            DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " Mapped FB mem.\n",
                __FUNCTION__
                );
            fbSeg = *m_pAlloc->GetFbMemSeg();
            fbSeg.Print();
            Verif verif(seg.GetSize(), pVm->FbCopyNeedVerif());

            DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " Start FB mem seg PA 0x%llx CPU VA %p size 0x%llx reading ...\n",
                __FUNCTION__,
                fbSeg.GetAddr(),
                fbSeg.pCpuPtr,
                fbSeg.GetSize()
                );
            GetMemStream()->Open(fbSeg, true, GetMemStreamBandwidth());
            MASSERT(0 == (fbSeg.GetSize() % UINT64(bandwidth)));
            MemData dataBuf(bandwidth);
            for (UINT64 paOff = 0; paOff < fbSeg.GetSize(); paOff += bandwidth)
            {
                const PHYSADDR lwrOff = fbSeg.GetAddr() + paOff;
                pVm->PrintFbCopyProgress(lwrOff);
                size_t bufLen = bandwidth;
                GetMemStream()->ReadMem(&dataBuf[0], &bufLen);
                MASSERT(bandwidth == bufLen);

                CHECK_RC(m_pCli->SaveData(lwrOff, &dataBuf[0]));
                verif.AddData(&dataBuf[0], bandwidth);
            }
            dataBuf.clear();
            DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " Finished FB mem seg PA 0x%llx CPU VA %p size 0x%llx read.\n",
                __FUNCTION__,
                fbSeg.GetAddr(),
                fbSeg.pCpuPtr,
                fbSeg.GetSize()
                );

            CHECK_RC(m_pAlloc->Unmap());
            m_pCli->Flush();
            VerifyData(seg.GetAddr(), verif.GetBufSize(), verif.GetBuf(), __FUNCTION__);
            FillPatterlwerif(seg.GetAddr(), seg.GetSize());
        }
        return rc;
    }

    RC FbCopyStream::LoadData(const MemDescs& descs)
    {
        MASSERT(m_pCli != 0);
        m_pCli->GetConfig()->BandwidthSanityCheck();
        MASSERT(m_pAlloc != 0);

        RC rc;
        MDiagvGpuMigration* pVm = GetMDiagvGpuMigration();
        MemDescs::DescList memList;
        descs.GetDescList(&memList);
        for (const auto& seg : memList)
        {
            const size_t bandwidth = m_pCli->GetConfig()->bandwidth;
            DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " Seg addr 0x%llx size 0x%llx, pipe bandwidth 0x%llx.\n",
                __FUNCTION__,
                seg.GetAddr(),
                seg.GetSize(),
                UINT64(bandwidth)
                );
            MASSERT(seg.GetSize() >= UINT64(bandwidth));
            MASSERT(seg.GetSize() >= UINT64(GetMemStreamBandwidth()));
            m_pCli->OnStartSeg(seg);
            FbMemSeg fbSeg(seg);

            m_pAlloc->SetFbMemSeg(&fbSeg);
            if (m_pAlloc->NeedKind())
            {
                m_pAlloc->SetKind(LW_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE_DISABLE_PLC);
            }
            FillPatterlwerif(seg.GetAddr(), size_t(seg.GetSize()));
            CHECK_RC(m_pAlloc->Map());
            DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " Mapped FB mem.\n",
                __FUNCTION__
                );
            fbSeg = *m_pAlloc->GetFbMemSeg();
            fbSeg.Print();
            Verif verif(seg.GetSize(), pVm->FbCopyNeedVerif());

            DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " Start FB mem seg PA 0x%llx CPU VA %p size 0x%llx writing ...\n",
                __FUNCTION__,
                fbSeg.GetAddr(),
                fbSeg.pCpuPtr,
                fbSeg.GetSize()
                );
            GetMemStream()->Open(fbSeg, false, GetMemStreamBandwidth());
            MemData dataBuf(bandwidth);
            for (UINT64 paOff = 0; paOff < fbSeg.GetSize(); paOff += bandwidth)
            {
                const PHYSADDR lwrOff = fbSeg.GetAddr() + paOff;
                pVm->PrintFbCopyProgress(lwrOff);
                UINT64 srcPaOff = 0;
                CHECK_RC(m_pCli->LoadData(&srcPaOff, &dataBuf[0]));
                const PHYSADDR lwrSrcOff = fbSeg.GetSrcAddr() + paOff;
                if (lwrSrcOff != srcPaOff)
                {
                    ErrPrintf("FbCopyStream::%s, " vGpuMigrationTag " Expected to load FB mem PA offset 0x%llx, but read 0x%llx.\n",
                        __FUNCTION__,
                        lwrSrcOff,
                        srcPaOff
                        );
                    return RC::SOFTWARE_ERROR;
                }
                GetMemStream()->WriteMem(&dataBuf[0], bandwidth);
                verif.AddData(&dataBuf[0], bandwidth);
            }
            dataBuf.clear();
            GetMemStream()->FlushWrites();
            DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " Finished FB mem seg PA 0x%llx CPU VA %p size 0x%llx write.\n",
                __FUNCTION__,
                fbSeg.GetAddr(),
                fbSeg.pCpuPtr,
                fbSeg.GetSize()
                );
            CHECK_RC(m_pAlloc->Unmap());
            VerifyData(seg.GetAddr(), verif.GetBufSize(), verif.GetBuf(), __FUNCTION__);
        }

        return rc;
    }

    void FbCopyStream::SetClientBandwidth(size_t bandwidth)
    {
        MASSERT(m_pCli != 0);
        MemStream::CliConfig& cfg = m_pCli->GetConfigRef();
        if (cfg.bandwidth > bandwidth)
        {
            cfg.bandwidth = bandwidth;
        }
        m_pCli->OnResizeBandwidth();
        m_pCli->GetConfig()->BandwidthSanityCheck();
    }

    bool FbCopyStream::VerifyData(UINT64 paOff, size_t size, const void* pBuf, const char* pPoint) const
    {
        MDiagvGpuMigration* pVm = GetMDiagvGpuMigration();
        if (pVm->FbCopyNeedVerif())
        {
            DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " (%s) Verifying mem access @ PA 0x%016llx size 0x%x.\n",
                __FUNCTION__,
                pPoint,
                paOff,
                UINT32(size));
            MemData verifBuf;
            MASSERT(MDiagUtils::PraminPhysRdRaw(paOff, Memory::Fb, UINT32(size), &verifBuf) == OK);
            DataVerif data(paOff, size);
            return data.Compare(pBuf, &verifBuf[0]);
        }
        return true;
    }

    RC FbCopyStream::FillPatterlwerif(UINT64 paOff, size_t size) const
    {
        RC rc;
        MDiagvGpuMigration* pVm = GetMDiagvGpuMigration();
        if (pVm->FbCopyNeedVerif())
        {
            if (size >= MemStream::PatterlwerifSize)
            {
                UINT08 savedMem[MemStream::PatterlwerifSize];
                UINT08 pattern[MemStream::PatterlwerifSize];
                UINT64* pPattern = static_cast<UINT64*>(static_cast<void*>(pattern));
                for (size_t i = 0; i < sizeof(pattern) / sizeof(*pPattern); i++)
                {
                    pPattern[i] = 0xdeadbeefdeadbeefULL;
                }
                MemSegDesc segDesc(paOff, MemStream::PatterlwerifSize);
                FbMemSeg fbSeg(segDesc);
                m_pAlloc->SetFbMemSeg(&fbSeg);
                CHECK_RC(m_pAlloc->Map());
                fbSeg = *m_pAlloc->GetFbMemSeg();
                MemStream::MemCopy(savedMem, fbSeg.pCpuPtr, sizeof(pattern));
                MemStream::MemCopy(fbSeg.pCpuPtr, pattern, sizeof(pattern));
                CHECK_RC(m_pAlloc->Unmap());
                VerifyData(paOff, MemStream::PatterlwerifSize, pattern, "FillPatterlwerif-pattern filled");
                m_pAlloc->SetFbMemSeg(&fbSeg);
                CHECK_RC(m_pAlloc->Map());
                fbSeg = *m_pAlloc->GetFbMemSeg();
                MemStream::MemCopy(fbSeg.pCpuPtr, savedMem, sizeof(pattern));
                CHECK_RC(m_pAlloc->Unmap());
                VerifyData(paOff, MemStream::PatterlwerifSize, savedMem, "FillPatterlwerif-data recovered");
            }
            else
            {
                DebugPrintf("FbCopyStream::%s, " vGpuMigrationTag " Ignore FillPattern verif belwase size 0x%x < PatterlwerifSize 0x%x.\n",
                    __FUNCTION__,
                    UINT32(size),
                    UINT32(MemStream::PatterlwerifSize));
            }
        }
        return rc;
    }

} // MDiagVmScope

