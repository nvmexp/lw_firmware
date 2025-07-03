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

#include "core/include/hbmtypes.h"
#include "gpu/repair/hbm/gpu_interface/gpu_hbm_interface.h"
#include "gpu/repair/hbm/hbm_interface/hbm_interface.h"
#include "gpu/repair/repair_module_mem.h"

#include <memory>

class RepairModuleHbm : public RepairModuleMemImpl
{
    using RawEcid              = Repair::RawEcid;
    using GpuHbmInterface      = Memory::Hbm::GpuHbmInterface;
    using HbmInterface         = Memory::Hbm::HbmInterface;
    using HbmModel             = Memory::Hbm::Model;

public:
    RepairModuleHbm() {}
    virtual ~RepairModuleHbm() = default;

    //!
    //! \brief Execute the given commands on the current GPU.
    //!
    //! \param settings Repair settings.
    //! \param[in,out] pSysState State of the system. Defines the current GPU.
    //! State will be updated to reflext the current state of the system.
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
    //! \brief Setup requirements for HBM repair.
    //!
    //! \param sysState State of the system.
    //! \param[out] ppHbmInterface HBM interface.
    //!
    RC SetupHbmRepairOnLwrrentGpu
    (
        const Settings& settings,
        const SystemState& sysState,
        std::unique_ptr<HbmInterface>* ppHbmInterface
    ) const;

private:
    //!
    //! \brief Execute the given commands on the current GPU.
    //!
    //! \param settings Repair settings.
    //! \param[in,out] pSysState State of the system. Defines the current GPU.
    //! State will be updated to reflext the current state of the system.
    //! \param cmdBuf Command buffer that defines exelwtion. Commands are
    //! exelwted in the same order as presented.
    //! \param ppHbmInterface HBM interface.
    //!
    RC ExelwteHbmCommandsOnLwrrentGpu
    (
        const Settings& settings,
        SystemState* pSysState,
        const CmdBuf& cmdBuf,
        std::unique_ptr<HbmInterface>* ppHbmInterface
    ) const;

    //!
    //! \brief Create GPU-to-HBM interface for the system state's current GPU.
    //!
    //! \param sysState State of the current system. Defines the current GPU.
    //! \param[out] ppGpuHbmInterface GPU-to-HBM interface. Any object stored in
    //! the given unique pointer will be destroyed.
    //!
    RC CreateGpuHbmInterface
    (
        const Settings& settings,
        const SystemState& sysState,
        std::unique_ptr<GpuHbmInterface>* ppGpuHbmInterface
    ) const;

    //!
    //! \brief Create an HBM interface for the system state's current GPU.
    //!
    //! \param sysState State of the current system. Defines the current GPU.
    //! \param hbmModel Interface to create for this model of HBM.
    //! \param[out] ppHbmInterface HBM interface. Any object stored in the given
    //! unique pointer will be destroyed.
    //!
    RC CreateHbmInterface
    (
        const Settings& settings,
        const SystemState& sysState,
        const HbmModel& hbmModel,
        std::unique_ptr<HbmInterface>* ppHbmInterface
    ) const;
};
