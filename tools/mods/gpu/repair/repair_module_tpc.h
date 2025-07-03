/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/repair/repair_module.h"
#include "gpu/repair/repair_module_commands.h"

class RepairModuleTpc : public RepairModuleImpl
{

public:
    RepairModuleTpc() {}
    virtual ~RepairModuleTpc() = default;

    //!
    //! \brief Validate all repair module settings.
    //!
    //! \param settings   Repair settings.
    //! \param prop       Command buffer properties.
    //!
    RC ValidateSettings
    (
        const Settings& settings,
        const RepairModule::CmdBufProperties& prop
    ) const override;

    //!
    //! \brief Setup and execute commands on all GPUs
    //!
    //! \param settings   Repair settings.
    //! \param cmdBuf     Command buffer that defines exelwtion. Commands are
    //!                   exelwted in the same order as presented.
    //! \param prop       Command buffer properties.
    //!
    RC ExelwteCommands
    (
        const Settings& settings,
        const CmdBuf& cmdBuf,
        const RepairModule::CmdBufProperties& prop
    ) const override;

private:
    //!
    //! \brief Setup and execute commands on given GPU
    //!
    //! \param pSubdev    Subdevice to execute commands on.
    //! \param settings   Repair settings.
    //! \param cmdBuf     Command buffer that defines exelwtion.
    //!
    RC ExelwteCommandsOnLwrrentGpu
    (
        GpuSubdevice *pSubdev,
        const Repair::Settings& settings,
        const CmdBuf& cmdBuf
    ) const;
};
