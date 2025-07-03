/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "repair_module_commands.h"
#include "repair_module_mem.h"
#include "repair_module_hbm.h"

#include "core/include/device.h"      // Device::LwDeviceId
#include "core/include/jscript.h"
#include "core/include/memlane.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "gpu/js_gpu.h" // Gpu_Object, ...
#include "gpu/repair/repair_util.h"
#include "gpu/repair/hbm/hbm_interface/hbm_interface.h"
#include "gpu/repair/tpc_repair/tpc_repair.h"
#include "gpu/utility/eclwtil.h"
#include "gpu/utility/hbmdev.h"
#include "gpu/include/hbmimpl.h"

#include <algorithm>
#include <sstream>

using namespace Memory;
using namespace Repair;

/******************************************************************************/
// Command

//!
//! \brief Colwerts the given repair command to a string.
//!
string Command::ToString(Command::Type commandType)
{
#define REPAIR_COMMAND_TO_STRING(Cmd)           \
    case Repair::Command::Type::Cmd:            \
        return #Cmd;

    switch (commandType)
    {
        REPAIR_COMMAND_TO_STRING(ReconfigGpuLanes);
        REPAIR_COMMAND_TO_STRING(MemRepairSeq);
        REPAIR_COMMAND_TO_STRING(LaneRepair);
        REPAIR_COMMAND_TO_STRING(RowRepair);
        REPAIR_COMMAND_TO_STRING(LaneRemapInfo);
        REPAIR_COMMAND_TO_STRING(InfoRomRepairPopulate);
        REPAIR_COMMAND_TO_STRING(InfoRomRepairValidate);
        REPAIR_COMMAND_TO_STRING(InfoRomRepairClear);
        REPAIR_COMMAND_TO_STRING(InfoRomRepairPrint);
        REPAIR_COMMAND_TO_STRING(PrintHbmDevInfo);
        REPAIR_COMMAND_TO_STRING(PrintRepairedLanes);
        REPAIR_COMMAND_TO_STRING(PrintRepairedRows);
        REPAIR_COMMAND_TO_STRING(InfoRomTpcRepairPopulate);
        REPAIR_COMMAND_TO_STRING(InfoRomTpcRepairClear);
        REPAIR_COMMAND_TO_STRING(InfoRomTpcRepairPrint);
        REPAIR_COMMAND_TO_STRING(RunMBIST);

        default:
            MASSERT(!"Unknown repair command type");
            // [[fallthrough]] @c++17

        REPAIR_COMMAND_TO_STRING(Unknown);
    }

#undef REPAIR_COMMAND_TO_STRING
}

bool Command::IsSet(Flags flags) const
{
    return static_cast<UINT32>(m_Flags & flags) != 0;
}

RC Command::Validate(const Settings& settings, ValidationResult* pResult) const
{
    MASSERT(pResult);
    return RC::OK;
}

RC Command::Execute
(
    const Settings& settings,
    const TpcRepair& tpcRepair
) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Command::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Command::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Gddr::GddrInterface>* ppGddrInterface
) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

string Command::ToString() const
{
    return Command::ToString(m_Type) + "()";
}

/******************************************************************************/
// RemapGpuLanesCommand

RemapGpuLanesCommand::RemapGpuLanesCommand(string mappingFile)
    : Command(Command::Type::ReconfigGpuLanes,
              Command::Group::Mem,
              Flags::PrimaryCommand | Flags::ExplicitRepair
              | Flags::InstInSysRequired),
      m_MappingFile(mappingFile)
{}

RC RemapGpuLanesCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    // TODO(stewarts): implement
    return RC::UNSUPPORTED_FUNCTION;
}

string RemapGpuLanesCommand::ToString() const
{
    return Utility::StrPrintf("%s(mappingFile='%s')",
                              Command::ToString(m_Type).c_str(),
                              m_MappingFile.c_str());
}

/******************************************************************************/
// MemRepairSeqCommand

MemRepairSeqCommand::MemRepairSeqCommand()
    : Command(Command::Type::MemRepairSeq,
              Command::Group::Mem,
              Flags::PrimaryCommand | Flags::InstInSysRequired)
{}

RC MemRepairSeqCommand::Validate
(
    const Settings& settings,
    ValidationResult* pResult
) const
{
    MASSERT(pResult);

    bool bDoLaneRepair    = Repair::IsSet(settings.modifiers.memRepairSeqMode,
                                          MemRepairSeqMode::Lane);
    bool bDoRowRepair     = Repair::IsSet(settings.modifiers.memRepairSeqMode,
                                          MemRepairSeqMode::Row);
    bool bDoBlacklist     = Repair::IsSet(settings.modifiers.memRepairSeqMode,
                                          MemRepairSeqMode::Blacklist);
    bool bDoHardRepair    = Repair::IsSet(settings.modifiers.memRepairSeqMode,
                                          MemRepairSeqMode::Hard);
    bool bForceHardRepair = Repair::IsSet(settings.modifiers.memRepairSeqMode,
                                          MemRepairSeqMode::ForceHard);

    if (bDoRowRepair)
    {
        Printf(Tee::PriError, "Invalid repair mode : Row Repair is lwrrently not supported "
                              "via the mem repair sequence\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (bDoBlacklist)
    {
        // TODO: Replace with row remapping
        Printf(Tee::PriError, "Invalid repair mode : Blacklisting is not supported "
                               "via mem repair sequence\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (!bDoHardRepair && bForceHardRepair)
    {
        Printf(Tee::PriError, "Invalid repair mode : Force hard repair when hard repair "
                              "not selected\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (bDoHardRepair && !(bDoLaneRepair || bDoRowRepair || bDoBlacklist))
    {
        Printf(Tee::PriError, "Invalid repair mode : Force hard repair when repair type "
                              "not selected\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    pResult->isAttemptingHardRepair = bDoHardRepair;
    return RC::OK;
}

RC MemRepairSeqCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    RC rc;
    MASSERT(pLwrrSysState);
    MASSERT(ppHbmInterface);

    GpuSubdevice *pSubdev = pLwrrSysState->lwrrentGpu.pSubdev;
    MASSERT(pSubdev);
    HBMImpl* pHbmImpl     = pSubdev->GetHBMImpl();
    MASSERT(pHbmImpl);
    HBMDevicePtr pHbmDev  = pHbmImpl->GetHBMDev();
    MASSERT(pHbmDev);

    bool bEccSupported = false;
    UINT32 eccEnableMask = 0;
    CHECK_RC(pSubdev->GetEccEnabled(&bEccSupported, &eccEnableMask));
    if (bEccSupported && Ecc::IsUnitEnabled(Ecc::Unit::DRAM, eccEnableMask))
    {
        Printf(Tee::PriError, "FB ECC must be disabled in order to perform Memory Repair!\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    bool bDoLaneRepair    = Repair::IsSet(settings.modifiers.memRepairSeqMode,
                                          MemRepairSeqMode::Lane);
    bool bDoHardRepair    = Repair::IsSet(settings.modifiers.memRepairSeqMode,
                                          MemRepairSeqMode::Hard);
    bool bForceHardRepair = Repair::IsSet(settings.modifiers.memRepairSeqMode,
                                          MemRepairSeqMode::ForceHard);

    JavaScriptPtr pJs;
    bool bInsufficientRepair = false;
    vector<LaneError> lanesToRepair;
    if (bDoLaneRepair)
    {
        Printf(Tee::PriNormal, "\n---- Running memory tests ----\n");
        JSObject* pGlobalJsObj = pJs->GetGlobalObject();
        JsArray jsCollectLaneErrorsArgs;
        jsval   jsDevInst;
        jsval   jsRetVal = JSVAL_NULL;
        CHECK_RC(pJs->ToJsval(pSubdev->DevInst(), &jsDevInst));
        jsCollectLaneErrorsArgs.push_back(jsDevInst);
        CHECK_RC(pJs->CallMethod(pGlobalJsObj,
                                 "RunTestsAndCollectLaneErrors",
                                 jsCollectLaneErrorsArgs,
                                 &jsRetVal));

        UINT32 testRc;
        JSObject* pJsRetObj;
        CHECK_RC(pJs->FromJsval(jsRetVal, &pJsRetObj));
        CHECK_RC(pJs->GetProperty(pJsRetObj, "Rc", &testRc));
        if (testRc == static_cast<UINT32>(RC::OK))
        {
            Printf(Tee::PriNormal, "No memory errors detected. No repairs required\n");
            return RC::OK;
        }
        else if (testRc != static_cast<UINT32>(RC::BAD_MEMORY))
        {
            // Can't handle other errors through mem repair
            return testRc;
        }

        JsArray jsLaneErrors;
        CHECK_RC(pJs->GetProperty(pJsRetObj, "LaneErrors", &jsLaneErrors));
        lanesToRepair.resize(jsLaneErrors.size());
        for (UINT32 i = 0; i < jsLaneErrors.size(); i++)
        {
            JSObject* pJsAddrObj;
            string laneTypeStr;
            CHECK_RC(pJs->FromJsval(jsLaneErrors[i], &pJsAddrObj));

            const UINT32 subp = pHbmDev->GetSubpartition(lanesToRepair[i]);
            string laneName = lanesToRepair[i].ToString();
            CHECK_RC(pJs->UnpackFields(
                pJsAddrObj,     "sIIIs",
                "LaneName",     &laneName,
                "HwFbio",       &lanesToRepair[i].hwFbpa, // Should change to HwFbpa, but already in use
                "Subpartition", &subp,
                "LaneBit",      &lanesToRepair[i].laneBit,
                "RepairType",   &laneTypeStr));

            lanesToRepair[i].laneType = LaneError::GetLaneTypeFromString(laneTypeStr);
        }

        if (!lanesToRepair.empty())
        {
            // Look through the list of lanes to see if any are actually DBI errors,
            // and adjust the list accordingly
            MemError::DetectDBIErrors(&lanesToRepair);

            Printf(Tee::PriNormal, "\n---- Starting Soft Lane-Repair sequence ----\n");
            CHECK_RC((*ppHbmInterface)->LaneRepair(settings,
                                                   lanesToRepair,
                                                   Memory::RepairType::SOFT));
            Printf(Tee::PriNormal, "\n---- Soft Lane-Repair sequence complete ----\n");

            Printf(Tee::PriNormal, "\n---- Re-running memory tests to validate soft repairs ----\n");
            JsArray jsValidateLaneRepairsArgs;
            jsval   jsLaneErrorsArg;
            CHECK_RC(pJs->ToJsval(&jsLaneErrors, &jsLaneErrorsArg));
            jsValidateLaneRepairsArgs.push_back(jsDevInst);
            jsValidateLaneRepairsArgs.push_back(jsLaneErrorsArg);
            CHECK_RC(pJs->CallMethod(pGlobalJsObj,
                                     "RunTestsAndValidateLaneRepairs",
                                     jsValidateLaneRepairsArgs,
                                     &jsRetVal));

            UINT32 retestRc;
            CHECK_RC(pJs->FromJsval(jsRetVal, &retestRc));
            if (retestRc == RC::OK)
            {
                // CreateGpuTestList checks Log.FirstError for a failure, and throws if
                // there is one. Clear errors from the previous test run.
                Log::ResetFirstError();
            }
            else if (retestRc == static_cast<UINT32>(RC::REPAIR_INSUFFICIENT) && bForceHardRepair)
            {
                // Continue the sequence
                bInsufficientRepair = true;
                Printf(Tee::PriWarn, "Forcing hard repair\n");
            }
            else
            {
                CHECK_RC(retestRc);
            }
        }
        else
        {
            // No lane errors found. Nothing to do with the current supported sequence
            // TODO : Change if support is added for row_repair / row remapping
            Printf(Tee::PriError, "Errors not repairable via current -mem_repair sequence\n");
            return testRc;
        }
    }

    // Apply hard repair if the option is used
    if (bDoHardRepair)
    {
        if (bDoLaneRepair && !lanesToRepair.empty())
        {
            if (settings.modifiers.forceHtoJ)
            {
                Printf(Tee::PriError, "Host-to-JTAG interface not supported\n");
                return RC::UNSUPPORTED_FUNCTION;
            }

            std::unique_ptr<Memory::Hbm::HbmInterface> pNewHbmInterface;

            bool bSkipRmStateInit = false;
            CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipRmStateInit, &bSkipRmStateInit));
            if (!bSkipRmStateInit)
            {
                // Restart the GPU without RM to avoid HBM Temperature reads from corrupting hard
                // lane repairs
                Printf(Tee::PriLow, "Restarting GPU w/o RM to avoid RM accessing IEEE1500\n");

                InitializationConfig gpuConfig = {};
                gpuConfig.hbmDeviceInitMethod  = Gpu::InitHbmFbPrivToIeee;
                gpuConfig.initializeGpuTests   = false;
                gpuConfig.skipRmStateInit      = true;

                std::unique_ptr<RepairModuleMem> pRepairModule = std::make_unique<RepairModuleMem>();
                CHECK_RC(pRepairModule->DoGpuReset(pLwrrSysState,
                                                   Gpu::ResetMode::NoReset,
                                                   gpuConfig,
                                                   true));

                std::unique_ptr<RepairModuleHbm> pRepairHbm = std::make_unique<RepairModuleHbm>();
                CHECK_RC(pRepairHbm->SetupHbmRepairOnLwrrentGpu(settings,
                                                                *pLwrrSysState,
                                                                 ppHbmInterface));
            }
            Printf(Tee::PriNormal, "\n---- Starting Hard Lane-Repair sequence ----\n");
            CHECK_RC((*ppHbmInterface)->LaneRepair(settings,
                                                   lanesToRepair,
                                                   Memory::RepairType::HARD));
            Printf(Tee::PriNormal, "\n---- Hard Lane-Repair sequence complete ----\n");
        }

        /*
        if (bDoRowRepair)
        {
            // TODO: Perform hard row-repair
        }

        if (bDoBlacklist)
        {
            // TODO: Replace with row remapping + perform row remapping
        }
        */

        // Report a failure if there were additional errors after SLR and we
        // forced a hard repair
        if (bInsufficientRepair)
        {
            return RC::REPAIR_INSUFFICIENT;
        }
    }

    Printf(Tee::PriNormal, "\n---- Memory Repair Sequence complete! ----\n");
    return RC::OK;
}

/******************************************************************************/
// LaneRepairCommand

LaneRepairCommand::LaneRepairCommand(Memory::RepairType repairType, string argument)
    : Command(Command::Type::LaneRepair,
              Command::Group::Mem,
              Flags::PrimaryCommand | Flags::ExplicitRepair
              | Flags::InstInSysRequired),
      m_RepairType(repairType), m_Argument(argument)
{}

RC LaneRepairCommand::Validate
(
    const Settings& settings,
    ValidationResult* pResult
) const
{
    MASSERT(pResult);

    // TODO(stewarts): settings verification

    if (m_RepairType == Memory::RepairType::UNKNOWN)
    {
        Printf(Tee::PriError, "Lane repair: invalid repair type\n");
        return RC::BAD_PARAMETER;
    }

    pResult->isAttemptingHardRepair = (m_RepairType == Memory::RepairType::HARD);

    return RC::OK;
}

RC LaneRepairCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    RC rc;
    MASSERT(pLwrrSysState);
    MASSERT(ppHbmInterface);

    vector<FbpaLane> lanes;
    CHECK_RC(GetLanes(m_Argument, &lanes));

    CHECK_RC((*ppHbmInterface)->LaneRepair(settings, lanes, m_RepairType));

    return rc;
}

string LaneRepairCommand::ToString() const
{
    return Utility::StrPrintf("%s(repairType=%s, argument='%s')",
                              Command::ToString(m_Type).c_str(),
                              Memory::ToString(m_RepairType).c_str(),
                              m_Argument.c_str());
}

/******************************************************************************/
// RowRepairCommand

RowRepairCommand::RowRepairCommand(Memory::RepairType repairType, string argument)
    : Command(Command::Type::RowRepair,
              Command::Group::Mem,
              Flags::PrimaryCommand | Flags::ExplicitRepair
              | Flags::InstInSysRequired),
      m_RepairType(repairType), m_FailingPagesFilePath(argument)
{}

RC RowRepairCommand::Validate
(
    const Settings& settings,
    ValidationResult* pResult
) const
{
    MASSERT(pResult);

    // TODO(stewarts): settings verification

    if (m_RepairType == Memory::RepairType::UNKNOWN)
    {
        Printf(Tee::PriError, "Row repair: invalid repair type\n");
        return RC::BAD_PARAMETER;
    }

    pResult->isAttemptingHardRepair = (m_RepairType == Memory::RepairType::HARD);

    return RC::OK;
}

RC RowRepairCommand::GetRowErrorsFromFile(vector<RowError> *pRowErrors) const
{
    RC rc;
    MASSERT(pRowErrors);

    struct IsEqualRowErrorByRowFuncObj
    {
        bool operator()(const RowError& a, const RowError& b)
            { return IsEqualRowErrorByRow(a, b); }
    };

    pRowErrors->clear();
    CHECK_RC(GetRowErrors(m_FailingPagesFilePath, pRowErrors));
    const size_t originalNumRows = pRowErrors->size();

    std::sort(pRowErrors->begin(), pRowErrors->end(), Repair::CmpRowErrorByRow);
    RemoveDuplicatesFromSorted<vector<RowError>,
                               IsEqualRowErrorByRowFuncObj>(pRowErrors);

    if (pRowErrors->size() != originalNumRows)
    {
        MASSERT(originalNumRows > pRowErrors->size());
        Printf(Tee::PriNormal, "Duplicate rows removed from the repair list (%zu "
               "entries).\n", originalNumRows - pRowErrors->size());
    }

    return rc;
}

RC RowRepairCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    RC rc;
    StickyRC firstRc;

    MASSERT(pLwrrSysState);
    MASSERT(ppHbmInterface);

    vector<RowError> rowErrors;
    CHECK_RC(GetRowErrorsFromFile(&rowErrors));
    firstRc = (*ppHbmInterface)->RowRepair(settings, rowErrors, m_RepairType);

    JavaScriptPtr pJs;
    firstRc = pJs->CallMethod(pJs->GetGlobalObject(),
                              "RemoveFailingPagesFromRepairedRows");

    return firstRc;
}

RC RowRepairCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Gddr::GddrInterface>* ppGddrInterface
) const
{
    RC rc;
    StickyRC firstRc;

    vector<RowError> rowErrors;
    CHECK_RC(GetRowErrorsFromFile(&rowErrors));
    firstRc = (*ppGddrInterface)->RowRepair(settings, rowErrors, m_RepairType);

    JavaScriptPtr pJs;
    firstRc = pJs->CallMethod(pJs->GetGlobalObject(),
                              "RemoveFailingPagesFromRepairedRows");

    return firstRc;
}

string RowRepairCommand::ToString() const
{
    return Utility::StrPrintf("%s(repairType=%s, argument='%s')",
                              Command::ToString(m_Type).c_str(),
                              Memory::ToString(m_RepairType).c_str(),
                              m_FailingPagesFilePath.c_str());
}

/******************************************************************************/
// LaneRemapInfoCommand

LaneRemapInfoCommand::LaneRemapInfoCommand(string deviceIdStr, string argument)
    : Command(Command::Type::LaneRemapInfo,
              Command::Group::Mem,
              Flags::NoGpuRequired),
      m_DeviceIdStr(deviceIdStr), m_Argument(argument)
{}

RC LaneRemapInfoCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    RC rc;
    MASSERT(pLwrrSysState);
    MASSERT(ppHbmInterface);

    string devIdStr = m_DeviceIdStr; // mutable copy
    // Colwert the string to the recognized format for device IDs
    std::transform(devIdStr.begin(), devIdStr.end(), devIdStr.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    Device::LwDeviceId devId = Device::StringToDeviceId(devIdStr);

    if (devId == Device::LwDeviceId::ILWALID_DEVICE)
    {
        Printf(Tee::PriError, "Bad device ID specifier: '%s'\n", m_DeviceIdStr.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    vector<FbpaLane> lanes;
    CHECK_RC(GetLanes(m_Argument, &lanes));
    // TODO(stewarts): remove dependency on HBMDevice
    CHECK_RC(HBMDevice::GetLaneRemappingValues(static_cast<Gpu::LwDeviceId>(devId), lanes));

    return rc;
}

string LaneRemapInfoCommand::ToString() const
{
    return Utility::StrPrintf("%s(deviceIdStr='%s', argument='%s')",
                              Command::ToString(m_Type).c_str(),
                              m_DeviceIdStr.c_str(),
                              m_Argument.c_str());
}

/******************************************************************************/
// InfoRomRepairPopulateCommand

InfoRomRepairPopulateCommand::InfoRomRepairPopulateCommand()
    : Command(Command::Type::InfoRomRepairPopulate,
              Command::Group::Mem)
{}

RC InfoRomRepairPopulateCommand::Validate
(
    const Settings& settings,
    ValidationResult* pResult
) const
{
    Printf(Tee::PriError, "%s is not supported\n", ToString().c_str());
    return RC::UNSUPPORTED_FUNCTION;
}

RC InfoRomRepairPopulateCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    MASSERT(!"Execute should not be called");
    Printf(Tee::PriError, "%s is not supported\n", ToString().c_str());
    return RC::UNSUPPORTED_FUNCTION;
}

/******************************************************************************/
// InfoRomRepairValidateCommand

InfoRomRepairValidateCommand::InfoRomRepairValidateCommand()
    : Command(Command::Type::InfoRomRepairValidate,
              Command::Group::Mem)
{}

RC InfoRomRepairValidateCommand::Validate
(
    const Settings& settings,
    ValidationResult* pResult
) const
{
    Printf(Tee::PriError, "%s is not supported\n", ToString().c_str());
    return RC::UNSUPPORTED_FUNCTION;
}

RC InfoRomRepairValidateCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    MASSERT(!"Execute should not be called");
    Printf(Tee::PriError, "%s is not supported\n", ToString().c_str());
    return RC::UNSUPPORTED_FUNCTION;
}

/******************************************************************************/
// InfoRomRepairClearCommand

InfoRomRepairClearCommand::InfoRomRepairClearCommand()
    : Command(Command::Type::InfoRomRepairClear,
              Command::Group::Mem)
{}

RC InfoRomRepairClearCommand::Validate
(
    const Settings& settings,
    ValidationResult* pResult
) const
{
    Printf(Tee::PriError, "%s is not supported\n", ToString().c_str());
    return RC::UNSUPPORTED_FUNCTION;
}

RC InfoRomRepairClearCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    MASSERT(!"Execute should not be called");
    Printf(Tee::PriError, "%s is not supported\n", ToString().c_str());
    return RC::UNSUPPORTED_FUNCTION;
}

/******************************************************************************/
// InfoRomRepairPrintCommand

InfoRomRepairPrintCommand::InfoRomRepairPrintCommand()
    : Command(Command::Type::InfoRomRepairPrint,
              Command::Group::Mem)
{}

RC InfoRomRepairPrintCommand::Validate
(
    const Settings& settings,
    ValidationResult* pResult
) const
{
    Printf(Tee::PriError, "%s is not supported\n", ToString().c_str());
    return RC::UNSUPPORTED_FUNCTION;
}

RC InfoRomRepairPrintCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    MASSERT(!"Execute should not be called");
    Printf(Tee::PriError, "%s is not supported\n", ToString().c_str());
    return RC::UNSUPPORTED_FUNCTION;
}

/******************************************************************************/
// PrintHbmDevInfoCommand

PrintHbmDevInfoCommand::PrintHbmDevInfoCommand()
    : Command(Command::Type::PrintHbmDevInfo,
              Command::Group::Mem)
{}

RC PrintHbmDevInfoCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    MASSERT(pLwrrSysState);
    MASSERT(ppHbmInterface);

    return (*ppHbmInterface)->PrintHbmDeviceInfo();
}

/******************************************************************************/
// PrintRepairedLanesCommand

PrintRepairedLanesCommand::PrintRepairedLanesCommand()
    : Command(Command::Type::PrintRepairedLanes,
              Command::Group::Mem)
{}

RC PrintRepairedLanesCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    return (*ppHbmInterface)->PrintRepairedLanes(settings.modifiers.readHbmLanes);
}

/******************************************************************************/
// PrintRepairedRowsCommand

PrintRepairedRowsCommand::PrintRepairedRowsCommand()
    : Command(Command::Type::PrintRepairedRows,
              Command::Group::Mem)
{}

RC PrintRepairedRowsCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    MASSERT(pLwrrSysState);
    MASSERT(ppHbmInterface);

    return (*ppHbmInterface)->PrintRepairedRows();
}

/******************************************************************************/
// InfoRomTpcRepairPopulateCommand

InfoRomTpcRepairPopulateCommand::InfoRomTpcRepairPopulateCommand(string repairData, bool bAppend)
    : Command(Command::Type::InfoRomTpcRepairPopulate,
              Command::Group::Tpc,
              Flags::PrimaryCommand | Flags::ExplicitRepair),
      m_RepairData(repairData), m_bAppend(bAppend)
{}

RC InfoRomTpcRepairPopulateCommand::Execute
(
    const Settings& settings,
    const TpcRepair& tpcRepair
) const
{
    return tpcRepair.PopulateInforomEntries(m_RepairData, m_bAppend);
}

/******************************************************************************/
// InfoRomTpcRepairClearCommand

InfoRomTpcRepairClearCommand::InfoRomTpcRepairClearCommand()
    : Command(Command::Type::InfoRomRepairClear,
              Command::Group::Tpc,
              Flags::PrimaryCommand)
{}

RC InfoRomTpcRepairClearCommand::Execute
(
    const Settings& settings,
    const TpcRepair& tpcRepair
) const
{
    return tpcRepair.ClearInforomEntries();
}

/******************************************************************************/
// InfoRomTpcRepairPrintCommand

InfoRomTpcRepairPrintCommand::InfoRomTpcRepairPrintCommand()
    : Command(Command::Type::InfoRomTpcRepairPrint,
              Command::Group::Tpc)
{}

RC InfoRomTpcRepairPrintCommand::Execute
(
    const Settings& settings,
    const TpcRepair& tpcRepair
) const
{
    return tpcRepair.PrintInforomEntries();
}

/******************************************************************************/
// RunMBISTCommand

RunMBISTCommand::RunMBISTCommand()
    : Command(Command::Type::RunMBIST,
              Command::Group::Mem)
{}

RC RunMBISTCommand::Execute
(
    const Settings& settings,
    SystemState* pLwrrSysState,
    std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
) const
{
    RC rc;
    StickyRC firstRc;

    MASSERT(pLwrrSysState);
    MASSERT(ppHbmInterface);

    firstRc = (*ppHbmInterface)->RunMBist();

    return firstRc;
}

/******************************************************************************/
void CommandBuffer::Clear()
{
    m_Buffer.clear();
    m_RemapGpuLanesBackingStore.clear();
    m_MemRepairSeqBackingStore.clear();
    m_LaneRepairBackingStore.clear();
    m_RowRepairBackingStore.clear();
    m_LaneRemapInfoBackingStore.clear();
    m_InfoRomRepairPopulateBackingStore.clear();
    m_InfoRomRepairValidateBackingStore.clear();
    m_InfoRomRepairClearBackingStore.clear();
    m_InfoRomRepairPrintBackingStore.clear();
    m_PrintHbmDevInfoBackingStore.clear();
    m_PrintRepairedLanesBackingStore.clear();
    m_PrintRepairedRowsBackingStore.clear();
    m_InfoRomTpcRepairPopulateBackingStore.clear();
    m_InfoRomTpcRepairClearBackingStore.clear();
    m_InfoRomTpcRepairPrintBackingStore.clear();
}

void CommandBuffer::AddCommandRemapGpuLanes(string mappingFile)
{
    m_RemapGpuLanesBackingStore.emplace_back(mappingFile);
    m_Buffer.push_back(m_RemapGpuLanesBackingStore.back());
}

void CommandBuffer::AddCommandMemRepairSeq()
{
    m_MemRepairSeqBackingStore.emplace_back();
    m_Buffer.push_back(m_MemRepairSeqBackingStore.back());
}

void CommandBuffer::AddCommandLaneRepair(Memory::RepairType repairType, string argument)
{
    m_LaneRepairBackingStore.emplace_back(repairType, argument);
    m_Buffer.push_back(m_LaneRepairBackingStore.back());
}

void CommandBuffer::AddCommandRowRepair(Memory::RepairType repairType, string argument)
{
    m_RowRepairBackingStore.emplace_back(repairType, argument);
    m_Buffer.push_back(m_RowRepairBackingStore.back());
}

void CommandBuffer::AddCommandLaneRemapInfo(string deviceIdStr, string argument)
{
    m_LaneRemapInfoBackingStore.emplace_back(deviceIdStr, argument);
    m_Buffer.push_back(m_LaneRemapInfoBackingStore.back());
}

void CommandBuffer::AddCommandInfoRomRepairPopulate()
{
    m_InfoRomRepairPopulateBackingStore.emplace_back();
    m_Buffer.push_back(m_InfoRomRepairPopulateBackingStore.back());
}

void CommandBuffer::AddCommandInfoRomRepairValidate()
{
    m_InfoRomRepairValidateBackingStore.emplace_back();
    m_Buffer.push_back(m_InfoRomRepairValidateBackingStore.back());
}

void CommandBuffer::AddCommandInfoRomRepairClear()
{
    m_InfoRomRepairClearBackingStore.emplace_back();
    m_Buffer.push_back(m_InfoRomRepairClearBackingStore.back());
}

void CommandBuffer::AddCommandInfoRomRepairPrint()
{
    m_InfoRomRepairPrintBackingStore.emplace_back();
    m_Buffer.push_back(m_InfoRomRepairPrintBackingStore.back());
}

void CommandBuffer::AddCommandPrintHbmDevInfo()
{
    m_PrintHbmDevInfoBackingStore.emplace_back();
    m_Buffer.push_back(m_PrintHbmDevInfoBackingStore.back());
}

void CommandBuffer::AddCommandPrintRepairedLanes()
{
    m_PrintRepairedLanesBackingStore.emplace_back();
    m_Buffer.push_back(m_PrintRepairedLanesBackingStore.back());
}

void CommandBuffer::AddCommandPrintRepairedRows()
{
    m_PrintRepairedRowsBackingStore.emplace_back();
    m_Buffer.push_back(m_PrintRepairedRowsBackingStore.back());
}

void CommandBuffer::AddCommandInfoRomTpcRepairPopulate(string inputData, bool bAppend)
{
    m_InfoRomTpcRepairPopulateBackingStore.emplace_back(inputData, bAppend);
    m_Buffer.push_back(m_InfoRomTpcRepairPopulateBackingStore.back());
}

void CommandBuffer::AddCommandInfoRomTpcRepairClear()
{
    m_InfoRomTpcRepairClearBackingStore.emplace_back();
    m_Buffer.push_back(m_InfoRomTpcRepairClearBackingStore.back());
}

void CommandBuffer::AddCommandInfoRomTpcRepairPrint()
{
    m_InfoRomTpcRepairPrintBackingStore.emplace_back();
    m_Buffer.push_back(m_InfoRomTpcRepairPrintBackingStore.back());
}

void CommandBuffer::AddCommandRunMBIST()
{
    m_RunMBISTBackingStore.emplace_back();
    m_Buffer.push_back(m_RunMBISTBackingStore.back());
}

void CommandBuffer::OrderCommandBuffer(CommandOrderCmpFunc cmp)
{
    std::sort(m_Buffer.begin(), m_Buffer.end(), cmp);
}

bool CommandBuffer::HasCommand(Repair::Command::Type type) const
{
    return std::find_if(m_Buffer.begin(), m_Buffer.end(),
                        [&](const std::reference_wrapper<const Repair::Command>& o)
                        {
                            return o.get().GetType() == type;
                        })
        != m_Buffer.end();
}

string CommandBuffer::ToString() const
{
    stringstream ss;

    // Colwert booleans to "true"/"false" instead of "1"/"0"
    ss << std::boolalpha;

    ss << "Commands: [";
    constexpr const char separator[] = ",";
    constexpr INT32 separatorLen = sizeof(separator) - 1; // -1 for null terminator
    for (const Repair::Command& cmd : m_Buffer)
    {
        ss << "\n  " << cmd.ToString() << separator;
    }

    if (!m_Buffer.empty())
    {
        // Write over the last separator
        ss.seekp(ss.tellp()
                 - static_cast<decltype(ss)::pos_type>(separatorLen));
    }

    ss << " ])";

    return ss.str();
}
