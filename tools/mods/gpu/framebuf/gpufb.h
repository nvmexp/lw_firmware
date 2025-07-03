/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/framebuf.h"

#ifdef MATS_STANDALONE
#include "gpu/reghal/reghal.h"
#else
#include "gpu/include/gpusbdev.h"
#endif

//! \brief Base class for all GPU FrameBuffer objects.
//!
//! Supplies the linkage to a GpuSubdevice.

class GpuFrameBuffer : public FrameBuffer
{
public:
    GpuFrameBuffer
    (
        const char   * name,
        GpuSubdevice * pGpuSubdevice,
        LwRm         * pLwRm
    );

    virtual RC BlacklistPage
    (
        UINT64 location,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        BlacklistSource source,
        UINT32 rbcAddress
    ) const;
    virtual RC BlacklistPage
    (
        UINT64 eccOnPage,
        UINT64 eccOffPage,
        BlacklistSource source,
        UINT32 rbcAddress
    ) const;
    virtual RC DumpFailingPage
    (
        UINT64 location,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        BlacklistSource source,
        UINT32 rbcAddress,
        INT32 originTestId
    ) const;
    virtual RC DumpMBISTFailingPage
    (
        UINT08 site,
        string rowName,
        UINT32 hwFbio,
        UINT32 fbpaSubp,
        UINT08 rank,
        UINT08 bank,
        UINT16 ra,
        UINT08 pseudoChannel
    ) const;
    virtual RC     GetFbRanges(FbRanges *pRanges) const;
    virtual UINT64 GetGraphicsRamAmount() const { return m_GraphicsRamAmount; }
    virtual bool   IsSOC()                const;
    virtual UINT32 GetBusWidth()          const;
    virtual UINT32 GetSubpartitions()     const { return 2; }
    virtual UINT32 GetChannelsPerFbio()   const { return m_ChannelsPerFbio; }

    virtual RamProtocol GetRamProtocol()  const { return m_RamProtocol; }
    virtual string GetRamProtocolString() const { return m_RamProtocolString; }

    virtual UINT32 GetPseudoChannelsPerChannel() const
                                { return m_PseudoChannelsPerChannel; }
    virtual UINT32 GetPseudoChannelsPerSubpartition() const
                                { return m_PseudoChannelsPerSubpartition; }
    virtual bool   IsEccOn()            const { return m_IsEccOn; }
    virtual UINT32 GetFbEccSectorSize() const { return 32; }

    virtual bool   IsRowRemappingOn()   const { return m_pRowRemapper->IsEnabled(); }

    virtual UINT32 GetRankCount()       const { return m_RankCount; }
    virtual UINT32 GetBankCount()       const { return m_BankCount; }
    virtual UINT32 GetRowBits()         const { return m_RowBits; }
    virtual UINT32 GetRowSize()         const { return m_RowSize; }
    virtual UINT32 GetBurstSize()       const;
    virtual UINT32 GetBeatSize()        const { return m_BeatSize; }
    virtual UINT32 GetDataRate() const;
    virtual RC     Initialize() { return InitCommon(); }

    virtual RamVendorId GetVendorId()    const { return m_VendorId; }
    virtual UINT32      GetConfigStrap() const { return m_ConfigStrap; }

    virtual unique_ptr<CheckbitsHolder> SaveFbEccCheckbits();
    virtual bool CanToggleFbEccCheckbits();
    virtual RC   EnableFbEccCheckbits();
    virtual RC   DisableFbEccCheckbits();
    virtual RC   DisableFbEccCheckbitsAtAddress(UINT64 physAddr, UINT32 pteKind,
                                                UINT32 pageSizeKB);
    virtual RC   ApplyFbEccWriteKillPtr(UINT64 physAddr, UINT32 pteKind,
                                UINT32 pageSizeKB, INT32 kptr0, INT32 kptr1);

    virtual RC RemapRow(const vector<RowRemapper::Request>& requests) const;
    virtual RC CheckMaxRemappedRows(const RowRemapper::MaxRemapsPolicy& policy) const;
    virtual RC CheckRemapError() const;
    virtual RC GetNumInfoRomRemappedRows(UINT32* pNumRows) const;
    virtual RC PrintInfoRomRemappedRows() const;
    virtual RC ClearRemappedRows
    (
        vector<RowRemapper::ClearSelection> clearSelections
    ) const;
    virtual UINT64 GetRowRemapReservedRamAmount() const;

protected:
    virtual RC InitCommon();
    RC CalibratedRead32(ModsGpuRegValue regValue, UINT32 desiredValue,
                        UINT32* pOutput) const;
    RC WriteEccRegisters(UINT32 ctrlValue, UINT32 ctrlMask,
                         UINT32 injectAddr, UINT32 injectAddrExt,
                         UINT32 injectHwFbio, UINT32 injectChannel);

private:
    RC BlacklistPageInternal
    (
        UINT64 eccOnPage,
        UINT64 eccOffPage,
        BlacklistSource source,
        UINT32 rbcAddress
    ) const;
    RamVendorId m_VendorId;
    UINT32      m_ConfigStrap;
    RamProtocol m_RamProtocol;
    string      m_RamProtocolString;

    UINT32      m_ChannelsPerFbio;
    UINT32      m_PseudoChannelsPerChannel;
    UINT32      m_PseudoChannelsPerSubpartition;
    bool        m_IsEccOn;
    UINT32      m_RankCount;
    UINT32      m_BankCount;
    UINT32      m_RowBits;
    UINT32      m_RowSize;
    UINT32      m_BeatSize;

    FbRanges    m_FbRanges;
    UINT64      m_GraphicsRamAmount;

    unique_ptr<RowRemapper> m_pRowRemapper;
};

FrameBuffer *CreateGF100FrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGF108FrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGF114FrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGK110FrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGK208FrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGK107FrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGKLIT1FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateTegraFrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateAmodFrameBuffer(  GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGM10XFrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGM20XFrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGP100FrameBuffer( GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGPLIT3FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGPLIT4FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGVLIT1FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGVLIT3FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGVLIT5FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateTULIT2FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGALIT1FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGALIT2FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
FrameBuffer *CreateGHLIT1FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
