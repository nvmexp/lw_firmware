/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2012,2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef BUFFINFO_H
#define BUFFINFO_H

#include "core/include/utility.h"
#include "surf_types.h"
#include <list>
#include <vector>
#include <string>
class BlockLinear;
class TextureHeader;

class BuffInfo
{
    enum COL_ID
    {
        COL_BUFF = 0,
        COL_HMEM,
        COL_HVA,
        COL_VA,
        COL_TEGRA_VA,
        COL_PA,
        COL_APER,
        COL_SIZE_B,
        COL_SIZE_P,
        COL_BL,
        COL_COMP,
        COL_GCACH,
        COL_KIND,
        COL_PRIV,
        COL_DESC,
        COL_PROT,
        COL_SPRT,
        COL_PAGE_SIZE,
        COL_CONTIG,
        COL_NUM, // number of columns in the table.
    };
    static const int m_EntryLength = 128;  // max entry length
    vector<UINT32>         m_MaxEntryLength;

    struct ChannelHandle
    {
        ChannelHandle(UINT32 handle, LwRm* pLwRm) :
                        m_Handle(handle), m_HandleLwRm(pLwRm) {};
        UINT32 m_Handle;
        LwRm* m_HandleLwRm;
    };
    typedef vector<ChannelHandle> ChannelHandles;
    ChannelHandles m_channelHandles;

    struct Entry
    {
        Entry(int lineLength) : m_Columns(lineLength), m_Printed(false),
            m_Header(false), m_BufferSize(0) {};

        vector<string> m_Columns;
        bool m_Printed;
        bool m_Header;
        UINT64 m_BufferSize;
    };
    typedef list<Entry>    Entries;
    Entries m_Entries;
    bool m_PrintBufferOnce;
    UINT64 m_LwrrentBufferMemory;

public:
         BuffInfo();
    void specifyChannelHandle(UINT32, LwRm* pLwRm);
    void AddEntry(const string& name);
    void SetHMem(UINT32 hMem);
    void SetCompr(UINT32 hMem, LwRm* pLwRm);
    void SetHVA(UINT32 hVa);
    void SetHVA(const string &name, UINT32 hVa);
    void SetVA(UINT64 va);
    void SetVA(const string &name, UINT64 va);
    void SetTegraVA(UINT64 va);
    void SetTegraVA(const string &name, UINT64 va);
    void SetSizeB(UINT64 va);
    void SetSizeBL(const MdiagSurf& surf);
    void SetSizeBL(const BlockLinear* bl);
    void SetSizeBL(const TextureHeader* bl);
    void SetSizeBL(const char*);
    void SetSizePix(const char*);
    void SetSizePix(const MdiagSurf& surf);
    void SetPTE(const char*);
    void SetPTE(const MdiagSurf& surf);
    void SetPriv(bool priv);
    void SetProtect(Memory::Protect prot);
    void SetPageSize(const MdiagSurf& surf);
    void SetContig(bool contig);
    void SetShaderAccess(const MdiagSurf& surf);
    void SetTarget(const MdiagSurf& surf, bool needPeerMapping, bool isLocal);
    void SetTarget(const string &name, const bool isLocal);
    void SetTarget(_DMA_TARGET target);
    void SetDmaBuff(const string& name, const MdiagSurf& buff,
                    UINT64 offset = 0, UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV,
                    bool needPeerMapping = false, UINT64 size_override = 0,
                    bool isLocal = true);
    void SetRenderSurface(const MdiagSurf& surf);
    void Print(const char *subtitle = 0, GpuDevice* pGpuDev = nullptr);
    void SetPrintBufferOnce(bool printOnce) { m_PrintBufferOnce = printOnce; }
    void SetDescription(const string &desc);
    void FreeBufferMemory(UINT64 bufferSize);
private:
    void outputChannelInformation(UINT32, LwRm* pLwRm, GpuDevice* pGpuDev = nullptr) const;
    void getMemOffsetAndCache0041(const MdiagSurf& surf, UINT64 offset, string &str);
    void getMemOffsetAndCache003E(const MdiagSurf& surf, UINT64 offset, string &str);
    void translateAccessFlag(Memory::Protect prot, int col);
};

#endif /* BUFFINFO_H */
