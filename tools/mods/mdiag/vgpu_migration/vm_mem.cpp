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

#include "vm_mem.h"
#include "core/include/massert.h"
#include "mdiag/sysspec.h"
#include "vgpu_migration.h"

namespace MDiagVmScope
{
    //////////////////MemSegDesc//////////////////

    void MemSegDesc::PopFront(UINT64 size)
    {
        MASSERT(size < GetSize());

        m_Addr += size;
        m_SrcAddr += size;
        m_Size -= size;
    }

    void MemSegDesc::Print() const
    {
        DebugPrintf(vGpuMigrationTag ", MemSegDesc:\n"
            "   Addr 0x%llx(%lluM)\n"
            "   SrcAddr 0x%llx(%lluM)\n"
            "   Size 0x%llx(%lluM)\n"
            "   NextOff 0x%llx\n"
            "   NextSrcOff 0x%llx\n",
            GetAddr(),
            GetAddr() >> 20,
            GetSrcAddr(),
            GetSrcAddr() >> 20,
            GetSize(),
            GetSize() >> 20,
            NextOffset(),
            NextSrcOffset()
            );
    }

    //////////////////FbMemSeg//////////////////

    void FbMemSeg::Print() const
    {
        DebugPrintf(vGpuMigrationTag ", FbMemSeg:\n"
            "   CpuAddr %p\n"
            "   AllocAddr 0x%llx\n"
            "   AllocSize 0x%llx\n"
            ,
            pCpuPtr,
            m_AllocDesc.GetAddr(),
            m_AllocDesc.GetSize()
            );
        MemSegDesc::Print();
    }

    //////////////////MemDescs//////////////////

    void MemDescs::PrintList() const
    {
        UINT32 id = 0;
        DebugPrintf(vGpuMigrationTag ", Num of MemSegDesc 0x%x:\n",
            UINT32(GetCount()));
        for (const auto& seg : m_DescList)
        {
            DebugPrintf(vGpuMigrationTag ", memDesc 0x%x:\n",
                id++);
            seg.Print();
        }
    }

    void MemDescs::Join(const MemDescs& in)
    {
        if (in.m_DescList.empty())
        {
            return;
        }
        for (const auto& seg : in.m_DescList)
        {
            auto& lastSeg = m_DescList.back();
            if (m_DescList.size() > 0 && lastSeg.IsNext(seg))
            {
                MASSERT(lastSeg.IsNextSrc(seg));
                lastSeg.Append(seg.GetSize());
            }
            else
            {
                m_DescList.emplace_back(seg);
            }
        }
    }

    void MemDescs::Split(UINT64 segSize)
    {
        if (m_DescList.empty())
        {
            return;
        }
        DescList mList;
        for (auto& seg : m_DescList)
        {
            while (seg.GetSize() > segSize)
            {
                mList.emplace_back(seg.GetAddr(), seg.GetSrcAddr(), segSize);
                seg.PopFront(segSize);
            } 
            // Now the segment size <= segSize
            MASSERT(seg.GetSize() >0 && seg.GetSize() <= segSize);
            mList.emplace_back(seg);
        }
        m_DescList.clear();
        m_DescList = mList;
    }

    UINT64 MemDescs::GetTotalSize() const
    {
        UINT64 size = 0;

        for (const auto& seg : m_DescList)
        {
            size += seg.GetSize();
        }

        return size;
    }

    UINT64 MemDescs::GetMinSegSize() const
    {
        UINT64 minSize = 0;

        if (!m_DescList.empty())
        {
            // Usually if split happened, the last seg size will be the min size one;
            // otherwize the segs should have the same size value.
            // So just take the last seg size.
            minSize = m_DescList.back().GetSize();
        }

        DebugPrintf("MemDescs::%s, " vGpuMigrationTag ", num descs 0x%llx returned minSegSize 0x%llx.\n",
            __FUNCTION__,
            UINT64(m_DescList.size()),
            minSize
            );
        return minSize;
    }

    PHYSADDR MemDescs::GetStartOffset() const
    {
        if (!m_DescList.empty())
        {
            const auto& seg = m_DescList.front();
            return seg.GetAddr();
        }
        return 0;
    }

    PHYSADDR MemDescs::GetEndOffset() const
    {
        if (!m_DescList.empty())
        {
            const auto& seg = m_DescList.back();
            return seg.NextOffset();
        }
        return 0;
    }

    bool MemDescs::VerifySrcSize(const MemDescs& srcMem)
    {
        if (GetCount() != srcMem.GetCount())
        {
            if (srcMem.GetCount() == 0)
            {
                // ignore empty source list.
                return true;
            }
            return false;
        }
        auto it = m_DescList.begin();
        auto itSrc = srcMem.m_DescList.begin();
        for (;
            it != m_DescList.end() && itSrc != srcMem.m_DescList.end();
            it++, itSrc++)
        {
            const auto& seg = *it;
            const auto& srcSeg = *itSrc;
            if (srcSeg.GetSize() > seg.GetSize())
            {
                ErrPrintf("%s, " vGpuMigrationTag " Current FB segment size 0x%llx < segment size 0x%llx from source which can't be supported.\n",
                    __FUNCTION__,
                    seg.GetSize(),
                    srcSeg.GetSize()
                    );
                return false;
            }
        }

        return true;
    }

    void MemDescs::AddSrcDescs(const MemDescs& srcMem)
    {
        auto it = m_DescList.begin();
        auto itSrc = srcMem.m_DescList.begin();
        for (;
            it != m_DescList.end() && itSrc != srcMem.m_DescList.end();
            it ++, itSrc ++)
        {
            auto& seg = *it;
            const auto& srcSeg = *itSrc;
            MASSERT(seg.GetSize() >= srcSeg.GetSize());
            seg.SetSize(srcSeg.GetSize()); // set size to min value.
            seg.SetSrcAddr(srcSeg.GetAddr());
        }
    }

    void MemDescs::GetMeta(MemMeta* pMeta) const
    {
        MASSERT(pMeta != nullptr);
        for (const auto& desc: m_DescList)
        {
            pMeta->emplace_back(desc.GetMeta());
        }
    }

    void MemDescs::ImportFromMeta(const MemMeta& meta)
    {
        if (meta.empty())
        {
            return;
        }
        for (const auto& seg : meta)
        {
            m_DescList.emplace_back(seg.first, seg.second);
        }
    }

    /////////////////MemAlloc/////////////////////

    void MemAlloc::SetFbMemSeg(FbMemSeg* pFbSeg)
    {
        MASSERT(pFbSeg != nullptr);
        m_FbSeg = *pFbSeg;
    }

} // MDiagVmScope

