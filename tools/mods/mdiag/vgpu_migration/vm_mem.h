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
#ifndef VM_MEM_H
#define VM_MEM_H

// Need to use std vector, unique_ptr, etc.
#include <memory>
#include <string>
#include <vector>

// RC
#include "core/include/rc.h"
#include "mdiag/utils/types.h"

namespace MDiagVmScope
{
    // A type to represent mem data buffer.
    typedef vector<UINT08> MemData;

    // The meta struct for stored in a hex dump file, etc,
    // when FB mem save/restore.
    typedef pair<PHYSADDR, UINT64> MemSegMeta;
    typedef vector<MemSegMeta> MemMeta;

    // A memory segment descriptor supporting source & dest addr
    // with equal segment size, used for FB copy.
    class MemSegDesc
    {
    public:
        MemSegDesc() = default;
        MemSegDesc(PHYSADDR addr, UINT64 size)
            : m_Addr(addr),
            m_SrcAddr(addr),
            m_Size(size)
        {}

        MemSegDesc(PHYSADDR dstAddr, PHYSADDR srcAddr, UINT64 size)
            : m_Addr(dstAddr),
            m_SrcAddr(srcAddr),
            m_Size(size)
        {}

        MemSegDesc(const MemSegMeta& meta)
            : MemSegDesc(meta.first, meta.second)
        {}
            
        PHYSADDR GetAddr() const { return m_Addr; }
        UINT64 GetSize() const { return m_Size; }
        const MemSegMeta GetMeta() const { return std::make_pair(GetAddr(), GetSize()); }

        void Init(PHYSADDR addr, UINT64 size)
        {
            SetAddr(addr);
            SetSrcAddr(addr);
            SetSize(size);
        }
        void Init(PHYSADDR dstAddr, PHYSADDR srcAddr, UINT64 size)
        {
            SetAddr(dstAddr);
            SetSrcAddr(srcAddr);
            SetSize(size);
        }

        void SetAddr(PHYSADDR addr) { m_Addr = addr; }
        void SetSize(UINT64 size) { m_Size = size; }
        // Get next offset.
        PHYSADDR NextOffset() const { return GetAddr() + GetSize(); }
        // If the input segment is next one?
        bool IsNext(const MemSegDesc& segIn) const { return NextOffset() == segIn.GetAddr(); }
        // Append a piece to the segment.
        void Append(UINT64 size) { m_Size += size; }
        // Get source addr.
        PHYSADDR GetSrcAddr() const { return m_SrcAddr; }
        void SetSrcAddr(PHYSADDR srcAddr) { m_SrcAddr = srcAddr; }
        // Cut off a front piece from the segment.
        void PopFront(UINT64 size);

        // Get next source offset.
        PHYSADDR NextSrcOffset() const
        {
            return GetSrcAddr() + GetSize();
        }
        // If the input segment is next one to source segment.
        bool IsNextSrc(const MemSegDesc& segIn) const
        {
            return NextSrcOffset() == segIn.GetSrcAddr();
        }

        // Debug prints purpose only.
        void Print() const;

    protected:
        PHYSADDR m_Addr = 0;
        PHYSADDR m_SrcAddr = 0;
        UINT64 m_Size = 0;
    };

    // Mem descriptors holding multiple mem segment descriptors.
    // FB memory or local memory for FB copy.
    class MemDescs
    {
    public:
        typedef vector<MemSegDesc>   DescList;

    public:
        MemDescs() = default;
        MemDescs(const MemSegDesc& desc)
        {
            m_DescList.emplace_back(desc);
        }
        MemDescs(const MemDescs& descs)
            : m_DescList(descs.m_DescList)
        {}

        size_t GetCount() const { return m_DescList.size(); }
        // Add a mem seg descriptor.
        void Add(PHYSADDR addr, UINT64 size) { m_DescList.emplace_back(addr, size); }
        void Add(const MemSegDesc& desc) { m_DescList.emplace_back(desc); }
        const MemSegDesc* GetMemSegDesc(UINT32 i) const { return &m_DescList[i]; }
        void GetDescList(DescList* pList) const { *pList  = m_DescList; }
        // Join two MemDescriptors.
        void Join(const MemDescs& in);
        
        // Split segment if any segment size in list > input segSize,
        // to make max seg size <= the required segSize.
        // Useful when BAR1 mapping, because BAR1 available size
        // will be limited.
        void Split(UINT64 segSize);

        UINT64 GetTotalSize() const;
        // Get the min seg size in the list.
        UINT64 GetMinSegSize() const;
        // The start offset of the MemDescriptors, or the
        // lowest addr boundary of all segments.
        PHYSADDR GetStartOffset() const;
        // The end offset of the MemDescriptors, or the
        // hightest addr boundary of all segments.
        PHYSADDR GetEndOffset() const;
        // Span-size == EndOffset - StartOffset.
        // The whole addr space size which MemDescriptors covered.
        UINT64 GetSpanSize() const
        {
            return GetEndOffset() - GetStartOffset();
        }
        // Verify source segment size.
        // Before add source mem descriptors into MemDescriptors,
        // it is required that source seg size would be equal to dest
        // seg size.
        bool VerifySrcSize(const MemDescs& srcDescs);
        void AddSrcDescs(const MemDescs& srcDescs);
        void Clear() { m_DescList.clear(); }
        void GetMeta(MemMeta* pMeta) const;
        void ImportFromMeta(const MemMeta& meta);
        
        // For debug prints purpose.
        void PrintList() const;

    private:
        DescList m_DescList;
    };

    // A mem segment descriptor having mapped CPU VA.
    struct FbMemSeg : public MemSegDesc
    {
        FbMemSeg() = default;
        FbMemSeg(const MemSegDesc& seg)
            : MemSegDesc(seg),
            m_AllocDesc(seg)
        {}

        const MemSegDesc& GetMemSegDesc() const
        {
            return *dynamic_cast<const MemSegDesc*>(this);
        }

        // Debug prints purpose only.
        void Print() const;

        // It's allowed that actually allocated/mapped PA offset
        // and size could be differnt from requested values.
        MemSegDesc m_AllocDesc;

        // CPU VA.
        union {
            void* pCpuPtr = nullptr;
            UINT32* p4BytePtr;
            UINT08* pBytePtr;
        };
    };

    // The abstract mem mapper/allocator type.
    // A derived concrete type could be BAR1 mapper or
    // surface allocator.
    class MemAlloc
    {
    public:
        virtual ~MemAlloc() {}
        virtual RC Map() = 0;
        virtual RC Unmap() = 0;

    public:
        void SetFbMemSeg(FbMemSeg* pFbSeg);
        FbMemSeg* GetFbMemSeg() { return &m_FbSeg; }
        const FbMemSeg* GetFbMemSegConst() const { return &m_FbSeg; }

        UINT32 GetAlignment() const { return m_Align; }
        void SetAlignment(UINT32 align) { m_Align = align; }

        UINT32 GetHandle() const { return m_Handle; }
        void SetHandle(UINT32 handle) { m_Handle = handle; }

        bool NeedKind() const { return m_bNeedKind; }
        void SetNeedKind(bool bNeed) { m_bNeedKind = bNeed; }

        UINT32 GetKind() const { return m_Kind; }
        void SetKind(UINT32 kind) { m_Kind = kind; }

    protected:
        FbMemSeg m_FbSeg;

    private:
        // 4K aligment by default.
        UINT32 m_Align = 0x4U << 10;
        UINT32 m_Handle = 0;
        UINT32 m_Kind = 0;
        bool m_bNeedKind = true;
    };

} // MDiagVmScope

#endif

