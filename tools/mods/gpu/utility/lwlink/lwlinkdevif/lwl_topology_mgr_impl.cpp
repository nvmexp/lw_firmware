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

#include "core/include/cmdline.h"
#include "lwl_topology_mgr_impl.h"

#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlcci.h"
#include "lwl_devif_mgr.h"
#include "gpu/i2c/lwswitchi2c.h"
#include "lwlink.h"
#include "lwlink_export.h"
#include "lwlink_lib_ctrl.h"

//------------------------------------------------------------------------------
// TopologyManagerImpl Public Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Constructor
//!
LwLinkDevIf::TopologyManagerImpl::TopologyManagerImpl
(
    string name
   ,TopologyMatch tm
   ,string topoFile
)
 : m_Name(move(name))
  ,m_TopologyMatch(tm)
  ,m_TopologyFile(topoFile)
  ,m_HwDetectMethod(HWD_NONE)
  ,m_TopologyFlags(TF_NONE)
{
}

//------------------------------------------------------------------------------
//! \brief Get all routes
//!
//! \param pRoutes : Vector of route pointers to return
//!
//! \return OK if successful, not OK otherwise
RC LwLinkDevIf::TopologyManagerImpl::GetRoutes
(
    vector<LwLinkRoutePtr> *pRoutes
)
{
    *pRoutes = m_Routes;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get all routes for the specified device
//!
//! \param pDevice : Pointer to device to return the routes on
//! \param pRoutes : Vector of route pointers to return
//!
//! \return OK if successful, not OK otherwise
RC LwLinkDevIf::TopologyManagerImpl::GetRoutesOnDevice
(
    TestDevicePtr           pDevice
   ,vector<LwLinkRoutePtr> *pRoutes
)
{
    for (size_t idx = 0; idx < m_Routes.size(); idx++)
    {
        if (m_Routes[idx]->UsesDevice(pDevice))
        {
            pRoutes->push_back(m_Routes[idx]);
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Print the topology at the specified priority
//!
//! \param devices : List of detected devices
//! \param pri     : Priority to print the topology at
//!
//! \return OK if successful, not OK otherwise
//!
void LwLinkDevIf::TopologyManagerImpl::Print
(
    const vector<TestDevicePtr> &devices
   ,Tee::Priority                pri
)
{
    JsonItem jsi;
    jsi.SetCategory(JsonItem::JSONLOG_LWLINKTOPOLOGY);
    jsi.SetJsonLimitedAllowAllFields(true);
    JsonItem jsc;
    jsc.SetCategory(JsonItem::JSONLOG_CARRIERFRU);
    jsc.SetJsonLimitedAllowAllFields(true);
    if (JsonLogStream::IsOpen())
    {
        jsi.SetTag("lwlink_topology");
        jsc.SetTag("carrier_fru_info");
    }

    for (size_t devIdx = 0; devIdx < devices.size(); devIdx++)
    {
        JsArray lwLinks;
        TestDevicePtr pDev = devices[devIdx];
        SCOPED_DEV_INST(pDev.get());
        auto pLwLink = pDev->GetInterface<LwLink>();

        string topoStr = "\n";
        topoStr += "LwLinks for " + pDev->GetName() +  " : \n";

        vector<LwLink::EndpointInfo> epInfo;
        if (OK != pLwLink->GetDetectedEndpointInfo(&epInfo))
        {
            Printf(Tee::PriWarn,
                   "%s : Unable to get detected endpoint info on %s!\n",
                   __FUNCTION__, pDev->GetName().c_str());
        }

        vector<LwLink::LinkStatus> linkStatus;
        if (OK != pLwLink->GetLinkStatus(&linkStatus))
        {
            Printf(Tee::PriWarn,
                   "%s : Unable to get link status on %s!\n",
                   __FUNCTION__, pDev->GetName().c_str());
        }

        vector<LwLink::PerLaneFomData> fomStatus;
        if (OK != pLwLink->GetFomStatus(&fomStatus))
        {
            Printf(Tee::PriWarn,
                   "%s : Unable to get FOM status on %s!\n",
                   __FUNCTION__, pDev->GetName().c_str());
        }

        for (UINT32 lwrLink = 0; lwrLink < pLwLink->GetMaxLinks(); lwrLink++)
        {
            JsonItem lwLink;
            TestDevicePtr pRemDev;
            LwLink::EndpointInfo lwrEpInfo;
            if (epInfo.empty())
            {
                topoStr += Utility::StrPrintf("  %2u : Unknown Remote Connection\n", lwrLink);
            }
            else
            {
                if (lwrLink < epInfo.size())
                    lwrEpInfo = epInfo[lwrLink];

                topoStr += Utility::StrPrintf("  %2u : ", lwrLink);
                if (JsonLogStream::IsOpen())
                {
                    lwLink.SetField("link_id", lwrLink);
                }

                UINT32 lineRateMbps = 0;
                UINT32 refClkSpeedMHz = 0;
                UINT32 linkDataRateKiBps = 0;
                if (lwrEpInfo.devType == TestDevice::TYPE_UNKNOWN)
                {
                    topoStr += "Not Connected\n";
                    if (JsonLogStream::IsOpen())
                    {
                        lwLink.SetField("not_connected", true);
                    }
                }
                else
                {
                    if (OK == GetDeviceById(devices, lwrEpInfo.id, &pRemDev))
                    {
                        topoStr += Utility::StrPrintf("%s, ", pRemDev->GetName().c_str());
                        if (JsonLogStream::IsOpen())
                        {
                            lwLink.SetField("remote_device", pRemDev->GetName().c_str());
                        }
                    }
                    else
                    {
                        topoStr += "Unknown Remote Device";
                        if (JsonLogStream::IsOpen())
                        {
                            lwLink.SetField("remote_device", "Unknown Remote Device");
                        }
                    }
                    topoStr += Utility::StrPrintf("Link %u\n", lwrEpInfo.linkNumber);

                    lineRateMbps = pLwLink->GetLineRateMbps(lwrLink);
                    if (pRemDev)
                    {
                        lineRateMbps =
                            min(lineRateMbps,
                                pRemDev->GetInterface<LwLink>()->GetLineRateMbps(lwrEpInfo.linkNumber)); //$
                    }
                    refClkSpeedMHz = pLwLink->GetRefClkSpeedMhz(lwrLink);
                    linkDataRateKiBps = pLwLink->GetLinkDataRateKiBps(lwrLink);
                }
                if (lineRateMbps != LwLink::UNKNOWN_RATE)
                {
                    topoStr += Utility::StrPrintf("       Line Rate            : %u Mbps\n", lineRateMbps); //$
                    if (JsonLogStream::IsOpen())
                        lwLink.SetField("line_rate_mbps", lineRateMbps);
                }
                if (refClkSpeedMHz != LwLink::UNKNOWN_RATE)
                {
                    topoStr += Utility::StrPrintf("       Ref Clock            : %u MHz\n", refClkSpeedMHz); //$
                    if (JsonLogStream::IsOpen())
                        lwLink.SetField("ref_clk_speed_mhz", refClkSpeedMHz);
                }
                if (linkDataRateKiBps != LwLink::UNKNOWN_RATE)
                {
                    topoStr += Utility::StrPrintf("       Link Data Rate       : %u KiBps\n", linkDataRateKiBps); //$
                    if (JsonLogStream::IsOpen())
                        lwLink.SetField("link_data_rate_kibps", linkDataRateKiBps);
                }
            }

            if (lwrLink < linkStatus.size())
            {
                topoStr += Utility::StrPrintf("       Link Status          : %s\n",
                    LwLink::LinkStateToString(linkStatus[lwrLink].linkState).c_str());
                topoStr += Utility::StrPrintf("       RX Sublink Status    : %s\n",
                    LwLink::SubLinkStateToString(linkStatus[lwrLink].rxSubLinkState).c_str());
                topoStr += Utility::StrPrintf("       TX Sublink Status    : %s\n",
                    LwLink::SubLinkStateToString(linkStatus[lwrLink].txSubLinkState).c_str());

                LwLink::SystemLineCode lineCode;
                LwLink::SystemBlockCodeMode blockCodeMode;
                string lineCodeStr = "Unknown";
                string blockCodeModeStr = "Unknown";
                const bool isLinkUp =
                    ((linkStatus[lwrLink].rxSubLinkState == LwLink::SLS_HIGH_SPEED)  ||
                     (linkStatus[lwrLink].rxSubLinkState == LwLink::SLS_SINGLE_LANE) ||
                     (linkStatus[lwrLink].rxSubLinkState == LwLink::SLS_SAFE_MODE))  &&
                    ((linkStatus[lwrLink].txSubLinkState == LwLink::SLS_HIGH_SPEED)  ||
                     (linkStatus[lwrLink].txSubLinkState == LwLink::SLS_SINGLE_LANE) ||
                     (linkStatus[lwrLink].txSubLinkState == LwLink::SLS_SAFE_MODE));

                if (isLinkUp && (RC::OK == pLwLink->GetLineCode(lwrLink, &lineCode)))
                {
                    lineCodeStr = LwLink::LineCodeToString(lineCode);
                }
                topoStr += Utility::StrPrintf("       Line code mode       : %s\n",
                    lineCodeStr.c_str());

                if (isLinkUp && (RC::OK == pLwLink->GetBlockCodeMode(lwrLink, &blockCodeMode)))
                {
                    blockCodeModeStr = LwLink::BlockCodeModeToString(blockCodeMode);
                }
                topoStr += Utility::StrPrintf("       Block code mode      : %s\n",
                    blockCodeModeStr.c_str());


                if (JsonLogStream::IsOpen())
                {
                    lwLink.SetField("link_status",
                                    LwLink::LinkStateToString(linkStatus[lwrLink].linkState).c_str());
                    lwLink.SetField("rx_sublink_status",
                                    LwLink::SubLinkStateToString(linkStatus[lwrLink].rxSubLinkState).c_str());
                    lwLink.SetField("tx_sublink_status",
                                    LwLink::SubLinkStateToString(linkStatus[lwrLink].txSubLinkState).c_str());
                    lwLink.SetField("line_code_mode", lineCodeStr.c_str());
                    lwLink.SetField("block_code_mode", blockCodeModeStr.c_str());
                }

                UINT32 rxdetLaneMask = pLwLink->GetRxdetLaneMask(lwrLink);
                if (rxdetLaneMask != LwLink::ILWALID_LANE_MASK)
                {
                    topoStr += Utility::StrPrintf("       RxDet Lane Mask      : 0x%x\n", rxdetLaneMask);
                    if (JsonLogStream::IsOpen())
                    {
                        lwLink.SetField("rxdet_lane_mask", rxdetLaneMask);
                    }
                }
                if ((lwrLink < fomStatus.size()) && !fomStatus[lwrLink].empty())
                {
                    JsArray jsFomValues;

                    topoStr += Utility::StrPrintf("       FOM Values           :");
                    string comma = "";
                    for (auto const & fomValue : fomStatus[lwrLink])
                    {
                        topoStr += Utility::StrPrintf("%s %u", comma.c_str(), fomValue);
                        comma = ",";
                        if (JsonLogStream::IsOpen())
                        {
                            jsval fomJsVal;
                            if (JavaScriptPtr()->ToJsval(fomValue, &fomJsVal) == RC::OK)
                            {
                                jsFomValues.push_back(fomJsVal);
                            }
                            else
                            {
                                Printf(Tee::PriWarn,
                                       "Failed to colwert FOM value to JS value, using 0\n");
                                jsFomValues.push_back(JSVAL_ZERO);
                            }
                        }
                    }
                    topoStr += "\n";
                    if (JsonLogStream::IsOpen())
                    {
                        lwLink.SetField("fom_values", &jsFomValues);
                    }
                }

                bool bCCPresent = false;
                if (pLwLink->SupportsInterface<LwLinkCableController>())
                {
                    auto pLwlCC = pLwLink->GetInterface<LwLinkCableController>();
                    const UINT32 cciId = pLwlCC->GetCciIdFromLink(lwrLink);

                    if (cciId != LwLinkCableController::ILWALID_CCI_ID)
                    {
                        bCCPresent = true;

                        const LwLinkCableController::CCInitStatus initStatus =
                            pLwlCC->GetInitStatus(cciId);
                        const char * pCcStatusString =
                            LwLinkCableController::InitStatusToString(initStatus);

                        topoStr += Utility::StrPrintf("       Cable Controller     : %s\n",
                                                      pCcStatusString);

                        if (JsonLogStream::IsOpen())
                        {
                            lwLink.SetField("cable_controller", pCcStatusString);
                        }

                        if (initStatus == LwLinkCableController::CCInitStatus::Success)
                        {
                            topoStr += Utility::StrPrintf("       CC Part Number       : %s\n"
                                                          "       CC HW Revision       : %s\n"
                                                          "       CC Serial Number     : %s\n",
                                                          pLwlCC->GetPartNumber(cciId).c_str(),
                                                          pLwlCC->GetHwRevision(cciId).c_str(),
                                                          pLwlCC->GetSerialNumber(cciId).c_str());

                            if (JsonLogStream::IsOpen())
                            {
                                lwLink.SetField("cc_part_number", pLwlCC->GetPartNumber(cciId).c_str());
                                lwLink.SetField("cc_hw_revision", pLwlCC->GetHwRevision(cciId).c_str());
                                lwLink.SetField("cc_serial_number", pLwlCC->GetSerialNumber(cciId).c_str());
                            }
                            auto LogCCFirmwareInfo = [&] (LwLinkCableController::CCFirmwareImage img)
                            {
                                const LwLinkCableController::CCFirmwareInfo & fwInfo =
                                    pLwlCC->GetFirmwareInfo(cciId, img);

                                if (fwInfo.status == LwLinkCableController::CCFirmwareStatus::NotPresent)
                                    return;

                                const char *pFwImgString =
                                    LwLinkCableController::FirmwareImageToString(img);

                                constexpr size_t fwStringSize = 28;
                                string fwstring = Utility::StrPrintf("       CC %s Firmware", pFwImgString);
                                MASSERT(fwstring.size() <= fwStringSize);
                                fwstring += string(fwStringSize - fwstring.size(), ' ');

                                const string jsonTag =
                                    Utility::ToLowerCase(Utility::StrPrintf("cc_%s_firmware", pFwImgString));

                                if (fwInfo.status != LwLinkCableController::CCFirmwareStatus::Unknown)
                                {
                                    string fwVerString = "Empty";
                                    if (fwInfo.status != LwLinkCableController::CCFirmwareStatus::Empty)
                                    {
                                        fwVerString =
                                            Utility::StrPrintf("%u.%u Build %u (%s%s)",
                                                               fwInfo.majorVersion,
                                                               fwInfo.minorVersion,
                                                               fwInfo.build,
                                                               LwLinkCableController::FirmwareStatusToString(fwInfo.status),
                                                               fwInfo.bIsBoot ? "/Boot" : "");
                                    }

                                    topoStr += fwstring;
                                    topoStr += Utility::StrPrintf(": %s\n", fwVerString.c_str());
                                    if (JsonLogStream::IsOpen())
                                    {
                                        lwLink.SetField(jsonTag.c_str(), fwVerString.c_str());
                                    }
                                }
                                else
                                {
                                    topoStr += fwstring;
                                    topoStr += ": Unknown";
                                    if (JsonLogStream::IsOpen())
                                    {
                                        lwLink.SetField(jsonTag.c_str(), "Unknown");
                                    }
                                }
                            };
                            LogCCFirmwareInfo(LwLinkCableController::CCFirmwareImage::Factory);
                            LogCCFirmwareInfo(LwLinkCableController::CCFirmwareImage::ImageA);
                            LogCCFirmwareInfo(LwLinkCableController::CCFirmwareImage::ImageB);

                            topoStr += Utility::StrPrintf("       CC Module Link Mask  : 0x%llx\n"
                                                          "       CC Host Lane Count   : %u\n"
                                                          "       CC Module Lane Count : %u\n",
                                                          pLwlCC->GetLinkMask(cciId),
                                                          pLwlCC->GetHostLaneCount(cciId),
                                                          pLwlCC->GetModuleLaneCount(cciId));
                            if (JsonLogStream::IsOpen())
                            {
                                lwLink.SetField("cc_module_link_mask", pLwlCC->GetLinkMask(cciId));
                                lwLink.SetField("cc_host_lane_count", pLwlCC->GetHostLaneCount(cciId));
                                lwLink.SetField("cc_module_lane_count", pLwlCC->GetModuleLaneCount(cciId));
                            }

                            const LwLinkCableController::CCModuleIdentifier mi =
                                pLwlCC->GetModuleIdentifier(cciId);
                            const char * pCcIdString =
                                LwLinkCableController::ModuleIdentifierToString(mi);
                            const LwLinkCableController::CCModuleMediaType mt =
                                pLwlCC->GetModuleMediaType(cciId);
                            const char * pCcMediaTypeString =
                                LwLinkCableController::ModuleMediaTypeToString(mt);

                            topoStr += Utility::StrPrintf("       CC Module Type       : %s\n"
                                                          "       CC Module ID         : %s\n",
                                                          pCcMediaTypeString,
                                                          pCcIdString);
                            if (JsonLogStream::IsOpen())
                            {
                                lwLink.SetField("cc_module_type", pCcMediaTypeString);
                                lwLink.SetField("cc_module_id", pCcIdString);
                            }

                            LwLinkCableController::ModuleState stateValue;
                            if (pLwlCC->GetCCModuleState(cciId, &stateValue) == RC::OK)
                            {
                                topoStr += Utility::StrPrintf("       CC Module State      : %s\n",
                                            LwLinkCableController::ModuleStateToString(stateValue));
                            }

                            vector<LwLinkCableController::GradingValues> gradingVals;
                            if (pLwlCC->GetGradingValues(cciId, &gradingVals) == RC::OK)
                            {
                                auto LogCCGrading = [&] (const string & valueStr,
                                                         const char * jsonStr,
                                                         INT32 firstLane,
                                                         INT32 lastLane,
                                                         const vector<UINT08> & vals)
                                {
                                    JsArray jsGradingValues;
                                    constexpr size_t columnWidth = 37;

                                    string headerStr = valueStr +
                                                       Utility::StrPrintf(" Lane(%d-%d)",
                                                                          firstLane, lastLane);
                                    if (headerStr.size() < columnWidth)
                                        headerStr += string(columnWidth - headerStr.size(), ' ');
                                    headerStr += ":";
                                    topoStr += headerStr;
                                    string comma = "";
                                    for (INT32 lane = firstLane; lane <= lastLane; lane++)
                                    {
                                        topoStr += Utility::StrPrintf("%s %u",
                                                                      comma.c_str(),
                                                                      vals[lane]);
                                        comma = ",";
                                        if (JsonLogStream::IsOpen())
                                        {
                                            jsval gradingJsVal;
                                            if (JavaScriptPtr()->ToJsval(vals[lane], &gradingJsVal) == RC::OK)
                                            {
                                                jsGradingValues.push_back(gradingJsVal);
                                            }
                                            else
                                            {
                                                Printf(Tee::PriWarn,
                                                       "Failed to colwert grading value to JS value, using 0\n");
                                                jsGradingValues.push_back(JSVAL_ZERO);
                                            }
                                            jsGradingValues.push_back(gradingJsVal);
                                        }
                                    }
                                    topoStr += Utility::StrPrintf("\n");
                                    if (JsonLogStream::IsOpen())
                                    {
                                        lwLink.SetField(jsonStr, &jsGradingValues);
                                    }
                                };

                                for (auto const & lwrGrading : gradingVals)
                                {
                                    if ((lwrGrading.linkId == lwrLink) && (lwrGrading.laneMask != 0))
                                    {
                                        const INT32 firstLane =
                                            Utility::BitScanForward(lwrGrading.laneMask);
                                        const INT32 lastLane =
                                            Utility::BitScanReverse(lwrGrading.laneMask);

                                        LogCCGrading("       CC RX Init Grading",
                                                     "cc_rx_init_grading",
                                                     firstLane,
                                                     lastLane,
                                                     lwrGrading.rxInit);
                                        LogCCGrading("       CC TX Init Grading",
                                                     "cc_tx_init_grading",
                                                     firstLane,
                                                     lastLane,
                                                     lwrGrading.txInit);
                                        LogCCGrading("       CC RX Maint Grading",
                                                     "cc_rx_maint_grading",
                                                     firstLane,
                                                     lastLane,
                                                     lwrGrading.rxMaint);
                                        LogCCGrading("       CC TX Maint Grading",
                                                     "cc_tx_maint_grading",
                                                     firstLane,
                                                     lastLane,
                                                     lwrGrading.txMaint);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                if (!bCCPresent)
                {
                    topoStr += Utility::StrPrintf("       Cable Controller     : Not Present\n");
                    if (JsonLogStream::IsOpen())
                    {
                        lwLink.SetField("cable_controller", "Not Present");
                    }
                }
            }
            else
            {
                topoStr += Utility::StrPrintf("       Link Status       : Unknown\n");
                topoStr += Utility::StrPrintf("       RX Sublink Status : Unknown\n");
                topoStr += Utility::StrPrintf("       TX Sublink Status : Unknown\n");
                topoStr += Utility::StrPrintf("       RxDet Lane Mask   : Unknown\n");
                topoStr += Utility::StrPrintf("       FOM Values        : Unknown\n");
                topoStr += Utility::StrPrintf("       Cable Controller  : Unknown\n");
                if (JsonLogStream::IsOpen())
                {
                    lwLink.SetField("link_status", "Unknown");
                    lwLink.SetField("rx_sublink_status", "Unknown");
                    lwLink.SetField("tx_sublink_status", "Unknown");
                    lwLink.SetField("line_code_mode", "Unknown");
                    lwLink.SetField("block_code_mode", "Unknown");
                    lwLink.SetField("rxdet_lane_mask", "Unknown");
                    lwLink.SetField("fom_values", "Unknown");
                    lwLink.SetField("cable_controller", "Unknown");
                    lwLink.SetField("cc_module_id", "Unknown");
                    lwLink.SetField("cc_module_link_mask", "Unknown");
                }
            }

            lwLinks.push_back(OBJECT_TO_JSVAL(lwLink.GetObj()));
        }
        if (JsonLogStream::IsOpen() && pLwLink->SupportsInterface<LwLinkCableController>())
        {
            JsArray carrierFrus;
            auto carrierFruInfos = pLwLink->GetInterface<LwLinkCableController>()->GetCarrierFruInfo();
            for (UINT08 fruSize = 0; fruSize < carrierFruInfos.size(); fruSize++)
            {
                JsonItem carrierFru;
                carrierFru.SetField("device", pDev->GetName().c_str());
                carrierFru.SetField("carrier_port_number", carrierFruInfos[fruSize].carrierPortNum.c_str());
                carrierFru.SetField("carrier_mfg_date",  carrierFruInfos[fruSize].carrierBoardMfgDate.c_str());
                carrierFru.SetField("carrier_mfg_Name",  carrierFruInfos[fruSize].carrierBoardMfgName.c_str());
                carrierFru.SetField("carrier_prod_name", carrierFruInfos[fruSize].carrierBoardProductName.c_str());
                carrierFru.SetField("carrier_serial_num", carrierFruInfos[fruSize].carrierBoardSerialNum.c_str());
                carrierFru.SetField("carrier_board_num", carrierFruInfos[fruSize].carrierBoardPartNum.c_str());
                carrierFru.SetField("carrier_hw_rev", carrierFruInfos[fruSize].carrierBoardHwRev.c_str());
                carrierFru.SetField("carrier_fru_ver", carrierFruInfos[fruSize].carrierBoardFruRev.c_str());
                carrierFrus.push_back(OBJECT_TO_JSVAL(carrierFru.GetObj()));
            }
            jsc.SetField(pDev->GetName().c_str(), &carrierFrus);
        }

        Printf(pri, GetTeeModule()->GetCode(), "%s", topoStr.c_str());
        if (JsonLogStream::IsOpen())
        {
            jsi.SetField(pDev->GetName().c_str(), &lwLinks);
        }
    }
    if (JsonLogStream::IsOpen())
    {
        jsi.WriteToLogfile();
        jsc.WriteToLogfile();
    }
    for (auto const & route : m_Routes)
    {
        route->Print(pri);
    }
}

//------------------------------------------------------------------------------
//! \brief Shutdown all contained LwLink Devices
//!
//! \return OK if successful, not OK otherwise
RC LwLinkDevIf::TopologyManagerImpl::Shutdown()
{
    StickyRC rc;

    for (size_t ii = 0; ii < m_Routes.size(); ii++)
    {
        if (!m_Routes[ii].unique())
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "LwLinkTopologyMgr : Multiple references to LwLink route %u"
                   " during shutdown!\n",
                   static_cast<UINT32>(ii));
            rc = RC::SOFTWARE_ERROR;
        }
    }
    m_Routes.clear();

    for (auto const & pCon : m_Connections)
    {
        if (!pCon.unique())
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "LwLinkTopologyMgr : Multiple references to LwLink connection "
                   "during shutdown!\n");
            rc = RC::SOFTWARE_ERROR;
        }
    }
    m_Connections.clear();

    return OK;
}

//------------------------------------------------------------------------------
// TopologyManagerImpl Protected Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Add a new connection to the connection set and return the connection
//! that was just added.  If the connection is already present then the new
//! connection is not added and the existing connection is returned
//!
//! \param pCon : Connection to add
//!
LwLinkConnectionPtr LwLinkDevIf::TopologyManagerImpl::AddConnection(LwLinkConnectionPtr pCon)
{
    if (m_Connections.count(pCon))
        return *m_Connections.find(pCon);
    m_Connections.insert(pCon);
    return pCon;
}

//------------------------------------------------------------------------------
//! \brief Add the specified path to an existing route (if there is alreay a
//! route connecting the two endpoint devices), otherwise create a new route
//! with the specified path
//!
//! \param path : Path to add to or create a new route from
//!
//! \return OK if successful, not OK otherwise
RC LwLinkDevIf::TopologyManagerImpl::AddPathToRoute(const LwLinkPath &path)
{
    RC rc;

    // Get the terminating endpoints from the path being added
    LwLinkConnection::EndpointDevices endpoints = path.GetEndpointDevices();

    // If a route between the endpoints doesnt already exist create one,
    // otherwise add the path to an existing route
    LwLinkRoutePtr pRoute;
    bool bNewRoute = GetRouteWithEndpoints(endpoints, &pRoute) != OK;
    if (bNewRoute)
        pRoute.reset(new LwLinkRoute(endpoints));

    CHECK_RC(pRoute->AddPath(path));

    if (bNewRoute)
        m_Routes.push_back(pRoute);

    return rc;
}

//------------------------------------------------------------------------------
void LwLinkDevIf::TopologyManagerImpl::ConsolidatePaths()
{
    for (auto pRoute : m_Routes)
    {
        pRoute->ConsolidatePathsByDataType();
    }
}

//------------------------------------------------------------------------------
//! \brief Create connections for all unconnected links on all devices
//!
//! \param devices : Devices to create unused connections on
//!
void LwLinkDevIf::TopologyManagerImpl::CreateUnusedConnections(const vector<TestDevicePtr> &devices)
{
    // Add "unconnected" connections for devices that are not the topology
    // master device type (otherswise it looks like they have fewer links than
    // they really do
    for (auto const & pLwrLwLinkDev : devices)
    {
        auto pLwrLwLink = pLwrLwLinkDev->GetInterface<LwLink>();
        UINT64 linkMask = 0LLU;
        for (auto const & pCon : m_Connections)
        {
            if (!pCon->UsesDevice(pLwrLwLinkDev) || !pCon->IsActive())
                continue;
            UINT64 lwrMask = pCon->GetLocalLinkMask(pLwrLwLinkDev);
            if (pCon->HasLoopout())
                lwrMask |= pCon->GetRemoteLinkMask(pLwrLwLinkDev);
            linkMask |= lwrMask;
        }

        vector<LwLinkConnection::EndpointIds> unconnectedLinks;
        for (UINT32 lwrLink = 0; lwrLink < pLwrLwLink->GetMaxLinks(); lwrLink++)
        {
            if (!pLwrLwLink->IsLinkActive(lwrLink) || !(linkMask & (1ULL << lwrLink)))
            {
                unconnectedLinks.push_back(LwLinkConnection::EndpointIds(lwrLink,
                                                                      LwLink::ILWALID_LINK_ID));
            }
        }
        if (!unconnectedLinks.empty())
        {
            LwLinkConnectionPtr newCon(new LwLinkConnection(pLwrLwLinkDev,
                                                            TestDevicePtr(),
                                                            unconnectedLinks));
            AddConnection(newCon);
        }
    }
}

//------------------------------------------------------------------------------
//! \brief Get a LwLink device that matches the specified ID
//!
//! \param deviceId : Device ID to get
//! \param ppDevice : Pointer to returned device pointer
//!
//! \return OK if successful, DEVICE_NOT_FOUND if unable to find a device
RC LwLinkDevIf::TopologyManagerImpl::GetDeviceById
(
    const vector<TestDevicePtr> &devices
   ,TestDevice::Id               deviceId
   ,TestDevicePtr               *ppDevice
) const
{
    MASSERT(ppDevice);
    ppDevice->reset();
    for (size_t devIdx = 0; devIdx < devices.size(); devIdx++)
    {
        if (devices[devIdx]->GetId() == deviceId)
        {
            *ppDevice = devices[devIdx];
            return OK;
        }
    }

    return RC::DEVICE_NOT_FOUND;
}

//------------------------------------------------------------------------------
// TopologyManagerImpl Private Implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Find a route connecting the specified endpoint devices
//!
//! \param endpoints : Endpoints to check a route between
//! \param ppRoute   : Pointer to returned route
//!
//! \return OK if successful, DEVICE_NOT_FOUND if unable to find a route
RC LwLinkDevIf::TopologyManagerImpl::GetRouteWithEndpoints
(
    LwLinkConnection::EndpointDevices endpoints
   ,LwLinkRoutePtr                   *ppRoute
)
{
    for (auto route : m_Routes)
    {
        if (route->UsesEndpoints(endpoints))
        {
            *ppRoute = route;
            return OK;
        }
    }
    return RC::DEVICE_NOT_FOUND;
}

namespace LwLinkDevIf
{
    namespace TopologyManagerImplFactory
    {
        //! Structure describing a registered factory function for creating
        //! a topology interface
        struct FactoryFuncData
        {
            string      name;     //! Name of the topology interface
            FactoryFunc factFunc; //! Pointer to function that creates the interface
        };

        //! Factory data.  Note that these data members are used by the Schwartz
        //! counter so it is important that their initializations here are to
        //! builtin types rather than classes.  If there initializations are to
        //! classes then the Schwartz counter will not work since class based
        //! initializations are performed as the last step of static
        //! construction and the data members will actually be re-initialized
        //! by static construction after they are filled in by the Schwartz
        //! counter class.
        UINT32 m_FactInitCounter = 0;
        map<UINT32, FactoryFuncData> * m_FactoryFunctions = 0;

        //----------------------------------------------------------------------
        //! \brief Initialize factory data, used by Schwartz counter
        //!
        void Initialize()
        {
            m_FactoryFunctions = new map<UINT32, FactoryFuncData>;
        }

        //----------------------------------------------------------------------
        //! \brief Shutdown factory data, used by Schwartz counter
        //!
        void Shutdown()
        {
            m_FactoryFunctions->clear();
            delete m_FactoryFunctions;
            m_FactoryFunctions = nullptr;
        }
    }
};

//------------------------------------------------------------------------------
//! \brief Factory initializer Schwartz counter, initialize factory data the
//! first time one of the classes is constructed
//!
LwLinkDevIf::TopologyManagerImplFactory::Initializer::Initializer()
{
    if (0 == m_FactInitCounter++)
    {
        Initialize();
    }
}

//------------------------------------------------------------------------------
//! \brief Free factory data the last time one of the classes is destructed
//!
LwLinkDevIf::TopologyManagerImplFactory::Initializer::~Initializer()
{
    if (0 == --m_FactInitCounter)
    {
        Shutdown();
    }
}

//------------------------------------------------------------------------------
//! \brief Constructor that registers a device interface creation function with
//! the factory
//!
//! \param order : Order in which the device interface should be created
//! \param name  : Name of the device interface
//! \param f     : Link ID on this device to query
//!
LwLinkDevIf::TopologyManagerImplFactory::FactoryFuncRegister::FactoryFuncRegister
(
    UINT32      order
   ,string      name
   ,FactoryFunc f
)
{
    if (m_FactoryFunctions->find(order) != m_FactoryFunctions->end())
    {
        map<UINT32, FactoryFuncData>::iterator pEntry = m_FactoryFunctions->find(order);
        Printf(Tee::PriError,
               "Failed to register %s, %s already exists with order %u!!\n",
               name.c_str(),
               pEntry->second.name.c_str(),
               order);
        return;
    }
    FactoryFuncData factData = { name, f };
    pair<UINT32, FactoryFuncData> mapData(order, factData);
    m_FactoryFunctions->insert(mapData);

    // Note : cannot use modules here since this is called during static
    // construction and the module may not have been created yet
    Printf(Tee::PriLow,
           "Registered LwLink topology manager implementation %s with order %u!!\n",
           name.c_str(),
           order);
}

//------------------------------------------------------------------------------
//! \brief Create all device interfaces
//!
//! \param pInterfaces : Pointer to returned vector of interface pointers
//!
void LwLinkDevIf::TopologyManagerImplFactory::CreateAll
(
    TopologyMatch                    tm
   ,const string &                   topoFile
   ,vector<TopologyManagerImplPtr> * pTopoMgrs
)
{

    for (auto const & factEntry : *m_FactoryFunctions)
    {
        TopologyManagerImplPtr pTopoMgr(factEntry.second.factFunc(factEntry.second.name,
                                                                  tm,
                                                                  topoFile));
        pTopoMgrs->push_back(pTopoMgr);

        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Created topology manager implementation %s\n",
               __FUNCTION__, factEntry.second.name.c_str());
    }
}

