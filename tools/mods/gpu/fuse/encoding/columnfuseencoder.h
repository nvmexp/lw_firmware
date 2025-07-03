/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/fuse/encoding/subencoder.h"
#include "core/include/fuseutil.h"
#include "gpu/fuse/fusetypes.h"

// Column fuses are the simplest encoding type.
// A fuse is assigned several fuse bits at the top of the fuse block.
// Certain fuses may be assigned a second range for more reliability;
// this second redundant range is OR'd with the original range
// to produce the final value.
class ColumnFuseEncoder : public SubFuseEncoder
{
public:
    RC Initialize(const FuseUtil::FuseDefMap& fuseDefs);

    void SetUseUndoFuse(bool useUndo) { m_UseUndoFuse = useUndo; }

    void SetUseRirOnFuse(const string& fuseName)
     { m_CanUseRir.insert(Utility::ToUpperCase(fuseName)); }

    // Check if we can program the desired value given the old value
    bool CanBurnFuse(const FuseUtil::FuseDef& fuseDef, UINT32 oldVal, UINT32 newVal);

protected:
    RC EncodeFuseValsImpl
    (
        FuseValues* pFuseRequest,
        const FuseValues& lwrrentVals,
        vector<UINT32>* pRawFuses
    ) override;

    RC DecodeFuseValsImpl
    (
        const vector<UINT32>& rawFuses,
        FuseValues* pFuseData
    ) override;

    RC EncodeReworkFuseValsImpl
    (
        FuseValues*  pSrcFuses,
        FuseValues*  pDestFuses,
        ReworkRequest* pRework
    ) override;

private:
    bool m_UseUndoFuse = false;
    FuseUtil::FuseDefMap m_FuseDefs;

    set<string> m_CanUseRir;

    RC ExtractFuseVal
    (
        const list<FuseUtil::FuseLoc>& fuseLocList,
        const vector<UINT32>& rawFuses,
        UINT32* pRtn
    );

    RC InsertFuseVal
    (
        const list<FuseUtil::FuseLoc>& fuseLocList,
        UINT32 newVal,
        vector<UINT32>* pRawFuses
    );

    RC GetValToBurn
    (
        const FuseUtil::FuseDef& fuseDef,
        UINT32 lwrVal,
        UINT32 *pValToBurn,
        bool canUseRir,
        bool *pNeedsRir
    );
};
