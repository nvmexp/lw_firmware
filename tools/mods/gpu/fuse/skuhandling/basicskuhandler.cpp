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

#include "gpu/include/gpusbdev.h"

#include "gpu/fuse/skuhandling/basicskuhandler.h"
#include "gpu/fuse/fuseutils.h"

using namespace FuseUtil;

RC BasicSkuHandler::Initialize()
{
    RC rc;
    string fuseFile = FuseUtils::GetFuseFilename(m_pDev->GetDeviceId());
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fuseFile));
    CHECK_RC(pParser->ParseFile(fuseFile, nullptr, &m_pSkuList, &m_pMiscInfo));
    m_IsInitialized = true;
    return rc;
}

RC BasicSkuHandler::GetDesiredFuseValues
(
    const string& sku,
    bool useLwrrOptFuses,
    bool bIsSwFusing,
    FuseValues* pRequest
)
{
    MASSERT(pRequest != nullptr);
    pRequest->Clear();
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }
    auto skuIter = m_pSkuList->find(sku);
    if (skuIter == m_pSkuList->end())
    {
        Printf(FuseUtils::GetErrorPriority(), "Cannot find SKU %s!\n", sku.c_str());
        return RC::ILWALID_ARGUMENT;
    }

    FuseFilter filter = useLwrrOptFuses ? FuseFilter::OPT_FUSES_ONLY :
                        FuseFilter::ALL_FUSES;
    const FuseUtil::SkuConfig& skuConfig = skuIter->second;
    if (bIsSwFusing)
    {
        for (auto &fuseName : m_SwFusingIgnoredFuses)
        {
            m_pEncoder->TempIgnoreFuse(fuseName);
        }
    }
    for (const auto& fuseInfo : skuConfig.FuseList)
    {
        const string& name = fuseInfo.pFuseDef->Name;
        UINT32 valToBurn;
        bool shouldBurn = false;
        CHECK_RC(GetFuseValToBurn(fuseInfo, filter, &valToBurn, &shouldBurn));
        if (shouldBurn)
        {
            pRequest->m_NamedValues[name] = valToBurn;
        }
    }

    // IFF
    pRequest->m_HasIff = true;
    CHECK_RC(GetSkuIffRecords(skuConfig, &pRequest->m_IffRecords));

    return rc;
}

RC BasicSkuHandler::AddDebugFuseOverrides(const string& sku)
{
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }
    auto skuIter = m_pSkuList->find(sku);
    if (skuIter == m_pSkuList->end())
    {
        Printf(FuseUtils::GetErrorPriority(), "Cannot find SKU %s!\n", sku.c_str());
        return RC::ILWALID_ARGUMENT;
    }
    const FuseUtil::SkuConfig& skuConfig = skuIter->second;

    // Add these fuse values as overrides
    // Will be applied only if they exist
    FuseOverride fuseFileVersion = { m_pMiscInfo->P4Data.Version, true };
    OverrideFuseVal("OPT_FUSE_FILE_VERSION", fuseFileVersion);

    FuseOverride skuId = { skuConfig.skuId, true };
    OverrideFuseVal("OPT_SKU_ID", skuId);

    return rc;
}

RC BasicSkuHandler::GetIffRecords
(
    const string& sku,
    vector<shared_ptr<IffRecord>>* pIffRecords
)
{
    MASSERT(pIffRecords != nullptr);
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }
    auto skuIter = m_pSkuList->find(sku);
    if (skuIter == m_pSkuList->end())
    {
        Printf(FuseUtils::GetErrorPriority(), "Cannot find SKU %s!\n", sku.c_str());
        return RC::ILWALID_ARGUMENT;
    }
    const FuseUtil::SkuConfig& skuConfig = skuIter->second;

    pIffRecords->clear();
    CHECK_RC(GetSkuIffRecords(skuConfig, pIffRecords));

    return rc;
}

RC BasicSkuHandler::GetSkuIffRecords
(
    const FuseUtil::SkuConfig& skuConfig,
    vector<shared_ptr<IffRecord>>* pRecords
)
{
    RC rc;
    CHECK_RC_MSG
    (
        m_pEncoder->GetIffRecords(skuConfig.iffPatches, pRecords),
        "Invalid IFF record in SKU %s\n",
        skuConfig.SkuName.c_str()
    );
    return rc;
}

RC BasicSkuHandler::GetFuseValToBurn
(
    const FuseUtil::FuseInfo& info,
    FuseFilter filter,
    UINT32* pValToBurn,
    bool* pShouldBurn
)
{
    MASSERT(pValToBurn != nullptr);
    MASSERT(pShouldBurn != nullptr);
    RC rc;

    const string& fuseName = info.pFuseDef->Name;
    *pShouldBurn = true;

    // Don't burn this fuse if we don't support it
    if (!m_pEncoder->IsFuseSupported(*info.pFuseDef, filter))
    {
        *pShouldBurn = false;
        return rc;
    }

    UINT32 lwrrentVal;
    CHECK_RC(m_pEncoder->GetFuseValue(fuseName, filter, &lwrrentVal));

    const FuseOverride* pOverride = nullptr;
    auto overrideIter = m_Overrides.find(fuseName);
    if (overrideIter != m_Overrides.end())
    {
        pOverride = &overrideIter->second;

        if (pOverride->useSkuDef)
        {
            lwrrentVal = pOverride->value;
        }
        else
        {
            *pValToBurn = pOverride->value;
            return rc;
        }
    }

    bool bIsReadProtected = false;
    if (std::find(m_ReadProtectedFuses.begin(), m_ReadProtectedFuses.end(), fuseName) !=
                  m_ReadProtectedFuses.end())
    {
        bIsReadProtected = true;
    }
    // Don't touch the read-only (i.e. ATE) fuses
    if ((info.Attribute & FuseUtil::FLAG_READONLY) != 0)
    {
        *pShouldBurn = false;
        return OK;
    }
    // Tracking fuses remain as-is
    else if ((info.Attribute & FuseUtil::FLAG_DONTCARE) != 0)
    {
        // Only write to Tracking fuses if there's an override
        if (pOverride != nullptr)
        {
            *pValToBurn = lwrrentVal;
        }
        else
        {
            *pShouldBurn = false;
        }
        return OK;
    }
    // ValMatchHex/ValMatchBin should use one of the given values
    // if it's already fused to that value, otherwise, it should
    // try to match one of the values.
    else if ((info.Attribute & FuseUtil::FLAG_MATCHVAL) != 0)
    {
        if (m_UseTestValues && info.hasTestVal)
        {
            if (bIsReadProtected ||
                m_pEncoder->CanBurnFuse(fuseName, filter, info.testVal))
            {
                *pValToBurn = info.testVal;
            }
        }
        else
        {
            // Check if any of our current values are valid, if so,
            // use that value instead of searching for a new one
            bool lwrValIsValid = false;
            for (auto validVal : info.Vals)
            {
                if (lwrrentVal == validVal)
                {
                    lwrValIsValid = true;
                    *pValToBurn = lwrrentVal;
                    break;
                }
            }
            if (!lwrValIsValid)
            {
                bool possible = false;
                for (auto validVal : info.Vals)
                {
                    if (bIsReadProtected ||
                        m_pEncoder->CanBurnFuse(fuseName, filter, validVal))
                    {
                        possible = true;
                        *pValToBurn = validVal;
                        break;
                    }
                }
                if (!possible)
                {
                    Printf(FuseUtils::GetErrorPriority(),
                        "cannot blow fuse %s\n", fuseName.c_str());
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }
            }
        }
    }
    // BurnCount burns bits to meet the given count
    else if ((info.Attribute & FuseUtil::FLAG_BITCOUNT) != 0)
    {
        UINT32 targetValue = (m_UseTestValues && info.hasTestVal) ? info.testVal : info.Value;
        // Try to start with a blank mask or the override if provided
        UINT32 value = 0;
        if (pOverride != nullptr && pOverride->useSkuDef)
        {
            value = pOverride->value;
        }
        if (bIsReadProtected ||
            !m_pEncoder->CanBurnFuse(fuseName, filter, value))
        {
            value = lwrrentVal;
        }

        UINT32 bitCount = Utility::CountBits(value);
        // Set any preferred bits first
        for (UINT32 i = 0; (i < info.Vals.size()) && (bitCount < targetValue); i++)
        {
            value |= 1 << info.Vals[i];
            bitCount = Utility::CountBits(value);
        }

        // Add bits until we reach our burn count
        for (UINT32 i = 0;
             static_cast<UINT32>(Utility::CountBits(value)) < targetValue;
             i++)
        {
            value |= 1 << i;
        }
        *pValToBurn = value;
    }
    // ValMatchBin for en/dis fuse types
    else if ((info.Attribute & FuseUtil::FLAG_UNDOFUSE) != 0)
    {
        UINT32 valToBurn;
        if (m_UseTestValues && info.hasTestVal)
        {
            valToBurn = info.testVal;
        }
        else if (info.StrValue == "0" || info.StrValue == "00")
        {
            valToBurn = 0;
        }
        else if (info.StrValue == "1" || info.StrValue == "01")
        {
            valToBurn = 1;
        }
        else if (info.StrValue == "10" || info.StrValue == "1x" || info.StrValue == "11")
        {
            valToBurn = 2;
        }
        else
        {
            Printf(FuseUtils::GetErrorPriority(), "Unknown Undo fuse type for %s: %s\n",
                fuseName.c_str(), info.StrValue.c_str());
            return RC::SOFTWARE_ERROR;
        }

        if (bIsReadProtected ||
            !m_pEncoder->CanBurnFuse(fuseName, filter, valToBurn))
        {
            Printf(FuseUtils::GetErrorPriority(), "Cannot burn fuse %s to 0x%x\n",
                fuseName.c_str(), valToBurn);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
        *pValToBurn = valToBurn;
    }
    else
    {
        Printf(FuseUtils::GetErrorPriority(), "Should never reach here... unknown fuse type\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC BasicSkuHandler::OverrideFuseVal(const string& fuseName, FuseOverride fuseOverride)
{
    if (m_Overrides.count(fuseName) != 0)
    {
        Printf(FuseUtils::GetErrorPriority(), "Duplicate override for %s!\n", fuseName.c_str());
        return RC::ILWALID_ARGUMENT;
    }
    m_Overrides[fuseName] = fuseOverride;
    return OK;
}

RC BasicSkuHandler::DoesSkuMatch
(
    const string& sku,
    bool* pDoesMatch,
    FuseFilter filter,
    Tee::Priority pri
)
{
    MASSERT(pDoesMatch);
    *pDoesMatch = true;
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    auto skuIter = m_pSkuList->find(sku);
    if (skuIter == m_pSkuList->end())
    {
        Printf(FuseUtils::GetErrorPriority(), "Cannot find SKU %s\n", sku.c_str());
        return RC::ILWALID_ARGUMENT;
    }

    const auto& skuConfig = skuIter->second;
    FuseValues fuseVals;

    CHECK_RC(m_pEncoder->GetFuseValues(&fuseVals, filter));

    for (const auto& fuseInfo : skuConfig.FuseList)
    {
        // Skip this fuse if it isn't supported given the filter
        if (!m_pEncoder->IsFuseSupported(*fuseInfo.pFuseDef, filter))
        {
            continue;
        }

        const string& fuseName = fuseInfo.pFuseDef->Name;

        // Skip this fuse if it is read protected
        if (std::find(m_ReadProtectedFuses.begin(), m_ReadProtectedFuses.end(), fuseName) !=
                      m_ReadProtectedFuses.end())
        {
            continue;
        }

        if (fuseVals.m_NamedValues.count(fuseName) == 0)
        {
            Printf(FuseUtils::GetErrorPriority(), "SKU %s specified an unknown fuse: %s\n",
                sku.c_str(), fuseName.c_str());
            return RC::SOFTWARE_ERROR;
        }
        UINT32 fuseVal = fuseVals.m_NamedValues.at(fuseName);

        string expectedStr;
        if (!IsFuseRight(fuseInfo, fuseVal, &expectedStr))
        {
            Printf(pri, "Chip does not match SKU %s\n", sku.c_str());
            Printf(pri, "  Fuse %s miscompared - %s\n", fuseName.c_str(), expectedStr.c_str());
            *pDoesMatch = false;
            break;
        }

    }

    // Don't bother looking at Floorsweeping or IFF if the named fuses miscompared
    if (!*pDoesMatch)
    {
        return rc;
    }

    // For the FS check, we don't care about customer visibility:
    // the FS fuses should always be customer visible, but the fuse file
    // doesn't always get updated promptly/correctly.
    fuseVals.Clear();
    CHECK_RC(m_pEncoder->GetFuseValues(&fuseVals, filter & ~FuseFilter::LWSTOMER_FUSES_ONLY));

    if ((filter & FuseFilter::SKIP_FS_FUSES) == 0)
    {
        CHECK_RC(IsFsRight(fuseVals, skuConfig, sku, pri, pDoesMatch));
    }
    else
    {
        Printf(Tee::PriWarn, "Skipping floorsweeping checks during SKU match\n");
    }

    // Don't bother looking at IFF if the named fuses miscompared.
    // Also skip IFF if we're only checking OPT fuses or customer fuses
    // as IFF records don't have OPT fuses nor are they customer visible.
    if (!*pDoesMatch ||
        (filter & (FuseFilter::OPT_FUSES_ONLY | FuseFilter::LWSTOMER_FUSES_ONLY)) != 0)
    {
        return rc;
    }

    CHECK_RC(IsIffRight(fuseVals, skuConfig, sku, pri, pDoesMatch));

    if (*pDoesMatch)
    {
        Printf(pri, "Chip matches SKU %s\n", sku.c_str());
    }

    return rc;
}

RC BasicSkuHandler::FindMatchingSkus(vector<string>* pMatches, FuseFilter filter)
{
    MASSERT(pMatches);
    pMatches->clear();
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    for (const auto& skuListPair : *m_pSkuList)
    {
        const string& skuName = skuListPair.first;
        bool doesMatch = false;
        CHECK_RC(DoesSkuMatch(skuName, &doesMatch, filter, FuseUtils::GetVerbosePriority()));
        if (doesMatch)
        {
            pMatches->push_back(skuName);
        }
    }
    return rc;
}

RC BasicSkuHandler::CheckAteFuses(const string& sku)
{
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    auto skuIter = m_pSkuList->find(sku);
    if (skuIter == m_pSkuList->end())
    {
        Printf(FuseUtils::GetErrorPriority(), "Cannot find SKU %s\n", sku.c_str());
        return RC::ILWALID_ARGUMENT;
    }

    const auto& skuConfig = skuIter->second;
    FuseValues fuseVals;
    FuseFilter filter = FuseFilter::ALL_FUSES;

    CHECK_RC(m_pEncoder->GetFuseValues(&fuseVals, filter));

    for (const auto& fuseInfo : skuConfig.FuseList)
    {
        // Skip this fuse if it isn't supported given the filter
        if (!m_pEncoder->IsFuseSupported(*fuseInfo.pFuseDef, filter))
        {
            continue;
        }

        // Only check read-only fuses since those are the ATE fuses
        if ((fuseInfo.Attribute & FuseUtil::FLAG_READONLY) == 0)
        {
            continue;
        }
        const string& fuseName = fuseInfo.pFuseDef->Name;

        if (fuseVals.m_NamedValues.count(fuseName) == 0)
        {
            Printf(FuseUtils::GetErrorPriority(), "SKU %s specified an unknown fuse: %s\n",
                sku.c_str(), fuseName.c_str());
            return RC::SOFTWARE_ERROR;
        }
        UINT32 fuseVal = fuseVals.m_NamedValues.at(fuseName);

        // Check for an override
        // ATE fuses can have overrides in special cases, in which case
        // the affected value will only be changed after the fusing step
        auto overrideIter = m_Overrides.find(fuseName);
        if (overrideIter != m_Overrides.end())
        {
            const FuseOverride& override = overrideIter->second;
            if (!override.useSkuDef)
            {
                if (fuseVal != override.value)
                {
                    Printf(FuseUtils::GetWarnPriority(),
                                "  Ignoring overridden ATE fuse miscompare - %s\n",
                                fuseName.c_str());
                }
                continue;
            }
        }

        string expectedStr;
        if (!IsFuseRight(fuseInfo, fuseVal, &expectedStr))
        {
            Printf(FuseUtils::GetErrorPriority(), "SKU %s has invalid ATE fuses!\n",
                sku.c_str());
            Printf(FuseUtils::GetErrorPriority(), "  Fuse %s miscompared - %s\n",
                fuseName.c_str(), expectedStr.c_str());
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }
    return RC::OK;
}

RC BasicSkuHandler::GetMismatchingFuses
(
    const string& chipSku,
    FuseFilter filter,
    FuseDiff* pRetInfo
)
{
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    FuseValues fuseVals;
    CHECK_RC(m_pEncoder->GetFuseValues(&fuseVals, filter));

    auto skuIter = m_pSkuList->find(chipSku);
    if (skuIter == m_pSkuList->end())
    {
        Printf(FuseUtils::GetErrorPriority(),
            "Chip sku: %s not found in fuse file\n", chipSku.c_str());
        return RC::ILWALID_ARGUMENT;
    }

    for (const auto& fuseInfo : skuIter->second.FuseList)
    {
        // Skip unsupported and pseudo fuses
        if (!m_pEncoder->IsFuseSupported(*fuseInfo.pFuseDef, filter) ||
            fuseInfo.pFuseDef->Type == FuseUtil::TYPE_PSEUDO)
        {
            continue;
        }

        const string& fuseName = fuseInfo.pFuseDef->Name;
        if (fuseVals.m_NamedValues.count(fuseName) == 0)
        {
            Printf(FuseUtils::GetErrorPriority(), "SKU %s specified an unknown fuse: %s\n",
                chipSku.c_str(), fuseName.c_str());
            return RC::SOFTWARE_ERROR;
        }

        FuseVals fuseData;
        fuseData.name = fuseName;
        fuseData.actualVal = fuseVals.m_NamedValues.at(fuseName);

        if (!IsFuseRight(fuseInfo, fuseData.actualVal, &fuseData.expectedStr))
        {
            pRetInfo->namedFuseMismatches.push_back(fuseData);
        }
    }

    bool match = true;
    // For the FS check, we don't care about customer visibility
    fuseVals.Clear();
    CHECK_RC(m_pEncoder->GetFuseValues(&fuseVals, filter & ~FuseFilter::LWSTOMER_FUSES_ONLY));

    CHECK_RC(IsFsRight(fuseVals, skuIter->second, chipSku,
        FuseUtils::GetVerbosePriority(), &match));

    pRetInfo->doesFsMismatch = !match;

    match = true;
    CHECK_RC(IsIffRight(fuseVals, skuIter->second, chipSku,
        FuseUtils::GetVerbosePriority(), &match));

    pRetInfo->doesIffMismatch = !match;
    return rc;
}

RC BasicSkuHandler::PrintMismatchingFuses
(
    const string& chipSku,
    FuseFilter filter
)
{
    RC rc;
    FuseDiff badFuses;
    CHECK_RC(GetMismatchingFuses(chipSku, filter, &badFuses));
    if (badFuses.Matches())
    {
        Printf(FuseUtils::GetVerbosePriority(), "Chip matches SKU %s\n", chipSku.c_str());
    }
    for (auto& badFuse : badFuses.namedFuseMismatches)
    {
        Printf(FuseUtils::GetPrintPriority(),
            "Fuse %s (0x%x) miscompares - %s\n",
            badFuse.name.c_str(), badFuse.actualVal, badFuse.expectedStr.c_str());
    }
    if (badFuses.doesFsMismatch)
    {
        Printf(FuseUtils::GetPrintPriority(), "Floorsweeping settings mismatch\n");
    }
    if (badFuses.doesIffMismatch)
    {
        Printf(FuseUtils::GetPrintPriority(), "IFF Records mismatch\n");
    }

    return rc;
}

RC BasicSkuHandler::GetFuseAttribute
(
    const string& sku,
    const string& fuse,
    INT32 *pAttribute
)
{
    MASSERT(pAttribute != nullptr);
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    auto skuIter = m_pSkuList->find(sku);
    if (skuIter == m_pSkuList->end())
    {
        Printf(FuseUtils::GetErrorPriority(), "Cannot find SKU %s!\n", sku.c_str());
        return RC::ILWALID_ARGUMENT;
    }

    bool bFuseFound = false;
    const FuseUtil::SkuConfig& skuConfig = skuIter->second;
    for (const auto& fuseInfo : skuConfig.FuseList)
    {
        const string& name = fuseInfo.pFuseDef->Name;
        if (name == fuse)
        {
            *pAttribute = fuseInfo.Attribute;
            bFuseFound = true;
            break;
        }
    }
    if (!bFuseFound)
    {
        Printf(FuseUtils::GetErrorPriority(), "Cannot find fuse %s!\n", fuse.c_str());
        return RC::ILWALID_ARGUMENT;
    }

    return rc;
}

bool BasicSkuHandler::IsFuseRight
(
    const FuseUtil::FuseInfo& fuseInfo,
    UINT32 fuseVal,
    string* pExpectedStr
)
{
    MASSERT(pExpectedStr != nullptr);
    pExpectedStr->clear();
    const string& fuseName = fuseInfo.pFuseDef->Name;

    Tee::Priority pri = (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK &&
                             FuseUtils::GetIgnoreAte()) ? Tee::PriLow :
                             FuseUtils::GetWarnPriority();
    if (fuseVal == 0xBADF510C)
    {
        Printf(pri, "Priv level violation. Ignoring fuse %s\n", fuseName.c_str());
        return true;
    }

    // Check for an override
    auto overrideIter = m_Overrides.find(fuseName);
    if (overrideIter != m_Overrides.end())
    {
        const FuseOverride& override = overrideIter->second;
        // Only use the override value directly if we
        // weren't told to use the SKU's definition
        if (!override.useSkuDef)
        {
            if (fuseVal != override.value)
            {
                *pExpectedStr =
                    Utility::StrPrintf("Expected override value: 0x%x\n", override.value);
                return false;
            }
            else
            {
                return true;
            }
        }
    }

    if ((FuseUtils::GetUseDummyFuses() || FuseUtils::GetIgnoreAte()) &&
        ((fuseInfo.Attribute & FuseUtil::FLAG_ATE_NOT)   != 0 ||
         (fuseInfo.Attribute & FuseUtil::FLAG_ATE_RANGE) != 0))
    {
        Printf(pri, "Ignoring ATE set fuse %s\n", fuseName.c_str());
        return true;
    }

    if ((fuseInfo.Attribute & FuseUtil::FLAG_DONTCARE) != 0)
    {
        return true;
    }
    else if ((fuseInfo.Attribute & FuseUtil::FLAG_MATCHVAL) != 0)
    {
        bool valid = false; 
        if (fuseInfo.pFuseDef->Type == "en/dis")
        {
            // Assuming a single Value for en/dis instead of a range or list of values
            // We need to modify the logic if this changes
            
            // If the MSB of fuseVal is set, fuse is already undone or in disabled state
            // All values 0b1xxx == 0b000
            if (!fuseVal)
            {
                
                valid = fuseInfo.Value == 0 ||
                        (fuseInfo.Value & (1 << fuseInfo.pFuseDef->NumBits));
            }
            else if (fuseVal & (1 << fuseInfo.pFuseDef->NumBits))
            {
                // if fuseInfo.Value has MSB set then fuseVal == 0  is an invalid case
                valid = (fuseInfo.Value & (1 << fuseInfo.pFuseDef->NumBits));
            }
            else
            {
                valid = fuseVal == fuseInfo.Value;
            }

            if (!valid)
            {
                *pExpectedStr = Utility::StrPrintf("Expected value: 0x%x", fuseInfo.Value);
                return false;
            }
        }
        // Invalid if value is not found
        else if (std::find(fuseInfo.Vals.begin(), fuseInfo.Vals.end(), fuseVal) ==
                fuseInfo.Vals.end())
        {
            *pExpectedStr = "Expected value(s): { ";
            for (auto& validVal: fuseInfo.Vals)
            {
                *pExpectedStr += Utility::StrPrintf("0x%x ", validVal);
            }
            *pExpectedStr += "}";
            return false;
        }
    }
    else if ((fuseInfo.Attribute & FuseUtil::FLAG_BITCOUNT) != 0)
    {
        UINT32 numBits = Utility::CountBits(fuseVal);
        if (numBits != fuseInfo.Value)
        {
            *pExpectedStr = Utility::StrPrintf("Expected burn count: %d", fuseInfo.Value);
            return false;
        }
    }
    else if ((fuseInfo.Attribute & FuseUtil::FLAG_ATE_NOT) != 0)
    {
        // Invalid if value is found
        if (std::find(fuseInfo.Vals.begin(), fuseInfo.Vals.end(), fuseVal) != fuseInfo.Vals.end())
        {
            *pExpectedStr = "Disallowed value(s): { ";
            for (auto& ilwalidVal: fuseInfo.Vals)
            {
                *pExpectedStr += Utility::StrPrintf("0x%x ", ilwalidVal);
            }
            *pExpectedStr += "}";
            return false;
        }
    }
    else if ((fuseInfo.Attribute & FuseUtil::FLAG_ATE_RANGE) != 0)
    {
        bool inRange = false;
        for (auto& range: fuseInfo.ValRanges)
        {
            if (range.Min <= fuseVal && fuseVal <= range.Max)
            {
                inRange = true;
                break;
            }
        }
        if (!inRange)
        {
            *pExpectedStr = "Expected fuse range(s): {";
            for (auto& range: fuseInfo.ValRanges)
            {
                if (range.Min == range.Max)
                {
                    *pExpectedStr +=
                        Utility::StrPrintf("0x%x ", range.Min);
                }
                else
                {
                    *pExpectedStr +=
                        Utility::StrPrintf("0x%x-0x%x ", range.Min, range.Max);
                }
            }
            *pExpectedStr += "}";

            return false;
        }
    }
    else if ((fuseInfo.Attribute & FuseUtil::FLAG_UNDOFUSE) != 0)
    {
        bool valid = false;
        if (fuseInfo.StrValue == "01" || fuseInfo.StrValue == "1")
        {
            valid = fuseVal == 1;
        }
        else if (fuseInfo.StrValue ==  "0" ||
                 fuseInfo.StrValue == "00" ||
                 fuseInfo.StrValue == "10" ||
                 fuseInfo.StrValue == "1x" ||
                 fuseInfo.StrValue == "11")
        {
            valid = fuseVal == 0 || fuseVal == 2 || fuseVal == 3;
        }

        if (!valid)
        {
            *pExpectedStr =
                Utility::StrPrintf("Expected Undo setting: %s", fuseInfo.StrValue.c_str());
            return false;
        }
    }
    return true;
}

RC BasicSkuHandler::IsFsRight
(
    const FuseValues& fuseVals,
    const FuseUtil::SkuConfig& skuConfig,
    const string& sku,
    Tee::Priority pri,
    bool* pIsRight
)
{
    RC rc;
    if (!m_pMiscInfo->perfectCounts.populated || skuConfig.fsSettings.empty())
    {
        return rc;
    }
    // Fuse Name, FsConfig Name, Perfect Unit Count
    vector<tuple<string, string, UINT32,  ModsGpuRegAddress>> fsFuseInfo =
    {
        make_tuple("OPT_GPC_DISABLE", "gpc", m_pMiscInfo->perfectCounts.gpcCount,
                MODS_FUSE_OPT_GPC_DISABLE),
        make_tuple("OPT_ROP_DISABLE", "rop", m_pMiscInfo->perfectCounts.ropCount,
                MODS_FUSE_OPT_ROP_DISABLE),
        make_tuple("OPT_FBP_DISABLE", "fbp", m_pMiscInfo->perfectCounts.fbpCount,
                 MODS_FUSE_OPT_FBP_DISABLE),
        make_tuple("OPT_FBPA_DISABLE", "fbpa", m_pMiscInfo->perfectCounts.fbpaCount,
                 MODS_FUSE_OPT_FBPA_DISABLE),
        make_tuple("OPT_FBIO_DISABLE", "fbpa", m_pMiscInfo->perfectCounts.fbpaCount,
                MODS_FUSE_OPT_FBIO_DISABLE),
        make_tuple("OPT_ROP_L2_DISABLE", "l2", m_pMiscInfo->perfectCounts.l2Count,
                MODS_FUSE_OPT_ROP_L2_DISABLE),
        make_tuple("OPT_LTC_DISABLE", "l2", m_pMiscInfo->perfectCounts.l2Count,
                MODS_FUSE_OPT_LTC_DISABLE)
    };

    // Add lwdec and lwjpg
    GpuSubdevice *pSubdev = dynamic_cast<GpuSubdevice*>(m_pDev);
    MASSERT(pSubdev != nullptr);
    UINT32 maxLwdecCount = pSubdev->GetMaxLwdecCount();
    UINT32 maxLwjpgCount = pSubdev->GetMaxLwjpgCount();
    if (maxLwdecCount)
    {
        fsFuseInfo.push_back(
            make_tuple("OPT_LWDEC_DISABLE", "lwdec", maxLwdecCount, MODS_FUSE_OPT_LWDEC_DISABLE)
        );
    }
    if (maxLwjpgCount)
    {
        fsFuseInfo.push_back(
            make_tuple("OPT_LWJPG_DISABLE", "lwjpg", maxLwjpgCount, MODS_FUSE_OPT_LWJPG_DISABLE)
        );
    }

    RegHal& regs = m_pDev->Regs();

    for (const auto& fsInfoTuple : fsFuseInfo)
    {
        const string& fuseName = get<0>(fsInfoTuple);
        const string& configName = get<1>(fsInfoTuple);
        const UINT32 perfectCount = get<2>(fsInfoTuple);
        const ModsGpuRegAddress fuseReg = get<3>(fsInfoTuple);

        if (!regs.IsSupported(fuseReg))
        {
            Printf(Tee::PriDebug, "BasicSkuHandler: Fuse %s was not found on the current sku\n",
                    fuseName.c_str());
            continue;
        }

        if (fuseVals.m_NamedValues.count(fuseName) == 0)
        {
            Printf(FuseUtils::GetErrorPriority(), "Cannot find fuse: %s\n", fuseName.c_str());
            return RC::SOFTWARE_ERROR;
        }

        UINT32 fuseValue = fuseVals.m_NamedValues.at(fuseName);
        UINT32 bitsEnabled = perfectCount - Utility::CountBits(fuseValue);
        const FuseUtil::FsConfig& fsConfig = skuConfig.fsSettings.at(configName);
        // Check unit count
        if (fsConfig.enableCount != FuseUtil::UNKNOWN_ENABLE_COUNT &&
            bitsEnabled != fsConfig.enableCount)
        {
            Printf(pri, "Enable count for %s does not match SKU %s: %u\n",
                         fuseName.c_str(), sku.c_str(), bitsEnabled);
            *pIsRight = false;
        }
        // Check enable list
        for (const auto &enableBit : fsConfig.mustEnableList)
        {
            if (((fuseValue >> enableBit) & 0x1) == 0x1)
            {
                Printf(pri, "%s%u disabled, but must be enabled for SKU %s\n",
                             configName.c_str(), enableBit, sku.c_str());
                *pIsRight = false;
            }
        }
        // Check disable list
        for (const auto &disableBit : fsConfig.mustDisableList)
        {
            if (((fuseValue >> disableBit) & 0x1) == 0x0)
            {
                Printf(pri, "%s%u enabled, but must be disabled for SKU %s\n",
                             configName.c_str(), disableBit, sku.c_str());
                *pIsRight = false;
            }
        }
    }

    // TPC requires special handling since it's split between several fuses
    // and potentially has reconfig fuses blown.

    const FuseUtil::FsConfig& tpcConfig = skuConfig.fsSettings.at("tpc");

    UINT32 disabledTpcs = 0;
    UINT32 reconfigTpcs = 0;
    vector<UINT32> disabledTpcList;
    vector<UINT32> reconfigTpcList;
    UINT32 tpcsPerGpc = m_pMiscInfo->perfectCounts.tpcsPerGpc;
    UINT32 gpcDisableMask = fuseVals.m_NamedValues.at("OPT_GPC_DISABLE");
    for (UINT32 gpc = 0; gpc < m_pMiscInfo->perfectCounts.gpcCount; gpc++)
    {
        string disableFuseName = "OPT_TPC_GPC" + to_string(gpc) + "_DISABLE";
        string reconfigFuseName = "OPT_TPC_GPC" + to_string(gpc) + "_RECONFIG";
        if (fuseVals.m_NamedValues.count(disableFuseName) == 0)
        {
            Printf(FuseUtils::GetErrorPriority(),
                "Cannot find fuse: %s\n", disableFuseName.c_str());
            return RC::SOFTWARE_ERROR;
        }

        UINT32 disabledTpcMask = fuseVals.m_NamedValues.at(disableFuseName);
        UINT32 disabledTpcsInGpc = Utility::CountBits(disabledTpcMask);
        disabledTpcs += disabledTpcsInGpc;

        // Count reconfig if available
        UINT32 reconfigTpcMask = 0;
        UINT32 reconfigTpcsInGpc = 0;
        if (fuseVals.m_NamedValues.count(reconfigFuseName) != 0)
        {
            reconfigTpcMask = fuseVals.m_NamedValues.at(reconfigFuseName);
            reconfigTpcsInGpc = Utility::CountBits(reconfigTpcMask);
            reconfigTpcs += reconfigTpcsInGpc;
        }

        // Check for min TPCs per GPC
        if (tpcConfig.minEnablePerGroup && ((gpcDisableMask & (1 << gpc)) == 0))
        {
            UINT32 numEnabledTpcsInGpc = tpcsPerGpc - disabledTpcsInGpc - reconfigTpcsInGpc;
            if (numEnabledTpcsInGpc < tpcConfig.minEnablePerGroup)
            {
                Printf(pri, "Only %u TPCs enabled in GPC%u. Min required for %s = %u\n",
                             numEnabledTpcsInGpc, gpc, sku.c_str(), tpcConfig.minEnablePerGroup);
                *pIsRight = false;
            }
        }

        for (UINT32 tpc = 0; tpc < tpcsPerGpc; tpc++)
        {
            UINT32 absTpc = (gpc * tpcsPerGpc) + tpc;
            if (((disabledTpcMask >> tpc) & 0x1) == 0x1)
            {
                disabledTpcList.push_back(absTpc);
            }
            if (((reconfigTpcMask >> tpc) & 0x1) == 0x1)
            {
                reconfigTpcList.push_back(absTpc);
            }
        }
    }

    // Check enable and disable lists
    //
    // NOTE - Reconfig TPCs can't be part of both enabled and disabled lists since they
    // may be either enabled or disabled based on VBIOS programming
    for (const auto &enableBit : tpcConfig.mustEnableList)
    {
        if ((std::find(disabledTpcList.begin(), disabledTpcList.end(), enableBit) != disabledTpcList.end()) ||
            (std::find(reconfigTpcList.begin(), reconfigTpcList.end(), enableBit) != reconfigTpcList.end()))
        {
            Printf(pri, "TPC%u must be enabled for SKU %s, but isn't\n",
                         enableBit, sku.c_str());
            *pIsRight = false;
        }
    }
    for (const auto &disableBit : tpcConfig.mustDisableList)
    {
        if ((std::find(disabledTpcList.begin(), disabledTpcList.end(), disableBit) == disabledTpcList.end()) ||
            (std::find(reconfigTpcList.begin(), reconfigTpcList.end(), disableBit) != reconfigTpcList.end()))
        {
            Printf(pri, "TPC%u must be disabled for SKU %s, but isn't\n",
                         disableBit, sku.c_str());
            *pIsRight = false;
        }
    }

    if (tpcConfig.enableCount == FuseUtil::UNKNOWN_ENABLE_COUNT &&
        !tpcConfig.hasReconfig)
    {
        // Number of TPCs hasn't been specified, end the check here
        return rc;
    }

    if (tpcConfig.hasReconfig)
    {
        if (tpcConfig.reconfigMinCount == 0 || tpcConfig.reconfigMaxCount == 0)
        {
            Printf(FuseUtils::GetErrorPriority(),
                "Reconfig counts need to be set for SKU %s\n", sku.c_str());
            return RC::SOFTWARE_ERROR;
        }
        if (tpcConfig.reconfigMaxCount < tpcConfig.reconfigMinCount)
        {
            Printf(FuseUtils::GetErrorPriority(),
                "Reconfig Max is less than Min for SKU %s\n", sku.c_str());
            return RC::SOFTWARE_ERROR;
        }
        if (tpcConfig.enableCount != FuseUtil::UNKNOWN_ENABLE_COUNT &&
            tpcConfig.enableCount != tpcConfig.reconfigMaxCount)
        {
            Printf(FuseUtils::GetErrorPriority(),
                "Static TPC count can't be used with reconfig... SKU %s\n", sku.c_str());
            return RC::SOFTWARE_ERROR;
        }
        if (tpcConfig.repairMaxCount < tpcConfig.repairMinCount)
        {
            Printf(FuseUtils::GetErrorPriority(),
                "Repair Max is less than Min for SKU %s\n", sku.c_str());
            return RC::SOFTWARE_ERROR;
        }
        if ((tpcConfig.reconfigMaxCount + tpcConfig.repairMaxCount) >
             m_pMiscInfo->perfectCounts.tpcCount)
        {
            Printf(FuseUtils::GetErrorPriority(),
                "Reconfig + Repair max is greater than perfect TPC count for SKU %s\n", sku.c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    const UINT32 skuNumReconfigTpcs = tpcConfig.reconfigMaxCount - tpcConfig.reconfigMinCount;
    const UINT32 skuMaxReconfigTpcs = skuNumReconfigTpcs + tpcConfig.repairMaxCount;
    UINT32 skuMinReconfigTpcs = skuNumReconfigTpcs + tpcConfig.repairMinCount;
    if (m_UseTestValues)
    {
        skuMinReconfigTpcs = 0;
    }

    UINT32 skuMinDisableTpcs  = m_pMiscInfo->perfectCounts.tpcCount - tpcConfig.enableCount;
    UINT32 skuMaxDisableTpcs  = skuMinDisableTpcs;
    if (tpcConfig.hasReconfig)
    {
        skuMinDisableTpcs = m_pMiscInfo->perfectCounts.tpcCount - tpcConfig.reconfigMaxCount
                            - tpcConfig.repairMaxCount;
        skuMaxDisableTpcs = m_pMiscInfo->perfectCounts.tpcCount - tpcConfig.reconfigMaxCount
                            - tpcConfig.repairMinCount;
    }

    // The TPC enable count is now split up between the DISABLE and RECONFIG fuses
    // We thus have to make sure that -
    // Number of Disabled TPCs : [PERFECT - MAX - REPAIR_MAX, PERFECT - MAX - REPAIR_MIN]
    // Number of Reconfig TPCs : [MAX - MIN + REPAIR_MIN , MAX - MIN + REPAIR_MAX],
    // where [MIN,MAX] is pure reconfig TPC count
    bool isTpcConfigRight = true;
    if (disabledTpcs > skuMaxDisableTpcs || disabledTpcs < skuMinDisableTpcs)
    {
        Printf(pri, "Num of disabled TPCs %u doesn't fall within [%u, %u] for SKU %s\n",
                     disabledTpcs, skuMinDisableTpcs, skuMaxDisableTpcs, sku.c_str());
        isTpcConfigRight = false;
    }
    if (tpcConfig.hasReconfig &&
        (reconfigTpcs > skuMaxReconfigTpcs || reconfigTpcs < skuMinReconfigTpcs))
    {
        Printf(pri, "Num of reconfig TPCs %u doesn't fall within [%u, %u] for SKU %s\n",
                     reconfigTpcs, skuMinReconfigTpcs, skuMaxReconfigTpcs, sku.c_str());
        isTpcConfigRight = false;
    }

    if (!isTpcConfigRight)
    {
        if (tpcConfig.hasReconfig && !m_CheckReconfigFuses)
        {
            GpuSubdevice *pSubdev = dynamic_cast<GpuSubdevice*>(m_pDev);
            MASSERT(pSubdev != nullptr);
            UINT32 lwrTpcs = pSubdev->GetTpcCount();
            if (lwrTpcs == tpcConfig.reconfigLwrCount)
            {
                Printf(FuseUtils::GetWarnPriority(), "Ignoring reconfig fuses mismatch\n");
            }
            else
            {
                *pIsRight = false;
            }
        }
        else
        {
            *pIsRight = false;
        }
    }

    // L2 slice requires special handling since it's split between several fuses as well
    if (regs.IsSupported(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, 0))
    {
        UINT32 disabledSlices = 0;
        for (UINT32 i = 0; i < m_pMiscInfo->perfectCounts.fbpCount; i++)
        {
            string disableFuseName = "OPT_L2SLICE_FBP" + to_string(i) + "_DISABLE";
            if (fuseVals.m_NamedValues.count(disableFuseName) == 0)
            {
                Printf(FuseUtils::GetErrorPriority(), "Cannot find fuse: %s\n",
                                                       disableFuseName.c_str());
                return RC::SOFTWARE_ERROR;
            }
            disabledSlices += Utility::CountBits(fuseVals.m_NamedValues.at(disableFuseName));
        }
        UINT32 bitsEnabled = m_pMiscInfo->perfectCounts.l2SliceCount - disabledSlices;
        const FuseUtil::FsConfig& fsConfig = skuConfig.fsSettings.at("l2_slice");
        if (fsConfig.enableCount != FuseUtil::UNKNOWN_ENABLE_COUNT &&
            bitsEnabled != fsConfig.enableCount)
        {
            Printf(pri, "Enable count for l2 slices does not match SKU %s: %u\n",
                         sku.c_str(), bitsEnabled);
            *pIsRight = false;
        }
    }

    return rc;
}

RC BasicSkuHandler::IsIffRight
(
    const FuseValues fuseVals,
    const FuseUtil::SkuConfig& skuConfig,
    const string& sku,
    Tee::Priority pri,
    bool* pDoesMatch
)
{
    RC rc;

    const vector<shared_ptr<IffRecord>>& chipRecords = fuseVals.m_IffRecords;
    vector<shared_ptr<IffRecord>> skuRecords;
    CHECK_RC(GetSkuIffRecords(skuConfig, &skuRecords));
    CHECK_RC(m_pEncoder->ValidateIffRecords(chipRecords, skuRecords, pDoesMatch));
    if (!(*pDoesMatch))
    {
        Printf(pri, "IFF miscompares for SKU %s\n", sku.c_str());
    }
    return rc;
}

void BasicSkuHandler::UseTestValues()
{
    m_UseTestValues = true;
}

void BasicSkuHandler::CheckReconfigFuses(bool check)
{
    m_CheckReconfigFuses = check;
}

bool BasicSkuHandler::IsReconfigFusesCheckEnabled()
{
    return m_CheckReconfigFuses;
}

// Pull information from the individual FS fuses and from the
// Floorsweeping_options section (if present) and merge it into
// a single format for use by the down-bin script.
// This function has been copied from FuseUtil::GetFsInfo which
// is for legacy support
// This has some modifications for filtering fuses wrt gpu and
// removing pseudo_tpc_disable.
RC BasicSkuHandler::GetFsInfo
(
    const string &fileName,
    const string &chipSku,
    map<string, FuseUtil::DownbinConfig>* pConfigs
)
{
    MASSERT(pConfigs != nullptr);
    map<string, FuseUtil::DownbinConfig>& downbinConfigs = *pConfigs;

    RC rc;
    const FuseDefMap *pFuseDefMap;
    const SkuList    *pSkuList;
    const MiscInfo   *pMiscInfo;

    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fileName));
    CHECK_RC(pParser->ParseFile(fileName, &pFuseDefMap, &pSkuList, &pMiscInfo));
    const FloorsweepingUnits& perfectCounts = pMiscInfo->perfectCounts;
    auto skuIter = pSkuList->find(chipSku);
    if (skuIter == pSkuList->end())
    {
        Printf(Tee::PriError, "Chip SKU %s not found\n", chipSku.c_str());
        return RC::BAD_PARAMETER;
    }
    const SkuConfig& skuConfig = skuIter->second;

    // Try to use data from the Floorsweeping_options section first
    if (perfectCounts.populated && !skuConfig.fsSettings.empty())
    {
        GpuSubdevice *pSubdev = dynamic_cast<GpuSubdevice*>(m_pDev);
        MASSERT(pSubdev != nullptr);
        UINT32 maxLwdecCount = pSubdev->GetMaxLwdecCount();
        UINT32 maxLwjpgCount = pSubdev->GetMaxLwjpgCount();
        const map<string, UINT32> fsUnitToMaxCount =
        {
            { "gpc",      perfectCounts.gpcCount },
            { "tpc",      perfectCounts.tpcCount },
            { "rop",      perfectCounts.ropCount },
            { "fbp",      perfectCounts.fbpCount },
            { "fbpa",     perfectCounts.fbpaCount },
            { "l2",       perfectCounts.l2Count },
            { "l2_slice", perfectCounts.l2SliceCount },
            { "lwdec",    maxLwdecCount },
            { "lwjpg",    maxLwjpgCount }
        };

        // Each unit specifies a disable count along with a list of any bits
        // that should or should not be disabled.
        for (auto& fsUnitMaxCountPair : fsUnitToMaxCount)
        {
            const UINT32 maxCount = fsUnitMaxCountPair.second;
            const string& unit = fsUnitMaxCountPair.first;
            const FsConfig& unitConfig = skuConfig.fsSettings.at(unit);
            FuseUtil::DownbinConfig& downbinConfig = downbinConfigs[unit];

            INT32 disableCount = -1;
            if (unitConfig.enableCount != UNKNOWN_ENABLE_COUNT)
            {
                if (maxCount)
                {
                    disableCount = static_cast<INT32>(maxCount - unitConfig.enableCount);
                }
                downbinConfig.enableCount = unitConfig.enableCount;
            }
            downbinConfig.disableCount      = disableCount;
            downbinConfig.mustDisableList   = unitConfig.mustDisableList;
            downbinConfig.mustEnableList    = unitConfig.mustEnableList;
            downbinConfig.minEnablePerGroup = unitConfig.minEnablePerGroup;
            downbinConfig.enableCountPerGroup = unitConfig.enableCountPerGroup;
            downbinConfig.maxUnitUgpuImbalance = unitConfig.maxUnitUgpuImbalance;
        }
        // TPCs have some extra data
        if (skuConfig.fsSettings.count("tpc") != 0)
        {
            const FsConfig& tpcConfig = skuConfig.fsSettings.at("tpc");
            FuseUtil::DownbinConfig& tpcDownbin = downbinConfigs["tpc"];
            if (tpcConfig.minEnablePerGroup == 0)
            {
                tpcDownbin.maxBurnPerGroup = -1;
            }
            else
            {
                tpcDownbin.maxBurnPerGroup = perfectCounts.tpcsPerGpc -
                    tpcConfig.minEnablePerGroup;
            }

            tpcDownbin.hasReconfig = tpcConfig.hasReconfig;
            if (tpcConfig.hasReconfig)
            {
                // If we're using TPC reconfig, we need both Min and Max defined
                if (tpcConfig.reconfigMinCount == 0 || tpcConfig.reconfigMaxCount == 0)
                {
                    Printf(Tee::PriError,
                        "Reconfig values for TPC must be defined and non-zero for SKU %s!\n",
                        chipSku.c_str());
                    return RC::SOFTWARE_ERROR;
                }
                // Max must not be less than Min
                if (tpcConfig.reconfigMaxCount < tpcConfig.reconfigMinCount)
                {
                    Printf(Tee::PriError,
                        "TPC Reconfig: Max Count < Min Count for SKU %s!\n", chipSku.c_str());
                    return RC::SOFTWARE_ERROR;
                }
                // The static TPC count should not be specified if Reconfig
                // is used; if it is specified, throw an error, unless it
                // matches reconfigMaxCount, in which case we'll exlwse it.
                if (tpcConfig.enableCount != UNKNOWN_ENABLE_COUNT &&
                    tpcConfig.enableCount != tpcConfig.reconfigMaxCount)
                {
                    Printf(Tee::PriError,
                        "TPC Reconfig specified, but static value is also provided for SKU %s\n",
                        chipSku.c_str());
                    return RC::SOFTWARE_ERROR;
                }
                tpcDownbin.disableCount = perfectCounts.tpcCount - tpcConfig.reconfigMaxCount;
                tpcDownbin.reconfigCount = perfectCounts.tpcCount - tpcConfig.reconfigMinCount;

                if (tpcConfig.repairMaxCount < tpcConfig.repairMinCount)
                {
                    Printf(Tee::PriError,
                        "TPC Repair: Max Count < Min Count for SKU %s!\n", chipSku.c_str());
                    return RC::SOFTWARE_ERROR;
                }
                tpcDownbin.repairMinCount = tpcConfig.repairMinCount;
                tpcDownbin.repairMaxCount = tpcConfig.repairMaxCount;
            }

            tpcDownbin.minGraphicsTpcs = tpcConfig.minGraphicsTpcs;

            // Check vGpcSkylineList. It must be a sorted list of TPC count
            if (!std::is_sorted(tpcConfig.vGpcSkylineList.begin(), tpcConfig.vGpcSkylineList.end()))
            {
                Printf(Tee::PriError, "vGpcSkylineList is not a sorted List\n");
                return RC::SOFTWARE_ERROR;
            }
            tpcDownbin.vGpcSkylineList = tpcConfig.vGpcSkylineList;
        }

        // FBP can have HBM related info
        if (skuConfig.fsSettings.count("fbp") != 0)
        {
            const FsConfig& fbpConfig = skuConfig.fsSettings.at("fbp");
            FuseUtil::DownbinConfig& fbpDownbin = downbinConfigs["fbp"];
            fbpDownbin.fullSitesOnly = fbpConfig.fullSitesOnly;
        }

        // PES/CPC can have MinEnablePerGroup
        FuseUtil::DownbinConfig& pesDownbin = downbinConfigs["pes"];
        pesDownbin.minEnablePerGroup = skuConfig.fsSettings.at("pes").minEnablePerGroup;
        
        FuseUtil::DownbinConfig& cpcDownbin = downbinConfigs["cpc"];
        cpcDownbin.minEnablePerGroup = skuConfig.fsSettings.at("cpc").minEnablePerGroup;

        // SysPipe can also have MinEnable
        FuseUtil::DownbinConfig& syspipeDownbin    = downbinConfigs["sys_pipe"];
        syspipeDownbin.minEnablePerGroup = skuConfig.fsSettings.at("sys_pipe").minEnablePerGroup;

        // Ioctrl can also have MinEnable
        FuseUtil::DownbinConfig& ioctrlDownbin    = downbinConfigs["ioctrl"];
        ioctrlDownbin.minEnablePerGroup = skuConfig.fsSettings.at("ioctrl").minEnablePerGroup;

        // Perlink can also have MinEnablePerGroup
        FuseUtil::DownbinConfig& perLinkDownbin   = downbinConfigs["perlink"];
        perLinkDownbin.minEnablePerGroup = skuConfig.fsSettings.at("perlink").minEnablePerGroup;
    }
    // If the Floorsweeping_options section wasn't populated or didn't contain this
    // SKU, default back to the opt_*_disable registers for floorsweeping settings
    else
    {
        const multimap<string, pair <string, ModsGpuRegAddress>> fsUnitToFuseName =
        {
            { "gpc",   make_pair("opt_gpc_disable", MODS_FUSE_OPT_GPC_DISABLE) },
            { "rop",   make_pair("opt_rop_disable", MODS_FUSE_OPT_ROP_DISABLE) },
            { "fbp",   make_pair("opt_fbp_disable", MODS_FUSE_OPT_FBP_DISABLE) },
            { "fbpa",  make_pair("opt_fbpa_disable", MODS_FUSE_OPT_FBPA_DISABLE) },
            { "fbpa",  make_pair("opt_fbio_disable", MODS_FUSE_OPT_FBIO_DISABLE) },
            { "l2",    make_pair("opt_rop_l2_disable", MODS_FUSE_OPT_ROP_L2_DISABLE) },
            { "l2",    make_pair("opt_ltc_disable",  MODS_FUSE_OPT_LTC_DISABLE) },
            { "lwdec", make_pair("opt_lwdec_disable",  MODS_FUSE_OPT_LWDEC_DISABLE) },
            { "lwjpg", make_pair("opt_lwjpg_disable",  MODS_FUSE_OPT_LWJPG_DISABLE) },
            { "sys_pipe", make_pair("opt_sys_pipe_disable",  MODS_FUSE_OPT_SYS_PIPE_DISABLE) }
        };

        RegHal& regs = m_pDev->Regs();
        for (const auto& unitNamePair : fsUnitToFuseName)
        {
            const string& unit = unitNamePair.first;
            const string& fuseName = unitNamePair.second.first;
            const ModsGpuRegAddress fuseReg = unitNamePair.second.second;
            if (!regs.IsSupported(fuseReg))
            {
                Printf(Tee::PriDebug, "NewFuse: Fuse %s was not found on the current sku\n",
                        fuseName.c_str());
                continue;
            }

            const FuseInfo* pUnitInfo = nullptr;
            FuseUtil::DownbinConfig& downbinConfig = downbinConfigs[unit];
            if (!FindFuseInfo(fuseName, skuConfig, &pUnitInfo))
            {
                Printf(Tee::PriError, "Fuse %s not found\n", fuseName.c_str());
                return RC::BAD_PARAMETER;
            }
            // Fuse must have value X
            if ((pUnitInfo->Attribute & FLAG_MATCHVAL) != 0)
            {
                downbinConfig.isBurnCount = false;
                downbinConfig.matchValues = pUnitInfo->Vals;
            }
            // Burn X bits (optional: don't burn certain bits)
            else if ((pUnitInfo->Attribute & FLAG_BITCOUNT) != 0)
            {
                downbinConfig.isBurnCount = true;
                downbinConfig.disableCount = static_cast<INT32>(pUnitInfo->Value);
                downbinConfig.mustEnableList = pUnitInfo->protectedBits;
            }
            // Don't care: burn whatever is necessary, leave the rest alone
            else if ((pUnitInfo->Attribute & FLAG_DONTCARE) != 0)
            {
                downbinConfig.isBurnCount = true;
                downbinConfig.disableCount = -1;
            }
            else
            {
                Printf(Tee::PriError, "Invalid attribute for %s Floorsweeping: %d\n",
                       unit.c_str(), pUnitInfo->Attribute);
                return RC::BAD_PARAMETER;
            }
        }

        // TPC is special since it has different representations for different chips
        // and because it has extra data like MaxBurnPerGroup
        const FuseInfo* pTpcInfo = nullptr;
        FuseUtil::DownbinConfig& tpcConfig = downbinConfigs["tpc"];
        // If opt_tpc_disable exists, we're on an older chip and,
        // we only need to look at the settings for opt_tpc_disable
        if (FindFuseInfo("opt_tpc_disable", skuConfig, &pTpcInfo))
        {
            if ((pTpcInfo->Attribute & FLAG_MATCHVAL) != 0)
            {
                tpcConfig.isBurnCount = false;
                tpcConfig.matchValues = pTpcInfo->Vals;
            }
            else if ((pTpcInfo->Attribute & FLAG_BITCOUNT) != 0)
            {
                tpcConfig.isBurnCount = true;
                tpcConfig.disableCount = static_cast<INT32>(pTpcInfo->Value);
                tpcConfig.mustEnableList = pTpcInfo->protectedBits;
                tpcConfig.maxBurnPerGroup = pTpcInfo->maxBurnPerGroup;
            }
            else if ((pTpcInfo->Attribute & FLAG_DONTCARE) != 0)
            {
                tpcConfig.isBurnCount = true;
                tpcConfig.disableCount = -1;
            }
            else
            {
                Printf(Tee::PriError, "Invalid attribute for TPC Floorsweeping: %d\n",
                       pTpcInfo->Attribute);
                return RC::BAD_PARAMETER;
            }
        }
        else if (FindFuseInfo("opt_tpc_gpc0_disable", skuConfig, &pTpcInfo))
        {
            // We can assume that if one opt_tpc_gpcX_disable is ValMatchHex
            // then all opt_tpc_gpcX_disable masks will be ValMatchHex
            if ((pTpcInfo->Attribute & FLAG_MATCHVAL) != 0)
            {
                tpcConfig.isBurnCount = false;
                tpcConfig.matchValues.push_back(pTpcInfo->Value);
                for (UINT32 i = 1; i < 32; i++)
                {
                    string fuseName = "opt_tpc_gpc" + Utility::ToString(i) + "_disable";
                    if (!FindFuseInfo(fuseName, skuConfig, &pTpcInfo))
                    {
                        break;
                    }
                    tpcConfig.matchValues.push_back(pTpcInfo->Value);
                }
            }
            else if ((pTpcInfo->Attribute & FLAG_BITCOUNT) != 0)
            {
                tpcConfig.disableCount = static_cast<INT32>(pTpcInfo->Value);
                tpcConfig.mustEnableList = pTpcInfo->protectedBits;
                tpcConfig.maxBurnPerGroup = pTpcInfo->maxBurnPerGroup;
            }
            else if ((pTpcInfo->Attribute & FLAG_DONTCARE) != 0)
            {
                tpcConfig.disableCount = -1;
            }
            else
            {
                Printf(Tee::PriError, "Invalid TPC fuse options\n");
                return RC::BAD_PARAMETER;
            }
        }
        else
        {
            Printf(Tee::PriError, "Cannot find fuse information for TPC\n");
            return RC::BAD_PARAMETER;
        }

        const FuseInfo* pL2SliceInfo = nullptr;
        FuseUtil::DownbinConfig& l2SliceConfig = downbinConfigs["l2_slice"];
        // L2 slice also needs to be handled specially since it is on a per FBP basis
        if (FindFuseInfo("opt_l2slice_fbp0_disable", skuConfig, &pL2SliceInfo))
        {
            // We can assume that if one opt_l2slice_fbpX_disable is ValMatchHex
            // then all opt_l2slice_fbpX_disable masks will be ValMatchHex
            if ((pL2SliceInfo->Attribute & FLAG_MATCHVAL) != 0)
            {
                l2SliceConfig.isBurnCount = false;
                l2SliceConfig.matchValues.push_back(pL2SliceInfo->Value);
                for (UINT32 i = 1; i < 32; i++)
                {
                    string fuseName = "opt_l2slice_fbp" + Utility::ToString(i) + "_disable";
                    if (!FindFuseInfo(fuseName, skuConfig, &pL2SliceInfo))
                    {
                        break;
                    }
                    l2SliceConfig.matchValues.push_back(pL2SliceInfo->Value);
                }
            }
            else if ((pL2SliceInfo->Attribute & FLAG_BITCOUNT) != 0)
            {
                l2SliceConfig.disableCount    = static_cast<INT32>(pL2SliceInfo->Value);
                l2SliceConfig.mustEnableList  = pL2SliceInfo->protectedBits;
                l2SliceConfig.maxBurnPerGroup = pL2SliceInfo->maxBurnPerGroup;
            }
            else if ((pL2SliceInfo->Attribute & FLAG_DONTCARE) != 0)
            {
                l2SliceConfig.disableCount = -1;
            }
            else
            {
                Printf(Tee::PriError, "Invalid L2 slice fuse options\n");
                return RC::BAD_PARAMETER;
            }
        }
    }

    return rc;
}

RC BasicSkuHandler::DumpFsInfo(const string &chipSku)
{
    RC rc;
    unique_ptr<JsonItem> jsi = make_unique<JsonItem>();
    jsi->SetTag("DumpFsInfo");
    map<string, FuseUtil::DownbinConfig> fuseConfig;
    CHECK_RC(GetFuseConfig(chipSku, &fuseConfig));
    RegHal &regs = m_pDev->Regs();

    shared_ptr<JsonItem> pGpcJsi = make_shared<JsonItem>();
    CHECK_RC(ConfigToJson(fuseConfig.count("gpc") == 0 ? nullptr : &fuseConfig.at("gpc"),
                pGpcJsi));
    jsi->SetField("GPC_CONFIG", pGpcJsi.get());

    shared_ptr<JsonItem> pPesJsi = make_shared<JsonItem>();
    CHECK_RC(ConfigToJson(fuseConfig.count("pes") == 0 ? nullptr : &fuseConfig.at("pes"),
                pPesJsi));
    jsi->SetField("PES_CONFIG", pPesJsi.get());

    shared_ptr<JsonItem> pTpcJsi = make_shared<JsonItem>();
    CHECK_RC(ConfigToJson(fuseConfig.count("tpc") == 0 ? nullptr : &fuseConfig.at("tpc"),
                pTpcJsi));
    jsi->SetField("TPC_CONFIG", pTpcJsi.get());

    shared_ptr<JsonItem> pFbpJsi = make_shared<JsonItem>();
    CHECK_RC(ConfigToJson(fuseConfig.count("fbp") == 0 ? nullptr : &fuseConfig.at("fbp"),
                pFbpJsi));
    jsi->SetField("FBP_CONFIG", pFbpJsi.get());

    // NOTE: For GA10X onwards we don't have FBPA DISABLE fuse. We use FBIO and FBPA interchangeably
    // with 1-1 mapping. fsUnitToMaxCount doesn't list fbio, so here we read fbpa for FBIO_CONFIG
    shared_ptr<JsonItem> pFbioJsi = make_shared<JsonItem>();
    CHECK_RC(ConfigToJson(fuseConfig.count("fbpa") == 0 ? nullptr : &fuseConfig.at("fbpa"),
                pFbioJsi));
    jsi->SetField("FBIO_CONFIG", pFbioJsi.get());

    // If FBPA disable is supported
    if (regs.IsSupported(MODS_FUSE_OPT_FBPA_DISABLE))
    {
        shared_ptr<JsonItem> pFbpaJsi = make_shared<JsonItem>();
        CHECK_RC(ConfigToJson(fuseConfig.count("fbpa") == 0 ? nullptr : &fuseConfig.at("fbpa"),
                pFbpaJsi));
        jsi->SetField("FBPA_CONFIG", pFbpaJsi.get());
    }

    // If ROP in GPC
    if (regs.IsSupported(MODS_FUSE_OPT_LTC_DISABLE))
    {
        shared_ptr<JsonItem> pLtcJsi = make_shared<JsonItem>();
        CHECK_RC(ConfigToJson(fuseConfig.count("l2") == 0 ? nullptr : &fuseConfig.at("l2"),
                pLtcJsi));
        jsi->SetField("LTC_CONFIG", pLtcJsi.get());
    }
    else
    {
        shared_ptr<JsonItem> pRopL2Jsi = make_shared<JsonItem>();
        CHECK_RC(ConfigToJson(fuseConfig.count("rop_l2") == 0 ? nullptr : &fuseConfig.at("rop_l2"),
                pRopL2Jsi));
        jsi->SetField("ROP_L2_CONFIG", pRopL2Jsi.get());
    }

    if (regs.IsSupported(MODS_FUSE_OPT_ROP_DISABLE))
    {
        shared_ptr<JsonItem> pRopJsi = make_shared<JsonItem>();
        CHECK_RC(ConfigToJson(fuseConfig.count("rop") == 0 ? nullptr : &fuseConfig.at("rop"),
                pRopJsi));
        jsi->SetField("ROP_CONFIG", pRopJsi.get());
    }

    // If L2Slice disable is supported
    if (regs.IsSupported(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, 0))
    {
        shared_ptr<JsonItem> pL2SliceJsi = make_shared<JsonItem>();
        CHECK_RC(ConfigToJson(fuseConfig.count("l2_slice") == 0 ? nullptr : &fuseConfig.at("l2_slice"),
                pL2SliceJsi));
        jsi->SetField("L2SLICE_CONFIG", pL2SliceJsi.get());
    }

    // If CPC disable is supported
    if (regs.IsSupported(MODS_FUSE_OPT_CPC_DISABLE))
    {
        shared_ptr<JsonItem> pCpcJsi = make_shared<JsonItem>();
        CHECK_RC(ConfigToJson(fuseConfig.count("cpc") == 0 ? nullptr : &fuseConfig.at("cpc"),
                pCpcJsi));
        jsi->SetField("CPC_CONFIG", pCpcJsi.get());
    }
    CHECK_RC(jsi->WriteToLogfile());
    return RC::OK;
}

RC BasicSkuHandler::GetFuseConfig(const string &chipSku, map<string, DownbinConfig>* pFuseConfig)
{
    MASSERT(pFuseConfig);
    RC rc;
    CHECK_RC(GetFsInfo(FuseUtils::GetFuseFilename(m_pDev->GetDeviceId()),
                chipSku, pFuseConfig));
    map<string, DownbinConfig>& fuseConfig = *pFuseConfig;
    fuseConfig["rop_l2"] = fuseConfig["l2"];
    return rc;
}

RC BasicSkuHandler::ConfigToJson(const DownbinConfig *pConfig, shared_ptr<JsonItem> pJsonItem)
{
    string attr;
    UINT32 val = 0;
    if (pConfig == nullptr)
    {
        attr = "DONTCARE";
        val = 0;
    }
    else if (!pConfig->isBurnCount)
    {
        attr = "MATCHVAL";
        // Merge all the possible disabled bits
        // so floorsweep.egg checks all of them
        for (UINT32 i = 0; i < pConfig->matchValues.size(); i++)
        {
            val |= pConfig->matchValues[i];
        }
    }
    else
    {
        if (pConfig->disableCount >= 0)
        {
            val = pConfig->disableCount;
            attr = "BITCOUNT";
        }
        else
        {
            attr = "DONTCARE";
        }
    }
    pJsonItem->SetField("ATTRIBUTE", attr.c_str());
    pJsonItem->SetField("VAL", val);
    return RC::OK;
}

void BasicSkuHandler::AddSwFusingIgnoreFuse(const string &fuseName)
{
    m_SwFusingIgnoredFuses.push_back(fuseName);
}

void BasicSkuHandler::AddReadProtectedFuse(const string& fuseName)
{
    string upperName = Utility::ToUpperCase(fuseName);
    m_ReadProtectedFuses.push_back(upperName);
}
