/* LWIDIA_COPYRIGHT_BEGIN                                                 */
/*                                                                        */
/* Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All  */
/* information contained herein is proprietary and confidential to LWPU */
/* Corporation.  Any use, reproduction, or disclosure without the written */
/* permission of LWPU Corporation is prohibited.                        */
/*                                                                        */
/* LWIDIA_COPYRIGHT_END                                                   */

#include "gpu/uphy/uphyreglogger.h"

#include "core/include/gpu.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/mle_protobuf.h"
#include "core/include/registry.h"
#include "core/include/script.h"
#include "core/include/types.h"
#include "device/interface/c2c.h"
#include "device/interface/c2c/c2lwphy.h"
#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlinkuphy.h"
#include "device/interface/pcie.h"
#include "device/interface/pcie/pcieuphy.h"
#include "device/interface/uphyif.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/lwlink/lwlink_mle_colw.h"
#include "gpu/uphy/uphyregloglists.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "lwmisc.h"
#include "lwRmReg.h"

#include <vector>
#include <map>

namespace
{
    #define GET_PERF_DATA(f)               \
        rc = f;                            \
        if (rc == RC::LWRM_NOT_SUPPORTED)  \
        {                                  \
            rc.Clear();                    \
        }                                  \
        else                               \
        {                                  \
            CHECK_RC(rc);                  \
        }

    using namespace UphyRegLogger;

    enum class RegLogType : UINT08
    {
        Short,
        Long,
        Last = Long
    };

    typedef vector<vector<vector<INT32>>>  EomDataFlattened;
    typedef vector<vector<vector<UINT16>>> RegData;

    // Uphy log lists that are generated from the UPHY/MSDV teams
    struct UphyLogLists
    {
        const UINT16* pShortList    = nullptr;
        size_t        shortListSize = 0;
        const UINT16* pLongList     = nullptr;
        size_t        longListSize  = 0;
    };
    map<UphyIf::Version, UphyLogLists> s_DLNUphyRegLogLists;
    map<UphyIf::Version, UphyLogLists> s_CLNUphyRegLogLists;
    map<UphyIf::Version, UphyLogLists> s_C2CGphyRegLogLists;

    // Command line provided PHY log lists
    vector<UINT16> s_LwstomDLNRegLogList;
    vector<UINT16> s_LwstomCLNRegLogList;
    vector<UINT16> s_LwstomC2CRegLogList;

    bool             s_Initialized       = false;
    RegLogType       s_RegLogType        = RegLogType::Short;
    UphyIf::RegBlock s_LoggedRegBlock    = UphyIf::RegBlock::ALL;
    UphyInterface    s_LoggedInterface   = UphyRegLogger::UphyInterface::All;
    string           s_TestType          = "modsfunctional";
    string           s_ReportSuffix      = "";
    FLOAT64          s_BgLogPeriodMs     = 0.0;
    Tasker::ThreadID s_BgLogThreadID     = Tasker::NULL_THREAD;
    bool             s_ExitBgLogThread   = false;
    RC               s_BgLogRc;
    UINT32           s_LogPointMask      = 0;
    bool             s_LoggingEnabled    = false;

    //-------------------------------------------------------------------------
    UphyIf * GetUphyInterface(TestDevicePtr pTestDevice, UphyRegLogger::UphyInterface interface)
    {
        UphyIf * pUphy = nullptr;
        switch (interface)
        {
            case UphyInterface::Pcie:
                pUphy = pTestDevice->GetInterface<PcieUphy>();
                break;
            case UphyInterface::LwLink:
                if (pTestDevice->SupportsInterface<LwLink>())
                {
                    pUphy = pTestDevice->GetInterface<LwLinkUphy>();
                }
                break;
            case UphyInterface::C2C:
                if (pTestDevice->SupportsInterface<C2C>())
                {
                    pUphy = pTestDevice->GetInterface<C2LWphy>();
                }
                break;
            default:
                break;
        }
        return pUphy;
    }

    //-------------------------------------------------------------------------
    const UINT16 * GetRegLogList
    (
        UphyIf *                      pUphy,
        UphyRegLogger::UphyInterface  interface,        
        UphyIf::RegBlock              regBlock,
        RegLogType                    logType,
        size_t *                      pRegLogSize
    )
    {
        // This should be caught well before this function, just sanity check it here
        MASSERT((regBlock == UphyIf::RegBlock::CLN) ||
                (regBlock == UphyIf::RegBlock::DLN));
        MASSERT(pRegLogSize);

        UphyIf::Version ver = pUphy->GetVersion();

        const UINT16 *pRegLogList = nullptr;

        if (interface == UphyRegLogger::UphyInterface::C2C)
        {
            // C2C doesnt have a CLN equivalent
            if (regBlock == UphyIf::RegBlock::CLN)
                return nullptr;

            pRegLogList = &s_LwstomC2CRegLogList[0];
            *pRegLogSize = s_LwstomC2CRegLogList.size();
        }
        else
        {
            pRegLogList = (regBlock == UphyIf::RegBlock::CLN)? &s_LwstomCLNRegLogList[0] :
                                                               &s_LwstomDLNRegLogList[0];
            *pRegLogSize = (regBlock == UphyIf::RegBlock::CLN) ? s_LwstomCLNRegLogList.size() :
                                                                 s_LwstomDLNRegLogList.size();
        }
        if (*pRegLogSize == 0)
        {
            map<UphyIf::Version, UphyLogLists> * pUphyRegLogLists = nullptr;
            if (interface == UphyRegLogger::UphyInterface::C2C)
            {
                pUphyRegLogLists = &s_C2CGphyRegLogLists;
            }
            else
            {
                pUphyRegLogLists = (regBlock == UphyIf::RegBlock::CLN) ? &s_CLNUphyRegLogLists :
                                                                         &s_DLNUphyRegLogLists;
            }
            auto logLists = pUphyRegLogLists->find(ver);

            if (logLists != pUphyRegLogLists->end())
            {

                if (logType == RegLogType::Short)
                {
                    pRegLogList = logLists->second.pShortList;
                    *pRegLogSize = logLists->second.shortListSize;
                }

                // Lists will always exist (due to how the script to generate them works) but may
                // be empty, if the short list is empty just use the long list which will be all
                // registers
                if ((logType == RegLogType::Long) || (*pRegLogSize == 0))
                {
                    pRegLogList = logLists->second.pLongList;
                    *pRegLogSize = logLists->second.longListSize;
                }
            }
        }
        if (pRegLogList == nullptr)
        {
            Printf(Tee::PriWarn,
                   "No %s %s UPHY register log list list found, no registers will be logged\n",
                   UphyIf::VersionToString(pUphy->GetVersion()),
                   UphyIf::RegBlockToString(regBlock));
        }
        return pRegLogList;
    }

    //-------------------------------------------------------------------------
    RC ReadRegBlock
    (
        UphyIf * pUphy,
        UphyIf::RegBlock block,
        const UINT16 * pLoggedAddrs,
        size_t addrSize,
        vector<vector<vector<UINT16>>> * pRegLog
    )
    {
        // Should have been caught well before this point
        MASSERT(pUphy);
        MASSERT((block == UphyIf::RegBlock::DLN) || (block == UphyIf::RegBlock::CLN));
        MASSERT(pRegLog);

        pRegLog->clear();

        RC rc;
        const UINT64 regBlockMask = pUphy->GetRegLogRegBlockMask(block);
        const UINT32 laneMask = pUphy->GetActiveLaneMask(block);
        UINT64 activeRegBlocks = 0;

        // The size of the register log is important.
        if (block == UphyIf::RegBlock::CLN)
        {
            // CLN registers do not have lanes like DLN registers, for CLN registers treat the
            // outermost array as the lanes, and use the first inner array as the register
            // block index.  This allows us to extract the 2 dimensional CLN data later without
            // any other post processing or custom code
            //
            // For CLN registers each entry in the inner 2d array must at least contain an empty
            // vector (no data gathered for the ioctl) in order to properly log the data to MLE
            // MLE needs to use index as an ioctl number
            pRegLog->resize(1);
            pRegLog->at(0).resize(pUphy->GetMaxRegBlocks(block));
            for (UINT32 lwrRegBlock = 0;
                  lwrRegBlock < pUphy->GetMaxRegBlocks(block); ++lwrRegBlock)
            {
                bool bIsActive;
                CHECK_RC(pUphy->IsRegBlockActive(block, lwrRegBlock, &bIsActive));
                if (bIsActive)
                    activeRegBlocks |= (1ULL << lwrRegBlock);
            }
        }
        else
        {
            // For DLN registers each entry in the outer array must at least contain an empty
            // 2d array (no data gathered for the link) and if data is present for the link,
            // the first inner array must contain an entry for each lane even if empty.
            // MLE needs to use the outer index as a link number and the first inner index as a
            // lane number
            pRegLog->resize(pUphy->GetMaxRegBlocks(block));
            for (UINT32 lwrRegBlock = 0;
                  lwrRegBlock < pUphy->GetMaxRegBlocks(block); ++lwrRegBlock)
            {
                bool bIsActive;
                CHECK_RC(pUphy->IsRegBlockActive(block, lwrRegBlock, &bIsActive));
                if (((regBlockMask & (1ULL << lwrRegBlock))) &&
                    pUphy->GetMaxLanes(block) && bIsActive)
                {
                    pRegLog->at(lwrRegBlock).resize(pUphy->GetMaxLanes(block));
                    activeRegBlocks |= (1ULL << lwrRegBlock);
                }
            }
        }

        for (UINT32 lwrRegBlock = 0; lwrRegBlock < pUphy->GetMaxRegBlocks(block); ++lwrRegBlock)
        {
            if ((!(regBlockMask & (1ULL << lwrRegBlock))) ||
                !(activeRegBlocks & (1ULL << lwrRegBlock)))
            {
                continue;
            }

            for (UINT32 lwrLane = 0; lwrLane < pUphy->GetMaxLanes(block); ++lwrLane)
            {
                if (!(laneMask & (1 << lwrLane)))
                    continue;

                for (size_t lwrAddrIdx = 0; lwrAddrIdx < addrSize; lwrAddrIdx++)
                {
                    UINT16 regData;
                    if (block == UphyIf::RegBlock::CLN)
                    {
                        MASSERT(lwrLane == 0);
                        rc = pUphy->ReadPllReg(lwrRegBlock,
                                               pLoggedAddrs[lwrAddrIdx],
                                               &regData);
                        if (rc == RC::PRIV_LEVEL_VIOLATION)
                        {
                            if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
                            {
                                Printf(Tee::PriWarn,
                                       "Cannot read UPHY PLL registers due to priv protection!\n"
                                       "Disabling UPHY PLL data gathering.\n");
                            }
                            s_LoggedRegBlock = UphyIf::RegBlock::DLN;
                            pRegLog->clear();
                            return RC::OK;
                        }
                        CHECK_RC(rc);

                        // CLN doesnt have lanes, use lane as the first index so the inner 2d
                        // array can be easily extracted
                        pRegLog->at(lwrLane).at(lwrRegBlock).push_back(regData);
                    }
                    else
                    {
                        CHECK_RC(pUphy->ReadPadLaneReg(lwrRegBlock,
                                                       lwrLane,
                                                       pLoggedAddrs[lwrAddrIdx],
                                                       &regData));
                        pRegLog->at(lwrRegBlock).at(lwrLane).push_back(regData);
                    }
                }
            }
        }
        return rc;
    }

    //-------------------------------------------------------------------------
    RC LogDataToMle(TestDevicePtr pTestDevice,
                    UphyIf::Version version,
                    UphyInterface intfc,
                    LogPoint logPoint,
                    RC errorCode,
                    bool bLogDevicePerfData,
                    const vector<Mle::UphyRegLog::FomMode> & mleFomModes,
                    EomDataFlattened * pMleEomData,
                    const UINT16 * pDlnAddrs,
                    size_t dlnAddrSize,
                    RegData * pDllwalues,
                    const UINT16 * pClnAddrs,
                    size_t clnAddrSize,
                    vector<vector<UINT16>> * pCllwalues)
    {
        RC rc;
        string ecidStr;
        string biosVer;
        string board;
        string boardVer;
        string boardSer;
        string ateIddqStr;
        string ateSpeedoStr;
        string perfPoint;
        UINT32 lwvddMv = 0U;
        UINT64 gpcClkKhz = 0ULL;
        UINT64 dramClkKhz = 0ULL;
        UINT64 hostClkKhz = 0ULL;
        UINT64 sysClkKhz = 0ULL;
        UINT64 pwrClkKhz = 0ULL;
        UINT64 dispClkKhz = 0ULL;
        FLOAT32 tempC = 0.0;

        // Gather all the data necessary for the associated MLE data, some of this would be
        // available through other MLE logs (e.g. data from header), however MLE/MLA do not
        // yet support LwSwitch devices, so it is necessary to explicitly gather it so that
        // the MLA reports will work for switch devices
        CHECK_RC(pTestDevice->GetEcidString(&ecidStr));

        UINT32 ateIddq;
        UINT32 ateVer;
        if (pTestDevice->GetAteIddq(&ateIddq, &ateVer) == RC::OK)
            ateIddqStr = Utility::StrPrintf("%u mA (Ver %u)", ateIddq, ateVer);

        vector<UINT32> speedoValues;
        if (pTestDevice->GetAteSpeedos(&speedoValues, &ateVer) == RC::OK)
        {
            for (auto const & lwrSpeedo : speedoValues)
            {
                ateSpeedoStr += Utility::StrPrintf("%u ", lwrSpeedo);
            }
            ateSpeedoStr += Utility::StrPrintf("(Version:%u)", ateVer);
        }

        if (bLogDevicePerfData)
        {
            vector<FLOAT32> temps;
            CHECK_RC(pTestDevice->GetChipTemps(&temps));
            tempC = temps[0];
        }

        auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
        if (pSubdev != nullptr)
        {
            biosVer = pSubdev->GetRomVersion();
            string boardProjectSer = pSubdev->GetBoardProjectSerialNumber();
            if (!boardProjectSer.empty())
            {
                vector<string> boardData;
                CHECK_RC(Utility::Tokenizer(boardProjectSer, "-", &boardData));

                if (boardData.size() > 0)
                    board = boardData[0];
                if (boardData.size() > 1)
                    boardVer = boardData[1];
                if (boardData.size() > 2)
                    boardSer = boardData[2];
            }

            if (bLogDevicePerfData)
            {
                perfPoint = pSubdev->GetPerf()->GetInflectionPtStr();
                GET_PERF_DATA(pSubdev->GetPerf()->GetCoreVoltageMv(&lwvddMv));
                GET_PERF_DATA(pSubdev->GetClock(Gpu::ClkGpc, &gpcClkKhz, nullptr, nullptr, nullptr));
                GET_PERF_DATA(pSubdev->GetClock(Gpu::ClkM, &dramClkKhz, nullptr, nullptr, nullptr));
                GET_PERF_DATA(pSubdev->GetClock(Gpu::ClkHost, &hostClkKhz, nullptr, nullptr, nullptr));
                GET_PERF_DATA(pSubdev->GetClock(Gpu::ClkSys, &sysClkKhz, nullptr, nullptr, nullptr));
                GET_PERF_DATA(pSubdev->GetClock(Gpu::ClkPwr, &pwrClkKhz, nullptr, nullptr, nullptr));
                GET_PERF_DATA(pSubdev->GetClock(Gpu::ClkDisp, &dispClkKhz, nullptr, nullptr, nullptr));
            }
        }
        else if (bLogDevicePerfData)
        {
            // Ignore the return, voltage queries are not always available on switch devices
            (void)pTestDevice->GetVoltageMv(&lwvddMv);
        }
        UINT32 lineRateMbps = 0;
        UINT32 maxLanes = 0;
        UINT32 maxLinks = 0;
        UINT32 linksPerCln = 0;
        switch (intfc)
        {
            case UphyInterface::Pcie:
            {
                auto pPcie = pTestDevice->GetInterface<Pcie>();
                auto pPcieUphy = pPcie->GetInterface<PcieUphy>();
                lineRateMbps = static_cast<UINT32>(pPcie->GetLinkSpeed(Pci::SpeedUnknown));
                maxLanes = pPcieUphy->GetMaxLanes(UphyIf::RegBlock::DLN);
                maxLinks = pPcieUphy->GetMaxRegBlocks(UphyIf::RegBlock::DLN);
                linksPerCln = 1;
            }
            break;
            case UphyInterface::LwLink:
            {
                auto pLwLink = pTestDevice->GetInterface<LwLink>();
                auto pLwLinkUphy = pLwLink->GetInterface<LwLinkUphy>();
                for (UINT32 lwrLink = 0;
                      (lwrLink < pLwLink->GetMaxLinks()) && !lineRateMbps; lwrLink++)
                {
                    if (pLwLink->IsLinkActive(lwrLink))
                        lineRateMbps = pLwLink->GetLineRateMbps(lwrLink);
                }
                maxLanes = pLwLinkUphy->GetMaxLanes(UphyIf::RegBlock::DLN);
                maxLinks = pLwLinkUphy->GetMaxRegBlocks(UphyIf::RegBlock::DLN);
                linksPerCln = pLwLinkUphy->GetMaxRegBlocks(UphyIf::RegBlock::DLN) /
                              pLwLinkUphy->GetMaxRegBlocks(UphyIf::RegBlock::CLN);
            }
            break;
            case UphyInterface::C2C:
            {
                auto pC2C = pTestDevice->GetInterface<C2C>();
                auto pC2LWphy = pC2C->GetInterface<C2LWphy>();
                for (UINT32 lwrPartition = 0;
                      (lwrPartition < pC2C->GetMaxPartitions()) && !lineRateMbps; lwrPartition++)
                {
                    if (pC2C->IsPartitionActive(lwrPartition))
                         CHECK_RC(pC2C->GetLineRateMbps(lwrPartition, &lineRateMbps));
                }
                maxLanes = pC2LWphy->GetMaxLanes(UphyIf::RegBlock::DLN);
                maxLinks = pC2LWphy->GetMaxRegBlocks(UphyIf::RegBlock::DLN);
                linksPerCln = 0;
            }
            break;
            default:
                Printf(Tee::PriError, "Unknown UPHY interface %u\n", static_cast<UINT32>(intfc));
                MASSERT(!"See previous message");
                return RC::SOFTWARE_ERROR;
        }

        string topologyFile;
#if defined(INCLUDE_LWLINK)
        topologyFile = LwLinkDevIf::TopologyManager::GetTopologyFile();
#endif
        vector<UINT16> dlnAddrs;
        if (pDlnAddrs != nullptr)
            dlnAddrs.insert(dlnAddrs.begin(), pDlnAddrs, pDlnAddrs + dlnAddrSize);
        vector<UINT16> clnAddrs;
        if (pClnAddrs != nullptr)
            clnAddrs.insert(clnAddrs.begin(), pClnAddrs, pClnAddrs + clnAddrSize);

        vector<UINT16> dlnCfgIndexes;
        dlnCfgIndexes.resize(dlnAddrs.size(), 0);

        Mle::UphyRegLog::Version versionEnum = Mle::UphyRegLog::unknown_ver;
        switch (version)
        {
            case UphyIf::Version::V10: versionEnum = Mle::UphyRegLog::v10; break;
            case UphyIf::Version::V30: versionEnum = Mle::UphyRegLog::v30; break;
            case UphyIf::Version::V31: versionEnum = Mle::UphyRegLog::v31; break;
            case UphyIf::Version::V32: versionEnum = Mle::UphyRegLog::v32; break;
            case UphyIf::Version::V50: versionEnum = Mle::UphyRegLog::v50; break;
            case UphyIf::Version::V61: versionEnum = Mle::UphyRegLog::v61; break;
            default:
                MASSERT(version == UphyIf::Version::UNKNOWN);
                break;
        }

        Mle::UphyRegLog::LogPoint logPointEnum = Mle::UphyRegLog::unknown;
        switch (logPoint)
        {
            case UphyRegLogger::LogPoint::EOM:              logPointEnum = Mle::UphyRegLog::eom; break;
            case UphyRegLogger::LogPoint::BandwidthTest:    logPointEnum = Mle::UphyRegLog::bandwidth_test; break;
            case UphyRegLogger::LogPoint::PreTest:          logPointEnum = Mle::UphyRegLog::pre_test; break;
            case UphyRegLogger::LogPoint::PostTest:         logPointEnum = Mle::UphyRegLog::post_test; break;
            case UphyRegLogger::LogPoint::PostTestError:    logPointEnum = Mle::UphyRegLog::post_test_error; break;
            case UphyRegLogger::LogPoint::PostLinkTraining: logPointEnum = Mle::UphyRegLog::post_link_training; break;
            case UphyRegLogger::LogPoint::Manual:           logPointEnum = Mle::UphyRegLog::manual; break;
            case UphyRegLogger::LogPoint::Background:       logPointEnum = Mle::UphyRegLog::background; break;
            default:
                MASSERT(!"Unknown UPHY log point");
                break;
        }

        Mle::UphyRegLog::Interface intfcEnum = Mle::UphyRegLog::unknown_if;
        switch (intfc)
        {
            case UphyInterface::LwLink: intfcEnum = Mle::UphyRegLog::lwlink; break;
            case UphyInterface::Pcie:   intfcEnum = Mle::UphyRegLog::pcie;   break;
            case UphyInterface::C2C:    intfcEnum = Mle::UphyRegLog::c2c;    break;
            default:
                MASSERT(!"Unknown UPHY interface");
                break;
        }

        ByteStream bs;
        auto entry = Mle::Entry::uphy_reg_log(&bs);
        entry
            .device_name(pTestDevice->GetName())
            .chip_name(pTestDevice->DeviceIdString())
            .test_type(s_TestType)
            .report_suffix(s_ReportSuffix)
            .ecid(ecidStr)
            .bios_ver(biosVer)
            .board(board)
            .board_ver(boardVer)
            .board_serial(boardSer)
            .topology_file(topologyFile)
            .ate_iddq(ateIddqStr)
            .ate_speedo(ateSpeedoStr)
            .interface(intfcEnum)
            .links_per_cln(linksPerCln)
            .max_links(maxLinks)
            .max_lanes(maxLanes)
            .line_rate_mbps(lineRateMbps)
            .perf_point(perfPoint)
            .lwvdd_mv(lwvddMv)
            .gpc_clk_khz(static_cast<UINT32>(gpcClkKhz))
            .dram_clk_khz(static_cast<UINT32>(dramClkKhz))
            .host_clk_khz(static_cast<UINT32>(hostClkKhz))
            .sys_clk_khz(static_cast<UINT32>(sysClkKhz))
            .pwr_clk_khz(static_cast<UINT32>(pwrClkKhz))
            .disp_clk_khz(static_cast<UINT32>(dispClkKhz))
            .temp_c(tempC)
            .version(versionEnum)
            .log_point(logPointEnum)
            .error_code(errorCode.Get())
            .cln_address(clnAddrs);

        for (UINT32 fomIdx = 0; fomIdx < mleFomModes.size(); fomIdx++)
        {
            auto fomEntry = entry.fom();
            fomEntry.mode(static_cast<Mle::UphyRegLog::FomMode>(mleFomModes[fomIdx]));
            MASSERT(fomIdx < pMleEomData->size());
            for (const vector<INT32>& link : (*pMleEomData)[fomIdx])
            {
                fomEntry.link().eom_per_lane(link);
            }
        }

        for (const vector<UINT16>& cln : *pCllwalues)
        {
            entry.cln().cln_value_per_addr(cln);
        }

        for (UINT32 dlnIdx = 0; dlnIdx < dlnAddrs.size(); dlnIdx++)
        {
            MASSERT(dlnIdx < dlnCfgIndexes.size());
            entry.dln_address()
                .address(dlnAddrs[dlnIdx])
                .cfg_index(dlnCfgIndexes[dlnIdx]);
        }

        for (const vector<vector<UINT16>>& link : *pDllwalues)
        {
            auto linkEntry = entry.dln_link();
            for (const vector<UINT16>& lane : link)
            {
                linkEntry.lane().dln_value_per_addr(lane);
            }
        }

        entry.Finish();
        Tee::PrintMle(&bs);

        return rc;
    }

    //-------------------------------------------------------------------------
    RC LogInterfaces
    (
        TestDevicePtr                            pTestDevice,
        const vector<UphyInterface> &            interfacesToLog,
        UphyRegLogger::LogPoint                  logPoint,
        RC                                       errorCode,
        bool                                     bLogDevicePerfData,
        UphyInterface                            eomDataInterface,        
        const vector<Mle::UphyRegLog::FomMode> & mleFomModes,
        EomDataFlattened *                       pMleEomData
    )
    {
        RC rc;
        vector<Mle::UphyRegLog::FomMode> emptyFomModes;
        EomDataFlattened emptyEomData;

        for (const auto & lwrInterface : interfacesToLog)
        {
            const UINT16 * pClnAddresses = nullptr;
            size_t  clnAddrSize = 0;
            RegData cllwalues;
            const UINT16 * pDlnAddresses = nullptr;
            size_t  dlnAddrSize = 0;
            RegData dllwalues;

            UphyIf * pUphy = GetUphyInterface(pTestDevice, lwrInterface);
            if ((s_LoggedRegBlock == UphyIf::RegBlock::ALL) ||
                (s_LoggedRegBlock == UphyIf::RegBlock::DLN))
            {
                pDlnAddresses = GetRegLogList(pUphy,
                                              lwrInterface,
                                              UphyIf::RegBlock::DLN,
                                              s_RegLogType,
                                              &dlnAddrSize);
                if (pDlnAddresses != nullptr)
                {
                    CHECK_RC(ReadRegBlock(pUphy,
                                          UphyIf::RegBlock::DLN,
                                          pDlnAddresses,
                                          dlnAddrSize,
                                          &dllwalues));
                }
            }

            if (s_LoggedRegBlock == UphyIf::RegBlock::ALL)
            {
                pClnAddresses = GetRegLogList(pUphy,
                                              lwrInterface,
                                              UphyIf::RegBlock::CLN,
                                              s_RegLogType,
                                              &clnAddrSize);
                if (pClnAddresses != nullptr)
                {
                    CHECK_RC(ReadRegBlock(pUphy,
                                          UphyIf::RegBlock::CLN,
                                          pClnAddresses,
                                          clnAddrSize,
                                          &cllwalues));
                }
            }

            // Need an empty inner entry
            if (cllwalues.empty())
            {
                cllwalues.push_back(vector<vector<UINT16>>());

                // CLN block can only be read on non-priv protected parts, when CLN is
                // requested on a priv protected part a warning is printed an CLN logging
                // is disabled.  In this case cllwalues will be empty.  In order to be
                // consistent with CLN logging disabled, ensure that the address list is
                // also cleared otherwise MLA will expect data
                pClnAddresses = nullptr;
                clnAddrSize = 0;
            }

            CHECK_RC(LogDataToMle(pTestDevice,
                                  pUphy->GetVersion(),
                                  lwrInterface,
                                  logPoint,
                                  errorCode,
                                  bLogDevicePerfData,
                                  (lwrInterface == eomDataInterface) ? mleFomModes : emptyFomModes,
                                  (lwrInterface == eomDataInterface) ? pMleEomData : &emptyEomData,
                                  pDlnAddresses,
                                  dlnAddrSize,
                                  &dllwalues,
                                  pClnAddresses,
                                  clnAddrSize,
                                  &cllwalues[0]));
        }
        return rc;
    }

    //--------------------------------------------------------------------------
    vector<UphyInterface> GetInterfacesToLog(TestDevicePtr pTestDevice, UphyInterface interface)
    {
        const UINT08 startIntfc =
            (interface == UphyInterface::All) ? 0 : static_cast<UINT08>(interface);
        const UINT08 endIntfc =
            (interface == UphyInterface::All) ? static_cast<UINT08>(UphyInterface::All) :
                                                static_cast<UINT08>(interface) + 1;
        vector<UphyInterface> interfacesToLog;

        for (UINT08 lwrIntfc = startIntfc; lwrIntfc < endIntfc; ++lwrIntfc)
        {
            const UphyInterface eLwrIntfc = static_cast<UphyInterface>(lwrIntfc);
            if ((s_LoggedInterface != UphyInterface::All) && (s_LoggedInterface != eLwrIntfc))
            {
                continue;
            }

            UphyIf * pUphy = GetUphyInterface(pTestDevice, eLwrIntfc);
            if (pUphy == nullptr)
                continue;

            if (!pUphy->GetRegLogEnabled())
                continue;

            interfacesToLog.push_back(eLwrIntfc);
        }
        return interfacesToLog;
    }

    void BackgroundUphyLogThread(void *)
    {
        while (!s_ExitBgLogThread && (s_BgLogRc == RC::OK))
        {
            // The thread can startup before data can potentially be logged
            // Wait for GPU initialization
            if (!GpuPtr()->IsInitialized())
            {
                Tasker::Sleep(100);
                continue;
            }

            // Even after initialization it is necessary to wait for full GPU manager
            // initialization in order to log perf data
            GpuDevMgr *const pGpuMgr = DevMgrMgr::d_GraphDevMgr;
            s_BgLogRc = ForceLogRegs(UphyRegLogger::UphyInterface::All,
                                     UphyRegLogger::LogPoint::Background,
                                     RC::OK,
                                     pGpuMgr->IsInitialized());
            if (s_BgLogRc == RC::OK)
            {
                Tasker::Sleep(s_BgLogPeriodMs);
            }
            else
            {
                Printf(Tee::PriError, "UPHY background logger thread exiting due to failure\n");
            }
        }
    }
}

//------------------------------------------------------------------------------
FLOAT64 UphyRegLogger::GetBgLogPeriodMs()
{
    return s_BgLogPeriodMs;
}

//------------------------------------------------------------------------------
UINT08 UphyRegLogger::GetLoggedInterface()
{
    return static_cast<UINT08>(s_LoggedInterface);
}

//------------------------------------------------------------------------------
UINT08 UphyRegLogger::GetLoggedRegBlock()
{
    return static_cast<UINT08>(s_LoggedRegBlock);
}

//------------------------------------------------------------------------------
bool UphyRegLogger::GetLoggingEnabled()
{
    return s_LoggingEnabled;
}

//------------------------------------------------------------------------------
UINT32 UphyRegLogger::GetLogPointMask()
{
    return s_LogPointMask;
}

//------------------------------------------------------------------------------
UINT08 UphyRegLogger::GetLogType()
{
    return static_cast<UINT08>(s_RegLogType);
}

//------------------------------------------------------------------------------
string UphyRegLogger::GetReportSuffix()
{
    return s_ReportSuffix;
}

//------------------------------------------------------------------------------
string UphyRegLogger::GetTestType()
{
    return s_TestType;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::Initialize()
{
    if (s_Initialized)
        return RC::OK;

#define DEFINE_UPHY(uphyVersion, uphyBlock, verEnum)                           \
    s_ ## uphyBlock ## UphyRegLogLists[ verEnum ].pShortList =                 \
        UphyRegLogLists::g_Uphy ## uphyVersion ## uphyBlock ## ShortDump;      \
    s_ ## uphyBlock ## UphyRegLogLists[ verEnum ].shortListSize =              \
        UphyRegLogLists::g_Uphy ## uphyVersion ## uphyBlock ## ShortDumpSize;  \
    s_ ## uphyBlock ## UphyRegLogLists[ verEnum ].pLongList =                  \
        UphyRegLogLists::g_Uphy ## uphyVersion ## uphyBlock ## LongDump;       \
    s_ ## uphyBlock ## UphyRegLogLists[ verEnum ].longListSize =               \
        UphyRegLogLists::g_Uphy ## uphyVersion ## uphyBlock ## LongDumpSize;
#define DEFINE_C2C_GPHY(uphyVersion, verEnum)                                  \
    s_C2CGphyRegLogLists[ verEnum ].pShortList =                               \
        UphyRegLogLists::g_C2CGphy ## uphyVersion ## ShortDump;                \
    s_C2CGphyRegLogLists[ verEnum ].shortListSize =                            \
        UphyRegLogLists::g_C2CGphy ## uphyVersion ## ShortDumpSize;            \
    s_C2CGphyRegLogLists[ verEnum ].pLongList =                                \
        UphyRegLogLists::g_C2CGphy ## uphyVersion ## LongDump;                 \
    s_C2CGphyRegLogLists[ verEnum ].longListSize =                             \
        UphyRegLogLists::g_C2CGphy ## uphyVersion ## LongDumpSize;
#include "uphy_list.h"
#undef DEFINE_UPHY
#undef DEFINE_C2C_GPHY

    if (s_BgLogPeriodMs != 0.0)
    {
        s_BgLogThreadID = Tasker::CreateThread(BackgroundUphyLogThread,
                                               0, 0, "UphyBackgroundLogger");
        if (s_BgLogThreadID == Tasker::NULL_THREAD)
        {
            Printf(Tee::PriError, "Unable to create uphy background logger thread\n");
            return RC::WAS_NOT_INITIALIZED;
        }
    }

    s_Initialized = true;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::LogRegs(UphyInterface interface, LogPoint logPoint, RC errorCode)
{
    RC rc;
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    for (UINT32 devInst = 0; devInst < pTestDeviceMgr->NumDevices(); devInst++)
    {
        TestDevicePtr pTestDevice;
        CHECK_RC(pTestDeviceMgr->GetDevice(devInst, &pTestDevice));
        CHECK_RC(LogRegs(pTestDevice, interface, logPoint, errorCode));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::LogRegs
(
    TestDevicePtr pTestDevice,
    UphyInterface interface,
    LogPoint      logPoint,
    RC            errorCode
)
{
    map<LwLink::FomMode, vector<vector<INT32>>> emptyEomData;
    return LogRegs(pTestDevice,
                   interface,
                   logPoint,
                   errorCode,
                   emptyEomData);
}

//------------------------------------------------------------------------------
RC UphyRegLogger::LogRegs
(
    TestDevicePtr pTestDevice,
    UphyInterface interface,
    LogPoint      logPoint,
    RC            errorCode,
    const map<LwLink::FomMode, vector<vector<INT32>>> & eomData
)
{
    vector<UphyInterface> interfacesToLog = GetInterfacesToLog(pTestDevice, interface);
    if (interfacesToLog.empty())
    {
        Printf(Tee::PriLow, "%s no uphy interfaces found to log, skipping\n",
               pTestDevice->GetName().c_str());
        return RC::OK;
    }

    vector<Mle::UphyRegLog::FomMode> mleFomModes;
    EomDataFlattened mleEomData;
    for (auto const & lwrEomData : eomData)
    {
        Mle::UphyRegLog::FomMode fomMode = Mle::ToEomFomMode(lwrEomData.first);
        mleFomModes.push_back(fomMode);
        mleEomData.push_back(move(lwrEomData.second));
    }

    return LogInterfaces(pTestDevice,
                         interfacesToLog,
                         logPoint,
                         errorCode,
                         true,
                         UphyInterface::LwLink,
                         mleFomModes,
                         &mleEomData);
}

//------------------------------------------------------------------------------
RC UphyRegLogger::LogRegs
(
    TestDevicePtr pTestDevice,
    UphyInterface interface,
    LogPoint      logPoint,
    RC            errorCode,
    const map<Pci::FomMode, vector<vector<INT32>>> & eomData
)
{
    vector<UphyInterface> interfacesToLog = GetInterfacesToLog(pTestDevice, interface);
    if (interfacesToLog.empty())
    {
        Printf(Tee::PriLow, "%s no uphy interfaces found to log, skipping\n",
               pTestDevice->GetName().c_str());
        return RC::OK;
    }

    vector<Mle::UphyRegLog::FomMode> mleFomModes;
    EomDataFlattened mleEomData;
    for (auto const & lwrEomData : eomData)
    {
        Mle::UphyRegLog::FomMode fomMode = Mle::UphyRegLog::unknown_fm;
        switch (lwrEomData.first)
        {
            case Pci::FomMode::EYEO_X:    fomMode = Mle::UphyRegLog::x; break;
            case Pci::FomMode::EYEO_Y:    fomMode = Mle::UphyRegLog::y; break;
            default:
                MASSERT(!"Unknown UPHY FomMode colwersion");
                break;
        }
        mleFomModes.push_back(fomMode);
        mleEomData.push_back(move(lwrEomData.second));
    }

    return LogInterfaces(pTestDevice,
                         interfacesToLog,
                         logPoint,
                         errorCode,
                         true,
                         UphyInterface::Pcie,
                         mleFomModes,
                         &mleEomData);
}

//------------------------------------------------------------------------------
RC UphyRegLogger::ForceLogRegs
(
    UphyInterface interface,
    LogPoint      logPoint,
    RC            errorCode,
    bool          bLogDevicePerfData
)
{
    RC rc;
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    for (UINT32 devInst = 0; devInst < pTestDeviceMgr->NumDevices(); devInst++)
    {
        TestDevicePtr pTestDevice;
        CHECK_RC(pTestDeviceMgr->GetDevice(devInst, &pTestDevice));
        CHECK_RC(ForceLogRegs(pTestDevice,
                              static_cast<UINT08>(interface),
                              logPoint,
                              errorCode,
                              bLogDevicePerfData,
                              static_cast<UINT08>(s_LoggedRegBlock),
                              static_cast<UINT08>(s_RegLogType)));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::ForceLogRegs
(
    TestDevicePtr pTestDevice,
    UINT08        uphyInterface,
    LogPoint      logPoint,
    RC            errorCode,
    bool          bLogDevicePerfData,
    UINT08        regBlock,
    UINT08        logType
)
{
    UphyInterface savedInterface   = s_LoggedInterface;
    UphyIf::RegBlock savedRegBlock = s_LoggedRegBlock;
    RegLogType savedLogType        = s_RegLogType;
    DEFER
    {
        s_LoggedInterface = savedInterface;
        s_LoggedRegBlock  = savedRegBlock;
        s_RegLogType      = savedLogType;
    };

    RC rc;
    CHECK_RC(SetLoggedInterface(uphyInterface));
    CHECK_RC(SetLoggedRegBlock(regBlock));
    CHECK_RC(SetLogType(logType));


    bool bSavedLwLinkEnable = false;
    if (pTestDevice->SupportsInterface<LwLinkUphy>())
    {
        auto pUphy = pTestDevice->GetInterface<LwLinkUphy>();
        bSavedLwLinkEnable = pUphy->GetRegLogEnabled();
        pUphy->SetRegLogEnabled(true);
    }

    bool bSavedPcieEnable = false;
    if (pTestDevice->SupportsInterface<PcieUphy>())
    {
        auto pUphy = pTestDevice->GetInterface<PcieUphy>();
        bSavedPcieEnable = pUphy->GetRegLogEnabled();
        pUphy->SetRegLogEnabled(true);
    }

    bool bSavedC2CEnable = false;
    if (pTestDevice->SupportsInterface<C2LWphy>())
    {
        auto pUphy = pTestDevice->GetInterface<C2LWphy>();
        bSavedC2CEnable = pUphy->GetRegLogEnabled();
        pUphy->SetRegLogEnabled(true);
    }

    DEFER
    {
        if (pTestDevice->SupportsInterface<LwLinkUphy>())
            pTestDevice->GetInterface<LwLinkUphy>()->SetRegLogEnabled(bSavedLwLinkEnable);

        if (pTestDevice->SupportsInterface<PcieUphy>())
            pTestDevice->GetInterface<PcieUphy>()->SetRegLogEnabled(bSavedPcieEnable);

        if (pTestDevice->SupportsInterface<C2LWphy>())
            pTestDevice->GetInterface<C2LWphy>()->SetRegLogEnabled(bSavedC2CEnable);
    };

    vector<UphyInterface> interfacesToLog =
        GetInterfacesToLog(pTestDevice, static_cast<UphyInterface>(uphyInterface));
    if (interfacesToLog.empty())
    {
        Printf(Tee::PriLow, "%s no uphy interfaces found to log, skipping\n",
               pTestDevice->GetName().c_str());
        return RC::OK;
    }

    vector<Mle::UphyRegLog::FomMode> mleFomModes;
    EomDataFlattened mleEomData;
    CHECK_RC(LogInterfaces(pTestDevice,
                           interfacesToLog,
                           logPoint,
                           errorCode,
                           bLogDevicePerfData,
                           UphyInterface::All, // Passing a non-specific eom interface causes no
                                               // eom data to be logged
                           mleFomModes,
                           &mleEomData));
    return rc;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetBgLogPeriodMs(FLOAT64 logPeriodMs)
{
    s_BgLogPeriodMs = logPeriodMs;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetC2CRegListToLog(const vector<UINT16> &regList)
{
    s_LwstomC2CRegLogList = regList;
    sort(s_LwstomC2CRegLogList.begin(), s_LwstomC2CRegLogList.end());
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetLoggedInterface(UINT08 uphyInterface)
{
    if (uphyInterface > static_cast<UINT08>(UphyInterface::All))
    {
        Printf(Tee::PriError, "Unknown UPHY interface : %u\n", uphyInterface);
        return RC::BAD_PARAMETER;
    }
    s_LoggedInterface = static_cast<UphyInterface>(uphyInterface);
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetLoggedRegBlock(UINT08 regBlock)
{
    if (regBlock > static_cast<UINT08>(UphyIf::RegBlock::ALL))
    {
        Printf(Tee::PriError, "Unknown UPHY register block : %u\n", regBlock);
        return RC::BAD_PARAMETER;
    }

    UphyIf::RegBlock eRegBlock = static_cast<UphyIf::RegBlock>(regBlock);
    if (eRegBlock == UphyIf::RegBlock::CLN)
    {
        Printf(Tee::PriError, "Logging only CLN registers is not supported\n");
        return RC::BAD_PARAMETER;
    }
    s_LoggedRegBlock = eRegBlock;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetLoggingEnabled(bool bLoggingEnabled)
{
    s_LoggingEnabled = bLoggingEnabled;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetLogPointMask(UINT32 logPointMask)
{
    if (logPointMask & ~(LogPoint::All))
    {
        Printf(Tee::PriError, "Invalid log point mask : 0x%x\n", logPointMask);
        return RC::BAD_PARAMETER;
    }
    s_LogPointMask = logPointMask;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetLogType(UINT08 logType)
{
    if (logType > static_cast<UINT08>(RegLogType::Last))
    {
        Printf(Tee::PriError, "Unknown UPHY register log type : %u\n", logType);
        return RC::BAD_PARAMETER;
    }
    s_RegLogType = static_cast<RegLogType>(logType);
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetRegListToLog(UINT08 regBlock, const vector<UINT16> &regList)
{
    UphyIf::RegBlock eRegBlock = static_cast<UphyIf::RegBlock>(regBlock);
    switch (eRegBlock)
    {
        case UphyIf::RegBlock::DLN:
            s_LwstomDLNRegLogList = regList;
            sort(s_LwstomDLNRegLogList.begin(), s_LwstomDLNRegLogList.end());
            break;
        case UphyIf::RegBlock::CLN:
            s_LwstomCLNRegLogList = regList;
            sort(s_LwstomCLNRegLogList.begin(), s_LwstomCLNRegLogList.end());
            break;
        default:
            Printf(Tee::PriError, "Invalid UPHY register block : %u\n", regBlock);
            return RC::BAD_PARAMETER;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetReportSuffix(string reportSuffix)
{
    s_ReportSuffix = reportSuffix;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC UphyRegLogger::SetTestType(string testType)
{
    s_TestType = testType;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC UphyRegLogger::Shutdown()
{
    StickyRC rc;
    if (s_BgLogThreadID != Tasker::NULL_THREAD)
    {
        if (Tasker::IsValidThreadId(s_BgLogThreadID))
        {
            s_ExitBgLogThread = true;
            rc = Tasker::Join(s_BgLogThreadID);
        }
        s_BgLogThreadID = Tasker::NULL_THREAD;
        rc = s_BgLogRc;
        if (s_BgLogRc != RC::OK)
        {
            Printf(Tee::PriError, "UPHY background logging thread failed : %d\n",
                   static_cast<INT32>(s_BgLogRc));
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Javascript class for UphyRegLogger
//!
JS_CLASS(UphyRegLogger);

//-----------------------------------------------------------------------------
//! \brief Javascript object for UphyRegLogger
//!
SObject UphyRegLogger_Object
(
   "UphyRegLogger",
   UphyRegLoggerClass,
   0,
   0,
   "Uphy register logging control"
);

PROP_CONST(UphyRegLogger, LI_PCIE,               static_cast<UINT32>(UphyRegLogger::UphyInterface::Pcie));      //$
PROP_CONST(UphyRegLogger, LI_LWLINK,             static_cast<UINT32>(UphyRegLogger::UphyInterface::LwLink));    //$
PROP_CONST(UphyRegLogger, LI_C2C,                static_cast<UINT32>(UphyRegLogger::UphyInterface::C2C));       //$
PROP_CONST(UphyRegLogger, LI_ALL,                static_cast<UINT32>(UphyRegLogger::UphyInterface::All));       //$
PROP_CONST(UphyRegLogger, LRB_CLN,               static_cast<UINT32>(UphyIf::RegBlock::CLN));
PROP_CONST(UphyRegLogger, LRB_DLN,               static_cast<UINT32>(UphyIf::RegBlock::DLN));
PROP_CONST(UphyRegLogger, LRB_ALL,               static_cast<UINT32>(UphyIf::RegBlock::ALL));
PROP_CONST(UphyRegLogger, LT_SHORT,              static_cast<UINT32>(RegLogType::Short));
PROP_CONST(UphyRegLogger, LT_LONG,               static_cast<UINT32>(RegLogType::Long));
PROP_CONST(UphyRegLogger, LP_EOM,                static_cast<UINT32>(LogPoint::EOM));
PROP_CONST(UphyRegLogger, LP_BW_TEST,            static_cast<UINT32>(LogPoint::BandwidthTest));
PROP_CONST(UphyRegLogger, LP_PRE_TEST,           static_cast<UINT32>(LogPoint::PreTest));
PROP_CONST(UphyRegLogger, LP_POST_TEST,          static_cast<UINT32>(LogPoint::PostTest));
PROP_CONST(UphyRegLogger, LP_POST_TEST_ERROR,    static_cast<UINT32>(LogPoint::PostTestError));
PROP_CONST(UphyRegLogger, LP_POST_LINK_TRAINING, static_cast<UINT32>(LogPoint::PostLinkTraining));

PROP_READWRITE_NAMESPACE(UphyRegLogger, LoggedInterface, UINT08,
    "Set interface being logged : Pcie, LwLink, or All (default = All)");
PROP_READWRITE_NAMESPACE(UphyRegLogger, LoggedRegBlock, UINT08,
    "Set register block being logged : CLN, DLN, or All (default = All)");
PROP_READWRITE_NAMESPACE(UphyRegLogger, LogType, UINT08,
    "Set register log type : Short, Long, or Custom (default = Short)");
PROP_READWRITE_NAMESPACE(UphyRegLogger, TestType, string,
    "Set a custom test type string for the report (default = modsfunctional)");
PROP_READWRITE_NAMESPACE(UphyRegLogger, ReportSuffix, string,
    "Set a custom report suffix string for the report (default = \"\")");
PROP_READWRITE_NAMESPACE(UphyRegLogger, BgLogPeriodMs, FLOAT64,
    "Set the background logging period for uphy data (default = 0.0 - disabled)");
PROP_READWRITE_NAMESPACE(UphyRegLogger, LogPointMask, UINT32,
    "Bitmask of points to log uphy registers (default = 0 - disabled)");
PROP_READWRITE_NAMESPACE(UphyRegLogger, LoggingEnabled, bool,
    "Indicates whether logging is enabled on any device (default = false)");

PROP_CONST(UphyRegLogger, UV_V10,      static_cast<UINT32>(UphyIf::Version::V10));
PROP_CONST(UphyRegLogger, UV_V30,      static_cast<UINT32>(UphyIf::Version::V30));
PROP_CONST(UphyRegLogger, UV_V31,      static_cast<UINT32>(UphyIf::Version::V31));
PROP_CONST(UphyRegLogger, UV_V32,      static_cast<UINT32>(UphyIf::Version::V32));
PROP_CONST(UphyRegLogger, UV_V50,      static_cast<UINT32>(UphyIf::Version::V50));
PROP_CONST(UphyRegLogger, UV_V61,      static_cast<UINT32>(UphyIf::Version::V61));
PROP_CONST(UphyRegLogger, UV_UNKNOWN,  static_cast<UINT32>(UphyIf::Version::UNKNOWN));

JS_STEST_LWSTOM(UphyRegLogger, SetRegListToLog, 2, "Set a custom register list to log")
{
    STEST_HEADER(2, 2, "Usage: UphyRegLogger.SetRegListToLog(regBlock, regAddrArray)");
    STEST_ARG(0, UINT08, regBlock);
    STEST_ARG(1, JsArray, jsRegAddrArray);

    RC rc;
    vector<UINT16> regAddrArray;
    for (auto const & lwrAddr : jsRegAddrArray)
    {
        UINT16 tmpAddr;
        C_CHECK_RC(pJavaScript->FromJsval(lwrAddr, &tmpAddr));
        regAddrArray.push_back(tmpAddr);
    }

    RETURN_RC(UphyRegLogger::SetRegListToLog(regBlock, regAddrArray));
}

JS_STEST_LWSTOM(UphyRegLogger, SetC2CRegListToLog, 1, "Set a custom C2C register list to log")
{
    STEST_HEADER(1, 1, "Usage: UphyRegLogger.SetC2CRegListToLog(regAddrArray)");
    STEST_ARG(0, JsArray, jsRegAddrArray);

    RC rc;
    vector<UINT16> regAddrArray;
    for (auto const & lwrAddr : jsRegAddrArray)
    {
        UINT16 tmpAddr;
        C_CHECK_RC(pJavaScript->FromJsval(lwrAddr, &tmpAddr));
        regAddrArray.push_back(tmpAddr);
    }

    RETURN_RC(UphyRegLogger::SetC2CRegListToLog(regAddrArray));
}

#ifdef DEBUG
// Wrapper function designed to be called from a gdb prompt.  Return an INT32 instead of a RC
// to make it possible to call.  Returning a non-POD type (e.g. RC) makes calling from the gdb
// prompt either prohibitively diffilwlt or impossible
INT32 UphyLogRegs
(
    UINT32 devIndex,
    UINT08 uphyInterface,
    bool   bLogDevicePerfData,
    UINT08 regBlock,
    UINT08 logType
)
{
    RC rc;
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pTestDevice;
    rc = pTestDeviceMgr->GetDevice(devIndex, &pTestDevice);

    if (rc != RC::OK)
        return static_cast<INT32>(rc);

    return static_cast<INT32>(UphyRegLogger::ForceLogRegs(pTestDevice,
                                                          uphyInterface,
                                                          UphyRegLogger::LogPoint::Manual,
                                                          RC::OK,
                                                          bLogDevicePerfData,
                                                          regBlock,
                                                          logType));
}
#endif

