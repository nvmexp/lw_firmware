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

#include "repair_module_tpc.h"

#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "core/include/tee.h"
#include "gpu/include/gpudev.h"

using CmdGroup = Repair::Command::Group;

/******************************************************************************/

RC RepairModuleTpc::ValidateSettings
(
    const Repair::Settings& settings,
    const RepairModule::CmdBufProperties& prop
) const
{
    return RC::OK;
}

RC RepairModuleTpc::ExelwteCommandsOnLwrrentGpu
(
    GpuSubdevice *pSubdev,
    const Repair::Settings& settings,
    const CmdBuf& cmdBuf
) const
{
    StickyRC firstRc;
    MASSERT(pSubdev);

    std::unique_ptr<TpcRepair> pTpcRepair = make_unique<TpcRepair>(pSubdev);
    for (const Cmd& cmd : cmdBuf.Get())
    {
        if (cmd.GetType() == CmdT::Unknown)
        {
            Printf(Tee::PriError, "Unknown repair command\n");
            MASSERT(!"Unknown repair command");
            return RC::SOFTWARE_ERROR;
        }

        if (cmd.GetGroup() != CmdGroup::Tpc)
        {
            Printf(Tee::PriError, "Repair command %s not supported by TPC repair module\n",
                                   cmd.ToString().c_str());
            MASSERT(!"Unsupported repair command");
            return RC::SOFTWARE_ERROR;
        }

        Printf(Tee::PriLow, "[Exelwtion] start : %s\n", cmd.ToString().c_str());
        firstRc = cmd.Execute(settings, *pTpcRepair);
        Printf(Tee::PriLow, "[Exelwtion] result: %s\n\n", firstRc.Message());
    }

    return firstRc;
}

RC RepairModuleTpc::ExelwteCommands
(
    const Repair::Settings& settings,
    const CmdBuf& cmdBuf,
    const RepairModule::CmdBufProperties& prop
) const
{
    RC rc;

    if (cmdBuf.Empty())
    {
        Printf(Tee::PriError, "No TPC repair commands to execute\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    Gpu* pGpu = GpuPtr().Instance();
    CHECK_RC(pGpu->Initialize());

    GpuDevMgr* pGpuDevMgr = dynamic_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pGpuDevMgr);
    for (GpuSubdevice* pSubdev = pGpuDevMgr->GetFirstGpu();
         pSubdev != nullptr;
         pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
    {
        CHECK_RC(ExelwteCommandsOnLwrrentGpu(pSubdev, settings, cmdBuf));
    }

    return rc;
}
