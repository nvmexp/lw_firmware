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

#include "core/include/device.h"
#include "gpu/fuse/fusetypes.h"
#include "gpu/fuse/fusehwaccess.h"

#include <memory>

using FuseBinaryMap = map<FuseMacroType, vector<UINT32>>;
using SwFuseMap = map<FuseMacroType, map<UINT32, UINT32>>;

class FuseEncoder
{
public:
    virtual ~FuseEncoder() = default;

    virtual RC RemoveUnsupportedFuses
    (
        FuseValues *pFuseRequest
    ) = 0;

    virtual RC EncodeFuseValues
    (
        const FuseValues& fuseRequest,
        FuseBinaryMap* pFuseBinary
    ) = 0;

    virtual RC EncodeSwFuseValues
    (
        const FuseValues& fuseRequest,
        SwFuseMap* pSwFuseWrites
    ) = 0;

    virtual RC EncodeReworkFuseValues
    (
        const FuseValues &srcFuses,
        const FuseValues &destFuses,
        ReworkRequest *pRework
    ) = 0;

    virtual RC EncodeSwIffValues
    (
        const FuseValues& fuseRequest,
        vector<UINT32>* pIffRows
    ) = 0;

    virtual RC CallwlateIffCrc(const vector<UINT32>& iffRows, UINT32 * pCrcVal) = 0;

    virtual RC GetFuseValue(const string& fuseName, FuseFilter filter, UINT32* pVal) = 0;

    virtual bool CanBurnFuse(const string& fuseName, FuseFilter filter, UINT32 newValue) = 0;

    virtual bool GetUseUndoFuse() = 0;
    virtual void SetUseUndoFuse(bool useUndo) = 0;

    virtual RC SetUseRirOnFuse(const string& fuseName) = 0;

    virtual RC AddReworkFuseInfo
    (
        const string &name,
        UINT32 newVal,
        vector<ReworkFFRecord> &fuseRecords
    ) const = 0;
    virtual bool IsFsFuse(const string &fsFuseName) const = 0;
    virtual void AppendFsFuseList(const string& fsFuseName) = 0;

    virtual void IgnoreLwrrFuseVals(bool ignoreVals) = 0;

    virtual void SetIgnoreRawOptMismatch(bool ignore) = 0;

    virtual void ForceOrderedFFRecords(bool ordered) = 0;

    // Ilwalidate the current fuse value decode so the next time
    // a value is requested, the decode will have to be regenerated
    void MarkDecodeDirty()
    {
        m_RawFuseVals.Clear();
        m_OptFuseVals.Clear();
    }

    virtual RC GetFuseValues(FuseValues* pFilteredVals, FuseFilter filter) = 0;

    virtual bool IsFuseSupported(const FuseUtil::FuseDef& fuseDef, FuseFilter filter) = 0;

    virtual void PostSltIgnoreFuse(const string& fuseName) = 0;
    virtual void TempIgnoreFuse(const string& fuseName) = 0;
    virtual void ClearTempIgnoreList() = 0;
    virtual void SetNumSpareFFRows(const UINT32 numSpareFFRows)
    {
        //Only for subEncoders and for GA10x+
    }

    virtual RC DecodeIffRecord
    (
        const shared_ptr<IffRecord>& pRecord,
        string &output,
        bool* pIsValid
    ) = 0;

    virtual RC GetIffRecords
    (
        const list<FuseUtil::IFFInfo>& iffPatches,
        vector<shared_ptr<IffRecord>>* pRecords
    ) = 0;

    virtual RC ValidateIffRecords
    (
        const vector<shared_ptr<IffRecord>>& chipRecords,
        const vector<shared_ptr<IffRecord>>& skuRecords,
        bool* pDoesMatch
    ) = 0;

protected:
    RC GetRawFuseValues(const FuseValues** ppLwrVals)
    {
        RC rc;
        if (m_RawFuseVals.IsEmpty())
        {
            CHECK_RC(DecodeRawFuses(&m_RawFuseVals));
        }
        *ppLwrVals = &m_RawFuseVals;
        return rc;
    }
    virtual RC DecodeRawFuses(FuseValues* pDecodedVals) = 0;

    RC GetOptFuseValues(const FuseValues** ppLwrVals)
    {
        RC rc;
        if (m_OptFuseVals.IsEmpty())
        {
            CHECK_RC(ReadOptFuses(&m_OptFuseVals));
        }
        *ppLwrVals = &m_OptFuseVals;
        return rc;
    }
    virtual RC ReadOptFuses(FuseValues* pOptVals) = 0;

private:
    FuseValues m_RawFuseVals;
    FuseValues m_OptFuseVals;
};
