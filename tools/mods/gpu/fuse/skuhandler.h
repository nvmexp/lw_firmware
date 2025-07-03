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

#include "gpu/fuse/fusetypes.h"
#include "gpu/fuse/fuseencoder.h"

#include "core/include/gpu.h"

class SkuHandler
{
public:
    virtual ~SkuHandler() = default;

    virtual RC GetDesiredFuseValues
    (
        const string& sku,
        bool useLwrrOptFuses,
        bool isSwFusing,
        FuseValues* pRequest
    ) = 0;
    virtual RC AddDebugFuseOverrides(const string& sku) = 0;
    virtual RC OverrideFuseVal(const string& fuseName, FuseOverride fuseOverride) = 0;

    virtual RC GetIffRecords(const string& sku, vector<shared_ptr<IffRecord>>* pIffRecords) = 0;

    virtual RC DoesSkuMatch
    (
        const string& sku,
        bool* pDoesMatch,
        FuseFilter filter,
        Tee::Priority pri
    ) = 0;
    virtual RC FindMatchingSkus(vector<string>* pMatches, FuseFilter filter) = 0;

    virtual RC GetMismatchingFuses
    (
        const string& chipSku,
        FuseFilter filter,
        FuseDiff* pRetInfo
    ) = 0;

    virtual RC PrintMismatchingFuses
    (
        const string& chipSku,
        FuseFilter filter
    ) = 0;

    virtual void UseTestValues() = 0;

    virtual void CheckReconfigFuses(bool check) = 0;
    
    virtual bool IsReconfigFusesCheckEnabled() = 0;
    
    virtual RC CheckAteFuses(const string& skuName) = 0;

    virtual RC GetFuseAttribute(const string& sku, const string& fuse, INT32 *pAttribute) = 0;

    virtual RC DumpFsInfo(const string &chipSku) = 0;

    virtual RC GetFsInfo
    (
        const string &fileName,
        const string &chipSku,
        map<string, FuseUtil::DownbinConfig>* pConfigs
    ) = 0;

    virtual RC GetFuseConfig
    (
        const string &chipSku,
        map<string, FuseUtil::DownbinConfig>* pFuseConfig
    ) = 0;

    virtual void AddSwFusingIgnoreFuse(const string &fuseName) = 0;
    virtual void AddReadProtectedFuse(const string &fuseName) = 0;
};
