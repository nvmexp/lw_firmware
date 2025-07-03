/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "repair_module.h"
#include "repair_module_mem.h"
#include "repair_module_tpc.h"

#include "core/include/tee.h"

using CmdT     = Repair::Command::Type;
using CmdFlags = Repair::Command::Flags;
using CmdGroup = Repair::Command::Group;

/******************************************************************************/
// RepairModule

vector<Repair::Command::Type> RepairModule::s_CommandOrderReference =
{
    // Mem Pre-repairs
    CmdT::LaneRemapInfo,
    CmdT::PrintHbmDevInfo,
    CmdT::PrintRepairedLanes,
    CmdT::PrintRepairedRows,

    // Mem Repairs
    CmdT::MemRepairSeq,
    CmdT::LaneRepair,
    CmdT::RowRepair,
    CmdT::ReconfigGpuLanes,

    // Mem Post-repairs
    CmdT::InfoRomRepairPopulate,
    CmdT::InfoRomRepairClear,
    CmdT::InfoRomRepairPrint,
    CmdT::InfoRomRepairValidate,

    // TPC repairs
    CmdT::InfoRomTpcRepairPopulate,
    CmdT::InfoRomTpcRepairClear,
    CmdT::InfoRomTpcRepairPrint
};

bool RepairModule::CommandOrderCmp(const Cmd& a, const Cmd& b)
{
    const auto iterA = std::find(s_CommandOrderReference.begin(),
                                 s_CommandOrderReference.end(), a.GetType());
    const auto iterB = std::find(s_CommandOrderReference.begin(),
                                 s_CommandOrderReference.end(), b.GetType());

    return std::distance(s_CommandOrderReference.begin(), iterA)
        < std::distance(s_CommandOrderReference.begin(), iterB);
}

RC RepairModule::Run
(
    std::unique_ptr<Repair::Settings> pSettings,
    std::unique_ptr<CmdBuf> pCmdBuf
)
{
    RC rc;

    if (pCmdBuf->Empty())
    {
        Printf(Tee::PriError, "No actions to perform in the repair module\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Make sure all the commands belong to the same type
    CmdBufProperties cmdBufProp;
    cmdBufProp.cmdGroup = CmdGroup::Unknown;
    for (const Cmd& cmd : pCmdBuf->Get())
    {
        CmdGroup lwrrGroup = cmd.GetGroup();
        if (cmdBufProp.cmdGroup != CmdGroup::Unknown &&
            cmdBufProp.cmdGroup != lwrrGroup)
        {
            Printf(Tee::PriError, "Found commands from multiple repair groups\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        cmdBufProp.cmdGroup = lwrrGroup;
    }

    // Order commands to the proper repair order
    //
    // NOTE: Must be done before validation, otherwise we are not validating the
    // actual exelwtion order.
    pCmdBuf->OrderCommandBuffer(RepairModule::CommandOrderCmp);
    CHECK_RC(ValidateCommandBuffer(*pSettings, *pCmdBuf, &cmdBufProp));

    // Validate and execute commands based on the command group
    std::unique_ptr<RepairModuleImpl> pRepairModuleImpl;
    switch (cmdBufProp.cmdGroup)
    {
        case CmdGroup::Mem:
            pRepairModuleImpl = std::make_unique<RepairModuleMem>();
            break;
        case CmdGroup::Tpc:
            pRepairModuleImpl = std::make_unique<RepairModuleTpc>();
            break;
        default:
            Printf(Tee::PriError, "Unknown repair command\n");
            MASSERT(!"Unknown repair command");
            return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(pRepairModuleImpl->ValidateSettings(*pSettings, cmdBufProp));
    CHECK_RC(pRepairModuleImpl->ExelwteCommands(*pSettings, *pCmdBuf, cmdBufProp));

    return rc;
}

RC RepairModule::ValidateCommandBuffer
(
    const Repair::Settings& settings,
    const CmdBuf& cmdBuf,
    RepairModule::CmdBufProperties* pProp
) const
{
    MASSERT(pProp);
    RC rc;

    const Cmd* pPrimaryCommand = nullptr;
    const Cmd* pExplicitRepairCommand = nullptr;

    for (const Cmd& cmd : cmdBuf.Get())
    {
        Cmd::ValidationResult validationResult;
        CHECK_RC(cmd.Validate(settings, &validationResult));
        pProp->isAttemptingHardRepair |= validationResult.isAttemptingHardRepair;

        // Check for multiple primary commands
        if (cmd.IsSet(CmdFlags::PrimaryCommand))
        {
            if (pPrimaryCommand)
            {
                MASSERT(pPrimaryCommand);
                Printf(Tee::PriError,
                       "Found multiple primary commands, only one allowed: [%s, %s]\n",
                       pPrimaryCommand->ToString().c_str(), cmd.ToString().c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            else
            {
                pPrimaryCommand = &cmd;
            }
        }

        // Check for multiple explicit repair commands
        if (cmd.IsSet(CmdFlags::ExplicitRepair))
        {
            if (pProp->hasExplicitRepairCommand)
            {
                MASSERT(pExplicitRepairCommand);
                Printf(Tee::PriError,
                       "Multiple explicit repair commands, only one allowed: [%s, %s]\n",
                       pExplicitRepairCommand->ToString().c_str(), cmd.ToString().c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            else
            {
                pExplicitRepairCommand          = &cmd;
                pProp->hasExplicitRepairCommand = true;
            }
        }

        // In case of just printing register access sequence, -inst_in_sys is not required
        if (!settings.modifiers.printRegSeq)
        {
            // Check if -inst_in_sys required
            if (cmd.IsSet(CmdFlags::InstInSysRequired) && !settings.runningInstInSys)
            {
                Printf(Tee::PriError, "Command requires -inst_in_sys: %s\n",
                        cmd.ToString().c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    return rc;
}
