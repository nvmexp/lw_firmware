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

#include "repair_module_hbm.h"

#include "gpu/repair/hbm/hbm_interface/hbm_interface.h"
#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_bdie_interface.h"
#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_ga100_interface.h"
#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_gv100_interface.h"
#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2e_ga100_interface.h"
#include "gpu/repair/hbm/hbm_interface/vendor/skhynix/skhynix_hbm_interface.h"
#include "gpu/repair/hbm/hbm_interface/vendor/skhynix/skhynix_hbm2e_interface.h"
#include "gpu/repair/hbm/hbm_interface/vendor/micron/micron_hbm2e_interface.h"
#include "gpu/repair/hbm/gpu_interface/ampere_hbm_interface.h"
#include "gpu/repair/hbm/gpu_interface/volta_hbm_interface.h"
#include "gpu/repair/mem_repair_config.h"

// TODO(aperiwal): remove dependency on the following:
#include "gpu/include/hbmimpl.h"

using namespace Memory;

using CmdGroup = Repair::Command::Group;

/******************************************************************************/

RC RepairModuleHbm::CreateGpuHbmInterface
(
    const Repair::Settings& settings,
    const SystemState& sysState,
    std::unique_ptr<GpuHbmInterface>* ppGpuHbmInterface
) const
{
    MASSERT(ppGpuHbmInterface);
    ppGpuHbmInterface->release();

    using namespace Memory::Hbm;

    using GpuFamily = GpuDevice::GpuFamily;
    GpuSubdevice* pSubdev = sysState.lwrrentGpu.pSubdev;

    switch (pSubdev->GetParentDevice()->GetFamily())
    {
        case GpuFamily::Volta:
            if (settings.modifiers.forceHtoJ)
            {
                Printf(Tee::PriError, "No Host-to-JTAG interface supported for "
                       "Volta.\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            *ppGpuHbmInterface = std::make_unique<VoltaHbmInterface>(pSubdev);
            break;

        case GpuFamily::Ampere:
            if (settings.modifiers.forceHtoJ)
            {
                Printf(Tee::PriError, "No Host-to-JTAG interface supported for "
                       "Ampere.\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            *ppGpuHbmInterface = std::make_unique<AmpereHbmInterface>(pSubdev);
            break;

        default:
            Printf(Tee::PriError, "Unsupported GPU\n");
            MASSERT(!"Unsupported GPU");
            return RC::UNSUPPORTED_FUNCTION;
    }

    (*ppGpuHbmInterface)->SetSettings(settings);

    return RC::OK;
}

RC RepairModuleHbm::CreateHbmInterface
(
    const Repair::Settings& settings,
    const SystemState& sysState,
    const Hbm::Model& hbmModel,
    std::unique_ptr<HbmInterface>* ppHbmInterface
) const
{
    MASSERT(ppHbmInterface);
    RC rc;

    // Get the GPU-to-HBM and the HBM interface
    std::unique_ptr<Hbm::GpuHbmInterface> pGpuInterface;
    CHECK_RC(CreateGpuHbmInterface(settings, sysState, &pGpuInterface));

    using GpuFamily = GpuDevice::GpuFamily;
    GpuFamily gpuFamily = sysState.lwrrentGpu.pSubdev->GetParentDevice()->GetFamily();

    switch (hbmModel.vendor)
    {
        case Hbm::Vendor::Samsung:
            switch (hbmModel.spec)
            {
                case Hbm::SpecVersion::V2:
                    switch (hbmModel.die)
                    {
                        case Hbm::Die::B:
                            *ppHbmInterface = std::make_unique<Hbm::SamsungHbm2BDie>(
                                hbmModel,
                                std::move(pGpuInterface));
                            break;

                        case Hbm::Die::X:
                            switch (gpuFamily)
                            {
                                case GpuFamily::Volta:
                                    *ppHbmInterface = std::make_unique<Hbm::SamsungHbm2XDieGV100>(
                                        hbmModel,
                                        std::move(pGpuInterface));
                                    break;

                                case GpuFamily::Ampere:
                                    *ppHbmInterface = std::make_unique<Hbm::SamsungHbm2XDieGA100>(
                                        hbmModel,
                                        std::move(pGpuInterface));
                                    break;

                                default:
                                    Printf(Tee::PriError, "Unsupported GPU\n");
                                    MASSERT(!"Unsupported GPU");
                                    return RC::UNSUPPORTED_FUNCTION;
                            }
                            break;

                        default:
                            Printf(Tee::PriError, "Unsupported HBM die: %s\n",
                                   ToString(hbmModel.die).c_str());
                            MASSERT(!"Unsupported HBM die");
                            return RC::UNSUPPORTED_FUNCTION;
                    }
                    break;

                case Hbm::SpecVersion::V2e:
                    *ppHbmInterface = std::make_unique<Hbm::SamsungHbm2eGA100>(
                                      hbmModel,
                                      std::move(pGpuInterface));
                    break;

                default:
                    Printf(Tee::PriError, "Unsupported HBM specification: %s\n",
                           ToString(hbmModel.spec).c_str());
                    MASSERT(!"Unsupported HBM specification");
                    return RC::UNSUPPORTED_FUNCTION;
            }
            break;

        case Hbm::Vendor::SkHynix:
            switch (hbmModel.spec)
            {
                case Hbm::SpecVersion::V2:
                    *ppHbmInterface = std::make_unique<Hbm::SkHynixHbm2>(
                        hbmModel,
                        std::move(pGpuInterface));
                    break;

                case Hbm::SpecVersion::V2e:
                    *ppHbmInterface = std::make_unique<Hbm::SkHynixHbm2e>(
                        hbmModel,
                        std::move(pGpuInterface));
                    break;

                default:
                    Printf(Tee::PriError, "Unsupported HBM specification: %s\n",
                           ToString(hbmModel.spec).c_str());
                    MASSERT(!"Unsupported HBM specification");
                    return RC::UNSUPPORTED_FUNCTION;
            }
            break;
            
        case Hbm::Vendor::Micron:
            switch (hbmModel.spec)
            {
                case Hbm::SpecVersion::V2e:
                    *ppHbmInterface = std::make_unique<Hbm::MicronHbm2e>(
                        hbmModel,
                        std::move(pGpuInterface));
                    break;

                default:
                    Printf(Tee::PriError, "Unsupported HBM specification: %s\n",
                           ToString(hbmModel.spec).c_str());
                    MASSERT(!"Unsupported HBM specification");
                    return RC::UNSUPPORTED_FUNCTION;
            }
            break;

        default:
            Printf(Tee::PriError, "Unsupported HBM vendor: %s\n",
                   ToString(hbmModel.vendor).c_str());
            MASSERT(!"Unsupported HBM vendor");
            return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

RC RepairModuleHbm::SetupHbmRepairOnLwrrentGpu
(
    const Repair::Settings& settings,
    const SystemState& sysState,
    std::unique_ptr<HbmInterface>* ppHbmInterface
) const
{
    MASSERT(ppHbmInterface);
    RC rc;

    GpuSubdevice *pSubdev = sysState.lwrrentGpu.pSubdev;
    RawEcid rawEcid;
    CHECK_RC(pSubdev->GetRawEcid(&rawEcid));
    auto hbmModelItr = sysState.hbmModels.find(rawEcid);
    if (hbmModelItr == sysState.hbmModels.end())
    {
        Printf(Tee::PriError, "HBM model not found\n");
        return RC::SOFTWARE_ERROR;
    }

    Hbm::Model hbmModel = hbmModelItr->second;
    CHECK_RC(CreateHbmInterface(settings, sysState, hbmModel, ppHbmInterface));
    CHECK_RC((*ppHbmInterface)->Setup());

    return rc;
}

RC RepairModuleHbm::ExelwteHbmCommandsOnLwrrentGpu
(
    const Repair::Settings& settings,
    SystemState* pSysState,
    const CmdBuf& cmdBuf,
    std::unique_ptr<HbmInterface>* ppHbmInterface
) const
{
    MASSERT(pSysState);
    StickyRC firstRc;
    GpuSubdevice* pLwrSubdev = pSysState->lwrrentGpu.pSubdev;

    if (!cmdBuf.HasCommand(CmdT::ReconfigGpuLanes))
    {
        pLwrSubdev->GetHBMImpl()->SetHBMResetDelayMs(
            settings.modifiers.hbmResetDurationMs);
    }

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
            Printf(Tee::PriError, "Repair command %s not supported by HBM repair module\n",
                                   cmd.ToString().c_str());
            MASSERT(!"Unsupported repair command");
            return RC::SOFTWARE_ERROR;
        }

        Printf(Tee::PriLow, "[Exelwtion] start : %s\n", cmd.ToString().c_str());
        firstRc = cmd.Execute(settings, pSysState, ppHbmInterface);
        Printf(Tee::PriLow, "[Exelwtion] result: %s\n\n", firstRc.Message());
    }

    return firstRc;
}

RC RepairModuleHbm::ExelwteCommandsOnLwrrentGpu
(
    const Repair::Settings& settings,
    SystemState* pSysState,
    const CmdBuf& cmdBuf
) const
{
    RC rc;

    GpuSubdevice* pSubdev = pSysState->lwrrentGpu.pSubdev;
    if (pSubdev->GetNumHbmSites() == 0)
    {
        Printf(Tee::PriError, "GPU not compatible with HBM repair module\n");
        return RC::SOFTWARE_ERROR;
    }

    std::unique_ptr<HbmInterface> pHbmInterface;
    CHECK_RC(SetupHbmRepairOnLwrrentGpu(settings, *pSysState, &pHbmInterface));
    CHECK_RC(ExelwteHbmCommandsOnLwrrentGpu(settings, pSysState, cmdBuf, &pHbmInterface));

    return rc;
}
