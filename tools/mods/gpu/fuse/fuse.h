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

#include "core/include/fuseutil.h"
#include "gpu/fuse/fusetypes.h"
#include "gpu/fuse/downbin/downbinfs.h"

#include <map>
#include <memory>
#include <vector>

class FuseSource;

class Fuse
{
public:
    virtual ~Fuse() = default;

    virtual RC BlowFuses(const map<string, UINT32>& fuseValues) = 0;
    virtual RC SetSwFuses(const map<string, UINT32>& fuseValues) = 0;

    virtual RC FuseSku(const string& sku) = 0;
    virtual RC SwFuseSku(const string& sku) = 0;

    virtual RC SetOverride(const string& fuseName, FuseOverride fuseOverride) = 0;
    virtual RC SetOverrides(const map<string, FuseOverride>& overrides) = 0;

    virtual RC GetSkuReworkInfo
    (
        const string& srcSku,
        const string &destSku,
        const string &fileName    
    ) = 0;

    virtual RC PrintFuses(bool optFusesOnly) = 0;
    virtual RC PrintRawFuses() = 0;

    virtual RC DecodeIff() = 0;
    virtual RC DecodeIff(const string& sku) = 0;

    virtual RC FindMatchingSkus(vector<string>* pMatchingSkus, FuseFilter filter) = 0;
    virtual RC DoesSkuMatch(const string& sku, bool* pDoesMatch, FuseFilter filter)
    { return DoesSkuMatch(sku, pDoesMatch, filter, FuseUtils::GetVerbosePriority()); }
    virtual RC DoesSkuMatch
    (
        const string& sku,
        bool* pDoesMatch,
        FuseFilter filter,
        Tee::Priority pri
    ) = 0;

    virtual RC GetFuseValue(const string& fuseName, FuseFilter tol, UINT32* pVal) = 0;

    virtual RC PrintMismatchingFuses(const string& chipSku, FuseFilter filter) = 0;
    virtual RC GetMismatchingFuses
    (
        const string& chipSku,
        FuseFilter filter,
        FuseDiff *pRetInfo
    ) = 0;

    virtual bool IsSwFusingAllowed() = 0;
    virtual bool IsFusePrivSelwrityEnabled() = 0;

    virtual void MarkFuseReadDirty() = 0;
    virtual void DisableWaitForIdle() = 0;
    virtual void UseTestValues() = 0;
    virtual void CheckReconfigFuses(bool check) = 0;
    virtual bool IsReconfigFusesCheckEnabled() = 0;

    virtual string GetFuseFilename() = 0;
    virtual FuseSpec* GetFuseSpec() = 0;

    virtual bool GetUseUndoFuse() = 0;
    virtual void SetUseUndoFuse(bool b) = 0;

    virtual RC RequestRirOnFuse(const string& fuseName) = 0;
    virtual RC RequestDisableRirOnFuse(const string& fuseName) = 0;
    virtual void SetNumValidRir(UINT32 numValidRecords) = 0;
    virtual void SetOverridePoisonRir(UINT32 val) = 0;

    virtual void AppendFsFuseList(const string &fsFuseName) = 0;

    virtual void SetIgnoreRawOptMismatch(bool ignore) = 0;

    virtual RC SetCoreVoltage(UINT32 vddMv) = 0;
    virtual RC SetFuseBlowTime(UINT32 blowTimeNs) = 0;

    virtual RC GetFsInfo
    (
        const string &fileName,
        const string &chipSku,
        map<string, FuseUtil::DownbinConfig>* pConfigs
    )
    {
        return FuseUtil::GetFsInfo(fileName, chipSku, pConfigs);
    }
    virtual void SetNumSpareFFRows(const UINT32 numSpareFFRows)
    {
        //Overidden only for Ga10x cases
    }
    virtual RC SetReworkBinaryFilename(const string& filename) = 0;

    virtual RC SetDownbinInfo(Downbin::DownbinInfo downbinInfo)
    {
        return RC::OK;
    }

    virtual RC DumpFsInfo(const string &chipSku)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    virtual RC FuseOnlyFs(bool isSwFusing, const string& fsFusingOpt)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    virtual RC GetFuseConfig(const string &chipSku, map<string, FuseUtil::DownbinConfig>* pFuseConfig)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    virtual RC GetFusesInfo(bool optFusesOnly, map<string, UINT32>& fuseNameValPairs)
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC DumpFsScriptInfo()
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

//==============================================================================
// Functions/enums not yet colwerted to the new interface
// The interface for these will likely change, but either their usage is
// a bit more ilwolved, or I'm not certain how I want to change them, so for
// now they're sticking around as-is until they get refactored in a smaller change
public:
    // SkuMatching
    virtual RC CheckSwSku(string skuName, FuseFilter filter) = 0;

    // Ignore fuses
    virtual void ClearIgnoreList() = 0;
    virtual void AppendIgnoreList(string ignoreMe) = 0;
    
    // Fusing commands
    virtual RC BlowArray(vector<UINT32>& regs, string method, UINT32 durationNs, UINT32 lwvdd) = 0;
    virtual RC BlowFuseRows(const vector<UINT32>& regsToBlow, const FuseMacroType&  macroType) = 0;

    // Verification
    virtual RC CheckAteFuses(string skuName) = 0;

    // Simulated Fuses
    virtual void ClearSimulatedFuses() = 0;
    virtual RC GetSimulatedFuses(vector<UINT32>* pVec) = 0;
    virtual void SetSimulatedFuses(const vector<UINT32>& pVec) = 0;

    virtual RC GetCachedFuseReg(vector<UINT32>* pBinary) = 0;

    virtual RC SwReset() = 0;

//==============================================================================
// Deprecated functions
// These functions will be removed over later changes
public:
    virtual void PrintRecords() = 0;
};

