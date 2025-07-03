/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Frame buffer interface.
//
// For an overview of the concepts used in this class, see
// https://confluence.lwpu.com/x/rmb3BQ

#pragma once

#ifdef MATS_STANDALONE
#include "fakemods.h"
#else
#include "core/include/utility.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/repair/row_remapper/row_remapper.h"
#endif

#include "ctrl/ctrl2080.h"
#include "core/include/memtypes.h"
#include "core/utility/strong_typedef_helper.h"
#include <memory>

class GpuSubdevice;
class LwRm;

struct FbRange
{
    UINT64 start;
    UINT64 size;
};

typedef vector<FbRange> FbRanges;

    struct GpuLitter: Identifier
    {
        using Identifier::Identifier;
    };

struct EccAddrParams
{
    UINT32 eccAddr;     //ECC_ADDR;
    UINT32 eccAddrExt;  //ECC_ADDR_EXT;
    UINT32 eccAddrExt2; //ECC_ADDR_EXT2;
    UINT32 extBank;
    UINT32 subPartition;
    UINT32 partition;
    UINT32 channel;
};

struct EccErrInjectParams
{
    UINT32 eccCtrlWrtKllPtr0;   // ECC_CONTROL_WRITE_KILL_PTR0
    UINT32 eccErrInjectAddr;    // ECC_ERR_INJECTION_ADDR
    UINT32 eccErrInjectAddrExt; // ECC_ERR_INJECTION_ADDR_EXT
    UINT32 subPartition;
    UINT32 partition;           // logical
    UINT32 channel;
};

struct FbDecode
{
    // The following fields form the RBC address
    //
    UINT32 virtualFbio;   // FBIO number, packed for floorsweeping
    UINT32 subpartition;  // Controls part (usually half) of the FBIO bus
    UINT32 pseudoChannel; // Semi-independent part of subpartition
    UINT32 rank;          // DRAMs with separate chip selects
    UINT32 bank;          // Banks within a single physical RAM
    UINT32 row;           // A bank is a grid of capacitors, with rows & columns
    UINT32 burst;         // A series of reads or writes to memory
    UINT32 beat;          // One read or write in a burst
    UINT32 beatOffset;    // Byte offset into a beat

    // The following fields are auxiliary data, not part of RBC
    //
    UINT32 hbmSite;     // The HBM site, consisting of a stack of DRAMs
    UINT32 hbmChannel;  // A channel bus going into an HBM site
    UINT32 channel;     // A channel bus coming from the FBIO
    UINT32 extColumn;   // Same as "burst", but possibly swizzled or bit-shifted
    UINT32 slice;       // L2 Slice
    UINT64 xbarRawAddr; // Crossbar address
};

struct LtcIds
{
    UINT32 globalLtcId; // LTC Id across all FBPs
    UINT32 localLtcId;  // LTC Id within a FBP
};

class FrameBuffer
{
protected:
    using BlacklistSource = Memory::BlacklistSource;
    using MemErrT = Memory::ErrorType;

public:
    // The RM tells us the address of each ECC error, but we couldn't get
    // them to return the kind/pageSize.  We were told to use these constants.
    //
    static constexpr UINT32 GENERIC_DECODE_PTE_KIND = 0;
    static constexpr UINT32 GENERIC_DECODE_PAGE_SIZE = 4;

    explicit FrameBuffer
    (
        const char   * name,
        GpuSubdevice * pGpuSubdevice,
        LwRm         * pLwRm
    )
    : m_Name(name)
    , m_pGpuSubdevice(pGpuSubdevice)
    , m_pLwRm(pLwRm)
    {
    }

    virtual ~FrameBuffer() { }

    virtual RC     GetFbRanges(FbRanges *pRanges) const = 0;
    virtual UINT64 GetGraphicsRamAmount() const = 0;
    virtual bool   IsSOC() const = 0;
    virtual UINT32 GetSubpartitions() const = 0;  // Num subpartitions per FBIO
    UINT32         GetSubpartitionBusWidth() const; // Subpartition width in bits
    virtual UINT32 GetChannelsPerFbio() const { return GetSubpartitions(); }
    virtual UINT32 GetBusWidth()     const = 0;     // bus width in bits

    virtual UINT32 GetFbpMask()      const { return m_FbpMask; }     // FBP floorsweeping mask
    virtual UINT32 GetFbpCount()     const;                          // Num FBPs, after floorsweep
    virtual UINT32 GetMaxFbpCount()  const { return m_MaxFbpCount; } // Num FBPs, before floorsweep
    virtual UINT32 VirtualFbpToHwFbp(UINT32 virtualFbp) const;
    virtual UINT32 HwFbpToVirtualFbp(UINT32 hwFbp)      const;

    virtual UINT32 GetFbiosPerFbp()  const { return m_FbiosPerFbp; }
    virtual UINT32 GetFbioMask()     const { return m_FbioMask; }     // FBIO floorsweeping mask
    virtual UINT32 GetFbioCount()    const;                           // Num FBIOs, after floorsweep
    virtual UINT32 GetMaxFbioCount() const { return m_MaxFbioCount; } // Num FBIOs, before floorsweep
    virtual UINT32 GetHalfFbpaMask()   const { return 0; }
    virtual UINT32 GetHalfFbpaStride() const { return 0; }
    virtual UINT32 VirtualFbioToHwFbio(UINT32 virtualFbio) const;
    virtual UINT32 HwFbioToVirtualFbio(UINT32 hwFbio)      const;
    virtual UINT32 HwFbioToHwFbpa(UINT32 fbio)           const { return fbio; }
    virtual UINT32 HwFbpaToHwFbio(UINT32 fbpa)           const { return fbpa; }
    virtual UINT32 VirtualFbioToVirtualFbpa(UINT32 fbio) const { return fbio; }
    virtual UINT32 VirtualFbpaToVirtualFbio(UINT32 fbpa) const { return fbpa; }
    char           VirtualFbioToLetter(UINT32 virtualFbio) const;
    virtual RC     FbioSubpToHbmSiteChannel
    (
        UINT32  hwFbio,
        UINT32  subpartition,
        UINT32 *pHbmSite,
        UINT32 *pHbmChannel
    ) const;
    //This is for HBM3, it needs pseudoChannel as an additional input parameter
    // This virtual function is implemented in amapv1fb.cpp
    virtual RC     FbioSubpToHbmSiteChannel
    (
        UINT32  hwFbio,
        UINT32  subpartition,
        UINT32  pseudoChannel,
        UINT32 *pHbmSite,
        UINT32 *pHbmChannel
    ) const;
    virtual RC     HbmSiteChannelToFbioSubp
    (
        UINT32  hbmSite,
        UINT32  hbmChannel,
        UINT32 *pHwFbio,
        UINT32 *pSubpartition
    ) const;
    RC             HwFbpToHbmSite(UINT32 hwFbp, UINT32* pHbmSite) const;
    UINT32         GetFbpsPerHbmSite() const;

    //!
    //! \brief Is the site available for use (ie. not floorswept).
    //!
    //! \param hbmSite HBM site.
    //! \param[out] pIsAvailable True if the site is available, false otherwise.
    //!
    RC             IsHbmSiteAvailable(UINT32 hbmSite, bool* pIsAvailable) const;

    virtual UINT32 GetMaxLtcsPerFbp() const { return m_MaxLtcsPerFbp; }
    virtual UINT32 GetLtcMask()       const { return m_LtcMask; }     // Mask of all LTCs on all FBPs
    virtual UINT32 GetLtcCount()      const;                          // Num LTCs, after floorsweep
    virtual UINT32 GetMaxLtcCount()   const { return m_MaxLtcCount; } // Num LTCs, before floorsweep
    virtual UINT32 VirtualLtcToHwLtc(UINT32 virtualLtc) const;
    virtual UINT32 HwLtcToVirtualLtc(UINT32 hwLtc)      const;
    virtual UINT32 HwLtcToHwFbio(UINT32 hwLtc)          const;
    virtual UINT32 HwFbioToHwLtc(UINT32 hwFbio, UINT32 subpartition) const;
    UINT32         VirtualFbioToVirtualLtc(UINT32 virtualFbio,
                                           UINT32 subpartition) const;
    virtual UINT32 VirtualL2SliceToHwL2Slice(UINT32 virtualLtc, UINT32 virtualL2Slice) const;

    virtual UINT32 GetMaxSlicesPerLtc() const { return m_MaxSlicesPerLtc; } // Pre-Hopper unless need max slices
    virtual UINT32 GetSlicesPerLtc(UINT32 ltcIndex) const; // Must be used Hopper+
    virtual UINT32 GetL2SliceSize()  const { return m_L2SliceSize;  }
    virtual UINT32 GetL2SliceCount() const;
    virtual RC GlobalToLocalLtcSliceId(UINT32 globalSliceIndex, UINT32 *ltcId, UINT32 *localSliceId) const;
    virtual UINT32 GetFirstGlobalSliceIdForLtc(UINT32 ltcIndex) const;
    // Colwenience functions
    UINT32 GetMaxL2SlicesPerFbp() const
        { return GetMaxLtcsPerFbp() * GetMaxSlicesPerLtc(); }
    UINT32 GetL2SlicesPerFbp(UINT32 virtualFbp) const;
    UINT32 GetL2CacheSize() const
        { return GetL2SliceCount() * GetL2SliceSize();  }
    UINT32 GetL2FSSliceCount() const
        { return m_L2FSSlices; }
    //! Return offset of a byte in a row, ranging from 0 to GetRowSize()-1.
    UINT32 GetRowOffset(UINT32 burst, UINT32 beat, UINT32 beatOffset) const
        { return burst * GetBurstSize() + beat * GetBeatSize() + beatOffset; }

    virtual UINT32 GetPseudoChannelsPerChannel() const = 0;
    virtual UINT32 GetPseudoChannelsPerSubpartition() const { return GetPseudoChannelsPerChannel(); }
    virtual bool   IsEccOn()                     const = 0;

    //!
    //! \brief Return true if row remapping is on, otherwise false.
    //!
    //! If row remapping is not supported, it returns false.
    //!
    virtual bool IsRowRemappingOn() const = 0;

    virtual UINT32 GetRankCount()    const = 0; // number of ranks (ext banks)
    virtual UINT32 GetBankCount()    const = 0; // number of banks
    virtual UINT32 GetRowBits()      const = 0; // num bits in row index
    virtual UINT32 GetRowSize()      const = 0; // bytes in row, per pseudoch
    virtual UINT32 GetBurstSize()    const = 0; // bytes per burst
    virtual UINT32 GetBeatSize()     const { return 4; }
    virtual UINT32 GetAmapColumnSize() const { return 4; }
    virtual UINT32 GetChannelsPerSite() const 
    {
        switch (GetRamProtocol())
        {
            case RamHBM1:
            case RamHBM2:
                return 8;
            case RamHBM3:
                return 16;
            default:
                return 0;
        }
    }

    virtual UINT32 GetNumReversedBursts() const
        { return 0; } // number of consective bursts going out in reverse order
    virtual UINT32 GetDataRate() const = 0;
    virtual RC Initialize() = 0;
    virtual bool IsL2SliceValid(UINT32 hwSlice, UINT32 virtualFbp);

    virtual RC BlacklistPage
    (
        UINT64 location,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        BlacklistSource source,
        UINT32 rbcAddress
    ) const
    {
        Printf(Tee::PriWarn, "BlacklistPage function not implemented\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    virtual RC BlacklistPage
    (
        UINT64 eccOnPage,
        UINT64 eccOffPage,
        BlacklistSource source,
        UINT32 rbcAddress
    ) const
    {
        Printf(Tee::PriWarn, "BlacklistPage function not implemented\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    //!
    //! \brief Record the failing page in JS.
    //!
    //! \param location Physical address.
    //! \param pteKind Page table entry type.
    //! \param pageSizeKB Size of a page of memory.
    //! \param source Source of the blacklisting.
    //! \param rbcAddress Row-bank-column address of the failure.
    //! \param originTestId Test that was running when the error oclwred.
    //!
    virtual RC DumpFailingPage
    (
        UINT64 location,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        BlacklistSource source,
        UINT32 rbcAddress,
        INT32 originTestId
    ) const
    {
        Printf(Tee::PriWarn, "DumpFailingPage function not implemented\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

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
    ) const
    {
        Printf(Tee::PriWarn, "DumpMBISTFailingPage function not implemented\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    string GetDecodeString(const FbDecode &decode) const;
    virtual UINT32 GetFbioBitOffset(UINT32 subpartition, UINT32 pseudoChannel,
                                    UINT32 beatOffset, UINT32 bitOffset) const;

    virtual RC EncodeAddress(const FbDecode &decode, UINT32 pteKind,
                             UINT32 pageSizeKB, UINT64 *pEccOnAddr,
                             UINT64 *pEccOffAddr) const
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC DecodeAddress(FbDecode *pDecode, UINT64 fbOffset,
                             UINT32 pteKind, UINT32 pageSizeKB) const = 0;
    virtual void GetRBCAddress
    (
        EccAddrParams *pRbcAddr,
        UINT64 fbOffset,
        UINT32 pteKind,
        UINT32 pageSize,
        UINT32 errCount,
        UINT32 errPos
    ) = 0;
    virtual RC EccAddrToPhysAddr
    (
        const EccAddrParams & eccAddr,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        PHYSADDR *pPhysAddr
    ) { return RC::UNSUPPORTED_FUNCTION; }

    virtual void GetEccInjectRegVal
    (
       EccErrInjectParams *pInjectParams,
       UINT64 fbOffset,
       UINT32 pteKind,
       UINT32 pageSize,
       UINT32 errCount,
       UINT32 errPos
    ) = 0;

    virtual void EnableDecodeDetails(bool enable) {}

    const char*    GetName()    const { return m_Name; }
    virtual string GetGpuName(bool multiGpuOnly) const;

    enum RamProtocol
    {
        RamUnknown
        ,RamSdram
        ,RamDDR1
        ,RamDDR2
        ,RamDDR3
        ,RamSDDR4
        ,RamGDDR2
        ,RamGDDR3
        ,RamGDDR4
        ,RamGDDR5
        ,RamGDDR5X
        ,RamGDDR6
        ,RamGDDR6X
        ,RamLPDDR2
        ,RamLPDDR3
        ,RamLPDDR4
        ,RamLPDDR5
        ,RamHBM1
        ,RamHBM2
        ,RamHBM3
    };
    virtual bool        IsClamshell()          const;
    virtual bool        IsHalfFbpaEn(UINT32 virtualFbio) const;
    virtual RamProtocol GetRamProtocol()       const { return RamUnknown; }
    virtual string      GetRamProtocolString() const;
    bool                IsHbm()                const;
    bool                IsHbm3()                const;
    static bool         IsHbm(GpuSubdevice* pGpuSubdevice);

    enum RamVendorId
    {
        RamSAMSUNG = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_SAMSUNG,
        RamQIMONDA = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_QIMONDA,
        RamELPIDA = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_ELPIDA,
        RamETRON = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_ETRON,
        RamNANYA = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_NANYA,
        RamHYNIX = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_HYNIX,
        RamMOSEL = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_MOSEL,
        RamWINBOND = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_WINBOND,
        RamESMT = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_ESMT,
        RamMICRON = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_MICRON,
        RamVendorUnknown = LW2080_CTRL_FB_INFO_MEMORYINFO_VENDOR_ID_UNKNOWN,
    };

    virtual RamVendorId GetVendorId() const { return RamVendorUnknown; }

    virtual string GetVendorName() const
    {
       // Based on enum RamVendorId
       switch (GetVendorId())
       {
           case RamSAMSUNG: return "Samsung";
           case RamQIMONDA: return "Qimonda";
           case RamELPIDA:  return "Elpida";
           case RamETRON:   return "Etron";
           case RamNANYA:   return "Nanya";
           case RamHYNIX:   return "Hynix";
           case RamMOSEL:   return "Mosel";
           case RamWINBOND: return "Winbond";
           case RamESMT:    return "ESMT";
           case RamMICRON:  return "Micron";
           default:         return "Unknown vendor";
       }
    }

    virtual UINT32 GetConfigStrap() const { return 0; }
    virtual bool IsFbBroken() const { return false; }

    virtual UINT32 GetFbEccSectorSize() const { return  0; } //!< Bytes in a FB ECC sector
    virtual UINT32 GetL2EccSectorSize() const { return 32; } //!< Bytes in a L2 ECC sector
    virtual GpuLitter GetAmapLitter()   const { return GpuLitter(~0); }

    // Abstract RAII class to save/restore ECC checkbits registers
    class CheckbitsHolder
    {
    public:
        virtual ~CheckbitsHolder() {}
    };
    virtual unique_ptr<CheckbitsHolder> SaveFbEccCheckbits() { return nullptr; }

    virtual bool CanToggleFbEccCheckbits() { return false; }
    virtual RC EnableFbEccCheckbits() { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC DisableFbEccCheckbits() { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC DisableFbEccCheckbitsAtAddress(UINT64 physAddr, UINT32 pteKind,
                    UINT32 pageSizeKB) { return RC::UNSUPPORTED_FUNCTION; }
    enum { NO_KPTR = -1 };
    virtual RC ApplyFbEccWriteKillPtr(UINT64 physAddr, UINT32 pteKind,
                    UINT32 pageSizeKB, INT32 kptr0,
                    INT32 kptr1 = NO_KPTR) { return RC::UNSUPPORTED_FUNCTION; }

    //!
    //! \brief Remap the row corresponding to the given physical address.
    //!
    //! \param request Location to remap.
    //!
    RC RemapRow(const RowRemapper::Request& request) const
        { return RemapRow(vector<RowRemapper::Request>({request})); }


    //!
    //! \brief  Remap the rows corresponding to the given physical addresses.
    //!
    //! \param requests Locations to remap.
    //!
    virtual RC RemapRow(const vector<RowRemapper::Request>& requests) const
        { return RC::UNSUPPORTED_FUNCTION; }

    //!
    //! \brief Check if the policy for the maximum number of remapped rows has
    //! been violated.
    //!
    virtual RC CheckMaxRemappedRows(const RowRemapper::MaxRemapsPolicy& policy) const
        { return RC::UNSUPPORTED_FUNCTION; }

    //!
    //! \brief Check if a remap error oclwrred
    //!
    virtual RC CheckRemapError() const
        { return RC::UNSUPPORTED_FUNCTION; }

    //! \brief Get the number of remapped rows in the InfoROM.
    //!
    //! \param[out] pNumRows Number of remapped rows.
    //!
    virtual RC GetNumInfoRomRemappedRows(UINT32* pNumRows) const
        { return RC::UNSUPPORTED_FUNCTION; }

    //!
    //! \brief Print the details of each remapped row in the InfoROM.
    //!
    virtual RC PrintInfoRomRemappedRows() const { return RC::UNSUPPORTED_FUNCTION; }

    //!
    //! \brief Clear remapped rows based on the given criteria.
    //!
    //! \param clearSelections Descriptions of the types of remaps to be
    //! removed. A remapped row that meets any individual clear selection will
    //! be cleared.
    //!
    virtual RC ClearRemappedRows
    (
        vector<RowRemapper::ClearSelection> clearSelections
    ) const
        { return RC::UNSUPPORTED_FUNCTION; }

    //!
    //! \brief The number of bytes reserved as part of the reserved rows when
    //! the row remapper feature is enabled.
    //!
    virtual UINT64 GetRowRemapReservedRamAmount() const { return 0; }

private:
    const char   *m_Name;
    GpuSubdevice *m_pGpuSubdevice;
    LwRm         *m_pLwRm;

protected:
    struct LwRmPtr
    {
        LwRmPtr() = delete;
    };
    GpuSubdevice * GpuSub()      const { return m_pGpuSubdevice; }
    LwRm         * GetRmClient() const { return m_pLwRm; }
    virtual RC InitFbpInfo(); // Initalize FBP, FBIO, and LTC information
    UINT32 m_FbpMask         = 0;
    UINT32 m_MaxFbpCount     = 0;
    UINT32 m_FbiosPerFbp     = 0;
    UINT32 m_FbioMask        = 0;
    UINT32 m_MaxFbioCount    = 0;
    UINT32 m_MaxLtcsPerFbp   = 0;
    UINT32 m_LtcMask         = 0;
    UINT32 m_MaxLtcCount     = 0;
    UINT32 m_MaxSlicesPerLtc = 0;
    UINT32 m_L2SliceSize     = 0;
    UINT32 m_L2FSSlices      = 0;
    vector<UINT32> m_L2SliceMasks;
    vector<UINT32> m_LTCToLTSMap;
};

inline char FrameBuffer::VirtualFbioToLetter(UINT32 virtualFbio) const
{
    return 'A' + VirtualFbioToHwFbio(virtualFbio);
}

inline bool FrameBuffer::IsHbm() const
{
    switch (GetRamProtocol())
    {
        case RamHBM1:
        case RamHBM2:
        case RamHBM3:
            return true;
        default:
            return false;
    }
}

inline bool FrameBuffer::IsHbm3() const
{
    switch (GetRamProtocol())
    {
        case RamHBM3:
            return true;
        default:
            return false;
    }
}
