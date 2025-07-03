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

#include "gpu/fuse/fuseencoder.h"

class StubFuseEncoder : public FuseEncoder
{
public:
    virtual ~StubFuseEncoder() = default;
    
    RC RemoveUnsupportedFuses
    (
        FuseValues *pFuseRequest
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }
    
    RC EncodeFuseValues
    (
        const FuseValues& fuseRequest,
        FuseBinaryMap* pFuseBinary
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC EncodeSwFuseValues
    (
        const FuseValues& fuseRequest,
        SwFuseMap* pSwFuseWrites
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC EncodeReworkFuseValues
    (
        const FuseValues &srcFuses,
        const FuseValues &destFuses,
        ReworkRequest *pRework
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC EncodeSwIffValues
    (
        const FuseValues& fuseRequest,
        vector<UINT32>* pIffRows
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }
    
    RC CallwlateIffCrc(const vector<UINT32>& iffRows, UINT32 * pCrcVal) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC GetFuseValue(const string& fuseName, FuseFilter filter, UINT32* pVal) override
        { return RC::UNSUPPORTED_FUNCTION; }

    bool CanBurnFuse(const string& fuseName, FuseFilter filter, UINT32 newValue) override
        { return false; }

    void SetUseUndoFuse(bool) override
        {}

    bool GetUseUndoFuse() override
        { return false; }

    RC SetUseRirOnFuse(const string& fuseName) override
        { return RC::UNSUPPORTED_FUNCTION; }

    void IgnoreLwrrFuseVals(bool) override
        {}

    void ForceOrderedFFRecords(bool ordered) override
        {}

    RC GetFuseValues(FuseValues* pFilteredVals, FuseFilter filter) override
        { return RC::UNSUPPORTED_FUNCTION; }

    bool IsFuseSupported(const FuseUtil::FuseDef& fuseDef, FuseFilter filter) override
        { return false; }

    void PostSltIgnoreFuse(const string& fuseName) override
        { }
    void TempIgnoreFuse(const string& fuseName) override
        { }
    void ClearTempIgnoreList() override
        { }

    RC AddReworkFuseInfo
    (
        const string &name,
        UINT32 newVal,
        vector<ReworkFFRecord> &fuseRecords
    ) const override
        { return RC::UNSUPPORTED_FUNCTION; }
    bool IsFsFuse(const string &fsFuseName) const override
        { return false; }
    void AppendFsFuseList(const string& fsFuseName) override
        { }

    void SetIgnoreRawOptMismatch(bool ignore) override
        { }

    RC DecodeIffRecord
    (
        const shared_ptr<IffRecord>& pRecord,
        string &output,
        bool* pIsValid
    ) override { return RC::UNSUPPORTED_FUNCTION; }

    RC GetIffRecords
    (
        const list<FuseUtil::IFFInfo>& iffPatches,
        vector<shared_ptr<IffRecord>>* pRecords
    ) override { return RC::UNSUPPORTED_FUNCTION; }

    RC ValidateIffRecords
    (
        const vector<shared_ptr<IffRecord>>& chipRecords,
        const vector<shared_ptr<IffRecord>>& skuRecords,
        bool* pDoesMatch
    ) override { return RC::UNSUPPORTED_FUNCTION; }

protected:
    RC DecodeRawFuses(FuseValues* pDecodedVals) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC ReadOptFuses(FuseValues* pDecodedVals) override
        { return RC::UNSUPPORTED_FUNCTION; }
};
