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

#include "gpu/fuse/encoding/columnfuseencoder.h"
#include "gpu/fuse/fuseutils.h"

RC ColumnFuseEncoder::Initialize(const FuseUtil::FuseDefMap& fuseDefs)
{
    m_FuseDefs = fuseDefs;
    m_IsInitialized = true;
    // TODO: Sanity check the fuse defs to make sure they're all column fuses
    return OK;
}

//-----------------------------------------------------------------------------
// Encodes the column based fuses into binary for fusing
RC ColumnFuseEncoder::EncodeFuseValsImpl
(
    FuseValues* pFuseRequest,
    const FuseValues& lwrrentVals,
    vector<UINT32>* pRawFuses
)
{
    MASSERT(pRawFuses != nullptr);
    MASSERT(pFuseRequest != nullptr);
    RC rc;

    vector<string> handledFuses;
    for (const auto& nameValPair : pFuseRequest->m_NamedValues)
    {
        const string& name = nameValPair.first;
        const UINT32& requestedVal = nameValPair.second;

        if (m_FuseDefs.count(name) == 0)
        {
            // Handled by a different encoder
            continue;
        }

        auto& fuseDef = m_FuseDefs[name];

        const UINT32 lwrVal  = lwrrentVals.m_NamedValues.at(name);
        UINT32 valToBurn     = requestedVal;
        const bool canUseRir = (m_CanUseRir.count(name) != 0);
        bool needsRir        = false;    

        CHECK_RC(GetValToBurn(fuseDef, lwrVal, &valToBurn, canUseRir, &needsRir));

        if (needsRir && canUseRir)
        {
            Printf(Tee::PriNormal, "Need to use RIR for fuse %s\n", name.c_str());
            pFuseRequest->m_NeedsRir.insert(name);
            // Leave the 1->0 transition as is, to be handled by RIR later
            valToBurn |= lwrVal;
        }
        else
        {
            handledFuses.push_back(name);
        }

        CHECK_RC(InsertFuseVal(fuseDef.FuseNum, valToBurn, pRawFuses));
        if (!fuseDef.RedundantFuse.empty())
        {
            CHECK_RC(InsertFuseVal(fuseDef.RedundantFuse, valToBurn, pRawFuses));
        }
    }

    // Remove the fuse from the fuse request so we know we've handled it
    for (const auto& handledFuse : handledFuses)
    {
        pFuseRequest->m_NamedValues.erase(handledFuse);
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Decodes the column based fuses and store the values in the given map
RC ColumnFuseEncoder::DecodeFuseValsImpl
(
    const vector<UINT32>& rawFuses,
    FuseValues* pFuseData
)
{
    MASSERT(pFuseData != nullptr);
    RC rc;

    for (const auto& nameDefPair : m_FuseDefs)
    {
        const string& name = nameDefPair.first;
        const FuseUtil::FuseDef& fuseDef = nameDefPair.second;
        UINT32 fuseVal;
        CHECK_RC(ExtractFuseVal(fuseDef.FuseNum, rawFuses, &fuseVal));
        if (fuseDef.Type == "en/dis")
        {
            // For en/dis fuse, final_value = (fuse[MSB-1] == 1'b1) ? 0 : fuse[MSB-2:0]
            // For if MSB is set fuse goes to disabled state, i.e 0
            bool undone = (fuseVal & (1 << fuseDef.NumBits)) != 0;
            if (undone)
            {
                fuseVal = 0;
            }
        }
        pFuseData->m_NamedValues[name] = fuseVal;

        // TODO: account for redundant fuses according to the Tolerance enum
        // For now, we can assume that the fuse bits at FuseNum match
        // those at RedundantFuse, but later we should check if requested
    }
    return rc;
}
        
//-----------------------------------------------------------------------------
RC ColumnFuseEncoder::EncodeReworkFuseValsImpl
(
    FuseValues*  pSrcFuses,
    FuseValues*  pDestFuses,
    ReworkRequest* pRework
)
{
    MASSERT(pSrcFuses);
    MASSERT(pDestFuses);
    MASSERT(pRework);
    RC rc;
   
    vector<string> handledFuses;
    for (const auto& nameValPair : pDestFuses->m_NamedValues)
    {
        const string& name = nameValPair.first;
        const UINT32& requestedVal = nameValPair.second;

        if (m_FuseDefs.count(name) == 0)
        {
            // Handled by a different encoder
            continue;
        }
    
        auto& fuseDef = m_FuseDefs[name];

        UINT32 lwrVal  = 0;
        if (pSrcFuses->m_NamedValues.count(name) != 0)
        {
            lwrVal = pSrcFuses->m_NamedValues.at(name);
        }

        UINT32 valToBurn     = requestedVal;
        const bool canUseRir = (m_CanUseRir.count(name) != 0);
        bool needsRir        = false;    
        
        CHECK_RC(GetValToBurn(fuseDef, lwrVal, &valToBurn, canUseRir, &needsRir));

        if (needsRir && canUseRir)
        {
            Printf(Tee::PriNormal, "Need to use RIR for fuse %s\n", name.c_str());
            pDestFuses->m_NeedsRir.insert(name);
            // Leave the 1->0 transition as is, to be handled by RIR later
            valToBurn |= lwrVal;
        }
        else
        {
            handledFuses.push_back(name);
        }

        UINT32 newBits = ~lwrVal & valToBurn;
        auto *pFuseRows = &pRework->staticFuseRows;
        if (pRework->hasFpfMacro && fuseDef.isInFpfMacro)
        {
            pFuseRows = &pRework->fpfRows;
        }
        CHECK_RC(InsertFuseVal(fuseDef.FuseNum, newBits, pFuseRows));
        if (!fuseDef.RedundantFuse.empty())
        {
            CHECK_RC(InsertFuseVal(fuseDef.RedundantFuse, newBits, pFuseRows));
        }
    }

    // Remove the fuse from the fuse request so we know we've handled it
    for (const auto& handledFuse : handledFuses)
    {
        pDestFuses->m_NamedValues.erase(handledFuse);
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Given a list of fuse's locations and the raw binary from the macro, decode
// a fuse's value
RC ColumnFuseEncoder::ExtractFuseVal
(
    const list<FuseUtil::FuseLoc>& fuseLocList,
    const vector<UINT32>& rawFuses,
    UINT32* pRtn
)
{
    MASSERT(pRtn);
    *pRtn = 0;
    UINT32 rtnLsb = 0;
    for (auto& loc : fuseLocList)
    {
        if (loc.Number >= rawFuses.size())
        {
            Printf(FuseUtils::GetErrorPriority(), "Fuse file error... fuse number too large\n");
            return RC::SOFTWARE_ERROR;
        }
        UINT32 row = rawFuses[loc.Number];
        UINT32 numBits = loc.Msb - loc.Lsb + 1;
        UINT32 mask = numBits == 32 ? ~0u : (1 << numBits) - 1;
        if (rtnLsb < 32)
        {
            *pRtn |= ((row >> loc.Lsb) & mask) << rtnLsb;
        }
        rtnLsb += numBits;
    }
    return OK;
}

//-----------------------------------------------------------------------------
// Given a list of fuse's locations and the raw binary from the macro, decode
// a fuse's value
RC ColumnFuseEncoder::InsertFuseVal
(
    const list<FuseUtil::FuseLoc>& fuseLocList,
    UINT32 newVal,
    vector<UINT32>* pRawFuses
)
{
    MASSERT(pRawFuses);
    for (auto& loc : fuseLocList)
    {
        if (loc.Number >= pRawFuses->size())
        {
            Printf(FuseUtils::GetErrorPriority(), "Fuse file error... fuse number too large\n");
            return RC::SOFTWARE_ERROR;
        }

        UINT32& row = (*pRawFuses)[loc.Number];
        UINT32 numBits = loc.Msb - loc.Lsb + 1;
        UINT32 mask = numBits == 32 ? ~0u : (1 << numBits) - 1;

        UINT32 oldVal = (row >> loc.Lsb) & mask;
        if ((oldVal & ~newVal) != 0)
        {
            Printf(FuseUtils::GetErrorPriority(), "1 to 0 transition detected\n");
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
        row |= (newVal & mask) << loc.Lsb;
        newVal = (numBits >= 32) ? 0 : (newVal >> numBits);
    }
    return OK;
}

//-----------------------------------------------------------------------------
//
bool ColumnFuseEncoder::CanBurnFuse
(
    const FuseUtil::FuseDef& fuseDef,
    UINT32 oldVal,
    UINT32 newVal
)
{
    bool canBurn = true;

    if (fuseDef.Type == "en/dis")
    {
        // Going from 1 to 0 is only allowed if we're explicitly told to use undo fuses
        if ((oldVal & ~newVal) && !m_UseUndoFuse)
        {
            canBurn = false;
        }

        // We can't burn en/dis fuses anymore if MSB/undo bit is set
        // We are directly using NumBits i.e logical bits as per the fuse file here as
        // numPhysicalBits = numLogicalBits + 1 for en/dis fuses
        if (oldVal & (1 << fuseDef.NumBits))
        {
            canBurn = false;
        }
    }
    else
    {
        canBurn = ((oldVal & ~newVal) == 0);
    }

    if (!canBurn && m_CanUseRir.count(fuseDef.Name) != 0)
    {
        // Assume RIR can repair everything requested for now
        canBurn = true;
    }

    return canBurn;
}

//-----------------------------------------------------------------------------
//
RC ColumnFuseEncoder::GetValToBurn
(
    const FuseUtil::FuseDef& fuseDef,
    UINT32 lwrVal,
    UINT32 *pValToBurn,
    bool canUseRir,
    bool *pNeedsRir
)
{
    MASSERT(pValToBurn);
    MASSERT(pNeedsRir);
    RC rc;

    const string& name  = fuseDef.Name;
    UINT32 requestedVal = *pValToBurn;
    // Undo fuses need special handling
    if (fuseDef.Type == "en/dis")
    {
        // If MSB of requestedVal is 1
        if (requestedVal & (1 << fuseDef.NumBits))
        {
            // Set the final burnt value to all 1
            *pValToBurn = (1 << (fuseDef.NumBits + 1)) - 1;
        }
        // If requestedVal = 0
        else if (!requestedVal)
        {
            // If MSB of lwrVal is 1, Set final burnt value to all 1 and give a warning
            if (lwrVal & (1 << fuseDef.NumBits))
            {
                *pValToBurn = (1 << (fuseDef.NumBits + 1)) - 1;
                 Printf(FuseUtils::GetWarnPriority(),
                    "Attempting transition to zero from a non-zero value for %s,"
                    " lwrrentVal 0x%x\n", name.c_str(), lwrVal);
            }
            // If MSB of lwrVal is 0, and lwrVal != 0, there's 1->0 transition, check if undo
            // allowed
            else if (lwrVal)
            {
                if (m_UseUndoFuse)
                {
                    *pValToBurn = (1 << (fuseDef.NumBits + 1)) - 1;
                }
                else
                {
                    *pNeedsRir = true;
                    if (!canUseRir)
                    {
                        Printf(FuseUtils::GetErrorPriority(),
                            "1 to 0 transition detected for %s, requestedVal 0x%x, "
                            "lwrrentVal 0x%x\n", name.c_str(), requestedVal, lwrVal);
                        return RC::FUSE_VALUE_OUT_OF_RANGE;
                    }
                }
            }
        }
        // If MSB of requestedVal is 0, and requestedVal != 0
        else if (!(requestedVal & (1 << fuseDef.NumBits)))
        {

            // If MSB of lwrVal is 1, cannot enable a fuse with the undo bit set, exception
            // being RIR
            if (lwrVal & (1 << fuseDef.NumBits))
            {
                *pNeedsRir = true;
                if (!canUseRir)
                {
                    Printf(FuseUtils::GetErrorPriority(),
                           "%s fuse is already undone, cannot redo, lwrrentVal 0x%x\n",
                           name.c_str(), lwrVal);
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }
            }
            // If MSB of lwrVal is 0, and there's 1->0 transition, report an error
            else if (lwrVal & ~requestedVal)
            {
                *pNeedsRir = true;
                if (!canUseRir)
                {
                    Printf(FuseUtils::GetErrorPriority(),
                        "1 to 0 transition detected for %s\n", name.c_str());
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }
            }
        }
        else
        {
            Printf(FuseUtils::GetErrorPriority(), "Invalid value for en/dis fuse %s: 0x%x\n",
                   name.c_str(), requestedVal);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }
    else
    {
        *pNeedsRir = ((lwrVal & ~requestedVal) != 0);
        if (*pNeedsRir && !canUseRir)
        {   
            Printf(FuseUtils::GetErrorPriority(), "1 to 0 transition detected for %s, "
                    "requestedVal 0x%x, lwrrentVal 0x%x\n", name.c_str(), requestedVal, lwrVal);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    return rc;
}
