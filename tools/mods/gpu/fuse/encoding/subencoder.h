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

#include "core/include/fuseutil.h"
#include "gpu/fuse/fusetypes.h"

class SubFuseEncoder
{
public:
    virtual ~SubFuseEncoder() { }

    // Encodes fuse values into the given binary
    // This function needs to remove fuses it handles from pFuseRequest
    // so the main encoder can check if any fuses went unencoded
    RC EncodeFuseVals
    (
        FuseValues* pFuseRequest,
        const FuseValues& lwrrentVals,
        vector<UINT32>* pRawFuses
    )
    {
        if (!m_IsInitialized)
        {
            return RC::SOFTWARE_ERROR;
        }
        return EncodeFuseValsImpl(pFuseRequest, lwrrentVals, pRawFuses);
    }

    // Decodes the fuse values from the given binary
    // This function may create persistent state for use in Encode, but
    // that state should be re-created each time Decode is called.
    RC DecodeFuseVals(const vector<UINT32>& rawFuses, FuseValues* pFuseData)
    {
        if (!m_IsInitialized)
        {
            return RC::SOFTWARE_ERROR;
        }
        return DecodeFuseValsImpl(rawFuses, pFuseData);
    }

    // Encodes vals for a rework from srcFuses -> destFuses
    RC EncodeReworkFuseVals
    (
        FuseValues*  pSrcFuses,
        FuseValues*  pDestFuses,
        ReworkRequest* pRework
    )
    {
        if (!m_IsInitialized)
        {
            return RC::SOFTWARE_ERROR;
        }
        return EncodeReworkFuseValsImpl(pSrcFuses, pDestFuses, pRework);
    }

    // Option for number of spare rows needed in the Fuseless fuse section for special requests
    // like for MATHS/IST in ga10x
    void SetNumSpareFFRows(const UINT32 numSpareFFRows)
    {
        m_NumSpareFFRows = numSpareFFRows;
    }

    UINT32 GetNumSpareFFRows()
    {
        return m_NumSpareFFRows;
    }
protected:
    // Sub-encoders will require initialization before use.
    // If they're not initialized Encode/Decode operations should fail.
    bool m_IsInitialized = false;

    // Keeping the default for number of spare FF rows needed for flipping 2 fuseless fuses 5
    // times each as per MATHS/IST use case : Bug 2737459
    UINT32 m_NumSpareFFRows = 5;
    virtual RC EncodeReworkFuseValsImpl
    (
        FuseValues*  pSrcFuses,
        FuseValues*  pDestFuses,
        ReworkRequest* pRework
    )
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    virtual RC EncodeFuseValsImpl
    (
        FuseValues* pFuseRequest,
        const FuseValues& lwrrentVals,
        vector<UINT32>* pRawFuses
    ) = 0;
    virtual RC DecodeFuseValsImpl(const vector<UINT32>& rawFuses, FuseValues* pFuseData)
    {
        return RC::UNSUPPORTED_FUNCTION;
    };
};

// Fuseless fuses are essentially encoded writes to the JTAG chains.
// Several fuses will be assigned ranges of bits on a JTAG chain, and
// the fuseless fuse records will program those bits.
class FuselessFuseEncoder : public SubFuseEncoder
{
public:
    // Fuseless fuses will need both fuse definitions and the row where FF records start
    virtual RC Initialize(const FuseUtil::FuseDefMap& fuseDefs, UINT32 fuselessStart, 
        UINT32 fuselessEnd) = 0;
    
    virtual RC AddReworkFuseInfo
    (
        const string &name,
        UINT32 newVal,
        vector<ReworkFFRecord> &fuseRecords
    ) const
    {
        return RC::UNSUPPORTED_FUNCTION;
    };
    virtual bool IsFsFuse(const string &fsFuseName) const
    {
        return false;
    };
    virtual void AppendFsFuseList(const string& fsFuseName)
    {
        return;
    };
    virtual void RemoveFromFsFuseList(const string &fsFuseName)
    {
        return;
    };

    virtual void ForceOrderedFFRecords(bool ordered)
    {
        return;
    };
};
