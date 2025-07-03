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

#include "gpu/repair/gddr/gddr_model.h"
#include "gpu/repair/gddr/gddr_interface/gddr_interface.h"
#include "gpu/repair/mem_repair_config.h"
#include "gpu/repair/repair_module_mem.h"

#include <memory>

class RepairModuleGddr : public RepairModuleMemImpl
{
    using RawEcid          = Repair::RawEcid;
    using GddrInterface    = Memory::Gddr::GddrInterface;
    using GddrModel        = Memory::Gddr::Model;

public:
    RepairModuleGddr() {}
    virtual ~RepairModuleGddr() = default;

    //!
    //! \brief Execute the given commands on the current GPU.
    //!
    //! \param settings Repair settings.
    //! \param[in,out] pSysState State of the system. Defines the current GPU.
    //! State will be updated to reflect the current state of the system.
    //! \param cmdBuf Command buffer that defines exelwtion. Commands are
    //! exelwted in the same order as presented.
    //!
    RC ExelwteCommandsOnLwrrentGpu
    (
        const Settings& settings,
        SystemState* pSysState,
        const CmdBuf& cmdBuf
    ) const override;

    //!
    //! \brief Setup requirements for GDDR repair.
    //!
    //! \param sysState State of the system.
    //! \param[out] ppGddrInterface GDDR interface.
    //!
    RC SetupGddrRepairOnLwrrentGpu
    (
        const Repair::Settings& settings,
        const SystemState& sysState,
        std::unique_ptr<GddrInterface>* ppGddrInterface
    ) const;

private:
    //!
    //! \brief Create an GDDR interface for the system state's current GPU.
    //!
    //! \param sysState State of the current system. Defines the current GPU.
    //! \param gddrModel Interface to create for this model of GDDR.
    //! \param[out] ppGddrInterface GDDR interface. Any object stored in the given
    //! unique pointer will be destroyed.
    //!
    RC CreateGddrInterface
    (
        const Repair::Settings& settings,
        const SystemState& sysState,
        const Memory::Gddr::Model& gddrModel,
        std::unique_ptr<GddrInterface>* ppGddrInterface
    ) const;

    //!
    //! \brief Execute the given commands on the current GPU.
    //!
    //! \param settings Repair settings.
    //! \param[in,out] pSysState State of the system. Defines the current GPU.
    //! State will be updated to reflext the current state of the system.
    //! \param cmdBuf Command buffer that defines exelwtion. Commands are
    //! exelwted in the same order as presented.
    //! \param gddrInterface GDDR interface.
    //!
    RC ExelwteGddrCommandsOnLwrrentGpu
    (
        const Repair::Settings& settings,
        SystemState* pSysState,
        const CmdBuf& cmdBuf,
        std::unique_ptr<GddrInterface>* ppGddrInterface
    ) const;
};
