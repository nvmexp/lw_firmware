/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "fusetypes.h"

#include "gpu/fuse/fuse.h"

#include "core/include/fuseutil.h"

#include "gpu/fuse/skuhandler.h"
#include "gpu/fuse/fuseencoder.h"
#include "gpu/fuse/fusehwaccess.h"
#include "gpu/fuse/downbin/downbinimpl.h"
#include "gpu/fuse/iffrecord.h"

#include <map>
#include <memory>
#include <vector>

class TestDevice;

class NewFuse : public Fuse
{
public:
    NewFuse
    (
        TestDevice* pDev,
        unique_ptr<SkuHandler>&& pSkuHandler,
        shared_ptr<FuseEncoder>&& pEncoder,
        map<FuseMacroType, shared_ptr<FuseHwAccessor>>&& hwAccessors,
        unique_ptr<DownbinImpl>&& pDownbinImpl
    );
    virtual ~NewFuse() = default;

    static unique_ptr<Fuse> CreateFuseObj(TestDevice* pDev);
     
    virtual unique_ptr<FusePromptBase> CreateFusePromptHandler();
    virtual string GetPromptMessage();

    RC BlowFuses(const map<string, UINT32>& fuseValues) override;
    RC BlowFuseRows(const vector<UINT32>& regsToBlow, const FuseMacroType& macroType) override;
    RC SetSwFuses(const map<string, UINT32>& fuseValues) override;

    RC FuseSku(const string& sku) override;
    RC SwFuseSku(const string& sku) override;

    RC SetOverride(const string& fuseName, FuseOverride fuseOverride) override;
    RC SetOverrides(const map<string, FuseOverride>& overrides) override;

    RC GetSkuReworkInfo
    (
        const string& srcSku,
        const string &destSku,
        const string &fileName
    ) override;

    RC PrintFuses(bool optFusesOnly) override;
    RC PrintRawFuses() override;

    RC DecodeIff() override;
    RC DecodeIff(const string& sku) override;

    RC FindMatchingSkus(vector<string>* pMatchingSkus, FuseFilter tolerance) override;

    RC DoesSkuMatch
    (
        const string& sku,
        bool* pDoesMatch,
        FuseFilter filter,
        Tee::Priority pri
    ) override;

    RC SetDownbinInfo(Downbin::DownbinInfo downbinInfo) override;

    RC CheckSwSku(string skuName, FuseFilter filter) override;
    RC CheckAteFuses(string skuName) override;

    RC GetFuseValue(const string& fuseName, FuseFilter tol, UINT32* pVal) override;

    RC PrintMismatchingFuses(const string& chipSku, FuseFilter filter) override;
    RC GetMismatchingFuses
    (
        const string& chipSku,
        FuseFilter filter,
        FuseDiff* pRetInfo
    ) override;

    bool IsSwFusingAllowed() override;
    RC SwReset() override;

    bool IsFusePrivSelwrityEnabled() override;

    void MarkFuseReadDirty() override;
    void DisableWaitForIdle() override;
    void UseTestValues() override;
    void CheckReconfigFuses(bool check) override;
    bool IsReconfigFusesCheckEnabled() override;
    void SetUseUndoFuse(bool b) override;
    bool GetUseUndoFuse() override;

    void ClearIgnoreList() override;
    void AppendIgnoreList(string fuseToIgnore) override;

    RC RequestRirOnFuse(const string& fuseName) override;
    RC RequestDisableRirOnFuse(const string& fuseName) override;
    void SetNumValidRir(UINT32 numValidRecords) override;
    void SetOverridePoisonRir(UINT32 val) override;

    void AppendFsFuseList(const string& fsFuseName) override;

    void SetIgnoreRawOptMismatch(bool ignore) override;

    RC SetCoreVoltage (UINT32 vddMv) override;
    RC SetFuseBlowTime(UINT32 blowTimeNs) override;

    string GetFuseFilename() override;

    virtual RC GetFsInfo
    (
        const string &fileName,
        const string &chipSku,
        map<string, FuseUtil::DownbinConfig>* pConfigs
    ) override;

    virtual void SetNumSpareFFRows(const UINT32 numSpareFFRows) override;
    virtual RC SetReworkBinaryFilename(const string& filename) override;

    virtual RC DumpFsInfo(const string &chipSku) override;
    virtual RC FuseOnlyFs(bool isSwFusing, const string& fsFusingOpt) override;
    virtual RC GetFuseConfig
    (
        const string &chipSku,
        map<string, FuseUtil::DownbinConfig>* pFuseConfig
    ) override;

    RC GetFusesInfo(bool optFusesOnly, map<string, UINT32>& fuseNameValPairs) override;

    virtual RC DumpFsScriptInfo() override;
protected:
    unique_ptr<FusePromptBase> m_pPromptHandler;

private:
    TestDevice* m_pDev;
    unique_ptr<SkuHandler> m_pSkuHandler;
    shared_ptr<FuseEncoder> m_pEncoder;
    map<FuseMacroType, shared_ptr<FuseHwAccessor>> m_HwAccessors;
    unique_ptr<DownbinImpl> m_pDownbin;

    static constexpr UINT32 ALL_RIR_VALID = ~0U;
    UINT32 m_NumValidRir        = ALL_RIR_VALID;
    UINT32 m_OverridePoisonRir  = ~0U;
    string m_ReworkBinaryFilename = "fuserework.bin";

    RC BlowFuses(const FuseValues& fuseRequest, UINT32 numRecords);
    RC SetSwFuses(const FuseValues& fuseRequest);

    RC BlowFuseRows(const FuseBinaryMap& fuseBinary);
    RC WriteSwFuses(const SwFuseMap& swFuseWrites);
    RC SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc);
    RC VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc);

    RC RunFuseReworkBinary(const string &sku, const FuseValues& fuseRequest);

    RC PrintReworkInfoToFile
    (
        const ReworkRequest &reworkVals,
        const string &fileName
    );

    RC PrintDecodedIffRecords(const vector<shared_ptr<IffRecord>> &iffRecords);

    RC IlwalidateRirRecords(FuseBinaryMap* pFuseBinary, UINT32 numValidRir);

    //
    // WIP functions
    //
public:

    // Fusing commands
    RC BlowArray(vector<UINT32>& regs, string method, UINT32 durationNs, UINT32 lwvdd) override { return RC::UNSUPPORTED_FUNCTION; } // Handled by FuseSku

    // Printing fuses

    // Simulated Fuses
    void ClearSimulatedFuses() override {}
    RC GetSimulatedFuses(vector<UINT32>* pVec) override { return RC::UNSUPPORTED_FUNCTION; }
    void SetSimulatedFuses(const vector<UINT32>& pVec) override { }

    RC GetCachedFuseReg(vector<UINT32>* pFuseCache) override;

    // Move to FuseUtils
    FuseSpec* GetFuseSpec() override { return nullptr; }

//==============================================================================
// Deprecated functions
// These functions will be removed over later changes
public:
    void PrintRecords() override { MASSERT(false); }
};
