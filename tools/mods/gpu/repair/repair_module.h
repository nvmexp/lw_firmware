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

#pragma once

#include "core/include/rc.h"
#include "gpu/repair/repair_module_commands.h"

class RepairModule
{
    using Cmd        = Repair::Command;
    using CmdBuf     = Repair::CommandBuffer;
    using CmdGroup   = Repair::Command::Group;
    using CmdT       = Repair::Command::Type;
    using Settings   = Repair::Settings;

public:

    //!
    //! \brief Properties of the commands in the buffer
    //!
    struct CmdBufProperties
    {
        bool isAttemptingHardRepair = false;
        bool hasExplicitRepairCommand = false;   
        CmdGroup cmdGroup = CmdGroup::Unknown;
    };

    //!
    //! \brief Run the repair module.
    //!
    //! \param pSettings Repair settings.
    //! \param pCommandBuffer List of commands to run.
    //!
    RC Run
    (
        std::unique_ptr<Settings> pSettings,
        std::unique_ptr<CmdBuf> pCommandBuffer
    );

private:
    //! Defines the sort ordering for repair commands.
    static vector<CmdT> s_CommandOrderReference;

    //!
    //! \brief Command comparator.
    //!
    static bool CommandOrderCmp(const Cmd& a, const Cmd& b);

    //!
    //! \brief Validate command buffer.
    //!
    //! Done as part of all settings validation.
    //!
    //! \param settings    Repair settings.
    //! \param cmdBuf      Command buffer.
    //! \param[out] pProp  Command buffer properties
    //!
    //! \see ValidateSettings
    //!
    RC ValidateCommandBuffer
    (
        const Settings& settings,
        const CmdBuf& cmdBuf,
        RepairModule::CmdBufProperties* pProp
    ) const;
};

class RepairModuleImpl
{
public:
    using Cmd                  = Repair::Command;
    using CmdBuf               = Repair::CommandBuffer;
    using CmdFlags             = Repair::Command::Flags;
    using CmdT                 = Repair::Command::Type;
    using Settings             = Repair::Settings;

    virtual ~RepairModuleImpl() = default;

    //!
    //! \brief Validate all repair module settings.
    //!
    //! \param settings   Repair settings.
    //! \param prop       Command buffer properties
    //!
    virtual RC ValidateSettings
    (
        const Settings& settings,
        const RepairModule::CmdBufProperties& prop
    ) const = 0;

    //!
    //! \brief Setup and execute commands on all GPUs
    //!
    //! \param settings   Repair settings.
    //! \param cmdBuf     Command buffer that defines exelwtion. Commands are
    //!                   exelwted in the same order as presented.
    //! \param prop       Command buffer properties
    //!
    virtual RC ExelwteCommands
    (
        const Settings& settings,
        const CmdBuf& cmdBuf,
        const RepairModule::CmdBufProperties& prop
    ) const = 0;
};
