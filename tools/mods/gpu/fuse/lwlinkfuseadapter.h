/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/fuse/fuse.h"
#include "gpu/fuse/gpufuse.h"

class TestDevice;

// Temporary class to use the old GpuFuse class interchangeably with the new Fuse classes
class LwLinkFuseAdapter : public Fuse
{
private:
    unique_ptr<LwLinkFuse> m_pLwlFuse;

public:
    LwLinkFuseAdapter(TestDevice* pDev)
        : m_pLwlFuse(new LwLinkFuse(pDev))
    { }

    virtual ~LwLinkFuseAdapter() = default;

    RC BlowFuses(const map<string, UINT32>& fuseValues) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC BlowFuseRows(const vector<UINT32>& regsToBlow, const FuseMacroType& macroType) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC SetSwFuses(const map<string, UINT32>& fuseValues) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC FuseSku(const string& sku) override
    {
        RC rc;
        vector<UINT32> regsToBlow;
        CHECK_RC(m_pLwlFuse->GetDesiredFuses(sku, &regsToBlow));
        CHECK_RC(m_pLwlFuse->BlowArray(regsToBlow, "jsfunc_FuseSrcToggle", 0, 0));
        return rc;
    }
    RC SwFuseSku(const string& sku) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SetOverride(const string& fuseName, FuseOverride fuseOverride) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC SetOverrides(const map<string, FuseOverride>& overrides) override
    {
        map<string, OldFuse::OverrideInfo> oldOverrides;
        for (const auto& override : overrides)
        {
            OldFuse::OverrideInfo oldOrd;
            oldOrd.Value = override.second.value;
            oldOrd.Flag = override.second.useSkuDef ?
                OldFuse::USE_SKU_FLOORSWEEP_DEF : OldFuse::AS_IS;
            oldOverrides[override.first] = oldOrd;
        }
        m_pLwlFuse->SetOverrides(oldOverrides);
        return OK;
    }

    RC GetSkuReworkInfo
    (
        const string& srcSku,
        const string &destSku,
        const string &fileName    
    ) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC PrintFuses(bool optFusesOnly) override
    {
        if (optFusesOnly)
        {
            return RC::UNSUPPORTED_FUNCTION;
        }
        return m_pLwlFuse->PrintFuseValues("", OldFuse::ALL_FUSES,
            OldFuse::ALL_MATCH, FuseUtil::FLAG_ALLTYPES);
    }
    RC PrintRawFuses() override
    {
        RC rc;
        vector<UINT32> fuseRows;
        CHECK_RC(m_pLwlFuse->GetCachedFuseReg(&fuseRows));
        Printf(Tee::PriNormal,
               "\nTotal number of Fuse rows = %u\n",
               static_cast<UINT32>(fuseRows.size()));
        for (UINT32 row = 0; row < fuseRows.size(); row++)
        {
            Printf(Tee::PriNormal, "%3d: 0x%08x\n", row, fuseRows[row]);
        }

        return rc;
    }

    RC DecodeIff() override
    {
        return m_pLwlFuse->DecodeIff();
    }
    RC DecodeIff(const string& sku) override
    {
        return m_pLwlFuse->DecodeSkuIff(sku);
    }

    RC FindMatchingSkus(vector<string>* pMatchingSkus, FuseFilter filter) override
    {
        return m_pLwlFuse->FindSkuMatch(pMatchingSkus, FuseFilterToTolerance(filter));
    }
    RC DoesSkuMatch
    (
        const string& sku,
        bool* pDoesMatch,
        FuseFilter filter,
        Tee::Priority
    ) override
    {
        RC rc;
        vector<string> foundSkus;
        CHECK_RC(m_pLwlFuse->FindSkuMatch(&foundSkus, FuseFilterToTolerance(filter)));
        *pDoesMatch = (find(foundSkus.begin(), foundSkus.end(), sku) != foundSkus.end());
        return rc;
    }

    RC GetFuseValue(const string& fuseName, FuseFilter filter, UINT32* pVal) override
    {
        *pVal = m_pLwlFuse->GetFuseValGivenName(fuseName, FuseFilterToTolerance(filter));
        return OK;
    }

    bool IsSwFusingAllowed() override
    {
        return false;
    }
    bool IsFusePrivSelwrityEnabled() override
    {
        return false;
    }

    void MarkFuseReadDirty() override
    {
        m_pLwlFuse->MarkFuseReadDirty();
    }
    void DisableWaitForIdle() override
    {
        // Replaces EnableWaitForIdle(false);
    }
    void UseTestValues() override
    {
        // New for Turing
    }
    void CheckReconfigFuses(bool check) override
    {
        // New for Turing
    }
    bool IsReconfigFusesCheckEnabled() override
    {
        // New for Turing
        return true;
    }
    void AppendFsFuseList(const string& fsFuseName) override
    {
        // New for Turing
    }
    void SetIgnoreRawOptMismatch(bool ignore) override
    {
        // New for Ampere
    }

    RC SetCoreVoltage (UINT32 vddMv) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SetFuseBlowTime(UINT32 blowTimeNs) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    string GetFuseFilename() override
    {
        return m_pLwlFuse->GetFuseFilename();
    }
    FuseSpec* GetFuseSpec() override
    {
        return m_pLwlFuse->GetFuseSpec();
    }
    RC SetReworkBinaryFilename(const string& filename) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

//==============================================================================
// Functions not yet colwerted to the new interface
// The interface for these functions will likely change, but either their usage is
// a bit more ilwolved, or I'm not certain how I want to change them, so for
// now they're sticking around as-is until they get refactored in a smaller change
public:
    // SkuMatching
    RC CheckSwSku(string skuName, FuseFilter filter) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Ignore fuses
    void ClearIgnoreList() override
    {
        m_pLwlFuse->ClearIgnoreList();
    }
    void AppendIgnoreList(string ignoreMe) override
    {
        m_pLwlFuse->AppendIgnoreList(ignoreMe);
    }

    // Undo
    bool GetUseUndoFuse() override
    {
        return m_pLwlFuse->GetUseUndoFuse();
    }
    void SetUseUndoFuse(bool b) override
    {
        m_pLwlFuse->SetUseUndoFuse(b);
    }

    // RIR
    RC RequestRirOnFuse(const string& fuseName) override
    {
        m_pLwlFuse->RequestRirOnFuse(fuseName);
        return RC::OK;
    }
    RC RequestDisableRirOnFuse(const string& fuseName) override
    {
        m_pLwlFuse->RequestDisableRirOnFuse(fuseName);
        return RC::OK;
    }
    void SetNumValidRir(UINT32 numValidRecords) override
    {
        // New for Ampere
    }
    void SetOverridePoisonRir(UINT32 val) override
    {
        // New for Ampere
    }

    // Fusing commands
    RC BlowArray(vector<UINT32>& regs, string method, UINT32 durationNs, UINT32 lwvdd) override
    {
        return m_pLwlFuse->BlowArray(regs, method, durationNs, lwvdd);
    }

    // Verification
    RC CheckAteFuses(string skuName) override
    {
        return m_pLwlFuse->CheckAteFuses(skuName);
    }

    // Printing fuses
    RC PrintMismatchingFuses(const string& ChipSku,
                       FuseFilter filter) override
    {
        return m_pLwlFuse->PrintFuseValues(ChipSku,
            OldFuse::WhichFuses::BAD_FUSES,
            FuseFilterToTolerance(filter), ALL_FUSES);
    }

    // Simulated Fuses
    void ClearSimulatedFuses() override
    {
        m_pLwlFuse->ClearSimulatedFuses();
    }
    RC GetSimulatedFuses(vector<UINT32>* pVec) override
    {
        return m_pLwlFuse->GetSimulatedFuses(pVec);
    }
    void SetSimulatedFuses(const vector<UINT32>& pVec) override
    {
        m_pLwlFuse->SetSimulatedFuses(pVec);
    }

    RC GetCachedFuseReg(vector<UINT32>* pBinary) override
    {
        return m_pLwlFuse->GetCachedFuseReg(pBinary);
    }

    RC GetMismatchingFuses
    (
        const string& ChipSku,
        FuseFilter filter,
        FuseDiff* pRetInfo
    ) override
    {
        vector<OldFuse::FuseVals> retInfo;
        RC rc = m_pLwlFuse->GetFusesInfo(ChipSku,
            OldFuse::WhichFuses::BAD_FUSES,
            FuseFilterToTolerance(filter),
            FuseUtil::FLAG_ALLTYPES, &retInfo);
        for (auto val : retInfo)
        {
            pRetInfo->namedFuseMismatches.push_back(FuseVals(val));
        }
        return rc;
    }

    RC SwReset() override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

//==============================================================================
// Deprecated functions
// These functions will be removed over later changes
public:
    void PrintRecords() override
    {
        m_pLwlFuse->PrintRecords();
    }
};
