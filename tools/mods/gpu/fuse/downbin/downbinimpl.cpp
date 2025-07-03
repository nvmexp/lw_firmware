/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/errormap.h"
#include "core/include/jsonparser.h"
#include "core/include/massert.h"
#include "core/include/mle_protobuf.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "gpu/fuse/fusetypes.h"

#include "downbinimpl.h"
#include "downbingrouppickers.h"
#include "downbinelementpickers.h"
#include "downbinrulesimpl.h"

using namespace Downbin;
using namespace JsonParser;
using namespace rapidjson;
using namespace FuseUtil;

// Default FS result file
static string s_DefaultFsResultFile = "sweep.json";

static const map<FsUnit, string> s_FsUnitToSkuConfigMap =
{
    { FsUnit::GPC,      "gpc" },
    { FsUnit::TPC,      "tpc" },
    { FsUnit::PES,      "pes" },
    { FsUnit::CPC,      "cpc" },
    { FsUnit::ROP,      "rop" },
    { FsUnit::FBP,      "fbp" },
    { FsUnit::FBIO,     "fbpa" },
    { FsUnit::LTC,      "l2" },
    { FsUnit::L2_SLICE, "l2_slice" },
    { FsUnit::SYSPIPE,  "sys_pipe" },
    { FsUnit::LWDEC,    "lwdec" },
    { FsUnit::LWJPG,    "lwjpg" },
    { FsUnit::IOCTRL,   "ioctrl" },
    { FsUnit::PERLINK,  "perlink" }
};

//------------------------------------------------------------------------------
DownbinImpl::DownbinImpl(TestDevice *pDev)
    : m_pDev(pDev)
{
    MASSERT(pDev != nullptr);
    MASSERT(dynamic_cast<GpuSubdevice*>(pDev) != nullptr);
}

//------------------------------------------------------------------------------
void DownbinImpl::SetParent(Fuse *pFuse)
{
    MASSERT(pFuse != nullptr);
    m_pFuse = pFuse;
}

//------------------------------------------------------------------------------
RC DownbinImpl::Initialize()
{
    if (m_bIsInitialized)
    {
        return RC::OK;
    }

    RC rc;
    if (m_pFuse == nullptr)
    {
        Printf(Tee::PriError, "Parent fuse object not set\n");
        return RC::SOFTWARE_ERROR;
    }

    // Create FS sets
    CHECK_RC(CreateFsSets());

    // Setup FS rules
    CHECK_RC(AddBasicFsRules());

    // Setup chip FS rules
    CHECK_RC(AddChipFsRules());

    // Initialize SKU list
    string fuseFile = FuseUtils::GetFuseFilename(m_pDev->GetDeviceId());
    shared_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fuseFile));
    CHECK_RC(pParser->ParseFile(fuseFile, nullptr, &m_pSkuList, nullptr));

    m_bIsInitialized = true;
    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::CreateFsSets()
{
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    auto &regs = pSubdev->Regs();

    // 1. GPC set
    const UINT32 maxGpcs         = pSubdev->GetMaxGpcCount();
    const UINT32 maxTpcsPerGpc   = pSubdev->GetMaxTpcCount();
    const UINT32 maxPesPerGpc    = pSubdev->GetMaxPesCount();
    const UINT32 maxCpcsPerGpc   = regs.IsSupported(MODS_FUSE_OPT_CPC_DISABLE) ?
                                   regs.LookupAddress(MODS_SCAL_LITTER_NUM_CPC_PER_GPC) : 0;
    const UINT32 maxRopsPerGpc   = regs.IsSupported(MODS_FUSE_OPT_ROP_DISABLE) ?
                                   regs.Read32(MODS_PTOP_SCAL_NUM_ROP_PER_GPC) : 0;
    const bool bHasSplitTpcs     = regs.IsSupported(MODS_FUSE_OPT_TPC_GPCx_DISABLE, 0);
    const bool bHasReconfigTpcs  = regs.IsSupported(MODS_FUSE_OPT_TPC_GPCx_RECONFIG, 0);
    const bool bIsRopL2Valid     = regs.IsSupported(MODS_FUSE_OPT_ROP_L2_DISABLE);
    const bool bHasSplitPes      = regs.IsSupported(MODS_FUSE_OPT_PES_GPCx_DISABLE, 0);
    // Setup FS set
    FsSet gpcSet(FsUnit::GPC, maxGpcs);
    gpcSet.SetParentDownbin(this);
    gpcSet.AddElement(FsUnit::TPC, maxTpcsPerGpc, true, false);
    if ((!bIsRopL2Valid) && (maxRopsPerGpc != 0))
    {
        gpcSet.AddElement(FsUnit::ROP, maxRopsPerGpc, false, false);
    }
    if (maxCpcsPerGpc != 0)
    {
        gpcSet.AddElement(FsUnit::CPC, maxCpcsPerGpc, false, false);
    }
    gpcSet.AddElement(FsUnit::PES, maxPesPerGpc, false, false);
    // Setup fuse details
    gpcSet.SetIsPerGroupFuse(FsUnit::TPC, bHasSplitTpcs);
    gpcSet.SetIsPerGroupFuse(FsUnit::PES, bHasSplitPes);
    gpcSet.SetHasReconfig(FsUnit::TPC, bHasReconfigTpcs);
    // Add to FsSet map
    m_FsSets.emplace(FsUnit::GPC, std::move(gpcSet));

    // 2. FBP set
    const UINT32 maxFbps         = pSubdev->GetMaxFbpCount();
    const UINT32 maxFbiosPerFbp  = regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS) / maxFbps;
    const UINT32 maxLtcsPerFbp   = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
    const UINT32 maxSlicesPerLtc = regs.IsSupported(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, 0) ?
                                   regs.LookupAddress(MODS_SCAL_LITTER_NUM_SLICES_PER_LTC) : 0;
    const UINT32 maxSlicesPerFbp = maxSlicesPerLtc * maxLtcsPerFbp;
    const UINT32 halfFbpaStride  = pSubdev->GetFsImpl()->GetHalfFbpaWidth();
    // Setup FS set
    FsSet fbpSet(FsUnit::FBP, maxFbps);
    fbpSet.SetParentDownbin(this);
    fbpSet.AddElement(FsUnit::LTC, maxLtcsPerFbp, true, false);
    if (regs.IsSupported(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, 0))
    {
        fbpSet.AddElement(FsUnit::L2_SLICE, maxSlicesPerFbp, false, false);
    }
    fbpSet.AddElement(FsUnit::FBIO, maxFbiosPerFbp, false, false);
    if (regs.IsSupported(MODS_FUSE_OPT_FBPA_DISABLE))
    {
        fbpSet.AddElement(FsUnit::FBPA, maxFbiosPerFbp, false, false);
    }
    fbpSet.AddElement(FsUnit::HALF_FBPA, halfFbpaStride, false, true);
    // Setup fuse details
    fbpSet.SetIsPerGroupFuse(FsUnit::L2_SLICE, true);
    fbpSet.SetFuseName(FsUnit::LTC, bIsRopL2Valid ? "rop_l2" : "ltc");
    fbpSet.SetIsDisableFuse(FsUnit::HALF_FBPA, false);
    // Add to FsSet map
    m_FsSets.emplace(FsUnit::FBP, std::move(fbpSet));

    // 3. Other Fs Masks
    const UINT32 maxSyspipe = regs.IsSupported(MODS_FUSE_OPT_SYS_PIPE_DEFECTIVE) ?
                                pSubdev->GetMaxSyspipeCount() : 0;
    const UINT32 maxOfa = pSubdev->GetMaxOfaCount();
    const UINT32 maxLwjpg = pSubdev->GetMaxLwjpgCount();
    const UINT32 maxLwdec = pSubdev->GetMaxLwdecCount();
    const UINT32 maxLwenc = pSubdev->GetMaxLwencCount();
    const UINT32 maxLwlink   = regs.IsSupported(MODS_FUSE_OPT_LWLINK_DISABLE) ?
                                pSubdev->GetMaxLwlinkCount() : 0;
    const UINT32 maxPerlink = regs.IsSupported(MODS_FUSE_OPT_PERLINK_DEFECTIVE) ?
                                pSubdev->GetMaxPerlinkCount() : 0;

    if (maxSyspipe)
    {
        FsSet syspipeSet(FsUnit::SYSPIPE, maxSyspipe);
        syspipeSet.SetParentDownbin(this);
        m_FsSets.emplace(FsUnit::SYSPIPE, std::move(syspipeSet));
    }

    if (maxOfa)
    {
        FsSet ofaSet(FsUnit::OFA, maxOfa);
        ofaSet.SetParentDownbin(this);
        m_FsSets.emplace(FsUnit::OFA, std::move(ofaSet));
    }

    if (maxLwjpg)
    {
        FsSet lwjpgSet(FsUnit::LWJPG, maxLwjpg);
        lwjpgSet.SetParentDownbin(this);
        m_FsSets.emplace(FsUnit::LWJPG, std::move(lwjpgSet));
    }

    if (maxLwdec)
    {
        FsSet lwdecSet(FsUnit::LWDEC, maxLwdec);
        lwdecSet.SetParentDownbin(this);
        m_FsSets.emplace(FsUnit::LWDEC, std::move(lwdecSet));
    }

    if (maxLwenc)
    {
        FsSet lwencSet(FsUnit::LWENC, maxLwenc);
        lwencSet.SetParentDownbin(this);
        m_FsSets.emplace(FsUnit::LWENC, std::move(lwencSet));
    }

    if (maxLwlink)
    {
        FsSet lwlinkSet(FsUnit::IOCTRL, maxLwlink);
        lwlinkSet.SetParentDownbin(this);
        if (maxPerlink)
        {
            const UINT32 maxPerlinksPerIoctrl = maxPerlink / maxLwlink;
            lwlinkSet.AddElement(FsUnit::PERLINK, maxPerlinksPerIoctrl, true, false);
        }
        m_FsSets.emplace(FsUnit::IOCTRL, std::move(lwlinkSet));
    }

    // Check if board fuses are available
    // Assuming both GPC and FBP board fuses are available / not available together
    const bool bHasGpcBoardFs = regs.IsSupported(MODS_FUSE_OPT_BOARD_GPC_DISABLE);
    const bool bHasFbpBoardFs = regs.IsSupported(MODS_FUSE_OPT_BOARD_FBP_DISABLE);
    m_HasBoardFsFuses = bHasGpcBoardFs && bHasFbpBoardFs;

    return RC::OK;
}

//------------------------------------------------------------------------------
// Add minimum count guarentees for the fs unit in the given element set
void DownbinImpl::AddMinGroupCountRule
(
    FsSet &fsSet,
    UINT32 groupIdx,
    FsUnit fsUnit,
    UINT32 minCount
)
{
    FsElementSet *pElementSet = fsSet.GetElementSet(groupIdx, fsUnit);
    shared_ptr<DownbinRule> pRule = make_shared<MinGroupElementCountRule>(pElementSet,
                                                                          minCount);
    pElementSet->AddOnElementDisabledListener(pRule);
}

// Add rules to establish group-element like relationship between 2 elements of the same group
// Eg. PES-TPC
void DownbinImpl::AddElementDependencyRule
(
    FsGroup *pFsGroup,
    FsUnit element,
    FsUnit subElement
)
{
    shared_ptr<DownbinRule> pRule = make_shared<SubElementDependencyRule>(pFsGroup,
                                                                          element, subElement);
    pFsGroup->GetElementSet(element)->AddOnElementDisabledListener(pRule);
    pFsGroup->GetElementSet(subElement)->AddOnElementDisabledListener(pRule);
}

RC DownbinImpl::AddBasicGpcSetRules()
{
    RC rc;
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);
    const UINT32 numGpcs = gpcSet.GetNumGroups();
    const bool bComputeOnlyGpcs = m_pDev->HasFeature(Device::GPUSUB_SUPPORTS_COMPUTE_ONLY_GPC);
    for (UINT32 groupIdx = 0; groupIdx < numGpcs; groupIdx++)
    {
        FsGroup *pFsGroup = gpcSet.GetGroup(groupIdx);

        // Basic group disable rule
        shared_ptr<DownbinRule> pRule = make_shared<BasicGroupDisableRule>(pFsGroup);
        pFsGroup->AddOnGroupDisabledListener(pRule);
        // TPC
        AddMinGroupCountRule(gpcSet, groupIdx, FsUnit::TPC, 1);
        // PES
        if (!bComputeOnlyGpcs)
        {
            AddMinGroupCountRule(gpcSet, groupIdx, FsUnit::PES, 1);
            AddElementDependencyRule(pFsGroup, FsUnit::PES, FsUnit::TPC);
        }
        // ROP
        if (!bComputeOnlyGpcs && gpcSet.HasElementFsUnit(FsUnit::ROP))
        {
            AddMinGroupCountRule(gpcSet, groupIdx, FsUnit::ROP, 1);
        }
        // CPC
        if (gpcSet.HasElementFsUnit(FsUnit::CPC))
        {
            AddMinGroupCountRule(gpcSet, groupIdx, FsUnit::CPC, 1);
            AddElementDependencyRule(pFsGroup, FsUnit::CPC, FsUnit::TPC);
        }
    }
    return rc;
}

RC DownbinImpl::AddBasicFbpSetRules()
{
    RC rc;
    FsSet &fbpSet = m_FsSets.at(FsUnit::FBP);
    const UINT32 numFbps = fbpSet.GetNumGroups();
    const UINT32 numFbiosPerFbp = fbpSet.GetElementSet(0, FsUnit::FBIO)->GetNumElements();
    for (UINT32 groupIdx = 0; groupIdx < numFbps; groupIdx++)
    {
        FsGroup *pFsGroup = fbpSet.GetGroup(groupIdx);

        // Basic group disable rule
        shared_ptr<DownbinRule> pRule = make_shared<BasicGroupDisableRule>(pFsGroup);
        pFsGroup->AddOnGroupDisabledListener(pRule);
        // FBIO
        AddMinGroupCountRule(fbpSet, groupIdx, FsUnit::FBIO, numFbiosPerFbp);
        if (fbpSet.HasElementFsUnit(FsUnit::FBPA))
        {
            AddMinGroupCountRule(fbpSet, groupIdx, FsUnit::FBPA, numFbiosPerFbp);
        }
        // LTC
        AddMinGroupCountRule(fbpSet, groupIdx, FsUnit::LTC, 1);
        // L2 Slice
        if (fbpSet.HasElementFsUnit(FsUnit::L2_SLICE))
        {
            AddElementDependencyRule(pFsGroup, FsUnit::LTC, FsUnit::L2_SLICE);
        }
    }
    return rc;
}

RC DownbinImpl::AddBasicFsRules()
{
    RC rc;
    CHECK_RC(AddBasicGpcSetRules());
    CHECK_RC(AddBasicFbpSetRules());
    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::AddChipFsRules()
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC DownbinImpl::ApplyElementCountRules
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    const FuseUtil::DownbinConfig &downbinConfig
)
{
    MASSERT(pFsSet);
    UINT32 numGroups     = pFsSet->GetNumGroups();
    bool bIsGroupElement = pFsSet->IsGroupFsUnit(fsUnit);

    // Set per set limits
    if (downbinConfig.enableCount != FuseUtil::UNKNOWN_ENABLE_COUNT)
    {
        // Add a min enable count rule here
        // The downbinning process will take care of reaching the exact count
        if (bIsGroupElement)
        {
            FsElementSet *pElementSet = pFsSet->GetGroupElementSet();
            shared_ptr<DownbinRule> pRule = make_shared<MinElementCountRule>
                                               (pFsSet, fsUnit, downbinConfig.enableCount);
            pElementSet->AddOnElementDisabledListener(pRule);
        }
        else
        {
            UINT32 requiredCount = downbinConfig.enableCount;
            if (downbinConfig.hasReconfig)
            {
                requiredCount += downbinConfig.repairMinCount;
            }
            shared_ptr<DownbinRule> pRule = make_shared<MinElementCountRule>
                                               (pFsSet, fsUnit, requiredCount);
            for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
            {
                FsElementSet *pElementSet = pFsSet->GetElementSet(groupIdx, fsUnit);
                pElementSet->AddOnElementDisabledListener(pRule);
            }
        }
    }
    // Set per group limits
    if (downbinConfig.minEnablePerGroup != 0)
    {
        if (bIsGroupElement)
        {
            Printf(Tee::PriDebug, "Skip setting of MinGroupElementCountRule for %s\n",
                    Downbin::ToString(fsUnit).c_str());
            return RC::OK;
        }

        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
        {
            FsElementSet *pElementSet = pFsSet->GetElementSet(groupIdx, fsUnit);
            shared_ptr<DownbinRule> pRule = make_shared<MinGroupElementCountRule>
                                               (pElementSet, downbinConfig.minEnablePerGroup);
            pElementSet->AddOnElementDisabledListener(pRule);
        }
    }
    return RC::OK;
}

RC DownbinImpl::ApplyMustEnableListRules
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    const FuseUtil::DownbinConfig &downbinConfig
)
{
    MASSERT(pFsSet);

    // Return early if there are no entries in mustEnableList
    if (!downbinConfig.mustEnableList.size())
    {
        return RC::OK;
    }

    bool bIsGroupElement = pFsSet->IsGroupFsUnit(fsUnit);
    // Must enable elements
    for (auto absBit : downbinConfig.mustEnableList)
    {
        if (bIsGroupElement)
        {
            if (absBit >= pFsSet->GetNumGroups())
            {
                Printf(Tee::PriError, "Must-enable index out of bounds for %ss: %u\n",
                        Downbin::ToString(fsUnit).c_str(), absBit);
                return RC::ILWALID_INPUT;
            }
            FsElementSet *pElementSet = pFsSet->GetGroupElementSet();
            shared_ptr<DownbinRule> pRule = make_shared<AlwaysEnableRule>(pElementSet,
                                                                          absBit);
            pElementSet->AddOnElementDisabledListener(pRule);
            pElementSet->MarkProtected(absBit);
            pFsSet->GetGroup(absBit)->MarkProtected();
        }
        else
        {
            // Colwert bit index into group specific index
            UINT32 numPerGroup = pFsSet->GetNumElementsPerGroup(fsUnit);
            MASSERT(numPerGroup);
            if (absBit >= pFsSet->GetNumGroups() * numPerGroup)
            {
                Printf(Tee::PriError, "Must-enable index out of bounds for %ss: %u\n",
                          Downbin::ToString(fsUnit).c_str(), absBit);
                return RC::ILWALID_INPUT;
            }

            UINT32 groupNum    = absBit / numPerGroup;
            UINT32 elementNum  = absBit % numPerGroup;

            FsElementSet *pElementSet = pFsSet->GetElementSet(groupNum, fsUnit);
            shared_ptr<DownbinRule> pRule = make_shared<AlwaysEnableRule>(pElementSet,
                                                                          elementNum);
            pElementSet->AddOnElementDisabledListener(pRule);
            pElementSet->MarkProtected(elementNum);
        }
    }

    return RC::OK;
}

RC DownbinImpl::ApplyMustDisableListRules
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    const FuseUtil::DownbinConfig &downbinConfig
)
{
    RC rc;
    MASSERT(pFsSet);
    for (auto absBit : downbinConfig.mustDisableList)
    {
        CHECK_RC(pFsSet->DisableGroup(absBit));
    }
    return rc;
}

RC DownbinImpl::ApplyGraphicsTpcsFsConfigRules
(
    FsSet *pGpcSet,
    const FuseUtil::DownbinConfig &tpcConfig
)
{
    MASSERT(pGpcSet);
    MASSERT(pGpcSet->GetGroupFsUnit() == FsUnit::GPC);

    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    if (pSubdev->HasFeature(Device::Feature::GPUSUB_SUPPORTS_COMPUTE_ONLY_GPC) &&
        tpcConfig.minGraphicsTpcs > 0)
    {
        UINT32 numGpcs = pGpcSet->GetNumGroups();
        vector<UINT32> graphicsMasks(numGpcs);
        for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
        {
            graphicsMasks[gpc] = pSubdev->GetFullGraphicsTpcMask(gpc);
        }

        shared_ptr<DownbinRule> pRule = make_shared<MinElementCountRule>
                                            (pGpcSet, FsUnit::TPC, tpcConfig.minGraphicsTpcs,
                                             graphicsMasks);
        for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
        {
            FsElementSet *pElementSet = pGpcSet->GetElementSet(gpc, FsUnit::TPC);
            pElementSet->AddOnElementDisabledListener(pRule);
        }
    }

    return RC::OK;
}

RC DownbinImpl::ApplyHbmSiteFsConfigRules
(
    FsSet *pFbpSet,
    const FuseUtil::DownbinConfig &fbpConfig
)
{
    RC rc;
    MASSERT(pFbpSet);
    MASSERT(pFbpSet->GetGroupFsUnit() == FsUnit::FBP);

    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    UINT32 numHbmSites = pSubdev->GetNumHbmSites();
    if (numHbmSites > 0 && fbpConfig.fullSitesOnly)
    {
        for (UINT32 hbmSite = 0; hbmSite < numHbmSites; hbmSite++)
        {
            UINT32 fbp1, fbp2;
            CHECK_RC(pSubdev->GetHBMSiteFbps(hbmSite, &fbp1, &fbp2));

            FsGroup *pFsGroup1 = pFbpSet->GetGroup(fbp1);
            FsGroup *pFsGroup2 = pFbpSet->GetGroup(fbp2);
            shared_ptr<DownbinRule> pHbmRule1 = make_shared<CrossElementDependencyRule>(pFbpSet,
                                                                                        FsUnit::FBP,
                                                                                        0, fbp1,
                                                                                        0, fbp2);
            shared_ptr<DownbinRule> pHbmRule2 = make_shared<CrossElementDependencyRule>(pFbpSet,
                                                                                        FsUnit::FBP,
                                                                                        0, fbp2,
                                                                                        0, fbp1);
            pFsGroup1->AddOnGroupDisabledListener(pHbmRule1);
            pFsGroup2->AddOnGroupDisabledListener(pHbmRule2);
        }
    }

    return rc;
}

RC DownbinImpl::ApplyLtcSliceFsConfigRules
(
    FsSet *pFbpSet,
    const FuseUtil::DownbinConfig &sliceConfig
)
{
    MASSERT(pFbpSet);
    MASSERT(pFbpSet->GetGroupFsUnit() == FsUnit::FBP);

    if (pFbpSet->HasElementFsUnit(FsUnit::L2_SLICE) &&
        sliceConfig.enableCountPerGroup > 0)
    {
        UINT32 numFbps = pFbpSet->GetNumGroups();
        for (UINT32 fbpIdx = 0; fbpIdx < numFbps; fbpIdx++)
        {
            FsGroup *pFsGroup = pFbpSet->GetGroup(fbpIdx);
            shared_ptr<DownbinRule> pRule = make_shared<MinSubElementCountRule>
                                                (pFsGroup, FsUnit::LTC, FsUnit::L2_SLICE,
                                                 sliceConfig.enableCountPerGroup);

            FsElementSet *pL2SliceSet = pFbpSet->GetElementSet(fbpIdx, FsUnit::L2_SLICE);
            pL2SliceSet->AddOnElementDisabledListener(pRule);
        }
    }

    return RC::OK;
}

RC DownbinImpl::ApplyVGpcSkylineRule
(
    FsSet *pGpcSet,
    const FuseUtil::DownbinConfig &tpcConfig
)
{
    MASSERT(pGpcSet);
    MASSERT(pGpcSet->GetGroupFsUnit() == FsUnit::GPC);
    if (tpcConfig.vGpcSkylineList.size())
    {
        shared_ptr<DownbinRule> pRule = make_shared<VGpcSkylineRule>
                                        (pGpcSet, tpcConfig.vGpcSkylineList);
        UINT32 numGpcs = pGpcSet->GetNumGroups();
        for (UINT32 gpcIdx = 0; gpcIdx < numGpcs; gpcIdx++)
        {
            FsElementSet *pElementSet = pGpcSet->GetElementSet(gpcIdx, FsUnit::TPC);
            pElementSet->AddOnElementDisabledListener(pRule);
        }
    }
    return RC::OK;
}

RC DownbinImpl::ApplyMiscFsConfigRules
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    const FuseUtil::DownbinConfig &downbinConfig
)
{
    RC rc;
    MASSERT(pFsSet);

    // Apply graphics TPC rules
    if (pFsSet->GetGroupFsUnit() == FsUnit::GPC &&
        fsUnit == FsUnit::TPC)
    {
        CHECK_RC(ApplyGraphicsTpcsFsConfigRules(pFsSet, downbinConfig));
    }

    // Apply full HBM site rules
    if (pFsSet->GetGroupFsUnit() == FsUnit::FBP &&
        fsUnit == FsUnit::FBP)
    {
        CHECK_RC(ApplyHbmSiteFsConfigRules(pFsSet, downbinConfig));
    }

    // Apply L2Slice - LTC count rules
    if (pFsSet->GetGroupFsUnit() == FsUnit::FBP &&
        fsUnit == FsUnit::L2_SLICE)
    {
        CHECK_RC(ApplyLtcSliceFsConfigRules(pFsSet, downbinConfig));
    }

    // Apply vGPC Skyline Rule
    if (pFsSet->GetGroupFsUnit() == FsUnit::GPC &&
        fsUnit == FsUnit::TPC)
    {
        CHECK_RC(ApplyVGpcSkylineRule(pFsSet, downbinConfig));
    }

    return rc;
}

RC DownbinImpl::ApplyFsConfigRules
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    const FuseUtil::DownbinConfig &downbinConfig
)
{
    RC rc;
    MASSERT(pFsSet);

    CHECK_RC(ApplyElementCountRules(pFsSet, fsUnit, downbinConfig));
    CHECK_RC(ApplyMustEnableListRules(pFsSet, fsUnit, downbinConfig));
    CHECK_RC(ApplyMiscFsConfigRules(pFsSet, fsUnit, downbinConfig));

    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::GetUnitDownbinConfig
(
    const string &skuName,
    FsUnit fsUnit,
    FuseUtil::DownbinConfig *pConfig
)
{
    RC rc;
    MASSERT(pConfig);

    map<string, FuseUtil::DownbinConfig> skuConfig;
    CHECK_RC(m_pFuse->GetFuseConfig(skuName, &skuConfig));

    auto fsUnitIter = s_FsUnitToSkuConfigMap.find(fsUnit);
    if (fsUnitIter != s_FsUnitToSkuConfigMap.end())
    {
        *pConfig = skuConfig.at(fsUnitIter->second);
    }
    else
    {
        Printf(Tee::PriError, "No FS config found for %s\n", ToString(fsUnit).c_str());
        return RC::ILWALID_ARGUMENT;
    }

    return rc;
}

RC DownbinImpl::AddSkuFsRules(const string &skuName)
{
    RC rc;
    map<string, FuseUtil::DownbinConfig> fuseConfig;
    CHECK_RC(m_pFuse->GetFuseConfig(skuName, &fuseConfig));
    for (auto &set : m_FsSets)
    {
        auto &fsSet = set.second;
        vector<FsUnit> fsUnits;
        fsSet.GetFsUnits(&fsUnits);

        // Apply SKU config rules
        for (const auto& fsUnit : fsUnits)
        {
            auto fsUnitIter = s_FsUnitToSkuConfigMap.find(fsUnit);
            if (fsUnitIter != s_FsUnitToSkuConfigMap.end())
            {
                if (fuseConfig.count(fsUnitIter->second))
                {
                    CHECK_RC(ApplyFsConfigRules(&fsSet, fsUnit, fuseConfig.at(fsUnitIter->second)));
                }
                else
                {
                    Printf(Tee::PriLow, "FsUnit %s doesn't exist in fuse config map.\n", ToString(fsUnit).c_str());
                }
            }
            else
            {
                Printf(Tee::PriLow, "No FS config found for %s\n", ToString(fsUnit).c_str());
            }
        }

        // Must-disable list
        // Must be the last set of "rules" to be applied, since it actually disables elements
        // All the dependencies between elements should be added to the FS sets before this
        for (const auto& fsUnit : fsUnits)
        {
            auto fsUnitIter = s_FsUnitToSkuConfigMap.find(fsUnit);
            if (fsUnitIter != s_FsUnitToSkuConfigMap.end())
            {
                if (fuseConfig.count(fsUnitIter->second))
                {
                    CHECK_RC(ApplyMustDisableListRules(&fsSet, fsUnit, fuseConfig.at(fsUnitIter->second)));
                }
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::AddGpcDownbinPickers(const string& skuName)
{
    RC rc;
    CHECK_RC(AddGpcSetGroupDisablePickers(skuName));
    CHECK_RC(AddGpcSetGroupReducePickers(skuName));
    CHECK_RC(AddGpcSetElementDisablePickers(skuName));

    return rc;
}

RC DownbinImpl::AddFbpDownbinPickers(const string& skuName)
{
    RC rc;
    CHECK_RC(AddFbpSetGroupDisablePickers(skuName));
    CHECK_RC(AddFbpSetGroupReducePickers(skuName));
    CHECK_RC(AddFbpSetElementDisablePickers(skuName));

    return rc;
}

RC DownbinImpl::AddMiscDownbinPickers()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC DownbinImpl::AddDownbinPickers(const string &skuName)
{
    RC rc;
    // Add protected bits group disable pickers to all the FsSets
    // These are L0 pickers of highest priority and should be added before other pickers
    CHECK_RC(AddProtectedBitsGDPickers(skuName));

    CHECK_RC(AddGpcDownbinPickers(skuName));
    CHECK_RC(AddFbpDownbinPickers(skuName));
    CHECK_RC(AddMiscDownbinPickers());
    return rc;
}

RC DownbinImpl::CheckMustEnableList
(
    FsUnit fsUnit,
    const string & skuName,
    bool *pIsProtected
)
{
    RC rc;
    MASSERT(pIsProtected);
    *pIsProtected = false;

    // only check for fsUnits in s_FsUnitToSkuConfigMap
    auto fsUnitIter = s_FsUnitToSkuConfigMap.find(fsUnit);
    if (fsUnitIter == s_FsUnitToSkuConfigMap.end())
    {
        return RC::OK;
    }

    if (m_MustEnElemPickerMap[fsUnit])
    {
         *pIsProtected = true;
    }

    FuseUtil::DownbinConfig downbinConfig;
    CHECK_RC(GetUnitDownbinConfig(skuName, fsUnit, &downbinConfig));
    if (downbinConfig.mustEnableList.size())
    {
        m_MustEnElemPickerMap[fsUnit] = true;
        *pIsProtected = true;
    }

    return rc;
}

RC DownbinImpl::AddProtectedBitsGDPickers(const string &skuName)
{
    RC rc;
    for (auto &set : m_FsSets)
    {
        auto &fsSet = set.second;
        vector<FsUnit> fsUnits;
        fsSet.GetFsUnits(&fsUnits);
        for (const auto& fsUnit : fsUnits)
        {
            bool isProtected;
            CHECK_RC(CheckMustEnableList(fsUnit, skuName, &isProtected));
            if (isProtected && fsSet.IsGroupFsUnit(fsUnit))
            {
                // Add picker to select groups which are not protected nor contain
                // protected elements for downbining.
                unique_ptr<DownbinGroupPicker> pPicker =
                    make_unique<MostAvailableGroupPicker>(&fsSet, FsUnit::NONE, false,
                            false);
                fsSet.AddGroupDisablePicker(move(pPicker));
            }
        }
    }
    return rc;
}

RC DownbinImpl::AddProtectedBitsGRPickers(FsSet *pFsSet, const string &skuName)
{
    RC rc;
    vector<FsUnit> fsUnits;
    pFsSet->GetElementFsUnits(&fsUnits);
    FsUnit groupFsUnit = pFsSet->GetGroupFsUnit();
    for (FsUnit fsUnit : fsUnits)
    {
        bool isProtected;
        CHECK_RC(CheckMustEnableList(fsUnit, skuName, &isProtected));
        // if the current FsUnit or the groupFsUnit of this FsSet is protected
        if (isProtected || m_MustEnElemPickerMap[groupFsUnit])
        {
            // Add picker to select groups containing least number of protected elements for
            // downbinning.
            unique_ptr<DownbinGroupPicker> pPicker =
                make_unique<MostAvailableGroupPicker>(pFsSet, fsUnit, false, true);
            pFsSet->AddGroupReducePicker(fsUnit, move(pPicker));
        }
    }
    return rc;
}

RC DownbinImpl::AddProtectedBitsEDPickers(FsSet *pFsSet, const string &skuName)
{
    RC rc;
    vector<FsUnit> fsUnits;
    pFsSet->GetElementFsUnits(&fsUnits);
    for (FsUnit fsUnit : fsUnits)
    {
        bool isProtected;
        CHECK_RC(CheckMustEnableList(fsUnit, skuName, &isProtected));
        if (isProtected)
        {
            // Add picker to choose non-protected elements in a specific group
            unique_ptr<DownbinElementPicker> pElementPicker =
                make_unique<AvailableElementPicker>(pFsSet, fsUnit);
            pFsSet->AddElementDisablePicker(fsUnit, move(pElementPicker));
        }
    }
    return rc;
}

RC  DownbinImpl::AddMostDisabledGDPickers
(
    FsSet *pFsSet,
    FsUnit elementFsUnit
)
{
    if (pFsSet->HasFsUnit(elementFsUnit))
    {
        unique_ptr<DownbinGroupPicker> pGdPicker =
            make_unique<MostDisabledGroupPicker>(pFsSet, elementFsUnit);
        // Add basic group disable picker for element
        pFsSet->AddGroupDisablePicker(move(pGdPicker));
    }
    return RC::OK;
}

RC  DownbinImpl::AddMostAvailableGRPickers
(
    FsSet *pFsSet,
    FsUnit elementFsUnit
)
{
    if (pFsSet->HasFsUnit(elementFsUnit))
    {
        unique_ptr<DownbinGroupPicker> pGrPicker =
            make_unique<MostAvailableGroupPicker>(pFsSet, elementFsUnit, true, false);
        // Add basic group reduce picker for each element
        pFsSet->AddGroupReducePicker(elementFsUnit, move(pGrPicker));
    }
    return RC::OK;
}

RC DownbinImpl::AddGpcSetGroupDisablePickers(const string &skuName)
{
    // Defined for each derived class explicitly
    return RC::UNSUPPORTED_FUNCTION;
}

RC DownbinImpl::AddGpcSetGroupReducePickers(const string &skuName)
{
    // Defined for each derived class explicitly
    return RC::UNSUPPORTED_FUNCTION;
}

RC DownbinImpl::AddGpcSetElementDisablePickers(const string &skuName)
{
    // Defined for each derived class explicitly
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC DownbinImpl::AddLtcL2SliceDependencyGRPicker()
{
    FsSet &fbpSet  = m_FsSets.at(FsUnit::FBP);

    // Add picker to select FBP containing least number of protected L2SLICEs for disabling
    // LTC
    unique_ptr<DownbinGroupPicker> pPicker =
        make_unique<MostAvailableGroupPicker>(&fbpSet, FsUnit::L2_SLICE, false, true);
    fbpSet.AddGroupReducePicker(FsUnit::LTC, move(pPicker));

    // Add picker to choose FBPs with most number of disabled L2SLICES for disabling LTC
    unique_ptr<DownbinGroupPicker> pGrPicker =
        make_unique<MostAvailableGroupPicker>(&fbpSet, FsUnit::L2_SLICE, true, false);
    fbpSet.AddGroupReducePicker(FsUnit::LTC, move(pGrPicker));

    return RC::OK;
}
//------------------------------------------------------------------------------
RC DownbinImpl::AddLtcL2SliceDependencyEDPicker()
{
    FsSet &fbpSet  = m_FsSets.at(FsUnit::FBP);

    unique_ptr<DownbinElementPicker> pElementPicker =
        make_unique<MostDisabledDependentElementPicker>(&fbpSet,
                                                        FsUnit::LTC,
                                                        FsUnit::L2_SLICE);
    fbpSet.AddElementDisablePicker(FsUnit::LTC, move(pElementPicker));
    return RC::OK;
}

//--------------------------------------------------------------------------------
RC DownbinImpl::AddFbpSetGroupDisablePickers(const string &skuName)
{
    // Defined for each derived class explicitly
    return RC::UNSUPPORTED_FUNCTION;
}
RC DownbinImpl::AddFbpSetGroupReducePickers(const string &skuName)
{
    // Defined for each derived class explicitly
    return RC::UNSUPPORTED_FUNCTION;
}
RC DownbinImpl::AddFbpSetElementDisablePickers(const string &skuName)
{
    // Defined for each derived class explicitly
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
void DownbinImpl::PrintAllSets(Tee::Priority pri) const
{
    for (const auto &set : m_FsSets)
    {
        const auto &fsSet = set.second;
        fsSet.PrintSet(pri);
    }
}

//------------------------------------------------------------------------------
RC DownbinImpl::GetFusePrefixSuffix
(
    const FsSet &fsSet,
    FsUnit fsUnit,
    FuseMaskType type,
    string* pPrefix,
    string* pSuffix
) const
{
    MASSERT(pPrefix);
    MASSERT(pSuffix);

    *pPrefix = "opt_";
    switch (type)
    {
        case FuseMaskType::DISABLE:
            *pSuffix = fsSet.IsDisableFuse(fsUnit) ? "_disable" : "_enable";
            break;
        case FuseMaskType::BOARD:
            *pSuffix = fsSet.IsDisableFuse(fsUnit) ? "_disable" : "_enable";
            *pPrefix += "board_";
            break;
        case FuseMaskType::DEFECTIVE:
            if (!fsSet.IsDisableFuse(fsUnit))
            {
                // No defective fuses for enable type fuses
                return RC::BAD_PARAMETER;
            }
            *pSuffix = "_defective";
            break;
        case FuseMaskType::RECONFIG:
            if (!fsSet.HasReconfig(fsUnit))
            {
                return RC::BAD_PARAMETER;
            }
            *pSuffix = "_reconfig";
            break;
        case FuseMaskType::DISABLE_CP:
            // No disable_cp for enable type fuses. So just return _enable
            *pSuffix = fsSet.IsDisableFuse(fsUnit) ? "_disable_cp" : "_enable";
            break;
        default:
            MASSERT("Unknown fuse mask type\n");
            break;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC DownbinImpl::GetElementFuseNames
(
    const FsSet &fsSet,
    FsUnit fsUnit,
    FuseMaskType type,
    vector<string> *pFuseNames
) const
{
    RC rc;
    MASSERT(pFuseNames);

    if (!fsSet.IsPerGroupFuse(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    string prefix, suffix;
    CHECK_RC(GetFusePrefixSuffix(fsSet, fsUnit, type, &prefix, &suffix));

    FsUnit groupFsUnit  = fsSet.GetGroupFsUnit();
    string groupFuseStr = fsSet.GetFuseName(groupFsUnit);
    string elementStr   = fsSet.GetFuseName(fsUnit);
    for (UINT32 groupIdx = 0; groupIdx < fsSet.GetNumGroups(); groupIdx++)
    {
        // Add disable fuses
        string fuseName = Utility::StrPrintf("%s%s_%s%u%s", prefix.c_str(),
                                                            elementStr.c_str(),
                                                            groupFuseStr.c_str(),
                                                            groupIdx,
                                                            suffix.c_str());
        pFuseNames->push_back(fuseName);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::GetFuseName
(
    const FsSet &fsSet,
    FsUnit fsUnit,
    FuseMaskType type,
    string *pFuseName
) const
{
    RC rc;
    MASSERT(pFuseName);

    string prefix, suffix;
    CHECK_RC(GetFusePrefixSuffix(fsSet, fsUnit, type, &prefix, &suffix));

    *pFuseName = prefix + fsSet.GetFuseName(fsUnit) + suffix;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC DownbinImpl::ApplyFuseOverrides(FuseMaskType maskType)
{
    RC rc;
    map<string, FuseOverride> overrideList;
    map<string, UINT32> fuseMap;
    for (auto &set : m_FsSets)
    {
        CHECK_RC(GetFuseMapFromFsSets(&fuseMap, maskType, set.first));
    }
    // Copy the content of fuseMap to overrideList
    for (const auto& fuse : fuseMap)
    {
        FuseOverride fuseOverride = {0};
        fuseOverride.value = fuse.second;
        overrideList[fuse.first] = fuseOverride;
    }
    Printf(Tee::PriNormal, "Applying fuse overrides - \n");
    for (const auto& fuse : overrideList)
    {
        Printf(Tee::PriNormal, "%30s - 0x%x\n", fuse.first.c_str(), fuse.second.value);
    }
    CHECK_RC(m_pFuse->SetOverrides(overrideList));
    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::GetFuseValuesFromFsSets
(
    FuseMaskType maskType,
    FuseValues *pRequest,
    const string& fsFusingOpt
)
{
    RC rc;
    MASSERT(pRequest);

    if (fsFusingOpt == "fb")
    {
        CHECK_RC(GetFuseMapFromFsSets(&(pRequest->m_NamedValues), maskType, FsUnit::FBP));
    }
    else if (fsFusingOpt == "gpc")
    {
        CHECK_RC(GetFuseMapFromFsSets(&(pRequest->m_NamedValues), maskType, FsUnit::GPC));
    }
    else if (fsFusingOpt == "all")
    {
        for (auto &set : m_FsSets)
        {
            CHECK_RC(GetFuseMapFromFsSets(&(pRequest->m_NamedValues), maskType, set.first));
        }
    }
    else
    {
        Printf(Tee::PriError, "Unsupported Fs Fusing Option %s\n", fsFusingOpt.c_str());
        return RC::ILWALID_ARGUMENT;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC DownbinImpl::ImportGpuFsInfo(bool bMarkDefective)
{
    RC rc;

    bool bIsSltFused = false;
    CHECK_RC(m_pDev->IsSltFused(&bIsSltFused));

    if (bMarkDefective)
    {
        m_DownbinSettings.bModifyDefective = true;
    }
    DEFER
    {
        m_DownbinSettings.bModifyDefective = false;
    };

    for (auto &set : m_FsSets)
    {
        auto& fsSet         = set.second;

        UINT32 numGroups    = fsSet.GetNumGroups();
        FsUnit groupFsUnit  = fsSet.GetGroupFsUnit();

        if (bIsSltFused)
        {
            string groupDefectiveFuseName;
            // Only need to read defective Fuse when bIsSltFused
            if (GetFuseName(fsSet, groupFsUnit, FuseMaskType::DEFECTIVE, &groupDefectiveFuseName) == RC::OK)
            {
                UINT32 fuseVal = 0;
                CHECK_RC(m_pFuse->GetFuseValue(groupDefectiveFuseName, FuseFilter::ALL_FUSES, &fuseVal));
                fsSet.DisableGroups(fuseVal);
            }
        }
        else
        {
            string groupDisableFuseName;
            string groupDisableCpFuseName;
            // Need to read disable Fuse and disable_cp when not bIsSltFused
            if (GetFuseName(fsSet, groupFsUnit, FuseMaskType::DISABLE, &groupDisableFuseName) == RC::OK &&
                GetFuseName(fsSet, groupFsUnit, FuseMaskType::DISABLE_CP, &groupDisableCpFuseName) == RC::OK)
            {
                UINT32 disableFuseVal = 0;
                UINT32 disableCpFuseVal = 0;
                CHECK_RC(m_pFuse->GetFuseValue(groupDisableFuseName, FuseFilter::ALL_FUSES, &disableFuseVal));
                CHECK_RC(m_pFuse->GetFuseValue(groupDisableCpFuseName, FuseFilter::ALL_FUSES, &disableCpFuseVal));
                fsSet.DisableGroups(disableFuseVal | disableCpFuseVal);
            }
        }

        vector<FsUnit> elementFsUnits;
        fsSet.GetElementFsUnits(&elementFsUnits);
        for (const auto& fsUnit : elementFsUnits)
        {
            if (fsSet.IsPerGroupFuse(fsUnit))
            {
                if (bIsSltFused)
                {
                    vector<string> elementDefectiveFuseNames;
                    // Only need to read defective Fuse when bIsSltFused
                    if (GetElementFuseNames(fsSet, fsUnit, FuseMaskType::DEFECTIVE, &elementDefectiveFuseNames) == RC::OK)
                    {
                        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
                        {
                            UINT32 fuseVal = 0;
                            CHECK_RC(m_pFuse->GetFuseValue(elementDefectiveFuseNames[groupIdx],
                                                        FuseFilter::ALL_FUSES,
                                                        &fuseVal));
                            CHECK_RC(fsSet.DisableElements(fsUnit, groupIdx, fuseVal));
                        }
                    }
                }
                else
                {
                    vector<string> elementDisableFuseNames;
                    vector<string> elementDisableCpFuseNames;
                    // Only need to read defective Fuse when bIsSltFused
                    if (GetElementFuseNames(fsSet, fsUnit, FuseMaskType::DISABLE, &elementDisableFuseNames) == RC::OK &&
                        GetElementFuseNames(fsSet, fsUnit, FuseMaskType::DISABLE_CP, &elementDisableCpFuseNames) == RC::OK)
                    {
                        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
                        {
                            UINT32 disableFuseVal = 0;
                            UINT32 disableCpFuseVal = 0;
                            CHECK_RC(m_pFuse->GetFuseValue(elementDisableFuseNames[groupIdx],
                                                        FuseFilter::ALL_FUSES,
                                                        &disableFuseVal));
                            CHECK_RC(m_pFuse->GetFuseValue(elementDisableCpFuseNames[groupIdx],
                                                        FuseFilter::ALL_FUSES,
                                                        &disableCpFuseVal));
                            CHECK_RC(fsSet.DisableElements(fsUnit, groupIdx, disableFuseVal | disableCpFuseVal));
                        }
                    }
                }
            }
            else
            {
                if (bIsSltFused)
                {
                    string elementDefectiveFuseName;
                    if (GetFuseName(fsSet, fsUnit, FuseMaskType::DEFECTIVE, &elementDefectiveFuseName) == RC::OK)
                    {
                        UINT32 fuseVal = 0;
                        CHECK_RC(m_pFuse->GetFuseValue(elementDefectiveFuseName,
                                                    FuseFilter::ALL_FUSES,
                                                    &fuseVal));

                        UINT32 numPerGroup = fsSet.GetNumElementsPerGroup(fsUnit);
                        UINT32 grpFullMask = fsSet.GetElementSet(0, fsUnit)->GetFsMask(FuseMaskType::DEFECTIVE)->GetFullMask();
                        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
                        {
                            UINT32 groupVal = (fuseVal >> (groupIdx * numPerGroup)) & grpFullMask;
                            CHECK_RC(fsSet.DisableElements(fsUnit, groupIdx, groupVal));
                        }
                    }
                }
                else
                {
                    string elementDisableFuseName;
                    string elementDisableCpFuseName;
                    if (GetFuseName(fsSet, fsUnit, FuseMaskType::DISABLE, &elementDisableFuseName) == RC::OK &&
                        GetFuseName(fsSet, fsUnit, FuseMaskType::DISABLE_CP, &elementDisableCpFuseName) == RC::OK)
                    {
                        UINT32 disableFuseVal = 0;
                        UINT32 disableCpFuseVal = 0;
                        CHECK_RC(m_pFuse->GetFuseValue(elementDisableFuseName,
                                                    FuseFilter::ALL_FUSES,
                                                    &disableFuseVal));
                        CHECK_RC(m_pFuse->GetFuseValue(elementDisableCpFuseName,
                                                    FuseFilter::ALL_FUSES,
                                                    &disableCpFuseVal));

                        UINT32 numPerGroup = fsSet.GetNumElementsPerGroup(fsUnit);
                        UINT32 grpFullMask = fsSet.GetElementSet(0, fsUnit)->GetFsMask(FuseMaskType::DISABLE)->GetFullMask();
                        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
                        {
                            UINT32 groupVal = ((disableFuseVal | disableCpFuseVal) >> (groupIdx * numPerGroup)) & grpFullMask;
                            CHECK_RC(fsSet.DisableElements(fsUnit, groupIdx, groupVal));
                        }
                    }
                }
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
string DownbinImpl::GetFsResultFile() const
{
    return m_FsResultFile.empty() ? s_DefaultFsResultFile : m_FsResultFile;
}

//------------------------------------------------------------------------------
// Format of FS result file -
// [
//   {
//      name: "",
//      time_begin: "",
//      result:
//      {
//          "Exit Code": 0,
//          "OPT_FBP_DISABLE": "0x0",
//          "OPT_FBIO_DISABLE": "0x0",
//          "OPT_LTC_DISABLE": "['0x0', '0x0', '0x0', '0x0', '0x0', '0x0']",
//          "OPT_FBP_DEFECTIVE": "0x0",
//          "OPT_FBIO_DEFECTIVE": "0x14",
//          "OPT_LTC_DEFECTIVE": "['0x0', '0x1', '0x0', '0x0', '0x0', '0x0']",
//           ....
//      },
//      time_end: ""
//   },
//   {
//      ....
//   }
//  ]

/* static */ RC DownbinImpl::ParseFsSections(const string& resultFile, map<string, FsSection> *pFsSections)
{
    RC rc;
    Document doc;
    MASSERT(pFsSections);

    CHECK_RC(ParseJson(resultFile, &doc));

    MASSERT(doc.IsArray());
    size_t numSections = doc.Size();
    for (UINT32 i = 0; i < numSections; i++)
    {
        const JsonTree& section = doc[i];

        string sectionName;
        CHECK_RC(FindJsolwal(section, JSON_VALUE_NAME, sectionName));
        if (sectionName == JSON_SECTION_GPU_INFO)
        {
            continue;
        }

        FsSection fsSection;
        fsSection.Name = Utility::ToLowerCase(sectionName);
        CHECK_RC(FindJsolwal(section, JSON_VALUE_TIME_START, fsSection.StartTime));
        CHECK_RC(FindJsolwal(section, JSON_VALUE_TIME_END, fsSection.EndTime));

        CHECK_RC(CheckHasSection(section, JSON_SECTION_RESULT));
        const JsonTree& results = section[JSON_SECTION_RESULT];
        for (auto resultIter = results.MemberBegin(); resultIter != results.MemberEnd(); resultIter++)
        {
            string itemName     = Utility::ToLowerCase(resultIter->name.GetString());
            const Value& values = resultIter->value;
            if (values.IsString())
            {
                string val = values.GetString();
                size_t pos = val.find("[");
                if (pos == std::string::npos)
                {
                    // Is a single value
                    fsSection.FuseVals[itemName] = { Utility::ColwertStrToHex(val) };
                }
                else
                {
                    // Is an array
                    // Remove all oclwrences of [ and '' in the string
                    size_t endPos   = val.find("]");
                    string arrayStr = val.substr(pos + 1, endPos - pos);
                    arrayStr.erase(std::remove(arrayStr.begin(), arrayStr.end(), '\''), arrayStr.end());

                    vector<string> arrayVals;
                    CHECK_RC(Utility::Tokenizer(arrayStr, ",", &arrayVals));

                    vector<UINT32> fuseVals;
                    for (auto arrayVal : arrayVals)
                    {
                        fuseVals.push_back(Utility::ColwertStrToHex(arrayVal));
                    }
                    fsSection.FuseVals[itemName] = std::move(fuseVals);
                }
            }
            else if (values.IsInt())
            {
                fsSection.FuseVals[itemName] = { static_cast<UINT32>(values.GetInt()) };
            }
            else
            {
                Printf(Tee::PriError, "Invalid format for %s in %s\n",
                                       itemName.c_str(), resultFile.c_str());
                return RC::ILWALID_INPUT;
            }
        }
        CHECK_RC(AddFsSectionToMle(fsSection));
        (*pFsSections)[fsSection.Name] = std::move(fsSection);
    }
    return rc;
}

//------------------------------------------------------------------------------
/* static */ RC DownbinImpl:: AddFsSectionToMle(const FsSection& fsSection)
{
    ByteStream bs;
    auto entry = Mle::Entry::floorsweep_info(&bs);
    entry.start_time(fsSection.StartTime)
         .end_time(fsSection.EndTime)
         .section_tag(fsSection.Name);

    for (const auto& fuse : fsSection.FuseVals)
    {
        entry.result()
             .fuse(fuse.first)
             .mask(fuse.second);
    }
    entry.Finish();
    Tee::PrintMle(&bs);

    return RC::OK;
}

//------------------------------------------------------------------------------
RC DownbinImpl::GetFsScriptInfo
(
    bool bIsRequired,
    map<string, FsSection> *pFsSections,
    bool *pFoundScript,
    RC *pExitCode
) const
{
    string resultFile = GetFsResultFile();
    if (!Xp::DoesFileExist(Utility::DefaultFindFile(resultFile, true)))
    {
        *pFoundScript = false;
        if (bIsRequired || !m_FsResultFile.empty())
        {
            Printf(Tee::PriError, "Could not find %s\n", resultFile.c_str());
            return RC::FILE_DOES_NOT_EXIST;
        }
        else
        {
            Printf(Tee::PriWarn, "Could not find %s. Skipping importing FS results\n",
                                  resultFile.c_str());
            return RC::OK;
        }
    }

    RC rc;
    *pFoundScript = true;
    CHECK_RC(ParseFsSections(resultFile, pFsSections));

    const auto& finalResults = (*pFsSections)[JSON_SECTION_FINAL].FuseVals;
    if (finalResults.find(JSON_VALUE_EXIT_CODE) != finalResults.end())
    {
        ErrorMap exitCode((finalResults.at("exit code"))[0]);
        *pExitCode = exitCode.Code();
        if (*pExitCode != RC::OK)
        {
            Printf(Tee::PriNormal, "%s reports a floorsweeping error code : %u (%s)\n",
                                    resultFile.c_str(),
                                    static_cast<UINT32>(*pExitCode),
                                    exitCode.Message());
        }
        else
        {
            Printf(Tee::PriNormal, "%s reports a floorsweeping success\n",
                                    resultFile.c_str());
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::ApplyFsScriptInfo
(
    const map<string, FsSection> &fsSections,
    bool bMarkDefective
)
{
    RC rc;

    string resultFile = GetFsResultFile();
    if (bMarkDefective)
    {
        m_DownbinSettings.bModifyDefective = true;
    }
    DEFER
    {
        m_DownbinSettings.bModifyDefective = false;
    };

    const auto& finalResults = fsSections.at("final").FuseVals;
    for (auto &set : m_FsSets)
    {
        auto& fsSet = set.second;

        string groupFuseName;
        FsUnit groupFsUnit  = fsSet.GetGroupFsUnit();
        CHECK_RC(GetFuseName(fsSet, groupFsUnit, FuseMaskType::DEFECTIVE, &groupFuseName));
        auto fsResultIter = finalResults.find(Utility::ToLowerCase(groupFuseName));
        if (fsResultIter == finalResults.end())
        {
            CHECK_RC(GetFuseName(fsSet, groupFsUnit, FuseMaskType::DISABLE, &groupFuseName));
            fsResultIter = finalResults.find(Utility::ToLowerCase(groupFuseName));
        }
        if (fsResultIter == finalResults.end())
        {
            Printf(Tee::PriLow, "Could not find entry for %s in %s\n",
                                 ToString(groupFsUnit).c_str(), resultFile.c_str());
        }

        else
        {
            MASSERT(fsResultIter->second.size() == 1);
            UINT32 disableMask = fsResultIter->second[0];
            CHECK_RC(fsSet.DisableGroups(disableMask));
        }

        vector<FsUnit> elementFsUnits;
        fsSet.GetElementFsUnits(&elementFsUnits);
        for (const auto& fsUnit : elementFsUnits)
        {
            UINT32 numGroups = fsSet.GetNumGroups();

            // These are not actual fuse names and don't have different names for per group fuses
            string elFuseName = "opt_" + ToString(fsUnit) + "_defective";
            auto fsElResultIter = finalResults.find(Utility::ToLowerCase(elFuseName));
            if (fsElResultIter == finalResults.end())
            {
                elFuseName = "opt_" + ToString(fsUnit) + "_disable";
                fsElResultIter = finalResults.find(Utility::ToLowerCase(elFuseName));
            }
            if (fsElResultIter == finalResults.end())
            {
                Printf(Tee::PriLow, "Could not find entry for %s in %s\n",
                                     ToString(fsUnit).c_str(), resultFile.c_str());
            }
            else
            {
                if (fsElResultIter->second.size() == 1)
                {
                    // Split them into per group fuses to apply the masks appropraitely
                    UINT32 fuseVal       = (fsElResultIter->second)[0];
                    UINT32 numElPerGroup = fsSet.GetNumElementsPerGroup(fsUnit);
                    UINT32 groupElMask   = (1 << numElPerGroup) - 1;
                    vector<UINT32> splitVals(numGroups);
                    for (UINT32 group = 0; group < numGroups; group++)
                    {
                        splitVals[group] = fuseVal & groupElMask;
                        fuseVal >>= numElPerGroup;
                    }
                    CHECK_RC(fsSet.DisableElements(fsUnit, splitVals));
                }
                else
                {
                    // Already split masks, disable as is
                    CHECK_RC(fsSet.DisableElements(fsUnit, fsElResultIter->second));
                }
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::ImportFsScriptInfo(bool bMarkDefective)
{
    RC rc;
    map<string, FsSection> fsSections;

    RC scriptExitCode;
    bool bFoundScript = false;
    CHECK_RC(GetFsScriptInfo(false, &fsSections, &bFoundScript, &scriptExitCode));
    if (scriptExitCode != RC::OK)
    {
        return scriptExitCode;
    }
    else if (bFoundScript)
    {
        CHECK_RC(ApplyFsScriptInfo(fsSections, bMarkDefective));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::DumpFsScriptInfo()
{
    RC rc;
    CHECK_RC(Initialize());

    string resultFile = GetFsResultFile();

    RC scriptExitCode;
    bool bFoundScript = false;
    map<string, FsSection> fsSections;
    CHECK_RC(GetFsScriptInfo(true, &fsSections, &bFoundScript, &scriptExitCode));

    const UINT32 numAllowedRcs = 7;
    const RC allowedRcs[numAllowedRcs] =
    {
        RC::INCORRECT_L2SLICES,
        RC::INCORRECT_LTC,
        RC::INCORRECT_TPC,
        RC::INCORRECT_PES,
        RC::INCORRECT_GPC,
        RC::CANNOT_MEET_FS_REQUIREMENTS,
        RC::FS_SANITY_FAILURE
    };
    if (scriptExitCode != RC::OK)
    {
        bool bIsAllowed = false;
        for (UINT32 i = 0; i < numAllowedRcs; i++)
        {
            if (scriptExitCode == allowedRcs[i])
            {
                Printf(Tee::PriWarn, "%s results may be incomplete\n", resultFile.c_str());
                bIsAllowed = true;
            }
        }
        if (!bIsAllowed)
        {
            // Return early
            return scriptExitCode;
        }
    }

    // Current existing defectives on the chip
    CHECK_RC(ImportGpuFsInfo(true));

    Printf(Tee::PriLow, "\n-- After GPU Import--\n");
    PrintAllSets(Tee::PriLow);

    // FS script results ; but don't mark as defective to differentiate
    CHECK_RC(ApplyFsScriptInfo(fsSections, false));
    Printf(Tee::PriLow, "\n-- After FS Import --\n");
    PrintAllSets(Tee::PriLow);

    string gpuDefects;
    string fsDefects;
    string finalDefects;
    UINT32 gpuMask, fsMask;
    map<string, vector<UINT32>> fsDisableFuseVals;
    map<string, vector<UINT32>> fsEggDefectiveFuseVals;

    for (const auto &set : m_FsSets)
    {
        const auto &fsSet = set.second;

        UINT32 numGroups    = fsSet.GetNumGroups();
        FsUnit groupFsUnit  = fsSet.GetGroupFsUnit();
        const FsElementSet* pGroupElement = fsSet.GetGroupElementSet();

        string fsUnitName = Utility::ToUpperCase(ToString(groupFsUnit));
        gpuMask = pGroupElement->GetFsMask(FuseMaskType::DEFECTIVE)->GetMask();
        fsMask  = pGroupElement->GetFsMask(FuseMaskType::DISABLE)->GetMask();
        gpuDefects   += Utility::StrPrintf("%-10s : [ 0x%x ]\n", fsUnitName.c_str(), gpuMask);
        fsDefects    += Utility::StrPrintf("%-10s : [ 0x%x ]\n", fsUnitName.c_str(),
                                            ~gpuMask & fsMask);
        finalDefects += Utility::StrPrintf("%-10s : [ 0x%x ]\n", fsUnitName.c_str(), fsMask);
        fsDisableFuseVals[fsUnitName].push_back(gpuMask);
        fsEggDefectiveFuseVals[fsUnitName].push_back(~gpuMask & fsMask);

        vector<FsUnit> elementFsUnits;
        fsSet.GetElementFsUnits(&elementFsUnits);
        for (const auto& fsUnit : elementFsUnits)
        {
            fsUnitName = Utility::ToUpperCase(ToString(fsUnit));
            string elementTag = Utility::StrPrintf("%-10s : [", fsUnitName.c_str());
            gpuDefects   += elementTag;
            fsDefects    += elementTag;
            finalDefects += elementTag;
            if (fsSet.IsPerGroupFuse(fsUnit))
            {
                for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
                {
                    const FsElementSet *pElementSet = fsSet.GetElementSet(groupIdx, fsUnit);
                    gpuMask = pElementSet->GetFsMask(FuseMaskType::DEFECTIVE)->GetMask();
                    fsMask  = pElementSet->GetFsMask(FuseMaskType::DISABLE)->GetMask();

                    gpuDefects   += Utility::StrPrintf(" 0x%x", gpuMask);
                    fsDefects    += Utility::StrPrintf(" 0x%x", ~gpuMask & fsMask);
                    finalDefects += Utility::StrPrintf(" 0x%x", fsMask);
                    fsDisableFuseVals[fsUnitName].push_back(gpuMask);
                    fsEggDefectiveFuseVals[fsUnitName].push_back(~gpuMask & fsMask);
                }
            }
            else
            {
                const UINT32 numPerGroup = fsSet.GetNumElementsPerGroup(fsUnit);

                gpuMask = 0;
                fsMask  = 0;
                for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
                {
                    const FsElementSet *pElementSet = fsSet.GetElementSet(groupIdx, fsUnit);
                    gpuMask |= (pElementSet->GetFsMask(FuseMaskType::DEFECTIVE)->GetMask() <<
                               (groupIdx * numPerGroup));
                    fsMask  |= (pElementSet->GetFsMask(FuseMaskType::DISABLE)->GetMask() <<
                               (groupIdx * numPerGroup));
                }
                gpuDefects   += Utility::StrPrintf(" 0x%x", gpuMask);
                fsDefects    += Utility::StrPrintf(" 0x%x", ~gpuMask & fsMask);
                finalDefects += Utility::StrPrintf(" 0x%x", fsMask);
                fsDisableFuseVals[fsUnitName].push_back(gpuMask);
                fsEggDefectiveFuseVals[fsUnitName].push_back(~gpuMask & fsMask);
            }
            gpuDefects   += " ]\n";
            fsDefects    += " ]\n";
            finalDefects += " ]\n";
        }
    }
    Printf(Tee::PriNormal, "===== Pre-existing disable masks =====\n%s\n", gpuDefects.c_str());
    Printf(Tee::PriNormal, "===== FS script defectives masks =====\n%s\n", fsDefects.c_str());
    Printf(Tee::PriNormal, "===== Final disable masks =====\n%s\n",        finalDefects.c_str());

    FsSection fsSection;
    fsSection.StartTime = "";
    fsSection.EndTime = "";
    fsSection.Name = "Pre-existingMasks";
    fsSection.FuseVals = fsDisableFuseVals;
    CHECK_RC(AddFsSectionToMle(fsSection));
    fsSection.Name = "FloorsweepEgg-Defectives";
    fsSection.FuseVals = fsEggDefectiveFuseVals;
    CHECK_RC(AddFsSectionToMle(fsSection));

    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::VerifyDownbinConfig(const string& skuname)
{
    // TODO
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC DownbinImpl::IsFsValid(FsLib::FsMode fsMode)
{
#ifdef INCLUDE_FSLIB
    RC rc;
    string fsError = "";
    CHECK_RC(FsLib::SetMode(fsMode));
    FsLib::GpcMasks gpcMasks = {};
    FsLib::FbpMasks fbpMasks = {};
    CHECK_RC(GetFsLibGpcMask(&gpcMasks));
    CHECK_RC(GetFsLibFbpMask(&fbpMasks));
    // Check with Arch FS library
    if (!FsLib::IsFloorsweepingValid(m_pDev->GetDeviceId(), gpcMasks, fbpMasks, fsError))
    {
        Printf(Tee::PriError, "Requested floorsweeping invalid.\n"
                              "FsLib Error : %s\n", fsError.c_str());
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }
    return RC::OK;
#else
    Printf(Tee::PriLow, "FsLib not included. Bypassing FS validity check\n");
    return RC::OK;
#endif
}

RC DownbinImpl::GetFsLibGpcMask(FsLib::GpcMasks *pGpcMasks) const
{
    if (!m_FsSets.count(FsUnit::GPC))
    {
        Printf(Tee::PriError, "No GPC FsSet exists. Can not extract GPC FsSet info\n");
        return RC::SOFTWARE_ERROR;
    }
    const FsSet &fsSet = m_FsSets.at(FsUnit::GPC);
    UINT32 numGroups = fsSet.GetNumGroups();
    const FsElementSet* pGroupElementSet = fsSet.GetGroupElementSet();
    UINT32 groupElementFsMask  = pGroupElementSet->GetFsMask(FuseMaskType::DISABLE)->GetMask();
    pGpcMasks->gpcMask = groupElementFsMask;
    vector<FsUnit> elementFsUnits;
    fsSet.GetElementFsUnits(&elementFsUnits);
    for (const auto& fsUnit : elementFsUnits)
    {
        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
        {
            const FsElementSet *pElementSet = fsSet.GetElementSet(groupIdx, fsUnit);
            UINT32 fsMask = pElementSet->GetFsMask(FuseMaskType::DISABLE)->GetMask();
            switch (fsUnit)
            {
                case FsUnit::PES:
                    pGpcMasks->pesPerGpcMasks[groupIdx] = fsMask;
                    break;
                case FsUnit::TPC:
                    pGpcMasks->tpcPerGpcMasks[groupIdx] = fsMask;
                    break;
                case FsUnit::ROP:
                    pGpcMasks->ropPerGpcMasks[groupIdx] = fsMask;
                    break;
                case FsUnit::CPC:
                    pGpcMasks->cpcPerGpcMasks[groupIdx] = fsMask;
                    break;
                default:
                    Printf(Tee::PriError, "Wrong FsUnit Type %s for GPC Group\n", ToString(fsUnit).c_str());
                    return RC::SOFTWARE_ERROR;
            }
        }
    }
    return RC::OK;
}

RC DownbinImpl::GetFsLibFbpMask(FsLib::FbpMasks *pFbpMasks) const
{
    if (!m_FsSets.count(FsUnit::FBP))
    {
        Printf(Tee::PriError, "No FBP FsSet exists. Can not extract FBP FsSet info\n");
        return RC::SOFTWARE_ERROR;
    }
    const FsSet &fsSet = m_FsSets.at(FsUnit::FBP);
    UINT32 numGroups = fsSet.GetNumGroups();
    const FsElementSet* pGroupElementSet = fsSet.GetGroupElementSet();
    UINT32 groupElementFsMask  = pGroupElementSet->GetFsMask(FuseMaskType::DISABLE)->GetMask();
    pFbpMasks->fbpMask = groupElementFsMask;
    vector<FsUnit> elementFsUnits;
    fsSet.GetElementFsUnits(&elementFsUnits);
    for (const auto& fsUnit : elementFsUnits)
    {
        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
        {
            const FsElementSet *pElementSet = fsSet.GetElementSet(groupIdx, fsUnit);
            UINT32 fsMask = pElementSet->GetFsMask(FuseMaskType::DISABLE)->GetMask();
            switch (fsUnit)
            {
                case FsUnit::FBIO:
                    pFbpMasks->fbioPerFbpMasks[groupIdx] = fsMask;
                    break;
                case FsUnit::FBPA:
                    pFbpMasks->fbpaPerFbpMasks[groupIdx] = fsMask;
                    break;
                case FsUnit::HALF_FBPA:
                    pFbpMasks->halfFbpaPerFbpMasks[groupIdx] = fsMask;
                    break;
                case FsUnit::LTC:
                    pFbpMasks->ltcPerFbpMasks[groupIdx] = fsMask;
                    break;
                case FsUnit::L2_SLICE:
                    pFbpMasks->l2SlicePerFbpMasks[groupIdx] = fsMask;
                    break;
                default:
                    Printf(Tee::PriError, "Wrong FsUnit Type %s for FBP Group\n", ToString(fsUnit).c_str());
                    return RC::SOFTWARE_ERROR;
            }
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC DownbinImpl::DownbinSku(const string &skuName)
{
    RC rc;
    CHECK_RC(Initialize());

    if (!Downbin::IsDownbinFlagSet(m_DownbinInfo.downbinFlags, DownbinFlag::IGNORE_SKU_FS))
    {
        CHECK_RC(AddSkuFsRules(skuName));
        Printf(Tee::PriNormal, "\n-- After applying SKU rules--\n");
        PrintAllSets(Tee::PriNormal);
    }

    // If import defective
    if (!Downbin::IsDownbinFlagSet(m_DownbinInfo.downbinFlags, DownbinFlag::SKIP_GPU_IMPORT))
    {
        CHECK_RC(ImportGpuFsInfo(true));
        Printf(Tee::PriNormal, "\n-- After GPU Import--\n");
        PrintAllSets(Tee::PriNormal);
    }

    // If import from sweep.json
    if (!Downbin::IsDownbinFlagSet(m_DownbinInfo.downbinFlags, DownbinFlag::SKIP_FS_IMPORT))
    {
        CHECK_RC(ImportFsScriptInfo(true));
        Printf(Tee::PriNormal, "\n-- After FS Import --\n");
        PrintAllSets(Tee::PriNormal);
    }

    map<string, FuseUtil::DownbinConfig> fuseConfig;
    CHECK_RC(m_pFuse->GetFuseConfig(skuName, &fuseConfig));
    // Add pickers for various fs sets
    CHECK_RC(AddDownbinPickers(skuName));

    // Run downbin
    if (!Downbin::IsDownbinFlagSet(m_DownbinInfo.downbinFlags, DownbinFlag::SKIP_DOWN_BIN))
    {
        CHECK_RC(DownbinSet(&m_FsSets.at(FsUnit::GPC), fuseConfig));
        CHECK_RC(DownbinSet(&m_FsSets.at(FsUnit::FBP), fuseConfig));
        if (m_FsSets.count(FsUnit::LWDEC))
        {
            CHECK_RC(DownbinSet(&m_FsSets.at(FsUnit::LWDEC), fuseConfig));
        }
        if (m_FsSets.count(FsUnit::LWJPG))
        {
            CHECK_RC(DownbinSet(&m_FsSets.at(FsUnit::LWJPG), fuseConfig));
        }
        // Downbin syspipes after gpc downbinning
        if (m_FsSets.count(FsUnit::SYSPIPE))
        {
            CHECK_RC(DownbinSyspipeSet(fuseConfig.at("sys_pipe")));
        }

        Printf(Tee::PriNormal, "\n-- After Downbin --\n");
        PrintAllSets(Tee::PriNormal);
    }

    // Run post downbin rules
    CHECK_RC(ApplyPostDownbinRules(skuName));

    // Check if FS is valid against FsLib
    CHECK_RC(IsFsValid(m_DownbinInfo.fsMode));

    // Add in User defined Overrides from -fuse_override
    // Can change the value of ATE Fuse
    if (!m_DownbinInfo.fuseOverrideList.empty())
    {
        Printf(Tee::PriNormal, "Applying user defined fuse overrides - \n");
        for (const auto& fuse : m_DownbinInfo.fuseOverrideList)
        {
            Printf(Tee::PriNormal, "%30s - 0x%x\n", fuse.first.c_str(), fuse.second.value);
        }
        CHECK_RC(m_pFuse->SetOverrides(m_DownbinInfo.fuseOverrideList));
    }

    bool bUseBoardFs = Downbin::IsDownbinFlagSet(m_DownbinInfo.downbinFlags, DownbinFlag::USE_BOARD_FS);
    if (bUseBoardFs && !m_HasBoardFsFuses)
    {
        Printf(Tee::PriError, "Board FS fuses not available on this chip\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Apply fuse overrides
    // If write to defective
    if (!Downbin::IsDownbinFlagSet(m_DownbinInfo.downbinFlags, DownbinFlag::SKIP_DEFECTIVE_FUSE) &&
        !bUseBoardFs)
    {
        CHECK_RC(ApplyFuseOverrides(FuseMaskType::DEFECTIVE));
    }
    if (bUseBoardFs)
    {
        CHECK_RC(ApplyFuseOverrides(FuseMaskType::BOARD));
    }
    else
    {
        CHECK_RC(ApplyFuseOverrides(FuseMaskType::DISABLE));
        CHECK_RC(ApplyFuseOverrides(FuseMaskType::RECONFIG));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::GetOnlyFsFuseInfo(FuseValues* pRequest, const string& fsFusingOpt)
{
    MASSERT(pRequest);
    RC rc;
    if (!m_bIsInitialized)
    {
        CHECK_RC(Initialize());
    }

    // If import defective
    if (!Downbin::IsDownbinFlagSet(m_DownbinInfo.downbinFlags, DownbinFlag::SKIP_GPU_IMPORT))
    {
        CHECK_RC(ImportGpuFsInfo(true));
        Printf(Tee::PriNormal, "\n-- After GPU Import--\n");
        PrintAllSets(Tee::PriNormal);
    }

    // Import from sweep.json
    CHECK_RC(ImportFsScriptInfo(true));
    Printf(Tee::PriNormal, "\n-- After FS Import --\n");
    PrintAllSets(Tee::PriNormal);

    bool bUseBoardFs = Downbin::IsDownbinFlagSet(m_DownbinInfo.downbinFlags, DownbinFlag::USE_BOARD_FS);
    if (bUseBoardFs && !m_HasBoardFsFuses)
    {
        Printf(Tee::PriError, "Board FS fuses not available on this chip\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    pRequest->Clear();
    // Populate defective fuses
    if (!(Downbin::IsDownbinFlagSet(m_DownbinInfo.downbinFlags, DownbinFlag::SKIP_DEFECTIVE_FUSE) ||
          bUseBoardFs))
    {
        CHECK_RC(GetFuseValuesFromFsSets(FuseMaskType::DEFECTIVE, pRequest, fsFusingOpt));
    }
    // Populate disable / board FS fuses
    if (bUseBoardFs)
    {
        CHECK_RC(GetFuseValuesFromFsSets(FuseMaskType::BOARD, pRequest, fsFusingOpt));
    }
    else
    {
        CHECK_RC(GetFuseValuesFromFsSets(FuseMaskType::DISABLE, pRequest, fsFusingOpt));
    }

    // Add in User defined overrides from -fuse_override
    map<string, UINT32> &namedValues = pRequest->m_NamedValues;
    // Extract info from fuseOverrideList, which is map<string, FuseOverride>.
    for (const auto &fuseOverrideEntry : m_DownbinInfo.fuseOverrideList)
    {
        namedValues[fuseOverrideEntry.first] = fuseOverrideEntry.second.value;
    }

    CHECK_RC(IsFsValid(m_DownbinInfo.fsMode));

    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::GetFuseMapFromFsSets
(
    map<string, UINT32>* pFuseMap,
    FuseMaskType maskType,
    FsUnit desiredFsUnit
) const
{
    MASSERT(pFuseMap);
    if (!m_FsSets.count(desiredFsUnit))
    {
        Printf(Tee::PriError, "FsUnit %s doesn't exist in FsSets.\n", ToString(desiredFsUnit).c_str());
        return RC::SOFTWARE_ERROR;
    }
    const auto &fsSet = m_FsSets.at(desiredFsUnit);
    UINT32 numGroups    = fsSet.GetNumGroups();
    const FsElementSet* pGroupElement = fsSet.GetGroupElementSet();

    string groupFuseName;
    FuseMaskType getMaskType = maskType;
    // If mask type == BOARD, we need to copy the disable fuses into the board fuses
    if (maskType == FuseMaskType::BOARD)
    {
        getMaskType = FuseMaskType::DISABLE;
    }

    if (GetFuseName(fsSet, desiredFsUnit, maskType, &groupFuseName) == RC::OK)
    {
        groupFuseName = Utility::ToUpperCase(groupFuseName);
        (*pFuseMap)[groupFuseName] = pGroupElement->GetFsMask(getMaskType)->GetMask();
    }

    vector<FsUnit> elementFsUnits;
    fsSet.GetElementFsUnits(&elementFsUnits);
    for (const auto& fsUnit : elementFsUnits)
    {
        if (fsSet.IsPerGroupFuse(fsUnit))
        {
            vector<string> elementFuseNames;
            if (GetElementFuseNames(fsSet, fsUnit, maskType, &elementFuseNames) == RC::OK)
            {
                for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
                {
                    const FsElementSet *pElementSet = fsSet.GetElementSet(groupIdx, fsUnit);
                    string fuseName = Utility::ToUpperCase(elementFuseNames[groupIdx]);
                    (*pFuseMap)[fuseName] = pElementSet->GetFsMask(getMaskType)->GetMask();
                }
            }
        }
        else
        {
            string fuseName;
            if (GetFuseName(fsSet, fsUnit, maskType, &fuseName) == RC::OK)
            {
                const UINT32 numPerGroup = fsSet.GetNumElementsPerGroup(fsUnit);
                for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
                {
                    const FsElementSet *pElementSet = fsSet.GetElementSet(groupIdx, fsUnit);
                    fuseName = Utility::ToUpperCase(fuseName);
                    (*pFuseMap)[fuseName] |= (pElementSet->GetFsMask(getMaskType)->GetMask() <<
                                            (groupIdx * numPerGroup));
                }
            }
        }
    }

    Printf(Tee::PriDebug, "Getting %s FuseMap - \n", ToString(desiredFsUnit).c_str());
    for (const auto& fuse : (*pFuseMap))
    {
        Printf(Tee::PriDebug, "%30s - 0x%x\n", fuse.first.c_str(), fuse.second);
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC DownbinImpl::DownbinSet(FsSet* pFsSet, const map<string, FuseUtil::DownbinConfig>& fuseConfig)
{
    MASSERT(pFsSet);
    RC rc;

    const FuseUtil::DownbinConfig& groupConfig = fuseConfig.at(ToString(pFsSet->GetGroupFsUnit()));
    vector<FsUnit> elementFsUnits;
    pFsSet->GetElementFsUnits(&elementFsUnits);
    map<FsUnit, FuseUtil::DownbinConfig> elementConfigs;
    for (const auto &elementFsUnit : elementFsUnits)
    {
        auto fsUnitIter = s_FsUnitToSkuConfigMap.find(elementFsUnit);
        if (fsUnitIter != s_FsUnitToSkuConfigMap.end())
        {
            if (fuseConfig.count(fsUnitIter->second))
            {
                elementConfigs[elementFsUnit] = fuseConfig.at(fsUnitIter->second);
            }
            else
            {
                Printf(Tee::PriLow, "FsUnit %s doesn't exist in fuse config map.\n", ToString(elementFsUnit).c_str());
            }
        }
        else
        {
            Printf(Tee::PriLow, "No FS config found for %s\n", ToString(elementFsUnit).c_str());
        }
    }

    // Check for ValMatchHex first so we don't disable arbitrary
    // bits when we know which bits are supposed to be disabled.
    if (!groupConfig.isBurnCount)
    {
        CHECK_RC(pFsSet->DisableGroups(groupConfig.matchValues[0]));
    }

    for (const auto &elementEntry : elementConfigs)
    {
        const FsUnit elementFsUnit = elementEntry.first;
        const FuseUtil::DownbinConfig &elementConfig = elementEntry.second;
        if (!elementConfig.isBurnCount)
        {
            CHECK_RC(pFsSet->DisableElements(elementFsUnit, elementConfig.matchValues));
        }
    }

    // Down-bin to meet the requested BurnCount(s)
    if (groupConfig.isBurnCount)
    {
        UINT32 lwrrentDisableCount = 0;
        CHECK_RC(pFsSet->GetTotalDisabledElements(pFsSet->GetGroupFsUnit(), &lwrrentDisableCount));
        if (groupConfig.disableCount != -1)
        {
            while (lwrrentDisableCount < static_cast<UINT32>(groupConfig.disableCount))
            {
                CHECK_RC(pFsSet->DisableOneGroup());
                CHECK_RC(pFsSet->GetTotalDisabledElements(pFsSet->GetGroupFsUnit(), &lwrrentDisableCount));
            }
        }
    }

    for (const auto &elementEntry : elementConfigs)
    {
        const FsUnit elementFsUnit = elementEntry.first;
        const FuseUtil::DownbinConfig &elementConfig = elementEntry.second;
        if (elementConfig.isBurnCount && elementConfig.disableCount != -1)
        {
            UINT32 burnCount = elementConfig.disableCount - elementConfig.repairMaxCount;
            UINT32 lwrrentDisableCount = 0;
            CHECK_RC(pFsSet->GetTotalDisabledElements(elementFsUnit, &lwrrentDisableCount));
            while (lwrrentDisableCount < burnCount)
            {
                CHECK_RC(pFsSet->DisableOneElement(elementFsUnit));
                CHECK_RC(pFsSet->GetTotalDisabledElements(elementFsUnit, &lwrrentDisableCount));
            }
        }
    }

    // Deal with Reconfig
    for (const auto &elementEntry : elementConfigs)
    {
        const FsUnit elementFsUnit = elementEntry.first;
        const FuseUtil::DownbinConfig &elementConfig = elementEntry.second;
        if (elementConfig.isBurnCount && elementConfig.hasReconfig)
        {
            UINT32 burnCount = elementConfig.reconfigCount;
            m_DownbinSettings.bModifyReconfig = true;
            UINT32 lwrrentReconfigCount = 0;
            CHECK_RC(pFsSet->GetTotalReconfigElements(elementFsUnit, &lwrrentReconfigCount));
            while (lwrrentReconfigCount < burnCount)
            {
                CHECK_RC(pFsSet->DisableOneElement(elementFsUnit));
                CHECK_RC(pFsSet->GetTotalReconfigElements(elementFsUnit, &lwrrentReconfigCount));
            }
            m_DownbinSettings.bModifyReconfig = false;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC DownbinImpl::DownbinSyspipeSet(const FuseUtil::DownbinConfig &downbinConfig)
{
    return RC::UNSUPPORTED_FUNCTION;
}
//------------------------------------------------------------------------------
RC DownbinImpl::ApplyPostDownbinRules(const string &skuName)
{
    return RC::OK;
}
