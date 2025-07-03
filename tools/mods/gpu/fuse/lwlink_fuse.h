/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDE_LWLINK_FUSE_H
#define INCLUDE_LWLINK_FUSE_H

#include "gpu/fuse/fuse.h"
#include "core/include/fuseparser.h"

#include <vector>

class Device;
struct JSObject;

#define IFF_RECORD(suffix) LW_PBUS_FUSE_FMT_IFF_RECORD_ ## suffix
#define IFF_OPCODE(op) IFF_RECORD(CMD_OP_ ## op)
#define IFF_SPACE(space) IFF_RECORD(CMD_SET_FIELD_SPACE_LW_ ## space)

// Fuse class for fusable LwLink devices (originally, only LwSwitch)
class LwLinkFuse : public OldFuse
{
public:
    LwLinkFuse(Device *pDevice);
    virtual ~LwLinkFuse() { };

    FuseSpec* GetFuseSpec() { return &m_FuseSpec; }

    RC CacheFuseReg() override;
    RC GetCachedFuseReg(vector<UINT32> *pCachedReg);
    virtual void MarkFuseReadDirty() { m_FuseReadDirty = true; }

    bool IsFuseSupported(const FuseUtil::FuseInfo &spec, Tolerance strictness) override;

    RC BlowArray(const vector<UINT32> &regsToBlow,
                 string                method,
                 UINT32                durationNs,
                 UINT32                lwVddMv);

    RC DumpFuseRecToRegister(vector<UINT32> *pFuseReg) override;
    RC ReadInRecords() override;
    void PrintRecords() override;
    virtual UINT32 GetFuselessRecordCount() { return m_AvailableRecIdx; }

    RC GetIffRecords(vector<FuseUtil::IFFRecord>* pRecords, bool keepDeadRecords) override;
    RC ApplyIffPatches(string chipSku, vector<UINT32>* pRegsToBlow) override;

    string GetFuseFilename();

    void SetReblowAttempts(UINT32 attempts){ m_ReblowAttempts = attempts; }
    UINT32 GetReblowAttempts(){ return m_ReblowAttempts; }

    void EnableWaitForIdle(bool toEnable) { m_WaitForIdleEnabled = toEnable; };
    bool IsWaitForIdleEnabled() const { return m_WaitForIdleEnabled; };

    virtual RC DecodeIff();
    virtual RC DecodeSkuIff(string skuName);

protected:

    // Generic format of a fuse instruction (record):
    //
    // Volta has 2 types of instructions:
    // Fuseless fuse and IFF
    // For fuseless fuses in Volta, non-IFF records behave like replace records
    // MODS will also support IFF fuses although it is a CYA feature
    //
    struct FuseRecord
    {
        UINT32 data;
        UINT32 dataSize;   // num bits in the data field
        UINT32 type;
        UINT32 offset;
        UINT32 offsetSize;  // num bits in the offset field
        UINT32 chainId;
        UINT32 totalSize;
    };

    // Volta fuseless fuse record format
    //-------------------------------------------------------------------
    //| Data (16 bits) | Offset (10 bits) | Type (1bit) | Chain (5 bits)|
    //-------------------------------------------------------------------

    // constants for fuseless fuse type
    static const UINT32 CHAINID_SIZE        = 5;
    static const UINT32 TYPE_SIZE           = 1;
    static const UINT32 REPLACE_OFFSET_SIZE = 10;
    static const UINT32 REPLACE_DATA_SIZE   = 16;
    static const UINT32 ILWALID_CHAIN_ID    = 0x1F;
    static const UINT32 REPLACE_RECORD_SIZE = 32;

    enum RecordType
    {
        REPLACE_REC    = 0,
        INIT_FROM_FUSE = 1,
    };

    FuseSpec       m_FuseSpec;
    UINT32         m_ReblowAttempts = 0;
    bool           m_WaitForIdleEnabled = true;

    map<UINT32, map<UINT32, bool> > m_JtagChain;
    UINT32 m_AvailableRecIdx = 0;       // this is the first empty record index
    vector<FuseRecord> m_Records;

    map<UINT32, string> m_SetFieldSpaceName;
    map<UINT32, UINT32> m_SetFieldSpaceAddr;

    virtual RC GetFuseWord(UINT32 fuseRow, FLOAT64 timeoutMs, UINT32 *pRetData);

    bool DoesOptAndRawFuseMatch(const FuseUtil::FuseDef* pDef,
                                UINT32 rawFuseVal,
                                UINT32 optFuseVal) override;

    UINT32 ExtractFuseVal(const FuseUtil::FuseDef* pDef, Tolerance strictness) override;
    virtual UINT32 ExtractFuselessFuseVal(const FuseUtil::FuseDef* pDef, Tolerance strictness);
    UINT32 ExtractOptFuseVal(const FuseUtil::FuseDef* pDef) override;

    void GetPartialFuseVal(UINT32 rowNumber, UINT32 lsb, UINT32 msb, UINT32 *pFuseVal);

    RC SetPartialFuseVal(const list<FuseUtil::FuseLoc> &fuseLocList,
                         UINT32 desiredValue,
                         vector<UINT32> *pRegsToBlow) override;

    virtual RC ProcessNonRRFuseRecord(UINT32 recordType,
                                      UINT32 chainId,
                                      const vector<UINT32> &fuseRegs,
                                      UINT32 *pLwrrOffset);

    static UINT32 GetFuseSnippet(const vector<UINT32> &fuseRegs,
                                 UINT32 numBits,
                                 UINT32 *pLwrrOffset);

    static RC WriteFuseSnippet(vector<UINT32> *pFuseRegs,
                               UINT32 valToOR,
                               UINT32 numBits,
                               UINT32 *pLwrrOffset);

    // Every JTAG chain offset sits in a 16-bit block in Maxwell.
    // When reading the fuse offset we need to colwert the block ID of the
    // 16-bit block in which a JTAG chain offset resides into a 16 bit offset
    // (lowest being block ID 0)
    UINT32 GetBitOffsetInChain (UINT32 offset) { return (offset << 4); }
    UINT32 GetRecordAddress (UINT32 offset) { return (offset >> 4); }

    UINT32 GetRecordTypeSize() const { return TYPE_SIZE; }
    UINT32 GetRecordAddressSize() const { return REPLACE_OFFSET_SIZE; }
    UINT32 GetRecordDataSize() const { return REPLACE_DATA_SIZE; }

    UINT32 GetReplaceRecordBitPattern() const { return REPLACE_REC; }

    RC DecodeIffRecords(const vector<FuseUtil::IFFRecord>& iffRecords);

    // API to fetch the per GPU secure PMU code
    UINT32 *GetSelwrePmuUcode() const { return nullptr; }

    static bool PollFusesIdle(void *pWrapperParam);
    static bool PollFuseSenseDone(void *pParam);

    RC ParseXmlInt(const FuseUtil::FuseDefMap **ppFuseDefMap,
                   const FuseUtil::SkuList    **ppSkuList,
                   const FuseUtil::MiscInfo   **ppMiscInfo,
                   const FuseDataSet          **ppFuseDataSet) override;

    RC BlowFuseRows(const vector<UINT32> &newBitsToBLow,
                    FLOAT64               timeoutMs);

    virtual RC BlowFusesInt(const vector<UINT32> &regsToBlow);

    RC EnableFuseSrc(bool setHigh);

    RC LatchFuseToOptReg(FLOAT64 timeoutMs);
    RC FindConsistentRead(vector<UINT32> *pFuseRegs, FLOAT64 timeoutMs);

    RC UpdateRecord(const FuseUtil::FuseDef &Def, UINT32 value) override;
    RC UpdateSubFuseRecord(FuseUtil::FuseLoc subFuse, UINT32 newDataField);

    RC AppendRecord(UINT32 chainId, UINT32 offset, UINT32 data);
    UINT32 GetJtagChainData(UINT32 chainId, UINT32 msb, UINT32 lsb);
    RC UpdateJtagChain(const FuseRecord &record);
    RC RereadJtagChain();
};

#endif
