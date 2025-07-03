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

#include "core/include/utility.h"

#include "downbingrouppickers.h"
#include "fsset.h"

using namespace Downbin;

//------------------------------------------------------------------------------
// MostDisabledGroupPicker

MostDisabledGroupPicker::MostDisabledGroupPicker
(
    FsSet *pFsSet,
    FsUnit fsUnit
)
    : m_pFsSet(pFsSet)
    , m_ElementFsUnit(fsUnit)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->HasElementFsUnit(fsUnit));
}

string MostDisabledGroupPicker::GetPickerName() const
{
    string name = Utility::StrPrintf("MostDisabledGroupPicker-%s",
                                      Downbin::ToString(m_ElementFsUnit).c_str());
    return name;
}

RC MostDisabledGroupPicker::Pick
(
    const FsMask& inGroupMask,
    FsMask *pOutGroupMask
) const
{
    MASSERT(pOutGroupMask);

    UINT32 numGroups = m_pFsSet->GetNumGroups();
    UINT32 maxDisableCount = 0;
    pOutGroupMask->ClearAll();
    for (INT32 grpIdx = Utility::BitScanForward(inGroupMask.GetMask());
         grpIdx >= 0;
         grpIdx = Utility::BitScanForward(inGroupMask.GetMask(), grpIdx + 1))
    {
        if (static_cast<UINT32>(grpIdx) >= numGroups)
        {
            break;
        }

        const FsMask *pMask = m_pFsSet->GetElementSet(grpIdx, m_ElementFsUnit)->GetTotalDisableMask();
        UINT32 disableCount = pMask->GetNumSetBits();
        if (disableCount == maxDisableCount)
        {
            // Equally valid choice
            pOutGroupMask->SetBits(1 << grpIdx);
        }
        else if (disableCount > maxDisableCount)
        {
            // Greater disable count => discard previously chosen groups
            pOutGroupMask->SetMask(1 << grpIdx);
            maxDisableCount = disableCount;
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// MostEnabledUgpuPicker

MostEnabledUgpuPicker::MostEnabledUgpuPicker
(
    TestDevice *pDev,
    FsSet *pFsSet,
    FsUnit fsUnit
)
    : m_pDev(pDev)
    , m_pFsSet(pFsSet)
    , m_FsUnit(fsUnit)
{
    MASSERT(pDev);
    MASSERT(pFsSet);

    MASSERT(pFsSet->HasFsUnit(fsUnit));
}

string MostEnabledUgpuPicker::GetPickerName() const
{
    string name = Utility::StrPrintf("MostEnabledUgpuPicker-%s",
                                      Downbin::ToString(m_FsUnit).c_str());
    return name;
}

RC MostEnabledUgpuPicker::Pick
(
    const FsMask& inGroupMask,
    FsMask *pOutGroupMask
) const
{
    RC rc;
    MASSERT(pOutGroupMask);
    // TODO: check that inGroupMask is contained in m_pFsSet

    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    UINT32 maxEnableCount = 0;
    pOutGroupMask->ClearAll();

    vector<FsMask> uGpuGroupEnMasks;
    CHECK_RC(Downbin::GetUGpuGroupEnMasks(*pSubdev, m_pFsSet->GetGroupFsUnit(), inGroupMask,
                &uGpuGroupEnMasks));
    FsMask ugpuMask;
    ugpuMask.SetMask(pSubdev->GetUGpuMask());
    if (ugpuMask.GetNumSetBits() <= 0)
    {
        Printf(Tee::PriError, "Invalid UGPU count : %d\n", ugpuMask.GetNumSetBits());
        return RC::SOFTWARE_ERROR;
    }

    for (INT32 ugpu = Utility::BitScanForward(ugpuMask.GetMask()); ugpu >= 0;
            ugpu = Utility::BitScanForward(ugpuMask.GetMask(), ugpu + 1))
    {
        UINT32 lwrEnableCount = 0;
        CHECK_RC(m_pFsSet->GetPartialEnabledElements(uGpuGroupEnMasks[ugpu],
                    m_FsUnit, &lwrEnableCount));
        // total element disable count for current ugpu
        if (lwrEnableCount == maxEnableCount)
        {
            // equally valid ugpu
            pOutGroupMask->SetBits(uGpuGroupEnMasks[ugpu].GetMask());
        }
        else if (lwrEnableCount > maxEnableCount)
        {
            // update with ugpu containing max count of enabled groups
            pOutGroupMask->SetMask(uGpuGroupEnMasks[ugpu].GetMask());
            maxEnableCount = lwrEnableCount;
        }
    }

    if (pOutGroupMask->IsEmpty())
    {
        pOutGroupMask->SetMask(inGroupMask);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// MostAvailableGroupPicker

MostAvailableGroupPicker::MostAvailableGroupPicker
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    bool bCheckDisabled,
    bool bCheckProtected
)
    : m_pFsSet(pFsSet)
    , m_ElementFsUnit(fsUnit)
    , m_CheckDisabled(bCheckDisabled)
    , m_CheckProtected(bCheckProtected)
{
    MASSERT(pFsSet);
    if (fsUnit != FsUnit::NONE)
    {
        MASSERT(pFsSet->HasElementFsUnit(fsUnit));
    }
}

string MostAvailableGroupPicker::GetPickerName() const
{
    string name = Utility::StrPrintf("MostAvailableGroupPicker-%s",
                                      Downbin::ToString(m_ElementFsUnit).c_str());
    return name;
}

RC MostAvailableGroupPicker::Pick
(
    const FsMask& inGroupMask,
    FsMask *pOutGroupMask
) const
{
    RC rc;
    MASSERT(pOutGroupMask);

    pOutGroupMask->ClearAll();

    if (m_ElementFsUnit == FsUnit::NONE)
    {
        CHECK_RC(PickUnprotectedGroups(inGroupMask, pOutGroupMask, true));
    }
    else if (m_pFsSet->HasElementFsUnit(m_ElementFsUnit))
    {
        CHECK_RC(PickUnprotectedGroups(inGroupMask, pOutGroupMask, false));
        FsMask newInGroupMask = *pOutGroupMask;
        CHECK_RC(PickMostAvailableGroups(newInGroupMask, pOutGroupMask));
    }

    return rc;
}

RC MostAvailableGroupPicker::PickUnprotectedGroups
(
    const FsMask& inGroupMask,
    FsMask *pOutGroupMask,
    bool checkElement
) const
{
    RC rc;
    UINT32 numGroups = m_pFsSet->GetNumGroups();
    vector<FsUnit> fsUnits;
    m_pFsSet->GetElementFsUnits(&fsUnits);
    for (INT32 grpIdx = Utility::BitScanForward(inGroupMask.GetMask());
         grpIdx >= 0;
         grpIdx = Utility::BitScanForward(inGroupMask.GetMask(), grpIdx + 1))
    {
        if (static_cast<UINT32>(grpIdx) >= numGroups)
        {
            break;
        }
        UINT32 numProtectedElements = 0;
        if (checkElement)
        {
            for (auto ele : fsUnits)
            {
                numProtectedElements += m_pFsSet->GetElementSet(grpIdx, ele)->
                                        GetProtectedMask()->GetNumSetBits();
            }
        }
        if (numProtectedElements || m_pFsSet->GetGroup(grpIdx)->IsProtected())
        {
            continue;
        }

        // Equally valid choice
        pOutGroupMask->SetBits(1 << grpIdx);
    }

    // If no good choice, return input Mask
    if (pOutGroupMask->IsEmpty())
    {
        pOutGroupMask->SetMask(inGroupMask);
    }
 
    return rc;
}

RC MostAvailableGroupPicker::PickMostAvailableGroups
(
    const FsMask& inGroupMask,
    FsMask *pOutGroupMask
) const
{
    RC rc;
    UINT32 numGroups = m_pFsSet->GetNumGroups();
    UINT32 maxAvailableCount = 0;
    for (INT32 grpIdx = Utility::BitScanForward(inGroupMask.GetMask());
         grpIdx >= 0;
         grpIdx = Utility::BitScanForward(inGroupMask.GetMask(), grpIdx + 1))
    {
        if (static_cast<UINT32>(grpIdx) >= numGroups)
        {
            break;
        }
        
        UINT32 grpAvailableCount = 0;
        CHECK_RC(CountAvailableElements(grpIdx, &grpAvailableCount));
        if (grpAvailableCount ==  maxAvailableCount)
        {
            // equally valid group
            pOutGroupMask->SetBits(1 << grpIdx);
        }
        else if (grpAvailableCount >  maxAvailableCount)
        {
            // more available group found. So, reject previous groups
            pOutGroupMask->SetMask(1 << grpIdx);
            maxAvailableCount = grpAvailableCount;
        }
    }
    return rc;
}

RC MostAvailableGroupPicker::CountAvailableElements
( 
     UINT32 grpIdx,
     UINT32 *pAvailableCount
) const
{
    if (!m_CheckDisabled && !m_CheckProtected)
    {
        Printf(Tee::PriError, "Neither checking disabled nor protected elements for %s\n",
                               GetPickerName().c_str());
        return RC::BAD_PARAMETER;
    }

    MASSERT(pAvailableCount);
    MASSERT(grpIdx >= 0 && grpIdx < m_pFsSet->GetNumGroups());

    const FsElementSet *pElementSet = m_pFsSet->GetElementSet(grpIdx, m_ElementFsUnit);
    *pAvailableCount = 0;
    *pAvailableCount += m_CheckProtected ? pElementSet->GetProtectedMask()->GetNumUnsetBits()
                                         : 0;
    *pAvailableCount += m_CheckDisabled ? pElementSet->GetTotalDisableMask()->GetNumUnsetBits()
                                         : 0;
    return RC::OK;
}

//------------------------------------------------------------------------------
// GH100FBPGroupPicker

GH100FBPGroupPicker::GH100FBPGroupPicker
(
    TestDevice *pDev,
    FsSet *pFbpSet,
    const FuseUtil::DownbinConfig &fbpConfig
)
    : m_pDev(pDev)
    , m_pFbpSet(pFbpSet)
    , m_FbpConfig(fbpConfig)
{
    MASSERT(pDev);
    MASSERT(pFbpSet);
    m_TotalFbpDisableCount = -1;
    if (m_FbpConfig.isBurnCount)
    {
        m_TotalFbpDisableCount = fbpConfig.disableCount;
    }
}

string GH100FBPGroupPicker::GetPickerName() const
{
    string name = Utility::StrPrintf("GH100FBPGroupPicker-FB");
    return name;
}

RC GH100FBPGroupPicker::Pick
(
    const FsMask& inGroupMask,
    FsMask *pOutGroupMask
) const
{
    RC rc;
    MASSERT(pOutGroupMask);

    pOutGroupMask->ClearAll();

    // If a FBP is disabled and its HBM pair isn't, return the paired FBP
    if (!m_FbpConfig.fullSitesOnly)
    {
        CHECK_RC(PickHBMPairFbp(inGroupMask, pOutGroupMask)); 
    }
    
    vector<UINT32> desiredIndices;
    CHECK_RC(GeneratePreferredOrder(&desiredIndices, inGroupMask.GetNumSetBits()));

    for (INT32 fbIdx : desiredIndices)
    {
        if (inGroupMask.IsSet(fbIdx))
        {
            pOutGroupMask->SetBit(fbIdx);
            return rc;
        }
    } 

    // We should ideally never reach here. We always return early if there is a
    // valid FBP to disable
    return rc;
}

RC GH100FBPGroupPicker::PickHBMPairFbp
(
    const FsMask& inGroupMask,
    FsMask *pOutGroupMask
) const
{
    RC rc;
    MASSERT(pOutGroupMask);
 
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    UINT32 numHbmSites = pSubdev->GetNumHbmSites();

    if (!numHbmSites)
    {
        return rc;
    }
    
    for (UINT32 hbmSite = 0; hbmSite < numHbmSites; hbmSite++)
    {
        UINT32 fbp[s_NumFbpPerHbmSite];
        CHECK_RC(pSubdev->GetHBMSiteFbps(hbmSite, &fbp[0], &fbp[1]));
        for (UINT32 i = 0; i < s_NumFbpPerHbmSite; i++)
        {
            if (m_pFbpSet->GetGroupElementSet()->GetTotalDisableMask()->IsSet(fbp[i]) &&
                    !m_pFbpSet->GetGroupElementSet()->GetTotalDisableMask()->IsSet(fbp[1 - i]))
            {
                if (inGroupMask.IsSet(fbp[1 - i]))
                {
                    pOutGroupMask->SetBit(fbp[1 - i]);
                    return rc;
                }
            }

        }

    }

    return rc;
}

RC GH100FBPGroupPicker::GeneratePreferredOrder
(
    vector<UINT32> *pDesiredIndices,
    UINT32 numFbpDisabled
) const
{
    MASSERT(pDesiredIndices);
    pDesiredIndices->clear();
    vector <UINT32> fbIndices;
    if (m_TotalFbpDisableCount != -1)
    {
        UINT32 numToBeDisabled = m_TotalFbpDisableCount - numFbpDisabled;
        if (!(numToBeDisabled % 4))
        {
            // Go in HBM site order of A, C, D, F, E, B 
            fbIndices = { 2, 0, 5, 3, 9, 7, 10, 8, 11, 6, 4, 1 };

        }
        else
        {
            // Go in HBM site order of  E, B, A, C, D, F
            fbIndices = { 11, 6, 4, 1, 2, 0, 5, 3, 9, 7, 10, 8 };

        }
    }
    else
    {
         // Go in HBM site order of E, B, A, C, D, F
         // Refer http://lwbugs/2730309/5
         fbIndices = { 11, 6, 4, 1, 2, 0, 5, 3, 9, 7, 10, 8 };
    }
    
    pDesiredIndices->insert(pDesiredIndices->begin(), fbIndices.begin(), fbIndices.end());
    return RC::OK;
}


//------------------------------------------------------------------------------
// VGpcSkylineGroupPicker

VGpcSkylineGroupPicker::VGpcSkylineGroupPicker
(
    FsSet *pGpcSet,
    const FuseUtil::DownbinConfig &tpcConfig
)
    : m_pGpcSet(pGpcSet)
{
    MASSERT(pGpcSet);
    m_vGpcSkylineList = tpcConfig.vGpcSkylineList;
    m_MinEnableTpcPerGpc = tpcConfig.minEnablePerGroup;
}

string VGpcSkylineGroupPicker::GetPickerName() const
{
    string name = Utility::StrPrintf("VGpcSkylineGroupPicker");
    return name;
}

RC VGpcSkylineGroupPicker::Pick
(
    const FsMask& inGroupMask,
    FsMask *pOutGroupMask
) const
{
    MASSERT(pOutGroupMask);
    RC rc;
    vector<UINT32> numsEnabledTpcPerGpc;
    pOutGroupMask->ClearAll();
    // If vGpc skyline is not required, just return the inGroupMask
    if (!m_vGpcSkylineList.size())
    {
        pOutGroupMask->SetMask(inGroupMask);
        return RC::OK;
    }

    // Get the number of enabled TPC for each GPC
    CHECK_RC(m_pGpcSet->GetNumsEnabledElementsPerGroup(FsUnit::TPC, &numsEnabledTpcPerGpc));

    if (numsEnabledTpcPerGpc.size() < m_vGpcSkylineList.size())
    {
        Printf(Tee::PriError, "Length of vGpc Skyline List: %lu should not exceed GPC count: %lu\n",
                m_vGpcSkylineList.size(), numsEnabledTpcPerGpc.size());
        return RC::BAD_PARAMETER;
    }

    vector <UINT32> padVGpcSkylineList = m_vGpcSkylineList;

    // Pad the VGpcSkylineList with 0, if the length of it is shorter than GPC count
    padVGpcSkylineList.insert(padVGpcSkylineList.begin(), 
                numsEnabledTpcPerGpc.size() - padVGpcSkylineList.size(), 0);

    for (UINT32 gpcIdx = 0; gpcIdx < numsEnabledTpcPerGpc.size(); gpcIdx++)
    {
        // First check whether it is set in the input mask, and the TPC count of this GPC is
        // bigger than the minimum tpcCountPerGpc
        if ((numsEnabledTpcPerGpc[gpcIdx] > m_MinEnableTpcPerGpc) && inGroupMask.IsSet(gpcIdx))
        {
            vector<UINT32> temp = numsEnabledTpcPerGpc;
            // Second check whether it's within vGpc skyline after disable a TPC
            temp[gpcIdx] = temp[gpcIdx] - 1;
            if (IsWithilwgpcSkyline(temp, padVGpcSkylineList))
            {
                pOutGroupMask->SetBit(gpcIdx);
            }
        }
    }

    return RC::OK;
}

bool VGpcSkylineGroupPicker::IsWithilwgpcSkyline
(
    vector<UINT32> &numsEnabledTpcPerGpc,
    const vector<UINT32> &padVGpcSkylineList
) const
{
    sort(numsEnabledTpcPerGpc.begin(), numsEnabledTpcPerGpc.end());
    for (UINT32 i = 0; i < numsEnabledTpcPerGpc.size(); i++)
    {
        if (numsEnabledTpcPerGpc[i] < padVGpcSkylineList[i])
        {
            return false;
        }
    }
    return true;
}

CrossFsSetDependencyGroupPicker::CrossFsSetDependencyGroupPicker
(
    FsSet *pLwrFsSet,
    FsSet *pRefFsSet
)
    : m_pLwrFsSet(pLwrFsSet)
    , m_pRefFsSet(pRefFsSet)
{
    MASSERT(pLwrFsSet);
    MASSERT(pRefFsSet);
}

string CrossFsSetDependencyGroupPicker::GetPickerName() const
{
    string name = "CrossFsSetDependencyGroupPicker";
    return name;
}

RC CrossFsSetDependencyGroupPicker::Pick
(
    const FsMask& InGroupMask,
    FsMask *pOutGroupMask
) const
{
    MASSERT(pOutGroupMask);
    pOutGroupMask->ClearAll();

    UINT32 numGroupLwr = m_pLwrFsSet->GetNumGroups();
    UINT32 numGroupRef = m_pRefFsSet->GetNumGroups();
    if (numGroupLwr != numGroupRef)
    {
        Printf(Tee::PriError, "CrossFsSetDependencyGroupPicker: NumGroup of %s FsSet is %u. NumGroup of %s FsSet is %u."
                "Which are not the same.", Downbin::ToString(m_pLwrFsSet->GetGroupFsUnit()).c_str(),
                numGroupLwr, Downbin::ToString(m_pRefFsSet->GetGroupFsUnit()).c_str(), numGroupRef);
        return RC::SOFTWARE_ERROR;
    }

    const FsMask *pLwrMask = m_pLwrFsSet->GetGroupElementSet()->GetTotalDisableMask();
    const FsMask *pRefMask = m_pRefFsSet->GetGroupElementSet()->GetTotalDisableMask();

    for (INT32 grpIdx = Utility::BitScanForward(InGroupMask.GetMask());
        grpIdx >= 0;
        grpIdx = Utility::BitScanForward(InGroupMask.GetMask(), grpIdx + 1))
    {
        if (!pRefMask->IsSet(grpIdx) && !pLwrMask->IsSet(grpIdx))
        {
            pOutGroupMask->SetBit(grpIdx);
        }
    }

    // if Output Mask is empty, return Input Mask, since all choices are equally bad 
    if (pOutGroupMask->IsEmpty())
    {
        pOutGroupMask->SetMask(InGroupMask);
    }

    return RC::OK;
}


