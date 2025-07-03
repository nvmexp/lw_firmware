/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_limerock_dev.h"

#include "core/include/platform.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "ctrl_dev_lwswitch.h"
#include "device/interface/i2c.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/include/pcicfg.h"
#include "gpu/ism/lwswitchism.h"
#include "gpu/utility/hulkprocessing/limerockhulkloader.h"
#include "gpu/utility/lwswitchfalcon.h"
#include "lwl_devif.h"
#include "lwl_libif.h"
#include "lwl_topology_mgr_impl.h"

//--------------------------------------------------------------------------
//! \brief Constructor
LwLinkDevIf::LR10Dev::LR10Dev
(
    LibIfPtr pLwSwitchLibif,
    Id i,
    LibInterface::LibDeviceHandle handle
) : LimerockDev(pLwSwitchLibif, i, handle, Device::LR10)
{
    m_pIsm.reset(new LwSwitchIsm(this, LwSwitchIsm::GetTable(Device::LR10)));
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LR10Dev::Shutdown()
{
    m_pIsm.reset();
    return LimerockDev::Shutdown();
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::LimerockDev::HotReset(FundamentalResetOptions funResetOption)
{
    RC rc;
    CHECK_RC(LwLinkDevIf::LwSwitchDev::HotReset(funResetOption));

    // After a reset on Limerock, SOE will eventually halt. Experimentally it takes ~100ms
    // for the halt to register.  If MODS tries to initialize the a limerock device after
    // a hot reset before the halt has registered, the initialization will fail
    //
    // This behavior seems to have been caused by a mandatory VBIOS update, hopefully this
    // delay can be removed in the future
    Tasker::Sleep(150);

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LimerockDev::Initialize()
{
    RC rc;
    CHECK_RC(LwLinkDevIf::LwSwitchDev::Initialize());
    CHECK_RC(GetBjtHotspotOffsets(&m_bjtHotspotOffsets));
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LimerockDev::GetChipTemps(vector<FLOAT32> *pTempsC)
{
    MASSERT(pTempsC);

    UINT32 allChannelMask = 0;
    for (UINT32 i = 0; i < LWSWITCH_NUM_CHANNELS_LR10; i++)
    {
        // Bug 2720088: TDIODE readings are broken
        // Enable it back once the above bug is fixed
        if (HasBug(2720088) && i == LWSWITCH_THERM_CHANNEL_LR10_TDIODE)
        {
            continue;
        }
        allChannelMask |= BIT(i);
    }

    RC rc;
    LWSWITCH_CTRL_GET_TEMPERATURE_PARAMS params = {};
    params.channelMask = allChannelMask;
    CHECK_RC(GetLibIf()->Control(GetLibHandle(),
                                 LibInterface::CONTROL_LWSWITCH_GET_TEMPERATURE,
                                 &params,
                                 sizeof(params)));

    for (UINT32 i = 0; i < LWSWITCH_NUM_CHANNELS_LR10; i++)
    {
        pTempsC->push_back(Utility::FromLwTemp(params.temperature[i]));

        if (i == LWSWITCH_THERM_CHANNEL_LR10_TSENSE_OFFSET_MAX)
        {
            CHECK_RC(GetOverTempCounter().Update(OverTempCounter::TSENSE_OFFSET_MAX_TEMP,
                     static_cast<INT32>((*pTempsC)[i] + 0.5f)));
        }
    }

    vector<ModsGpuRegField> bjtTempFields =
    {
        MODS_THERM_TSENSE_U2_A_0_BJT_0_TEMPERATURE,
        MODS_THERM_TSENSE_U2_A_0_BJT_1_TEMPERATURE,
        MODS_THERM_TSENSE_U2_A_0_BJT_2_TEMPERATURE,
        MODS_THERM_TSENSE_U2_A_0_BJT_3_TEMPERATURE,
        MODS_THERM_TSENSE_U2_A_0_BJT_4_TEMPERATURE,
        MODS_THERM_TSENSE_U2_A_0_BJT_5_TEMPERATURE
    };

    RegHal& regs = GetInterface<LwLinkRegs>()->Regs();
    for (UINT32 bjtIdx = 0; bjtIdx < bjtTempFields.size(); bjtIdx++)
    {
        UINT32 tempFxp = regs.Read32(bjtTempFields[bjtIdx]);
        FLOAT32 tempC = Utility::ColwertFXPToFloat(tempFxp, 9, 5);
        pTempsC->push_back(tempC - m_bjtHotspotOffsets[bjtIdx]);
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */
RC LwLinkDevIf::LimerockDev::GetPdi(UINT64* pPdi)
{
    MASSERT(pPdi);
    RC rc;

    RegHal& regs = GetInterface<LwLinkRegs>()->Regs();
    UINT32 pdiValue;
    CHECK_RC(regs.Read32Priv(MODS_FUSE_OPT_PDI_0, &pdiValue));
    *pPdi = pdiValue;
    CHECK_RC(regs.Read32Priv(MODS_FUSE_OPT_PDI_1, &pdiValue));
    *pPdi |= static_cast<UINT64>(pdiValue) << 32;

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LimerockDev::GetBjtHotspotOffsets(vector<FLOAT32> *pHotspotOffsetsC)
{
    MASSERT(pHotspotOffsetsC);

    vector<ModsGpuRegField> bjtHotspotFields =
    {
        MODS_THERM_TSENSE_U2_A_0_BJT_0_TEMPERATURE_MODIFICATIONS_TEMPERATURE_OFFSET,
        MODS_THERM_TSENSE_U2_A_0_BJT_1_TEMPERATURE_MODIFICATIONS_TEMPERATURE_OFFSET,
        MODS_THERM_TSENSE_U2_A_0_BJT_2_TEMPERATURE_MODIFICATIONS_TEMPERATURE_OFFSET,
        MODS_THERM_TSENSE_U2_A_0_BJT_3_TEMPERATURE_MODIFICATIONS_TEMPERATURE_OFFSET,
        MODS_THERM_TSENSE_U2_A_0_BJT_4_TEMPERATURE_MODIFICATIONS_TEMPERATURE_OFFSET,
        MODS_THERM_TSENSE_U2_A_0_BJT_5_TEMPERATURE_MODIFICATIONS_TEMPERATURE_OFFSET
    };

    RC rc;
    RegHal& regs = GetInterface<LwLinkRegs>()->Regs();
    for (auto bjtHotspotField : bjtHotspotFields)
    {
        UINT32 hotspotFxp = regs.Read32(bjtHotspotField);
        FLOAT32 hotspotC = Utility::ColwertFXPToFloat(hotspotFxp, 9, 5);
        pHotspotOffsetsC->push_back(hotspotC);
    }

    return rc;
}
//-----------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LimerockDev::LoadHulkLicence
(
    const vector<UINT32> &hulkLicence,
    bool ignoreErrors
)
{
    return LimerockHulkLoader(this).LoadHulkLicence(hulkLicence, ignoreErrors);
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LimerockDev::PexReset()
{
    RC rc;
    unique_ptr<PciCfgSpace> pGpuCfgSpace(new PciCfgSpace());

    UINT16 domain;
    UINT16 bus;
    UINT16 dev;
    UINT16 func;
    GetId().GetPciDBDF(&domain, &bus, &dev, &func);

    Printf(Tee::PriNormal, "Preparing to pex reset %04x:%02x:%02x.%02x\n",
                            domain, bus, dev, func);

    CHECK_RC(pGpuCfgSpace->Initialize(domain, bus, dev, func));
    Printf(Tee::PriNormal, "Saving config space...\n");
    CHECK_RC(pGpuCfgSpace->Save());

    // From syseng/Projects/E4700/FW/FPGA/I2C_Access_Guide/E4700_FPGA_I2C_Request_Form.xlsx
    // PEX reset is triggered by register writes to the FPGA
    static constexpr UINT32 i2cPort       = 0x0;
    static constexpr UINT32 i2cAddr       = 0x12;
    static constexpr UINT32 pexResetReg   = 0x3B;
    static constexpr UINT32 pexTriggerBit = 2;

    I2c::Dev i2cDev = GetInterface<I2c>()->I2cDev(i2cPort, i2cAddr);
    i2cDev.SetFlavor(I2c::FLAVOR_HW);
    i2cDev.Write(pexResetReg, BIT(pexTriggerBit));

    // Wait for 150ms (value based on implementation of hot reset)
    Tasker::Sleep(150);

    CHECK_RC(pGpuCfgSpace->Restore());
    Printf(Tee::PriNormal, "Pex reset was successful\n");

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LimerockDev::DoStartThermalSlowdown(UINT32 periodUs)
{
    RC rc;

    DEFER
    {
        Platform::EnableInterrupts();
    };
    Platform::DisableInterrupts();

    LWSWITCH_CTRL_SET_THERMAL_SLOWDOWN params = {};
    params.slowdown = true;
    params.periodUs = periodUs;
    CHECK_RC(GetLibIf()->Control(GetLibHandle(),
                                 LibInterface::CONTROL_LWSWITCH_SET_THERMAL_SLOWDOWN,
                                 &params,
                                 sizeof(params)));

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LimerockDev::DoStopThermalSlowdown()
{
    RC rc;

    DEFER
    {
        Platform::EnableInterrupts();
    };
    Platform::DisableInterrupts();

    LWSWITCH_CTRL_SET_THERMAL_SLOWDOWN params = {};
    params.slowdown = false;
    params.periodUs = 1;
    CHECK_RC(GetLibIf()->Control(GetLibHandle(),
                                 LibInterface::CONTROL_LWSWITCH_SET_THERMAL_SLOWDOWN,
                                 &params,
                                 sizeof(params)));

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LimerockDev::GetIsDevidHulkRevoked(bool *pIsRevoked) const
{
    MASSERT(pIsRevoked);

    const RegHal& regs = GetInterface<LwLinkRegs>()->Regs();
    // LW_FUSE_OPT_FPF_FIELD_SPARE_FUSES_0[4:4] is used for DEVID based HULK revocation
    // See https://confluence.lwpu.com/display/GFS/Spare+FPF+Assignment
    UINT32 reg = regs.Read32(MODS_FUSE_OPT_FPF_FIELD_SPARE_FUSES_0);
    *pIsRevoked = (reg >> 4) & 0x1;

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LimerockDev::GetFpfGfwRev(UINT32 *pGfwRev) const
{
    MASSERT(pGfwRev);
    const RegHal& regs = GetInterface<LwLinkRegs>()->Regs();
    *pGfwRev = regs.Read32(MODS_FUSE_OPT_FPF_UCODE_GFW_REV);
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LimerockDev::GetFpfRomFlashRev(UINT32 *pRomFlashRev) const
{
    MASSERT(pRomFlashRev);
    const RegHal& regs = GetInterface<LwLinkRegs>()->Regs();
    *pRomFlashRev = regs.Read32(MODS_FUSE_OPT_FPF_UCODE_ROM_FLASH_REV);
    return RC::OK;
}
