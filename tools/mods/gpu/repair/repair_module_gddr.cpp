/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/framebuf.h"
#include "gpu/repair/gddr/gddr_interface/vendor/samsung/samsung_gddr6_interface.h"
#include "gpu/repair/gddr/gddr_interface/vendor/samsung/samsung_gddr6_ga10x_interface.h"

#include "repair_module_gddr.h"

using namespace Memory;

using CmdGroup = Repair::Command::Group;

using RamVendorId   = FrameBuffer::RamVendorId;
using RamProtocol   = FrameBuffer::RamProtocol;

/******************************************************************************/
RC RepairModuleGddr::CreateGddrInterface
(
    const Repair::Settings& settings,
    const SystemState& sysState,
    const Gddr::Model &gddrModel,
    std::unique_ptr<GddrInterface>* ppGddrInterface
) const
{
    MASSERT(ppGddrInterface);
    RC rc;

    GpuSubdevice *pSubdev = sysState.lwrrentGpu.pSubdev;
    MASSERT(pSubdev);

    using GpuFamily = GpuDevice::GpuFamily;
    switch (gddrModel.vendorId)
    {
        case RamVendorId::RamSAMSUNG:
            switch (gddrModel.ramProtocol)
            {
                case RamProtocol::RamGDDR6:
                    switch (pSubdev->GetParentDevice()->GetFamily())
                    {
                        case GpuFamily::Turing:
                            *ppGddrInterface = std::make_unique<Gddr::SamsungGddr6>(
                                gddrModel,
                                pSubdev);
                        break;

                        case GpuFamily::Ampere:
                        case GpuFamily::Ada:
                            *ppGddrInterface = std::make_unique<Gddr::SamsungGddr6GA10x>(
                                gddrModel,
                                pSubdev);
                        break;

                        default:
                            Printf(Tee::PriError, "Unsupported chip\n");
                            MASSERT(!"Unsupported chip");
                            return RC::UNSUPPORTED_FUNCTION;
                    }
                    break;

                default:
                    Printf(Tee::PriError, "Unsupported RAM protocol: %s\n",
                                           gddrModel.protocolName.c_str());
                    MASSERT(!"Unsupported RAM protocol string");
                    return RC::UNSUPPORTED_FUNCTION;
            }
            break;

        default:
            Printf(Tee::PriError, "Unsupported vendor: %s\n",
                                   gddrModel.vendorName.c_str());
            MASSERT(!"Unsupported vendor");
            return RC::UNSUPPORTED_FUNCTION;
    }

    (*ppGddrInterface)->SetSettings(settings);

    return rc;
}

RC RepairModuleGddr::SetupGddrRepairOnLwrrentGpu
(
    const Repair::Settings& settings,
    const SystemState& sysState,
    std::unique_ptr<GddrInterface>* ppGddrInterface
) const
{
    MASSERT(ppGddrInterface);
    RC rc;

    GpuSubdevice *pSubdev = sysState.lwrrentGpu.pSubdev;
    RawEcid rawEcid;
    CHECK_RC(pSubdev->GetRawEcid(&rawEcid));
    auto gddrModelItr = sysState.gddrModels.find(rawEcid);
    if (gddrModelItr == sysState.gddrModels.end())
    {
        Printf(Tee::PriError, "GDDR model not found\n");
        return RC::SOFTWARE_ERROR;
    }

    Gddr::Model gddrModel = gddrModelItr->second;
    CHECK_RC(CreateGddrInterface(settings, sysState, gddrModel, ppGddrInterface));
    CHECK_RC((*ppGddrInterface)->Setup());

    return rc;
}

RC RepairModuleGddr::ExelwteGddrCommandsOnLwrrentGpu
(
    const Repair::Settings& settings,
    SystemState* pSysState,
    const CmdBuf& cmdBuf,
    std::unique_ptr<GddrInterface>* ppGddrInterface
) const
{
    MASSERT(pSysState);
    StickyRC firstRc;

    for (const Cmd& cmd : cmdBuf.Get())
    {
        if (cmd.GetType() == CmdT::Unknown)
        {
            Printf(Tee::PriError, "Unknown repair command\n");
            MASSERT(!"Unknown repair command");
            return RC::SOFTWARE_ERROR;
        }

        if (cmd.GetGroup() != CmdGroup::Mem)
        {
            Printf(Tee::PriError, "Repair command %s not supported by mem repair module\n",
                                   cmd.ToString().c_str());
            MASSERT(!"Unsupported repair command");
            return RC::SOFTWARE_ERROR;
        }

        Printf(Tee::PriLow, "[Exelwtion] start : %s\n", cmd.ToString().c_str());
        firstRc = cmd.Execute(settings, pSysState, ppGddrInterface);
        Printf(Tee::PriLow, "[Exelwtion] result: %s\n\n", firstRc.Message());
    }

    return firstRc;
}

RC RepairModuleGddr::ExelwteCommandsOnLwrrentGpu
(
    const Repair::Settings& settings,
    SystemState* pSysState,
    const CmdBuf& cmdBuf
) const
{
    RC rc;

    if (pSysState->lwrrentGpu.pSubdev->GetNumHbmSites() != 0)
    {
        Printf(Tee::PriError, "GPU not compatible with GDDRx repair module\n");
        return RC::SOFTWARE_ERROR;
    }

    std::unique_ptr<GddrInterface> pGddrInterface;
    CHECK_RC(SetupGddrRepairOnLwrrentGpu(settings, *pSysState, &pGddrInterface));
    CHECK_RC(ExelwteGddrCommandsOnLwrrentGpu(settings, pSysState, cmdBuf, &pGddrInterface));

    return rc;
}
