/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
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

// RIR (redundancy information rows) is a feature traditionally meant
// to repair bits in the fuse macro
// We can utilize it to colwert some non-fuseless fuse bits from 1->0
// by treating them as bits to be 'repaired'
class RirEncoder : public SubFuseEncoder
{
public:
    RC Initialize(const FuseUtil::FuseDefMap& fuseDefs);

protected:
    RC EncodeFuseValsImpl
    (
        FuseValues* pFuseRequest,
        const FuseValues& lwrrentVals,
        vector<UINT32>* pRir
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
    FuseUtil::FuseDefMap m_FuseDefs;

    vector<UINT16> m_RirRecords;

    RC AddRirRecord
    (
        const FuseValues& lwrrentVals,
        const FuseUtil::FuseDef &fuseDef,
        UINT32 valToBurn
    );

    RC InsertRirRecord
    (
        INT32 bitToFlip,
        const list<FuseUtil::FuseLoc>& fuseNums
    );

    //! Creates a RIR record for a given bit to be repaired
    //! bit       : absolute index of the bit in the fuse macro
    //! shouldSet : whether bit to be repaired should be set to 1
    UINT32 CreateRirRecord
    (
        UINT32 bit,
        bool shouldSet
    ) const;

    RC GetAbsoluteBitIndex
    (
        INT32 bitIndex,
        const list<FuseUtil::FuseLoc>& fuseNums,
        UINT32* absIndex
    ) const;
};
