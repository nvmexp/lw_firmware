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

#include "gpu/fuse/encoding/splitmacroencoder.h"

#include "core/include/utility.h"
#include "gpu/fuse/fuseutils.h"

SplitMacroEncoder::SplitMacroEncoder
(
    TestDevice *pDev,
    const map<FuseMacroType, shared_ptr<FuseHwAccessor>>& hwAccessors,
    unique_ptr<FuselessFuseEncoder>&& pFFEncoder,
    unique_ptr<IffEncoder>&& pIffEncoder,
    unique_ptr<ColumnFuseEncoder>&& pFpfColEncoder,
    unique_ptr<RirEncoder>&& pRirEncoder
)
    : m_pDev(pDev)
    , m_HwAccessors(hwAccessors)
    , m_pFuselessFuseEncoder(move(pFFEncoder))
    , m_pIffEncoder(move(pIffEncoder))
    , m_pFpfColEncoder(move(pFpfColEncoder))
    , m_pRirEncoder(move(pRirEncoder))
{
    MASSERT(m_pDev != nullptr);
    MASSERT(m_pFuselessFuseEncoder != nullptr);
    m_DevId = m_pDev->GetDeviceId();
    m_SubEncoders[FuseMacroType::Fuse].push_back(&m_ColumnFuseEncoder);
    m_SubEncoders[FuseMacroType::Fuse].push_back(m_pFuselessFuseEncoder.get());
    if (m_pIffEncoder.get() != nullptr)
    {
        m_HasIff = true;
        m_SubEncoders[FuseMacroType::Fuse].push_back(m_pIffEncoder.get());
    }
    if (m_pFpfColEncoder.get() != nullptr)
    {
        m_HasFpf = true;
        m_SubEncoders[FuseMacroType::Fpf].push_back(m_pFpfColEncoder.get());
    }
    if (m_pRirEncoder.get() != nullptr)
    {
        m_HasRir = true;
        m_SubEncoders[FuseMacroType::Rir].push_back(m_pRirEncoder.get());
    }
}

RC SplitMacroEncoder::RemoveUnsupportedFuses
(
    FuseValues *pFuseRequest
)
{
    RC rc;
    MASSERT(pFuseRequest);

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    vector<string> fusesToRemove;
    for (const auto& nameValPair : pFuseRequest->m_NamedValues)
    {
        const string& name = nameValPair.first;
        if (m_FuseDefs.count(name) == 0)
        {
            fusesToRemove.push_back(name);
        }
    }

    for (const auto& fuse : fusesToRemove)
    {
        Printf(FuseUtils::GetWarnPriority(), "Removing unknown fuse: %s\n", fuse.c_str());
        pFuseRequest->m_NamedValues.erase(fuse);
    }

    return rc;
}

RC SplitMacroEncoder::EncodeFuseValues
(
    const FuseValues& fuseRequest,
    FuseBinaryMap* pFuseBinary
)
{
    MASSERT(pFuseBinary);
    pFuseBinary->clear();
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    // Create a copy of the fuse request so the sub-encoders
    // can remove fuse values as they are encoded
    FuseValues fusesToBurn = fuseRequest;

    // Get a copy of the current fuse values so the
    // sub-encoders start from the current fusing
    const FuseValues* pLwrrentValues = nullptr;
    CHECK_RC(GetRawFuseValues(&pLwrrentValues));

    // Grab the FUSE macro and encode a new binary
    const vector<UINT32>* pOldFuses = nullptr;
    CHECK_RC(m_HwAccessors[FuseMacroType::Fuse]->GetFuseBlock(&pOldFuses));
    vector<UINT32>& fuseBlock = (*pFuseBinary)[FuseMacroType::Fuse];
    fuseBlock = *pOldFuses;

    for (auto* pSubEncoder : m_SubEncoders[FuseMacroType::Fuse])
    {
        CHECK_RC(pSubEncoder->EncodeFuseVals(&fusesToBurn, *pLwrrentValues, &fuseBlock));
    }

    // Compute and encode IFF CRC once IFF records have been encoded
    RegHal& regHal = m_pDev->Regs();
    string crcFuse = "OPT_SELWRE_IFF_CRC_CHECK";
    if (regHal.IsSupported(MODS_FUSE_OPT_SELWRE_IFF_CRC_CHECK) &&
        fuseRequest.m_HasIff &&
        m_TempIgnoredFuses.count(crcFuse) == 0)
    {
        const UINT32 iffCrcValid = 1 << 16;
        UINT32 iffCrc = 0;
        CHECK_RC(m_pIffEncoder->CallwlateMacroIffCrc(fuseBlock, &iffCrc));
        FuseValues iffCrcRequest;
        iffCrcRequest.m_NamedValues[crcFuse] = iffCrc | iffCrcValid;
        for (auto* pSubEncoder : m_SubEncoders[FuseMacroType::Fuse])
        {
            CHECK_RC(pSubEncoder->EncodeFuseVals(&iffCrcRequest, *pLwrrentValues, &fuseBlock));
        }
    }

    // If present, grab the FPF macro and encode a new binary
    if (m_HasFpf)
    {
        const vector<UINT32>* pOldFpfs = nullptr;
        CHECK_RC(m_HwAccessors[FuseMacroType::Fpf]->GetFuseBlock(&pOldFpfs));
        vector<UINT32>& fpfBlock = (*pFuseBinary)[FuseMacroType::Fpf];
        fpfBlock = *pOldFpfs;

        for (auto* pSubEncoder : m_SubEncoders[FuseMacroType::Fpf])
        {
            CHECK_RC(pSubEncoder->EncodeFuseVals(&fusesToBurn, *pLwrrentValues, &fpfBlock));
        }
    }

    // If present, grab the RIR info and encode a new binary
    if (m_HasRir)
    {
        const vector<UINT32>* pOldRir = nullptr;
        CHECK_RC(m_HwAccessors[FuseMacroType::Rir]->GetFuseBlock(&pOldRir));
        vector<UINT32>& rirBlock = (*pFuseBinary)[FuseMacroType::Rir];
        rirBlock = *pOldRir;

        for (auto* pSubEncoder : m_SubEncoders[FuseMacroType::Rir])
        {
            CHECK_RC(pSubEncoder->EncodeFuseVals(&fusesToBurn, *pLwrrentValues, &rirBlock));
        }
    }

    if (!fusesToBurn.IsEmpty())
    {
        for (const auto& nameValPair : fusesToBurn.m_NamedValues)
        {
            const string& name = nameValPair.first;
            if (m_FuseDefs.count(name) == 0)
            {
                Printf(FuseUtils::GetErrorPriority(), "Unknown fuse: %s\n", name.c_str());
            }
            else
            {
                Printf(FuseUtils::GetErrorPriority(), "Unencoded fuse: %s\n", name.c_str());
                MASSERT(!"Unencoded Fuse");
                return RC::SOFTWARE_ERROR;
            }
        }

        if (!fusesToBurn.m_IffRecords.empty())
        {
            Printf(FuseUtils::GetErrorPriority(), "IFF records not handled\n");
        }

        rc = RC::ILWALID_ARGUMENT;
    }

    return rc;
}

RC SplitMacroEncoder::EncodeSwFuseValues
(
    const FuseValues& fuseRequest,
    SwFuseMap* pSwFuseWrites
)
{
    MASSERT(pSwFuseWrites != nullptr);
    pSwFuseWrites->clear();
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    // In theory, there's no reason to separate the writes between FUSE/FPF
    // since offsets are absolute to the chip, but just in case something
    // (e.g. security) changes, it's better to separate the writes.
    auto& fuseWrites = (*pSwFuseWrites)[FuseMacroType::Fuse];
    map<UINT32, UINT32>* pFpfWrites = nullptr;
    if (m_HasFpf)
    {
        pFpfWrites = &(*pSwFuseWrites)[FuseMacroType::Fpf];
    }

    for (const auto& nameValPair : fuseRequest.m_NamedValues)
    {
        auto fuseDefIter = m_FuseDefs.find(nameValPair.first);
        if (fuseDefIter == m_FuseDefs.end())
        {
            Printf(FuseUtils::GetErrorPriority(), "Unknown fuse: %s\n", nameValPair.first.c_str());
            return RC::ILWALID_ARGUMENT;
        }
        const string& name = fuseDefIter->first;
        const auto& fuseDef = fuseDefIter->second;
        UINT32 offset = fuseDef.PrimaryOffset.Offset;
        if (offset != FuseUtil::UNKNOWN_OFFSET)
        {
            UINT32 value = nameValPair.second;
            if (fuseDef.Type == "dis")
            {
                // For disable fuses, a 1 in the raw fuses
                // is translated to a 0 in the OPT register
                UINT32 mask = (1 << fuseDef.NumBits) - 1;
                value = ~value & mask;
            }
            else if (fuseDef.Type == "en/dis")
            {
                // For undo fuses, we need to process the undo operation
                // before writing to the OPT register
                if ((value & (1 << fuseDef.NumBits)) != 0)
                {
                    value = 0;
                }
            }
            if (fuseDef.isInFpfMacro)
            {
                if (!m_HasFpf)
                {
                    Printf(FuseUtils::GetErrorPriority(),
                        "FPF fuse definition found, but encoder doesn't support FPF\n");
                    return RC::SOFTWARE_ERROR;
                }
                (*pFpfWrites)[offset] = value;
            }
            else
            {
                fuseWrites[offset] = value;
            }
            Printf(FuseUtils::GetPrintPriority(), "%28s @ 0x%08x to 0x%08x\n",
                   name.c_str(), offset, value);
        }
        else
        {
            Printf(FuseUtils::GetPrintPriority(), "%s has no OPT fuse register. Bypassing\n",
                           name.c_str());
        }
    }
    return rc;
}

RC SplitMacroEncoder::EncodeReworkFuseValues
(
    const FuseValues &srcFuses,
    const FuseValues &destFuses,
    ReworkRequest *pRework
)
{
    MASSERT(pRework);
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    // Create a copy of the fuse values so the sub-encoders
    // can remove fuse values as they are encoded
    FuseValues srcFusesCopy  = srcFuses;
    FuseValues destFusesCopy = destFuses;

    vector<string> fusesToRemove;
    for (const auto& nameValPair : destFusesCopy.m_NamedValues)
    {
        const string& name = nameValPair.first;
        const UINT32& requestedVal = nameValPair.second;

        // Fuse is marked as don't care in the source SKU,
        // but not in the dest SKU
        if (srcFusesCopy.m_NamedValues.count(name) == 0)
        {
            continue;
        }

        const UINT32 lwrVal  = srcFusesCopy.m_NamedValues.at(name);
        UINT32 valToBurn     = requestedVal;

        if (lwrVal == valToBurn)
        {
            // Remove all fuses that are the same across both SKUs
            fusesToRemove.push_back(name);
            // Remove all floorsweeping fuses that shouldn't be modifiable
            m_pFuselessFuseEncoder->RemoveFromFsFuseList(name);
        }
    }
    for (const auto &name: fusesToRemove)
    {
        srcFusesCopy.m_NamedValues.erase(name);
        destFusesCopy.m_NamedValues.erase(name);
    }

    // Encode fuse macro values
    UINT32 numRows;
    CHECK_RC(m_HwAccessors[FuseMacroType::Fuse]->GetNumFuseRows(&numRows));
    pRework->staticFuseRows.resize(numRows, 0);
    for (auto* pSubEncoder : m_SubEncoders[FuseMacroType::Fuse])
    {
        CHECK_RC(pSubEncoder->EncodeReworkFuseVals(&srcFusesCopy,
                                                   &destFusesCopy,
                                                   pRework));
    }

    pRework->hasFpfMacro = m_HasFpf;
    if (m_HasFpf)
    {
        // Encode FPF macro values
        CHECK_RC(m_HwAccessors[FuseMacroType::Fpf]->GetNumFuseRows(&numRows));
        pRework->fpfRows.resize(numRows, 0);
        for (auto* pSubEncoder : m_SubEncoders[FuseMacroType::Fpf])
        {
            CHECK_RC(pSubEncoder->EncodeReworkFuseVals(&srcFusesCopy,
                                                       &destFusesCopy,
                                                       pRework));
        }
    }

    pRework->supportsRir = m_HasRir;
    if (m_HasRir)
    {
        // Encode RIR section
        CHECK_RC(m_HwAccessors[FuseMacroType::Rir]->GetNumFuseRows(&numRows));
        pRework->rirRows.resize(numRows, 0);
        for (auto* pSubEncoder : m_SubEncoders[FuseMacroType::Rir])
        {
            CHECK_RC(pSubEncoder->EncodeReworkFuseVals(&srcFusesCopy,
                                                       &destFusesCopy,
                                                       pRework));
        }
    }

    if (!destFusesCopy.IsEmpty())
    {
        for (const auto& nameValPair : destFusesCopy.m_NamedValues)
        {
            const string& name = nameValPair.first;
            if (m_FuseDefs.count(name) == 0)
            {
                Printf(FuseUtils::GetErrorPriority(), "Unknown fuse: %s\n", name.c_str());
            }
            else
            {
                Printf(FuseUtils::GetErrorPriority(), "Unencoded fuse: %s\n", name.c_str());
                MASSERT(!"Unencoded Fuse");
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    return rc;
}

RC SplitMacroEncoder::EncodeSwIffValues
(
    const FuseValues& fuseRequest,
    vector<UINT32>* pIffRows
)
{
    return m_pIffEncoder->EncodeSwFuseVals(fuseRequest, pIffRows);
}

RC SplitMacroEncoder::CallwlateIffCrc(const vector<UINT32>& iffRows, UINT32 * pCrcVal)
{
    MASSERT(pCrcVal != nullptr);

    return m_pIffEncoder->CallwlateCrc(iffRows, pCrcVal);
}

bool SplitMacroEncoder::CanBurnFuse(const string& fuseName, FuseFilter filter, UINT32 newValue)
{
    if (m_IgnoreLwrrFuseVals)
    {
        return true;
    }

    auto fuseDefIter = m_FuseDefs.find(fuseName);
    if (fuseDefIter == m_FuseDefs.end())
    {
        return false;
    }
    UINT32 oldValue = 0;
    RC rc = GetFuseValue(fuseName, filter, &oldValue);
    if (rc != OK)
    {
        return false;
    }

    const auto& fuseDef = fuseDefIter->second;

    // Fuseless fuses can always be changed to any value
    if (fuseDef.Type == "fuseless")
    {
        return true;
    }

    if (fuseDef.isInFpfMacro)
    {
        return m_pFpfColEncoder->CanBurnFuse(fuseDef, oldValue, newValue);
    }
    else
    {
        return m_ColumnFuseEncoder.CanBurnFuse(fuseDef, oldValue, newValue);
    }
}

bool SplitMacroEncoder::GetUseUndoFuse()
{
    return m_UseUndo;
}

void SplitMacroEncoder::SetUseUndoFuse(bool useUndo)
{
    m_UseUndo = true;
    m_ColumnFuseEncoder.SetUseUndoFuse(useUndo);
    if (m_HasFpf)
    {
        m_pFpfColEncoder->SetUseUndoFuse(useUndo);
    }
}

void SplitMacroEncoder::ForceOrderedFFRecords(bool ordered)
{
    m_pFuselessFuseEncoder->ForceOrderedFFRecords(ordered);
}

RC SplitMacroEncoder::SetUseRirOnFuse(const string& fuseName)
{
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    if (!m_HasRir)
    {
        Printf(FuseUtils::GetErrorPriority(), "RIR not supported. Ignoring RIR request for %s\n",
                                               fuseName.c_str());
        return RC::ILWALID_ARGUMENT;
    }
    else
    {
        auto fuseDefIter = m_FuseDefs.find(Utility::ToUpperCase(fuseName));
        if (fuseDefIter == m_FuseDefs.end())
        {
            Printf(FuseUtils::GetErrorPriority(), "Invalid fuse name : %s, Ignoring RIR request\n",
                                                   fuseName.c_str());
            return RC::ILWALID_ARGUMENT;
        }

        const auto& fuseDef = fuseDefIter->second;
        if (fuseDef.Type == "fuseless")
        {
            Printf(FuseUtils::GetWarnPriority(), "Ignoring RIR request for fuseless fuse %s\n",
                                                  fuseName.c_str());
        }
        else
        {
            m_ColumnFuseEncoder.SetUseRirOnFuse(fuseName);
        }
    }

    return rc;
}

void SplitMacroEncoder::IgnoreLwrrFuseVals(bool ignoreVals)
{
    m_IgnoreLwrrFuseVals = ignoreVals;
}

void SplitMacroEncoder::SetIgnoreRawOptMismatch(bool ignore)
{
    m_IgnoreRawOptMismatch = ignore;
}

RC SplitMacroEncoder::GetFuseValue(const string& fuse, FuseFilter filter, UINT32* pVal)
{
    RC rc;
    MASSERT(pVal != nullptr);

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    const string fuseName = Utility::ToUpperCase(fuse);
    auto fuseDefIter = m_FuseDefs.find(fuseName);
    if (fuseDefIter == m_FuseDefs.end())
    {
        Printf(FuseUtils::GetErrorPriority(), "Fuse %s does not exist!\n", fuseName.c_str());
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    const FuseUtil::FuseDef& fuseDef = fuseDefIter->second;
    if (!IsFuseSupported(fuseDef, filter))
    {
        Printf(FuseUtils::GetErrorPriority(), "Invalid fuse passed to GetFuseValue: %s\n",
                                               fuseName.c_str());
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT32 optVal = 0;
    UINT32 rawVal = 0;
    const FuseValues* pOptFuses = nullptr;
    const FuseValues* pRawFuses = nullptr;

    CHECK_RC(GetOptFuseValues(&pOptFuses));
    bool optFusesOnly = ((filter & FuseFilter::OPT_FUSES_ONLY) != 0);
    bool hasOptFuse   = (pOptFuses->m_NamedValues.count(fuseName) != 0);
    MASSERT(hasOptFuse || !optFusesOnly);
    if (hasOptFuse)
    {
        optVal = pOptFuses->m_NamedValues.at(fuseName);
    }

    if (!optFusesOnly)
    {
        CHECK_RC(GetRawFuseValues(&pRawFuses));
        rawVal = pRawFuses->m_NamedValues.at(fuseName);
        if (fuseDef.Type == "en/dis")
        {
            if ((rawVal & (1 << fuseDef.NumBits)) != 0)
            {
                rawVal = 0;
            }
        }
    }

    if (hasOptFuse && !optFusesOnly &&
        !FuseUtils::GetUseDummyFuses())
    {
        if (rawVal != optVal &&
            !m_IgnoreRawOptMismatch)
        {
            Printf(FuseUtils::GetWarnPriority(),
                              "RAW/OPT Mismatch for %s: Raw 0x%x Opt 0x%x\n",
                              fuseName.c_str(), rawVal, optVal);
        }
    }

    *pVal = optFusesOnly ? optVal : rawVal;
    return rc;
}

RC SplitMacroEncoder::GetFuseValues(FuseValues* pFilteredValues, FuseFilter filter)
{
    RC rc;
    MASSERT(pFilteredValues != nullptr);

    const FuseValues* pFuseValues = nullptr;
    if ((filter & FuseFilter::OPT_FUSES_ONLY) != 0)
    {
        CHECK_RC(GetOptFuseValues(&pFuseValues));
    }
    else
    {
        CHECK_RC(GetRawFuseValues(&pFuseValues));
    }

    for (const auto& fuseNameDefPair : m_FuseDefs)
    {
        const auto& fuseName = fuseNameDefPair.first;
        const auto& fuseDef = fuseNameDefPair.second;
        if (!IsFuseSupported(fuseDef, filter))
        {
            continue;
        }
        UINT32 fuseVal = 0;
        CHECK_RC(GetFuseValue(fuseName, filter, &fuseVal));

        pFilteredValues->m_NamedValues[fuseName] = fuseVal;
    }

    if ((filter & FuseFilter::OPT_FUSES_ONLY) == 0)
    {
        pFilteredValues->m_IffRecords = pFuseValues->m_IffRecords;
        pFilteredValues->m_HasIff = pFuseValues->m_HasIff;
    }
    return rc;
}

RC SplitMacroEncoder::DecodeRawFuses(FuseValues* pDecodedVals)
{
    MASSERT(pDecodedVals);
    RC rc;

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }

    const vector<UINT32>* pBinary = nullptr;
    CHECK_RC(m_HwAccessors[FuseMacroType::Fuse]->GetFuseBlock(&pBinary));
    for (auto* pSubEncoder : m_SubEncoders[FuseMacroType::Fuse])
    {
        CHECK_RC(pSubEncoder->DecodeFuseVals(*pBinary, pDecodedVals));
    }

    if (m_HasFpf)
    {
        pBinary = nullptr;
        CHECK_RC(m_HwAccessors[FuseMacroType::Fpf]->GetFuseBlock(&pBinary));
        for (auto* pSubEncoder : m_SubEncoders[FuseMacroType::Fpf])
        {
            CHECK_RC(pSubEncoder->DecodeFuseVals(*pBinary, pDecodedVals));
        }
    }

    return rc;
}

RC SplitMacroEncoder::ReadOptFuses(FuseValues* pDecodedVals)
{
    for (const auto& nameDefPair : m_FuseDefs)
    {
        const FuseUtil::FuseDef& fuseDef = nameDefPair.second;
        UINT32 offset = fuseDef.PrimaryOffset.Offset;
        if (offset != FuseUtil::UNKNOWN_OFFSET)
        {
            UINT32 fuseValue = 0;
            if (fuseDef.isInFpfMacro)
            {
                fuseValue = m_HwAccessors[FuseMacroType::Fpf]->GetOptFuseValue(offset);
            }
            else
            {
                fuseValue = m_HwAccessors[FuseMacroType::Fuse]->GetOptFuseValue(offset);
            }
            // "dis" fuses have ilwerted OPT registers. Ilwert the
            // value here so we don't get OPT/RAW mismatches later.
            if (fuseDef.Type == "dis")
            {
                UINT32 mask = (1 << fuseDef.NumBits) - 1;
                fuseValue  ^= mask;
            }
            pDecodedVals->m_NamedValues[nameDefPair.first] = fuseValue;
        }
    }

    return RC::OK;
}

RC SplitMacroEncoder::Initialize()
{
    RC rc;
    string fuseFile = FuseUtils::GetFuseFilename(m_DevId);
    const FuseUtil::FuseDefMap* pFuseDefs = nullptr;
    FuseUtil::FuselessFuseInfo fuselessFuseInfo;
    bool hasFuseMacroMerged = false;

    if (m_pDev->HasFeature(Device::GPUSUB_HAS_MERGED_FUSE_MACROS))
    {
        hasFuseMacroMerged = true;
    }

    CHECK_RC(FuseUtil::GetFuseMacroInfo(fuseFile, &fuselessFuseInfo, &pFuseDefs,
                                        hasFuseMacroMerged));
    m_FuseDefs = *pFuseDefs;

    FuseUtil::FuseDefMap columnDefs;
    FuseUtil::FuseDefMap ffDefs;
    FuseUtil::FuseDefMap fpfDefs;

    for (const auto nameDefPair : m_FuseDefs)
    {
        if (nameDefPair.second.isInFpfMacro)
        {
            fpfDefs.insert(nameDefPair);
        }
        else if (nameDefPair.second.Type == "fuseless")
        {
            ffDefs.insert(nameDefPair);
        }
        else
        {
            columnDefs.insert(nameDefPair);
        }
    }

    m_ColumnFuseEncoder.Initialize(columnDefs);
    m_pFuselessFuseEncoder->Initialize(ffDefs, fuselessFuseInfo.fuselessStartRow,
        fuselessFuseInfo.fuselessEndRow);
    if (m_HasIff)
    {
        m_pIffEncoder->Initialize(fuselessFuseInfo.fuselessStartRow,
            fuselessFuseInfo.fuselessEndRow);
    }

    if (m_HasFpf)
    {
        m_pFpfColEncoder->Initialize(fpfDefs);
    }

    if (m_HasRir)
    {
        // Column encoded fuses are eligible for RIR
        m_pRirEncoder->Initialize(columnDefs);
    }

    m_IsInitialized = true;

    return rc;
}

void SplitMacroEncoder::PostSltIgnoreFuse(const string& fuseName)
{
    string upperName = Utility::ToUpperCase(fuseName);
    m_PostSltIgnoredFuses.insert(upperName);
}

void SplitMacroEncoder::TempIgnoreFuse(const string& fuseName)
{
    string upperName = Utility::ToUpperCase(fuseName);
    m_TempIgnoredFuses.insert(upperName);
}

void SplitMacroEncoder::ClearTempIgnoreList()
{
    m_TempIgnoredFuses.clear();
}

RC SplitMacroEncoder::AddReworkFuseInfo
(
    const string &name,
    UINT32 newVal,
    vector<ReworkFFRecord> &fuseRecords
) const
{
    return m_pFuselessFuseEncoder->AddReworkFuseInfo(name, newVal, fuseRecords);
}

bool SplitMacroEncoder::IsFsFuse(const string &fsFuseName) const
{
    return m_pFuselessFuseEncoder->IsFsFuse(fsFuseName);
}

void SplitMacroEncoder::AppendFsFuseList(const string& fsFuseName)
{
    m_pFuselessFuseEncoder->AppendFsFuseList(fsFuseName);
}

bool SplitMacroEncoder::IsFuseSupported(const FuseUtil::FuseDef& fuseDef, FuseFilter filter)
{
    // Check for customer visibility and the presence of an OPT fuse
    bool lwstomerVisibleOk = fuseDef.Visibility != 0 ||
        (filter & FuseFilter::LWSTOMER_FUSES_ONLY) == 0;

    bool optFusesOk = fuseDef.PrimaryOffset.Offset != FuseUtil::UNKNOWN_OFFSET ||
                      (filter & FuseFilter::OPT_FUSES_ONLY) == 0;

    bool bIsSltFused = true;
    m_pDev->IsSltFused(&bIsSltFused);

    string fuseName = Utility::ToUpperCase(fuseDef.Name);
    bool ignored = (bIsSltFused && m_PostSltIgnoredFuses.count(fuseName) != 0) ||
                   m_TempIgnoredFuses.count(fuseName) != 0;

    return lwstomerVisibleOk && optFusesOk && !ignored;
}

void SplitMacroEncoder::SetNumSpareFFRows(const UINT32 numSpareFFRows)
{
    m_pFuselessFuseEncoder->SetNumSpareFFRows(numSpareFFRows);
}

/* virtual */ RC SplitMacroEncoder::DecodeIffRecord
(
    const shared_ptr<IffRecord>& pRecord,
    string &output,
    bool* pIsValid
)
{
    return m_pIffEncoder->DecodeIffRecord(pRecord, output, pIsValid);
}

/* virtual */ RC SplitMacroEncoder::GetIffRecords
(
    const list<FuseUtil::IFFInfo>& iffPatches,
    vector<shared_ptr<IffRecord>>* pRecords
)
{
    return m_pIffEncoder->GetIffRecords(iffPatches, pRecords);
}

/* virtual */ RC SplitMacroEncoder::ValidateIffRecords
(
    const vector<shared_ptr<IffRecord>>& chipRecords,
    const vector<shared_ptr<IffRecord>>& skuRecords,
    bool* pDoesMatch
)
{
    return m_pIffEncoder->ValidateIffRecords(chipRecords, skuRecords, pDoesMatch);
}
