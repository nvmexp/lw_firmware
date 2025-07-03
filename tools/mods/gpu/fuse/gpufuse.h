/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007,2009,2013,2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDE_GPUFUSE_H
#define INCLUDE_GPUFUSE_H

#include "core/utility/fuse.h"
#include "core/include/fuseparser.h"
#include "gpu/perf/pmusub.h"

#include "gpu/fuse/fuse.h"

class Device;
class GpuSubdevice;
class FuseSource;
struct JSObject;

class GpuFuse : public OldFuse
{
public:
    GpuFuse(Device *pDevice);
    virtual ~GpuFuse() { };

    // Todo: yick... figure out a way to clean this up

    FuseSpec* GetFuseSpec() { return &m_FuseSpec; }

    static GpuFuse* CreateFuseObj(GpuSubdevice* pSubdev);

    RC CacheFuseReg() override;
    RC GetCachedFuseReg(vector<UINT32> *pCachedReg);
    virtual void MarkFuseReadDirty() { m_FuseReadDirty = true; }

    bool IsFuseSupported(const FuseUtil::FuseInfo &Spec, Tolerance Strictness) override;

    RC BlowArray(const vector<UINT32> &RegsToBlow,
                 string                Method,
                 UINT32                DurationNs,
                 UINT32                LwVddMv);

    RC GetKFuseWord(UINT32 Row, UINT32 *pFuseVal, FLOAT64 TimeoutMs);
    RC BlowKFuses(const vector<UINT32> &RegsToBlow, FuseSource* pFuseSrc);

    RC DumpFuseRecToRegister(vector<UINT32> *pFuseReg) override;
    RC ReadInRecords() override;
    void PrintRecords() override;
    virtual UINT32 GetFuselessRecordCount() { return m_AvailableRecIdx; }

    RC ApplyIffPatches(string chipSku, vector<UINT32>* pRegsToBlow) override { return OK; }

    string GetFuseFilename();

    void SetReblowAttempts(UINT32 Attempts){ m_ReblowAttempts = Attempts; }
    UINT32 GetReblowAttempts(){ return m_ReblowAttempts; }

    virtual RC EnableSwFusing(const string Method, UINT32 DurationNs, UINT32 LwVddMv)
        { return RC::UNSUPPORTED_FUNCTION; }
    virtual bool IsSwFusingEnabled() { return false; }
    virtual bool IsSwFusingAllowed() { return false; }
    // after SW fusing is enabled, set a SKU by writing the SW OPT fuses reg
    RC SetSwSku(const string SkuName);
    RC SetSwFuseByName(string FuseName, UINT32 Value);
    virtual RC SetSwFuse(const FuseUtil::FuseDef* pDef, UINT32 Value)
        { return RC::UNSUPPORTED_FUNCTION; }

    RC CheckSwSku(const string SkuName);

    virtual RC SwReset();

    // Send fusekey to JS to be written to the daughter card.
    virtual RC GetFuseKeyToInstall(string &installFuseKey)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    //Encrypt PMU binary
    RC EncryptFuseFile(string fileName, string keyFileName, bool isEncryptionKey);

    // Set fusekey read from inforom of fuse daughter card
    virtual RC SetInstalledFuseKey(string fuseKey)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    void EnableMarginRead(bool ToEnable) { m_MarginReadEnabled = ToEnable; };
    bool IsMarginReadEnabled() const { return m_MarginReadEnabled; };
    void EnableWaitForIdle(bool ToEnable) { m_WaitForIdleEnabled = ToEnable; };
    bool IsWaitForIdleEnabled() const { return m_WaitForIdleEnabled; };

    // These secure PMU status codes are based on //sw/pvt/mods/fuse/src/codes.h
    enum SelwrePmuStatusCode
    {
        SELWRE_PMU_STATUS_SUCCESS = 0xCAFE,
        SELWRE_PMU_STATUS_UCODE_VERSION_MISMATCH = 0xBAD1,
        SELWRE_PMU_STATUS_UNSUPPORTED_GPU = 0xBAD2,
        SELWRE_PMU_STATUS_FAILURE = 0xBAD3
    };

    const static UINT32 FUSE_KEY_LENGTH   = 16; //Encryption key is 128 bit

    // API to support secure fuseblow feature
    virtual RC EnableSelwreFuseblow(bool isPMUBinaryEncrypted)
        { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetSelwrePmuUcode(bool isPMUBinaryEncrypted,
                                 vector<UINT32> *pUcode,
                                 vector<PMU::PmuUcodeInfo> *pData,
                                 vector<PMU::PmuUcodeInfo> *pCode)
        { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetFuseKeyFromFile(string fileName, size_t keyLength,
                                  string &key)
        { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC ReadFromFuseBinary(const string &fuseChunk,
                                  vector<UINT08> fuseBinary,
                                  vector<UINT32> *data)
        { return RC::UNSUPPORTED_FUNCTION; }

    // API to indicate whether a GPU has fuse security enabled
    virtual bool IsFusePrivSelwrityEnabled() { return false; }

    // API to indicate whether a GPU supports fuse security
    virtual bool IsFuseSelwritySupported() { return false; }

    // API to clear priv security SW fuse settings
    virtual RC ClearPrivSelwritySwFuse() { return RC::UNSUPPORTED_FUNCTION; }

    // APIs to cache and restore priv security SW fuse
    virtual RC CachePrivSelwritySwFuse() { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC RestorePrivSelwritySwFuse() { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC DecodeIff() { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC DecodeSkuIff(string skuName) { return RC::UNSUPPORTED_FUNCTION; }

protected:

    // Generic format of a fuse instruction (record):
    //
    // Fermi/Kepler we have 4 types of instructions:
    // Short, Bitwise_OR, Block, Replace
    // For fuse blowing in Kepler, we only use replace records instructions
    //
    // Maxwell we have 2 types of instructions:
    // Replace and IFF
    // For fuse blowing in Maxwell, we will use replace records instructions
    // MODS will also support IFF fuses although it is a CYA feature
    //
    struct FuseRecord
    {
        UINT32 Data;
        UINT32 DataSize;   // num bits in the data field
        UINT32 Type;
        UINT32 Offset;
        UINT32 OffsetSize;  // num bits in the offset field
        UINT32 ChainId;
        UINT32 TotalSize;
    };

    //In Fermi/Kepler replace record format
    //-------------------------------------------------------------------
    //| Data (11 bits) | Offset (13 bits) | Type (3bit) | Chain (5 bits)|
    //-------------------------------------------------------------------

    // constants for fuseless fuse type
    static const UINT32 CHAINID_SIZE        = 5;
    static const UINT32 TYPE_SIZE           = 3;
    static const UINT32 REPLACE_OFFSET_SIZE = 13;
    static const UINT32 REPLACE_DATA_SIZE   = 11;
    static const UINT32 ILWALID_CHAIN_ID    = 0x1F;
    static const UINT32 REPLACE_RECORD_SIZE = 32;

    enum RecordType
    {
        SHORT_REC      = 0,
        BITWISE_OR_REC = 1,
        BLOCK_REC      = 3,
        REPLACE_REC    = 7,
    };

    FuseSpec       m_FuseSpec;
    UINT32         m_ReblowAttempts;
    bool           m_MarginReadEnabled;
    bool           m_WaitForIdleEnabled;

    map<UINT32, map<UINT32, bool> > m_JtagChain;
    UINT32 m_AvailableRecIdx;         // this is the first empty record index
    vector<FuseRecord> m_Records;

    virtual bool CanOnlyUseOptFuses() override;

    virtual RC GetFuseWord(UINT32 FuseRow, FLOAT64 TimeoutMs, UINT32 *pRetData);

    bool DoesOptAndRawFuseMatch(const FuseUtil::FuseDef* pDef,
                                UINT32 RawFuseVal,
                                UINT32 OptFuseVal) override;

    UINT32 ExtractFuseVal(const FuseUtil::FuseDef* pDef, Tolerance Strictness) override;
    virtual UINT32 ExtractFuselessFuseVal(const FuseUtil::FuseDef* pDef, Tolerance Strictness);
    UINT32 ExtractOptFuseVal(const FuseUtil::FuseDef* pDef) override;

    void GetPartialFuseVal(UINT32 RowNumber, UINT32 Lsb, UINT32 Msb, UINT32 *pFuseVal);

    RC SetPartialFuseVal(const list<FuseUtil::FuseLoc> &FuseLocList,
                         UINT32 DesiredValue,
                         vector<UINT32> *pRegsToBlow) override;

    virtual RC ProcessNonRRFuseRecord(UINT32 RecordType,
                                      UINT32 ChainId,
                                      const vector<UINT32> &FuseRegs,
                                      UINT32 *pLwrrOffset);

    static UINT32 GetFuseSnippet(const vector<UINT32> &FuseRegs,
                                     UINT32 NumBits,
                                     UINT32 *pLwrrOffset);

    static RC WriteFuseSnippet(vector<UINT32> *pFuseRegs,
                                   UINT32 ValToOR,
                                   UINT32 NumBits,
                                   UINT32 *pLwrrOffset);

    // In Fermi/Kepler family the "Offset" defined in the fuse record format
    // is such bit offset in the chain = chain offset
    virtual UINT32 GetBitOffsetInChain(UINT32 Offset) { return Offset; }
    virtual UINT32 GetRecordAddress(UINT32 Offset) { return Offset; }

    virtual UINT32 GetRecordTypeSize() const { return TYPE_SIZE; }
    virtual UINT32 GetRecordAddressSize() const { return REPLACE_OFFSET_SIZE; }
    virtual UINT32 GetRecordDataSize() const { return REPLACE_DATA_SIZE; }

    virtual UINT32 GetReplaceRecordBitPattern() const { return REPLACE_REC; }

    virtual bool IsFuseVisible();
    virtual void SetFuseVisibility(bool IsVisible);

    // API to fetch the per GPU secure PMU code
    virtual UINT32 *GetSelwrePmuUcode() const { return nullptr; }

    static bool PollFusesIdleWrapper(void *pWrapperParam);
    virtual bool PollFusesIdle();
    static bool PollFuseSenseDone(void *pParam);
    static bool PollKFusesIdle(void *pParam);

    RC ParseXmlInt(const FuseUtil::FuseDefMap **ppFuseDefMap,
                   const FuseUtil::SkuList    **ppSkuList,
                   const FuseUtil::MiscInfo   **ppMiscInfo,
                   const FuseDataSet          **ppFuseDataSet) override;

    virtual RC BlowFuseRows(const vector<UINT32> &NewBitsToBLow,
                            const vector<UINT32> &RegsToBlow,
                            FuseSource*           pFuseSrc,
                            FLOAT64               TimeoutMs);

    virtual RC BlowFusesInt(const vector<UINT32> &RegsToBlow,
                            UINT32                LwVddMv,
                            FuseSource*           pFuseSrc);

    UINT32 SetFuseBlowTime(UINT32 DurationNs);
    virtual RC LatchFuseToOptReg(FLOAT64 TimeoutMs);
    RC FindConsistentRead(vector<UINT32> *pFuseRegs, FLOAT64 TimeoutMs);

    RC UpdateRecord(const FuseUtil::FuseDef &Def, UINT32 Value) override;
    virtual RC UpdateSubFuseRecord(FuseUtil::FuseLoc SubFuse, UINT32 NewDataField);

    UINT32 GetSwFuseValToSet(const FuseUtil::FuseInfo &Info);

    RC AppendRecord(UINT32 ChainId, UINT32 Offset, UINT32 Data);
    UINT32 GetJtagChainData(UINT32 ChainId, UINT32 Msb, UINT32 Lsb);
    RC UpdateJtagChain(const FuseRecord &Record);
    RC RereadJtagChain();
};

#endif
