/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2017,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDE_GM10X_FUSE_H
#define INCLUDE_GM10X_FUSE_H

#include "gpufuse.h"


#define IFF_RECORD(suffix) LW_PBUS_FUSE_FMT_IFF_RECORD_ ## suffix
#define IFF_OPCODE(op) IFF_RECORD(CMD_OP_ ## op)
#define IFF_SPACE(space) IFF_RECORD(CMD_SET_FIELD_SPACE_LW_ ## space)

class GM10xFuse : public GpuFuse
{
public:
    GM10xFuse(Device *pDevice);

    // return true since fuses are all visible
    bool IsFuseVisible() override { return true; }
    // no-op
    void SetFuseVisibility(bool IsVisible) override {}

    //! Writes a value to SW Priv register based on the Spec
    //! Calls GetDesiredSwFuseVal -> from GpuFuse
    virtual RC SetSwFuse(const FuseUtil::FuseDef* pDef, UINT32 Value);
    virtual RC GetFuseWord(UINT32 FuseRow, FLOAT64 TimeoutMs,
                           UINT32 *pRetData);

protected:

    string m_GPUSuffix;

    //In Maxwell replace record format
    //-------------------------------------------------------------------------
    //| Data (16 bits) | Block ID (10 bits) | Reserved (1bit) | Chain (5 bits)|
    //-------------------------------------------------------------------------
    //
    // 1 bit reserved to figure if a fuseless record is IFF or not
    // Block ID refers to an index of non-overlapping 16-bit data block within
    // a JTAG chain
    //
    UINT32 GetRecordTypeSize() const override { return TYPE_SIZE; }
    UINT32 GetRecordAddressSize() const override { return BLOCK_ID_SIZE; }
    UINT32 GetRecordDataSize() const override { return REPLACE_DATA_SIZE; }

    RC ReadInRecords() override;
    RC ProcessNonRRFuseRecord(UINT32 RecordType,
                              UINT32 ChainId,
                              const vector<UINT32> &FuseRegs,
                              UINT32 *pLwrrOffset) override;
    virtual bool PollFusesIdle();

    // Maxwell family defines its own bits to represent a replace record
    // MODS fuse record read/writes will still follow the same algorithm
    UINT32 GetReplaceRecordBitPattern() const override { return REPLACE_REC; }

    // Every JTAG chain offset sits in a 16-bit block in Maxwell.
    // When reading the fuse offset we need to colwert the block ID of the
    // 16-bit block in which a JTAG chain offset resides into a 16 bit offset
    // (lowest being block ID 0)
    UINT32 GetBitOffsetInChain (UINT32 Offset) override { return (Offset << 4); }
    UINT32 GetRecordAddress (UINT32 Offset) override { return (Offset >> 4); }

    RC GetSelwrePmuUcode(bool isPMUBinaryEncrypted,
                         vector<UINT32> *pUcode,
                         vector<PMU::PmuUcodeInfo> *pData,
                         vector<PMU::PmuUcodeInfo> *pCode) override;
    RC ReadFromFuseBinary(const string &fuseChunk,
                          vector<UINT08> fuseBinary,
                          vector<UINT32> *data) override;
    RC GetFuseKeyFromFile(string fileName, size_t keyLength,
                          string &key) override;
    RC GetFuseKeyToInstall(string &installFuseKey) override;
    RC SetInstalledFuseKey(string fuseKey) override;
    RC EnableSelwreFuseblow(bool isPMUBinaryEncrypted) override;
    bool IsFusePrivSelwrityEnabled() override;
    bool IsFuseSelwritySupported() override { return true; }
    RC ClearPrivSelwritySwFuse() override;
    RC CachePrivSelwritySwFuse() override;
    RC RestorePrivSelwritySwFuse() override;

    // SW fusing support in Maxwell
    bool IsSwFusingEnabled() override;
    bool IsSwFusingAllowed() override;

    //! Enables the software override feature in Maxwell chips
    RC EnableSwFusing(const string Method,
                      UINT32 DurationNs,
                      UINT32 LwVddMv) override;
    RC DecodeIff() override;
    RC DecodeSkuIff(string skuName) override;
    RC GetIffRecords(vector<FuseUtil::IFFRecord>* pRecords, bool keepDeadRecords) override;

protected:
    // constants for fuseless fuse type
    static const UINT32 TYPE_SIZE           = 1; // IFF bit
    static const UINT32 CHAINID_SIZE        = 5;
    static const UINT32 BLOCK_ID_SIZE       = 10;
    static const UINT32 REPLACE_DATA_SIZE   = 16;
    static const UINT32 REPLACE_RECORD_SIZE = 32;

    string m_L2Key;
    map<UINT32, string> m_SetFieldSpaceName;
    map<UINT32, UINT32> m_SetFieldSpaceAddr;

    enum RecordType
    {
        REPLACE_REC    = 0,
        INIT_FROM_FUSE = 1,
    };

    virtual RC DecodeIffRecords(const vector<FuseUtil::IFFRecord>& iffRecords);

private:
    UINT32 m_PrivSecSwFuseValue;
    virtual RC BlowFuseRows(const vector<UINT32> &NewBitsToBLow,
                            const vector<UINT32> &RegsToBlow,
                            FuseSource*           pFuseSrc,
                            FLOAT64               TimeoutMs);

    virtual const FuseUtil::FuseDef* GetSwFusingFuseDef();
    
};

#endif

