/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_laguna_dev.h"
#include "core/include/platform.h"

#include "ctrl/ctrl2080.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "ctrl_dev_lwswitch.h"
#include "lwswitch/ls10/ptop_discovery_ip.h"

namespace
{
    constexpr UINT32 BASE_XTL = LW_PTOP_UNICAST_SW_DEVICE_BASE_XTL_0;

    // Need to hack the register read function directly
    // because the LwLink interface hasn't been initialized yet
    UINT32 XtlRead32(void* pBar0, UINT32 offset)
    {
        return MEM_RD32((const volatile void*)((UINT08*)pBar0 + BASE_XTL + offset));
    };
    void XtlWrite32(void* pBar0, UINT32 offset, UINT32 data)
    {
        MEM_WR32((volatile void*)((UINT08*)pBar0 + BASE_XTL + offset), data);
    };
};

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LagunaDev::GetChipTemps(vector<FLOAT32> *pTempsC)
{
    MASSERT(pTempsC);

    UINT32 allChannelMask = (1 << LWSWITCH_NUM_CHANNELS_LS10) - 1;

    RC rc;
    LWSWITCH_CTRL_GET_TEMPERATURE_PARAMS params = {};
    params.channelMask = allChannelMask;
    CHECK_RC(GetLibIf()->Control(GetLibHandle(),
                                 LibInterface::CONTROL_LWSWITCH_GET_TEMPERATURE,
                                 &params,
                                 sizeof(params)));

    for (UINT32 i = 0; i < LWSWITCH_NUM_CHANNELS_LS10; i++)
    {
        pTempsC->push_back(Utility::FromLwTemp(params.temperature[i]));

        if (i == LWSWITCH_THERM_CHANNEL_LS10_TSENSE_OFFSET_MAX)
        {
            CHECK_RC(GetOverTempCounter().Update(OverTempCounter::TSENSE_OFFSET_MAX_TEMP,
                     static_cast<INT32>((*pTempsC)[i] + 0.5f)));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LagunaDev::HotReset(FundamentalResetOptions resetOption)
{
    StickyRC rc;
    UINT32 startingDevId = GetInterface<Pcie>()->DeviceId();
    RegHal& regs = Regs();

    unique_ptr<PciCfgSpace> pCfgSpace(new PciCfgSpace());

    UINT16 domain = 0;
    UINT16 bus = 0;
    UINT16 dev = 0;
    UINT16 func = 0;
    GetId().GetPciDBDF(&domain, &bus, &dev, &func);

    void* pBar0 = nullptr;
    CHECK_RC(GetLibIf()->GetBarInfo(GetLibHandle(), 0, nullptr, nullptr, &pBar0));

    // determine whether fundamental and hot reset are coupled
    bool fundamentalResetCoupled = false;
    if (regs.IsSupported(MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET))
    {
        UINT32 value = XtlRead32(pBar0, regs.LookupAddress(MODS_XTL_EP_PRI_RESET));

        fundamentalResetCoupled = regs.TestField(value, MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_RESET);
    }

    // determine whether we want to trigger a fundamental reset
    bool fundamentalReset = false;
    switch (resetOption)
    {
        case FundamentalResetOptions::Default:
            fundamentalReset = fundamentalResetCoupled;
            break;
        case FundamentalResetOptions::Enable:
            fundamentalReset = true;
            break;
        case FundamentalResetOptions::Disable:
            fundamentalReset = false;
            break;
        default:
            Printf(Tee::PriError, "Unknown fundamental reset option: %d\n",
                   static_cast<UINT32>(resetOption));
            return RC::ILWALID_ARGUMENT;
    }
    string resetString = fundamentalReset ? "fundamental" : "hot";

    Printf(Tee::PriNormal, "Preparing to %s reset %04x:%02x:%02x.%02x\n",
           resetString.c_str(), domain, bus, dev, func);

    CHECK_RC(pCfgSpace->Initialize(domain, bus, dev, func));
    Printf(Tee::PriNormal, "Saving config space...\n");
    CHECK_RC(pCfgSpace->Save());

    // optionally override whether fundamental reset gets triggered. doing this after pci cfg space
    // is saved so that the original value is restored.
    if (fundamentalReset != fundamentalResetCoupled)
    {
        if (!regs.IsSupported(MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET))
        {
            Printf(Tee::PriError, "Unable to trigger %s reset.\n", resetString.c_str());
            return RC::UNSUPPORTED_FUNCTION;
        }

        if (regs.IsSupported(MODS_XTL_EP_PRI_RESET_CTRL_PRIV_LEVEL_MASK))
        {
            UINT32 plmValue = XtlRead32(pBar0, regs.LookupAddress(MODS_XTL_EP_PRI_RESET_CTRL_PRIV_LEVEL_MASK));

            if (!regs.TestField(plmValue, MODS_XTL_EP_PRI_RESET_CTRL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED) ||
                !regs.TestField(plmValue, MODS_XTL_EP_PRI_RESET_CTRL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED))
            {
                Printf(Tee::PriError, "Unable to trigger %s reset due to priv mask.\n",
                       resetString.c_str());
                return RC::UNSUPPORTED_FUNCTION;
            }
        }

        UINT32 value = XtlRead32(pBar0, regs.LookupAddress(MODS_XTL_EP_PRI_RESET));

        regs.SetField(&value, fundamentalReset ?
                              MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_RESET :
                              MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_NORESET);

        XtlWrite32(pBar0, regs.LookupAddress(MODS_XTL_EP_PRI_RESET), value);
    }

    PexDevice* bridge;
    UINT32 parentPort;
    CHECK_RC(GetInterface<Pcie>()->GetUpStreamInfo(&bridge, &parentPort));

    // disable pcie hotplug interrupts. otherwise the OS could get notified and interfere
    bool hotPlugEnabled = false;
    CHECK_RC(bridge->GetDownstreamPortHotplugEnabled(parentPort, &hotPlugEnabled));
    if (hotPlugEnabled)
    {
        CHECK_RC(bridge->SetDownstreamPortHotplugEnabled(parentPort, false));
    }

    DEFER
    {
        if (hotPlugEnabled)
        {
            bridge->SetDownstreamPortHotplugEnabled(parentPort, true);
        }
    };

    bridge->ResetDownstreamPort(parentPort);

    // Wait 200ms for SOE boot to run after issuing hot reset
    Tasker::Sleep(200);

    rc = Tasker::PollHw(1000, [this, startingDevId]()->bool
        {
            // Read the device ID and see if it matches the expected one.
            UINT32 deviceId = GetInterface<Pcie>()->DeviceId();

            return startingDevId == deviceId;
        },
        __FUNCTION__);

    if (rc != OK)
    {
        // We shouldn't return here. We should at least *try* to
        // restore the PCIE config space(s) so we don't force the poor
        // user to reboot their system.
        rc.Clear();
        rc = RC::PCI_HOT_RESET_FAIL;
    }

    CHECK_RC(pCfgSpace->Restore());

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "%s reset was successful\n", resetString.c_str());
    }

    Tasker::Sleep(150);

    return rc;
}