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

#include "gpu/fuse/skuhandler.h"

class StubSkuHandler : public SkuHandler
{
public:
    virtual ~StubSkuHandler() = default;

    RC GetDesiredFuseValues
    (
        const string& sku,
        bool useLwrrOptFuses,
        bool isSwFusing,
        FuseValues* pRequest
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC AddDebugFuseOverrides(const string& sku) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC OverrideFuseVal(const string& fuseName, FuseOverride fuseOverride) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC GetIffRecords(const string& sku, vector<shared_ptr<IffRecord>>* pIffRecords) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC DoesSkuMatch
    (
        const string& sku,
        bool* pDoesMatch,
        FuseFilter filter,
        Tee::Priority pri
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC FindMatchingSkus(vector<string>* pMatches, FuseFilter) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC GetMismatchingFuses
    (
        const string& chipSku,
        FuseFilter filter,
        FuseDiff *pRetInfo
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC PrintMismatchingFuses
    (
        const string& chipSku,
        FuseFilter filter
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }

    void UseTestValues() override
        { MASSERT(!"Unsupported function"); }

    void CheckReconfigFuses(bool check) override
        { MASSERT(!"Unsupported function"); }

    bool IsReconfigFusesCheckEnabled() override
        { return true; }

    RC CheckAteFuses(const string& skuName) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC GetFuseAttribute(const string& sku, const string& fuse, INT32 *pAttribute) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC DumpFsInfo(const string &chipSku) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC GetFsInfo
    (
        const string &fileName,
        const string &chipSku,
        map<string, FuseUtil::DownbinConfig>* pConfigs
    ) override
    { return RC::UNSUPPORTED_FUNCTION; }

    RC GetFuseConfig
    (
        const string &chipSku,
        map<string, FuseUtil::DownbinConfig>* pFuseConfig
    ) override
    { return RC::UNSUPPORTED_FUNCTION; }

    void AddSwFusingIgnoreFuse(const string &fuseName) override {}
    void AddReadProtectedFuse(const string &fuseName) override {}
};
