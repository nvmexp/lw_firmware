/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
//
// The purpose of this test is to validate the board and sku configuration
// against data entered via the boardsdb website.
//------------------------------------------------------------------------------

#include "core/include/framebuf.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "ctrl/ctrl2080.h"  // graphics caps
#include "device/interface/pcie.h"
#include "gpu/fuse/fuse.h"
#include "gpu/include/boarddef.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "gpu/tests/gputest.h"
#include "cheetah/include/sysutil.h"

#ifdef INCLUDE_AZALIA
    #include "device/azalia/azactrl.h"
#endif

#include <algorithm>
#include <set>
#include <vector>
#include <map>

//------------------------------------------------------------------------------
// ValidSkuCheck2 -- this is the test itself.
//------------------------------------------------------------------------------
class ValidSkuCheck2 : public GpuTest
{
public:
    ValidSkuCheck2();
    RC Setup();
    RC Cleanup();
    bool IsSupported();

    // Functions
    RC Run();

    RC TestBoard(const BoardDB::BoardEntry & def, Tee::Priority outputPri);

    RC InitFromJs();
    void PrintJsProperties(Tee::Priority pri);
    SETGET_PROP(BoardName, string);
    SETGET_PROP(IgnoreReconfigFuses, bool);
  private:
    RC CalcMaxPexSpeedPState();
    bool VerifyAerOffset(INT32 domainNumber,
                         INT32 busNumber,
                         INT32 deviceNumber,
                         INT32 functionNumber,
                         UINT16 expectedOffset) const;
    RC ConfirmBoard( const string &boardName, UINT32 index );
    RC InnerPlxRun(GpuSubdevice* pGpuSubdev, Tee::Priority printPri);
    RC GeminiCheck(GpuSubdevice* pGpuSubdev, Tee::Priority printPri);
    template <typename T>
    RC CheckGolden
    (
        const BoardDB::BoardEntry & def,
        const string & name,
        const T & value,
        bool required,
        bool falseIsIgnore,
        RC error,
        string & log
    );
    string FieldToString( UINT32 value );
    string FieldToString( bool value );
    string FieldToString( string value );

    UINT32 m_MaxSpeedPState;
    Perf::PerfPoint m_OrigPerfPoint;
    bool m_OrigReconfigFusesCheck;

    set<string> m_SubTestSkipList;

    string m_BoardName         = "";
    bool m_IgnoreReconfigFuses = false;
};

//------------------------------------------------------------------------------
bool ValidSkuCheck2::IsSupported()
{
    // ValidSkuCheck2 is not supported on Emulation/Simulation.
    if (GetBoundGpuSubdevice()->IsEmOrSim())
    {
        JavaScriptPtr pJs;
        bool sanityMode = false;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_SanityMode", &sanityMode);
        if (!sanityMode)
            return false;
    }

    // Replaced by LWSpecs Test 14
    if (GetBoundGpuDevice()->GetFamily() >= GpuDevice::GpuFamily::Ampere)
    {
        Printf(Tee::PriLow, "ValidSkuCheck2 not supported on this GPU. On Ampere+, ValidSkuCheck2 "
                            "has been replaced by LWSpecs test\n");
        return false;
    }

    const BoardDB & boards = BoardDB::Get();
    UINT32 version = boards.GetDUTSupportedVersion(GetBoundGpuSubdevice());

    // ValidSkuCheck2 is only supported with version 2+
    if ( version < 2 )
        return false;

    return true;
}

//------------------------------------------------------------------------------
ValidSkuCheck2::ValidSkuCheck2() :
    m_MaxSpeedPState(0),
    m_OrigReconfigFusesCheck(true),
    m_BoardName(""),
    m_IgnoreReconfigFuses(false)
{
    SetName("ValidSkuCheck2");
}

JS_CLASS_INHERIT(ValidSkuCheck2, GpuTest, "Valid board configuration test");
CLASS_PROP_READWRITE(ValidSkuCheck2, BoardName, string,
                     "SKU to check.");
CLASS_PROP_READWRITE(ValidSkuCheck2, IgnoreReconfigFuses, bool,
                     "Ignore reconfig fuses for chip SKU match");
//------------------------------------------------------------------------------
// object, methods, and tests.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC ValidSkuCheck2::Setup()
{
    RC rc = OK;
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();

    CHECK_RC(GpuTest::Setup());

    Perf *pPerf = pGpuSubdev->GetPerf();
    UINT32 numPStates = 0;
    CHECK_RC(pPerf->GetNumPStates(&numPStates));
    if (numPStates)
    {
        CHECK_RC(pPerf->GetLwrrentPerfPoint(&m_OrigPerfPoint));
        CHECK_RC(CalcMaxPexSpeedPState());
        if (m_OrigPerfPoint.PStateNum != m_MaxSpeedPState)
        {
            CHECK_RC(pPerf->SetInflectionPoint(m_MaxSpeedPState, Perf::GpcPerf_MAX));
        }
    }
    else
    {
        Printf(Tee::PriWarn, "ValidSkuCheck2: No valid PState defined\n");
    }

    // Skip Gemini test if we are lwrrently using only_pci_dev
    set<UINT64> allowedPciDevices;
    GpuPtr()->GetAllowedPciDevices(&allowedPciDevices);
    if (!allowedPciDevices.empty())
    {
        Printf(GetVerbosePrintPri(),
               "Skipping Gemini Check since '-only_pci_dev' is in use\n");
        m_SubTestSkipList.insert("Gemini");
    }

    m_OrigReconfigFusesCheck = pGpuSubdev->GetFuse()->IsReconfigFusesCheckEnabled();
    pGpuSubdev->GetFuse()->CheckReconfigFuses(!m_IgnoreReconfigFuses);

    return rc;
}

//------------------------------------------------------------------------------
RC ValidSkuCheck2::Cleanup()
{
    RC rc = OK;
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();
    pGpuSubdev->GetFuse()->CheckReconfigFuses(m_OrigReconfigFusesCheck);
    Perf *pPerf = pGpuSubdev->GetPerf();
    UINT32 numPStates = 0;
    CHECK_RC(pPerf->GetNumPStates(&numPStates));
    if (numPStates)
    {
        if (m_OrigPerfPoint.PStateNum != m_MaxSpeedPState)
        {
            // Restore original PerfPoint
            CHECK_RC(pPerf->SetPerfPoint(m_OrigPerfPoint));
        }
    }
    CHECK_RC(GpuTest::Cleanup());
    return rc;
}

//------------------------------------------------------------------------------
RC ValidSkuCheck2::CalcMaxPexSpeedPState()
{
    RC rc;
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();
    Perf *pPerf = pGpuSubdev->GetPerf();
    vector <UINT32> availablePStates;
    UINT32 maxlnkSpeed = 0;
    UINT32 linkSpeed = 0;

    UINT32 lwrrPState;
    CHECK_RC(pPerf->GetLwrrentPState(&lwrrPState));
    CHECK_RC(pPerf->GetAvailablePStates(&availablePStates));
    if (availablePStates.size() == 1)
    {
        m_MaxSpeedPState = lwrrPState;
    }
    else
    {
        vector <UINT32>::const_reverse_iterator ii = availablePStates.rbegin();
        while (ii != availablePStates.rend())
        {
            // Read expected link speed for all PStates
            CHECK_RC(pPerf->GetPexSpeedAtPState((*ii), &linkSpeed));
            if (linkSpeed > maxlnkSpeed)
            {
                maxlnkSpeed = linkSpeed;
                m_MaxSpeedPState = *ii;
            }
            ii++;
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
bool ValidSkuCheck2::VerifyAerOffset
(
    INT32 domainNumber,
    INT32 busNumber,
    INT32 deviceNumber,
    INT32 functionNumber,
    UINT16 expectedOffset
) const
{
    UINT08 baseCapOffset;
    UINT16 extCapOffset;
    UINT16 extCapSize;

    Pci::FindCapBase(domainNumber,
                     busNumber,
                     deviceNumber,
                     functionNumber,
                     PCI_CAP_ID_PCIE,
                     &baseCapOffset);

    if (OK == Pci::GetExtendedCapInfo(domainNumber,
                                      busNumber,
                                      deviceNumber,
                                      functionNumber,
                                      Pci::AER_ECI,
                                      baseCapOffset,
                                      &extCapOffset,
                                      &extCapSize))
    {
        return extCapOffset == expectedOffset;
    }

    return false;
}

// Checks if the GPU subdevice is attached to a properly configured PLX PCIe switch
//
// This test is very strict and is only required to work starting with Pascal.
// Older boards are likely to have outdated PLX firmware which is improperly configured.
// Most older boards will fail this test unless the PLX firmware is updated.
RC ValidSkuCheck2::InnerPlxRun(GpuSubdevice*  pGpuSubdev, Tee::Priority printPri)
{
    RC rc;
    Printf(printPri, "%-15s", "PLX Check:");

    PexDevice* bridge;
    UINT32 parentPort;
    UINT32 readVal = 0;
    UINT16 readVal16 = 0;
    CHECK_RC(pGpuSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&bridge, &parentPort));

    // Upstream PCIe device must exist
    if (bridge == nullptr)
    {
        Printf(printPri, "Upstream PCIe device not present\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    // PCIe device must be a switch
    if (bridge->IsRoot())
    {
        Printf(printPri, "Upstream PEX device is not a switch\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    PexDevice::PciDevice* upstreamPort = bridge->GetUpStreamPort();
    MASSERT(upstreamPort);

    // 1. Make sure the PCI Vendor ID == 0x10B5 which corresponds to PLX Technology
    CHECK_RC(Platform::PciRead16(upstreamPort->Domain, upstreamPort->Bus, upstreamPort->Dev,
                                 upstreamPort->Func, 0x00, &readVal16));

    if (readVal16 != Pci::Plx)
    {
        Printf(printPri, "Upstream PEX device is not a PLX switch\n");
        return RC::PCI_ILWALID_VENDOR_IDENTIFICATION;
    }

    // 2. Make sure bits 26 and 3 of pci config register 0xF60 is set
    CHECK_RC(Platform::PciRead32(upstreamPort->Domain, upstreamPort->Bus, upstreamPort->Dev,
                    upstreamPort->Func, 0xF60, &readVal));

    if (~readVal & BIT(26))
    {
        Printf(printPri,
               "The PLX switch at %02x:%02x.%x does not disable upstream BARs\n",
               upstreamPort->Bus, upstreamPort->Dev, upstreamPort->Func);
        return RC::PCI_FUNCTION_NOT_SUPPORTED;
    }
    if (~readVal & BIT(3))
    {
        Printf(printPri,
               "The PLX switch at %02x:%02x.%x does not route TLPs correctly\n",
               upstreamPort->Bus, upstreamPort->Dev, upstreamPort->Func);
        return RC::PCI_FUNCTION_NOT_SUPPORTED;
    }

    // 3. PLX switch must have SVID == 0x10DE (LWPU)
    UINT08 ssvidCapOffset;
    CHECK_RC(Pci::FindCapBase(upstreamPort->Domain, upstreamPort->Bus,
                              upstreamPort->Dev, upstreamPort->Func,
                              PCI_CAP_ID_SSVID, &ssvidCapOffset));

    CHECK_RC(Platform::PciRead16(upstreamPort->Domain, upstreamPort->Bus,
                                 upstreamPort->Dev, upstreamPort->Func,
                                 ssvidCapOffset + LW_PCI_CAP_SSVID_VENDOR,
                                 &readVal16));

    if (readVal16 != Pci::Lwpu)
    {
        Printf(printPri,
               "Invalid SVID (0x%04x) on the PLX switch at %02x:%02x.%x."
               " Expecting SVID to be 0x10de.\n",
               readVal16, upstreamPort->Bus, upstreamPort->Dev,
               upstreamPort->Func);
        return RC::PCI_ILWALID_VENDOR_IDENTIFICATION;
    }

    // 4. PLX switch must have same SSID as the GPU
    CHECK_RC(Platform::PciRead16(upstreamPort->Domain, upstreamPort->Bus,
                                 upstreamPort->Dev, upstreamPort->Func,
                                 ssvidCapOffset + LW_PCI_CAP_SSVID_DEVICE,
                                 &readVal16));

    const UINT32 gpuSSID = pGpuSubdev->GetInterface<Pcie>()->SubsystemDeviceId();
    if (readVal16 != gpuSSID)
    {
        Printf(printPri,
               "Invalid SSID (0x%04x) on the PLX switch at %02x:%02x.%x."
               " Expecting SSID to be 0x%04x\n",
               readVal16, upstreamPort->Bus, upstreamPort->Dev,
               upstreamPort->Func, gpuSSID);
        return RC::PCI_ILWALID_VENDOR_IDENTIFICATION;
    }

    Printf(printPri, "Board has properly configured PLX switch\n");
    return OK;
}

// Checks if the GPU subdevice is part of a Gemini board
//
// 1. The Gemini GPUs must be connected to a PLX brand PCIe switch
// 2. The board project, board SKU, and SSID/SVID of Gemini GPUs must match
// 3. The Gemini GPUs must be connected to the same PCIe switch
RC ValidSkuCheck2::GeminiCheck(GpuSubdevice*  pMySubdev, Tee::Priority printPri)
{
    RC rc;
    Printf(printPri, "%-15s", "Gemini Check:");

    BoardDB::Signature mySig;
    BoardDB::Get().GetDUTSignature(pMySubdev, &mySig);

    PexDevice* myBridge;
    CHECK_RC(pMySubdev->GetInterface<Pcie>()->GetUpStreamInfo(&myBridge));

    // 1. Gemini GPUs must be connected to a PLX brand PCIe switch
    //
    // Use a subset of the PLX check, since that check is very strict about
    // the configuration of the PLX switch, and older PLX roms will not pass

    // Upstream PCIe device must exist
    if (myBridge == nullptr)
    {
        Printf(printPri, "Upstream PCIe device not present\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    // PCIe device must be a switch
    if (myBridge->IsRoot())
    {
        Printf(printPri, "Upstream PEX device is not a switch\n");
        return RC::INCORRECT_FEATURE_SET;
    }

    // Switch must be a PLX brand switch
    UINT16 readVal16;
    PexDevice::PciDevice* upstreamPort = myBridge->GetUpStreamPort();
    MASSERT(upstreamPort);
    CHECK_RC(Platform::PciRead16(upstreamPort->Domain, upstreamPort->Bus, upstreamPort->Dev,
                                 upstreamPort->Func, 0x00, &readVal16));
    if (readVal16 != Pci::Plx)
    {
        Printf(printPri, "Upstream PEX device is not a PLX brand switch\n");
        return RC::PCI_ILWALID_VENDOR_IDENTIFICATION;
    }

    // Find all gemini matches, inclusive of the GpuSubDevice being tested
    vector<GpuSubdevice*> matches;
    GpuDevMgr* deviceMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    for (GpuSubdevice* pLwrSubdev = deviceMgr->GetFirstGpu();
                       pLwrSubdev != nullptr;
                       pLwrSubdev = deviceMgr->GetNextGpu(pLwrSubdev))
    {
        BoardDB::Signature lwrSig;
        BoardDB::Get().GetDUTSignature(pLwrSubdev, &lwrSig);

        // 2. If the Board Project, Board SKU, and SSID/SVID match it may be Gemini
        if (lwrSig.m_BoardProjectAndSku == mySig.m_BoardProjectAndSku &&
            lwrSig.m_SSID == mySig.m_SSID &&
            lwrSig.m_SVID == mySig.m_SVID)
        {
            PexDevice* lwrBridge;
            CHECK_RC(pLwrSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&lwrBridge));
            MASSERT(lwrBridge);

            // 3. If the bridges are the same (and both PLX), it is a Gemini match
            if (lwrBridge == myBridge)
            {
                matches.push_back(pLwrSubdev);
            }
        }
    }

    if (matches.size() == 2)
    {
        Printf(printPri, "Board is a Gemini board\n");
        return OK;
    }
    else
    {
        Printf(printPri, "Found %zu matching GPU device(s) under PLX switch", matches.size());
        return RC::DEVICE_NOT_FOUND;
    }
}

//------------------------------------------------------------------------------
RC ValidSkuCheck2::Run()
{
    StickyRC        rc;
    GpuSubdevice *  pGpuSubdev = GetBoundGpuSubdevice();

    string boardName;
    UINT32 index;
    const BoardDB & boards = BoardDB::Get();

    // Ignore cached information if reconfig fuses need to be ignored
    CHECK_RC(pGpuSubdev->GetBoardInfoFromBoardDB(&boardName, &index, m_IgnoreReconfigFuses));
    const BoardDB::BoardEntry *pGoldEntry =
        boards.GetBoardEntry(pGpuSubdev->DeviceId(), boardName, index);

    const BoardDB::BoardEntry *pWebsiteEntry = nullptr;
    boards.GetWebsiteEntry(&pWebsiteEntry);

    BoardDB::BoardEntry dutEntry;
    boards.GetDUTSignature(pGpuSubdev, &(dutEntry.m_Signature));

    if (pGoldEntry)
    {
        Printf(Tee::PriNormal, "Found %s[%d]\n", boardName.c_str(), index);

        if (pWebsiteEntry != nullptr)
        {
            if ((boardName != pWebsiteEntry->m_BoardName)
                || (index != pWebsiteEntry->m_BoardDefIndex))
            {
                Printf(Tee::PriError, "Board doesn't match website entry.\n");
                rc = RC::UNKNOWN_BOARD_DESCRIPTION;
            }
        }

        if ((m_BoardName != "") && (boardName != m_BoardName))
        {
            Printf(Tee::PriError, "Board name in the target = %s doesn't match expected entry = %s.\n", boardName.c_str(), m_BoardName.c_str());
            rc = RC::UNKNOWN_BOARD_DESCRIPTION;
        }
    }
    else
    {
        Printf(Tee::PriError,
            "BOARD MATCH NOT FOUND\n"
            "The probable cause of this is that you have an outdated\n"
            "boards.db file.\n");
        rc = RC::UNKNOWN_BOARD_DESCRIPTION;
    }

    if (rc == OK)
    {
        CHECK_RC(TestBoard(*pGoldEntry, GetVerbosePrintPri() ));
        CHECK_RC(ConfirmBoard(boardName, index));
    }
    else
    {
        if (dutEntry.m_Signature.m_OfficialChipSKUs.size() == 0)
        {
            Printf(Tee::PriNormal, "Chip doesn't match any known chip sku.\n");

            string fileName = pGpuSubdev->GetFuse()->GetFuseFilename();
            vector<string> chipSkuNames;
            FuseUtil::GetSkuNames(fileName, &chipSkuNames);

            size_t minDifferences = ~0;
            vector<string> closestChipSkus;
            for (UINT32 i = 0; i < chipSkuNames.size(); i++)
            {
                FuseDiff badFuses;
                pGpuSubdev->GetFuse()->GetMismatchingFuses(
                    chipSkuNames[i],
                    FuseFilter::LWSTOMER_FUSES_ONLY,
                    &badFuses);

                size_t diffCount = badFuses.namedFuseMismatches.size();
                if (badFuses.doesFsMismatch)
                {
                    diffCount++;
                }

                if (badFuses.doesIffMismatch)
                {
                    diffCount++;
                }

                if (diffCount < minDifferences)
                {
                    minDifferences = diffCount;
                    closestChipSkus.clear();
                    closestChipSkus.push_back(chipSkuNames[i]);
                }
                else if (diffCount == minDifferences)
                {
                    closestChipSkus.push_back(chipSkuNames[i]);
                }
            }

            if (closestChipSkus.size() > 0)
            {
                for (UINT32 i = 0; i < closestChipSkus.size(); i++)
                {
                    Printf(Tee::PriNormal, "Differences from %s:\n",
                           closestChipSkus[i].c_str());
                    pGpuSubdev->GetFuse()->PrintMismatchingFuses(
                            closestChipSkus[i],
                            FuseFilter::LWSTOMER_FUSES_ONLY);
                }
            }
        }
        else
        {
            Printf(Tee::PriNormal,
                 "************************ LWPU AE / FAEs *************************\n"
                 "* Please update http://mods/boards.html using the following       *\n"
                 "* information as well as the board POR.                           *\n"
                 "* See https://wiki.lwpu.com/engwiki/index.php/MODS/Board_Editor *\n"
                 "* for instructions.                                               *\n"
                 "*******************************************************************\n");
        }

        if (pWebsiteEntry != nullptr)
        {
            string svid("*");
            string ssid("*");
            string pciClassCode("*");

            if (pWebsiteEntry->m_Signature.m_SVID != 0)
                svid = Utility::StrPrintf("0x%x",
                                          pWebsiteEntry->m_Signature.m_SVID);

            if (pWebsiteEntry->m_Signature.m_SSID != 0)
                ssid = Utility::StrPrintf("0x%x",
                                          pWebsiteEntry->m_Signature.m_SSID);

            if (pWebsiteEntry->m_Signature.m_PciClassCode != "")
                pciClassCode = pWebsiteEntry->m_Signature.m_PciClassCode;

            Printf( Tee::PriNormal,
                " Signature            Expected   Actual\n"
                "-----------------------------------------------------------------\n");
            Printf(Tee::PriNormal, " %-20s %-10s %-10s\n", "GPU",
                   Gpu::DeviceIdToInternalString(static_cast<Gpu::LwDeviceId>(
                                   pWebsiteEntry->m_Signature.m_GpuId)).c_str(),
                   Gpu::DeviceIdToInternalString(static_cast<Gpu::LwDeviceId>(
                                   dutEntry.m_Signature.m_GpuId)).c_str());
            Printf(Tee::PriNormal, " %-20s %-10s 0x%-8x\n", "SVID",
                   svid.c_str(),
                   dutEntry.m_Signature.m_SVID);
            Printf(Tee::PriNormal, " %-20s %-10s 0x%-8x\n", "SSID",
                   ssid.c_str(),
                   dutEntry.m_Signature.m_SSID);
            Printf(Tee::PriNormal, " %-20s 0x%-8x 0x%-8x\n", "BoardID",
                   pWebsiteEntry->m_Signature.m_BoardId,
                   dutEntry.m_Signature.m_BoardId);
            Printf(Tee::PriNormal, " %-20s 0x%-8x 0x%-8x\n", "Rows",
                   pWebsiteEntry->m_Signature.m_Rows,
                   dutEntry.m_Signature.m_Rows);
            Printf(Tee::PriNormal, " %-20s 0x%-8x 0x%-8x\n", "Columns",
                   pWebsiteEntry->m_Signature.m_Columns,
                   dutEntry.m_Signature.m_Columns);
            Printf(Tee::PriNormal, " %-20s %-10d %-10d\n", "Banks",
                   pWebsiteEntry->m_Signature.m_Intbanks,
                   dutEntry.m_Signature.m_Intbanks);
            Printf(Tee::PriNormal, " %-20s %-10s %-10s\n", "BoardProjectAndSku",
                   pWebsiteEntry->m_Signature.m_BoardProjectAndSku.c_str(),
                   dutEntry.m_Signature.m_BoardProjectAndSku.c_str());
            Printf(Tee::PriNormal, " %-20s %-10s %-10s\n", "VBIOSChipName",
                   pWebsiteEntry->m_Signature.m_VBIOSChipName.size()?
                        pWebsiteEntry->m_Signature.m_VBIOSChipName.c_str() : "*",
                   dutEntry.m_Signature.m_VBIOSChipName.c_str());
            Printf(Tee::PriNormal, " %-20s %-10s %-10s\n", "FBBAR",
                   Utility::StrPrintf("%d MB",
                            pWebsiteEntry->m_Signature.m_FBBar * 32).c_str(),
                   Utility::StrPrintf("%d MB",
                            dutEntry.m_Signature.m_FBBar * 32).c_str());
            Printf(Tee::PriNormal, " %-20s %-10s %-10s\n", "PCI Class Code",
                   pciClassCode.c_str(),
                   dutEntry.m_Signature.m_PciClassCode.c_str());

            Printf(Tee::PriNormal, "\n OfficialChipSKU\n"
                                   "    Expected: ");
            for (UINT32 i = 0; i < pWebsiteEntry->m_Signature.m_OfficialChipSKUs.size(); i ++)
            {
                if (i > 0)
                    Printf(Tee::PriNormal, " OR ");
                Printf(Tee::PriNormal,
                       "\"%s\"",
                       pWebsiteEntry->m_Signature.m_OfficialChipSKUs[i].m_Name.c_str());
            }
            Printf(Tee::PriNormal, "\n    Actual  : ");
            for (UINT32 i = 0; i < dutEntry.m_Signature.m_OfficialChipSKUs.size(); i ++)
            {
                if (i > 0)
                    Printf(Tee::PriNormal, " OR ");
                Printf(Tee::PriNormal,
                       "\"%s\"",
                       dutEntry.m_Signature.m_OfficialChipSKUs[i].m_Name.c_str());
            }
            if (dutEntry.m_Signature.m_OfficialChipSKUs.size() == 0)
                Printf(Tee::PriNormal, "unknown chip sku");
            Printf(Tee::PriNormal, "\n\n");

            TestBoard(*pWebsiteEntry, GetVerbosePrintPri() );
        }
        else
        {
            BoardDB::BoardEntry emptyEntry;

            Printf(Tee::PriNormal, "\nBoard signature (%s[%d]):\n",
                   boardName.c_str(),
                   index);
            dutEntry.Print( true, Tee::PriNormal );
            TestBoard(emptyEntry, Tee::PriNormal );

        }

    }

    return rc;
}

//--------------------------------------------------------------------
//! Helper function for simple checks of the dut's value against the
//! golden value (accounting for command-line overrides).
//!
//! \param def Boards.db Entry to be checked against.
//! \param name Name of the subtest.
//! \param value DUT's actual value (to be compared against the golden).
//! \param required Boards.db Entry is required to have a golden.
//! \param falseIsIgnore Treat "false" entries in boards.db as "Don't Care"
//! \param error RC to use on mismatch.
//! \param log string to append log messages to.
//!
//! \return OK on success. error (param) on mismatch.
//!

template <typename T>
RC ValidSkuCheck2::CheckGolden
(
    const BoardDB::BoardEntry & def,
    const string & name,
    const T & value,
    bool required,
    bool falseIsIgnore,
    RC error,
    string & log
)
{
    JavaScriptPtr pJs;
    RC rc;

    string resultString("Pass");
    string goldenString("-");
    string valueString = FieldToString(value);

    if (m_SubTestSkipList.count( name ))
    {
        resultString = "Skip";
    }
    else
    {
        bool goldenFound = false;
        T golden;
        JSObject * pJsObject = GetJSObject();
        JSObject * pJsSubObject;

        // get the golden from boards.db
        if (def.m_JSData != nullptr)
        {
            if (OK == pJs->GetProperty(def.m_JSData, name.c_str(), &golden))
            {
                goldenFound = true;
            }
        }

        // override the golden from testarg
       if (OK == pJs->GetProperty(pJsObject, "OverrideSubTests", &pJsSubObject))
        {
            if (OK==pJs->GetProperty(pJsSubObject, name.c_str(), &golden))
            {
                goldenFound = true;
            }
        }

        if (goldenFound)
        {
            goldenString = FieldToString(golden);
            if ( goldenString == "false" && falseIsIgnore )
            {
                resultString = "Skip";
            }
            else if ( golden != value )
            {
                resultString = "Fail";
                rc = error;
            }
        }
        else if ( required )
        {
            resultString = "Fail - No golden value in boards.db.";
            rc = error;
        }
        else
        {
            resultString = "Skip";
        }
    }

    log.append(
               Utility::StrPrintf(" %-20s %-10s %-10s %s\n",
               name.c_str(),
               goldenString.c_str(),
               valueString.c_str(),
               resultString.c_str() )
              );

    return rc;
}

string ValidSkuCheck2::FieldToString( UINT32 value )
{
    return Utility::StrPrintf("%d", value);
}

string ValidSkuCheck2::FieldToString( bool value )
{
    return value? "true" : "false";
}

string ValidSkuCheck2::FieldToString( string value )
{
    return value;
}

//--------------------------------------------------------------------

RC ValidSkuCheck2::TestBoard
(
    const BoardDB::BoardEntry & def,
    Tee::Priority outputPri
)
{
    StickyRC rc = OK;
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();

    FrameBuffer *pFb = pGpuSubdev->GetFB();
    auto pGpuPcie = pGpuSubdev->GetInterface<Pcie>();
    LwRm *pLwRm = GetBoundRmClient();

    // Check strapped fan pwm
    string strappedFanDebugPwm("Disabled");
    if (pGpuSubdev->HasFeature(Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_66) ||
        pGpuSubdev->HasFeature(Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_33))
    {
        if (pGpuSubdev->IsFeatureEnabled(
                    Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_66))
        {
            strappedFanDebugPwm = "66";
        }
        else if (pGpuSubdev->IsFeatureEnabled(
                    Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_33))
        {
            strappedFanDebugPwm = "33";
        }
    }

    // Check for ECC
    bool eccEnabled = false;
    if (pGpuSubdev->HasFeature(Device::GPUSUB_SUPPORTS_ECC))
    {
        bool eccSupported;
        UINT32 enableMask;
        CHECK_RC(pGpuSubdev->GetEccEnabled(&eccSupported, &enableMask));
        eccEnabled = eccSupported && enableMask;
    }

    // Check for AER
    bool aerEnabled = false;

    bool gpuAerEnabled = pGpuPcie->AerEnabled();
    bool gpuAerOffsetCorrect = false;

    if (gpuAerEnabled)
    {
        // Verify AER offset in PCIE Extended Capabilities list
        gpuAerOffsetCorrect = VerifyAerOffset(pGpuSubdev->GetInterface<Pcie>()->DomainNumber(),
                                              pGpuSubdev->GetInterface<Pcie>()->BusNumber(),
                                              pGpuSubdev->GetInterface<Pcie>()->DeviceNumber(),
                                              pGpuSubdev->GetInterface<Pcie>()->FunctionNumber(),
                                              pGpuSubdev->GetInterface<Pcie>()->GetAerOffset());
    }

#ifdef INCLUDE_AZALIA
    // If we have a multifunctional SKU, both functions must support AER
    AzaliaController* const pAzalia = pGpuSubdev->GetAzaliaController();
    if (pAzalia != nullptr)
    {
        bool azaAerEnabled = pAzalia->AerEnabled();
        bool azaAerOffsetCorrect = false;

        if (azaAerEnabled)
        {
            // Find AER offset in PCIE Extended Capabilities list
            SysUtil::PciInfo azaPciInfo = {0};
            CHECK_RC(pAzalia->GetPciInfo(&azaPciInfo));

            azaAerOffsetCorrect = VerifyAerOffset(azaPciInfo.DomainNumber,
                                                  azaPciInfo.BusNumber,
                                                  azaPciInfo.DeviceNumber,
                                                  azaPciInfo.FunctionNumber,
                                                  pAzalia->GetAerOffset());
        }

        aerEnabled =    azaAerEnabled &&
                        azaAerOffsetCorrect &&
                        gpuAerEnabled &&
                        gpuAerOffsetCorrect;
    }
    else
    {
        aerEnabled = gpuAerEnabled && gpuAerOffsetCorrect;
    }
#else
    aerEnabled = gpuAerEnabled && gpuAerOffsetCorrect;
#endif

    // Check for pwr cap
    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS pwrPolicyParams = { 0 };
    CHECK_RC(pLwRm->ControlBySubdevice(pGpuSubdev,
                            LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_INFO,
                            &pwrPolicyParams,
                            sizeof(pwrPolicyParams)));
    bool pwrCapEnabled = (pwrPolicyParams.policyMask != 0);

    // Check Gpu Pci Capability
    const bool gen4Capable = (pGpuPcie->LinkSpeedCapability() > Pci::Speed8000MBPS);
    const bool gen3Capable = (gen4Capable ||
                              pGpuPcie->LinkSpeedCapability() > Pci::Speed5000MBPS);
    const bool gen2Capable = (gen3Capable ||
                              pGpuPcie->LinkSpeedCapability() > Pci::Speed2500MBPS);
    string lwrBusSpeed;
    switch (pGpuPcie->GetLinkSpeed(Pci::SpeedUnknown))
    {
        case Pci::Speed16000MBPS:
            lwrBusSpeed = "Gen4";
            break;
        case Pci::Speed8000MBPS:
            lwrBusSpeed = "Gen3";
            break;
        case Pci::Speed5000MBPS:
            lwrBusSpeed = "Gen2";
            break;
        case Pci::Speed2500MBPS:
            lwrBusSpeed = "Gen1";
            break;
        default:
            break;
    }

    // Check for properly configured PLX switch
    bool plxEnabled = (OK == InnerPlxRun(pGpuSubdev, GetVerbosePrintPri()));

    // Check for Gemini board
    bool geminiEnabled = (OK == GeminiCheck(pGpuSubdev, GetVerbosePrintPri()));

    const bool bCmodel = ((Platform::GetSimulationMode() == Platform::Fmodel) ||
                          (Platform::GetSimulationMode() == Platform::Amodel));

    // check against goldens...
    string results =
        " Subtest              Expected   Actual     Result\n"
        "-----------------------------------------------------------------\n";
    rc = CheckGolden(def, "ExternalBanks", pFb->GetRankCount(),
                     !bCmodel, false, RC::MEMORY_SIZE_ERROR, results);
    rc = CheckGolden(def, "FBBus", pFb->GetBusWidth(),
                     !bCmodel, false, RC::MEMORY_SIZE_ERROR, results);
    rc = CheckGolden(def, "PcieLanes", pGpuPcie->GetLinkWidth(),
                     !bCmodel, false, RC::INCORRECT_PCIX_LANES, results);
    rc = CheckGolden(def, "TpcCount", pGpuSubdev->GetTpcCount(),
                     false, false, RC::INCORRECT_FEATURE_SET, results);

    rc = CheckGolden(def, "Gl", GetBoundGpuDevice()->CheckCapsBit(GpuDevice::QUADRO_GENERIC),
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "Ecc", eccEnabled,
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "Pwrcap", pwrCapEnabled,
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "Gen4", gen4Capable,
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "Gen3", gen3Capable,
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "Gen2", gen2Capable,
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "InitGen", lwrBusSpeed,
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "FanDebugPwm", strappedFanDebugPwm,
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "Aer", aerEnabled,
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "PLX", plxEnabled,
                     false, false, RC::INCORRECT_FEATURE_SET, results);
    rc = CheckGolden(def, "Gemini", geminiEnabled,
                     false, true, RC::INCORRECT_FEATURE_SET, results);

    Printf(rc != OK ? Tee::PriNormal : outputPri, "%s\n", results.c_str());

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC ValidSkuCheck2::InitFromJs()
{
    RC rc;
    JavaScriptPtr pJS;
    JSObject *    pJsObject = GetJSObject();

    CHECK_RC(GpuTest::InitFromJs());

    vector<string> subTestSkipList;
    if (OK == pJS->GetProperty(pJsObject, "SkipSubTests", &subTestSkipList))
    {
        m_SubTestSkipList.insert(subTestSkipList.begin(),
                                 subTestSkipList.end());
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ void ValidSkuCheck2::PrintJsProperties(Tee::Priority pri)
{

    GpuTest::PrintJsProperties(pri);
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
}

//--------------------------------------------------------------------
//! Confirm the board (was tested) if being run to do so.  Also ensure
//! that no test arguments or command line options are present that
//! curtail a full validation of the new boards.db entry
//!
//! \param boardName Board name that was found.
//! \param index Index for the board that was found
//!
//! \return OK if board was confirmed (or no confirmation necessary), not OK if
//!     confirmation is required but test argumetns or command line options
//!     skipped validation of some portion of the new entry.
//!
RC ValidSkuCheck2::ConfirmBoard
(
    const string &boardName,
    UINT32 index
)
{
    const BoardDB & boards = BoardDB::Get();

    if (!boards.BoardChangeRequested())
        return OK;

    string testParams = "";
    string cmdLineArgs = "";

    // check for subtest skipping
    if (!m_SubTestSkipList.empty())
    {
        for (set<string>::iterator i = m_SubTestSkipList.begin();
             i != m_SubTestSkipList.end(); i++)
        {
            cmdLineArgs += "!!    -feature_check_skip " + *i + "!!\n";
        }
    }

    // check for golden overrides
    JavaScriptPtr pJs;
    JSObject * pJsObject = GetJSObject();
    JSObject * pJsSubObject;
    if (OK == pJs->GetProperty(pJsObject, "OverrideSubTests", &pJsSubObject))
    {
        vector<string> subTestNames;
        if (OK == pJs->GetProperties(pJsSubObject, &subTestNames))
        {
            for (vector<string>::iterator i = subTestNames.begin();
                 i != subTestNames.end(); i++)
            {
                cmdLineArgs += "!!    -feature_check_override " + *i + " x!!\n";
            }
        }
    }

    if (testParams != "")
    {
        testParams.insert(0,
            "!!   Test Parameters (from \"-testarg\" or .spc file):"
            "          !!\n");
        testParams +=
           "!!                                                            !!\n";
    }
    if (cmdLineArgs != "")
    {
        cmdLineArgs.insert(0,
            "!!   Command Line Arguments:"
            "                                  !!\n");
        cmdLineArgs +=
           "!!                                                            !!\n";
    }
    if ((testParams != "") || (cmdLineArgs != ""))
    {
        Printf(Tee::PriNormal,
            "\n\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "!!     USER ERROR   USER ERROR    USER ERROR   USER ERROR     !!\n"
            "!!             CANNOT GENERATE CONFIRMATION STRING            !!\n"
            "!!                                                            !!\n"
            "!! Generating a confirmation string must be done on a POR GPU !!\n"
            "!! board including POR VBIOS on a motherboard that supports   !!\n"
            "!! the POR.  Remove any of the following test parameters or   !!\n"
            "!! command line options that are present and re-run           !!\n"
            "!! %s to generate the confirmation string:        !!\n"
            "!!                                                            !!\n"
            "%s%s"
            "!!                                                            !!\n"
            "!! Running MODS with a boards.dbe from the Board Editor       !!\n"
            "!! Website that contains an unconfirmed entry for any purpose !!\n"
            "!! other than generation of a confirmation string is not      !!\n"
            "!! supported                                                  !!\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",
            GetName().c_str(), testParams.c_str(), cmdLineArgs.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    else
    {
        BoardDB::ConfirmBoard(boardName, index);

    }
    return OK;
}
