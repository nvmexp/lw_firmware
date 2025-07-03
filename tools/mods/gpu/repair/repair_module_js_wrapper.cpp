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

#include "gpu/repair/repair_module_js_wrapper.h"

#include "core/include/jswrap.h"
#include "core/include/script.h"

#include <utility>

using namespace Memory;

std::unique_ptr<RepairModule>
RepairModuleJsWrapper::s_pRepairModule(nullptr);

std::unique_ptr<Repair::CommandBuffer>
RepairModuleJsWrapper::s_pCommandBuffer = std::make_unique<Repair::CommandBuffer>();

/******************************************************************************/
// RepairModuleJsWrapper

void RepairModuleJsWrapper::ResetCommands()
{
    s_pCommandBuffer->Clear();
}

void RepairModuleJsWrapper::AddCommandRemapGpuLanes(string mappingFile)
{
    s_pCommandBuffer->AddCommandRemapGpuLanes(mappingFile);
}

void RepairModuleJsWrapper::AddCommandMemRepairSeq()
{
    s_pCommandBuffer->AddCommandMemRepairSeq();
}

void RepairModuleJsWrapper::AddCommandLaneRepair(bool isHard, string argument)
{
    s_pCommandBuffer->AddCommandLaneRepair((isHard
                                            ? Memory::RepairType::HARD
                                            : Memory::RepairType::SOFT),
                                           argument);
}

void RepairModuleJsWrapper::AddCommandRowRepair(bool isHard, string argument)
{
    s_pCommandBuffer->AddCommandRowRepair((isHard
                                            ? Memory::RepairType::HARD
                                            : Memory::RepairType::SOFT),
                                          argument);
}

void RepairModuleJsWrapper::AddCommandLaneRemapInfo
(
    string deviceId,
    string argument
)
{
    s_pCommandBuffer->AddCommandLaneRemapInfo(deviceId, argument);
}

void RepairModuleJsWrapper::AddCommandInfoRomRepairPopulate()
{
    s_pCommandBuffer->AddCommandInfoRomRepairPopulate();
}

void RepairModuleJsWrapper::AddCommandInfoRomRepairValidate()
{
    s_pCommandBuffer->AddCommandInfoRomRepairValidate();
}

void RepairModuleJsWrapper::AddCommandInfoRomRepairClear()
{
    s_pCommandBuffer->AddCommandInfoRomRepairClear();
}

void RepairModuleJsWrapper::AddCommandInfoRomRepairPrint()
{
    s_pCommandBuffer->AddCommandInfoRomRepairPrint();
}

void RepairModuleJsWrapper::AddCommandPrintHbmDevInfo()
{
    s_pCommandBuffer->AddCommandPrintHbmDevInfo();
}

void RepairModuleJsWrapper::AddCommandPrintRepairedLanes()
{
    s_pCommandBuffer->AddCommandPrintRepairedLanes();
}

void RepairModuleJsWrapper::AddCommandPrintRepairedRows()
{
    s_pCommandBuffer->AddCommandPrintRepairedRows();
}

void RepairModuleJsWrapper::AddCommandInfoRomTpcRepairPopulate(string inputData, bool bAppend)
{
    s_pCommandBuffer->AddCommandInfoRomTpcRepairPopulate(inputData, bAppend);
}

void RepairModuleJsWrapper::AddCommandInfoRomTpcRepairClear()
{
    s_pCommandBuffer->AddCommandInfoRomTpcRepairClear();
}

void RepairModuleJsWrapper::AddCommandInfoRomTpcRepairPrint()
{
    s_pCommandBuffer->AddCommandInfoRomTpcRepairPrint();
}

void RepairModuleJsWrapper::AddCommandRunMBIST()
{
    s_pCommandBuffer->AddCommandRunMBIST();
}

RC RepairModuleJsWrapper::RunJsEntryPoint()
{
    RC rc;

    if (!s_pRepairModule)
    {
        s_pRepairModule = make_unique<RepairModule>();
    }

    unique_ptr<Repair::Settings> pSettings = make_unique<Repair::Settings>();
    unique_ptr<Repair::CommandBuffer> pCmdBuf = make_unique<Repair::CommandBuffer>();

    CHECK_RC(CollectConfiguration(pSettings.get(), &pCmdBuf));

    return s_pRepairModule->Run(std::move(pSettings), std::move(pCmdBuf));
}

RC RepairModuleJsWrapper::PullSettingsFromJs(Repair::Settings* pSettings)
{
    MASSERT(pSettings);

    RC rc;
    JavaScriptPtr pJs;
    JSObject* pJsGlobalObj = pJs->GetGlobalObject();

    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_HbmResetDurationMs",     &pSettings->modifiers.hbmResetDurationMs));
    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_ForceHtoJ",              &pSettings->modifiers.forceHtoJ));
    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_PseudoHardRepair",       &pSettings->modifiers.pseudoHardRepair));
    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_ReadHbmLanes",           &pSettings->modifiers.readHbmLanes));
    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_SkipHbmFuseRepairCheck", &pSettings->modifiers.skipHbmFuseRepairCheck));
    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_IgnoreHbmFuseRepairCheckResult", &pSettings->modifiers.ignoreHbmFuseRepairCheckResult));
    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_RepairMode",             reinterpret_cast<UINT32*>(&pSettings->modifiers.memRepairSeqMode)));
    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_PrintRegSeq",            &pSettings->modifiers.printRegSeq));

    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_InstInSys", &pSettings->runningInstInSys));

    return rc;
}

RC RepairModuleJsWrapper::CollectConfiguration
(
    Repair::Settings* pSettings,
    std::unique_ptr<Repair::CommandBuffer>* ppCmdBuf
)
{
    MASSERT(pSettings);
    MASSERT(ppCmdBuf);

    RC rc;
    CHECK_RC(PullSettingsFromJs(pSettings));
    std::swap(*ppCmdBuf, s_pCommandBuffer);
    s_pCommandBuffer->Clear();
    return rc;
}

/******************************************************************************/
// JS Linkage
//
// NOTE(stewarts): Could create multiple instance of repair module by defining a
// JsRepairModule and having JS do `let repairModule = RepairModule()`. This
// seems unnecessary for now.

JS_CLASS(RepairModule);
static SObject RepairModule_Object
(
    "RepairModule",
    RepairModuleClass,
    0,
    0,
    "RepairModule JS Object"
);

//!
//! \see RepairModuleJsWrapper::RunJsEntryPoint
//!
C_(RepairModule_RunJsEntryPoint);
static STest RepairModule_RunJsEntryPoint
(
    RepairModule_Object,
    "RunJsEntryPoint",
    C_RepairModule_RunJsEntryPoint,
    0,
    "Repair Module JS entry point"
);

C_(RepairModule_RunJsEntryPoint)
{
    RETURN_RC(RepairModuleJsWrapper::RunJsEntryPoint());
}

//!
//! \brief Expose RepairMode to JS.
//!
//! \see Repair::MemRepairSeqMode
//!
JS_CLASS(MemRepairModeConst);
static SObject MemRepairModeConst_Object
(
    "MemRepairModeConst",
    MemRepairModeConstClass,
    0,
    0,
    "MemRepairModeConst JS Object"
);
PROP_CONST(MemRepairModeConst, Lane, static_cast<UINT32>(Repair::MemRepairSeqMode::Lane));
PROP_CONST(MemRepairModeConst, Row, static_cast<UINT32>(Repair::MemRepairSeqMode::Row));
PROP_CONST(MemRepairModeConst, Blacklist, static_cast<UINT32>(Repair::MemRepairSeqMode::Blacklist));
PROP_CONST(MemRepairModeConst, Hard, static_cast<UINT32>(Repair::MemRepairSeqMode::Hard));
PROP_CONST(MemRepairModeConst, ForceHard, static_cast<UINT32>(Repair::MemRepairSeqMode::ForceHard));
PROP_CONST(MemRepairModeConst, DefaultMemRepairSeqMode, static_cast<UINT32>(Repair::s_DefaultMemRepairSeqMode));

//!
//! \see RepairModuleJsWrapper
//!
JS_CLASS(RepairModuleJsWrapper);
static SObject RepairModuleJsWrapper_Object
(
    "RepairModuleJsWrapper",
    RepairModuleJsWrapperClass,
    0,
    0,
    "RepairModuleJsWrapper class"
);
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandRemapGpuLanes,
                   "Add RemapGpuLanes to command buffer. Usage: AddCommandRemapGpuLanes(mappingFile:string)");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandMemRepairSeq,
                   "Add memory repair sequence to command buffer. Usage: AddCommandMemRepairSeq()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandLaneRepair,
                   "Add lane repair to command buffer. Usage: AddCommandLaneRepair(isHard:bool, argument:string)");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandRowRepair,
                   "Add row repair to command buffer. Usage: AddCommandRowRepair(isHard:bool, argument:string)");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandLaneRemapInfo,
                   "Add lane remap info to command buffer. Usage: AddCommandLaneRemapInfo(deviceId:string, argument:string)");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandInfoRomRepairPopulate,
                   "Add InfoROM RPR populate to command buffer. Usage: AddCommandInfoRomRepairPopulate()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandInfoRomRepairValidate,
                   "Add InfoROM RPR validate to command buffer. Usage: AddCommandInfoRomRepairValidate()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandInfoRomRepairClear,
                   "Add InfoROM RPR clear to command buffer. Usage: AddCommandInfoRomRepairClear()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandInfoRomRepairPrint,
                   "Add InfoROM RPR print to command buffer. Usage: AddCommandInfoRomRepairPrint()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandPrintHbmDevInfo,
                   "Add print HBM device information to command buffer. Usage: AddCommandPrintHbmDevInfo()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandPrintRepairedLanes,
                   "Add print repaired lanes to command buffer. Usage: AddCommandPrintRepairedLanes()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandPrintRepairedRows,
                   "Add print repaired rows to command buffer. Usage: AddCommandPrintRepairedRows()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandInfoRomTpcRepairPopulate,
                   "Add InfoROM TPC repair populate to command buffer. Usage: AddCommandInfoRomTpcRepairPopulate(input:string, append:bool)");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandInfoRomTpcRepairClear,
                   "Add InfoROM TPC repair clear to command buffer. Usage: AddCommandInfoRomTpcRepairClear()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandInfoRomTpcRepairPrint,
                   "Add InfoROM TPC repair print to command buffer. Usage: AddCommandInfoRomTpcRepairPrint()");
JS_REGISTER_METHOD(RepairModuleJsWrapper, AddCommandRunMBIST,
                   "Add Run MBIST to command buffer. Usage: AddCommandRunMBIST()");
