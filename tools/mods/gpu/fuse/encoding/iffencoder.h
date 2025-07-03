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

#include "gpu/fuse/encoding/subencoder.h"
#include "gpu/fuse/fusetypes.h"

// IFFs are a CYA feature where register writes are encoded at
// the end of the fuse block. The fuse hardware will read these
// and perform the writes very early in the reset sequence.
class IffEncoder : public SubFuseEncoder
{
public:
    virtual RC Initialize
    (
     UINT32 fuselessStart,
     UINT32 fuselessEnd
     );

    virtual RC EncodeSwFuseVals
    (
        const FuseValues& fuseRequest,
        vector<UINT32>* pSwFuseRows
    );

    virtual RC CallwlateCrc(const vector<UINT32>& iffRows, UINT32* pCrcVal)
        { return RC::UNSUPPORTED_FUNCTION; }

    RC CallwlateMacroIffCrc(const vector<UINT32> &rawFuses, UINT32 *pCrc);

    virtual RC DecodeIffRecord
    (
        const shared_ptr<IffRecord>& record,
        string &output,
        bool* pIsValid
    );

    //! \brief Retrieves the IFF records that a SKU.IffPatches has specified
    virtual RC GetIffRecords
    (
        const list<FuseUtil::IFFInfo>& iffPatches,
        vector<shared_ptr<IffRecord>>* pRecords
    );

    virtual RC ValidateIffRecords
    (
        const vector<shared_ptr<IffRecord>>& chipRecords,
        const vector<shared_ptr<IffRecord>>& skuRecords,
        bool* pDoesMatch
    );

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

    virtual RC CheckIfRecordValid(const shared_ptr<IffRecord> pRecord);

    virtual bool IsIffRow(const UINT32 row) const
        { return (row & (1 << s_TypeBit)) != 0; }

    virtual UINT32 GetIlwalidatedRow() const { return s_IlwalidatedRow; }
    virtual bool IsIlwalidatedRow(const UINT32 row) const
        { return row == s_IlwalidatedRow; }

    virtual UINT32 GetFuselessStart() const { return m_FuselessStart; }
    virtual UINT32 GetFuselessEnd() const { return m_FuselessEnd; }

private:
    static constexpr UINT08 s_TypeBit = 5; // Once bit<5> == 1, the records are IFF
    static constexpr UINT32 s_IlwalidatedRow = ~0U;
    UINT32 m_FuselessStart = 0;
    UINT32 m_FuselessEnd   = 0;
};
