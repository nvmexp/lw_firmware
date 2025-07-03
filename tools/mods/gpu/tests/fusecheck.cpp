/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "gputest.h"
#include "core/include/fuseutil.h"
#include "core/include/fuseparser.h"
#include "core/include/mle_protobuf.h"
#include "core/include/platform.h"
#include "core/include/version.h"
#include "gpu/include/boarddef.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/fuse/fuse.h"
#include "gpu/include/testdevice.h"

#include <list>
#include <map>
#include <string>
#include <vector>

class FuseCheck : public GpuTest
{
public:
    FuseCheck()
    {
        SetName("FuseCheck");
    }

    SETGET_PROP(PorCheck, bool);
    SETGET_PROP(FsCheck, bool);
    SETGET_PROP(SkuCheck, bool);
    SETGET_PROP(HEccCheck, bool);
    SETGET_PROP(HbmCheck, bool);
    SETGET_PROP(SkuName, string);
    SETGET_PROP(AllowReconfig, bool);
    SETGET_PROP(TestSpeedoVal, UINT32);
    SETGET_PROP(KappaTarget, FLOAT64);

    GET_PROP(CrcCheck, bool);
    RC SetCrcCheck(bool crcCheck)
    {
        m_CrcCheckModified = true;
        m_CrcCheck         = crcCheck;
        return RC::OK;
    }

    SETGET_PROP(FpfCheck, bool);
    SETGET_PROP(TestGfwRev, UINT32);
    SETGET_PROP(TestRomFlashRev, UINT32);
    SETGET_PROP(TestDevidHulkFuse, bool);
    SETGET_PROP(TestDevidHulkFuseEn, bool);
    SETGET_PROP(TestLwdecRev, UINT32);
    SETGET_PROP(TestLwdelwcode2Ver, UINT32);
    SETGET_PROP(TestLwdelwcode4Ver, UINT32);

    SETGET_PROP(SpareCheck, bool);
    SETGET_PROP(TestSpareNum, UINT32);
    SETGET_PROP(TestSpareVal, UINT32);
    SETGET_PROP(ListKappas, bool);
    SETGET_PROP(KappaBinCheck, bool);

    bool IsSupported() override
    {
        GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

        if ((Platform::GetSimulationMode() == Platform::Fmodel) ||
            (Platform::GetSimulationMode() == Platform::Amodel))
        {
            return false;
        }

        if (pSubdev)
        {
            if (m_PorCheck)
            {
                if (!pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_POR_BIN_CHECK))
                {
                    Printf(Tee::PriLow, "POR check not supported on this chip\n");
                    return false;
                }
                if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
                {
                    Printf(Tee::PriLow, "Insufficient permissions for POR check\n");
                    return false;
                }
            }

            // FS Check is run unless FsCheck is false, and another sub-test is requested
            if ((m_FsCheck ||
                 !(m_PorCheck || m_SkuCheck || m_HEccCheck || m_HbmCheck ||
                   m_CrcCheck || m_FpfCheck || m_SpareCheck || m_ListKappas)) &&
                 !pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_FS_CHECK))
            {
                Printf(Tee::PriLow, "FS check not supported on this chip\n");
                return false;
            }

            if (m_HEccCheck && !pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_HECC_FUSING))
            {
                Printf(Tee::PriLow, "HECC fusing check not supported on this chip\n");
                return false;
            }

            if (m_HbmCheck && !pSubdev->GetFB()->IsHbm())
            {
                Printf(Tee::PriLow, "HBM check not supported on this chip\n");
                return false;
            }

            if (m_CrcCheck && !pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_CRC_FUSE_CHECK))
            {
                Printf(Tee::PriLow, "CRC status check not supported on this chip\n");
                return false;
            }

            if (m_KappaBinCheck && !pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_KAPPA_BINNING))
            {
                Printf(Tee::PriLow, "Kappa bin check not supported\n");
                return false;
            }
        }

        return true;
    }

    // The default behavior is to run CheckFloorsweeping, but if the user
    // specifies a specific sub-test, then run only the requested sub-tests
    RC Run() override
    {
        StickyRC rc;

        set<SubTest> subTests;
        const bool isLwSwitch = GetBoundTestDevice()->GetType() == TestDevice::TYPE_LWIDIA_LWSWITCH;

        // LwSwitch doesn't support PorCheck or FsCheck
        if ((m_PorCheck || m_FsCheck) && isLwSwitch)
        {
            Printf(Tee::PriError, "%s not supported on LwSwitch!\n",
                m_PorCheck ? "POR check" : "Floorsweeping check");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        if (m_PorCheck)
        {
            subTests.insert(PorCheck);
        }

        if (m_SkuCheck)
        {
            subTests.insert(SkuCheck);
        }

        if (m_FsCheck)
        {
            subTests.insert(FsCheck);
        }

        if (m_HEccCheck)
        {
            subTests.insert(HammingEccCheck);
        }

        if (m_HbmCheck)
        {
            subTests.insert(HbmCheck);
        }

        if (m_CrcCheck)
        {
            subTests.insert(CrcCheck);
        }

        if (m_FpfCheck)
        {
            subTests.insert(FpfCheck);
        }

        if (m_SpareCheck)
        {
            subTests.insert(SpareCheck);
        }

        if (m_ListKappas)
        {
            subTests.insert(ListKappas);
        }

        if (subTests.empty())
        {
            // Default subtest for LwSwitch is SkuCheck
            if (isLwSwitch)
            {
                subTests.insert(SkuCheck);
            }
            // Default subtests for non-LwSwitch are FsCheck, Hamming ECC check and CRC check
            else
            {
                subTests.insert(FsCheck);
                subTests.insert(HammingEccCheck);
                // Add CrcCheck if supported and not explicitly disabled by user
                if (GetBoundTestDevice()->HasFeature(Device::GPUSUB_SUPPORTS_CRC_FUSE_CHECK) &&
                    (m_CrcCheck || !m_CrcCheckModified))
                {
                    subTests.insert(CrcCheck);
                }
            }
        }

        if (subTests.count(PorCheck) != 0)
        {
            rc = CheckPorBin();
        }

        if (subTests.count(SkuCheck) != 0)
        {
            rc = CheckSkuMatch();
        }

        if (subTests.count(FsCheck) != 0)
        {
            rc = CheckFloorsweeping();
        }

        if (subTests.count(HammingEccCheck) != 0)
        {
            rc = CheckHammingEcc();
        }

        if (subTests.count(HbmCheck) != 0)
        {
            rc = CheckHbmFusesMatchGpu();
        }

        if (subTests.count(CrcCheck) != 0)
        {
            rc = CheckCrcStatus();
        }

        if (subTests.count(FpfCheck) != 0)
        {
            rc = CheckFpfFusing();
        }

        if (subTests.count(SpareCheck) != 0)
        {
            rc = CheckSpareFusing();
        }
        if (subTests.count(ListKappas) != 0)
        {
            rc = DoListKappas();
        }

        return rc;
    }

    void PrintJsProperties(Tee::Priority pri) override;

private:

    enum SubTest
    {
        PorCheck,
        FsCheck,
        SkuCheck,
        HammingEccCheck,
        HbmCheck,
        CrcCheck,
        FpfCheck,
        SpareCheck,
        ListKappas
    };

    bool m_PorCheck   = false;
    bool m_FsCheck    = false;
    bool m_SkuCheck   = false;
    bool m_HEccCheck  = false;
    bool m_HbmCheck   = false;
    bool m_CrcCheck   = false;
    bool m_FpfCheck   = false;
    bool m_SpareCheck = false;
    bool m_ListKappas = false;
    bool m_KappaBinCheck = false;

    bool m_CrcCheckModified = false;

    string m_SkuName = "";
    bool m_AllowReconfig = true;

    UINT32 m_TestSpeedoVal     = 0;
    UINT32 m_TestGfwRev        = 0;
    UINT32 m_TestRomFlashRev   = 0;
    bool   m_TestDevidHulkFuse = false;
    bool   m_TestDevidHulkFuseEn = false;
    UINT32 m_TestLwdecRev      = 0;
    UINT32 m_TestLwdelwcode2Ver = 0;
    UINT32 m_TestLwdelwcode4Ver = 0;
    static constexpr FLOAT64 ILWALID_KAPPA = std::numeric_limits<FLOAT64>::max();
    FLOAT64 m_KappaTarget = ILWALID_KAPPA;

    UINT32 m_TestSpareNum = ~0U;
    UINT32 m_TestSpareVal = ~0U;

    static ModsGpuRegAddress s_TpcOnGpcRegister[];

    struct ChipBinProperties
    {
        UINT32 Speedo;
        UINT32 HvtSpeedo;
        FLOAT64 HvtSvtRatio;
        UINT32 Iddq;
        INT32 SramBin;
        bool SramBilwalid;
    };
    RC GetChipBinProperties(ChipBinProperties* chipBinProps);

    RC CheckPorBin();

    bool ChipInPorBin
    (
        const ChipBinProperties& chipBinProps,
        const FuseUtil::PorBin& bin,
        const string &skuName,
        UINT32 binId,
        string& errorMsg,
        Tee::Priority pri
    );

    RC CheckFloorsweeping();

    RC CheckSkuMatch();

    RC CheckHbmFusesMatchGpu();

    UINT32 GetTpcOnGpcFuseMask(UINT32 gpc, UINT32 tpcPerGpc);

    RC CheckHammingEcc();

    RC CheckCrcStatus();

    RC CheckFpfFusing();

    RC CheckSpareFusing();

    RC DoListKappas();
};

void FuseCheck::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "%s Js Properties\n", GetName().c_str());

    Printf(pri, "\t%-31s %s\n", "AllowReconfig:", m_AllowReconfig ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "CrcCheck:", m_CrcCheck ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "FpfCheck:", m_FpfCheck ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "FsCheck:", m_FsCheck ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "HbmCheck:", m_HbmCheck ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "HEccCheck:", m_HEccCheck ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "KappaBinCheck:", m_KappaBinCheck ? "true" : "false");
    if (m_KappaTarget == ILWALID_KAPPA)
    {
        Printf(pri, "\t%-31s %s\n", "KappaTarget:", "NONE");
    }
    else
    {
        Printf(pri, "\t%-31s %.1f\n", "KappaTarget:", m_KappaTarget);
    }
    Printf(pri, "\t%-31s %s\n", "ListKappas:", m_ListKappas ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "PorCheck:", m_PorCheck ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "SkuCheck:", m_SkuCheck ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "SkuName:", m_SkuName.c_str());
    Printf(pri, "\t%-31s %s\n", "SpareCheck:", m_SpareCheck ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "TestDevidHulkFuse:", m_TestDevidHulkFuse ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "TestDevidHulkFuseEn:", m_TestDevidHulkFuseEn ? "true" : "false");
    Printf(pri, "\t%-31s %u\n", "TestGfwRev:", m_TestGfwRev);
    Printf(pri, "\t%-31s %u\n", "TestRomFlashRev:", m_TestRomFlashRev);
    Printf(pri, "\t%-31s %u\n", "TestSpareNum:", m_TestSpareNum);
    Printf(pri, "\t%-31s %u\n", "TestSpareVal:", m_TestSpareVal);
    Printf(pri, "\t%-31s %u\n", "TestSpeedoVal:", m_TestSpeedoVal);
    Printf(pri, "\t%-31s %u\n", "TestLwdecRev:", m_TestLwdecRev);
    Printf(pri, "\t%-31s %u\n", "TestLwdelwcode2Ver:", m_TestLwdelwcode2Ver);
    Printf(pri, "\t%-31s %u\n", "TestLwdelwcode4Ver:", m_TestLwdelwcode4Ver);
}

RC FuseCheck::GetChipBinProperties(ChipBinProperties* pChipBinProps)
{
    MASSERT(pChipBinProps);
    RC rc;

    UINT32 version; // unused
    UINT32 moreIddq;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    if (m_TestSpeedoVal != 0)
    {
        pChipBinProps->Speedo = m_TestSpeedoVal;
    }
    else
    {
        CHECK_RC(pSubdev->GetAteSpeedo(0, &pChipBinProps->Speedo, &version));
    }
    CHECK_RC(pSubdev->GetAteIddq(&pChipBinProps->Iddq, &version));
    // opt_speedo2 contains the HVT speedo of the chip.
    CHECK_RC(pSubdev->GetAteSpeedo(2, &pChipBinProps->HvtSpeedo, &version));
    // hvtSvtRatio = opt_speedo2 / opt_speedo0
    pChipBinProps->HvtSvtRatio = static_cast<FLOAT64>(pChipBinProps->HvtSpeedo) /
                                 static_cast<FLOAT64>(pChipBinProps->Speedo);

    // If chip has split rail, the final iddq is the sum of logic and SRAM
    if (pSubdev->GetAteSramIddq(&moreIddq) == RC::OK)
    {
        pChipBinProps->Iddq += moreIddq;
    }

    // For GA10X+, it has separate LWVDD and MSVDD IDDQ values fused
    if (pSubdev->GetAteMsvddIddq(&moreIddq) == RC::OK)
    {
        pChipBinProps->Iddq += moreIddq;
    }

    pChipBinProps->SramBilwalid = false;
    RC tempRc = pSubdev->GetSramBin(&pChipBinProps->SramBin);
    if (tempRc == OK)
    {
        pChipBinProps->SramBilwalid = true;
    }
    else if (tempRc != RC::UNSUPPORTED_FUNCTION)
    {
        CHECK_RC(tempRc);
    }

    return rc;
}

RC FuseCheck::CheckPorBin()
{
    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_POR_BIN_CHECK))
    {
        Printf(Tee::PriError, "This GPU doesn't support POR Check!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        VerbosePrintf("POR Check not allowed\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    Fuse* pFuse = pSubdev->GetFuse();

    ChipBinProperties chipBinProps = {};
    CHECK_RC(GetChipBinProperties(&chipBinProps));

    FLOAT64 kappaTarget = ILWALID_KAPPA;

    string fuseFileName = pFuse->GetFuseFilename();
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fuseFileName));
    const FuseUtil::FuseDefMap* pFuseDefMap;
    const FuseUtil::SkuList* pSkuList;
    const FuseUtil::MiscInfo* pMiscInfo;
    const FuseDataSet* pFuseDataSet;
    CHECK_RC(pParser->ParseFile(fuseFileName,
        &pFuseDefMap, &pSkuList, &pMiscInfo, &pFuseDataSet));

    // If the user specifies a SKU name, check that one, and only that one
    // Otherwise, check any SKUs this chip matches.
    vector<string> skusToCheck;
    if (m_SkuName != "")
    {
        skusToCheck.push_back(m_SkuName);
    }
    else
    {
        pFuse->FindMatchingSkus(&skusToCheck, FuseFilter::ALL_FUSES);
        if (skusToCheck.empty())
        {
            Printf(Tee::PriError, "Chip does not match any SKUs!\n");
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    if (m_KappaBinCheck)
    {
        if (abs(m_KappaTarget - ILWALID_KAPPA) < 0.001f)
        {
            CHECK_RC(pSubdev->GetKappaValue(&kappaTarget));
            VerbosePrintf("Kappa target not specified, getting kappa target from the chip: %lf\n",
                kappaTarget);
        }
        else
        {
            kappaTarget = m_KappaTarget;
        }
    }

    vector<string>::iterator skuName;
    bool checkedBin = false;
    string errorMsg = "";
    for (skuName = skusToCheck.begin(); skuName != skusToCheck.end(); skuName++)
    {
        FuseUtil::SkuList::const_iterator skuConfig = pSkuList->find(*skuName);
        if (skuConfig == pSkuList->end())
        {
            // This can only happen if the sku given is bad
            Printf(Tee::PriError, "Cannot find SKU %s\n", skuName->c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        const auto& binList = skuConfig->second.porBinsMap.find(FuseUtil::PORBIN_GPU);

        if (binList == skuConfig->second.porBinsMap.end() || binList->second.empty())
        {
            Printf(Tee::PriError, "No POR bins found for SKU %s\n", skuName->c_str());
            return RC::POR_SUBBIN_ERROR;
        }

        UINT32 targetSubBinId = UINT32_MAX;
        if (m_KappaBinCheck)
        {
            //Find the id of the sub bin which has the smaller closest kappa value
            FLOAT64 minDiff = ILWALID_KAPPA;
            for (auto& bin: binList->second)
            {
                FLOAT64 lwrDiff = kappaTarget - bin.kappaVal;
                if (lwrDiff < minDiff && lwrDiff >= 0.0f)
                {
                    minDiff = lwrDiff;
                    targetSubBinId = bin.subBinId;
                }
            }
        }

        for (auto& bin: binList->second)
        {
            if (!m_KappaBinCheck || bin.subBinId == targetSubBinId)
            {
                checkedBin = true;
                if (ChipInPorBin(chipBinProps, bin, *skuName, bin.subBinId,
                                 errorMsg, Tee::PriNormal))
                {
                    return rc;
                }
            }
        }
    }

    if (!checkedBin)
    {
        Printf(Tee::PriError, "Chip was not checked against any POR bins!\n");
        Printf(Tee::PriError, "KappaTarget (%.1f) is not in any of the SubBins\n",
            m_KappaTarget);
        return RC::KAPPA_BINNING_ERROR;
    }

    Printf(Tee::PriError, "Chip does not fall in any acceptable POR bin!\n");
    Printf(Tee::PriError, "%s", errorMsg.c_str());
    return RC::POR_SUBBIN_ERROR;
}

bool FuseCheck::ChipInPorBin
(
    const ChipBinProperties& chipBinProps,
    const FuseUtil::PorBin& bin,
    const string &skuName,
    UINT32 binId,
    string& errorMsg,
    Tee::Priority pri
)
{
    // Print POR values
    string por = Utility::StrPrintf("Comparing with SKU %s POR bin %u. POR Values:\n",
                                     skuName.c_str(), binId);
    por += Utility::StrPrintf("Min Speedo: %lf\n",  bin.speedoMin);
    por += Utility::StrPrintf("Max Speedo: %lf\n",  bin.speedoMax);
    por += Utility::StrPrintf("Min HVT Speedo: %lf\n",  bin.hvtSpeedoMin);
    por += Utility::StrPrintf("Max HVT Speedo: %lf\n",  bin.hvtSpeedoMax);
    por += Utility::StrPrintf("Min HVTSVTRatio: %lf\n",  bin.hvtSvtRatioMin);
    por += Utility::StrPrintf("Max HVTSVTRatio: %lf\n",  bin.hvtSvtRatioMax);
    por += Utility::StrPrintf("Max Iddq:   %u\n",  (UINT32)(bin.iddqMax * 1000));
    if (bin.hasAlpha)
    {
        por += Utility::StrPrintf("Alpha:   b = %f, c= %f\n",
                                   bin.alphaLine.bVal,
                                   bin.alphaLine.cVal);
    }
    if (chipBinProps.SramBilwalid)
    {
        por += Utility::StrPrintf("Min Sram Bin:%d\n", bin.minSram);
    }
    Printf(pri, "%s", por.c_str());

    bool inBin = true;
    errorMsg += Utility::StrPrintf("For Sub Bin %u, \n", binId);
    if (chipBinProps.Speedo < bin.speedoMin)
    {
        errorMsg += Utility::StrPrintf("Speedo %u doesn't match POR. MinExpected %lf\n",
                                        chipBinProps.Speedo, bin.speedoMin);
        inBin = false;
    }

    if (bin.speedoMax != 0 && chipBinProps.Speedo > bin.speedoMax)
    {
        errorMsg += Utility::StrPrintf("Speedo %u doesn't match POR. MaxExpected %lf\n",
                                        chipBinProps.Speedo, bin.speedoMax);
        inBin = false;
    }

    if (chipBinProps.HvtSpeedo < bin.hvtSpeedoMin)
    {
        errorMsg += Utility::StrPrintf("HVT Speedo %u doesn't match POR. MinExpected %lf\n",
                                        chipBinProps.HvtSpeedo, bin.hvtSpeedoMin);
        inBin = false;
    }

    if (bin.hvtSpeedoMax != 0 && chipBinProps.HvtSpeedo > bin.hvtSpeedoMax)
    {
        errorMsg += Utility::StrPrintf("HVT Speedo %u doesn't match POR. MaxExpected %lf\n",
                                        chipBinProps.HvtSpeedo, bin.hvtSpeedoMax);
        inBin = false;
    }
    // If the Max and Min fields are empty or invalid, then we should ignore the HVT/SVT ratio check
    if (bin.hvtSvtRatioMax != 0 && chipBinProps.HvtSvtRatio > bin.hvtSvtRatioMax)
    {
        errorMsg += Utility::StrPrintf("HVTSVTRatio %lf doesn't match POR. MaxExpected %lf\n",
                                        chipBinProps.HvtSvtRatio, bin.hvtSvtRatioMax);
        inBin = false;
    }

    if (bin.hvtSvtRatioMin != 0 && chipBinProps.HvtSvtRatio < bin.hvtSvtRatioMin)
    {
        errorMsg += Utility::StrPrintf("HVTSVTRatio %lf doesn't match POR. MinExpected %lf\n",
                                        chipBinProps.HvtSvtRatio, bin.hvtSvtRatioMin);
        inBin = false;
    }

    // bin.iddqMax is in V, we need mV
    if (chipBinProps.Iddq > bin.iddqMax * 1000)
    {
        errorMsg += Utility::StrPrintf("Iddq %u doesn't match POR. MaxExpected %u\n",
                                        chipBinProps.Iddq, (UINT32)(bin.iddqMax * 1000));
        inBin = false;
    }

    if (bin.hasAlpha)
    {
        FuseUtil::AlphaLine alphaLine = bin.alphaLine;
        FLOAT64 alphaVal = alphaLine.cVal * exp(chipBinProps.Speedo * alphaLine.bVal);
        if (chipBinProps.Iddq > alphaVal)
        {
            errorMsg += Utility::StrPrintf("Iddq %u doesn't match POR. MaxAlphaValExpected %lf\n",
                                chipBinProps.Iddq, alphaVal);
            inBin = false;
        }
    }

    if (chipBinProps.SramBilwalid && chipBinProps.SramBin < bin.minSram)
    {
        errorMsg += Utility::StrPrintf("Sram Bin %u doesn't match POR. MinExpected %u\n",
                                chipBinProps.SramBin, bin.minSram);
        inBin = false;
    }

    if (inBin)
    {
        VerbosePrintf("Chip readings matched POR\n");
    }
    return inBin;
}

//------------------------------------------------------------------------------
// Check that this chip's fusing matches the given SKU
// Note: this is essentially a copy of Test 1's verify SKU, but this one works
// with LwSwitch devices (and isn't implemented as a raw test in gputest.js)
RC FuseCheck::CheckSkuMatch()
{
    RC rc;
    Fuse* pFuse = nullptr;

    if (m_SkuName == "")
    {
        Printf(Tee::PriError, "SkuName must be specified to run SKU check\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    pFuse = GetBoundTestDevice()->GetFuse();

    bool doesMatch = false;
    FuseFilter filter = FuseFilter::ALL_FUSES;
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        filter |= FuseFilter::LWSTOMER_FUSES_ONLY;
    }
    CHECK_RC(pFuse->DoesSkuMatch(m_SkuName, &doesMatch, filter));
    if (!doesMatch)
    {
        CHECK_RC(pFuse->PrintMismatchingFuses(m_SkuName, filter));
        rc = RC::INCORRECT_FEATURE_SET;
    }
    return rc;
}

//------------------------------------------------------------------------------
// Check that this chip's HBM config matches the HBM Device ID information
RC FuseCheck::CheckHbmFusesMatchGpu()
{
    RC rc;

    // On FrameBuffer::InitCommon() some values are read from gpu registers that
    // will tell the framebuffer how to behave given the memory type and config.
    // Use these cached values to compare against values read directly from
    // HBMDeviceID, which are read and cached at Gpu Initialization as well.

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    FrameBuffer*  pFb     = pSubdev->GetFB();

    // Get FrameBuffer config values
    FrameBuffer::RamProtocol ramProtocol = pFb->GetRamProtocol();
    UINT32 pseudoChannelsPerChannel      = pFb->GetPseudoChannelsPerChannel();
    UINT32 rankCount                     = pFb->GetRankCount();
    bool isEccOn                         = pFb->IsEccOn();

    // Get HBM config values
    GpuSubdevice::HbmDeviceIdInfo hbmDeviceIdInfo;
    if (!pSubdev->GetHBMDeviceIdInfo(&hbmDeviceIdInfo))
    {
        Printf(Tee::PriError, "HBMDevice was not initialized.\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    // Compare config values

    // Compare FrameBuffer RamProtocol with HBM Gen2Support
    if ((ramProtocol == FrameBuffer::RamProtocol::RamHBM2) && !hbmDeviceIdInfo.gen2Supported)
    {
        string ramProtocolStr = pFb->GetRamProtocolString();
        Printf(Tee::PriError, "FrameBuffer RamProtocol (%s) does not match "
                              "HBM Gen2Support (%s).\n",
                              ramProtocolStr.c_str(),
                              (hbmDeviceIdInfo.gen2Supported ? "true":"false"));
        rc = RC::BAD_HBM_FUSE_DATA;
    }

    // Compare FrameBuffer pseudoChannelsPerChannel with HBM Addressing Mode (Pseudocahnnel/Legacy)
    if (((pseudoChannelsPerChannel == 2) && !hbmDeviceIdInfo.pseudoChannelSupported)
        || ((pseudoChannelsPerChannel == 1) && hbmDeviceIdInfo.pseudoChannelSupported))
    {
        string adressingMode = hbmDeviceIdInfo.pseudoChannelSupported
                                ? "Only Pseudo Channel Mode Supported"
                                : "Only Legacy Mode Supported";

        VerbosePrintf("FrameBuffer pseudoChannelsPerChannel (%d) does not match "
                      "HBM Addressing Mode (%s).\n"
                      "HBM Adressing Mode can be modified later at runtime in order to match "
                      "FrameBuffer pseudoChannelsPerChannel value.\n",
                      pseudoChannelsPerChannel, adressingMode.c_str());

        // Do not fail if these values mismatch. The HBM Addressing Mode can be modified later at
        // runtime. This happens for example on GPUs with HBM2 but the GpuFrameBuffer is
        // configured explicitly to only operate in Legacy mode (HBM1 Protocol)
    }

    // Compare FrameBuffer rankCount with HBM StackHeight
    if (((rankCount == 2) && hbmDeviceIdInfo.stackHeight != 8)
        || ((rankCount == 1) && hbmDeviceIdInfo.stackHeight != 4))
    {
        Printf(Tee::PriError, "FrameBuffer rankCount (%d) does not match HBM StackHeight (%d).\n",
                              rankCount, hbmDeviceIdInfo.stackHeight);
        rc = RC::BAD_HBM_FUSE_DATA;
    }

    // Compare FrameBuffer IsEccOn with HBM ECC Support
    if (isEccOn && !hbmDeviceIdInfo.eccSupported)
    {
         Printf(Tee::PriError, "FrameBuffer IsEccOn (true) does not match HBM ECC Support (false).\n");
        rc = RC::BAD_HBM_FUSE_DATA;
    }

    return rc;
}

//------------------------------------------------------------------------------
// Check the various Floor-Sweepable fuses/registers to make sure that VBIOS or
// something else isn't doing anything we're not expecting.
RC FuseCheck::CheckFloorsweeping()
{
    RC rc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    RegHal& regHal = pSubdev->Regs();

    // OPT_X_DISABLE is always a subset of STATUS_OPT_X, so the XOR yields
    // any bits set in the status register that aren't in the fuses.

    // GPC-PES-TPC-ROP check

    UINT32 numGpcs = regHal.Read32(MODS_PTOP_SCAL_NUM_GPCS);
    UINT32 gpcMask = (1 << numGpcs) - 1;

    UINT32 gpcStatus = regHal.Read32(MODS_FUSE_STATUS_OPT_GPC) & gpcMask;
    UINT32 gpcFuse = regHal.Read32(MODS_FUSE_OPT_GPC_DISABLE) & gpcMask;

    if (gpcFuse != gpcStatus)
    {
        Printf(Tee::PriError, "Extra GPC floor-sweeping found: 0x%x\n", gpcStatus ^ gpcFuse);
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }

    string boardName;
    UINT32 index;
    CHECK_RC(pSubdev->GetBoardInfoFromBoardDB(&boardName, &index, false));
    const BoardDB& boards = BoardDB::Get();
    const BoardDB::BoardEntry* pDef = (boardName != "unknown board") ?
                                      boards.GetBoardEntry(pSubdev->DeviceId(), boardName, index) :
                                      nullptr;

    UINT32 numTpcPerGpc = regHal.Read32(MODS_PTOP_SCAL_NUM_TPC_PER_GPC);
    UINT32 expectedTpcCount;
    JavaScriptPtr pJs;

    // If Boards.DB specifies a TPC count, then check for that value,
    // otherwise check that floorsweeping matches fusing.
    if (pDef != nullptr && pDef->m_JSData != nullptr &&
        OK == pJs->GetProperty(pDef->m_JSData, "TpcCount", &expectedTpcCount))
    {
        UINT32 tpcCount = 0;
        for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
        {
            if ((gpcFuse & (1 << gpc)) != 0)
            {
                // Don't bother checking disabled GPCs
                continue;
            }

            UINT32 status = ~regHal.Read32(MODS_FUSE_STATUS_OPT_TPC_GPC, gpc) & ((1 << numTpcPerGpc) - 1);
            tpcCount += Utility::CountBits(status);
        }
        if (tpcCount != expectedTpcCount)
        {
            Printf(Tee::PriError, "TPC floor-sweeping doesn't match boards.db entry:\n");
            Printf(Tee::PriError, "  Expected %d, Actual %d\n", expectedTpcCount, tpcCount);
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }
    }
    else
    {
        for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
        {
            if ((gpcFuse & (1 << gpc)) != 0)
            {
                // Don't bother checking disabled GPCs
                continue;
            }
            UINT32 status = regHal.Read32(MODS_FUSE_STATUS_OPT_TPC_GPC, gpc) & ((1 << numTpcPerGpc) - 1);
            UINT32 fuse = GetTpcOnGpcFuseMask(gpc, numTpcPerGpc);
            UINT32 diff = status ^ fuse;
            if (m_AllowReconfig && regHal.IsSupported(MODS_FUSE_OPT_TPC_GPCx_RECONFIG, gpc))
            {
                // Allow fuses to differ from _STATUS for bits fused in _RECONFIG
                // Don't force an exact match because _RECONFIG bits can be "unset" by
                // another register
                UINT32 reconfig = regHal.Read32(MODS_FUSE_OPT_TPC_GPCx_RECONFIG, gpc);
                UINT32 extraTpcs = diff & ~reconfig;
                if (regHal.IsSupported(MODS_FUSE_DISABLE_EXTEND_OPT_TPC_GPC, gpc))
                {
                    // Ignore TPCs that have been swapped for repair
                    UINT32 disExtend = regHal.Read32(MODS_FUSE_DISABLE_EXTEND_OPT_TPC_GPC, gpc);
                    extraTpcs = extraTpcs & ~disExtend;
                }
                if (extraTpcs != 0)
                {
                    Printf(Tee::PriError,
                        "Extra TPC floor-sweeping found on GPC%u - "
                        "Fuse/Reconfig: 0x%02x ; Status: 0x%02x\n",
                        gpc, fuse | reconfig, status);
                    return RC::CANNOT_MEET_FS_REQUIREMENTS;
                }
            }
            else
            {
                if (diff != 0)
                {
                    Printf(Tee::PriError,
                        "Extra TPC floor-sweeping found on GPC%u - "
                        "Fuse: 0x%02x ; Status: 0x%02x\n",
                        gpc, fuse, status);
                    return RC::CANNOT_MEET_FS_REQUIREMENTS;
                }
            }
        }
    }

    bool bHasJointPesFuse = regHal.IsSupported(MODS_FUSE_OPT_PES_DISABLE);
    bool bHasSplitPesFuse = regHal.IsSupported(MODS_FUSE_OPT_PES_GPCx_DISABLE, 0);
    if (bHasSplitPesFuse || bHasJointPesFuse)
    {
        vector<UINT32> pesFuses(numGpcs);
        UINT32 numPesPerGpc = pSubdev->GetMaxPesCount();
        UINT32 pesOnGpcMask = (1 << numPesPerGpc) - 1;
        if (bHasJointPesFuse)
        {
            UINT32 numPesPerGpcStride = pSubdev->GetFsImpl()->PesOnGpcStride();
            UINT32 pesFuse = regHal.Read32(MODS_FUSE_OPT_PES_DISABLE);
            for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
            {
                pesFuses[gpc] = (pesFuse >> gpc * numPesPerGpcStride) & pesOnGpcMask;
            }
        }
        else
        {
            for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
            {
                pesFuses[gpc] = regHal.Read32(MODS_FUSE_OPT_PES_GPCx_DISABLE, gpc);
            }
        }
        for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
        {
            if ((gpcFuse & (1 << gpc)) != 0)
            {
                // Don't bother checking disabled GPCs
                continue;
            }
            UINT32 status  = regHal.Read32(MODS_FUSE_STATUS_OPT_PES_GPC, gpc) & pesOnGpcMask;
            UINT32 subFuse = pesFuses[gpc];
            if (subFuse != status)
            {
                // On some chips, the status bits are directly tied to 1s to indicate that the
                // PESs are not present itself
                // Check the CTRL registers explicitly as well to see if they are non-zero
                UINT32 ctrlFuse = regHal.Read32(MODS_FUSE_CTRL_OPT_PES_GPC, gpc) & pesOnGpcMask;
                if (ctrlFuse != 0)
                {
                    Printf(Tee::PriError, "Extra PES floor-sweeping found on GPC%u: 0x%x\n",
                                           gpc, status ^ subFuse);
                    return RC::CANNOT_MEET_FS_REQUIREMENTS;
                }
            }
        }
    }

    if (regHal.IsSupported(MODS_FUSE_OPT_CPC_DISABLE))
    {
        UINT32 numCpcPerGpc = regHal.LookupAddress(MODS_SCAL_LITTER_NUM_CPC_PER_GPC);
        UINT32 cpcOnGpcMask = (1 << numCpcPerGpc) - 1;
        UINT32 cpcFuse      = regHal.Read32(MODS_FUSE_OPT_CPC_DISABLE);
        for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
        {
            if ((gpcFuse & (1 << gpc)) != 0)
            {
                // Don't bother checking disabled GPCs
                continue;
            }
            UINT32 status  = regHal.Read32(MODS_FUSE_STATUS_OPT_CPC_GPC, gpc) & cpcOnGpcMask;
            UINT32 subFuse = (cpcFuse >> gpc * numCpcPerGpc) & cpcOnGpcMask;
            if (subFuse != status)
            {
                Printf(Tee::PriError, "Extra CPC floor-sweeping found on GPC%u: 0x%x\n",
                                       gpc, status ^ subFuse);
                return RC::CANNOT_MEET_FS_REQUIREMENTS;
            }
        }
    }

    if (regHal.IsSupported(MODS_FUSE_OPT_ROP_DISABLE))
    {
        UINT32 numRopPerGpc = regHal.Read32(MODS_PTOP_SCAL_NUM_ROP_PER_GPC);
        UINT32 ropOnGpcMask = (1 << numRopPerGpc) - 1;
        UINT32 ropFuse      = regHal.Read32(MODS_FUSE_OPT_ROP_DISABLE);
        for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
        {
            if ((gpcFuse & (1 << gpc)) != 0)
            {
                // Don't bother checking disabled GPCs
                continue;
            }
            UINT32 status  = regHal.Read32(MODS_FUSE_STATUS_OPT_ROP_GPC, gpc) & ropOnGpcMask;
            UINT32 subFuse = (ropFuse >> gpc * numRopPerGpc) & ropOnGpcMask;
            if (subFuse != status)
            {
                // On some chips, the status bits are directly tied to 1s to indicate that the
                // ROPs are not present itself
                // Check the CTRL registers explicitly as well to see if they are non-zero
                UINT32 ctrlFuse = regHal.Read32(MODS_FUSE_CTRL_OPT_ROP_GPC, gpc) & ropOnGpcMask;
                if (ctrlFuse != 0)
                {
                    Printf(Tee::PriError, "Extra ROP floor-sweeping found on GPC%u: 0x%x\n",
                                           gpc, status ^ subFuse);
                    return RC::CANNOT_MEET_FS_REQUIREMENTS;
                }
            }
        }
    }

    // FBP-FBIO-32bFBPA-L2-L2Slice check

    UINT32 numFbps = regHal.Read32(MODS_PTOP_SCAL_NUM_FBPS);
    UINT32 fbpMask = (1 << numFbps) - 1;

    UINT32 fbpStatus = regHal.Read32(MODS_FUSE_STATUS_OPT_FBP) & fbpMask;
    UINT32 fbpFuse = regHal.Read32(MODS_FUSE_OPT_FBP_DISABLE) & fbpMask;

    // Bug 1684053 - GP100 FBPs must be disabled via floorsweeping, not fuses
    if (!GetBoundGpuSubdevice()->HasBug(1684053) && fbpFuse != fbpStatus)
    {
        Printf(Tee::PriError, "Extra FBP floor-sweeping found: 0x%x\n", fbpStatus ^ fbpFuse);
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }

    // Number of FBIO == Number of FBPA
    UINT32 numFbpa = regHal.Read32(MODS_PTOP_SCAL_NUM_FBPAS);
    UINT32 fbpaMask = (1 << numFbpa) - 1;

    UINT32 fbioStatus = regHal.Read32(MODS_FUSE_STATUS_OPT_FBIO) & fbpaMask;
    UINT32 fbioFuse = regHal.Read32(MODS_FUSE_OPT_FBIO_DISABLE) & fbpaMask;

    if (fbioFuse != fbioStatus)
    {
        Printf(Tee::PriError, "Extra FBIO floor-sweeping found: 0x%x\n", fbioStatus ^ fbioFuse);
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }

    if (regHal.IsSupported(MODS_FUSE_OPT_HALF_FBPA_ENABLE))
    {
        UINT32 halfFbpaStatus = regHal.Read32(MODS_FUSE_STATUS_OPT_HALF_FBPA);
        UINT32 halfFbpaFuse = regHal.Read32(MODS_FUSE_OPT_HALF_FBPA_ENABLE);

        if (halfFbpaFuse != halfFbpaStatus)
        {
            Printf(Tee::PriError, "Extra Half-FBPA floor-sweeping found: 0x%x\n",
                                   halfFbpaStatus ^ halfFbpaFuse);
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }
    }

    bool   bRopInGpc   = regHal.IsSupported(MODS_FUSE_OPT_LTC_DISABLE);
    UINT32 numL2PerFbp = regHal.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
    UINT32 l2OnFbpMask = (1 << numL2PerFbp) - 1;
    UINT32 l2Fuse      = bRopInGpc ? regHal.Read32(MODS_FUSE_OPT_LTC_DISABLE) :
                                     regHal.Read32(MODS_FUSE_OPT_ROP_L2_DISABLE);

    for (UINT32 fbp = 0; fbp < numFbps; fbp++)
    {
        if ((fbpFuse & (1 << fbp)) != 0)
        {
            // Don't bother checking disabled FBPs
            continue;
        }

        UINT32 statusFuse = bRopInGpc ? regHal.Read32(MODS_FUSE_STATUS_OPT_LTC_FBP, fbp) :
                                        regHal.Read32(MODS_FUSE_STATUS_OPT_ROP_L2_FBP, fbp);
        UINT32 status     = statusFuse & l2OnFbpMask;
        UINT32 subFuse    = (l2Fuse >> numL2PerFbp * fbp) & l2OnFbpMask;
        if (status != subFuse)
        {
            Printf(Tee::PriError, "Extra L2 floor-sweeping found on FBP%d: 0x%02x\n",
                                   fbp, status ^ subFuse);
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }
    }

    if (regHal.IsSupported(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, 0))
    {
        UINT32 numL2SlicesPerFbp = regHal.LookupAddress(MODS_SCAL_LITTER_NUM_SLICES_PER_LTC) *
                                   numL2PerFbp;
        UINT32 l2SliceMask       = (1 << numL2SlicesPerFbp) - 1;

        if (!regHal.IsSupported(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, numFbps-1))
        {
            Printf(Tee::PriError, "L2SLICE_FBPx_DISABLE registers not defined for all FBPs!\n");
            return RC::SOFTWARE_ERROR;
        }
        for (UINT32 fbp = 0; fbp < numFbps; fbp++)
        {
            if ((fbpFuse & (1 << fbp)) != 0)
            {
                // Don't bother checking disabled FBPs
                continue;
            }

            UINT32 l2SliceFuse   = regHal.Read32(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, fbp) &
                                   l2SliceMask;
            UINT32 l2SliceStatus = regHal.Read32(MODS_FUSE_STATUS_OPT_L2SLICE_FBP, fbp) &
                                   l2SliceMask;
            if (l2SliceStatus != l2SliceFuse)
            {
                Printf(Tee::PriError, "Extra L2 slice floor-sweeping found on FBP%d: 0x%x\n",
                                       fbp, l2SliceFuse ^ l2SliceStatus);
                return RC::CANNOT_MEET_FS_REQUIREMENTS;
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// Since Volta's TPC count exceeds 32, the register access to the TPC fuses has
// changed. This method hides that change.
UINT32 FuseCheck::GetTpcOnGpcFuseMask(UINT32 gpc, UINT32 tpcPerGpc)
{
    RegHal& regs = GetBoundGpuSubdevice()->Regs();
    if (regs.IsSupported(MODS_FUSE_OPT_TPC_DISABLE))
    {
        return (regs.Read32(MODS_FUSE_OPT_TPC_DISABLE)
                >> tpcPerGpc * gpc) & ((1 << tpcPerGpc) - 1);
    }
    else if (regs.IsSupported(MODS_FUSE_OPT_TPC_GPCx_DISABLE, gpc))
    {
        return regs.Read32(MODS_FUSE_OPT_TPC_GPCx_DISABLE, gpc);
    }
    else
    {
        MASSERT(regs.IsSupported(MODS_FUSE_OPT_TPC_GPCx_DISABLE, gpc));
        return ~0;
    }
}

//-----------------------------------------------------------------------------
// Check that there isn't a problem with the Hamming ECC fuses
RC FuseCheck::CheckHammingEcc()
{
    RegHal& regs = GetBoundGpuSubdevice()->Regs();
    if (!regs.IsSupported(MODS_FUSE_ECC_STATUS))
    {
        // No Hamming ECC to check
        return OK;
    }

    // Check that HECC hasn't detected any issues
    bool eccOk = regs.Test32(MODS_FUSE_ECC_STATUS_H0_PARITY_FAIL_NO) &&
                 regs.Test32(MODS_FUSE_ECC_STATUS_H1_PARITY_FAIL_NO) &&
                 regs.Test32(MODS_FUSE_ECC_STATUS_H2_PARITY_FAIL_NO) &&
                 regs.Test32(MODS_FUSE_ECC_STATUS_H0_UNCORRECTABLE_ERRORS_NO) &&
                 regs.Test32(MODS_FUSE_ECC_STATUS_H1_UNCORRECTABLE_ERRORS_NO) &&
                 regs.Test32(MODS_FUSE_ECC_STATUS_H2_UNCORRECTABLE_ERRORS_NO);
    return eccOk ? OK : RC::FUSE_VALUE_OUT_OF_RANGE;
}

//-----------------------------------------------------------------------------
// Check that there isn't a problem with the CRC fuses
RC FuseCheck::CheckCrcStatus()
{
    RegHal& regs = GetBoundGpuSubdevice()->Regs();
    if (!regs.IsSupported(MODS_FUSE_CRC_STATUS))
    {
        // No CRC fuses to check
        return RC::OK;
    }

    if (regs.Test32(MODS_FUSE_CRC_STATUS_H0_CRC_NOT_PRESENT) ||
        regs.Test32(MODS_FUSE_CRC_STATUS_H1_CRC_NOT_PRESENT))
    {
        Printf(Tee::PriError, "CRC not present\n");
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
// Check that the FPF fusing is valid
RC FuseCheck::CheckFpfFusing()
{
    RC rc;
    TestDevicePtr pDev = GetBoundTestDevice();

    if (m_TestGfwRev != 0)
    {
        UINT32 gfwRev;
        CHECK_RC(pDev->GetFpfGfwRev(&gfwRev));
        if (gfwRev != m_TestGfwRev)
        {
            Printf(Tee::PriError, "GFW Rev (%u) does not match expected (%u)\n",
                                   gfwRev, m_TestGfwRev);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    if (m_TestRomFlashRev != 0)
    {
        UINT32 romFlashRev;
        CHECK_RC(pDev->GetFpfRomFlashRev(&romFlashRev));
        if (romFlashRev != m_TestRomFlashRev)
        {
            Printf(Tee::PriError, "ROM Flash Rev (%u) does not match expected (%u)\n",
                                   romFlashRev, m_TestRomFlashRev);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    if (m_TestDevidHulkFuseEn)
    {
        bool bIsEnabled;
        CHECK_RC(pDev->GetIsDevidHulkEnabled(&bIsEnabled));
        if (!bIsEnabled)
        {
            Printf(Tee::PriError, "DEVID based HULK access not enabled\n");
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    if (m_TestDevidHulkFuse)
    {
        bool bIsRevoked;
        CHECK_RC(pDev->GetIsDevidHulkRevoked(&bIsRevoked));
        if (!bIsRevoked)
        {
            Printf(Tee::PriError, "DEVID based HULK access not revoked\n");
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    if (m_TestLwdecRev)
    {
        RegHal& regs = GetBoundGpuSubdevice()->Regs();
        if (regs.IsSupported(MODS_FUSE_OPT_FPF_UCODE_LWDEC_REV) &&
            regs.Test32(MODS_FUSE_OPT_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL0_ENABLE))
        {
            UINT32 lwdecRev = regs.Read32(MODS_FUSE_OPT_FPF_UCODE_LWDEC_REV);
            if (lwdecRev != m_TestLwdecRev)
            {
                Printf(Tee::PriError, "Lwdec revision (%u) does not match expected (%u)\n",
                                       lwdecRev, m_TestLwdecRev);
                return RC::FUSE_VALUE_OUT_OF_RANGE;
            }
        }
        else
        {
            Printf(Tee::PriNormal, "lwdec_rev not supported\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    if (m_TestLwdelwcode2Ver)
    {
        RegHal& regs = GetBoundGpuSubdevice()->Regs();
        if (regs.IsSupported(MODS_FUSE_OPT_FPF_LWDEC_UCODE2_VERSION) &&
            regs.Test32(MODS_FUSE_OPT_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL0_ENABLE))
        {
            UINT32 ucode2Ver = regs.Read32(MODS_FUSE_OPT_FPF_LWDEC_UCODE2_VERSION);
            if (ucode2Ver != m_TestLwdelwcode2Ver)
            {
                Printf(Tee::PriError, "Lwdec ucode2 version (%u) does not match expected (%u)\n",
                                       ucode2Ver, m_TestLwdelwcode2Ver);
                return RC::FUSE_VALUE_OUT_OF_RANGE;
            }
        }
        else
        {
            Printf(Tee::PriNormal, "lwdec_ucode2_version not supported\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    if (m_TestLwdelwcode4Ver)
    {
        RegHal& regs = GetBoundGpuSubdevice()->Regs();
        if (regs.IsSupported(MODS_FUSE_OPT_FPF_LWDEC_UCODE4_VERSION) &&
            regs.Test32(MODS_FUSE_OPT_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL0_ENABLE))
        {
            UINT32 ucode4Ver = regs.Read32(MODS_FUSE_OPT_FPF_LWDEC_UCODE4_VERSION);
            if (ucode4Ver != m_TestLwdelwcode4Ver)
            {
                Printf(Tee::PriError, "Lwdec ucode4 version (%u) does not match expected (%u)\n",
                                       ucode4Ver, m_TestLwdelwcode4Ver);
                return RC::FUSE_VALUE_OUT_OF_RANGE;
            }
        }
        else
        {
            Printf(Tee::PriNormal, "lwdec_ucode4_version not supported\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    return RC::OK;
}

RC FuseCheck::CheckSpareFusing()
{
    RegHal& regs = GetBoundGpuSubdevice()->Regs();

    if ((m_TestSpareNum == ~0U) || (m_TestSpareVal == ~0U))
    {
        Printf(Tee::PriNormal, "TestSpareNum and TestSpareVal need to be specified for SpareCheck\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (!regs.IsSupported(MODS_FUSE_SPARE_BIT_x, m_TestSpareNum))
    {
        Printf(Tee::PriError, "Invalid spare number %u\n", m_TestSpareNum);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    UINT32 spareVal = regs.Read32(MODS_FUSE_SPARE_BIT_x, m_TestSpareNum);
    if (spareVal != m_TestSpareVal)
    {
        Printf(Tee::PriError, "Spare Check failed. Expected : 0x%x, Actual : 0x%x\n",
                               m_TestSpareVal, spareVal);
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    return RC::OK;
}

RC FuseCheck::DoListKappas()
{
    RC rc;

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    Fuse* pFuse = pSubdev->GetFuse();

    if (!pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_POR_BIN_CHECK) ||
        Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        Printf(Tee::PriError, "ListKappas is not supported\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    ChipBinProperties chipBinProps = {};
    CHECK_RC(GetChipBinProperties(&chipBinProps));

    string fuseFileName = pFuse->GetFuseFilename();
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fuseFileName));
    const FuseUtil::FuseDefMap* pFuseDefMap;
    const FuseUtil::SkuList* pSkuList;
    const FuseUtil::MiscInfo* pMiscInfo;
    const FuseDataSet* pFuseDataSet;
    CHECK_RC(pParser->ParseFile(fuseFileName,
        &pFuseDefMap, &pSkuList, &pMiscInfo, &pFuseDataSet));

    // If the user specifies a SKU name, check that one, and only that one
    // Otherwise, check any SKUs this chip matches.
    vector<string> skusToCheck;
    if (m_SkuName != "")
    {
        skusToCheck.push_back(m_SkuName);
    }
    else
    {
        skusToCheck.reserve(pSkuList->size());
        for (const auto& sku : *pSkuList)
        {
            skusToCheck.push_back(sku.first);
        }
    }

    string errMsg;
    set<FLOAT32> skuKappas;
    string kappaStr;
    bool atLeastOne = false;

    Printf(Tee::PriNormal, "Compatible SKU/kappa values:\n");
    for (const auto& skuName : skusToCheck)
    {
        FuseUtil::SkuList::const_iterator skuConfig = pSkuList->find(skuName);
        if (skuConfig == pSkuList->end())
        {
            if (!m_SkuName.empty())
            {
                Printf(Tee::PriError, "Cannot find SKU %s\n", skuName.c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            continue;
        }

        const auto& binList = skuConfig->second.porBinsMap.find(FuseUtil::PORBIN_GPU);

        if (binList == skuConfig->second.porBinsMap.end() || binList->second.empty())
        {
            if (!m_SkuName.empty())
            {
                Printf(Tee::PriError, "No POR bins found for SKU %s\n", skuName.c_str());
                return RC::UNSUPPORTED_FUNCTION;
            }
            continue;
        }

        skuKappas.clear();
        for (const auto& bin : binList->second)
        {
            if (!m_KappaBinCheck ||
                ChipInPorBin(chipBinProps, bin, skuName, bin.subBinId,
                             errMsg, GetVerbosePrintPri()))
            {
                skuKappas.insert(static_cast<FLOAT32>(bin.kappaVal));
            }
        }
        vector<FLOAT32> kappaVector(skuKappas.rbegin(), skuKappas.rend());
        kappaStr = skuName + ":";
        for (const auto kappa : kappaVector)
        {
            atLeastOne = true;
            kappaStr += Utility::StrPrintf(" %.1f", kappa);
        }
        Printf(Tee::PriNormal, "%s\n", kappaStr.c_str());
        Mle::Print(Mle::Entry::sku_kappa)
            .sku_name(skuName)
            .kappa_values(kappaVector);
    }
    if (!atLeastOne)
    {
        Printf(Tee::PriError, "No supported SKU and kappa combinations found\n");
        return RC::KAPPA_BINNING_ERROR;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

JS_CLASS_INHERIT(FuseCheck, GpuTest,
                 "Fuse sanity test");

CLASS_PROP_READWRITE(FuseCheck, SkuName, string,
                     "SKU to check.");

CLASS_PROP_READWRITE(FuseCheck, TestSpeedoVal, UINT32,
                     "Speedo value to test with instead of the actual value.");

CLASS_PROP_READWRITE(FuseCheck, PorCheck, bool,
                     "Run POR check");

CLASS_PROP_READWRITE(FuseCheck, SkuCheck, bool,
                     "Run SKU check");

CLASS_PROP_READWRITE(FuseCheck, FsCheck, bool,
                     "Run Floor-sweeping check");

CLASS_PROP_READWRITE(FuseCheck, HEccCheck, bool,
                     "Run Hamming-ECC check");

CLASS_PROP_READWRITE(FuseCheck, HbmCheck, bool,
                     "Run HBM check");

CLASS_PROP_READWRITE(FuseCheck, CrcCheck, bool,
                     "Run CRC fusing check");

CLASS_PROP_READWRITE(FuseCheck, FpfCheck, bool,
                     "Run FPF fusing check");
CLASS_PROP_READWRITE(FuseCheck, TestGfwRev, UINT32,
                     "Check GFW Rev FPF fuse is set to the specified value");
CLASS_PROP_READWRITE(FuseCheck, TestRomFlashRev, UINT32,
                     "Check ROM Flash Rev FPF fuse is set to the specified value");
CLASS_PROP_READWRITE(FuseCheck, TestDevidHulkFuse, bool,
                     "Check if DEVID based HULK licenses have been revoked");
CLASS_PROP_READWRITE(FuseCheck, TestDevidHulkFuseEn, bool,
                     "Check if DEVID based HULK licenses have been enabled");
CLASS_PROP_READWRITE(FuseCheck, TestLwdecRev, UINT32,
                     "Check Lwdec Rev FPF fuse is set to the specified value");
CLASS_PROP_READWRITE(FuseCheck, TestLwdelwcode2Ver, UINT32,
                     "Check LWDEC Ucode2 FPF fuse is set to the specified value");
CLASS_PROP_READWRITE(FuseCheck, TestLwdelwcode4Ver, UINT32,
                     "Check LWDEC Ucode4 FPF fuse is set to the specified value");

CLASS_PROP_READWRITE(FuseCheck, AllowReconfig, bool,
                     "Allow reconfig fusing during floorsweeping check");

CLASS_PROP_READWRITE(FuseCheck, KappaTarget, FLOAT64,
                     "Target Kappa Value for the SKU in POR check");

CLASS_PROP_READWRITE(FuseCheck, SpareCheck, bool,
                     "Run spare fusing check");
CLASS_PROP_READWRITE(FuseCheck, TestSpareNum, UINT32,
                     "Spare fuse number to check for");
CLASS_PROP_READWRITE(FuseCheck, TestSpareVal, UINT32,
                     "Expected value for given spare fuse");
CLASS_PROP_READWRITE(FuseCheck, ListKappas, bool,
                     "List all kappa values that are compatible with the GPU under test");
CLASS_PROP_READWRITE(FuseCheck, KappaBinCheck, bool,
                     "Enforce that kappa must be in a POR bin - affects PorCheck and ListKappas");
