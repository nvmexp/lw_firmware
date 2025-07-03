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

class RepairModuleMem : public RepairModuleImpl
{
    using InitializationConfig = Repair::InitializationConfig;
    using RawEcid              = Repair::RawEcid;
    using SystemState          = Repair::SystemState;

public:
    RepairModuleMem() {}
    virtual ~RepairModuleMem() = default;

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

    //!
    //! \brief Reset all GPUs on the system.
    //!
    //! Will only shutdown the GPUs if they were previously initialized within
    //! the context of the repair module.
    //!
    //! NOTE: If there is no current ECID set in the system state, then
    //! the system state's subdevice will be set to nullptr.
    //!
    //! \param[in,out] pSysState State of the system. Updated to reflect the
    //! current state of the system.
    //! \param resetMode GPU reset mode.
    //! \param initConfig Initialization configuration to use when
    //! re-initializing the GPUs.
    //! \param printSystemInfo True if the GPU initialization information should
    //! be printed after initialization, false otherwise. This includes the MODS
    //! header.
    //!
    RC DoGpuReset
    (
        SystemState* pSysState,
        Gpu::ResetMode resetMode,
        const InitializationConfig& initConfig,
        bool printSystemInfo
    ) const;

private:
    //!
    //! \brief Setups the system state for the next GPU to repair.
    //!
    //! If there is no next GPU, system state's \a lwrrentGpu will be default
    //! initialized (ie. not set).
    //!
    //! \param[in,out] pSysState State of the system. The current GPU will be
    //! updated to the next GPU to repair.
    //! \param gpuConfigAtRepairStart GPU initialization configuration expected
    //! at the start of the next repair.
    //!
    RC PrepareSystemForNextGpuRepair
    (
        SystemState* pSysState,
        const InitializationConfig& gpuConfigAtRepairStart
    ) const;

    //!
    //! \brief Determine expected GPU configuration before the start of each
    //! GPU's repair.
    //!
    //! The 'repair' is defined by the command buffer. The GPU state is based
    //! on the commands to be performed.
    //!
    //! \param settings Repair settings.
    //! \param cmdBuf Commands to execute during repair. Sequential in-order
    //! exelwtion.
    //! \param[out] pInitConfig The initalization configuration that represents
    //! the expected GPU configuration.
    //!
    RC GetExpectedInitialGpuConfiguration
    (
        const Settings& settings,
        const CmdBuf& cmdBuf,
        InitializationConfig* pInitConfig
    ) const;

    RC DoInitialGpuInit
    (
        const InitializationConfig &gpuConfigAtRepairStart,
        SystemState *pSysState
    ) const;

    //!
    //! \brief Get GPU's raw ECID and print message if an error oclwrs.
    //!
    //! \param pSubdev GPU subdevice to fetch raw ECID for.
    //! \param[out] pGpuRawEcid Raw ECID of the GPU.
    //!
    RC CheckedGetGpuRawEcid(GpuSubdevice* pSubdev, RawEcid* pGpuRawEcid) const;

    //!
    //! \brief Find the GPU subdevice corresponding to a raw ECID.
    //!
    //! \param[in] pGpuDevMgr GPU device manager.
    //! \param desiredGpuRawEcid Raw ECID of the GPU to search for.
    //! \param[out] ppSubdev Set to GPU device corresponding to the given raw
    //! ECID, or nullptr if the raw ECID could not be found on any active GPU.
    //!
    RC FindGpuSubdeviceFromRawEcid
    (
        GpuDevMgr* pGpuDevMgr,
        const RawEcid& desiredGpuRawEcid,
        GpuSubdevice** ppSubdev
    ) const;

    //!
    //! \brief Print the GPU repair header.
    //!
    //! Prints the GPU raw ECID as an identifier, or it's PID if the raw ECID is
    //! unavailable.
    //!
    //! \param sysState System of the system. Current GPU must be set.
    //!
    void PrintGpuRepairHeader(const SystemState& sysState) const;
};

// ----------------------------------------------------------------------------

class RepairModuleMemImpl
{
public:

    using Cmd          = Repair::Command;
    using CmdBuf       = Repair::CommandBuffer;
    using CmdT         = Repair::Command::Type;
    using Settings     = Repair::Settings;
    using SystemState  = Repair::SystemState;

    //!
    //! \brief Execute the given commands on the current GPU.
    //!
    //! \param settings Repair settings.
    //! \param[in,out] pSysState State of the system. Defines the current GPU.
    //! State will be updated to reflext the current state of the system.
    //! \param cmdBuf Command buffer that defines exelwtion. Commands are
    //! exelwted in the same order as presented.
    //!
    virtual RC ExelwteCommandsOnLwrrentGpu
    (
        const Settings& settings,
        SystemState* pSysState,
        const CmdBuf& cmdBuf
    ) const = 0;
};
