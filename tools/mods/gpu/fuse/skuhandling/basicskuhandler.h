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

#include "core/include/fuseutil.h"

class BasicSkuHandler : public SkuHandler
{
public:
    BasicSkuHandler(TestDevice *pDev, 
                    shared_ptr<FuseEncoder> pFuseEncoder)
        : m_pDev(pDev)
        , m_pEncoder(pFuseEncoder)
    {
        MASSERT(pDev);
    }
    virtual ~BasicSkuHandler() = default;

    //! Reads a SKU definition and determines the values needed to
    //! burn this chip's fuses to the SKU.
    //! If useLwrrOptFuses is set to true, it uses the current OPT register values
    //! to get chip state (as opposed to reading the raw fuse macro)
    RC GetDesiredFuseValues
    (
        const string& sku,
        bool useLwrrOptFuses,
        bool isSwFusing,
        FuseValues* pRequest
    ) override;

    //! Reads a SKU definition and populates the IFF records for that SKU
    RC GetIffRecords
    (
        const string& sku,
        vector<shared_ptr<IffRecord>>* pIffRecords
    ) override;

    //! Reads a SKU definition and determines the debug fuse values that need to be added
    //! for the SKU
    RC AddDebugFuseOverrides(const string& sku) override;

    //! \brief Mark a fuse to have a specific value.
    //!
    //! If FuseOverride.useSkuDef is true, the given value is used as a starting
    //! point, and GetDesiredFuseValues will still try to match the SKU. If false,
    //! GetDesiredFuseValues will use the given value regardless of what the SKU says.
    RC OverrideFuseVal(const string& fuseName, FuseOverride fuseOverride) override;

    //! \brief Checks whether or not the current fusing matches the given SKU
    RC DoesSkuMatch
    (
        const string& sku,
        bool* pDoesMatch,
        FuseFilter filter,
        Tee::Priority pri
    ) override;

    //! \brief Checks for a SKU match against each known SKU
    RC FindMatchingSkus(vector<string>* pMatches, FuseFilter filter) override;

    RC CheckAteFuses(const string& skuName) override;

    //! \brief Use SltTestValues instead of the actual SKU values for SLT testing
    void UseTestValues() override;

    //! \brief Set whether to compare reconfig fuses for SKU matching
    void CheckReconfigFuses(bool check) override;

    //! \brief Returns whether reconfig fuses are compared for SKU matching
    bool IsReconfigFusesCheckEnabled() override;

    RC GetMismatchingFuses
    (
        const string& chipSku,
        FuseFilter filter,
        FuseDiff* pRetInfo
    ) override;

    RC PrintMismatchingFuses
    (
        const string& chipSku,
        FuseFilter filter
    ) override;

    //! Get the attributes for the given fuse in the SKU
    RC GetFuseAttribute(const string& sku, const string& fuse, INT32 *pAttribute) override;

    RC DumpFsInfo(const string &chipSku) override;

    RC GetFsInfo
    (
        const string &fileName,
        const string &chipSku,
        map<string, FuseUtil::DownbinConfig>* pConfigs
    ) override;

    RC GetFuseConfig
    (
        const string &chipSku,
        map<string, FuseUtil::DownbinConfig>* pFuseConfig
    ) override;

    void AddSwFusingIgnoreFuse(const string &fuseName) override;
    void AddReadProtectedFuse(const string &fuseName) override;

private:
    TestDevice* m_pDev;
    
    shared_ptr<FuseEncoder> m_pEncoder;

    map<string, FuseOverride> m_Overrides;
    
    vector<string> m_SwFusingIgnoredFuses;
    vector<string> m_ReadProtectedFuses;

    bool m_IsInitialized = false;
    const FuseUtil::SkuList* m_pSkuList = nullptr;
    const FuseUtil::MiscInfo* m_pMiscInfo = nullptr;

    bool m_UseTestValues = false;
    bool m_CheckReconfigFuses = true;

    RC Initialize();

    RC GetFuseValToBurn
    (
        const FuseUtil::FuseInfo& info,
        FuseFilter filter,
        UINT32* pValToBlow,
        bool* pShouldBurn
    );

    //! \brief Retrieves the IFF records that a SKU has specified
    RC GetSkuIffRecords
    (
        const FuseUtil::SkuConfig& skuConfig,
        vector<shared_ptr<IffRecord>>* pRecords
    );

    bool IsFuseRight(const FuseUtil::FuseInfo& fuseInfo, UINT32 val, string* pMsg);

    RC IsFsRight
    (
        const FuseValues& fuseVals,
        const FuseUtil::SkuConfig& skuConfig,
        const string& sku,
        Tee::Priority pri,
        bool* pDoesMatch
    );

    RC IsIffRight
    (
        const FuseValues fuseVals,
        const FuseUtil::SkuConfig& skuConfig,
        const string& sku,
        Tee::Priority pri,
        bool* pDoesMatch
    );

    RC ConfigToJson(const FuseUtil::DownbinConfig *pConfig, shared_ptr<JsonItem> pJsonItem);
};
