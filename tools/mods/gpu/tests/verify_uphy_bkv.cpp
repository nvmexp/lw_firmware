/* LWIDIA_COPYRIGHT_BEGIN                                                 */
/*                                                                        */
/* Copyright 2021 by LWPU Corporation.  All rights reserved.  All       */
/* information contained herein is proprietary and confidential to LWPU */
/* Corporation.  Any use, reproduction, or disclosure without the written */
/* permission of LWPU Corporation is prohibited.                        */
/*                                                                        */
/* LWIDIA_COPYRIGHT_END                                                   */

#include "core/include/jsonparser.h"
#include "core/include/script.h"
#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlinkuphy.h"
#include "device/interface/pcie.h"
#include "device/interface/pcie/pcieuphy.h"
#include "device/interface/uphyif.h"
#include "gpu/tests/lwlink/lwlinktest.h"
#include "uphy_bkv_data.h"

// RapidJson includes
#include "document.h"

//-----------------------------------------------------------------------------
// VerifyUphyBkv verifies the BKV values programmed into the LwLink and Pcie
// UPHY DLM and CLM registers to ensure that the GPU UPHYs are programmed correctly
// The UPHY BKV settings are stored in the HW tree at a UPHY specific location.  For
// instance on GH100 (UPHY v6.1) the files are stored at:
//
//    //dev/mixsig/uphy/6.1/rel/5.1/vmod/dlm/LW_UPHY_DLM_gold_ref_pkg.json
//    //dev/mixsig/uphy/6.1/rel/5.1/vmod/dlm/LW_UPHY_CLM_gold_ref_pkg.json
//
// These files only contain the latest BKV version.  There is a MODS cron job responsible
// processing the BKV files into a MODS specific JSON that contains the full BKV history
// for all BKV version.  This allows VerifyUphyBkv to be capable of implementing testargs
// to verify specific BKV versions (or BKV protocol versions).
//
// The way the UPHY team has structured its files, there is one file for CLM and one file
// for DLM.  Each file has a top level BKV version (and the BKV versions may differ
// between the files).  In addition, each protocol (PCIE, LWLINK, etc.) has its own
// BKV version in each of the files
//
// Several bugs prevent this test from being 100% functional:
//
// https://lwbugs/3422555 : Implement dev_trim.h register access in LS10 driver
// https://lwbugs/3255519 : Update LS10 SOE functions for PCIE UPHY access
// https://lwbugs/3255516 : Update GH100 PMU functions for PCIE UPHY access
namespace MfgTest
{
    class VerifyUphyBkv : public LwLinkTest
    {
    public:

        VerifyUphyBkv();
        virtual ~VerifyUphyBkv() { }
        bool IsSupported() override;
        RC Setup() override;
        RC Cleanup() override;
        void PrintJsProperties(Tee::Priority pri) override;

        RC RunTest() override;

        SETGET_PROP_ENUM(VerifyInterface, UINT08, UphyIf::Interface,  UphyIf::Interface::All);
        SETGET_PROP_ENUM(VerifyRegBlock,  UINT08, UphyIf::RegBlock,  UphyIf::RegBlock::ALL);
        SETGET_PROP(ClmBkvVersion,               FLOAT32);
        SETGET_PROP(DlmBkvVersion,               FLOAT32);
        SETGET_PROP(LwLinkClmProtocolBkvVersion, FLOAT32);
        SETGET_PROP(LwLinkDlmProtocolBkvVersion, FLOAT32);
        SETGET_PROP(PcieClmProtocolBkvVersion,   FLOAT32);
        SETGET_PROP(PcieDlmProtocolBkvVersion,   FLOAT32);
        SETGET_PROP(VerifyOneInstance,  bool);

    private:
        struct RegisterMismatch
        {
            UINT16 address;
            UINT16 expectedValue;
            UINT16 actualValue;
            RegisterMismatch(UINT16 a, UINT16 e, UINT16 act) :
                address(a), expectedValue(e), actualValue(act) {}
        };

        struct BkvInfo
        {
            string name;
            FLOAT32 bkvVersion = -1.0;
            FLOAT32 protocolVersion = -1.0;
            vector<UphyBkvData::BkvRegister> regs;
            BkvInfo() { }
        };

        const BkvInfo & GetBkvInfo
        (
            UphyIf::Version version,
            UphyIf::Interface interface,
            UphyIf::RegBlock regblock,
            UINT32 regBlockIndex
        );
        RC InitRegNames(UphyIf::Version version, UphyIf::RegBlock regblock);
        void MergeRegs
        (
            vector<UphyBkvData::BkvRegister> * pDst,
            const vector<UphyBkvData::BkvRegister> & src
        );
        RC VerifyBkvValues
        (
            UphyIf * pUphy,
            UphyIf::Interface interface,
            UphyIf::RegBlock regblock
        );

        StickyRC m_DeferredRc;
        vector<BkvInfo> m_CachedBkvInfo;
        static const BkvInfo s_EmptyBkvInfo;
        map<UINT16, string> m_DlmRegisterNames;
        map<UINT16, string> m_ClmRegisterNames;

        // JS variables
        UphyIf::Interface m_VerifyInterface    = UphyIf::Interface::All;
        UphyIf::RegBlock m_VerifyRegBlock      = UphyIf::RegBlock::ALL;
        FLOAT32 m_ClmBkvVersion                = -1.0;
        FLOAT32 m_DlmBkvVersion                = -1.0;
        FLOAT32 m_LwLinkClmProtocolBkvVersion  = -1.0;
        FLOAT32 m_LwLinkDlmProtocolBkvVersion  = -1.0;
        FLOAT32 m_PcieClmProtocolBkvVersion    = -1.0;
        FLOAT32 m_PcieDlmProtocolBkvVersion    = -1.0;
        bool    m_VerifyOneInstance            = false;
    };
}

using namespace MfgTest;

const MfgTest::VerifyUphyBkv::BkvInfo MfgTest::VerifyUphyBkv::s_EmptyBkvInfo;

//-----------------------------------------------------------------------------
MfgTest::VerifyUphyBkv::VerifyUphyBkv()
{
    SetName("VerifyUphyBkv");
}

//-----------------------------------------------------------------------------
bool MfgTest::VerifyUphyBkv::IsSupported()
{
    if (GetBoundTestDevice()->IsSOC())
        return false;

    bool bVerifyLwLink = false;
    if (m_VerifyInterface != UphyIf::Interface::Pcie)
    {
        bVerifyLwLink = GetBoundTestDevice()->SupportsInterface<LwLinkUphy>() &&
                        LwLinkTest::IsSupported();
    }

    bool bVerifyPcie = false;
    if (m_VerifyInterface != UphyIf::Interface::LwLink)
    {
        bVerifyPcie = GetBoundTestDevice()->SupportsInterface<PcieUphy>();
    }

    if (!bVerifyLwLink && !bVerifyPcie)
    {
        Printf(Tee::PriLow, "%s : No interfaces found to verify, skipping\n", GetName().c_str());
    }

    return bVerifyLwLink || bVerifyPcie;
}

//-----------------------------------------------------------------------------
RC MfgTest::VerifyUphyBkv::Setup()
{
    RC rc;
    if ((m_VerifyInterface != UphyIf::Interface::Pcie) &&
        GetBoundTestDevice()->SupportsInterface<LwLinkUphy>())
    {
        CHECK_RC(LwLinkTest::Setup());
    }
    else
    {
        CHECK_RC(GpuTest::Setup());
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC MfgTest::VerifyUphyBkv::Cleanup()
{
    StickyRC rc;
    if ((m_VerifyInterface != UphyIf::Interface::Pcie) &&
        GetBoundTestDevice()->SupportsInterface<LwLink>())
    {
        rc = LwLinkTest::Cleanup();
    }
    else
    {
        rc = GpuTest::Cleanup();
    }

    return rc;
}

//-----------------------------------------------------------------------------
void MfgTest::VerifyUphyBkv::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tVerifyInterface:             %s\n",
           UphyIf::InterfaceToString(m_VerifyInterface));
    Printf(pri, "\tVerifyRegBlock:              %s\n",
           UphyIf::RegBlockToString(m_VerifyRegBlock));
    Printf(pri, "\tClmBkvVersion:               %s\n",
           (m_ClmBkvVersion < 0.0) ?
               "Not Set" : Utility::StrPrintf("%.1f", m_ClmBkvVersion).c_str());
    Printf(pri, "\tDlmBkvVersion:               %s\n",
           (m_DlmBkvVersion < 0.0) ?
               "Not Set" : Utility::StrPrintf("%.1f", m_DlmBkvVersion).c_str());
    Printf(pri, "\tLwLinkClmProtocolBkvVersion: %s\n",
           (m_LwLinkClmProtocolBkvVersion < 0.0) ?
               "Not Set" : Utility::StrPrintf("%.1f", m_LwLinkClmProtocolBkvVersion).c_str());
    Printf(pri, "\tLwLinkDlmProtocolBkvVersion: %s\n",
           (m_LwLinkDlmProtocolBkvVersion < 0.0) ?
               "Not Set" : Utility::StrPrintf("%.1f", m_LwLinkDlmProtocolBkvVersion).c_str());
    Printf(pri, "\tPcieClmProtocolBkvVersion:   %s\n",
           (m_PcieClmProtocolBkvVersion < 0.0) ?
               "Not Set" : Utility::StrPrintf("%.1f", m_PcieClmProtocolBkvVersion).c_str());
    Printf(pri, "\tPcieDlmProtocolBkvVersion:   %s\n",
           (m_PcieDlmProtocolBkvVersion < 0.0) ?
               "Not Set" : Utility::StrPrintf("%.1f", m_PcieDlmProtocolBkvVersion).c_str());
    Printf(pri, "\tVerifyOneInstance:  %s\n", m_VerifyOneInstance ? "true" : "false");
}

//-----------------------------------------------------------------------------
RC MfgTest::VerifyUphyBkv::RunTest()
{
    RC rc;
    const bool bStopOnError = GetGoldelwalues()->GetStopOnError();

    if (m_VerifyInterface != UphyIf::Interface::Pcie)
    {
        auto pLwLinkUphy = GetBoundTestDevice()->GetInterface<LwLinkUphy>();
        if (m_VerifyRegBlock != UphyIf::RegBlock::CLN)
        {
            CHECK_RC(VerifyBkvValues(pLwLinkUphy,
                                     UphyIf::Interface::LwLink,
                                     UphyIf::RegBlock::DLN));
        }
        if (m_VerifyRegBlock != UphyIf::RegBlock::DLN)
        {
            rc = VerifyBkvValues(pLwLinkUphy, UphyIf::Interface::LwLink, UphyIf::RegBlock::CLN);
            if ((rc == RC::PRIV_LEVEL_VIOLATION) && !bStopOnError)
            {
                m_DeferredRc = rc;
                rc.Clear();
            }
            CHECK_RC(rc);
        }
    }

    if (GetBoundTestDevice()->SupportsInterface<PcieUphy>() &&
        (m_VerifyInterface != UphyIf::Interface::LwLink))
    {
        auto pPcieUphy = GetBoundTestDevice()->GetInterface<PcieUphy>();
        if (m_VerifyRegBlock != UphyIf::RegBlock::CLN)
        {
            CHECK_RC(VerifyBkvValues(pPcieUphy, UphyIf::Interface::Pcie, UphyIf::RegBlock::DLN));
        }
        if (m_VerifyRegBlock != UphyIf::RegBlock::DLN)
        {
            rc = VerifyBkvValues(pPcieUphy, UphyIf::Interface::Pcie, UphyIf::RegBlock::CLN);
            if ((rc == RC::PRIV_LEVEL_VIOLATION) && !bStopOnError)
            {
                m_DeferredRc = rc;
                rc.Clear();
            }
            CHECK_RC(rc);
        }
    }
    return m_DeferredRc;
}

//-----------------------------------------------------------------------------
const MfgTest::VerifyUphyBkv::BkvInfo & MfgTest::VerifyUphyBkv::GetBkvInfo
(
    UphyIf::Version version,
    UphyIf::Interface interface,
    UphyIf::RegBlock regBlock,
    UINT32 regBlockIndex
)
{
    RC rc;
    UINT32 speed;
    UINT32 allSpeeds;
    TestDevicePtr pTestDev = GetBoundTestDevice();
    FLOAT32 protocolVersion;
    FLOAT32 bkvVersion;

    // Different BKV tables may be used depending on the current speed of the interface.  Some
    // BKV tables can be used for all speeds however
    if (interface == UphyIf::Interface::LwLink)
    {
        speed =
           static_cast<UINT32>(pTestDev->GetInterface<LwLink>()->GetSystemLineRate(regBlockIndex));
        allSpeeds = static_cast<UINT32>(LwLink::SystemLineRate::GBPS_UNKNOWN);
        bkvVersion = (regBlock == UphyIf::RegBlock::DLN) ? m_DlmBkvVersion :
                                                           m_ClmBkvVersion;
        protocolVersion = (regBlock == UphyIf::RegBlock::DLN) ? m_LwLinkDlmProtocolBkvVersion :
                                                                m_LwLinkClmProtocolBkvVersion;
    }
    else
    {
        speed =
            static_cast<UINT32>(pTestDev->GetInterface<Pcie>()->GetLinkSpeed(Pci::SpeedUnknown));
        allSpeeds = static_cast<UINT32>(Pci::SpeedUnknown);
        bkvVersion = (regBlock == UphyIf::RegBlock::DLN) ? m_DlmBkvVersion :
                                                           m_ClmBkvVersion;
        protocolVersion = (regBlock == UphyIf::RegBlock::DLN) ? m_PcieDlmProtocolBkvVersion :
                                                                m_PcieClmProtocolBkvVersion;
    }
    const UphyBkvData::BkvTable & bkvTable = UphyBkvData::GetBkvTable(version, interface);

    for (auto const & lwrTableEntry : bkvTable)
    {
        if (lwrTableEntry.regBlock == regBlock)
        {
            MASSERT(!lwrTableEntry.speeds.empty());
            auto speedIt = find_if(lwrTableEntry.speeds.begin(), lwrTableEntry.speeds.end(),
                                   [speed] (const UINT32 & lwrSpeed) -> bool
                                   {
                                       return (speed == lwrSpeed);
                                   });
            // If the current BKV table is used for all speeds then there will only be one entry
            // in the speed vector and it will indicate that this table entry should be used for
            // all speeds, otherwise if the speed was found then this is the table entry that
            // should be used
            if ((lwrTableEntry.speeds[0] == allSpeeds) || (speedIt != lwrTableEntry.speeds.end()))
            {
                // Return the cached table if it exists
                auto bkvIt = find_if(m_CachedBkvInfo.begin(), m_CachedBkvInfo.end(),
                                     [lwrTableEntry] (const BkvInfo & lwrBkvInfo) -> bool
                                     {
                                         return (lwrTableEntry.bkvName == lwrBkvInfo.name);
                                     });
                VerbosePrintf("%s : Using %s %s BKV values %s on %s%s\n",
                              GetName().c_str(),
                              UphyIf::InterfaceToString(interface),
                              UphyIf::RegBlockToString(regBlock),
                              lwrTableEntry.bkvName.c_str(),
                              GetBoundTestDevice()->GetName().c_str(),
                              (interface == UphyIf::Interface::LwLink) ?
                                  Utility::StrPrintf(", link %u", regBlockIndex).c_str() : "");
                if (bkvIt != m_CachedBkvInfo.end())
                {
                    return *bkvIt;
                }

                BkvInfo bkvInfo;
                bkvInfo.name = lwrTableEntry.bkvName;

                for (auto const &lwrBkvSettings : lwrTableEntry.bkvEntries)
                {
                    // The script that generates the BKV tables ensures that the entries are
                    // arranged in order of protocol version.
                    //
                    // In addition, the registers entries in the registers vector capture
                    // the difference in BKV register settings from the previous protocol version
                    // so merge the registers into the info array as we iterate
                    if (bkvInfo.regs.empty())
                    {
                        bkvInfo.regs = lwrBkvSettings.bkvRegisters;
                    }
                    else
                    {
                        MergeRegs(&bkvInfo.regs, lwrBkvSettings.bkvRegisters);
                    }

                    // Both BKV version and protocol version were specified, ensure that the
                    // specified BKV version uses the protocol version
                    if ((protocolVersion >= 0.0) && (bkvVersion >= 0.0) &&
                        (protocolVersion == lwrBkvSettings.protocolVersion) &&
                        !lwrBkvSettings.bkvVersions.count(bkvVersion))
                    {
                        Printf(Tee::PriError,
                               "%s : Specified %s %s protocol version %.1f not used in "
                               "specified BKV version %.1f for %s\n",
                               GetName().c_str(),
                               UphyIf::InterfaceToString(interface),
                               UphyIf::RegBlockToString(regBlock),
                               protocolVersion,
                               bkvVersion,
                               lwrTableEntry.bkvName.c_str());
                        return s_EmptyBkvInfo;
                    }

                    if ((protocolVersion >= 0.0) &&
                        (lwrBkvSettings.protocolVersion == protocolVersion))
                    {
                        bkvInfo.bkvVersion =
                            (bkvVersion >= 0.0) ? bkvVersion :
                                                  *lwrBkvSettings.bkvVersions.rbegin();
                        bkvInfo.protocolVersion = protocolVersion;
                        m_CachedBkvInfo.push_back(bkvInfo);
                        return m_CachedBkvInfo.back();
                    }
                    else if ((bkvVersion >= 0.0) &&
                             lwrBkvSettings.bkvVersions.count(bkvVersion))
                    {
                        bkvInfo.bkvVersion = bkvVersion;
                        bkvInfo.protocolVersion = lwrBkvSettings.protocolVersion;
                        m_CachedBkvInfo.push_back(bkvInfo);
                        return m_CachedBkvInfo.back();
                    }
                }
                if ((protocolVersion >= 0.0) || (bkvVersion >= 0.0))
                {
                    string errString =
                        Utility::StrPrintf("%s : Specified %s %s ",
                                           GetName().c_str(),
                                           UphyIf::InterfaceToString(interface),
                                           UphyIf::RegBlockToString(regBlock));
                    if (protocolVersion >= 0.0)
                    {
                        errString += Utility::StrPrintf("protocol version %.1f%s",
                                                        protocolVersion,
                                                        (bkvVersion >= 0.0) ? " and " : "");

                    }
                    if (bkvVersion >= 0.0)
                    {
                        errString += Utility::StrPrintf("BKV version %.1f",
                                                        bkvVersion);
                    }
                    Printf(Tee::PriError, "%s not found in %s\n",
                           errString.c_str(), lwrTableEntry.bkvName.c_str());
                    return s_EmptyBkvInfo;
                }

                bkvInfo.protocolVersion =
                    lwrTableEntry.bkvEntries.back().protocolVersion;
                bkvInfo.bkvVersion = *(lwrTableEntry.bkvEntries.back().bkvVersions.rbegin());
                m_CachedBkvInfo.push_back(bkvInfo);
                return m_CachedBkvInfo.back();
            }
        }
    }

    string errString =
        Utility::StrPrintf("%s : No %s %s BKV entry found for %s%s at ",
                           GetName().c_str(),
                           UphyIf::InterfaceToString(interface),
                           UphyIf::RegBlockToString(regBlock),
                           GetBoundTestDevice()->GetName().c_str(),
                           (interface == UphyIf::Interface::LwLink) ?
                              Utility::StrPrintf(", link %u", regBlockIndex).c_str() : "");
    if (interface == UphyIf::Interface::LwLink)
    {
        errString +=
            Utility::StrPrintf("%uMbps",
                               pTestDev->GetInterface<LwLink>()->GetLineRateMbps(regBlockIndex));
    }
    else
    {
        errString += Utility::StrPrintf("%uMbps", speed);
    }
    return s_EmptyBkvInfo;
}

//-----------------------------------------------------------------------------
RC MfgTest::VerifyUphyBkv::InitRegNames(UphyIf::Version version, UphyIf::RegBlock regblock)
{
    map<UINT16, string> * pRegNames = (regblock == UphyIf::RegBlock::DLN) ? &m_DlmRegisterNames :
                                                                            &m_ClmRegisterNames;
    if (!pRegNames->empty())
        return RC::OK;

    string filename = Utility::StrPrintf("%s-reg-spec-%s.json",
                                         UphyIf::RegBlockToString(regblock),
                                         UphyIf::VersionToString(version));
    transform(filename.begin(), filename.end(), filename.begin(),
              [](unsigned char c){ return tolower(c); });

    // Avoid any prints if the file is not found
    const auto fileStatus = Utility::FindPkgFile(filename, nullptr);
    if (fileStatus == Utility::NoSuchFile)
        return RC::OK;

    rapidjson::Document jsonDoc;
    RC rc;
    CHECK_RC(JsonParser::ParseJson(filename, &jsonDoc));
    CHECK_RC(JsonParser::CheckHasSection(jsonDoc, "registers"));
    if (!jsonDoc["registers"].IsArray())
    {
        Printf(Tee::PriError, "Invalid registers section in %s\n", filename.c_str());
        return RC::CANNOT_PARSE_FILE;
    }

    for (rapidjson::SizeType ii = 0; ii < jsonDoc["registers"].Size(); ++ii)
    {
        const JsonTree& reg = jsonDoc["registers"][ii];
        CHECK_RC(JsonParser::CheckHasSection(reg, "name"));
        CHECK_RC(JsonParser::CheckHasSection(reg, "addr"));

        UINT32 addr;
        string name;
        CHECK_RC(JsonParser::RetrieveJsolwal(reg, "addr", addr));
        CHECK_RC(JsonParser::RetrieveJsolwal(reg, "name", name));
        pRegNames->emplace(addr, name);
    }
    return rc;
}

//-----------------------------------------------------------------------------
void  MfgTest::VerifyUphyBkv::MergeRegs
(
    vector<UphyBkvData::BkvRegister> * pDst,
    const vector<UphyBkvData::BkvRegister> & src
)
{
    for (auto const & lwrReg : src)
    {
        auto dstIt = find_if(pDst->begin(), pDst->end(),
                             [lwrReg] (const UphyBkvData::BkvRegister & reg) -> bool
                             {
                                 return lwrReg.addr == reg.addr;
                             });
        if (dstIt == pDst->end())
        {
            pDst->push_back(lwrReg);
        }
        else
        {
            dstIt->data = lwrReg.data;
            dstIt->mask = lwrReg.mask;
            dstIt->block = lwrReg.block;
        }
    }
}

//-----------------------------------------------------------------------------
RC MfgTest::VerifyUphyBkv::VerifyBkvValues
(
    UphyIf * pUphy,
    UphyIf::Interface interface,
    UphyIf::RegBlock regBlock
)
{
    RC rc;

    const bool bStopOnError = GetGoldelwalues()->GetStopOnError();
    const UINT32 laneMask = pUphy->GetActiveLaneMask(regBlock);

    for (UINT32 lwrRegBlock = 0; lwrRegBlock < pUphy->GetMaxRegBlocks(regBlock); ++lwrRegBlock)
    {
        bool bIsActive;
        CHECK_RC(pUphy->IsRegBlockActive(regBlock, lwrRegBlock, &bIsActive));

        if (!bIsActive)
            continue;

        auto bkvInfo = GetBkvInfo(pUphy->GetVersion(), interface, regBlock, lwrRegBlock);
        if (bkvInfo.name.empty())
        {
            if (bStopOnError)
                return RC::BAD_PARAMETER;
            else
                m_DeferredRc = RC::BAD_PARAMETER;
        }

        for (UINT32 lwrLane = 0; lwrLane < pUphy->GetMaxLanes(regBlock); ++lwrLane)
        {
            if (!(laneMask & (1 << lwrLane)))
                continue;

            vector<RegisterMismatch> regMismatches;
            for (auto const & lwrReg : bkvInfo.regs)
            {
                UINT16 regData = 0;
                if (regBlock == UphyIf::RegBlock::CLN)
                {
                    CHECK_RC(pUphy->ReadPllReg(lwrRegBlock, lwrReg.addr, &regData));
                }
                else
                {
                    CHECK_RC(pUphy->ReadPadLaneReg(lwrRegBlock, lwrLane, lwrReg.addr, &regData));
                }

                const UINT16 expectedValue = lwrReg.data & lwrReg.mask;
                const UINT16 actualValue = regData & lwrReg.mask;
                if (expectedValue != actualValue)
                {
                    regMismatches.emplace_back(lwrReg.addr, expectedValue, actualValue);
                }
            }
            if (!regMismatches.empty())
            {
                CHECK_RC(InitRegNames(pUphy->GetVersion(), regBlock));
                map<UINT16, string> * pRegNames =
                    (regBlock == UphyIf::RegBlock::DLN) ? &m_DlmRegisterNames :
                                                          &m_ClmRegisterNames;

                string mismatchStr = Utility::StrPrintf("%s : %s %s BKV mismatch",
                                                        GetBoundTestDevice()->GetName().c_str(),
                                                        UphyIf::InterfaceToString(interface),
                                                        UphyIf::RegBlockToString(regBlock));
                if (interface == UphyIf::Interface::LwLink)
                {
                    if (regBlock == UphyIf::RegBlock::CLN)
                    {
                        mismatchStr += Utility::StrPrintf(" on IOCTRL %u\n", lwrRegBlock);
                    }
                    else
                    {
                        mismatchStr += Utility::StrPrintf(" on link %u, lane %u\n",
                                                          lwrRegBlock, lwrLane);
                    }
                }
                else if (regBlock == UphyIf::RegBlock::DLN)
                {
                    mismatchStr += Utility::StrPrintf(" on lane %u\n", lwrLane);
                }
                else
                {
                    mismatchStr += "\n";
                }
                mismatchStr += "  BKV Name         : " + bkvInfo.name + "\n";
                mismatchStr += Utility::StrPrintf("  BKV Version      : %.1f\n",
                                                  bkvInfo.bkvVersion);
                mismatchStr += Utility::StrPrintf("  Protocol Version : %.1f\n",
                                                  bkvInfo.protocolVersion);
                if (!pRegNames->empty())
                {
                    mismatchStr +=
                        "                             Register      Addr    Expected    Actual\n";
                    mismatchStr +=
                        "  -------------------------------------------------------------------\n";
                }
                else
                {
                    mismatchStr += "    Addr    Expected    Actual\n";
                    mismatchStr += "  ----------------------------\n";
                }
                for (auto const & lwrMismatch : regMismatches)
                {
                    if (!pRegNames->empty())
                    {
                        mismatchStr +=
                            Utility::StrPrintf("  %35s    0x%04x      0x%04x    0x%04x\n",
                                pRegNames->count(lwrMismatch.address) ?
                                    pRegNames->at(lwrMismatch.address).c_str() : "",
                                lwrMismatch.address,
                                lwrMismatch.expectedValue,
                                lwrMismatch.actualValue);
                    }
                    else
                    {
                        mismatchStr += Utility::StrPrintf("  0x%04x      0x%04x    0x%04x\n",
                                                          lwrMismatch.address,
                                                          lwrMismatch.expectedValue,
                                                          lwrMismatch.actualValue);
                    }
                }
                Printf(Tee::PriError, "\n%s\n", mismatchStr.c_str());
                if (bStopOnError)
                    return RC::GOLDEN_VALUE_MISCOMPARE;
                m_DeferredRc = RC::GOLDEN_VALUE_MISCOMPARE;
            }

            if (m_VerifyOneInstance)
                return rc;
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_INHERIT(VerifyUphyBkv, LwLinkTest, "UPHY BKV verification test");
CLASS_PROP_READWRITE(VerifyUphyBkv, VerifyInterface, UINT08,
    "UPHY interface to run the test on : 0=Pcie, 1=LwLink, 2=All (default = All)");
CLASS_PROP_READWRITE(VerifyUphyBkv, VerifyRegBlock, UINT08,
    "UPHY register block to run the test on : 0=CLM, 1=DLM, 2=All (default = All)");
CLASS_PROP_READWRITE(VerifyUphyBkv, ClmBkvVersion, FLOAT32,
    "CLM BKV version to use when validating (default = Use version from latest top level BKV)");
CLASS_PROP_READWRITE(VerifyUphyBkv, DlmBkvVersion, FLOAT32,
    "DLM BKV version to use when validating (default = Use version from latest top level BKV)");
CLASS_PROP_READWRITE(VerifyUphyBkv, LwLinkClmProtocolBkvVersion, FLOAT32,
    "LwLink Protocol CLM BKV version to use when validating (default = Use version from latest top level BKV)");
CLASS_PROP_READWRITE(VerifyUphyBkv, LwLinkDlmProtocolBkvVersion, FLOAT32,
    "LwLink Protocol CLM BKV version to use when validating (default = Use version from latest top level BKV)");
CLASS_PROP_READWRITE(VerifyUphyBkv, PcieClmProtocolBkvVersion, FLOAT32,
    "Pcie Protocol CLM BKV version to use when validating (default = Use version from latest top level BKV)");
CLASS_PROP_READWRITE(VerifyUphyBkv, PcieDlmProtocolBkvVersion, FLOAT32,
    "Pcie Protocol DLM BKV version to use when validating (default = Use version from latest top level BKV)");
CLASS_PROP_READWRITE(VerifyUphyBkv, VerifyOneInstance, bool,
    "Verify only one instance of each interface/reg block (default = false)");
