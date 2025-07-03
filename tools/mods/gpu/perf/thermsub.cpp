/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080thermal.h"
#include "device/interface/i2c.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/thermsub.h"
#include "gpu/utility/bglogger.h"
#include "lwtypes.h"
#include <cmath>

// Dummy defines so we can include thrmtbl.h from resman directly
// instead of doing a copy of defines
#define POBJGPU void*
#define POBJVBIOS void*
#define POBJTHERM void*
#define BOOL bool
#include "gpu/therm/therm_table.h"

#if !defined(LW_WINDOWS) && !defined(LW_MACINTOSH) || defined(WIN_MFG)
#include "device/smb/smbdev.h"
#include "device/smb/smbmgr.h"
#include "device/smb/smbport.h"
#else
#include "stub/smb_stub.h"
#endif

#ifndef VARIADIC_MACROS_NOT_SUPPORTED
#include "cheetah/dev/fuse/tests/skutest.h"
#include "cheetah/dev/soctherm/socthermctrl.h"
#include "cheetah/dev/soctherm/socthermctrlmgr.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#endif

struct HWfsEventInfo
{
    UINT32 eventId;
    Thermal::HwfsEventCategory category;
};

#if defined(TEGRA_MODS)
// Thermal event id macros are not available when building Android MODS
//
static const HWfsEventInfo s_ThermalSlowdownAlerts[] = {{~0U, Thermal::HwfsEventCategory::UNKNOWN}};
#else
// All thermal events that can cause a thermal slowdown, which has the
// side-effect of making the RM send a notifier to mods.
//
static const HWfsEventInfo s_ThermalSlowdownAlerts[] =
{
     { LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_OVERT,         Thermal::HwfsEventCategory::EXT_GPIO   }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT,         Thermal::HwfsEventCategory::EXT_GPIO   }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_POWER,         Thermal::HwfsEventCategory::EXT_GPIO   }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_0H,          Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_1H,          Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_2H,          Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_3H,          Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_4H,          Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_NEG1H,       Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_0,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_1,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_2,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_3,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_4,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_5,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_6,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_7,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_8,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_9,         Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_10,        Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_11,        Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_DEDICATED_OVERT,   Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_SCI_FS_OVERT,      Thermal::HwfsEventCategory::TEMP_BASED }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT_0,       Thermal::HwfsEventCategory::EXT_GPIO   }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT_1,       Thermal::HwfsEventCategory::EXT_GPIO   }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_VOLTAGE_HW_ADC,    Thermal::HwfsEventCategory::EDPP       }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EDPP_VMIN,         Thermal::HwfsEventCategory::EDPP       }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EDPP_FONLY,        Thermal::HwfsEventCategory::EDPP       }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_BA_BA_W2_T1H,      Thermal::HwfsEventCategory::BA         }
};

// TODO To be fixed at lwbug http://lwbugs/200653224/23
//ct_assert(LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID__COUNT == LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT_1 + 1);
#endif

// Colwert LwTemp to Celsius without losing data if LwTemp is
// non-integral.  (There are colwersion macros in lwtypes.h, but they
// assume integer temperatures.)  The extra precision is mostly an
// issue for RestoreInitialThermalLimit(), to make sure mods restores
// the original data.
//
static inline FLOAT32 FromLwTemp(LwTemp temp)
{
    return static_cast<FLOAT32>(temp / 256.0);
}

// Colwert possibly-fractional Celsius to LwTemp such that
// ToLwTemp(FromLwTemp(x)) == x.
//
static inline LwTemp ToLwTemp(FLOAT32 temp)
{
    return static_cast<LwTemp>(floor(temp * 256.0 + 0.5));
}

// Colwert FXP_9_5 to real world Celsius
// While accessing BJT FXP9_5 temp, the sign bit should be considered
// This is especially handy if the temperature is sub zero
static inline FLOAT32 FromFX9P5ToCelsius(UINT32 fxpTemp)
{
    return static_cast<FLOAT32>(Utility::ColwertFXPToFloat(fxpTemp, 9, 5));
}

//! If the last volterra temperature reading was more than
//! (VOLTERRA_RESET_LAST_TEMP * BgLogger::GetReadIntervalMs() in the past then
//! recallwlate the last valid reading
#define VOLTERRA_RESET_LAST_TEMP    3

#define VOLTERRA_INIT_TEMP_RETRIES  10 //!< Maximum number of retries when
                                       //!< initializing temperatures
#define VOLTERRA_INIT_TEMP_DIFF  4 //!< Maximum temperature seperation for
                                   //!< first readings
#define VOLTERRA_INIT_TEMP_COUNT 2 //!< Number of valid conselwtive readings
                                   //!< required at initialization
#define VOLTERRA_GET_TEMP_RETRIES   5 //!< Maximum number of retries when
                                      //!< getting a volterra temperature

namespace
{
//--------------------------------------------------------------------
//! \brief Read a sysfs file containing a temperature in millicelsius
//!
RC ReadMilliC
(
    const char *filename,
    FLOAT32 *pTemp
)
{
    MASSERT(Platform::IsTegra());

    int tempInMillicelsius = 0;
    if (OK == Xp::InteractiveFileRead(filename, &tempInMillicelsius))
    {
        *pTemp = tempInMillicelsius / 1000.0f;
    }
    else
    {
        *pTemp = -273;
    }

    return OK;
}

struct CodeToStr
{
    UINT32 Code;
    const char * Str;
};

const char * CodeToStrLookup
(
    UINT32 code,
    const CodeToStr * table,
    const CodeToStr * tableEnd
)
{
    for (const CodeToStr * p = table; p < tableEnd; p++)
    {
        if (p->Code == code)
            return p->Str;
    }
    return "INVALID";
}

#define CODE_LOOKUP(c, table) CodeToStrLookup(c, table, table + NUMELEMS(table))

const CodeToStr DEVICE_CLASS_ToStr[] =
{
    { LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_ILWALID,            ENCJSENT("INVALID") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU,               ENCJSENT("GPU") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC,     ENCJSENT("GPU_GPC_TSOSC") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_SCI,           ENCJSENT("GPU_SCI") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_COMBINED,  ENCJSENT("GPC_COMBINED") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2CS_GT21X,        ENCJSENT("I2CS_GT21X") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2CS_GF11X,        ENCJSENT("I2CS_GF11X") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7473_1,     ENCJSENT("I2C_ADT7473_1") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADM1032,       ENCJSENT("I2C_ADM1032") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_VT1165,        ENCJSENT("I2C_VT1165") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_MAX6649,       ENCJSENT("I2C_MAX6649") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP411,        ENCJSENT("I2C_TMP411") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7461,       ENCJSENT("I2C_ADT7461") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP451,        ENCJSENT("I2C_TMP451") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_RM,                ENCJSENT("RM") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE,         ENCJSENT("HBM2_SITE") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_COMBINED,     ENCJSENT("HBM2_COMBINED") }
    ,{ LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GDDR6_X_COMBINED,  ENCJSENT("GDDR6_X_COMBINED") }
};

const CodeToStr CHANNEL_TYPE_ToStr[] =
{
    { LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_GPU_AVG,            ENCJSENT("GPU_AVG") }
    ,{ LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_GPU_MAX,           ENCJSENT("GPU_MAX") }
    ,{ LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_BOARD,             ENCJSENT("BOARD") }
    ,{ LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_MEMORY,            ENCJSENT("MEMORY") }
    ,{ LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_PWR_SUPPLY,        ENCJSENT("PWR_SUPPLY") }
    ,{ LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_MAX_COUNT,         ENCJSENT("MAX_COUNT") }
};

const CodeToStr THERMAL_CAP_POLICY_INDEX_ToStr[] =
{
    { Thermal::GPS,            ENCJSENT("GPS") }
    ,{ Thermal::Acoustic,      ENCJSENT("Acoustic") }
    ,{ Thermal::MEMORY,        ENCJSENT("MEMORY") }
};

const CodeToStr HWFS_EVT_TYPE_ToStr[] =
{
    { LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_OVERT,           ENCJSENT("EVENT_ID_EXT_OVERT") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT,          ENCJSENT("EVENT_ID_EXT_ALERT") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_POWER,          ENCJSENT("EVENT_ID_EXT_POWER") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_OVERT,              ENCJSENT("EVENT_ID_OVERT") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_0H,           ENCJSENT("EVENT_ID_ALERT_0H") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_1H,           ENCJSENT("EVENT_ID_ALERT_1H") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_2H,           ENCJSENT("EVENT_ID_ALERT_2H") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_3H,           ENCJSENT("EVENT_ID_ALERT_3H") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_4H,           ENCJSENT("EVENT_ID_ALERT_4H") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_NEG1H,        ENCJSENT("EVENT_ID_ALERT_NEG1H") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_0,          ENCJSENT("EVENT_ID_THERMAL_0") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_1,          ENCJSENT("EVENT_ID_THERMAL_1") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_2,          ENCJSENT("EVENT_ID_THERMAL_2") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_3,          ENCJSENT("EVENT_ID_THERMAL_3") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_4,          ENCJSENT("EVENT_ID_THERMAL_4") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_5,          ENCJSENT("EVENT_ID_THERMAL_5") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_6,          ENCJSENT("EVENT_ID_THERMAL_6") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_7,          ENCJSENT("EVENT_ID_THERMAL_7") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_8,          ENCJSENT("EVENT_ID_THERMAL_8") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_9,          ENCJSENT("EVENT_ID_THERMAL_9") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_10,         ENCJSENT("EVENT_ID_THERMAL_10") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_11,         ENCJSENT("EVENT_ID_THERMAL_11") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_DEDICATED_OVERT,    ENCJSENT("EVENT_ID_DEDICATED_OVERT") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_SCI_FS_OVERT,       ENCJSENT("EVENT_ID_SCI_FS_OVERT") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT_0,        ENCJSENT("EVENT_ID_EXT_ALERT_0") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT_1,        ENCJSENT("EVENT_ID_EXT_ALERT_1") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_VOLTAGE_HW_ADC,     ENCJSENT("EVENT_ID_VOLTAGE_HW_ADC") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EDPP_VMIN,          ENCJSENT("EVENT_ID_EDPP_VMIN") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EDPP_FONLY,         ENCJSENT("EVENT_ID_EDPP_FONLY") }
    ,{ LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_BA_BA_W2_T1H,       ENCJSENT("EVENT_ID_BA_BA_W2_T1") }
};

const CodeToStr HwfsEventCategory_ToStr[] =
{
     { static_cast<UINT32>(Thermal::HwfsEventCategory::EXT_GPIO),   ENCJSENT("External GPIO based") }
    ,{ static_cast<UINT32>(Thermal::HwfsEventCategory::TEMP_BASED), ENCJSENT("Temp based") }
    ,{ static_cast<UINT32>(Thermal::HwfsEventCategory::BA),         ENCJSENT("BA") }
    ,{ static_cast<UINT32>(Thermal::HwfsEventCategory::EDPP),       ENCJSENT("EDPP") }
    ,{ static_cast<UINT32>(Thermal::HwfsEventCategory::UNKNOWN),    ENCJSENT("Unknown") }
};

Thermal::HwfsEventCategory GetHwfsEventCategory
(
    UINT32 eventId
)
{
    for (auto itr : s_ThermalSlowdownAlerts)
    {
        if (itr.eventId == eventId)
        {
            return itr.category;
        }
    }
    return Thermal::HwfsEventCategory::UNKNOWN;
}
} // End anonymous namespace

// -----------------------------------------------------------------------------
//  ThermalSensors
// -----------------------------------------------------------------------------

ThermalSensors::ThermalSensors(GpuSubdevice *sbDev)
  : m_pSubdev(sbDev)
{
}

ThermalSensors::~ThermalSensors()
{
}

map<UINT32, ThermalSensors::Sensor> ThermalSensors::CreateLw2080TargetTypeToThermalSensorsMap()
{
    map<UINT32, ThermalSensors::Sensor> translationMap;
    translationMap[LW2080_CTRL_THERMAL_SYSTEM_TARGET_GPU] =          ThermalSensors::GPU;
    translationMap[LW2080_CTRL_THERMAL_SYSTEM_TARGET_MEMORY] =       ThermalSensors::MEMORY;
    translationMap[LW2080_CTRL_THERMAL_SYSTEM_TARGET_POWER_SUPPLY] = ThermalSensors::POWER_SUPPLY;
    translationMap[LW2080_CTRL_THERMAL_SYSTEM_TARGET_BOARD] =        ThermalSensors::BOARD;
    return translationMap;
}

// Initialize translation map
map<UINT32, ThermalSensors::Sensor> ThermalSensors::lw2080TargetTypeToThermalSensorsMap(
    ThermalSensors::CreateLw2080TargetTypeToThermalSensorsMap());

RC ThermalSensors::TranslateLw2080TargetTypeToThermalSensor(UINT32 lw2080TargetType,
                                                            Sensor *outSensor)
{
    MASSERT(outSensor);
    map<UINT32, ThermalSensors::Sensor>::const_iterator it;
    it = lw2080TargetTypeToThermalSensorsMap.find(lw2080TargetType);
    if (it == lw2080TargetTypeToThermalSensorsMap.end())
    {
        Printf(Tee::PriError,
               "Could not find thermal target type in translation map\n");
        return RC::SOFTWARE_ERROR;
    }

    *outSensor = it->second;

    return OK;
}

// -----------------------------------------------------------------------------
//  TSM20Sensors
// -----------------------------------------------------------------------------

TSM20Sensors::TSM20Sensors(RC *rc, GpuSubdevice *sbDev)
  : ThermalSensors(sbDev)
{
    LwRmPtr pLwRm;

    LW2080_CTRL_THERMAL_DEVICE_INFO_PARAMS devParams = {};

    *rc = pLwRm->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_THERMAL_DEVICE_GET_INFO,
        &devParams,
        sizeof(devParams)
    );
    if (OK != *rc)
        return;

    for (UINT32 i = 0; i < LW2080_CTRL_THERMAL_THERM_DEVICE_MAX_COUNT; i++)
    {
        if (devParams.super.objMask.super.pData[0] & (1 << i))
        {
            m_Devices[i] = devParams.device[i];
        }
    }

    LW2080_CTRL_THERMAL_CHANNEL_INFO_PARAMS chanParams = {};

    *rc = pLwRm->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_THERMAL_CHANNEL_GET_INFO,
        &chanParams,
        sizeof(chanParams)
    );
    if (OK != *rc)
        return;

    // Store primary channel index for each thermal channel type
    memcpy(m_PriChIdx, chanParams.priChIdx,
           sizeof(UINT08) * LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_MAX_COUNT);

    UINT32 index;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(index, &chanParams.super.objMask.super)
        LW2080_CTRL_THERMAL_CHANNEL_INFO *chanInfo = &chanParams.channel[index];
        if (m_Devices.find(chanInfo->data.device.thermDevIdx) == m_Devices.end())
        {
            Printf(Tee::PriError,
                "Thermal channel %u references invalid thermal device index %u",
                index,
                chanInfo->data.device.thermDevIdx);
            *rc = RC::LWRM_ERROR;
            return;
        }

        const auto& dev = devParams.device[chanInfo->data.device.thermDevIdx];
        string chName = ProviderIndexToString(
            dev.type, chanInfo->data.device.thermDevProvIdx);
        if (dev.type == LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC)
        {
            chName += Utility::StrPrintf("_%u", dev.data.gpuGpcTsosc.gpcTsoscIdx);
        }
        else if (dev.type == LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE)
        {
            chName += Utility::StrPrintf("_%u", dev.data.hbm2Site.siteIdx);
        }

        m_Channels[index] = ChannelInfo(*chanInfo, chName);
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    *rc = HandleFloorsweptChannels();

    if (m_Channels.empty())
    {
        Printf(Tee::PriLow, "No thermal sensors found\n");
    }
}

RC TSM20Sensors::HandleFloorsweptChannels()
{
    RC rc;
    const UINT32 gpcMask = m_pSubdev->GetFsImpl()->GpcMask();

    for (auto& chInfo : m_Channels)
    {
        const auto devIdx = chInfo.second.RmInfo.data.device.thermDevIdx;
        const auto& thermDev = m_Devices.find(devIdx);
        if (thermDev == m_Devices.end())
        {
            return RC::SOFTWARE_ERROR;
        }
        const auto chType = thermDev->second.type;

        if (chType == LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC)
        {
            const UINT32 tsoscIdx = thermDev->second.data.gpuGpcTsosc.gpcTsoscIdx;
            const UINT32 tsoscGpc = 1 << tsoscIdx;
            if (!(tsoscGpc & gpcMask))
            {
                chInfo.second.IsFloorswept = true;
            }
        }
        else if (chType == LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE)
        {
            bool shouldRemove = false;

            if (m_pSubdev->IsFbBroken())
            {
                shouldRemove = true;
            }
            else
            {
                bool isHbmSiteAvailable = false;
                CHECK_RC(m_pSubdev->GetFB()->IsHbmSiteAvailable(thermDev->second.data.hbm2Site.siteIdx,
                                                                &isHbmSiteAvailable));
                shouldRemove = !isHbmSiteAvailable;
            }

            if (shouldRemove)
            {
                chInfo.second.IsFloorswept = true;
            }
        }
    }

    return rc;
}

RC TSM20Sensors::GetChannelIdx(Sensor sensor, UINT32* idx)
{
    MASSERT(idx);

    UINT32 type;
    // Get the RM Channel Type corresponding to the MODS Sensor type
    switch (sensor)
    {
        case GPU_AVG:
            type = LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_GPU_AVG;
            break;
        case GPU_MAX:
            type = LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_GPU_MAX;
            break;
        case BOARD:
            type = LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_BOARD;
            break;
        case MEMORY:
            type = LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_MEMORY;
            break;
        case POWER_SUPPLY:
            type = LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_PWR_SUPPLY;
            break;
        default:
            return RC::ILWALID_ARGUMENT;
    }

    *idx = 0;
    if (m_PriChIdx[type] < LW2080_CTRL_THERMAL_THERM_CHANNEL_MAX_CHANNELS)
    {
        *idx = m_PriChIdx[type];
        return OK;
    }
    return RC::ILWALID_ARGUMENT;
}

/* virtual */ RC TSM20Sensors::ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent)
{
    MASSERT(pShouldBePresent);
    RC rc;
    *pShouldBePresent = false;

    UINT32 idx = 0;
    if (OK == GetChannelIdx(sensor, &idx))
    {
        *pShouldBePresent = true;
    }

    return rc;
}

/* virtual */ RC TSM20Sensors::IsSensorPresent(Sensor sensor, bool *pIsPresent)
{
    MASSERT(pIsPresent);
    *pIsPresent = false;
    UINT32 dummyIdx;

    if (GetChannelIdx(sensor, &dummyIdx) == OK)
    {
        *pIsPresent = true;
    }

    return OK;
}

/* virtual */ RC TSM20Sensors::GetTemp(Sensor sensor, FLOAT32 *temp)
{
    MASSERT(temp);

    UINT32 idx = 0;
    if (OK == GetChannelIdx(sensor, &idx))
    {
        return ReadChannel(idx, temp);
    }

    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TSM20Sensors::SetSimulatedTemp
(
    Sensor sensor,
    FLOAT32 temp
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TSM20Sensors::ClearSimulatedTemp(Sensor sensor)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TSM20Sensors::PrintSensors(Tee::Priority pri)
{
    Printf(pri, "------------------------------------------------------------------------------------------\n");
    Printf(pri, "Thermal devices:\n");

    UINT32 idx = 0;
    for (const auto& lwrDev : m_Devices)
    {
        Printf(pri, " %u  Device class = %s\n", idx++, DeviceClassStr(lwrDev.second.super.type));

        switch (lwrDev.second.super.type)
        {
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADM1032:
                Printf(pri, "    i2cDevIdx = %u\n", lwrDev.second.data.adm1032.i2c.i2cDevIdx);
                break;
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_MAX6649:
                Printf(pri, "    i2cDevIdx = %u\n", lwrDev.second.data.max6649.i2c.i2cDevIdx);
                break;
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP411:
                Printf(pri, "    i2cDevIdx = %u\n", lwrDev.second.data.tmp411.i2c.i2cDevIdx);
                break;
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7461:
                Printf(pri, "    i2cDevIdx = %u\n", lwrDev.second.data.adt7461.i2c.i2cDevIdx);
                break;
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP451:
                Printf(pri, "    i2cDevIdx = %u\n", lwrDev.second.data.tmp451.i2c.i2cDevIdx);
                break;
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC:
                Printf(pri, "    gpcTsoscIdx = %u\n", lwrDev.second.data.gpuGpcTsosc.gpcTsoscIdx);
                break;
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE:
                Printf(pri, "    siteIdx = %u\n", lwrDev.second.data.hbm2Site.siteIdx);
                break;
        }
    }

    Printf(pri, "\nThermal channels:\n");
    for (auto& chanItr : m_Channels)
    {
        const auto& rmInfo = chanItr.second.RmInfo;
        Printf(
            pri,
            " %u  type = %s, device index = %u, min = %.2f, max = %.2f,"
            " hotspot offset SW = %.2f, hotspot offset HW = %.2f\n",
            chanItr.first,
            ChanTypeStr(rmInfo.chType),
            rmInfo.data.device.thermDevIdx,
            FromLwTemp(rmInfo.minTemp),
            FromLwTemp(rmInfo.maxTemp),
            FromLwTemp(rmInfo.offsetSw),
            FromLwTemp(rmInfo.offsetHw)
        );
    }
    Printf(pri, "------------------------------------------------------------------------------------------\n");
    return OK;
}

RC TSM20Sensors::ReadChannel(UINT32 channelIdx, FLOAT32 *temp)
{
    MASSERT(temp);
    if (channelIdx >= 32)
    {
        return RC::ILWALID_CHANNEL;
    }
    RC rc;
    vector<FLOAT32> temps;
    CHECK_RC(ReadChannels(1U << channelIdx, &temps));
    *temp = temps[0];
    return rc;
}

RC TSM20Sensors::ReadChannels(UINT32 chMask, vector<FLOAT32>* temps)
{
    MASSERT(temps);
    temps->clear();

    RC rc;

    LW2080_CTRL_THERMAL_CHANNEL_STATUS_PARAMS params = {};
    params.super.objMask.super.pData[0] = chMask;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_THERMAL_CHANNEL_GET_STATUS,
        &params,
        sizeof(params)
    ));

    UINT32 chIdx;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(chIdx, &params.super.objMask.super)
        temps->push_back(FromLwTemp(params.channel[chIdx].lwrrentTemp));
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC TSM20Sensors::SetTempViaChannel(UINT32 chMask, FLOAT32 temp)
{
    RC rc;
    LW2080_CTRL_THERMAL_CHANNEL_CONTROL_PARAMS params = {};
    PerfUtil::BoardObjGrpMaskBitSet(&params.super.objMask.super, chMask);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
            m_pSubdev,
            LW2080_CTRL_CMD_THERMAL_CHANNEL_GET_CONTROL,
            &params,
            sizeof(params)));

    UINT32 chIdx;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(chIdx, &params.super.objMask.super)
        params.channel[chIdx].bTempSimEnable = true;
        params.channel[chIdx].targetTemp = ToLwTemp(temp);
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
            m_pSubdev,
            LW2080_CTRL_CMD_THERMAL_CHANNEL_SET_CONTROL,
            &params,
            sizeof(params)));

    return rc;
}

/* virtual */ RC TSM20Sensors::GetTempsViaChannels(vector<ChannelTempData> *pTemp)
{
    MASSERT(pTemp);
    RC rc;

    UINT32 chMask = 0;
    for (auto& chanItr : m_Channels)
    {
        if (!chanItr.second.IsFloorswept)
        {
            chMask |= 1U << chanItr.first;
        }
    }

    vector<FLOAT32> temps;
    CHECK_RC(ReadChannels(chMask, &temps));
    UINT32 tempsIdx = 0;

    pTemp->resize(Utility::CountBits(chMask));
    while (chMask)
    {
        const UINT32 chIdx = Utility::BitScanForward(chMask);
        chMask ^= 1U << chIdx;

        (*pTemp)[tempsIdx].temp = temps[tempsIdx];
        FillTempDataMiscInfo(chIdx, &(*pTemp)[tempsIdx]);
        tempsIdx++;
    }

    return rc;
}

/* virtual */ RC TSM20Sensors::GetTempViaChannel
(
    UINT32 chIdx,
    ChannelTempData *pTemp
)
{
    RC rc;
    if (m_Channels.find(chIdx) == m_Channels.end())
    {
        Printf(Tee::PriError,
            "Invalid thermal channel index (%u)\n", chIdx);
        return RC::ILWALID_CHANNEL;
    }

    MASSERT(pTemp);
    CHECK_RC(ReadChannel(chIdx, &pTemp->temp));
    FillTempDataMiscInfo(chIdx, pTemp);
    return rc;
}

void TSM20Sensors::FillTempDataMiscInfo(UINT32 chIdx, ChannelTempData* pTemp)
{
    MASSERT(pTemp);

    pTemp->chIdx = chIdx;
    pTemp->devIdx = m_Channels[chIdx].RmInfo.data.device.thermDevIdx;
    pTemp->devClass = m_Devices[pTemp->devIdx].super.type;
    pTemp->provIdx = m_Channels[chIdx].RmInfo.data.device.thermDevProvIdx;

    // Populate device specific data
    switch (pTemp->devClass)
    {
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADM1032:
            pTemp->i2cDevIdx = m_Devices[pTemp->devIdx].data.adm1032.i2c.i2cDevIdx;
            break;
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_MAX6649:
            pTemp->i2cDevIdx = m_Devices[pTemp->devIdx].data.max6649.i2c.i2cDevIdx;
            break;
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP411:
            pTemp->i2cDevIdx = m_Devices[pTemp->devIdx].data.tmp411.i2c.i2cDevIdx;
            break;
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7461:
            pTemp->i2cDevIdx = m_Devices[pTemp->devIdx].data.adt7461.i2c.i2cDevIdx;
            break;
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP451:
            pTemp->i2cDevIdx = m_Devices[pTemp->devIdx].data.tmp451.i2c.i2cDevIdx;
            break;
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC:
            pTemp->tsoscIdx = m_Devices[pTemp->devIdx].data.gpuGpcTsosc.gpcTsoscIdx;
            break;
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE:
            pTemp->siteIdx = m_Devices[pTemp->devIdx].data.hbm2Site.siteIdx;
            break;
        default:
            break;
    }
}

/* static */ const char * TSM20Sensors::ProviderIndexToString
(
    UINT32 devClass,
    UINT32 thermDevProvIdx
)
{
    switch (devClass)
    {
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE:
                    return "SYS_TSENSE";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE_OFFSET:
                    return "SYS_TSENSE_OFFSET";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_CONST:
                    return "CONST";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_MAX:
                    return "MAX";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX:
                    return "GPU_MAX";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG:
                    return "GPU_AVG";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX:
                    return "GPU_OFFSET_MAX";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_AVG:
                    return "GPU_OFFSET_AVG";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_DYNAMIC_HOTSPOT:
                    return "DYNAMIC_HOTSPOT";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_DYNAMIC_HOTSPOT_OFFSET:
                    return "DYNAMIC_HOTSPOT_OFFSET";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_GPC_TSOSC_PROV_TSOSC:
                    return "TSOSC";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_GPC_TSOSC_PROV_TSOSC_OFFSET:
                    return "TSOSC_OFFSET";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_SCI:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_SCI_PROV_MINI_TSENSE:
                    return "SCI_MINI_TSENSE";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_COMBINED:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_GPC_COMBINED_PROV_GPC_AVG_UNMUNGED:
                    return "GPC_AVG_UNMUNGED";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_GPC_COMBINED_PROV_GPC_AVG_MUNGED:
                    return "GPC_AVG_MUNGED";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADM1032:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_EXT:
                    return "I2C_ADM1032_LOW_PREC_EXT";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_INT:
                    return "I2C_ADM1032_LOW_PREC_INT";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_MAX6649:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_EXT:
                    return "I2C_MAX6649_LOW_PREC_EXT";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_INT:
                    return "I2C_MAX6649_LOW_PREC_INT";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP411:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_EXT:
                    return "I2C_TMP411_LOW_PREC_EXT";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_INT:
                    return "I2C_TMP411_LOW_PREC_INT";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7461:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_EXT:
                    return "I2C_ADT7461_LOW_PREC_EXT";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_INT:
                    return "I2C_ADT7461_LOW_PREC_INT";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP451:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_EXT:
                    return "I2C_TMP451_LOW_PREC_EXT";
                case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_INT:
                    return "I2C_TMP451_LOW_PREC_INT";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_HBM2_SITE_PROV_DEFAULT:
                    return "HBM2_DEFAULT";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_COMBINED:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_HBM2_COMBINED_PROV_MAX:
                    return "HBM2_COMBINED_MAX";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GDDR6_X_COMBINED:
        {
            switch (thermDevProvIdx)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_GDDR6_X_COMBINED_PROV_MAX:
                    return "GDDR6_X_COMBINED_MAX";
                default:
                    return "UNKNOWN_PROV_IDX";
            }
        }
        default:
            return "UNKNOWN_DEVICE_CLASS";
    }
}

/* static */ const char *TSM20Sensors::DeviceClassStr(UINT32 devClass)
{
    return CODE_LOOKUP(devClass, DEVICE_CLASS_ToStr);
}

/* static */ const char *TSM20Sensors::ChanTypeStr(UINT32 chanType)
{
    return CODE_LOOKUP(chanType, CHANNEL_TYPE_ToStr);
}

RC TSM20Sensors::GetChannelInfos(ChannelInfos* pChInfos) const
{
    MASSERT(pChInfos);
    *pChInfos = m_Channels;
    return RC::OK;
}

/* static */ const char *Thermal::HwfsEventTypeStr(UINT32 HwfsEventType)
{
    return CODE_LOOKUP(HwfsEventType, HWFS_EVT_TYPE_ToStr);
}

/* static */ const char *Thermal::ThermalCapPolicyIndexToStr(UINT32 policyIndex)
{
    return CODE_LOOKUP(policyIndex, THERMAL_CAP_POLICY_INDEX_ToStr);
}

/* static */ const char *Thermal::HwfsEventCategoryToStr(Thermal::HwfsEventCategory eventCat)
{
    return CODE_LOOKUP(static_cast<UINT32>(eventCat), HwfsEventCategory_ToStr);
}

// -----------------------------------------------------------------------------
//  ExelwteSensors
// -----------------------------------------------------------------------------

namespace
{
    RC Execute(
        GpuSubdevice *pSubdev,
        LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION *ops,
        UINT32 opNum
    );

    const char *ExecStatusMsg(LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION *op);

    RC Execute
    (
        GpuSubdevice *pSubdev,
        LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION *ops,
        UINT32 opNum
    )
    {
        RC rc;
        LwRmPtr pLwRm;

        MASSERT(ops);
        MASSERT(opNum);

        LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_V2_PARAMS params = {
            THERMAL_SYSTEM_API_VER,                           // clientAPIVersion
            THERMAL_SYSTEM_API_REV,                           // clientAPIRevision
            sizeof(LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION),   // clientInstructionSizeOf
            LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_FLAGS_DEFAULT, // exelwteFlags
            0,                                                // successfulInstructions
            opNum,                                            // instructionListSize
        };

        if (opNum > NUMELEMS(params.instructionList))
        {
            Printf(Tee::PriError,
                   "Number of opcodes %u exceeds max %zu\n",
                   opNum, NUMELEMS(params.instructionList));
            return LW_ERR_INSUFFICIENT_RESOURCES;
        }

        memcpy(&params.instructionList, ops,
               opNum * sizeof(params.instructionList[0]));

        CHECK_RC(pLwRm->Control(
            pLwRm->GetSubdeviceHandle(pSubdev),
            LW2080_CTRL_CMD_THERMAL_SYSTEM_EXELWTE_V2,
            &params,
            sizeof(params)
        ));

        memcpy(ops, &params.instructionList,
               opNum * sizeof(params.instructionList[0]));

        Tee::Priority pri;

        if (params.successfulInstructions != opNum)
        {
            rc = RC::LWRM_ERROR;
            pri = Tee::PriNormal;
            Printf(
                pri,
                "Only %u out of %u instructions succeeded! ",
                params.successfulInstructions,
                opNum
            );
        }
        else
        {
            pri = Tee::PriSecret;
        }

        Printf(pri, "Exelwted thermal system instructions:\n");
        for (UINT32 i = 0; i < opNum; i++)
        {
            Printf( pri, "\t%u\topcode = 0x%08x, %s\n",
                    i, ops[i].opcode, ExecStatusMsg(ops + i) );
        }

        return rc;
    }

    const char *ExecStatusMsg
    (
        LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION *op
    )
    {
        switch (op->exelwted)
        {
            case 0:
                return "not exelwted";
            case 1:
                switch (op->result)
                {
                    case LW_OK:
                        return "success";
                    case LW_ERR_ILWALID_ARGUMENT:
                        return "error - invalid param argument";
                    case LW_ERR_ILWALID_PARAM_STRUCT:
                        return "error - invalid param struct";
                    default:
                        return "illegal \"result\" code";
                }
            default:
                return "illegal \"exelwted\" code";
        }
    }
}

ExelwteSensors::ExelwteSensors(RC *rc, GpuSubdevice *sbDev)
  : ThermalSensors(sbDev)
{
    *rc = ConstructExelwteSensors();
}

// Helper function so that we can use StickyRC and all of the nice RC macros
RC ExelwteSensors::ConstructExelwteSensors()
{
    RC rc;
    LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION available[3] = {{0}};

    available[0].opcode =
        LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_TARGETS_AVAILABLE_OPCODE;
    available[1].opcode =
        LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_PROVIDERS_AVAILABLE_OPCODE;
    available[2].opcode =
        LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_SENSORS_AVAILABLE_OPCODE;

    CHECK_RC(Execute(m_pSubdev, available, static_cast<UINT32>(Utility::ArraySize(available))));

    UINT32 availableTargets = available[0].operands.getInfoTargetsAvailable.availableTargets;
    UINT32 availableProviders = available[1].operands.getInfoProvidersAvailable.availableProviders;
    UINT32 availableSensors = available[2].operands.getInfoSensorsAvailable.availableSensors;

    if (availableTargets < 1 || availableProviders < 1 || availableSensors < 1)
    {
        Printf(Tee::PriError,
               "Thermal opcodes reports 0 sensors, providers or targets!\n");
        Printf(Tee::PriError, "Available Targets = %d\n", availableTargets);
        Printf(Tee::PriError, "Available providers = %d\n", availableProviders);
        Printf(Tee::PriError, "Available sensors = %d\n", availableSensors);
        return RC::SOFTWARE_ERROR;
    }

    // Get info about the targets associated with each sensor
    vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> sensorInfo(availableSensors);
    memset(&sensorInfo[0], 0, sizeof(sensorInfo[0]) * availableSensors);

    for (UINT32 i = 0; i < availableSensors; ++i)
    {
        sensorInfo[i].opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_SENSOR_TARGET_OPCODE;
        sensorInfo[i].operands.getInfoSensorTarget.sensorIndex = i;
    }

    CHECK_RC(Execute(m_pSubdev, &sensorInfo[0], availableSensors));

    // Get the target type for each target
    vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> targetInfo(availableTargets);
    memset(&targetInfo[0], 0, sizeof(targetInfo[0]) * availableTargets);

    for (UINT32 i = 0; i < availableTargets; ++i)
    {
        targetInfo[i].opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_TARGET_TYPE_OPCODE;
        targetInfo[i].operands.getInfoTargetType.targetIndex = i;
    }

    CHECK_RC(Execute(m_pSubdev, &targetInfo[0], availableTargets));

    // Since thermal opcodes only supports reading from the 0th sensor,
    // make sure that the 0th sensor reads the GPU
    UINT32 sensor0TargetIndex = sensorInfo[0].operands.getInfoSensorTarget.targetIndex;
    UINT32 sensor0TargetType = targetInfo[sensor0TargetIndex].operands.getInfoTargetType.type;
    Sensor sensor;
    CHECK_RC(TranslateLw2080TargetTypeToThermalSensor(sensor0TargetType, &sensor));
    if (sensor != ThermalSensors::GPU)
    {
        Printf(Tee::PriError,
               "Logical sensor 0 does not measure the GPU! "
               "Please check board or BIOS setting\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    // Save the various targets that we have access to
    // Associate a ThermalSensors::Sensor with each sensor index
    for (UINT32 i = 0; i < availableSensors; ++i)
    {
        UINT32 sensorTargetIndex;
        // This variable is of type LW2080_CTRL_THERMAL_SYSTEM_TARGET_*
        UINT32 sensorTargetType;
        ThermalSensors::Sensor newSensor;
        sensorTargetIndex = sensorInfo[i].operands.getInfoSensorTarget.targetIndex;
        sensorTargetType = targetInfo[sensorTargetIndex].operands.getInfoTargetType.type;
        CHECK_RC(TranslateLw2080TargetTypeToThermalSensor(sensorTargetType, &newSensor));
        m_presentSensors[newSensor] = i;

        Printf(Tee::PriLow,
               "Sensor %d, target index %d, with type %d\n",
               i, sensorTargetIndex, sensorTargetType);
    }

    // Get and print the providers
    vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> providerInfo(availableProviders);
    memset(&providerInfo[0], 0, sizeof(providerInfo[0]) * availableProviders);
    for (UINT32 i = 0; i < availableProviders; ++i)
    {
        providerInfo[i].opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_PROVIDER_TYPE_OPCODE;
        providerInfo[i].operands.getInfoProviderType.providerIndex = i;
    }

    CHECK_RC(Execute(m_pSubdev, &providerInfo[0], availableProviders));

    for (UINT32 i = 0; i < availableProviders; ++i)
    {
        Printf(Tee::PriLow,
               "Provider %d has type %d\n",
               i, providerInfo[i].operands.getInfoProviderType.type);
    }

    return rc;
}

/* virtual */ RC ExelwteSensors::ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent)
{
    *pShouldBePresent = m_presentSensors.find(sensor) != m_presentSensors.end();
    return OK;
}

/* virtual */ RC ExelwteSensors::IsSensorPresent(Sensor sensor, bool *pIsPresent)
{
    MASSERT(pIsPresent);
    RC rc;
    FLOAT32 dummyTemp = 0.0;
    *pIsPresent = false;

    rc = GetTemp(sensor, &dummyTemp);
    if (rc == OK)
    {
        *pIsPresent = true;
    }
    else if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        rc.Clear();
    }

    return rc;
}

/* virtual */ RC ExelwteSensors::GetTemp(Sensor sensor, FLOAT32 *temp)
{
    RC rc;

    bool shouldBePresent = false;
    CHECK_RC(ShouldSensorBePresent(sensor, &shouldBePresent));
    if (!shouldBePresent)
        return RC::UNSUPPORTED_FUNCTION;

    map<Sensor, UINT32>::iterator it = m_presentSensors.find(sensor);
    UINT32 sensorIndex = it->second;

    LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION read;
    read.opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_STATUS_SENSOR_READING_OPCODE;
    read.operands.getStatusSensorReading.sensorIndex = sensorIndex;

    CHECK_RC(Execute(m_pSubdev, &read, 1));

    *temp = static_cast<FLOAT32>(read.operands.getStatusSensorReading.value);

    return rc;
}

/* virtual */ RC ExelwteSensors::GetTempsViaChannels(vector<ChannelTempData> *pTemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC ExelwteSensors::GetTempViaChannel
(
    UINT32 chIdx,
    ChannelTempData *pTemp
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC ExelwteSensors::SetSimulatedTemp
(
    Sensor sensor,
    FLOAT32 temp
)
{
    if (GPU != sensor)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;

    LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION setSim[2] = {{0}};
    setSim[0].opcode =
        LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_SUPPORTS_TEMPSIM_OPCODE;
    setSim[0].operands.getInfoSupportsTempSim.sensorIndex = 0;
    setSim[1].opcode =
        LW2080_CTRL_THERMAL_SYSTEM_SET_TEMPSIM_CONTROL_AND_TEMPERATURE_OPCODE;
    setSim[1].operands.setTempSimControlAndTemperature.sensorIndex = 0;
    setSim[1].operands.setTempSimControlAndTemperature.temperature =
        static_cast<LwS32>(temp);
    setSim[1].operands.setTempSimControlAndTemperature.control =
        LW2080_CTRL_THERMAL_SYSTEM_TEMPSIM_CONTROL_ENABLE_YES;

    CHECK_RC(Execute(m_pSubdev, setSim, static_cast<UINT32>(Utility::ArraySize(setSim))));

    if (LW2080_CTRL_THERMAL_SYSTEM_TEMPSIM_SUPPORT_YES !=
        setSim[0].operands.getInfoSupportsTempSim.support)
    {
        // This particular sensor doesn't support simulated temperatures
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

/* virtual */ RC ExelwteSensors::ClearSimulatedTemp(Sensor sensor)
{
    if (GPU != sensor)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION clearSim;
    clearSim.opcode =
        LW2080_CTRL_THERMAL_SYSTEM_SET_TEMPSIM_CONTROL_AND_TEMPERATURE_OPCODE;
    clearSim.operands.setTempSimControlAndTemperature.sensorIndex = 0;
    clearSim.operands.setTempSimControlAndTemperature.control =
        LW2080_CTRL_THERMAL_SYSTEM_TEMPSIM_CONTROL_ENABLE_NO;

    return Execute(m_pSubdev, &clearSim, 1);
}

/* virtual */ RC ExelwteSensors::PrintSensors(Tee::Priority pri)
{
    Printf(pri, "Execute\n");
    return OK;
}

// CheetAh Sensors
TegraSensors::TegraSensors(RC *rc, GpuSubdevice *sbDev)
    : ThermalSensors(sbDev)
{
    *rc = OK;
}

/* virtual */ RC TegraSensors::ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent)
{
    MASSERT(pShouldBePresent);
    *pShouldBePresent = (sensor == GPU);
    return OK;
}

/* virtual */ RC TegraSensors::IsSensorPresent(Sensor sensor, bool *pIsPresent)
{
    MASSERT(pIsPresent);
    RC rc;
    FLOAT32 dummyTemp = 0.0;
    *pIsPresent = false;

    rc = GetTemp(sensor, &dummyTemp);
    if (rc == OK)
    {
        *pIsPresent = true;
    }
    else if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        rc.Clear();
    }

    return rc;
}

/* virtual */ RC TegraSensors::GetTemp(Sensor sensor, FLOAT32 *temp)
{
    RC rc;
    switch (sensor)
    {
        case GPU:
            CHECK_RC(GetTegraGpuTemp(temp));
            break;
        default:
            return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
}

/* virtual */ RC TegraSensors::GetTempsViaChannels(vector<ChannelTempData> *pTemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraSensors::GetTempViaChannel
(
    UINT32 chIdx,
    ChannelTempData *pTemp
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraSensors::SetSimulatedTemp(Sensor sensor, FLOAT32 temp)
{
    return OK;
}

/* virtual */ RC TegraSensors::ClearSimulatedTemp(Sensor sensor)
{
    return OK;
}

/* virtual */ RC TegraSensors::PrintSensors(Tee::Priority pri/*=Tee::PriNormal*/)
{
    Printf(pri, "CheetAh temperature sensors\n");
    return OK;
}

RC TegraSensors::GetTegraGpuTemp(FLOAT32 *temp)
{
    return Thermal::GetSocGpuTemp(temp);
}

StubSensors::StubSensors(RC *rc, GpuSubdevice *sbDev)
  : ThermalSensors(sbDev)
{
    *rc = OK;
}

/* virtual */ RC StubSensors::ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent)
{
    *pShouldBePresent = false;
    return OK;
}

/* virtual */ RC StubSensors::IsSensorPresent(Sensor sensor, bool *pIsPresent)
{
    *pIsPresent = false;
    return OK;
}

/* virtual */ RC StubSensors::GetTemp(Sensor sensor, FLOAT32 *temp)
{
    *temp = StubSensors::STUB_TEMPERATURE;
    return OK;
}

/* virtual */ RC StubSensors::GetTempsViaChannels(vector<ChannelTempData> *pTemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC StubSensors::GetTempViaChannel
(
    UINT32 chIdx,
    ChannelTempData *pTemp
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC StubSensors::SetSimulatedTemp(Sensor sensor, FLOAT32 temp)
{
    return OK;
}

/* virtual */ RC StubSensors::ClearSimulatedTemp(Sensor sensor)
{
    return OK;
}

/* virtual */ RC StubSensors::PrintSensors(Tee::Priority pri/*=Tee::PriNormal*/)
{
    Printf(pri, "Stubbed temperature sensor\n");
    return OK;
}

const FLOAT32 StubSensors::STUB_TEMPERATURE = 30.0;

// BJT Sensors
GpuBJTSensors::GpuBJTSensors(RC *rc, GpuSubdevice *sbDev)
    : ThermalSensors(sbDev)
{
    *rc = RC::OK;
}

/* virtual */ RC GpuBJTSensors::ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent)
{
    MASSERT(pShouldBePresent);
    *pShouldBePresent = (sensor == GPU_BJT);
    return RC::OK;
}

/* virtual */ RC GpuBJTSensors::IsSensorPresent(Sensor sensor, bool *pIsPresent)
{
    MASSERT(pIsPresent);
    RegHal& regs = m_pSubdev->Regs();

    if (sensor == GPU_BJT && (regs.IsSupported(MODS_FUSE_OPT_GPC_TS_DIS_MASK_0_DATA) 
                              || regs.IsSupported(MODS_FUSE_OPT_GPC_TS_DIS_MASK_1_DATA)))
    {
        const UINT32 bjtEnabledMask0 =
            ~regs.Read32(MODS_FUSE_OPT_GPC_TS_DIS_MASK_0_DATA) & regs.LookupMask(MODS_FUSE_OPT_GPC_TS_DIS_MASK_0_DATA);
        const UINT32 bjtEnabledMask1 = 
            ~regs.Read32(MODS_FUSE_OPT_GPC_TS_DIS_MASK_1_DATA) & regs.LookupMask(MODS_FUSE_OPT_GPC_TS_DIS_MASK_1_DATA);
        // At least one BJT sensor must be present
        *pIsPresent = (Utility::CountBits(bjtEnabledMask0) > 0 
        || Utility::CountBits(bjtEnabledMask1) > 0)? true : false;
    }
    else if (sensor == LWL_BJT && regs.IsSupported(MODS_FUSE_OPT_LWL_TS_DIS_MASK_DATA))
    {
        const UINT32 bjtEnabledMask =
            ~regs.Read32(MODS_FUSE_OPT_LWL_TS_DIS_MASK_DATA) & regs.LookupMask(MODS_FUSE_OPT_LWL_TS_DIS_MASK_DATA);

        // At least one BJT sensor is present
        *pIsPresent = Utility::CountBits(bjtEnabledMask) > 0 ? true : false;
    }
    else if (sensor == SYS_BJT)
    {
        // Starting Hopper the SYS BJT sensor can be disabled through fuse, so we are reading this
        // register to know if SYS BJT is disabled
        if (regs.IsSupported(MODS_FUSE_OPT_SYS_TS_DIS_MASK_DATA))
        {
            const UINT32 bjtEnabledMask =
                ~regs.Read32(MODS_FUSE_OPT_SYS_TS_DIS_MASK_DATA) &
                regs.LookupMask(MODS_FUSE_OPT_SYS_TS_DIS_MASK_DATA);

            // At least one BJT sensor is present
            *pIsPresent = Utility::CountBits(bjtEnabledMask) > 0;
        }
        else
        {
            // RM says it'll always be present
            if (m_pSubdev->Regs().IsSupported(MODS_THERM_TEMP_SENSOR_SYS_TSENSE_SNAPSHOT))
            {
                *pIsPresent = true;
            }
            else
            {
                *pIsPresent = false;
            }
        }
    }
    else if (sensor == SCI_BJT)
    {
        if (m_pSubdev->Regs().IsSupported(MODS_PGC6_SCI_FS_OVERT_TEMPERATURE))
        {
            *pIsPresent = true;
        }
        else
        {
            *pIsPresent = false;
        }
    }
    else
    {
        *pIsPresent = false;
    }

    return RC::OK;
}

/* virtual */ RC GpuBJTSensors::GetTemp(Sensor sensor, FLOAT32 *ptemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuBJTSensors::GetTempsViaChannels(vector<ChannelTempData> *pTemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuBJTSensors::GetTempViaChannel(UINT32 chIdx, ChannelTempData *pTemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuBJTSensors::SetSimulatedTemp(Sensor sensor, FLOAT32 temp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuBJTSensors::ClearSimulatedTemp(Sensor sensor)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuBJTSensors::PrintSensors(Tee::Priority pri)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuBJTSensors::SetSnapShotTrigger()
{
    RegHal& regs = m_pSubdev->Regs();

    regs.Write32(MODS_THERM_TSENSE_GLOBAL_SNAPSHOT_CMD_TRIGGER);

    return RC::OK;
}

RC GpuBJTSensors::AcquireSnapshotMutex()
{
    RC rc;

    m_pPmgrMutex = make_unique<PmgrMutexHolder>(m_pSubdev,
        PmgrMutexHolder::MI_THERM_TSENSE_SNAPSHOT);
    CHECK_RC(m_pPmgrMutex->Acquire(Tasker::NO_TIMEOUT));

    return RC::OK;
}

RC GpuBJTSensors::ReleaseSnapShotMutex()
{
    MASSERT(m_pPmgrMutex);
    m_pPmgrMutex->Release();

    return RC::OK;
}

RC GpuBJTSensors::GetGpcBjtTempSnapshot(ThermalSensors::TsenseBJTTempMap *pTemp)
{
    RC rc;

    RegHal& regs = m_pSubdev->Regs();

    // Maximum no of BJTs available per GPC = MaximumIdx + 1;
    const UINT32 maxBjtPerGpc = regs.LookupValue(MODS_THERM_GPC_TSENSE_INDEX_SNAPSHOT_GPC_BJT_INDEX_MAX) + 1;

    // Maximum no GPC with BJTs = MaximumIdx + 1
    const UINT32 maxGpcs = regs.LookupValue(MODS_THERM_GPC_TSENSE_INDEX_SNAPSHOT_GPC_INDEX_MAX) + 1;

    // Total no of bjts
    const UINT32 totalBjts = maxBjtPerGpc * maxGpcs;

    // Available gpc mask
    const UINT32 gpcMask = m_pSubdev->GetFsImpl()->GpcMask();

    // Reset indexes
    regs.Write32(MODS_THERM_GPC_TSENSE_INDEX_SNAPSHOT_GPC_BJT_INDEX_MIN);
    regs.Write32(MODS_THERM_GPC_TSENSE_INDEX_SNAPSHOT_GPC_INDEX_MIN);

    // Enable read increment
    regs.Write32(MODS_THERM_GPC_TSENSE_INDEX_SNAPSHOT_READINCR_ENABLED);

    // Read BJTs SNAPSHOT values
    vector<FLOAT32> lwrrentTemp(totalBjts);
    const UINT32 lwrrentSnapshotStateValid =
        regs.LookupValue(MODS_THERM_TEMP_SENSOR_GPC_TSENSE_LWRR_SNAPSHOT_STATE_VALID);
    for (UINT32 index = 0; index < totalBjts; index++)
    {
        const UINT32 regValue = regs.Read32(MODS_THERM_TEMP_SENSOR_GPC_TSENSE_LWRR_SNAPSHOT);
        const bool isStateValid = regs.TestField(regValue, MODS_THERM_TEMP_SENSOR_GPC_TSENSE_LWRR_SNAPSHOT_STATE, lwrrentSnapshotStateValid);
        lwrrentTemp[index] = isStateValid
            ? FromFX9P5ToCelsius(regs.GetField(regValue, MODS_THERM_TEMP_SENSOR_GPC_TSENSE_LWRR_SNAPSHOT_FIXED_POINT))
            : GpuBJTSensors::ILWALID_TEMP;
    }

    // Read BJTs OFFSET SNAPSHOT values
    vector<FLOAT32> lwrrentOffsetTemp(totalBjts);
    const UINT32 lwrrentOffsetSnapshotStateValid =
        regs.LookupValue(MODS_THERM_TEMP_SENSOR_GPC_TSENSE_OFFSET_LWRR_SNAPSHOT_STATE_VALID);
    for (UINT32 index = 0; index < totalBjts; index++)
    {
        const UINT32 regValue = regs.Read32(MODS_THERM_TEMP_SENSOR_GPC_TSENSE_OFFSET_LWRR_SNAPSHOT);
        const bool isStateValid = regs.TestField(regValue, MODS_THERM_TEMP_SENSOR_GPC_TSENSE_OFFSET_LWRR_SNAPSHOT_STATE, lwrrentOffsetSnapshotStateValid);
        lwrrentOffsetTemp[index] = isStateValid
            ? FromFX9P5ToCelsius(regs.GetField(regValue, MODS_THERM_TEMP_SENSOR_GPC_TSENSE_OFFSET_LWRR_SNAPSHOT_FIXED_POINT))
            : GpuBJTSensors::ILWALID_TEMP;
    }

    // Store values
    for (UINT32 index = 0; index < totalBjts; index++)
    {
        const UINT32 gpcIndex = index / maxBjtPerGpc;
        if ((1 << gpcIndex) & gpcMask)
        {
            (*pTemp)[gpcIndex].emplace_back(lwrrentTemp[index], lwrrentOffsetTemp[index]);
        }
    }

    return rc;
}

RC GpuBJTSensors::GetLwlBjtTempSnapshot(ThermalSensors::TsenseBJTTempMap *pTemp)
{
    RC rc;

    RegHal& regs = m_pSubdev->Regs();

    // Maximum no of Lwl Bjt sensors = MaximumIdx + 1;
    const UINT32 maxBjt = regs.LookupValue(MODS_THERM_LWL_TSENSE_INDEX_SNAPSHOT_INDEX_MAX) + 1;

    // Reset index
    regs.Write32(MODS_THERM_LWL_TSENSE_INDEX_SNAPSHOT_INDEX_MIN);

    // Enable read increment
    regs.Write32(MODS_THERM_LWL_TSENSE_INDEX_SNAPSHOT_READINCR_ENABLED);

    // Read BJTs SNAPSHOT values
    vector<FLOAT32> lwrrentTemp(maxBjt);
    const UINT32 lwrrentSnapshotStateValid =
        regs.LookupValue(MODS_THERM_TEMP_SENSOR_LWL_TSENSE_SNAPSHOT_STATE_VALID);
    for (UINT32 index = 0; index < maxBjt; index++)
    {
        const UINT32 regValue = regs.Read32(MODS_THERM_TEMP_SENSOR_LWL_TSENSE_SNAPSHOT);
        const bool isStateValid = regs.TestField(regValue, MODS_THERM_TEMP_SENSOR_LWL_TSENSE_SNAPSHOT_STATE, lwrrentSnapshotStateValid);
        lwrrentTemp[index] = isStateValid
            ? FromFX9P5ToCelsius(regs.GetField(regValue, MODS_THERM_TEMP_SENSOR_LWL_TSENSE_SNAPSHOT_FIXED_POINT))
            : GpuBJTSensors::ILWALID_TEMP;
    }

    // Read BJTs OFFSET SNAPSHOT values
    vector<FLOAT32> lwrrentOffsetTemp(maxBjt);
    const UINT32 lwrrentOffsetSnapshotStateValid =
        regs.LookupValue(MODS_THERM_TEMP_SENSOR_OFFSET_LWL_TSENSE_SNAPSHOT_STATE_VALID);
    for (UINT32 index = 0; index < maxBjt; index++)
    {
        const UINT32 regValue = regs.Read32(MODS_THERM_TEMP_SENSOR_OFFSET_LWL_TSENSE_SNAPSHOT);
        const bool isStateValid = regs.TestField(regValue, MODS_THERM_TEMP_SENSOR_OFFSET_LWL_TSENSE_SNAPSHOT_STATE, lwrrentOffsetSnapshotStateValid);
        lwrrentOffsetTemp[index] = isStateValid
            ? FromFX9P5ToCelsius(regs.GetField(regValue, MODS_THERM_TEMP_SENSOR_OFFSET_LWL_TSENSE_SNAPSHOT_FIXED_POINT))
            : GpuBJTSensors::ILWALID_TEMP;
    }

    // Store values
    for (UINT32 index = 0; index < maxBjt; index++)
    {
        (*pTemp)[0].emplace_back(lwrrentTemp[index], lwrrentOffsetTemp[index]);
    }

    return rc;
}

RC GpuBJTSensors::GetSysBjtTempSnapshot(ThermalSensors::TsenseBJTTempMap *pTemp)
{
    RC rc;

    RegHal& regs = m_pSubdev->Regs();

    // Read BJTs SNAPSHOT values, there is only 1 SYS BJT sensor
    FLOAT32 lwrrentTemp;
    {
        const UINT32 lwrrentSnapshotStateValid =
            regs.LookupValue(MODS_THERM_TEMP_SENSOR_SYS_TSENSE_SNAPSHOT_STATE_VALID);
        const UINT32 regValue = regs.Read32(MODS_THERM_TEMP_SENSOR_SYS_TSENSE_SNAPSHOT);
        const bool isStateValid = regs.TestField(regValue, 
        MODS_THERM_TEMP_SENSOR_SYS_TSENSE_SNAPSHOT_STATE, lwrrentSnapshotStateValid);
        lwrrentTemp = isStateValid
            ? FromFX9P5ToCelsius(regs.GetField(regValue,
            MODS_THERM_TEMP_SENSOR_SYS_TSENSE_SNAPSHOT_FIXED_POINT)) : GpuBJTSensors::ILWALID_TEMP;
    }

    // Read BJTs OFFSET SNAPSHOT values
    FLOAT32 lwrrentOffsetTemp;
    {
        const UINT32 lwrrentOffsetSnapshotStateValid =
            regs.LookupValue(MODS_THERM_TEMP_SENSOR_OFFSET_SYS_TSENSE_SNAPSHOT_STATE_VALID);
        const UINT32 regValue = regs.Read32(MODS_THERM_TEMP_SENSOR_OFFSET_SYS_TSENSE_SNAPSHOT);
        const bool isStateValid = regs.TestField(regValue,
        MODS_THERM_TEMP_SENSOR_OFFSET_SYS_TSENSE_SNAPSHOT_STATE,
        lwrrentOffsetSnapshotStateValid);
        lwrrentOffsetTemp = isStateValid
            ? FromFX9P5ToCelsius(regs.GetField(regValue,
            MODS_THERM_TEMP_SENSOR_OFFSET_SYS_TSENSE_SNAPSHOT_FIXED_POINT)): GpuBJTSensors::ILWALID_TEMP;
    }
    (*pTemp)[0].emplace_back(lwrrentTemp, lwrrentOffsetTemp);

    return rc;
}

RC GpuBJTSensors::GetSciBjtTemp(ThermalSensors::TsenseBJTTempMap *pTemp)
{
    RC rc;

    RegHal& regs = m_pSubdev->Regs();

    // Read the instantaneous temperature value of the SCI OVERT Temperature sensor
    // the xx_INTEGER field contains the integer portion of the temperature sensor
    // and it ranges from -256C to 255C
    UINT32 lwrrentTemp = regs.Read32(MODS_PGC6_SCI_FS_OVERT_TEMPERATURE_INTEGER);
    UINT32 signbit = lwrrentTemp & (1 << 8);
    if (signbit > 0)
    {
        lwrrentTemp = lwrrentTemp | 0xFFFFFF00;
    }
    INT32 signedTemp = reinterpret_cast<INT32&>(lwrrentTemp);
    (*pTemp)[0].emplace_back(static_cast<FLOAT32>(signedTemp), 0.0f);

    return rc;
}

RC GpuBJTSensors::GetTempSnapshot
(
    map<string, ThermalSensors::TsenseBJTTempMap> *pBjtTemps,
    const vector<ThermalSensors::Sensor> &sensors
)
{
    RC rc;

    // Acquire Snapshot Mutex before every access
    CHECK_RC(AcquireSnapshotMutex());

    DEFER
    {
        // Release Snapshot mutex
        ReleaseSnapShotMutex();
    };

    // Trigger SnapShot
    CHECK_RC(SetSnapShotTrigger());

    for (auto & sensor : sensors)
    {
        if (sensor == GPU_BJT)
        {
            ThermalSensors::TsenseBJTTempMap tempMap;
            CHECK_RC(GetGpcBjtTempSnapshot(&tempMap));
            (*pBjtTemps)["GPC"] = tempMap;
        }
        else if (sensor == LWL_BJT)
        {
            ThermalSensors::TsenseBJTTempMap tempMap;
            CHECK_RC(GetLwlBjtTempSnapshot(&tempMap));
            (*pBjtTemps)["LWL"] = tempMap;
        }
        else if (sensor == SYS_BJT)
        {
            ThermalSensors::TsenseBJTTempMap tempMap;
            CHECK_RC(GetSysBjtTempSnapshot(&tempMap));
            (*pBjtTemps)["SYS"] = tempMap;
        }
        else if (sensor == SCI_BJT)
        {
            ThermalSensors::TsenseBJTTempMap tempMap;
            CHECK_RC(GetSciBjtTemp(&tempMap));
            (*pBjtTemps)["SCI"] = tempMap;
        }
        else
        {
            Printf(Tee::PriError, "BJT thermal sensor unknown\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    return RC::OK;
}

// DRAM Temp Sensors
DramTempSensors::DramTempSensors(RC* rc, GpuSubdevice* pSubdev)
: ThermalSensors(pSubdev)
{
    *rc = RC::OK;
}

/* virtual */ RC DramTempSensors::ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent)
{
    MASSERT(pShouldBePresent);
    *pShouldBePresent = (sensor == DRAM_TEMP);
    return RC::OK;
}

/* virtual */ RC DramTempSensors::IsSensorPresent(Sensor sensor, bool *pIsPresent)
{
    MASSERT(pIsPresent);
    *pIsPresent = true;

    const RegHal& regs = m_pSubdev->Regs();
    *pIsPresent = regs.IsSupported(MODS_PFB_FBPA_0_DQR_STATUS_DQ_ICx_SUBPy);

    return RC::OK;
}

/* virtual */ RC DramTempSensors::GetTemp(Sensor sensor, FLOAT32 *ptemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC DramTempSensors::GetTempsViaChannels(vector<ChannelTempData> *pTemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC DramTempSensors::GetTempViaChannel(UINT32 chIdx, ChannelTempData *pTemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC DramTempSensors::SetSimulatedTemp(Sensor sensor, FLOAT32 temp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC DramTempSensors::ClearSimulatedTemp(Sensor sensor)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC DramTempSensors::PrintSensors(Tee::Priority pri)
{
    return RC::UNSUPPORTED_FUNCTION;
}

// From the Jedec spec
INT32 DramTempSensors::DecodeTemp(UINT32 raw, bool clamshell)
{
    // Extract field
    const UINT32 shift = (clamshell) ? 24 : 16;
    const UINT32 field = (raw >> shift) & 0xFF;

    // Clamp output and colwert
    return (static_cast<INT32>(std::min(field, 80u)) - 20) * 2;
}

RC DramTempSensors::GetDramTempSnapshot(ThermalSensors::DramTempMap *pTemp)
{
    const RegHal& regs = m_pSubdev->Regs();
    FrameBuffer* pFb = m_pSubdev->GetFB();

    const bool  clamshell = pFb->IsClamshell();
    const UINT32 numFbios = pFb->GetFbioCount();
    for (UINT32 virtFbio = 0; virtFbio < numFbios; virtFbio++)
    {
        MASSERT(m_pSubdev->GetFsImpl()->GetHalfFbpaWidth());
        const bool isHalfFbpa = pFb->IsHalfFbpaEn(virtFbio);

        // LW_PFB_FBPA_<IDX> registers are indexed by the HW FBPA index
        const UINT32 hwFbpa = pFb->HwFbioToHwFbpa(pFb->VirtualFbioToHwFbio(virtFbio));
        const UINT32 status = regs.Read32(MODS_PFB_FBPA_0_DQR_STATUS_VLD, hwFbpa);
        if (!isHalfFbpa &&
            regs.Read32(status, MODS_PFB_FBPA_0_DQR_STATUS_VLD_IC0_SUBP0, hwFbpa))
        {
            UINT32 val = regs.Read32(MODS_PFB_FBPA_0_DQR_STATUS_DQ_ICx_SUBPy, {hwFbpa, 0, 0});
            (*pTemp)[make_tuple(virtFbio, 0, 0, 0)] = DecodeTemp(val, false);
            if (clamshell)
            {
                (*pTemp)[make_tuple(virtFbio, 0, 1, 0)] = DecodeTemp(val, true);
            }
        }
        if (!isHalfFbpa &&
            regs.Read32(status, MODS_PFB_FBPA_0_DQR_STATUS_VLD_IC1_SUBP0, hwFbpa))
        {
            UINT32 val = regs.Read32(MODS_PFB_FBPA_0_DQR_STATUS_DQ_ICx_SUBPy, {hwFbpa, 1, 0});
            (*pTemp)[make_tuple(virtFbio, 0, 0, 1)] = DecodeTemp(val, false);
            if (clamshell)
            {
                (*pTemp)[make_tuple(virtFbio, 0, 1, 1)] = DecodeTemp(val, true);
            }
        }
        if (regs.Read32(status, MODS_PFB_FBPA_0_DQR_STATUS_VLD_IC0_SUBP1, hwFbpa))
        {
            UINT32 val = regs.Read32(MODS_PFB_FBPA_0_DQR_STATUS_DQ_ICx_SUBPy, {hwFbpa, 0, 1});
            (*pTemp)[make_tuple(virtFbio, 1, 0, 0)] = DecodeTemp(val, false);
            if (clamshell)
            {
                (*pTemp)[make_tuple(virtFbio, 1, 1, 0)] = DecodeTemp(val, true);
            }
        }
        if (regs.Read32(status, MODS_PFB_FBPA_0_DQR_STATUS_VLD_IC1_SUBP1, hwFbpa))
        {
            UINT32 val = regs.Read32(MODS_PFB_FBPA_0_DQR_STATUS_DQ_ICx_SUBPy, {hwFbpa, 1, 1});
            (*pTemp)[make_tuple(virtFbio, 1, 0, 1)] = DecodeTemp(val, false);
            if (clamshell)
            {
                (*pTemp)[make_tuple(virtFbio, 1, 1, 1)] = DecodeTemp(val, true);
            }
        }
    }

    // Verify that we read all the DRAM temps
    // The number of bits in the halfFbpaMask corresponds to the number of SUBPs we are disabling
    const UINT32 expected =
        (numFbios * pFb->GetSubpartitions() - Utility::CountBits(pFb->GetHalfFbpaMask())) *
        (pFb->GetPseudoChannelsPerSubpartition()) *
        (clamshell ? 2 : 1);
    if (pTemp->size() != expected)
    {
        Printf(Tee::PriError,
               "Unable to read all DRAM temps, expected %d valid samples read %zu!\n",
                expected, pTemp->size());
        return RC::THERMAL_SENSOR_ERROR;
    }
    return RC::OK;
}

// -----------------------------------------------------------------------------
//  Thermal
// -----------------------------------------------------------------------------

/* static */ RC Thermal::CreateSensors
(
    GpuSubdevice    *pSbdev,
    ThermalSensors  **pSensors,
    GpuBJTSensors   **pSensorsBjt,
    DramTempSensors **pSensorsDramTemp
)
{
    RC rc;

    *pSensors = nullptr;
    *pSensorsBjt = nullptr;
    *pSensorsDramTemp = nullptr;

    if (pSbdev->GetParentDevice()->GetFamily() >= GpuDevice::Ampere)
    {
        // Create BJT sensors for Ampere and beyond
        *pSensorsBjt = new GpuBJTSensors(&rc, pSbdev);
    }

    if (pSbdev->HasFeature(Device::GPUSUB_SUPPORTS_DRAM_TEMP))
    {
        // Create FBPA_FB sensors for GA10X
        *pSensorsDramTemp = new DramTempSensors(&rc, pSbdev);
    }

    //simulations or stand alone cheetah gpu
    if (Platform::GetSimulationMode() != Platform::Hardware ||
        pSbdev->HasFeature(Device::GPUSUB_IS_STANDALONE_TEGRA_GPU))
    {
        Printf(Tee::PriLow, "Creating StubSensors...\n");
        *pSensors = new StubSensors(&rc, pSbdev);
    }
    else if (pSbdev->IsSOC())
    {
        Printf(Tee::PriLow, "Creating TegraSensors...\n");
        *pSensors = new TegraSensors(&rc, pSbdev);
    }
    else
    {
        if (GpuDevice::Maxwell <= pSbdev->GetParentDevice()->GetFamily())
        {
            Printf(Tee::PriLow, "Creating TSM20Sensors...\n");
            *pSensors = new TSM20Sensors(&rc, pSbdev);
        }
        else
        {
            Printf(Tee::PriLow, "Creating ExelwteSensors...\n");
            *pSensors = new ExelwteSensors(&rc, pSbdev);
        }
    }
    (*pSensors)->PrintSensors(Tee::PriLow);

    return rc;
}

Thermal::Thermal(GpuSubdevice *pSubdev)
:m_pSubdevice(pSubdev)
,m_SbdevSensors(nullptr)
,m_OverTempCount(0)
,m_ExpectedOverTempCount(0)
,m_Hooked(false)
,m_IntOffset(0)
,m_ExtOffset(0)
,m_IntOffsetSet(false)
,m_ExtOffsetSet(false)
,m_PX3584Detected(false)
,m_PrimarionAddress(0xE0)
,m_WritesToPrimarionEnabled(false)
,m_ForceInternal(false)
,m_SmbusOffset(0)
,m_SmbusSensor(0)
,m_SmbAddr(0)
,m_MeasurementOffset(0)
,m_SmbusOffsetSet(false)
,m_SmbusInit(false)
,m_SmbDevName("")
,m_SmbConfigChanged(false)
,m_pSmbPort(NULL)
,m_pSmbMgr(NULL)
,m_IpmiOffset(0)
,m_IpmiSensor(-1)
,m_IpmiMeasurementOffset(0)
,m_IpmiOffsetSet(false)
,m_IpmiDevName("")
,m_VolterraInit(false)
,m_VolterraMaxTempDelta(15)
,m_OverTempCounter(pSubdev)
{
    MASSERT(pSubdev);

    // Prevent RM asserts on fmodel/emulation where I2C bus
    // doesn't operate as RM expects
    if (!pSubdev->IsEmOrSim()
    // Also don't try this on CheetAh
        && !Platform::UsesLwgpuDriver()
        && m_pSubdevice->SupportsInterface<I2c>())
    {
        RC rc;
        // Let see if this port is implemented before actually trying
        // to read from it.
        I2c::PortInfo PortInfo;
        rc = m_pSubdevice->GetInterface<I2c>()->GetPortInfo(&PortInfo);
        if ((rc == OK) && (PortInfo[3].Implemented))
        {
            UINT32 PX3584ICVersion;
            I2c::Dev i2cDev = m_pSubdevice->GetInterface<I2c>()->I2cDev(3, 0xE0);  // Port 3, addr 0xE0
            i2cDev.SetOffsetLength(2);
            i2cDev.SetMessageLength(2);
            rc = i2cDev.Read(0xFE49, &PX3584ICVersion);
            if ((rc == OK) && ((PX3584ICVersion & 0x00F0) == 0x0040))
            {
                m_PX3584Detected = true;
            }
        }
    }

    m_ADT7461OrigRegs.bSet = false;
    m_ADT7461ModsRegs.bSet = false;

    // Save initial chip settings
    for (UINT32 ii = 0; ii < NUMELEMS(s_ThermalSlowdownAlerts); ++ii)
        SaveInitialThermalLimit(s_ThermalSlowdownAlerts[ii].eventId);
    SaveInitialThermalLimit(LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_OVERT);
}

Thermal::~Thermal()
{
    if (m_Hooked)
    {
        m_pSubdevice->UnhookResmanEvent(LW2080_NOTIFIERS_THERMAL_SW);
        m_pSubdevice->UnhookResmanEvent(LW2080_NOTIFIERS_THERMAL_HW);
        m_Hooked = false;
    }

    // Restore initial chip settings
    for (map<UINT32,LwTemp>::iterator iter = m_InitialThermalLimits.begin();
         iter != m_InitialThermalLimits.end(); ++iter)
    {
        RestoreInitialThermalLimit(iter->first);
    }

    EnablePrimarionWrites(false);

    if (m_pSmbPort && m_SmbConfigChanged)
    {
        if ((m_SmbDevName == "ADT7461") || (m_SmbDevName == "TMP411"))
        {
            // When shutting down, MODS may be going from extended to normal
            // mode.  In this case, it is important to write the config
            // register first otherwise, writing the lower values into the
            // limit registers (used for normal mode) could cause a thermal
            // alert until the config is updated
            m_pSmbPort->WrByteCmd(m_SmbAddr,
                                  ADT7461_CONFIG_WR_REG,
                                  m_ADT7461OrigRegs.Config);
            m_pSmbPort->WrByteCmd(m_SmbAddr,
                                  ADT7461_EXT_LIMIT_WR_REG,
                                  m_ADT7461OrigRegs.ExtHighLimit);
            m_pSmbPort->WrByteCmd(m_SmbAddr,
                                  ADT7461_LOC_LIMIT_WR_REG,
                                  m_ADT7461OrigRegs.LocHighLimit);
            m_pSmbPort->WrByteCmd(m_SmbAddr,
                                  ADT7461_EXT_THRM_LIMIT_REG,
                                  m_ADT7461OrigRegs.ExtThermLimit);
            m_pSmbPort->WrByteCmd(m_SmbAddr,
                                  ADT7461_LOC_THRM_LIMIT_REG,
                                  m_ADT7461OrigRegs.LocThermLimit);
            m_pSmbPort->WrByteCmd(m_SmbAddr,
                                  ADT7461_EXT_OFFSET_REG,
                                  m_ADT7461OrigRegs.ExtOffset);
            m_pSmbPort->WrByteCmd(m_SmbAddr,
                                  ADT7461_HYSTERESIS_REG,
                                  m_ADT7461OrigRegs.Hysteresis);
        }
        m_SmbConfigChanged = false;
    }

    if (m_pSmbMgr)
    {
        m_pSmbMgr->Uninitialize();
        m_pSmbMgr->Forget();
        m_SmbusInit = false;
    }
}

RC Thermal::Initialize()
{
    return CreateSensors(m_pSubdevice,
                         &m_SbdevSensors,
                         &m_pSbdevSensorsBjt,
                         &m_pSbdevSensorsDramTemp);
}

RC Thermal::Cleanup()
{
    delete m_SbdevSensors;
    m_SbdevSensors = nullptr;

    if (m_pSbdevSensorsBjt)
    {
        delete m_pSbdevSensorsBjt;
        m_pSbdevSensorsBjt = nullptr;
    }
    if (m_pSbdevSensorsDramTemp)
    {
        delete m_pSbdevSensorsDramTemp;
        m_pSbdevSensorsDramTemp = nullptr;
    }

    return OK;
}

RC Thermal::GetTempViaSensor(ThermalSensors::Sensor sensor, FLOAT32 *pTemp)
{
    MASSERT(pTemp);
    RC rc;

    CHECK_RC(m_SbdevSensors->GetTemp(sensor, pTemp));

    return rc;
}

RC Thermal::GetAmbientTemp(FLOAT32 *pTemp)
{
    MASSERT(pTemp);
    RC rc;
    CHECK_RC(GetTempViaSensor(ThermalSensors::BOARD, pTemp));
    return rc;
}

RC Thermal::GetChipTempViaExt(FLOAT32 *pTemp)
{
    MASSERT(pTemp);
    RC rc;

    CHECK_RC(GetTempViaSensor(ThermalSensors::BOARD, pTemp));
    CHECK_RC(m_OverTempCounter.Update(OverTempCounter::EXTERNAL_TEMP,
        static_cast<INT32>(*pTemp + 0.5f)));

    return rc;
}

RC Thermal::GetChipTempViaInt(FLOAT32 *pTemp)
{
    MASSERT(pTemp);
    RC rc;

    CHECK_RC(GetTempViaSensor(ThermalSensors::GPU_AVG, pTemp));
    CHECK_RC(m_OverTempCounter.Update(OverTempCounter::INTERNAL_TEMP,
        static_cast<INT32>(*pTemp + 0.5f)));

    return rc;
}

RC Thermal::GetChipMemoryTemp(FLOAT32 *pTemp)
{
    MASSERT(pTemp);
    RC rc;

    CHECK_RC(GetTempViaSensor(ThermalSensors::MEMORY, pTemp));
    CHECK_RC(m_OverTempCounter.Update(OverTempCounter::MEMORY_TEMP,
        static_cast<INT32>(*pTemp + 0.5f)));

    return rc;
}

RC Thermal::GetChipTempsViaChannels(vector<ThermalSensors::ChannelTempData> *pTemp)
{
    MASSERT(pTemp);
    StickyRC rc;

    CHECK_RC(m_SbdevSensors->GetTempsViaChannels(pTemp));

    UINT32 chIdx;
    if (m_SbdevSensors->GetChannelIdx(ThermalSensors::MEMORY, &chIdx) == OK)
    {
        for (const auto& tempData : *pTemp)
        {
            if (tempData.chIdx == chIdx)
            {
                rc = m_OverTempCounter.Update(OverTempCounter::MEMORY_TEMP,
                    static_cast<INT32>(tempData.temp + 0.5f));
                break;
            }
        }
    }
    if (m_SbdevSensors->GetChannelIdx(ThermalSensors::GPU_AVG, &chIdx) == OK)
    {
        for (const auto& tempData : *pTemp)
        {
            if (tempData.chIdx == chIdx)
            {
                rc = (m_OverTempCounter.Update(OverTempCounter::INTERNAL_TEMP,
                    static_cast<INT32>(tempData.temp + 0.5f)));
                break;
            }
        }
    }

    return rc;
}

RC Thermal::GetChipTempViaChannel(UINT32 chIdx, ThermalSensors::ChannelTempData *pTemp)
{
    MASSERT(pTemp);
    RC rc;

    CHECK_RC(m_SbdevSensors->GetTempViaChannel(chIdx, pTemp));

    UINT32 monitorChIdx;
    if (m_SbdevSensors->GetChannelIdx(ThermalSensors::MEMORY, &monitorChIdx) == OK &&
        chIdx == monitorChIdx)
    {
        CHECK_RC(m_OverTempCounter.Update(OverTempCounter::MEMORY_TEMP,
            static_cast<INT32>(pTemp->temp + 0.5f)));
    }
    else if (m_SbdevSensors->GetChannelIdx(ThermalSensors::GPU_AVG, &monitorChIdx) == OK &&
             chIdx == monitorChIdx)
    {
        CHECK_RC(m_OverTempCounter.Update(OverTempCounter::INTERNAL_TEMP,
            static_cast<INT32>(pTemp->temp + 0.5f)));
    }

    return rc;
}

RC Thermal::SetChipTempViaChannel(UINT32 chnMask, FLOAT32 temp)
{
    RC rc;
    CHECK_RC(m_SbdevSensors->SetTempViaChannel(chnMask, temp));
    return rc;
}

RC Thermal::GetTsenseHotspotOffset(FLOAT32 *pOffset)
{
    MASSERT(pOffset);

    RC rc;
    SysInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_SENSOR_OPCODE;
    instruction.vec[0].operands.getInfoSensor.sensorIndex = ThermalSensors::GPU_AVG;
    CHECK_RC(ExelwteSystemInstructions(&instruction.vec));
    *pOffset = FromLwTemp(instruction.vec[0].operands.getInfoSensor.hotspotOffset);

    return rc;
}

RC Thermal::GetThermalSlowdownRatio(Gpu::ClkDomain domain, FLOAT32 *pRatio) const
{
    MASSERT(pRatio);

    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CMD_THERMAL_HWFS_SLOWDOWN_AMOUNT_GET_PARAMS params = { 0 };
    params.clkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(domain);
    CHECK_RC(pLwRm->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_THERMAL_HWFS_SLOWDOWN_AMOUNT_GET,
        &params, sizeof(params)));
    *pRatio = static_cast<FLOAT32>(params.numerator) /
              static_cast<FLOAT32>(params.denominator);

    return rc;
}

RC Thermal::SetSimulatedTemperature(UINT32 Sensor, INT32 Temp)
{
    RC rc;
    SysInstrs instruction(2);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_SUPPORTS_TEMPSIM_OPCODE;
    instruction.vec[0].operands.getInfoSupportsTempSim.sensorIndex = Sensor;
    instruction.vec[1].opcode = LW2080_CTRL_THERMAL_SYSTEM_SET_TEMPSIM_CONTROL_AND_TEMPERATURE_OPCODE;
    instruction.vec[1].operands.setTempSimControlAndTemperature.sensorIndex = Sensor;
    instruction.vec[1].operands.setTempSimControlAndTemperature.temperature = Temp;
    instruction.vec[1].operands.setTempSimControlAndTemperature.control = LW2080_CTRL_THERMAL_SYSTEM_TEMPSIM_CONTROL_ENABLE_YES;
    rc = ExelwteSystemInstructions(&instruction.vec);
    if (rc != OK)
    {
        if (instruction.vec[0].operands.getInfoSupportsTempSim.support != LW2080_CTRL_THERMAL_SYSTEM_TEMPSIM_SUPPORT_YES)
        {
            Printf(Tee::PriNormal, "Temp Sim is not supported on this sensor\n");
        }
    }
    return rc;
}

RC Thermal::ClearSimulatedTemperature(UINT32 Sensor)
{
    RC rc;
    SysInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_SYSTEM_SET_TEMPSIM_CONTROL_AND_TEMPERATURE_OPCODE;
    instruction.vec[0].operands.setTempSimControlAndTemperature.sensorIndex = Sensor;
    instruction.vec[0].operands.setTempSimControlAndTemperature.control = LW2080_CTRL_THERMAL_SYSTEM_TEMPSIM_CONTROL_ENABLE_NO;
    CHECK_RC(ExelwteSystemInstructions(&instruction.vec));
    return OK;
}

RC Thermal::GetExternalPresent(bool *pPresent)
{
    MASSERT(pPresent);
    return m_SbdevSensors->IsSensorPresent(ThermalSensors::BOARD, pPresent);
}

RC Thermal::GetInternalPresent(bool *pPresent)
{
    MASSERT(pPresent);
    return m_SbdevSensors->IsSensorPresent(ThermalSensors::GPU_AVG, pPresent);
}

RC Thermal::GetMemorySensorPresent(bool *pPresent)
{
    MASSERT(pPresent);
    return m_SbdevSensors->IsSensorPresent(ThermalSensors::MEMORY, pPresent);
}

RC Thermal::GetShouldExtBePresent(bool *pPresent)
{
    MASSERT(pPresent);
    return m_SbdevSensors->ShouldSensorBePresent(ThermalSensors::BOARD, pPresent);
}

RC Thermal::GetShouldIntBePresent(bool *pPresent)
{
    MASSERT(pPresent);
    return m_SbdevSensors->ShouldSensorBePresent(ThermalSensors::GPU_AVG, pPresent);
}

RC Thermal::GetPrimarySensorType(UINT32 *pType)
{
    MASSERT(pType);
    *pType = LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_GPU_AVG;
    return OK;
}

RC Thermal::GetPrimaryPresent(bool *pPresent)
{
    MASSERT(pPresent);
    return m_SbdevSensors->IsSensorPresent(ThermalSensors::GPU_AVG, pPresent);
}

RC Thermal::GetChipTempViaPrimary(FLOAT32 *pTemp)
{
    MASSERT(pTemp);
    RC rc;
    CHECK_RC(GetTempViaSensor(ThermalSensors::GPU_AVG, pTemp));
    return m_OverTempCounter.Update(OverTempCounter::INTERNAL_TEMP, static_cast<INT32>(*pTemp + 0.5f));
}

//--------------------------------------------------------------------
//! \brief Get the temperature at which RM sends a notifier to mods
//!
RC Thermal::GetHwfsEventInfo
(
    LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS *pInfo,
    UINT32 eventId
)
{
    MASSERT(pInfo);
    LwRmPtr pLwRm;
    RC rc;

    // Special case requested by RM: ignore any thermal event that
    // was initally set up with a limit of 0C or lower.  Those
    // limits were created for something other than overheating.
    if ((m_InitialThermalLimits.count(eventId)) &&
        (m_InitialThermalLimits[eventId] !=
             LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_TEMP_ILWALID) &&
        (m_InitialThermalLimits[eventId] <= 0))
    {
        return RC::LWRM_NOT_SUPPORTED;;
    }

    LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS params = {0};
    params.eventId = eventId;
    CHECK_RC(pLwRm->ControlBySubdevice(
            m_pSubdevice,
            LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_GET,
            &params, sizeof(params)));
    *pInfo = params;
    return rc;
}

RC Thermal::GetChipHighLimit(FLOAT32 *pTemp)
{
    MASSERT(pTemp);
    LwRmPtr pLwRm;
    RC rc;

    CHECK_RC(CheckIsSupported());
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return RC::UNSUPPORTED_FUNCTION;

    // Loop through all the thermal alerts in s_ThermalSlowdownAlerts,
    // and find the one with the lowest temperature limit.
    bool foundLimit = false;
    for (UINT32 ii = 0; ii < NUMELEMS(s_ThermalSlowdownAlerts); ++ii)
    {
        FLOAT32 limit;
        LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS pInfo = {0};
        rc = GetHwfsEventInfo(&pInfo, s_ThermalSlowdownAlerts[ii].eventId);
        if ((rc == RC::LWRM_NOT_SUPPORTED) ||
            (pInfo.temperature == LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_TEMP_ILWALID))
        {
            rc.Clear();
            continue;
        }
        limit = FromLwTemp(pInfo.temperature);
        if (!foundLimit || limit < *pTemp)
        {
            *pTemp = limit;
            foundLimit = true;
        }
    }

    // Return error if none of the thermal alerts had a temperature limit
    if (!foundLimit)
    {
        return RC::DEVICE_NOT_FOUND;
    }

    return rc;
}

RC Thermal::GetHasChipHighLimit(bool *pHighLimitSupported)
{
    FLOAT32 temp;
    RC rc;
    rc =  GetChipHighLimit(&temp);
    if (rc == RC::DEVICE_NOT_FOUND)
    {
        Printf(Tee::PriNormal,"ChipHighLimit is not supported on this device\n");
        *pHighLimitSupported = false;
        return OK;
    }
    else
        *pHighLimitSupported = true;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Get the temperature at which RM sends a notifier to mods
//!
RC Thermal::SetChipHighLimit(FLOAT32 Temp)
{
    LwRmPtr pLwRm;
    RC rc;

    CHECK_RC(CheckIsSupported());
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return RC::UNSUPPORTED_FUNCTION;

    // Find the lowest temperature limit in s_ThermalSlowdownAlerts
    //
    FLOAT32 oldLimit;
    CHECK_RC(GetChipHighLimit(&oldLimit));

    // Loop through all the thermal alerts in s_ThermalSlowdownAlerts,
    // and adjust all of the temperature limits up or down by the same
    // amount so that the lowest limit matches the Temp argument.
    //
    // Implementation note: Adjusting all the limits isn't strictly
    // necessary; all we really need is to make sure that all the
    // limits are >= Temp, with at least one being exactly equal.  But
    // adding the same offset to all of them makes it so that we
    // restore the original state if we do something like adjust the
    // limit up by 10 degrees, and then back down by 10 degrees.
    //
    for (UINT32 ii = 0; ii < NUMELEMS(s_ThermalSlowdownAlerts); ++ii)
    {
        UINT32 eventId = s_ThermalSlowdownAlerts[ii].eventId;

        // Special case requested by RM: ignore any thermal event that
        // was initally set up with a limit of 0C or lower.  Those
        // limits were created for something other than overheating.
        //
        if ((m_InitialThermalLimits.count(eventId)) &&
            (m_InitialThermalLimits[eventId] !=
                 LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_TEMP_ILWALID) &&
            (m_InitialThermalLimits[eventId] <= 0))
        {
            continue;
        }

        // Do a read-modify-write to change the alert's thermal limit,
        // while leaving everything else the same.
        //
        LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS params = {0};
        params.eventId = eventId;
        rc = pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_GET,
                &params, sizeof(params));
        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            rc.Clear();
            continue;
        }
        CHECK_RC(rc);
        if (params.temperature ==
            LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_TEMP_ILWALID)
        {
            continue;
        }

        params.temperature += ToLwTemp(Temp - oldLimit);
        CHECK_RC(pLwRm->ControlBySubdevice(
                    m_pSubdevice,
                    LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_SET,
                    &params, sizeof(params)));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get the temperature at which chip shuts down
//!
RC Thermal::GetChipShutdownLimit(FLOAT32 *pTemp)
{
    MASSERT(pTemp);
    LwRmPtr pLwRm;
    RC rc;

    CHECK_RC(CheckIsSupported());
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return RC::UNSUPPORTED_FUNCTION;

    // Read the OVERT (overtemp) alert
    //
    LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS params = {0};
    params.eventId = LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_OVERT;
    CHECK_RC(pLwRm->ControlBySubdevice(
                m_pSubdevice, LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_GET,
                &params, sizeof(params)));
    if (params.temperature == LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_TEMP_ILWALID)
        return RC::DEVICE_NOT_FOUND;

    *pTemp = FromLwTemp(params.temperature);
    return rc;
}

//--------------------------------------------------------------------
//! \brief Set the temperature at which chip shuts down
//!
RC Thermal::SetChipShutdownLimit(FLOAT32 Temp)
{
    LwRmPtr pLwRm;
    RC rc;

    CHECK_RC(CheckIsSupported());
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return RC::UNSUPPORTED_FUNCTION;

    // Do a read-modify-write to change the OVERT (overtemp) alert's
    // temperature limit while leaving the other info the same.
    //
    LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS params = {0};
    params.eventId = LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_OVERT;
    CHECK_RC(pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_GET,
                &params, sizeof(params)));
    params.temperature = ToLwTemp(Temp);
    CHECK_RC(pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_SET,
                &params, sizeof(params)));

    return rc;
}

RC Thermal::GetIsSupported(bool *pIsSupported)
{
    if (m_pSubdevice->IsSOC()
        || m_pSubdevice->HasFeature(Device::GPUSUB_IS_STANDALONE_TEGRA_GPU))
    {
        *pIsSupported = false;
    }
    else
    {
        *pIsSupported = true;
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return an error code if Thermal isn't supported
//
RC Thermal::CheckIsSupported()
{
    RC rc;
    bool isSupported;
    CHECK_RC(GetIsSupported(&isSupported));
    if (!isSupported)
        return RC::UNSUPPORTED_FUNCTION;
    return rc;
}

RC Thermal::ResetThermalTable()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Thermal::WriteThermalTable(UINT32 *pTable, UINT32 Size, UINT32 Version)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Thermal::GetVbiosExtTempOffset(INT32 *pOffset)
{
    return IntGetVbiosTempOffset(pOffset, true);
}

RC Thermal::GetVbiosIntTempOffset(INT32 *pOffset)
{
    return IntGetVbiosTempOffset(pOffset, false);
}

RC Thermal::IntGetVbiosTempOffset(INT32 *pOffset, bool IsExternal)
{
    MASSERT(pOffset);
    LwRmPtr pLwRm;
    LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_PARAMS Params = {0};
    Params.flags = LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_FLAGS_VBIOS;

    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);

    RC rc;
    CHECK_RC(pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_THERMAL_GET_THERMAL_TABLE,
                            &Params,
                            sizeof(Params)));

    bool Found = false;
    for (UINT32 i = 0; i < Params.entrycount; i++)
    {
        UINT32 entry = Params.entries[i];
        if ((entry & 0xFF) != LW_THERMALTABLE_V2_OPCODE_SENSOR_DDINFO)
            continue;

        if (DRF_VAL(_THERMALTABLE, _V2, _SENSOR_DDINFO_MA, entry) !=
            LW_THERMALTABLE_V2_SENSOR_DDINFO_MA_GPU)
            continue;

        if ((IsExternal && DRF_VAL(_THERMALTABLE, _V2, _SENSOR_DDINFO_DA, entry) ==
                            LW_THERMALTABLE_V2_SENSOR_DDINFO_DA_INTERNAL)  ||
            (!IsExternal && DRF_VAL(_THERMALTABLE, _V2, _SENSOR_DDINFO_DA, entry) !=
                            LW_THERMALTABLE_V2_SENSOR_DDINFO_DA_INTERNAL))
            continue;

        Found = true;

        INT32 Offset = DRF_VAL(_THERMALTABLE,
                              _V2, _SENSOR_DDINFO_CALIBRATION_OFFSET, entry);
        UINT32 Size = DRF_SIZE(LW_THERMALTABLE_V2_SENSOR_DDINFO_CALIBRATION_OFFSET);

        // Need to change 7 bit sign to 32 bit sign int ...
        if (Offset & (1 << (Size - 1)))
        {
            Offset = (-1 << Size) + Offset;
        }
        *pOffset = Offset;

        break;
    }

    if (!Found)
        return RC::SOFTWARE_ERROR;

    return rc;
}

RC Thermal::ReadThermalTable(UINT32 TableNum,
                             UINT32 *pTable,
                             UINT32 *pTableSize,
                             UINT32 *pFlag,
                             UINT32 *pVersion)
{
    MASSERT(pTable);
    MASSERT(pTableSize);
    MASSERT(pFlag);
    MASSERT(pVersion);

    RC rc = OK;
    LwRmPtr pLwRm;
    LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_PARAMS Params = {0};
    Params.flags = TableNum;

    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    rc = pLwRm->Control(
          pRmHandle,
          LW2080_CTRL_CMD_THERMAL_GET_THERMAL_TABLE,
          &Params,
          sizeof(Params));

    *pTableSize = Params.entrycount;
    *pFlag = Params.flags;
    *pVersion = Params.version;
    if (OK == rc)
    {
       for (UINT32 i = 0; i < Params.entrycount; i++)
       {
          pTable[i] = Params.entries[i];
       }
    }
    return rc;
}

RC Thermal::DumpThermalTable(UINT32 TableNumb)
{
    RC rc = OK;
    LwRmPtr pLwRm;
    LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_PARAMS Params = {0};
    Params.flags = TableNumb;

    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    rc = pLwRm->Control(
            pRmHandle,
            LW2080_CTRL_CMD_THERMAL_GET_THERMAL_TABLE,
            &Params,
            sizeof(Params));

    if (OK == rc)
    {
        Printf(Tee::PriNormal,
                "Dump Thermal Table: flags 0x%x, version %x.%x, size %d\n",
                int(Params.flags),
                int(Params.version >> 4),
                int(Params.version & 0xf),
                int(Params.entrycount));

        UINT32 i;
        for (i = 0; i < Params.entrycount; i++)
        {
            UINT32 entry = Params.entries[i];
            Printf(Tee::PriNormal, "    [%2d] = %06x ", i, entry);

            switch (entry & 0xFF)
            {
                case LW_THERMALTABLE_V2_OPCODE_SENSOR_DIINFO:
                {
                    const char *meas = "???";
                    switch (DRF_VAL(_THERMALTABLE, _V2, _SENSOR_DIINFO_MA, entry))
                    {
                        case LW_THERMALTABLE_V2_SENSOR_DIINFO_MA_GPU:       meas = "GPU"; break;
                        case LW_THERMALTABLE_V2_SENSOR_DIINFO_MA_AMBIENT:   meas = "Ambient"; break;
                        case LW_THERMALTABLE_V2_SENSOR_DIINFO_MA_VIDEOMEM:  meas = "VideoMem"; break;
                        case LW_THERMALTABLE_V2_SENSOR_DIINFO_MA_SLIBRIDGE: meas = "SLIBridge"; break;
                    }

                    bool publ = DRF_VAL(_THERMALTABLE, _V2, _SENSOR_DIINFO_PUBLIC, entry)
                        == LW_THERMALTABLE_V2_SENSOR_DIINFO_PUBLIC_YES;

                    Printf(Tee::PriNormal,
                        "SENSOR_DIINFO meas=%s public=%s",
                        meas,
                        publ ? "yes" : "no");
                    break;
                }
                case LW_THERMALTABLE_V2_OPCODE_SENSOR_DDINFO:
                {
                    const char *meas = "???";
                    switch (DRF_VAL(_THERMALTABLE, _V2, _SENSOR_DDINFO_MA, entry))
                    {
                        case LW_THERMALTABLE_V2_SENSOR_DDINFO_MA_GPU:       meas = "GPU"; break;
                        case LW_THERMALTABLE_V2_SENSOR_DDINFO_MA_AMBIENT:   meas = "Ambient"; break;
                        case LW_THERMALTABLE_V2_SENSOR_DDINFO_MA_VIDEOMEM:  meas = "VideoMem"; break;
                        case LW_THERMALTABLE_V2_SENSOR_DDINFO_MA_SLIBRIDGE: meas = "SLIBridge"; break;
                    }
                    const char *da = "???";
                    switch (DRF_VAL(_THERMALTABLE, _V2, _SENSOR_DDINFO_DA, entry))
                    {
                        case LW_THERMALTABLE_V2_SENSOR_DDINFO_DA_INTERNAL:           da = "Int"; break;
                        case LW_THERMALTABLE_V2_SENSOR_DDINFO_DA_EXTERNAL_FIRST:     da = "Ext First"; break;
                        case LW_THERMALTABLE_V2_SENSOR_DDINFO_DA_EXTERNAL_SBMAX6649: da = "Ext SBMAX6649"; break;
                        case LW_THERMALTABLE_V2_SENSOR_DDINFO_DA_EXTERNAL_AUTO:      da = "Ext Auto"; break;
                    }

                    bool ignore = DRF_VAL(_THERMALTABLE, _V2, _SENSOR_DDINFO_IGNORE, entry)
                        == LW_THERMALTABLE_V2_SENSOR_DDINFO_IGNORE_YES;
                    Printf(Tee::PriNormal,
                        "SENSOR_DDINFO meas=%s dev=%s ignore=%s",
                        meas,
                        da,
                        ignore ? "yes" : "no");
                    break;
                }
                case LW_THERMALTABLE_V2_OPCODE_FAN_INFO:
                    Printf(Tee::PriNormal, "FAN_INFO");
                    break;

                case LW_THERMALTABLE_V2_OPCODE_SENSOR_DDXINFO:
                    Printf(Tee::PriNormal, "SENSOR_DDXINFO 0x%04x",
                            (entry >> 8));
                    break;

                case LW_THERMALTABLE_V2_OPCODE_FANSPEED_MINMAX:
                    Printf(Tee::PriNormal, "FANSPEED_MINMAX min %d%% max %d%%",
                            (entry >> 8) & 0xFF,
                            (entry >> 16) & 0xFF);
                    break;

                case LW_THERMALTABLE_V2_OPCODE_FAN_FREQUENCY:
                    Printf(Tee::PriNormal, "FAN_FREQUENCY 0x%04x",
                            (entry >> 8));
                    break;

                case LW_THERMALTABLE_V2_OPCODE_HW_FEATURE_CONTROL:
                    Printf(Tee::PriNormal, "HW_FEATURE_CONTROL 0x%04x",
                            (entry >> 8));
                    break;

                case LW_THERMALTABLE_V2_OPCODE_EOT:
                    Printf(Tee::PriNormal, "EOT");
                    break;

                case LW_THERMALTABLE_V2_OPCODE_SKIP_ENTRY:
                    Printf(Tee::PriNormal, "skip");
                    break;

                case LW_THERMALTABLE_V2_OPCODE_FAN_RAMP_UP_SLOPE:
                    Printf(Tee::PriNormal, "FAN_RAMP_UP_SLOPE %d msec/%%",
                           (entry >> 8));
                    break;

                case LW_THERMALTABLE_V2_OPCODE_FAN_RAMP_DOWN_SLOPE:
                    Printf(Tee::PriNormal, "FAN_RAMP_DOWN_SLOPE %d msec/%%",
                           (entry >> 8));
                    break;

                case LW_THERMALTABLE_V2_OPCODE_FAN_LINEAR_CONTROL:
                    Printf(Tee::PriNormal,
                           "FAN_LINEAR_CONTROL temp %d C to %d C",
                           (entry >> 8) & 0xff,
                           (entry >> 16));
                    break;

                default:
                    Printf(Tee::PriNormal, "???");
                    break;
            }
            Printf(Tee::PriNormal, "\n");
        }
    }
    return rc;
}

static void OverTempHandler(void* Args)
{
    MASSERT(Args);
    GpuSubdevice *pSubdev = (GpuSubdevice*)Args;

    LwNotification notifyData;
    RC rc = pSubdev->GetResmanEventData(
        LW2080_NOTIFIERS_THERMAL_HW,
        &notifyData);
    if (rc == OK)
    {
        Thermal *pThermal = pSubdev->GetThermal();
        UINT32 eventsMask = notifyData.info32;
        UINT32 ignoreEventsMask = 0;
        rc = pThermal->GetHwfsEventIgnoreMask(&ignoreEventsMask);
        if (rc == OK)
        {
            for (UINT32 id = 0;
                 id < LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID__COUNT;
                 id++)
            {
                if (BIT(id) & eventsMask)
                {
                    if (BIT(id) & ignoreEventsMask)
                    {
                        Printf(Tee::PriNormal, "Ignored HWFS event %s\n",
                            Thermal::HwfsEventTypeStr(id));
                    }
                    else
                    {
                        pThermal->IncrementHwfsEventCounter(id);
                        pThermal->IncrementOverTempCounter();
                    }
                }
            }
        }
    }
}

void Thermal::PrintHwfsEventData()
{
    RC rc;
    for (auto& eventItr : m_HwfsEventCount)
    {
        Thermal::HwfsEventCategory category =
            GetHwfsEventCategory(eventItr.first);
        Printf(Tee::PriNormal, "\tType = %s, Category = %s,",
               HwfsEventTypeStr(eventItr.first),
               HwfsEventCategoryToStr(category));

        // Print temperature limits for temp based events
        if (category == Thermal::HwfsEventCategory::TEMP_BASED)
        {
            LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS pInfo = {0};
            rc = GetHwfsEventInfo(&pInfo, eventItr.first);
            if (rc != RC::LWRM_NOT_SUPPORTED)
            {
                Printf(Tee::PriNormal,
                       " Limit = %.2f degrees C, Sensor = %s,",
                       FromLwTemp(pInfo.temperature),
                       TSM20Sensors::ProviderIndexToString(
                       LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU,
                       pInfo.sensorId));
            }
        }

        Printf(Tee::PriNormal, " Count = %u\n", eventItr.second);
    }
}

RC Thermal::GetHwfsEventIgnoreMask(UINT32 *mask)
{
    MASSERT(mask);
    *mask = m_HwfsIgnoreMask;
    return OK;
}
RC Thermal::SetHwfsEventIgnoreMask(UINT32 mask)
{
    m_HwfsIgnoreMask = mask;
    return OK;
}

RC Thermal::InitOverTempCounter()
{
    RC rc;
    if (!m_Hooked)
    {
        m_Hooked = true;
        CHECK_RC(m_pSubdevice->HookResmanEvent(
                    LW2080_NOTIFIERS_THERMAL_SW,
                    OverTempHandler, m_pSubdevice,
                    LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                    GpuSubdevice::NOTIFIER_MEMORY_ENABLED));
        CHECK_RC(m_pSubdevice->HookResmanEvent(
                    LW2080_NOTIFIERS_THERMAL_HW,
                    OverTempHandler, m_pSubdevice,
                    LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                    GpuSubdevice::NOTIFIER_MEMORY_ENABLED));
    }
    return rc;
}

RC Thermal::ShutdownOverTempCounter()
{
    StickyRC rc;
    if (m_Hooked)
    {
        rc = m_pSubdevice->UnhookResmanEvent(LW2080_NOTIFIERS_THERMAL_SW);
        rc = m_pSubdevice->UnhookResmanEvent(LW2080_NOTIFIERS_THERMAL_HW);
        m_Hooked = false;
    }
    return rc;
}

RC Thermal::GetOverTempCounter(UINT32 *pCount)
{
    MASSERT(pCount);
    *pCount = m_OverTempCount;
    return OK;
}

RC Thermal::GetExpectedOverTempCounter(UINT32 *pCount)
{
    MASSERT(pCount);
    *pCount = m_ExpectedOverTempCount;
    return OK;
}

void Thermal::IncrementOverTempCounter()
{
    if(!m_OverTempCounter.IsExpecting(OverTempCounter::RM_EVENT))
        m_OverTempCount++;
    else
        m_ExpectedOverTempCount++;
}

void Thermal::IncrementHwfsEventCounter(UINT32 eventId)
{
    m_HwfsEventCount[eventId]++;
}

RC Thermal::GetClockThermalSlowdownAllowed(bool *pAllowed)
{
    MASSERT(pAllowed);

    // Thermal slowdown allowed doesn't work without real hardware
    if (m_pSubdevice->IsEmOrSim())
    {
        // Default value for ThermalSlowdownAllowed is true
        *pAllowed = true;
        return OK;
    }

    RC rc;

    vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> instructions;

    LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION instr = {0};
    instr.opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_SLOWDOWN_STATE_OPCODE;

    instructions.push_back(instr);
    CHECK_RC(ExelwteSystemInstructions(&instructions));

    *pAllowed =
        instructions.back().operands.getThermalSlowdownState.slowdownState ==
        LW2080_CTRL_THERMAL_SLOWDOWN_ENABLED;

    return rc;
}

RC Thermal::SetClockThermalSlowdownAllowed(bool allowed)
{
    // Thermal slowdown allowed doesn't work without real hardware
    if (m_pSubdevice->IsEmOrSim())
    {
        return OK;
    }

    RC rc;

    vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> instructions;

    LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION instr = {0};
    instr.opcode = LW2080_CTRL_THERMAL_SYSTEM_SET_SLOWDOWN_STATE_OPCODE;

    instr.operands.setThermalSlowdownState.slowdownState =
        (allowed ?
        LW2080_CTRL_THERMAL_SLOWDOWN_ENABLED :
        LW2080_CTRL_THERMAL_SLOWDOWN_DISABLED_ALL);

    instructions.push_back(instr);
    rc = ExelwteSystemInstructions(&instructions);
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriWarn, "Cannot %sable thermal slowdown\n", allowed ? "en" : "dis");
        rc.Clear();
    }

    return rc;
}

RC Thermal::ExelwteCoolerInstruction
(
    vector<LW2080_CTRL_THERMAL_COOLER_INSTRUCTION> * pInstruction
)
{
    RC rc;
    bool supportsMultiFan = false;
    CHECK_RC(SupportsMultiFan(&supportsMultiFan));
    if (supportsMultiFan)
    {
        Printf(Tee::PriError, "Function not supported from fan 2.0\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (pInstruction->size() < 1)
    {
        return RC::BAD_PARAMETER;
    }
    LW2080_CTRL_THERMAL_COOLER_EXELWTE_PARAMS params = {0};
    params.clientAPIVersion        = THERMAL_COOLER_API_VER;
    params.clientAPIRevision       = THERMAL_COOLER_API_REV;
    params.clientInstructionSizeOf = sizeof(LW2080_CTRL_THERMAL_COOLER_INSTRUCTION);
    params.exelwteFlags = LW2080_CTRL_THERMAL_COOLER_EXELWTE_FLAGS_DEFAULT;
    params.instructionListSize     = (LwU32)pInstruction->size();
    params.instructionList = LW_PTR_TO_LwP64(&((*pInstruction)[0]));

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);

    rc = pLwRm->Control(
            pRmHandle,
            LW2080_CTRL_CMD_THERMAL_COOLER_EXELWTE,
            &params,
            sizeof(params));

    const UINT08 * pData = (const UINT08 *)(&(*pInstruction)[0]);
    for (UINT32 i=0; i < pInstruction->size(); i++)
    {
        Printf(Tee::PriDebug,
                "\tInstruction %u:\n"
                "\t    opcode   0x%x\n"
                "\t    exelwted %s\n"
                "\t    result   0x%x\n",
                i,
                (*pInstruction)[i].opcode,
                (*pInstruction)[i].exelwted ? "true" : "false",
                (UINT32)(*pInstruction)[i].result);
        const UINT32 instrSize = sizeof((*pInstruction)[0]);
        for (UINT32 j=0; j < instrSize; j++)
        {
            if (0 == j % 16)
            {
                Printf(Tee::PriDebug, "\t    raw =");
            }
            if (0 == j % 4)
            {
                Printf(Tee::PriDebug, " ");
            }

            Printf(Tee::PriDebug, "%02x ", pData[j + instrSize*i]);

            if ((j+1 == instrSize) || (0 == (j+1)%16))
            {
                Printf(Tee::PriDebug, "\n");
            }
        }
    }
    if (OK == rc)
    {
        bool execError = false;
        for (UINT32 i = 0; i < pInstruction->size(); i++)
        {
            if (!(*pInstruction)[i].exelwted)
            {
                execError = true;
                Printf(Tee::PriLow, "Fan instruction 0x%x at index %u was not exelwted\n",
                        (UINT32)(*pInstruction)[i].opcode, i);
            }
            if ((*pInstruction)[i].result)
            {
                execError = true;
                Printf(Tee::PriLow, "Fan instruction 0x%x at index %u failed with error 0x%x\n",
                       (unsigned int) (*pInstruction)[i].opcode,
                       (unsigned int) i,
                       (unsigned int) (*pInstruction)[i].result);
            }
        }
        if (execError)
        {
            rc = RC::LWRM_ERROR;
        }
    }

    return rc;
}

RC Thermal::GetPmuFanCtrlBlk(LW2080_CTRL_PMU_FAN_CONTROL_BLOCK *pFCB)
{
    MASSERT(pFCB);
    *pFCB = {{0}};

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_PMU_FAN_CONTROL_BLOCK_OPCODE;
    instruction.vec[0].operands.getPmuFanControlBlock.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pFCB = instruction.vec[0].operands.getPmuFanControlBlock.pmuFanControlBlock;
    return rc;
}

RC Thermal::SetPmuFanCtrlBlk(const LW2080_CTRL_PMU_FAN_CONTROL_BLOCK &FCB)
{
    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_SET_PMU_FAN_CONTROL_BLOCK_OPCODE;
    instruction.vec[0].operands.setPmuFanControlBlock.pmuFanControlBlock = FCB;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    return rc;
}

RC Thermal::GetCoolerPolicy(UINT32 * pPolicy)
{
    MASSERT(pPolicy);
    *pPolicy = (std::numeric_limits<INT32>::max)();

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_STATUS_POLICY_OPCODE;
    instruction.vec[0].operands.getStatusPolicy.coolerIndex = 0; // gpu
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pPolicy = instruction.vec[0].operands.getStatusPolicy.policy;
    return rc;
}

RC Thermal::SetCoolerPolicy(UINT32 policy)
{
    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_SET_CONTROL_POLICY_OPCODE;
    instruction.vec[0].operands.setControlPolicy.coolerIndex = 0; // gpu
    instruction.vec[0].operands.setControlPolicy.policy = policy;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    return rc;
}

RC Thermal::GetDefaultMinimumFanSpeed(UINT32 * pPct)
{
    MASSERT(pPct);
    *pPct = (std::numeric_limits<INT32>::max)();

    // Set up GET_INFO_LEVEL_MINIMUM_DEFAULT operation
    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_LEVEL_MINIMUM_DEFAULT_OPCODE;
    instruction.vec[0].operands.getInfoLevelMinimumDefault.coolerIndex = 0; // gpu

    // Execute instruction and retrieve default minimum fan speed
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pPct = instruction.vec[0].operands.getInfoLevelMinimumDefault.level;

    return rc;
}

RC Thermal::GetDefaultMaximumFanSpeed(UINT32 * pPct)
{
    MASSERT(pPct);
    *pPct = (std::numeric_limits<INT32>::max)();

    // Set up GET_INFO_LEVEL_MAXIMUM_DEFAULT operation
    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_LEVEL_MAXIMUM_DEFAULT_OPCODE;
    instruction.vec[0].operands.getInfoLevelMaximumDefault.coolerIndex = 0; // gpu

    // Execute instruction and retrieve default maximum fan speed
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pPct = instruction.vec[0].operands.getInfoLevelMaximumDefault.level;

    return rc;
}

RC Thermal::GetMinimumFanSpeed(UINT32 * pPct)
{
    MASSERT(pPct);
    *pPct = (std::numeric_limits<INT32>::max)();

    // Set up GET_STATUS_LEVEL_MINIMUM operation
    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_STATUS_LEVEL_MINIMUM_OPCODE;
    instruction.vec[0].operands.getStatusLevelMinimum.coolerIndex = 0; // gpu

    // Execute instruction and retrieve minimum fan speed
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pPct = instruction.vec[0].operands.getStatusLevelMinimum.level;

    return rc;
}

RC Thermal::GetMaximumFanSpeed(UINT32 * pPct)
{
    MASSERT(pPct);
    *pPct = (std::numeric_limits<INT32>::max)();

    // Set up GET_STATUS_LEVEL_MAXIMUM operation
    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_STATUS_LEVEL_MAXIMUM_OPCODE;
    instruction.vec[0].operands.getStatusLevelMaximum.coolerIndex = 0; // gpu

    // Execute instruction and retrieve maximum fan speed
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pPct = instruction.vec[0].operands.getStatusLevelMaximum.level;

    return rc;
}

RC Thermal::GetFanSpeed(UINT32 * pPct)
{
    MASSERT(pPct);
    *pPct = (std::numeric_limits<INT32>::max)();

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_STATUS_LEVEL_OPCODE;
    instruction.vec[0].operands.getStatusLevel.coolerIndex = 0; // gpu
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pPct = instruction.vec[0].operands.getStatusLevel.level;
    return rc;
}

RC Thermal::GetIsFanActivitySenseSupported(bool * isFanActivitySenseSupported)
{
    MASSERT(isFanActivitySenseSupported);
    *isFanActivitySenseSupported = false;

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode =
        LW2080_CTRL_THERMAL_COOLER_GET_INFO_TACHOMETER_OPCODE;
    instruction.vec[0].operands.getInfoTachometer.coolerIndex = 0; // gpu
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *isFanActivitySenseSupported =
            instruction.vec[0].operands.getInfoTachometer.bSupported != 0;
    return rc;
}

RC Thermal::ResetCoolerSettings()
{
    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode =
        LW2080_CTRL_THERMAL_COOLER_SET_CONTROL_DEFAULTS_OPCODE;
    instruction.vec[0].operands.setControlDefaults.coolerIndex = 0;
    instruction.vec[0].operands.setControlDefaults.defaultsType =
        LW2080_CTRL_THERMAL_COOLER_SET_CONTROL_DEFAULTS_ALL;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    return OK;
}

RC Thermal::GetIsFanSpinning(bool * isFanSpinning)
{
    MASSERT(isFanSpinning);
    *isFanSpinning = false;

    RC rc;
    bool fanActivitySenseSupported;
    rc = GetIsFanActivitySenseSupported(&fanActivitySenseSupported);
    if (rc != OK)
    {
        *isFanSpinning = false;
        Printf(Tee::PriError, "Failed to determine if activity sense is supprted.\n");
        return rc;
    }

    if (fanActivitySenseSupported)
    {
        Printf(Tee::PriDebug, "\nSensing of fan spin is ENABLED.\n");
        rc = ResetCoolerSettings();
        if (OK == rc)
        {
            Tasker::Sleep(1000);
            CoolerInstrs instruction(1);
            memset(&instruction.vec[0], 0, sizeof(instruction.vec[0]));
            instruction.vec[0].opcode =
                    LW2080_CTRL_THERMAL_COOLER_GET_STATUS_TACHOMETER_OPCODE;
            instruction.vec[0].operands.getStatusTachometer.coolerIndex = 0;

            CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
            *isFanSpinning =
                instruction.vec[0].operands.getStatusTachometer.speedRPM != 0;
        }
        else
        {
            Printf(Tee::PriError, "Failed to reset activity sense control state.\n");
            *isFanSpinning = false;
        }
    }
    else
    {
        rc = RC::UNSUPPORTED_FUNCTION;
        *isFanSpinning = false;
        Printf(Tee::PriDebug, "\nSensing of fan spin is DISABLED.\n");
    }

    return rc;
}

RC Thermal::SupportsMultiFan(bool * pSupportsMultiFan)
{
    RC rc;
    UINT32 policyMask;
    UINT32 coolerMask;
    CHECK_RC(GetFanPolicyMask(&policyMask));
    CHECK_RC(GetFanCoolerMask(&coolerMask));

    // A non zero mask means Fan 2.0+ is configured
    *pSupportsMultiFan = false;
    if (policyMask > 0 && coolerMask > 0)
    {
        *pSupportsMultiFan = true;
    }
    return rc;
}

RC Thermal::GetIsFanRPMSenseSupported(bool * isRPMSenseSupported)
{
    MASSERT(isRPMSenseSupported);
    *isRPMSenseSupported = false;

    RC rc;
    bool supportsMultiFan = false;
    CHECK_RC(SupportsMultiFan(&supportsMultiFan));
    if (supportsMultiFan)
    {
        LW2080_CTRL_FAN_COOLER_INFO_PARAMS infoParams = {};
        LW2080_CTRL_FAN_COOLER_STATUS_PARAMS statusParams = {};
        LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS ctrlParams = {};
        ctrlParams.bDefault = true;
        CHECK_RC(GetFanCoolerTable(&infoParams, &statusParams, &ctrlParams));
        INT32 count = 0;
        for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
        {
            if (!(BIT(index) & infoParams.coolerMask))
            {
                continue;
            }
            bool bTachSupported = false;
            switch(infoParams.coolers[index].type)
            {
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                    bTachSupported =
                        (infoParams.coolers[index].data.active.bTachSupported != LW_FALSE);
                    break;
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                    bTachSupported =
                        (infoParams.coolers[index].data.activePwm.active.bTachSupported != LW_FALSE);
                    break;
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                    bTachSupported =
                        (infoParams.coolers[index].data.activePwmTachCorr.activePwm.active.bTachSupported != LW_FALSE);
                    break;
            }
            if (bTachSupported)
            {
                count++;
            }
        }
        *isRPMSenseSupported = Utility::CountBits(infoParams.coolerMask) == count ?
                               true : false;
    }
    else
    {
        CoolerInstrs instruction(1);
        instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_TACHOMETER_OPCODE;
        instruction.vec[0].operands.getInfoTachometer.coolerIndex = 0;
        CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
        *isRPMSenseSupported = (instruction.vec[0].operands.getInfoTachometer.bSupported) ? true : false;
    }
    return rc;
}

RC Thermal::GetFanControlSignal(UINT32 *pSignalType)
{
    MASSERT(pSignalType);
    *pSignalType = (std::numeric_limits<INT32>::max)();

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_SIGNAL_OPCODE;
    instruction.vec[0].operands.getInfoSignal.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pSignalType = instruction.vec[0].operands.getInfoSignal.signal;
    return rc;
}

RC Thermal::GetFanTachRPM(UINT32 * rpm)
{
    MASSERT(rpm);
    *rpm = (std::numeric_limits<INT32>::max)();

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_STATUS_TACHOMETER_OPCODE;
    instruction.vec[0].operands.getStatusTachometer.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *rpm = instruction.vec[0].operands.getStatusTachometer.speedRPM;
    return rc;
}

RC Thermal::GetFanTachExpectedRPMSupported(bool *pbSupported)
{
    MASSERT(pbSupported);
    *pbSupported = false;

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_TACHOMETER_OPCODE;
    instruction.vec[0].operands.getInfoTachometer.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pbSupported = (instruction.vec[0].operands.getInfoTachometer.bExpectedRPMSupported) ?
                        true : false;
    return rc;
}

RC Thermal::GetFanTachMinRPM(UINT32 *pMinRPM)
{
    MASSERT(pMinRPM);
    *pMinRPM = (std::numeric_limits<INT32>::max)();

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_TACHOMETER_OPCODE;
    instruction.vec[0].operands.getInfoTachometer.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pMinRPM = instruction.vec[0].operands.getInfoTachometer.minSpeedRPM;
    return rc;
}

RC Thermal::GetFanTachMinPct(UINT32 *pMinPct)
{
    MASSERT(pMinPct);
    *pMinPct = (std::numeric_limits<INT32>::max)();

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_TACHOMETER_OPCODE;
    instruction.vec[0].operands.getInfoTachometer.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pMinPct = instruction.vec[0].operands.getInfoTachometer.minSpeedPct;
    return rc;
}

RC Thermal::GetFanTachMaxRPM(UINT32 *pMaxRPM)
{
    MASSERT(pMaxRPM);
    *pMaxRPM = (std::numeric_limits<INT32>::max)();

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_TACHOMETER_OPCODE;
    instruction.vec[0].operands.getInfoTachometer.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pMaxRPM = instruction.vec[0].operands.getInfoTachometer.maxSpeedRPM;
    return rc;
}

RC Thermal::GetFanTachMaxPct(UINT32 *pMaxPct)
{
    MASSERT(pMaxPct);
    *pMaxPct = (std::numeric_limits<INT32>::max)();

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_TACHOMETER_OPCODE;
    instruction.vec[0].operands.getInfoTachometer.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pMaxPct = instruction.vec[0].operands.getInfoTachometer.maxSpeedPct;
    return rc;
}

RC Thermal::GetFanTachEndpointError(JSObject *pEndpointError)
{
    MASSERT(pEndpointError);
    JavaScriptPtr pJs;
    RC rc;
    CHECK_RC(pJs->SetProperty(pEndpointError, "HighEnd", (std::numeric_limits<INT32>::max)()));
    CHECK_RC(pJs->SetProperty(pEndpointError, "LowEnd", (std::numeric_limits<INT32>::max)()));

    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_TACHOMETER_OPCODE;
    instruction.vec[0].operands.getInfoTachometer.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    UINT32 endmargin =
        static_cast<UINT32> (instruction.vec[0].operands.getInfoTachometer.highEndpointExpectedErrorPct);
    CHECK_RC(pJs->SetProperty(pEndpointError, "HighEnd", endmargin));
    endmargin =
        static_cast<UINT32> (instruction.vec[0].operands.getInfoTachometer.lowEndpointExpectedErrorPct);
    CHECK_RC(pJs->SetProperty(pEndpointError, "LowEnd", endmargin));
    return rc;
}

RC Thermal::GetFanTachInterpolationError(UINT32 *pInterpolationError)
{
    MASSERT(pInterpolationError);
    *pInterpolationError = (std::numeric_limits<INT32>::max)();

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_TACHOMETER_OPCODE;
    instruction.vec[0].operands.getInfoTachometer.coolerIndex = 0;
    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));
    *pInterpolationError = instruction.vec[0].operands.getInfoTachometer.interpolationExpectedErrorPct;
    return rc;
}

RC Thermal::SetMinimumFanSpeed(UINT32 pct)
{
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_SET_CONTROL_LEVEL_MINIMUM_OPCODE;
    instruction.vec[0].operands.setControlLevelMinimum.coolerIndex = 0; // gpu
    instruction.vec[0].operands.setControlLevelMinimum.level = pct;

    return ExelwteCoolerInstruction(&instruction.vec);
}

RC Thermal::SetMaximumFanSpeed(UINT32 pct)
{
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_SET_CONTROL_LEVEL_MAXIMUM_OPCODE;
    instruction.vec[0].operands.setControlLevelMaximum.coolerIndex = 0; // gpu
    instruction.vec[0].operands.setControlLevelMaximum.level = pct;

    return ExelwteCoolerInstruction(&instruction.vec);
}

RC Thermal::SetFanSpeed(UINT32 pct)
{
    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_SET_CONTROL_LEVEL_OPCODE;
    instruction.vec[0].operands.setControlLevel.coolerIndex = 0; // gpu
    instruction.vec[0].operands.setControlLevel.level = pct;

    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));

    if( rc == OK)
    {
        // Check actual speed of fan - a set below its minimum speed will succeed, but run @ minimum speed.
        UINT32 realSpeed;

        if( GetFanSpeed(&realSpeed) == OK )
        {
            Printf(Tee::PriLow, "THERMAL: Fan speed set to %d%%, running @ %d%%\n", pct, realSpeed);
        }
        else
        {
            Printf(Tee::PriLow, "THERMAL: Fan speed set to %d%%, unable to retreive actual current fan speed\n", pct);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the number of coolers that are available
//!
//! \param pCoolerCount  : Returned value for the number of available coolers
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::GetCoolerCount(UINT32 *pCoolerCount)
{
    MASSERT(pCoolerCount);

    if (m_pSubdevice->IsEmOrSim())
    {
        *pCoolerCount = 0;
        return OK;
    }

    // For now this is not supported on SOC GPUs
    if (m_pSubdevice->IsSOC())
    {
        *pCoolerCount = 0;
        return OK;
    }

    RC rc;
    CoolerInstrs instruction(1);
    instruction.vec[0].opcode = LW2080_CTRL_THERMAL_COOLER_GET_INFO_AVAILABLE_OPCODE;
    instruction.vec[0].operands.getInfoAvailable.availableCoolers = 0;

    CHECK_RC(ExelwteCoolerInstruction(&instruction.vec));

    *pCoolerCount = instruction.vec[0].operands.getInfoAvailable.availableCoolers;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Execute the specified thermal system instructions
//!
//! \param pInstructions  : Pointer to a list of instructions to execute
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::ExelwteSystemInstructions
(
    vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> * pInstructions
)
{
    return ExelwteSystemInstructions(pInstructions, NULL);
}

//-----------------------------------------------------------------------------
//! \brief Execute the specified thermal system instructions
//!
//! \param pInstructions  : Pointer to a list of instructions to execute
//! \param pbExelwteFailed : Returned bool indicating if any instruction failed
//!
//! \return OK if successfull, translated RM error code otherwise
RC Thermal::ExelwteSystemInstructions
(
    vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> * pInstructions,
    bool *pbExelwteFailed
)
{
    if (pbExelwteFailed)
       *pbExelwteFailed = false;

    if (pInstructions->size() < 1)
    {
        return RC::BAD_PARAMETER;
    }

    LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_V2_PARAMS params = {0};
    params.clientAPIVersion        = THERMAL_SYSTEM_API_VER;
    params.clientAPIRevision       = THERMAL_SYSTEM_API_REV;
    params.clientInstructionSizeOf = sizeof(LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION);
    params.exelwteFlags = DRF_DEF(2080_CTRL_THERMAL, _SYSTEM_EXELWTE_FLAGS, _INTERNAL, _TRUE);
    //params.successfulInstructions = 0;

    if (pInstructions->size() > NUMELEMS(params.instructionList))
    {
        Printf(Tee::PriError,
               "Number of opcodes %zu exceeds max %zu\n",
               pInstructions->size(), NUMELEMS(params.instructionList));
        return LW_ERR_INSUFFICIENT_RESOURCES;
    }

    params.instructionListSize     = (LwU32)pInstructions->size();
    memcpy(&params.instructionList, &((*pInstructions)[0]),
           pInstructions->size() * sizeof(params.instructionList[0]));

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);

    RC rc = pLwRm->Control(
         pRmHandle,
         LW2080_CTRL_CMD_THERMAL_SYSTEM_EXELWTE_V2,
         &params,
         sizeof(params));

    memcpy(&((*pInstructions)[0]), &params.instructionList,
           pInstructions->size() * sizeof(params.instructionList[0]));

    bool bAllExelwted = true;
    bool bExelwteFailed = false;
    RC   retval = OK;
    for (UINT32 lwrInst = 0; lwrInst < pInstructions->size(); lwrInst++)
    {
        const UINT08 * pData = (const UINT08 *)&((*pInstructions)[lwrInst]);
        UINT32 i;
        for(i = 0; i < params.clientInstructionSizeOf; i++)
        {
            if (0 == i % 16)
            {
                Printf(Tee::PriDebug, "\n\tInstruction[%d]  =", lwrInst);
            }
            if (0 == i % 4)
                Printf(Tee::PriDebug, " ");

            Printf(Tee::PriDebug, "%02x ", pData[i]);
        }
        if (!(*pInstructions)[lwrInst].exelwted)
        {
            bAllExelwted = false;
        }
        else if ((*pInstructions)[lwrInst].result)
        {
            bExelwteFailed = true;
            // will report the first ever error code
            retval = (retval == OK ) ?
                RmApiStatusToModsRC((*pInstructions)[lwrInst].result) :
                retval;
        }
    }
    if ((OK == rc) && (!bAllExelwted || bExelwteFailed))
    {
        // Only want to set the value of pbExelwteFailed if the RM call
        // succeeded but the exelwtion of the instruction failed
        if (pbExelwteFailed)
           *pbExelwteFailed = bExelwteFailed;
        rc = (retval == OK) ? RC::LWRM_ERROR : retval;
    }

    return rc;
}

RC Thermal::EnablePrimarionWrites(bool enable)
{
    RC rc;

    if (m_WritesToPrimarionEnabled == enable)
        return OK;

    I2c::Dev i2cDev = m_pSubdevice->GetInterface<I2c>()->I2cDev(3, m_PrimarionAddress);
    CHECK_RC(i2cDev.SetOffsetLength(2));
    CHECK_RC(i2cDev.SetMessageLength(2));
    CHECK_RC(i2cDev.Write(0xD102, enable ? 0x3D45 : 0x7777));

    m_WritesToPrimarionEnabled = enable;

    return rc;
}

RC Thermal::GetPhaseLwrrent(UINT32 PhaseIdx, INT32 *PhaseMiliAmps)
{
    // Don't test m_PX3584Detected in case detection failure
    // was intermittent, but we should still be able to execute
    // the Primarion Phase test
    RC rc;

    I2c::Dev i2cDev = m_pSubdevice->GetInterface<I2c>()->I2cDev(3, m_PrimarionAddress);
    CHECK_RC(i2cDev.SetOffsetLength(1));
    CHECK_RC(i2cDev.SetMessageLength(2));
    CHECK_RC(i2cDev.SetMsbFirst(false));

    if (PhaseIdx >= 6)
        return RC::SOFTWARE_ERROR; // Up to 6 phases are supported

    MASSERT(PhaseMiliAmps != NULL);
    *PhaseMiliAmps = 0;

    const UINT32 ThresholdmA = 30*1000;
    const UINT32 ThresholdmAdiv62_5 = UINT32(ThresholdmA/62.5);

    CHECK_RC(EnablePrimarionWrites(true));
    CHECK_RC(i2cDev.Write(0xE8, 0x0000));    // ResetSnapshotTrigger
    CHECK_RC(i2cDev.Write(0xE9, 0x0000));    // SnapDelay
    CHECK_RC(i2cDev.Write(0xE7, ThresholdmAdiv62_5));
    CHECK_RC(i2cDev.Write(0xE8, (((PhaseIdx + 1) & 0x0F) << 8) + 0x88));
                                             // TriggerAboveThreshold

    Tasker::Sleep(20);

    UINT32 TriggeredBytes;
    CHECK_RC(i2cDev.Read(0xA0, &TriggeredBytes));

    if ( (TriggeredBytes & 0x4000) == 0)
    {
        // Snapshot has not triggered
        *PhaseMiliAmps = 0;
        return rc;
    }

    CHECK_RC(i2cDev.Write(0xE8, (((PhaseIdx + 1) & 0x0F) << 8) + 0x08));
                                             // SelectNewestBSample

    UINT32 Value;
    CHECK_RC(i2cDev.Read(0xA0, &Value));
    Value &= 0xFFF;
    if (Value & 0x800) // sign
        Value |= 0xFFFFF800;

    *PhaseMiliAmps = INT32(62.5f * Value);

    return OK;
}

RC Thermal::SetExtTempOffset(INT32 Offset)
{
    m_ExtOffset = Offset;
    m_ExtOffsetSet = true;
    return OK;
}
RC Thermal::SetIntTempOffset(INT32 Offset)
{
    m_IntOffset = Offset;
    m_IntOffsetSet = true;
    return OK;
}
RC Thermal::GetForceInt(bool *pForced)
{
    MASSERT(pForced);
    *pForced = m_IntOffsetSet;
    return OK;
}
RC Thermal::GetForceExt(bool *pForced)
{
    MASSERT(pForced);

    *pForced = m_ExtOffsetSet;
    return OK;
}
RC Thermal::GetForceSmbus(bool *pForced)
{
    MASSERT(pForced);
    *pForced = m_SmbusOffsetSet;
    return OK;
}
RC Thermal::GetExtTempOffset(INT32 *pOffset)
{
    MASSERT(pOffset);
    *pOffset = m_ExtOffset;
    return OK;
}
RC Thermal::GetIntTempOffset(INT32 *pOffset)
{
    MASSERT(pOffset);
    *pOffset = m_IntOffset;
    return OK;
}

RC Thermal::SetSmbusTempOffset(INT32 Offset)
{
    m_SmbusOffset = Offset;
    m_SmbusOffsetSet = true;
    return OK;
}

RC Thermal::GetSmbusTempOffset(INT32 *pOffset)
{
    MASSERT(pOffset);
    *pOffset = m_SmbusOffset;
    return OK;
}

RC Thermal::SetSmbusTempSensor(INT32 Sensor)
{
    if (m_SmbDevName == "")
    {
        Printf(Tee::PriError,
               "Smbus device must be set before setting the sensor!\n");
        return RC::SOFTWARE_ERROR;
    }

    bool bUnknownSensor = false;
    if (m_SmbDevName == "ADT7473")
    {
        switch (Sensor)
        {
            case 0:
                m_SmbusSensor = ADT7473_LOCAL_TEMP_REG;
                return OK;
            case 1:
                m_SmbusSensor = ADT7473_REMOTE_TEMP1_REG;
                return OK;
            case 2:
                m_SmbusSensor = ADT7473_REMOTE_TEMP2_REG;
                return OK;
            default:
                bUnknownSensor = true;
                break;
        }
    }
    else if ((m_SmbDevName == "ADT7461") || (m_SmbDevName == "TMP411"))
    {
        switch (Sensor)
        {
            case 0:
                m_SmbusSensor = ADT7461_LOCAL_TEMP_REG;
                return OK;
            case 1:
                m_SmbusSensor = ADT7461_REMOTE_TEMP_REG;
                return OK;
            default:
                bUnknownSensor = true;
                break;
        }
    }

    if (bUnknownSensor)
    {
        Printf(Tee::PriError,
               "%d is not a valid sensor value for a %s device\n",
               Sensor, m_SmbDevName.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    Printf(Tee::PriError, "%s device not found\n", m_SmbDevName.c_str());
    return RC::DEVICE_NOT_FOUND;
}

RC Thermal::GetSmbusTempSensor(INT32 *pSensor)
{
    MASSERT(pSensor);

    if (m_SmbDevName == "")
    {
        Printf(Tee::PriError,
               "Smbus device must be set before getting the sensor!\n");
        return RC::SOFTWARE_ERROR;
    }

    bool bUnknownSensor = false;
    *pSensor = 0;
    if (m_SmbDevName == "ADT7473")
    {
        switch (m_SmbusSensor)
        {
            case ADT7473_LOCAL_TEMP_REG:
                *pSensor = 0;
                return OK;
            case ADT7473_REMOTE_TEMP1_REG:
                *pSensor = 1;
                return OK;
            case ADT7473_REMOTE_TEMP2_REG:
                *pSensor = 2;
                return OK;
            default:
                bUnknownSensor = true;
                break;
        }
    }
    else if ((m_SmbDevName == "ADT7461") || (m_SmbDevName == "TMP411"))
    {
        switch (m_SmbusSensor)
        {
            case ADT7461_LOCAL_TEMP_REG:
                *pSensor = 0;
                return OK;
            case ADT7461_REMOTE_TEMP_REG:
                *pSensor = 1;
                return OK;
            default:
                bUnknownSensor = true;
                break;
        }
    }

    if (bUnknownSensor)
    {
        Printf(Tee::PriError,
               "Current sensor %d is not valid for a %s device\n",
               m_SmbusSensor, m_SmbDevName.c_str());
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriError, "%s device not found\n", m_SmbDevName.c_str());
    return RC::DEVICE_NOT_FOUND;
}

RC Thermal::SetSmbusTempDevice(string Name)
{
    m_SmbDevName = Name;
    if (Name == "ADT7473")
    {
        m_SmbAddr= ADT7473_SMBUS_ADDR;
        m_SmbusSensor = ADT7473_REMOTE_TEMP1_REG;
        m_MeasurementOffset = TEMP_MEAS_OFFSET;
        return OK;
    }
    else if ((Name == "ADT7461") || (Name == "TMP411"))
    {
        m_SmbAddr= ADT7461_SMBUS_ADDR;
        m_SmbusSensor = ADT7461_REMOTE_TEMP_REG;
        m_MeasurementOffset = TEMP_MEAS_OFFSET;
        return OK;
    }
    else
    {
        Printf(Tee::PriError, "%s device not found\n", Name.c_str());
        return RC::DEVICE_NOT_FOUND;
    }
}

RC Thermal::GetSmbusTempDevice(string *pName)
{
    MASSERT(pName);
    *pName = m_SmbDevName;
    return OK;
}

RC Thermal::GetChipTempViaSmbus(INT32 *pTemp)
{
    MASSERT(pTemp);
    RC rc = OK;
    UINT08 Data = 0;
    if (!m_SmbusInit)
    {
        CHECK_RC(InitializeSmbusDevice());
    }
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, m_SmbusSensor, &Data));
    *pTemp = (INT32)Data + m_SmbusOffset + m_MeasurementOffset;
    CHECK_RC(m_OverTempCounter.Update(OverTempCounter::SMBUS_TEMP, *pTemp));
    return rc;
}

RC Thermal::InitializeSmbusDevice()
{
    RC rc = OK;
    SmbDevice    *pSmbDev;
    UINT08 tmp;

    if (m_SmbusInit)
    {
        Printf(Tee::PriError, "Smbus already initialized\n");
        return rc;
    }

    m_pSmbMgr = SmbMgr::Mgr();
    CHECK_RC(m_pSmbMgr->Find());
    CHECK_RC(m_pSmbMgr->InitializeAll());
    CHECK_RC(m_pSmbMgr->GetLwrrent((McpDev**)&pSmbDev));
    m_SmbusInit = true;
    // Search for thermal device on each port
    for (INT32 portNumber = 0; portNumber < 2; portNumber++)
    {
        //Calling GetSubDev with portNumber since the ports are stored in order
        CHECK_RC(pSmbDev->GetSubDev(portNumber, &m_pSmbPort));

        // Read offset 0 of the target address and check return code for errors.
        // If no errors are reported, then a device exists at the target
        // address.

        const UINT32 TEST_OFFSET = 0;

        if (OK == m_pSmbPort->RdByteCmd(m_SmbAddr, TEST_OFFSET, &tmp))
        {
            // For the ADT7461 and TMP411, force them to extended mode to
            // match the configuration of the ADT7473
            if ((m_SmbDevName == "ADT7461") || (m_SmbDevName == "TMP411"))
            {
                CHECK_RC(InitializeADT7461());
            }
            Printf(Tee::PriDebug,"Found device on Port %i\n", portNumber);
            return rc;
        }
    }

    Printf(Tee::PriError, "Device not found on Smbus\n");
    return RC::DEVICE_NOT_FOUND;
}

//-----------------------------------------------------------------------------
//! \brief Search for any volterra slaves and store data used to retrieve
//!        temperatures from the volterra devices
//!
//! \return OK if initialization succeeds, not OK otherwise
RC Thermal::InitializeVolterra()
{
    vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> instructions;
    RC rc;
    UINT32 lwrTarget;
    UINT32 lwrTargetType;
    UINT32 targetCount = 0;
    UINT32 lwrSlaveId;

    if (m_VolterraInit)
        return OK;

    m_VolterraSensors.clear();

    // Get the number of thermal sensor targets available
    instructions.resize(1);
    instructions[0].opcode =
        LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_TARGETS_AVAILABLE_OPCODE;
    instructions[0].operands.getInfoTargetsAvailable.availableTargets = 0;

    CHECK_RC(ExelwteSystemInstructions(&instructions));
    targetCount =
        instructions[0].operands.getInfoTargetsAvailable.availableTargets;

    // Do not continue if there are not any thermal targets available
    if (targetCount == 0)
        return OK;

    // Get the target type for each of the targets
    instructions.resize(targetCount);
    for (lwrTarget = 0; lwrTarget < targetCount; lwrTarget++)
    {
        instructions[lwrTarget].opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_INFO_TARGET_TYPE_OPCODE;
        instructions[lwrTarget].operands.getInfoTargetType.targetIndex = lwrTarget;
        instructions[lwrTarget].operands.getInfoTargetType.type = 0;
    }

    CHECK_RC(ExelwteSystemInstructions(&instructions));

    // Store the target indexes for each of the volterra targets
    for (lwrTarget = 0; lwrTarget < targetCount; lwrTarget++)
    {
        lwrTargetType = instructions[lwrTarget].operands.getInfoTargetType.type;
        switch (lwrTargetType)
        {
            case LW2080_CTRL_THERMAL_SYSTEM_TARGET_VTSLAVE1:
                lwrSlaveId = 1;
                break;
            case LW2080_CTRL_THERMAL_SYSTEM_TARGET_VTSLAVE2:
                lwrSlaveId = 2;
                break;
            case LW2080_CTRL_THERMAL_SYSTEM_TARGET_VTSLAVE3:
                lwrSlaveId = 3;
                break;
            case LW2080_CTRL_THERMAL_SYSTEM_TARGET_VTSLAVE4:
                lwrSlaveId = 4;
                break;
            default:
                lwrSlaveId = 0;
                break;
        }

        // Note that there is an inherent assumption here that RM will never
        // cannot have multiple targets of the same type.  This was confirmed
        // by RM to be a valid assumption in the current state of RM, however
        // put an assert here just in case RM changes this without notifying
        // MODS
        MASSERT(m_VolterraSensors.count(lwrSlaveId) == 0);

        // Volterra Slave IDs start at 1 (so a 0 indicates a non-volterra
        // target)
        if (lwrSlaveId != 0)
        {
            m_VolterraSensors[lwrSlaveId] = lwrTarget;
        }
    }

    m_VolterraInit = true;

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the valid volterra slave IDs
//!
//! \param pVolterraSlaves : Pointer to a set of available volterra slave IDs
//!                          determined during initialization
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::GetVolterraSlaves(set<UINT32> *pVolterraSlaves)
{
    RC rc;
    MASSERT(pVolterraSlaves);
    if (!m_VolterraInit)
    {
        CHECK_RC(InitializeVolterra());
    }
    map<UINT32, UINT32>::iterator pSensors = m_VolterraSensors.begin();

    pVolterraSlaves->clear();
    while (pSensors != m_VolterraSensors.end())
    {
        pVolterraSlaves->insert(pSensors->first);
        pSensors++;
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Determine if a specified volterra slave is present
//!
//! \param SlaveId  : Volterra slave ID to check for presence
//! \param pPresent : Returned boolean indicating whether the volterra slave
//!                   is present
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::GetVolterraPresent(UINT32 SlaveId, bool *pPresent)
{
    RC rc;
    if (!m_VolterraInit)
    {
        CHECK_RC(InitializeVolterra());
    }
    *pPresent = m_VolterraSensors.count(SlaveId) > 0;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Determine if any volterra slave is present
//!
//! \param pPresent : Returned boolean indicating whether any volterra slave
//!                   is present
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::GetIsVolterraPresent(bool *pPresent)
{
    RC rc;
    if (!m_VolterraInit)
    {
        CHECK_RC(InitializeVolterra());
    }
    *pPresent = m_VolterraSensors.size() > 0;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the current temperature reading from a volterra slave
//!
//! \param SlaveId  : Volterra slave ID to get the temperature for
//! \param pTemp    : Returned temperature reading
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::GetVolterraSlaveTemp(UINT32 SlaveId, UINT32 *pTemp)
{
    RC rc;
    bool bPresent;

    if (!m_VolterraInit)
    {
        CHECK_RC(InitializeVolterra());
    }

    *pTemp = 0;

    CHECK_RC(GetVolterraPresent(SlaveId, &bPresent));

    if (!bPresent)
        return RC::UNSUPPORTED_FUNCTION;

    UINT64 elapsedTimeMs;
    FLOAT64 sampleRateMs = BgLogger::GetReadIntervalMs();
    if (sampleRateMs == 0)
    {
        // Not periodically monitoring, no need to keep a reading history
        // in order to monitor correctness of readings
        elapsedTimeMs = 0;;
    }
    else if (m_LastVolterraReading.count(SlaveId))
    {
        elapsedTimeMs = Platform::GetTimeMS() -
                           m_LastVolterraReading[SlaveId].TimeMs;
    }
    else
    {
        // Periodically reading the volterra temp and there is no histor
        // force the elapsed time to be huge so the history re-inits
        elapsedTimeMs = ~0;
    }

    if (elapsedTimeMs > (VOLTERRA_RESET_LAST_TEMP * sampleRateMs))
    {
        CHECK_RC(InitLastVolterraTemp(SlaveId));
        *pTemp = m_LastVolterraReading[SlaveId].Temp;
    }
    else
    {
        CHECK_RC(DoGetVolterraTemp(m_VolterraSensors[SlaveId], pTemp));
        if (!IsVolterraTempValid(SlaveId, *pTemp))
        {
            return RC::THERMAL_SENSOR_ERROR;
        }
        m_LastVolterraReading[SlaveId].Temp = *pTemp;
        m_LastVolterraReading[SlaveId].TimeMs = Platform::GetTimeMS();
    }

    OverTempCounter::TempType tt =
        (OverTempCounter::TempType)(OverTempCounter::VOLTERRA_TEMP_1 +
                                    SlaveId - 1);

    if (tt > OverTempCounter::VOLTERRA_TEMP_LAST)
    {
        Printf(Tee::PriError, "Unknown Volterra Slave ID %d\n", SlaveId);
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(m_OverTempCounter.Update(tt, (INT32)*pTemp));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the maximum difference between two temperature readings on the
//!        same volterra slave
//!
//! \param pMaxDelta  : Pointer to returned maximum delta in C
//!
//! \return OK
RC Thermal::GetVolterraMaxTempDelta(UINT32 *pMaxDelta)
{
    *pMaxDelta = m_VolterraMaxTempDelta;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Set the maximum difference between two temperature readings on the
//!        same volterra slave
//!
//! \param MaxDiff  : Maximum delta in C
//!
//! \return OK
RC Thermal::SetVolterraMaxTempDelta(UINT32 MaxDelta)
{
    m_VolterraMaxTempDelta = MaxDelta;
    return OK;
}

// The Volterra part has a bug that can corrupt I2C transfers and can cause
// RM to return invalid values for the current volterra temperature.  See
// Bug 764107 for details.  MODS needs to attempt to determine "correctness"
// of a Volterra temperature based on the last reading
//
// At the start of time, MODS needs to attempt to find a "good" reading to
// use

//-----------------------------------------------------------------------------
//! \brief Private function to initialize the last valid volterra temp for a
//!        particular slave
//!
//! \param SlaveId  : Volterra slave ID to initialize the last reading for
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::InitLastVolterraTemp(UINT32 SlaveId)
{
    RC rc;
    UINT32 validTempCount = 0;
    UINT32 retries = 0;
    UINT32 lastTemp;
    VolterraTempReading lwrrentReading;

    // If in this function then we are resetting what the last valid volterra
    // reading was, so ilwalidate the current one
    m_LastVolterraReading.erase(SlaveId);

    // Read the volterra slave until a valid temperature is found
    do
    {
        CHECK_RC(DoGetVolterraTemp(m_VolterraSensors[SlaveId], &lastTemp));
    } while ((++retries < VOLTERRA_INIT_TEMP_RETRIES) &&
             !IsVolterraTempValid(SlaveId, lastTemp));

    if (!IsVolterraTempValid(SlaveId, lastTemp))
    {
        Printf(Tee::PriError,
               "InitLastVolterraTemp : Unable to read the volterra temperature"
               " on slave %d\n", SlaveId);
        return RC::THERMAL_SENSOR_ERROR;
    }
    retries = 0;

    // Now read the volterra slave until VOLTERRA_INIT_TEMP_COUNT additional
    // conselwtive readings are acquired within VOLTERRA_INIT_TEMP_DIFF of the
    // first reading
    do
    {
        CHECK_RC(DoGetVolterraTemp(m_VolterraSensors[SlaveId], &lwrrentReading.Temp));

        if (IsVolterraTempValid(SlaveId, lwrrentReading.Temp))
        {
            UINT32 diff = (lwrrentReading.Temp > lastTemp) ?
                                (lwrrentReading.Temp - lastTemp) :
                                (lastTemp - lwrrentReading.Temp);

            // The current reading is outside the range of the first reading so
            // reset the first reading as well as the valid count and start over
            if (diff > VOLTERRA_INIT_TEMP_DIFF)
            {
                lastTemp = lwrrentReading.Temp;
                validTempCount = 0;
            }
            else
            {

                validTempCount++;
            }
        }
    } while ((++retries < VOLTERRA_INIT_TEMP_RETRIES) &&
             (validTempCount != VOLTERRA_INIT_TEMP_COUNT));

    if (validTempCount != VOLTERRA_INIT_TEMP_COUNT)
    {
        Printf(Tee::PriError,
               "InitLastVolterraTemp : Unable to find stable initial "
               "volterra temperature on slave %d\n", SlaveId);
        return RC::SOFTWARE_ERROR;
    }

    // Save off the initial valid reading along with the time (necessary to
    // callwlate slope
    lwrrentReading.TimeMs = Platform::GetTimeMS();
    m_LastVolterraReading[SlaveId] = lwrrentReading;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to get the volterra temperature
//!
//! \param SlaveId  : Volterra slave ID to get the temperature on
//! \param pTemp    : Returned temperature in C
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::DoGetVolterraTemp(UINT32 SensorIndex, UINT32 *pTemp)
{
    RC rc;
    bool bExelwteFailed;
    UINT32 retries = 0;

    *pTemp = 0;

    LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION instr = { 0 };
    instr.opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_STATUS_SENSOR_READING_OPCODE;
    instr.operands.getStatusSensorReading.sensorIndex = SensorIndex;
    instr.operands.getStatusSensorReading.value = 0;
    SysInstrs instructions(1);

    do
    {
        instructions.vec[0] = instr;
        rc = ExelwteSystemInstructions(&instructions.vec, &bExelwteFailed);
    } while ((rc != OK) && bExelwteFailed &&
             (++retries < VOLTERRA_GET_TEMP_RETRIES));

    if (rc == OK)
    {
        *pTemp = instructions.vec[0].operands.getStatusSensorReading.value;
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to check the validity of a volterra temperature
//!        reading
//!
//! \param SlaveId  : Volterra slave ID to check
//! \param Temp     : Temperature to check for validity
//!
//! \return true if valid, false if not
bool Thermal::IsVolterraTempValid(UINT32 SlaveId, UINT32 Temp)
{
    // 0 and 255 are always considered invalid
    if ((Temp == 0) || (Temp == 255))
        return false;

    // If no last reading is available or the temperature reading is not
    // periodic or temperature delta checking is disabled, then checking
    // for 0 or 255 is the best that MODS can do
    if ((BgLogger::GetReadIntervalMs() == 0) ||
        (m_LastVolterraReading.count(SlaveId) == 0) ||
        (m_VolterraMaxTempDelta == 0))
    {
        return true;
    }

    UINT64 elapsedTimeMs;
    UINT32 tempDiff;
    FLOAT64 TempPerSec;

    // Callwlate the temperature change per second from the last valid reading
    elapsedTimeMs = Platform::GetTimeMS() - m_LastVolterraReading[SlaveId].TimeMs;

    if (m_LastVolterraReading[SlaveId].Temp > Temp)
        tempDiff = m_LastVolterraReading[SlaveId].Temp - Temp;
    else
        tempDiff = Temp - m_LastVolterraReading[SlaveId].Temp;
    TempPerSec = (static_cast<FLOAT64>(tempDiff) * 1000.0) /
                  static_cast<FLOAT64>(elapsedTimeMs);
    FLOAT64 MaxTempPerSec = (m_VolterraMaxTempDelta * 1000.0) /
                            BgLogger::GetReadIntervalMs();

    return TempPerSec < MaxTempPerSec;
}

//-----------------------------------------------------------------------------
//! \brief Initialize the over temp error counter
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::InitOverTempErrCounter()
{
    return m_OverTempCounter.Initialize();
}

//-----------------------------------------------------------------------------
//! \brief Shutdown the over temp error counter
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::ShutdownOverTempErrCounter()
{
    return m_OverTempCounter.ShutDown(true);
}

//-----------------------------------------------------------------------------
//! \brief Set the range in the over temp error counter for a specific
//!        temperatur check
//!
//! \param OverTempType : Type of temperature to set the range on
//! \param Min          : Minimum temperature
//! \param Max          : Maximum temperature
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::SetOverTempRange(OverTempCounter::TempType OverTempType,
                             INT32 Min, INT32 Max)
{
    return m_OverTempCounter.SetTempRange(OverTempType, Min, Max);
}

//-----------------------------------------------------------------------------
//! \brief Get the range in the over temp error counter for a specific
//!        temperature check
//!
//! \param OverTempType  : Type of temperature to set the range on
//! \param pMin          : Returned minimum temperature
//! \param pMax          : Returned maximum temperature
//!
//! \return OK if successfull, not OK otherwise
RC Thermal::GetOverTempRange(OverTempCounter::TempType OverTempType,
                             INT32 *pMin, INT32 *pMax)
{
    MASSERT(pMin);
    MASSERT(pMax);

    if ((OverTempType >= OverTempCounter::NUM_TEMPS) ||
        (OverTempType == OverTempCounter::RM_EVENT)  ||
        (pMin == nullptr) || (pMax == nullptr))
    {
        return RC::SOFTWARE_ERROR;
    }

    return m_OverTempCounter.GetTempRange(OverTempType, pMin, pMax);
}

//-----------------------------------------------------------------------------
//! \brief Enable or disable monitoring of temperature ranges (i.e. non
//!        RM_EVENT over temperature counters)
//!
//! \param bEnable : true to enable, false to disable
void Thermal::EnableOverTempRangeMonitoring(bool bEnable)
{
    m_OverTempCounter.EnableRangeMonitoring(bEnable);
}

//-----------------------------------------------------------------------------
//! \brief Initialize the ADT7461 by forcing it to extended mode and adjusting
//!        the limits as necessary to account for it
//!
//! \return OK if successful, not OK otherwise
RC Thermal::InitializeADT7461()
{
    RC rc;

    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_CONFIG_RD_REG,
                                   &m_ADT7461OrigRegs.Config));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_EXT_LIMIT_RD_REG,
                                   &m_ADT7461OrigRegs.ExtHighLimit));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_EXT_THRM_LIMIT_REG,
                                   &m_ADT7461OrigRegs.ExtThermLimit));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_LOC_LIMIT_RD_REG,
                                   &m_ADT7461OrigRegs.LocHighLimit));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_LOC_THRM_LIMIT_REG,
                                   &m_ADT7461OrigRegs.LocThermLimit));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_EXT_OFFSET_REG,
                                   &m_ADT7461OrigRegs.ExtOffset));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_HYSTERESIS_REG,
                                   &m_ADT7461OrigRegs.Hysteresis));

    // If an ADT configuration was not provided on the command line then
    // use the read configuration as the base configuration
    if (!m_ADT7461ModsRegs.bSet)
    {
        m_ADT7461ModsRegs           = m_ADT7461OrigRegs;
        m_ADT7461ModsRegs.ExtOffset = 0;
        m_ADT7461ModsRegs.bSet      = true;
    }

    if (!(m_ADT7461ModsRegs.Config & ADT7461_CONFIG_TEMP_OFFSET_MASK))
    {
        // Subtract the TEMP_MEAS_OFFSET from the thermal limits
        // which were set in the non-extended mode.  Since
        // TEMP_MEAS_OFFSET is negative this colwerts the limit
        // to the correct hex value for use in extended mode
        m_ADT7461ModsRegs.Config  = m_ADT7461ModsRegs.Config |
                                    ADT7461_CONFIG_TEMP_OFFSET_MASK;
        m_ADT7461ModsRegs.LocHighLimit  =
            static_cast<INT08>(m_ADT7461ModsRegs.LocHighLimit) -
            TEMP_MEAS_OFFSET;
        m_ADT7461ModsRegs.ExtHighLimit  =
            static_cast<INT08>(m_ADT7461ModsRegs.ExtHighLimit) -
            TEMP_MEAS_OFFSET;
        m_ADT7461ModsRegs.ExtThermLimit =
            static_cast<INT08>(m_ADT7461ModsRegs.ExtThermLimit) -
            TEMP_MEAS_OFFSET;
        m_ADT7461ModsRegs.LocThermLimit =
            static_cast<INT08>(m_ADT7461ModsRegs.ExtThermLimit) -
            TEMP_MEAS_OFFSET;
    }
    m_SmbConfigChanged = true;

    CHECK_RC(m_pSmbPort->WrByteCmd(m_SmbAddr,
                                   ADT7461_EXT_LIMIT_WR_REG,
                                   m_ADT7461ModsRegs.ExtHighLimit));
    CHECK_RC(m_pSmbPort->WrByteCmd(m_SmbAddr,
                                   ADT7461_EXT_THRM_LIMIT_REG,
                                   m_ADT7461ModsRegs.ExtThermLimit));
    CHECK_RC(m_pSmbPort->WrByteCmd(m_SmbAddr,
                                   ADT7461_LOC_LIMIT_WR_REG,
                                   m_ADT7461ModsRegs.LocHighLimit));
    CHECK_RC(m_pSmbPort->WrByteCmd(m_SmbAddr,
                                   ADT7461_LOC_THRM_LIMIT_REG,
                                   m_ADT7461ModsRegs.LocThermLimit));
    CHECK_RC(m_pSmbPort->WrByteCmd(m_SmbAddr,
                                   ADT7461_EXT_OFFSET_REG,
                                   m_ADT7461ModsRegs.ExtOffset));

    // Note that it is important to write the configuration register last since
    // moving to extended from normal mode requires first writing a higher
    // value to the limit registers so that when the switch to extended oclwrs
    // therm alert does not happen
    CHECK_RC(m_pSmbPort->WrByteCmd(m_SmbAddr,
                                   ADT7461_CONFIG_WR_REG,
                                   m_ADT7461ModsRegs.Config));

    if ((m_ADT7461ModsRegs.Config & ADT7461_CONFIG_TEMP_OFFSET_MASK) !=
        (m_ADT7461OrigRegs.Config & ADT7461_CONFIG_TEMP_OFFSET_MASK))
    {
        UINT08 TempUpdateDivisorLog2 = 0;
        const UINT32 BASE_TEMP_UPDATE_MS     = 16000;

        CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr,
                                       ADT7461_COLW_RATE_RD_REG,
                                       &TempUpdateDivisorLog2));

        // After changing the configuration, it is necessary to
        // wait for a temperature update.  If MODS were to read the
        // temperature too soon after changing the configuration,
        // it would result in MODS reading a pre-config change
        // temperature and interpreting it as if the config change
        // had happened.
        Tasker::Sleep((BASE_TEMP_UPDATE_MS >> TempUpdateDivisorLog2) + 1);
    }

    PrintADT7461Config();

    return rc;
}

//-----------------------------------------------------------------------------
RC Thermal::SetADT7461Config
(
    UINT32 Config,
    UINT32 LocLimit,
    UINT32 ExtLimit,
    UINT32 Hysteresis
)
{
    m_ADT7461ModsRegs.bSet          = true;
    m_ADT7461ModsRegs.Config        = static_cast<UINT08>(Config);
    m_ADT7461ModsRegs.LocHighLimit  = static_cast<UINT08>(LocLimit);
    m_ADT7461ModsRegs.ExtHighLimit  = static_cast<UINT08>(ExtLimit);
    m_ADT7461ModsRegs.ExtThermLimit = static_cast<UINT08>(ExtLimit);
    m_ADT7461ModsRegs.LocThermLimit = static_cast<UINT08>(LocLimit);
    m_ADT7461ModsRegs.Hysteresis    = static_cast<UINT08>(Hysteresis);
    m_ADT7461ModsRegs.ExtOffset     = 0;
    return OK;
}

//-----------------------------------------------------------------------------
RC Thermal::PrintADT7461Config()
{
    RC rc;
    ADT7461Regs tempRegs = {0};

    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_CONFIG_RD_REG,
                                   &tempRegs.Config));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_EXT_LIMIT_RD_REG,
                                   &tempRegs.ExtHighLimit));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_EXT_THRM_LIMIT_REG,
                                   &tempRegs.ExtThermLimit));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_LOC_LIMIT_RD_REG,
                                   &tempRegs.LocHighLimit));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_LOC_THRM_LIMIT_REG,
                                   &tempRegs.LocThermLimit));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_EXT_OFFSET_REG,
                                   &tempRegs.ExtOffset));
    CHECK_RC(m_pSmbPort->RdByteCmd(m_SmbAddr, ADT7461_HYSTERESIS_REG,
                                   &tempRegs.Hysteresis));

    Printf(Tee::PriLow, "ADT7461 Configuration During MODS: \n");
    Printf(Tee::PriLow, "    Config        = 0x%02x\n", tempRegs.Config);
    Printf(Tee::PriLow, "    ExtHighLimit  = 0x%02x\n", tempRegs.ExtHighLimit);
    Printf(Tee::PriLow, "    ExtThermLimit = 0x%02x\n", tempRegs.ExtThermLimit);
    Printf(Tee::PriLow, "    LocHighLimit  = 0x%02x\n", tempRegs.LocHighLimit);
    Printf(Tee::PriLow, "    LocThermLimit = 0x%02x\n", tempRegs.LocThermLimit);
    Printf(Tee::PriLow, "    ExtOffset     = 0x%02x\n", tempRegs.ExtOffset);
    Printf(Tee::PriLow, "    Hysteresis    = 0x%02x\n", tempRegs.Hysteresis);

    return rc;
}

//--------------------------------------------------------------------
//! \brief Save the GPU's temperature limit for one thermal event
//!
//! This should be called at the start of mods.
//!
RC Thermal::SaveInitialThermalLimit(UINT32 eventId)
{
    LwRmPtr pLwRm;
    RC rc;

    CHECK_RC(CheckIsSupported());
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return RC::UNSUPPORTED_FUNCTION;

    if (!m_InitialThermalLimits.count(eventId))
    {
        LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS params = {0};
        params.eventId = eventId;
        CHECK_RC(pLwRm->ControlBySubdevice(
                    m_pSubdevice,
                    LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_GET,
                    &params, sizeof(params)));
        m_InitialThermalLimits[eventId] = params.temperature;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Restore a temperature limit saved by SaveInialThermalLimit
//!
//! This should be called at the end of mods.
//!
RC Thermal::RestoreInitialThermalLimit(UINT32 eventId)
{
    LwRmPtr pLwRm;
    RC rc;

    CHECK_RC(CheckIsSupported());
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return RC::UNSUPPORTED_FUNCTION;

    if (m_InitialThermalLimits.count(eventId))
    {
        LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS params = {0};
        params.eventId = eventId;
        CHECK_RC(pLwRm->ControlBySubdevice(
                    m_pSubdevice,
                    LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_GET,
                    &params, sizeof(params)));
        params.temperature = m_InitialThermalLimits[eventId];
        CHECK_RC(pLwRm->ControlBySubdevice(
                    m_pSubdevice,
                    LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_SET,
                    &params, sizeof(params)));
    }
    return rc;
}

RC Thermal::GetThermalLimit(UINT32 eventId, INT32* temperature)
{
    MASSERT(temperature);
    LwRmPtr pLwRm;
    RC rc;

    CHECK_RC(CheckIsSupported());
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return RC::UNSUPPORTED_FUNCTION;

    LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS params = {0};
    params.eventId = eventId;
    CHECK_RC(pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_GET,
                &params, sizeof(params)));

    *temperature = params.temperature;

    return rc;
}

RC Thermal::SetThermalLimit(UINT32 eventId, INT32 temperature)
{
    LwRmPtr pLwRm;
    RC rc;

    CHECK_RC(CheckIsSupported());
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return RC::UNSUPPORTED_FUNCTION;

    LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS params = {0};
    params.eventId = eventId;
    CHECK_RC(pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_GET,
                &params, sizeof(params)));
    params.temperature = temperature;
    CHECK_RC(pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_SETTINGS_SET,
                &params, sizeof(params)));

    return rc;
}

void Thermal::BeginExpectingErrors(OverTempCounter::TempType idx)
{
    m_OverTempCounter.BeginExpectingErrors(idx);
}
void Thermal::EndExpectingErrors(OverTempCounter::TempType idx)
{
    m_OverTempCounter.EndExpectingErrors(idx);
}
void Thermal::EndExpectingAllErrors()
{
    m_OverTempCounter.EndExpectingAllErrors();
}
RC Thermal::ReadExpectedErrorCount(UINT64 * pCount)
{
    return m_OverTempCounter.ReadExpectedErrorCount(pCount);
}

//--------------------------------------------------------------------
//! \brief Read the GPU temperature.  This method only works for SOC.
//!
RC Thermal::GetSocGpuTemp(FLOAT32 *pTemp)
{
    static const char* const gpuTempFiles[] =
    {
        "/sys/kernel/debug/tegra_soctherm/gputemp",
        "/sys/kernel/debug/bpmp/debug/soctherm/group_GPU/temp",
        "/sys/kernel/debug/bpmp/debug/soctherm/gpu/temp"
    };

    static string gpuTempFile;

    if (gpuTempFile.empty())
    {
        for (const char* lwrFile : gpuTempFiles)
        {
            if (Xp::DoesFileExist(lwrFile))
            {
                gpuTempFile = lwrFile;
                break;
            }
        }

        if (gpuTempFile.empty())
        {
            static const char thermalZone[] = "/sys/class/thermal/thermal_zone%d/";
            for (int i=0; ; i++)
            {
                const string dir  = Utility::StrPrintf(thermalZone, i);
                const string type = dir + "type";
                const string temp = dir + "temp";
                if (!Xp::DoesFileExist(type.c_str()))
                {
                    break;
                }
                string value;
                if (OK != Xp::InteractiveFileRead(type.c_str(), &value))
                {
                    break;
                }
                while (!value.empty() && isspace(value[value.size()-1]))
                {
                    value.resize(value.size() - 1);
                }
                if (value == "GPU-therm")
                {
                    gpuTempFile = temp;
                    break;
                }
            }
        }
    }

    if (gpuTempFile.empty())
    {
        *pTemp = -273;
        return RC::OK;
    }

    return ReadMilliC(gpuTempFile.c_str(), pTemp);
}

RC Thermal::GetFanPolicyMask(UINT32 *pPolicyMask)
{
    RC rc;
    MASSERT(pPolicyMask);
    LwRmPtr pLwRm;
    LW2080_CTRL_FAN_POLICY_INFO_PARAMS infoParams = {0};
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(
        pRmHandle,
        LW2080_CTRL_CMD_FAN_POLICY_GET_INFO,
        &infoParams,
        sizeof(infoParams)));
    *pPolicyMask = infoParams.policyMask;
    return rc;
}

RC Thermal::DumpFanPolicyTable(bool defaultValues)
{
    RC rc;
    UINT32 policyMask;
    LW2080_CTRL_FAN_POLICY_INFO_PARAMS infoParams = {0};
    LW2080_CTRL_FAN_POLICY_STATUS_PARAMS statusParams = {};
    LW2080_CTRL_FAN_POLICY_CONTROL_PARAMS ctrlParams = {0};
    ctrlParams.bDefault = defaultValues;
    CHECK_RC(GetFanPolicyTable(&infoParams, &statusParams, &ctrlParams));
    CHECK_RC(GetFanPolicyMask(&policyMask));
    Printf(Tee::PriNormal, "\nPolicy Mask: 0x%x\n", policyMask);

    for (UINT32 index = 0; index < LW2080_CTRL_FAN_POLICY_MAX_POLICIES; index++)
    {
        if (!(BIT(index) & policyMask))
        {
            continue;
        }
        string policyType = "Policy Type : ";
        policyType += Utility::StrPrintf(" (Mask 0x%x) ", BIT(index));

        LW2080_CTRL_FAN_POLICY_INFO_DATA_IIR_TJ_FIXED_DUAL_SLOPE_PWM infoiirTFDSP = {0};
        LW2080_CTRL_FAN_POLICY_STATUS_DATA_IIR_TJ_FIXED_DUAL_SLOPE_PWM statusiirTFDSP = {0};
        LW2080_CTRL_FAN_POLICY_CONTROL_DATA_IIR_TJ_FIXED_DUAL_SLOPE_PWM ctrliirTFDSP = {0};

        switch (infoParams.policies[index].type)
        {
            case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
                Printf(Tee::PriNormal, "%sIIR Tj Fixed Dual Slope PWM V20", policyType.c_str());
                Printf(Tee::PriNormal, "\tCooler Idx = %u\n",
                        infoParams.policies[index].data.iirTFDSPV20.fanPolicyV20.coolerIdx);
                Printf(Tee::PriNormal, "\tSampling Period (ms) = %u\n",
                        infoParams.policies[index].data.iirTFDSPV20.fanPolicyV20.samplingPeriodms);
                infoiirTFDSP = infoParams.policies[index].data.iirTFDSPV20.iirTFDSP;
                statusiirTFDSP = statusParams.policies[index].data.iirTFDSPV20.iirTFDSP;
                ctrliirTFDSP = ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP;
                break;
            case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
                Printf(Tee::PriNormal, "%sIIR Tj Fixed Dual Slope PWM V30", policyType.c_str());
                Printf(Tee::PriNormal, "\tbFanLwrvePt2TjOverride = %u\n",
                        infoParams.policies[index].data.iirTFDSPV30.bFanLwrvePt2TjOverride);
                Printf(Tee::PriNormal, "\tbFanLwrveAdjSupported = %u\n",
                        infoParams.policies[index].data.iirTFDSPV30.bFanLwrveAdjSupported);
                infoiirTFDSP = infoParams.policies[index].data.iirTFDSPV30.iirTFDSP;
                statusiirTFDSP = statusParams.policies[index].data.iirTFDSPV30.iirTFDSP;
                ctrliirTFDSP = ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP;
                break;
            default:
                continue;
        }

        Printf(Tee::PriNormal, "\tThermal Channel Idx = %u\n",
                infoParams.policies[index].thermChIdx);

        // Just to be safe
        MASSERT(infoParams.policies[index].type == statusParams.policies[index].type &&
                statusParams.policies[index].type == ctrlParams.policies[index].type);

        switch (infoParams.policies[index].type)
        {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
            string pwmString = "";
            // INFO
            pwmString += Utility::StrPrintf(
                "\tControl Selector = %s\n"
                "\tFan Stop supported = %s\n"
                "\tFan start min hold time (ms) = %u\n"
                "\tFan stop power topology idx = %u\n",
                infoiirTFDSP.bUsePwm ? "PWM":"RPM",
                infoiirTFDSP.bFanStopSupported ? "true" : "false",
                (UINT32)infoiirTFDSP.fanStartMinHoldTimems,
                (UINT32)infoiirTFDSP.fanStopPowerTopologyIdx);
            // STATUS
            pwmString += Utility::StrPrintf(
                "\tAvg Long Term Tj (SFXP10.22) = 0x%x\n"
                "\tAvg Short Term Tj (SFXP10.22) = 0x%x\n"
                "\tTj Current (LwTemp) = 0x%x\n"
                "\tFiltered power mw = %u\n"
                "\tFan stop active = %s\n",
                (UINT32)statusiirTFDSP.tjAvgLongTerm,
                (UINT32)statusiirTFDSP.tjAvgShortTerm,
                (UINT32)statusiirTFDSP.tjLwrrent,
                (UINT32)statusiirTFDSP.filteredPwrmW,
                statusiirTFDSP.bFanStopActive ? "true" : "false");
                break;
            // CONTROL
            pwmString += Utility::StrPrintf(
                "\tIIR Min Gain (SFXP 16.16) = 0x%x\n"
                "\tIIR Max Gain (SFXP 16.16) = 0x%x\n"
                "\tIIR Short term Gain (SFXP 16.16) = 0x%x\n"
                "\tIIR FilterPower = %u\n"
                "\tIIR Long Term Sampling Ratio = %u\n"
                "\tIIR Filter Lower Width (SFXP 24.8) = 0x%x\n"
                "\tIIR Filter Upper Width (SFXP 24.8) = 0x%x\n",
                (UINT32)ctrliirTFDSP.iirGainMin,
                (UINT32)ctrliirTFDSP.iirGainMax,
                (UINT32)ctrliirTFDSP.iirGainShortTerm,
                ctrliirTFDSP.iirFilterPower,
                ctrliirTFDSP.iirLongTermSamplingRatio,
                (UINT32)ctrliirTFDSP.iirFilterWidthLower,
                (UINT32)ctrliirTFDSP.iirFilterWidthUpper);

            for (UINT32 lwrvePt = 0;
                lwrvePt < LW2080_CTRL_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_LWRVE_PTS;
                lwrvePt++)
            {
                pwmString += Utility::StrPrintf("\tFan Lwrve Point: %u\n\t\t"
                    "Tj (SFXP 24.8) = 0x%x\n\t\tPWM = %u\n\t\tRPM = %u\n",
                    lwrvePt, ctrliirTFDSP.fanLwrvePts[lwrvePt].tj,
                    ctrliirTFDSP.fanLwrvePts[lwrvePt].pwm,
                    (UINT32)ctrliirTFDSP.fanLwrvePts[lwrvePt].rpm);
            }

            pwmString += Utility::StrPrintf("\tFan stop params:\n"
                "\tFanStop Enable = %s\n"
                "\tFan stop temp limit lower (LwTemp) = 0x%x\n"
                "\tFan start temp limit upper (LwTemp) = 0x%x\n"
                "\tFan stop power limit lower (mW) = %u\n"
                "\tFan start power limit upper (mW) = %u\n"
                "\tFan stop iir gain power (SFXP20_12) = 0x%x\n",
                ctrliirTFDSP.bFanStopEnable ? "true" : "false",
                (UINT32)ctrliirTFDSP.fanStopTempLimitLower,
                (UINT32)ctrliirTFDSP.fanStartTempLimitUpper,
                (UINT32)ctrliirTFDSP.fanStopPowerLimitLowermW,
                (UINT32)ctrliirTFDSP.fanStartPowerLimitUppermW,
                (UINT32)ctrliirTFDSP.fanStopIIRGainPower);

            Printf(Tee::PriNormal, "%s\n", pwmString.c_str());
            break;
        }
    }
    return rc;
}

RC Thermal::GetFanPolicyStatus(LW2080_CTRL_FAN_POLICY_STATUS_PARAMS *pFanPolicyStatusParams)
{
    RC rc;
    UINT32 policyMask;
    CHECK_RC(GetFanPolicyMask(&policyMask));
    pFanPolicyStatusParams->super.objMask.super.pData[0] = policyMask;
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_FAN_POLICY_GET_STATUS
       ,pFanPolicyStatusParams
       ,sizeof(LW2080_CTRL_FAN_POLICY_STATUS_PARAMS));
}

RC Thermal::GetFanPolicyTable(
    LW2080_CTRL_FAN_POLICY_INFO_PARAMS *infoParams,
    LW2080_CTRL_FAN_POLICY_STATUS_PARAMS *statusParams,
    LW2080_CTRL_FAN_POLICY_CONTROL_PARAMS *ctrlParams
)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(pRmHandle,
                LW2080_CTRL_CMD_FAN_POLICY_GET_INFO,
                infoParams,
                sizeof(LW2080_CTRL_FAN_POLICY_INFO_PARAMS)));

    // Initialize the BOARDOBJ mask from policyMask.
    statusParams->super.objMask.super.pData[0] = infoParams->policyMask;
    CHECK_RC(pLwRm->Control(pRmHandle,
                LW2080_CTRL_CMD_FAN_POLICY_GET_STATUS,
                statusParams,
                sizeof(LW2080_CTRL_FAN_POLICY_STATUS_PARAMS)));

    ctrlParams->policyMask = infoParams->policyMask;
    CHECK_RC(pLwRm->Control(pRmHandle,
                LW2080_CTRL_CMD_FAN_POLICY_GET_CONTROL,
                ctrlParams,
                sizeof(LW2080_CTRL_FAN_POLICY_CONTROL_PARAMS)));
    return rc;
}

RC Thermal::SetFanPolicyTable(
    LW2080_CTRL_FAN_POLICY_CONTROL_PARAMS &ctrlParams
)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(pRmHandle,
                LW2080_CTRL_CMD_FAN_POLICY_SET_CONTROL,
                &ctrlParams,
                sizeof(ctrlParams)));
    return rc;
}

RC Thermal::GetFanCoolerMask(UINT32 *pCoolerMask)
{
    RC rc;
    MASSERT(pCoolerMask);
    LwRmPtr pLwRm;
    LW2080_CTRL_FAN_COOLER_INFO_PARAMS infoParams = {0};
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(pRmHandle,
        LW2080_CTRL_CMD_FAN_COOLER_GET_INFO,
        &infoParams,
        sizeof(infoParams)));
    *pCoolerMask = infoParams.coolerMask;
    return rc;
}

RC Thermal::DumpFanCoolerTable(bool defaultValues)
{
    RC rc;
    UINT32 coolerMask;
    LW2080_CTRL_FAN_COOLER_INFO_PARAMS infoParams = {0};
    LW2080_CTRL_FAN_COOLER_STATUS_PARAMS statusParams = {};
    LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS ctrlParams = {0};
    ctrlParams.bDefault = defaultValues;
    CHECK_RC(GetFanCoolerTable(&infoParams, &statusParams, &ctrlParams));
    CHECK_RC(GetFanCoolerMask(&coolerMask));
    Printf(Tee::PriNormal, "\nCooler Mask : %u\n", coolerMask);

    for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
    {
        if (!(BIT(index) & coolerMask))
        {
            continue;
        }

        string coolerType = "Cooler Type :";
        coolerType += Utility::StrPrintf(" (Mask 0x%x) ", BIT(index));
        switch (infoParams.coolers[index].type)
        {
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                coolerType += "Active";
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                coolerType += "Active PWM";
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                coolerType += "Active PWM Tach Corr";
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_UNKNOWN:
                coolerType += "Unknown";
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_ILWALID:
                coolerType += "Invalid";
                continue;
        }

        Printf(Tee::PriNormal, "%s\n", coolerType.c_str());

        string actString = "";
        string actPwmString = "";
        string actPwmTachString = "";
        LW2080_CTRL_FAN_COOLER_INFO_DATA_ACTIVE active;
        LW2080_CTRL_FAN_COOLER_INFO_DATA_ACTIVE_PWM activePwm;
        LW2080_CTRL_FAN_COOLER_INFO_DATA_ACTIVE_PWM_TACH_CORR activePwmTachCorr;
        switch (infoParams.coolers[index].type)
        {
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                active = infoParams.coolers[index].data.active;
                actString += Utility::StrPrintf(
                    "\tTach GPIO func = %u\n"
                    "\tTachrate = %u\n"
                    "\tTachometer present and supported = %s\n"
                    "\tControl Unit = %u\n",
                    (UINT32)active.tachFuncFan,
                    (UINT32)active.tachRate,
                    active.bTachSupported ? "true":"false",
                    (UINT32)active.controlUnit);
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                activePwm = infoParams.coolers[index].data.activePwm;
                actPwmString += Utility::StrPrintf(
                    "\tFan speed Scaling (SFXP4_12) = 0x%x\n"
                    "\tFan speed offset (SFXP4_12)  = 0x%x\n"
                    "\tGPIO function = %u\n"
                    "\tPWM frequency = %u\n"
                    "\tTach GPIO func = %u\n"
                    "\tTachrate = %u\n"
                    "\tTach supported = %s\n"
                    "\tControl Unit = %u\n"
                    "\tMax fan event mask = 0x%x\n"
                    "\tMax fan min time ms = %u\n",
                    (UINT32)activePwm.scaling,
                    (UINT32)activePwm.offset,
                    activePwm.gpioFuncFan,
                    activePwm.freq,
                    activePwm.active.tachFuncFan,
                    activePwm.active.tachRate,
                    activePwm.active.bTachSupported ? "true":"false",
                    (UINT32)activePwm.active.controlUnit,
                    activePwm.maxFanEvtMask,
                    activePwm.maxFanMinTimems);
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                activePwmTachCorr=infoParams.coolers[index].data.activePwmTachCorr;
                actPwmTachString += Utility::StrPrintf(
                    "\tFanspeedScaling(SFXP4_12) = 0x%x\n"
                    "\tFanspeedoffset(SFXP4_12) = 0x%x\n"
                    "\tGPIOfunction = %u\n"
                    "\tPWMfrequency = %u\n"
                    "\tTachGPIOfunc = %u\n"
                    "\tTachrate = %u\n"
                    "\tTachsupported = %s\n"
                    "\tControlUnit = %u\n",
                    (UINT32)activePwmTachCorr.activePwm.scaling,
                    (UINT32)activePwmTachCorr.activePwm.offset,
                    activePwmTachCorr.activePwm.gpioFuncFan,
                    activePwmTachCorr.activePwm.freq,
                    activePwmTachCorr.activePwm.active.tachFuncFan,
                    activePwmTachCorr.activePwm.active.tachRate,
                    activePwmTachCorr.activePwm.active.bTachSupported ? "true" : "false",
                    (UINT32)activePwmTachCorr.activePwm.active.controlUnit);
        }

        LW2080_CTRL_FAN_COOLER_STATUS_DATA_ACTIVE statusActive;
        LW2080_CTRL_FAN_COOLER_STATUS_DATA_ACTIVE_PWM statusActivePwm;
        LW2080_CTRL_FAN_COOLER_STATUS_DATA_ACTIVE_PWM_TACH_CORR statusActivePwmTachCorr;
        switch(statusParams.coolers[index].type)
        {
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                statusActive = statusParams.coolers[index].data.active;
                actString += Utility::StrPrintf(
                    "\tLwrrentRPM = %u\n"
                    "\tMinFanLevel(UFXP16_16) = 0x%x\n"
                    "\tMaxFanLevel(UFXP16_16) = 0x%x\n"
                    "\tLwrrentFanLevel(UFXP16_16) = 0x%x\n"
                    "\tTargetFanLevel(UFXP16_16) = 0x%x\n",
                    (UINT32)statusActive.rpmLwrr,
                    (UINT32)statusActive.levelMin,
                    (UINT32)statusActive.levelMax,
                    (UINT32)statusActive.levelLwrrent,
                    (UINT32)statusActive.levelTarget);
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                statusActivePwm = statusParams.coolers[index].data.activePwm;
                actPwmString += Utility::StrPrintf(
                    "\tLwrrent Fanspeed(UFXP16.16) = 0x%x\n"
                    "\tLwrrent RPM = %u\n"
                    "\tMin FanLevel(UFXP16_16) = 0x%x"
                    "\tMax FanLevel(UFXP16_16) = 0x%x\n"
                    "\tLwrrent FanLevel(UFXP16_16) = 0x%x\n"
                    "\tTarget FanLevel(UFXP16_16) = 0x%x\n"
                    "\tMax fan active = %s\n",
                    (UINT32)statusActivePwm.pwmLwrr,
                    (UINT32)statusActivePwm.active.rpmLwrr,
                    (UINT32)statusActivePwm.active.levelMin,
                    (UINT32)statusActivePwm.active.levelMax,
                    (UINT32)statusActivePwm.active.levelLwrrent,
                    (UINT32)statusActivePwm.active.levelTarget,
                    statusActivePwm.bMaxFanActive ? "true" : "false");
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                statusActivePwmTachCorr =
                        statusParams.coolers[index].data.activePwmTachCorr;
                actPwmTachString += Utility::StrPrintf(
                    "\tLast RPM = %u\n"
                    "\tTarget RPM = %u\n"
                    "\tLwrrent Fanspeed(UFXP16.16) = 0x%x\n"
                    "\tLwrrent RPM = %u\n"
                    "\tMin FanLevel(UFXP16_16) = 0x%x\n"
                    "\tMax FanLevel(UFXP16_16) = 0x%x\n"
                    "\tLwrrent FanLevel(UFXP16_16) = 0x%x\n"
                    "\tTarget FanLevel(UFXP16_16) = 0x%x\n"
                    "\tPWM actual (UFXP16_16) = 0x%x\n",
                    statusActivePwmTachCorr.rpmLast,
                    statusActivePwmTachCorr.rpmTarget,
                    (UINT32)statusActivePwmTachCorr.activePwm.pwmLwrr,
                    statusActivePwmTachCorr.activePwm.active.rpmLwrr,
                    (UINT32)statusActivePwmTachCorr.activePwm.active.levelMin,
                    (UINT32)statusActivePwmTachCorr.activePwm.active.levelMax,
                    (UINT32)statusActivePwmTachCorr.activePwm.active.levelLwrrent,
                    (UINT32)statusActivePwmTachCorr.activePwm.active.levelTarget,
                    (UINT32)statusActivePwmTachCorr.pwmActual);
                break;
        }

        LW2080_CTRL_FAN_COOLER_CONTROL_DATA_ACTIVE ctrlActive;
        LW2080_CTRL_FAN_COOLER_CONTROL_DATA_ACTIVE_PWM ctrlActivePwm;
        LW2080_CTRL_FAN_COOLER_CONTROL_DATA_ACTIVE_PWM_TACH_CORR ctrlActivePwmTachCorr;
        switch(ctrlParams.coolers[index].type)
        {
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                ctrlActive = ctrlParams.coolers[index].data.active;
                actString += Utility::StrPrintf(
                    "\tMin RPM = %u\n"
                    "\tMax RPM = %u\n"
                    "\tLevel Sim Active %s\n"
                    "\tOverride value of FanLevel(UFXP16_16) = 0x%x\n",
                    ctrlActive.rpmMin,
                    (UINT32)ctrlActive.rpmMax,
                    (UINT32)ctrlActive.bLevelSimActive ? "true":"false",
                    (UINT32)ctrlActive.levelSim);
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
            ctrlActivePwm = ctrlParams.coolers[index].data.activePwm;
                actPwmString += Utility::StrPrintf(
                    "\tElectrical Min FanSpeed = %u\n"
                    "\tElectrical Max FanSpeed(UFXP16_16) = 0x%x\n"
                    "\tFan state override = %s\n"
                    "\tOverride value for Electrical FanSpeed(UFXP16_16) = 0x%x\n"
                    "\tMin RPM(UFXP16_16) = 0x%x\n"
                    "\tMax RPM(UFXP16_16) = 0x%x\n"
                    "\tLevel Sim Active %s\n"
                    "\tOverride value of FanLevel(UFXP16_16) = 0x%x\n"
                    "\tMax Fan PWM (UFXP16_16) = 0x%x\n",
                    ctrlActivePwm.pwmMin,
                    (UINT32)ctrlActivePwm.pwmMax,
                    ctrlActivePwm.bPwmSimActive ? "true" : "false",
                    (UINT32)ctrlActivePwm.pwmSim,
                    (UINT32)ctrlActivePwm.active.rpmMin,
                    (UINT32)ctrlActivePwm.active.rpmMax,
                    (UINT32)ctrlActivePwm.active.bLevelSimActive ? "true" : "false",
                    (UINT32)ctrlActivePwm.active.levelSim,
                    (UINT32)ctrlActivePwm.maxFanPwm);
                break;
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                ctrlActivePwmTachCorr = ctrlParams.coolers[index].data.activePwmTachCorr;
                actPwmTachString += Utility::StrPrintf(
                    "\tTachometer Feedback Proportional Gain(UFXP16_16) = 0x%x\n"
                    "\tRPM Override = %s\n"
                    "\tOverride value for RPMsettings = %u\n"
                    "\tElectrical Min FanSpeed = %u\n"
                    "\tElectrical Max FanSpeed(UFXP16_16) = 0x%x\n"
                    "\tFan state override = %s\n"
                    "\tOverride value for Electrical FanSpeed(UFXP16_16) = 0x%x\n"
                    "\tMin RPM(UFXP16_16) = 0x%x\n"
                    "\tMax RPM(UFXP16_16) = 0x%x\n"
                    "\tLevel Sim Active %s\n"
                    "\tOverride value of FanLevel(UFXP16_16) = 0x%x\n"
                    "\tPWM floor limit offset (UFXP16_16) = 0x%x\n",
                    (UINT32)ctrlActivePwmTachCorr.propGain,
                    ctrlActivePwmTachCorr.bRpmSimActive ? "true" : "false",
                    ctrlActivePwmTachCorr.rpmSim,
                    ctrlActivePwmTachCorr.activePwm.pwmMin,
                    ctrlActivePwmTachCorr.activePwm.pwmMax,
                    ctrlActivePwmTachCorr.activePwm.bPwmSimActive ? "true" : "false",
                    (UINT32)ctrlActivePwmTachCorr.activePwm.pwmSim,
                    (UINT32)ctrlActivePwmTachCorr.activePwm.active.rpmMin,
                    (UINT32)ctrlActivePwmTachCorr.activePwm.active.rpmMax,
                    (UINT32)ctrlActivePwmTachCorr.activePwm.active.bLevelSimActive ?
                                                                             "true":
                                                                             "false",
                    (UINT32)ctrlActivePwmTachCorr.activePwm.active.levelSim,
                    (UINT32)ctrlActivePwmTachCorr.pwmFloorLimitOffset);
                break;
        }
        if (actString.length() > 0)
        {
            Printf(Tee::PriNormal, "%s\n", actString.c_str());
        }
        if (actPwmString.length() > 0)
        {
            Printf(Tee::PriNormal, "%s\n", actPwmString.c_str());
        }
        if (actPwmTachString.length() > 0)
        {
            Printf(Tee::PriNormal, "%s\n", actPwmTachString.c_str());
        }
    }
    return rc;
}

RC Thermal::GetFanCoolerStatus(LW2080_CTRL_FAN_COOLER_STATUS_PARAMS *pFanCoolerStatusParams)
{
    RC rc;
    UINT32 coolerMask;
    CHECK_RC(GetFanCoolerMask(&coolerMask));
    pFanCoolerStatusParams->super.objMask.super.pData[0] = coolerMask;
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_FAN_COOLER_GET_STATUS
       ,pFanCoolerStatusParams
       ,sizeof(LW2080_CTRL_FAN_COOLER_STATUS_PARAMS));
}

RC Thermal::GetFanCoolerTable(
    LW2080_CTRL_FAN_COOLER_INFO_PARAMS *infoParams,
    LW2080_CTRL_FAN_COOLER_STATUS_PARAMS *statusParams,
    LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS *ctrlParams
)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(pRmHandle,
                LW2080_CTRL_CMD_FAN_COOLER_GET_INFO,
                infoParams,
                sizeof(LW2080_CTRL_FAN_COOLER_INFO_PARAMS)));

    statusParams->super.objMask.super.pData[0] = infoParams->coolerMask;
    CHECK_RC(pLwRm->Control(pRmHandle,
                LW2080_CTRL_CMD_FAN_COOLER_GET_STATUS,
                statusParams,
                sizeof(LW2080_CTRL_FAN_COOLER_STATUS_PARAMS)));

    ctrlParams->coolerMask = infoParams->coolerMask;
    CHECK_RC(pLwRm->Control(pRmHandle,
                LW2080_CTRL_CMD_FAN_COOLER_GET_CONTROL,
                ctrlParams,
                sizeof(LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS)));
    return rc;
}

RC Thermal::SetFanCoolerTable(
    LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS &ctrlParams
)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(pRmHandle,
        LW2080_CTRL_CMD_FAN_COOLER_SET_CONTROL,
        &ctrlParams,
        sizeof(ctrlParams)));
    return rc;
}

RC Thermal::GetFanTestParams
(
    vector<FanTestParams> *testParams
)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    LW2080_CTRL_FAN_TEST_INFO_PARAMS infoParams = {0};
    CHECK_RC(pLwRm->Control(pRmHandle,
                LW2080_CTRL_CMD_FAN_TEST_GET_INFO,
                &infoParams,
                sizeof(infoParams)));

    if (infoParams.testMask == 0)
    {
        Printf(Tee::PriError, "Invalid Fan 2.0+ Test parameter mask.\n");
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 index = 0; index < LW2080_CTRL_FAN_TEST_MAX_TEST; index++)
    {
        if (BIT(index) & infoParams.testMask)
        {
            if (infoParams.test[index].type == LW2080_CTRL_FAN_TEST_COOLER_SANITY)
            {
                FanTestParams params;
                params.coolerIdx =
                    infoParams.test[index].data.coolerSanity.coolerTableIdx;
                params.measTolPct =
                    infoParams.test[index].data.coolerSanity.measurementTolerancePct;
                params.colwTimeMs =
                    infoParams.test[index].data.coolerSanity.colwergenceTimems;
                testParams->push_back(params);
            }
            else
            {
                Printf(Tee::PriError, "Unsupported or undefined Fan 2.0+ test param type\n");
                return RC::SOFTWARE_ERROR;
            }
        }
    }
    return rc;
}

RC Thermal::GetFanArbiterMask(UINT32 *pArbiterMask)
{
    RC rc;
    MASSERT(pArbiterMask);
    LW2080_CTRL_FAN_ARBITER_INFO_PARAMS fanArbiterInfoParams = {};
    CHECK_RC(GetFanArbiterInfo(&fanArbiterInfoParams));
    *pArbiterMask = fanArbiterInfoParams.super.objMask.super.pData[0];
    return rc;
}

RC Thermal::GetFanArbiterInfo(LW2080_CTRL_FAN_ARBITER_INFO_PARAMS *pFanArbiterInfoParams)
{
    MASSERT(pFanArbiterInfoParams);
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_FAN_ARBITER_GET_INFO
       ,pFanArbiterInfoParams
       ,sizeof(LW2080_CTRL_FAN_ARBITER_INFO_PARAMS));
}

RC Thermal::GetFanArbiterStatus(LW2080_CTRL_FAN_ARBITER_STATUS_PARAMS *pFanArbiterStatusParams)
{
    RC rc;
    MASSERT(pFanArbiterStatusParams);
    UINT32 arbiterMask;
    CHECK_RC(GetFanArbiterMask(&arbiterMask));
    pFanArbiterStatusParams->super.objMask.super.pData[0] = arbiterMask;
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_FAN_ARBITER_GET_STATUS
       ,pFanArbiterStatusParams
       ,sizeof(LW2080_CTRL_FAN_ARBITER_STATUS_PARAMS));
}

//-----------------------------------------------------------------------------
//! \brief Determine if a temperature controller is present
//!
//! \param pPresent : Returned boolean indicating whether any temp controller
//!                   is present
//!
//! \return OK if successful, not OK otherwise
RC Thermal::GetIsTempControllerPresent(bool *pPresent)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);

    LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS infoParams = {0};
    CHECK_RC(pLwRm->Control(pRmHandle,
                       LW2080_CTRL_CMD_THERMAL_POLICY_GET_INFO,
                       &infoParams, sizeof(infoParams)));
    *pPresent = infoParams.policyMask > 0;
    return rc;
}

RC Thermal::GetIsAcousticTempControllerPresent(bool *pPresent)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);

    LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS infoParams = {0};
    CHECK_RC(pLwRm->Control(pRmHandle,
                       LW2080_CTRL_CMD_THERMAL_POLICY_GET_INFO,
                       &infoParams, sizeof(infoParams)));
    *pPresent = infoParams.acousticPolicyIdx !=
        LW2080_CTRL_THERMAL_POLICY_TYPE_UNKNOWN;
    return rc;
}

namespace Fix8
{
    // RM passes degrees C in fixed-point S32 with 8 bits for fractional part.
    const float iota = 1.0F/(1<<8);
    float ToFloat(LwS32 x)    { return x * iota; }
    LwS32 FromFloat(float x)  { return static_cast<LwS32>((x / iota) + 0.5F); }
};

RC Thermal::GetCapRange
(
    Thermal::ThermalCapIdx i
   ,float* pMinDegC
   ,float* pRatedDegC
   ,float* pMaxDegC
)
{
    RC rc;
    LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS infoParams = {0};
    CHECK_RC(PrepPolicyInfoParam(&infoParams));
    CHECK_RC(ColwertPolicyIndex(&i, infoParams));

    *pMinDegC   = Fix8::ToFloat(infoParams.policies[i].limitMin);
    *pRatedDegC = Fix8::ToFloat(infoParams.policies[i].limitRated);
    *pMaxDegC   = Fix8::ToFloat(infoParams.policies[i].limitMax);
    return rc;
}

RC Thermal::GetCap
(
    Thermal::ThermalCapIdx i
   ,float * pDegC
)
{
    RC rc;
    LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS infoParams = {0};
    CHECK_RC(PrepPolicyInfoParam(&infoParams));

    LW2080_CTRL_THERMAL_POLICY_CONTROL_PARAMS controlParams = {0};
    controlParams.policyMask = infoParams.policyMask;
    CHECK_RC(GetPolicyControlParam(&controlParams));

    CHECK_RC(ColwertPolicyIndex(&i, infoParams));
    *pDegC = Fix8::ToFloat(controlParams.policies[i].limitLwrr);
    return rc;
}

RC Thermal::SetCap
(
    Thermal::ThermalCapIdx i
   ,float degC
)
{
    RC rc;
    LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS infoParams = {0};
    LW2080_CTRL_THERMAL_POLICY_CONTROL_PARAMS controlParams = {0};

    CHECK_RC(PrepPolicyInfoParam(&infoParams));
    controlParams.policyMask = infoParams.policyMask;
    CHECK_RC(GetPolicyControlParam(&controlParams));

    CHECK_RC(ColwertPolicyIndex(&i, infoParams));
    controlParams.policies[i].limitLwrr = Fix8::FromFloat(degC);

    return LwRmPtr()->ControlBySubdevice(
            m_pSubdevice,
            LW2080_CTRL_CMD_THERMAL_POLICY_SET_CONTROL,
            &controlParams, sizeof(controlParams));
}

RC Thermal::PrintCapPolicyTable (Tee::Priority pri)
{
    RC rc;
    LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS infoParams = {0};
    LW2080_CTRL_THERMAL_POLICY_CONTROL_PARAMS controlParams = {0};
    CHECK_RC(PrepPolicyInfoParam(&infoParams));
    controlParams.policyMask = infoParams.policyMask;
    CHECK_RC(GetPolicyControlParam(&controlParams));

    Printf(pri, "LW2080_CTRL_THERMAL_POLICY_INFO and _CONTROL:\n");
    Printf(pri, " policyMask\t0x%x\n", infoParams.policyMask);
    Printf(pri, " gpsPolicyIdx\t%u\n", infoParams.gpsPolicyIdx);
    Printf(pri, " acousticPolicyIdx\t%u\n", infoParams.acousticPolicyIdx);
    Printf(pri, " memPolicyIdx\t%u\n", infoParams.memPolicyIdx);
    Printf(pri, " gpuSwSlowdownPolicyIdx\t%u\n", infoParams.gpuSwSlowdownPolicyIdx);

    for (UINT32 i = 0; i < LW2080_CTRL_THERMAL_POLICY_MAX_POLICIES; i++)
    {
        if (0 == (infoParams.policyMask & (1<<i)))
            continue;
        const LW2080_CTRL_THERMAL_POLICY_INFO & policy = infoParams.policies[i];

        Printf(pri, " policies[%u].type = %u\n", i, policy.type);
        Printf(pri, " policies[%u].chIdx = %u\n", i, policy.chIdx);
        Printf(pri, " policies[%u].limitMin = %.1f degC\n", i,
               Fix8::ToFloat(policy.limitMin));
        Printf(pri, " policies[%u].limitRated = %.1f degC\n", i,
               Fix8::ToFloat(policy.limitRated));
        Printf(pri, " policies[%u].limitMax = %.1f degC\n", i,
               Fix8::ToFloat(policy.limitMax));

        MASSERT(controlParams.policyMask & (1 << i));
        const LW2080_CTRL_THERMAL_POLICY_CONTROL & policyCtrl = controlParams.policies[i];
        Printf(pri, " policies[%u].limitLwrr = %.1f degC\n", i,
               Fix8::ToFloat(policyCtrl.limitLwrr));
        Printf(pri, " policies[%u].pollingPeriodms = %u\n", i,
               policyCtrl.pollingPeriodms);
        Printf(pri, " policies[%u].bAllowBelowRatedTdp = %u\n", i,
               policyCtrl.bAllowBelowRatedTdp);

        switch (policy.type)
        {
            case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VPSTATE:
            {
                const LW2080_CTRL_THERMAL_POLICY_CONTROL_DATA_DOMGRP & domGrp =
                    policyCtrl.data.dtcVpstate.super;
                Printf(pri, " policies[%u].data.dtcVpstate.super.bRatedTdpVpstateFloor = %u\n",
                       i, domGrp.bRatedTdpVpstateFloor);

                const LW2080_CTRL_THERMAL_POLICY_CONTROL_DATA_DTC & dtc =
                    policyCtrl.data.dtcVpstate.dtc;
                Printf(pri, " policies[%u].data.dtcVpstate.dtc.aggressiveStep = %u\n",
                       i, dtc.aggressiveStep);
                Printf(pri, " policies[%u].data.dtcVpstate.dtc.releaseStep = %u\n",
                       i, dtc.releaseStep);
                Printf(pri, " policies[%u].data.dtcVpstate.dtc.holdSampleThreshold = %u\n",
                       i, dtc.holdSampleThreshold);
                Printf(pri, " policies[%u].data.dtcVpstate.dtc.stepSampleThreshold = %u\n",
                       i, dtc.stepSampleThreshold);
                Printf(pri, " policies[%u].data.dtcVpstate.dtc.thresholdCritical = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdCritical));
                Printf(pri, " policies[%u].data.dtcVpstate.dtc.thresholdAggressive = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdAggressive));
                Printf(pri, " policies[%u].data.dtcVpstate.dtc.thresholdModerate = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdModerate));
                Printf(pri, " policies[%u].data.dtcVpstate.dtc.thresholdRelease = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdRelease));
                Printf(pri, " policies[%u].data.dtcVpstate.dtc.thresholdDisengage = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdDisengage));

                const LW2080_CTRL_THERMAL_POLICY_INFO_DATA_DTC_VPSTATE & dtcVpstate =
                    policy.data.dtcVpstate;
                Printf(pri, " policies[%u].data.dtcVpstate.vpstateNum = %u\n",
                       i, dtcVpstate.vpstateNum);
                Printf(pri, " policies[%u].data.dtcVpstate.vpstateTdp = %u\n",
                       i, dtcVpstate.vpstateTdp);
                break;
            }
            case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VF:
            {
                const LW2080_CTRL_THERMAL_POLICY_CONTROL_DATA_DOMGRP & domGrp =
                    policyCtrl.data.dtcVf.super;
                Printf(pri, " policies[%u].data.dtcVf.super.bRatedTdpVpstateFloor = %u\n",
                       i, domGrp.bRatedTdpVpstateFloor);

                const LW2080_CTRL_THERMAL_POLICY_CONTROL_DATA_DTC & dtc =
                    policyCtrl.data.dtcVf.dtc;
                Printf(pri, " policies[%u].data.dtcVf.dtc.aggressiveStep = %u\n",
                       i, dtc.aggressiveStep);
                Printf(pri, " policies[%u].data.dtcVf.dtc.releaseStep = %u\n",
                       i, dtc.releaseStep);
                Printf(pri, " policies[%u].data.dtcVf.dtc.holdSampleThreshold = %u\n",
                       i, dtc.holdSampleThreshold);
                Printf(pri, " policies[%u].data.dtcVf.dtc.stepSampleThreshold = %u\n",
                       i, dtc.stepSampleThreshold);
                Printf(pri, " policies[%u].data.dtcVf.dtc.thresholdCritical = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdCritical));
                Printf(pri, " policies[%u].data.dtcVf.dtc.thresholdAggressive = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdAggressive));
                Printf(pri, " policies[%u].data.dtcVf.dtc.thresholdModerate = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdModerate));
                Printf(pri, " policies[%u].data.dtcVf.dtc.thresholdRelease = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdRelease));
                Printf(pri, " policies[%u].data.dtcVf.dtc.thresholdDisengage = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdDisengage));

                const LW2080_CTRL_THERMAL_POLICY_INFO_DATA_DTC_VF & dtcVf =
                    policy.data.dtcVf;
                Printf(pri, " policies[%u].data.dtcVf.limitFreqMaxKHz = %u\n",
                       i, dtcVf.limitFreqMaxKHz);
                Printf(pri, " policies[%u].data.dtcVf.limitFreqMinKHz = %u\n",
                       i, dtcVf.limitFreqMinKHz);
                Printf(pri, " policies[%u].data.dtcVf.ratedTdpFreqKHz = %u\n",
                       i, dtcVf.ratedTdpFreqKHz);
                break;
            }
            case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VOLT:
            {
                const LW2080_CTRL_THERMAL_POLICY_CONTROL_DATA_DOMGRP & domGrp =
                    policyCtrl.data.dtcVolt.super;
                Printf(pri, " policies[%u].data.dtcVolt.super.bRatedTdpVpstateFloor = %u\n",
                       i, domGrp.bRatedTdpVpstateFloor);

                const LW2080_CTRL_THERMAL_POLICY_CONTROL_DATA_DTC & dtc =
                    policyCtrl.data.dtcVolt.dtc;
                Printf(pri, " policies[%u].data.dtcVolt.dtc.aggressiveStep = %u\n",
                       i, dtc.aggressiveStep);
                Printf(pri, " policies[%u].data.dtcVolt.dtc.releaseStep = %u\n",
                       i, dtc.releaseStep);
                Printf(pri, " policies[%u].data.dtcVolt.dtc.holdSampleThreshold = %u\n",
                       i, dtc.holdSampleThreshold);
                Printf(pri, " policies[%u].data.dtcVolt.dtc.stepSampleThreshold = %u\n",
                       i, dtc.stepSampleThreshold);
                Printf(pri, " policies[%u].data.dtcVolt.dtc.thresholdCritical = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdCritical));
                Printf(pri, " policies[%u].data.dtcVolt.dtc.thresholdAggressive = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdAggressive));
                Printf(pri, " policies[%u].data.dtcVolt.dtc.thresholdModerate = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdModerate));
                Printf(pri, " policies[%u].data.dtcVolt.dtc.thresholdRelease = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdRelease));
                Printf(pri, " policies[%u].data.dtcVolt.dtc.thresholdDisengage = %.1f degC\n",
                       i, Fix8::ToFloat(dtc.thresholdDisengage));

                const LW2080_CTRL_THERMAL_POLICY_INFO_DATA_DTC_VOLT & dtcVolt =
                    policy.data.dtcVolt;
                Printf(pri, " policies[%u].data.dtcVolt.freqTdpKHz = %u\n",
                       i, dtcVolt.freqTdpKHz);
                Printf(pri, " policies[%u].data.dtcVolt.pstateIdxTdp = %u\n",
                       i, dtcVolt.pstateIdxTdp);
                Printf(pri, " policies[%u].data.dtcVolt.voltageMaxuV = %u\n",
                       i, dtcVolt.voltageMaxuV);
                Printf(pri, " policies[%u].data.dtcVolt.voltageMinuV = %u\n",
                       i, dtcVolt.voltageMinuV);
                Printf(pri, " policies[%u].data.dtcVolt.voltageStepuV = %u\n",
                       i, dtcVolt.voltageStepuV);
                break;
            }
            default:
                MASSERT(!"unhandled thermal policy type");
                break;
        }
    }
    return rc;
}

RC Thermal::PrepPolicyInfoParam
(
    LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS * pParam
)
{
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_THERMAL_POLICY_GET_INFO,
        pParam, sizeof(*pParam));
}

RC Thermal::GetPolicyControlParam
(
    LW2080_CTRL_THERMAL_POLICY_CONTROL_PARAMS * pParam
)
{
    return LwRmPtr()->ControlBySubdevice(
        m_pSubdevice,
        LW2080_CTRL_CMD_THERMAL_POLICY_GET_CONTROL,
        pParam, sizeof(*pParam));
}

RC Thermal::ColwertPolicyIndex
(
    ThermalCapIdx * pIdx // In/Out
   ,const LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS & infoParams
)
{
    ThermalCapIdx idx = *pIdx;
    if (idx == GPS)
    {
        idx = static_cast<ThermalCapIdx>(infoParams.gpsPolicyIdx);
    }
    else if (idx == Acoustic)
    {
        idx = static_cast<ThermalCapIdx>(infoParams.acousticPolicyIdx);
    }
    else if (idx == MEMORY)
    {
        idx = static_cast<ThermalCapIdx>(infoParams.memPolicyIdx);
    }
    else if (idx == SW_SLOWDOWN)
    {
        idx = static_cast<ThermalCapIdx>(infoParams.gpuSwSlowdownPolicyIdx);
    }
    else if (idx >= LW2080_CTRL_THERMAL_POLICY_MAX_POLICIES)
    {
        Printf(Tee::PriNormal, "ThermalCap policy index %u invalid.\n", idx);
        Printf(Tee::PriNormal,
               "Index must be 0 to %d or %d (GPS), %d (Acoustic), or %d (MEMORY)\n",
               LW2080_CTRL_THERMAL_POLICY_MAX_POLICIES-1, GPS, Acoustic, MEMORY);
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (idx >= LW2080_CTRL_THERMAL_POLICY_MAX_POLICIES ||
        (0 == (infoParams.policyMask & (1<<idx))))
    {
        Printf(Tee::PriLow, " %s thermal policy is not supported in the VBIOS on GPU %u\n",
               ThermalCapPolicyIndexToStr(*pIdx), m_pSubdevice->GetGpuInst());
        return RC::UNSUPPORTED_FUNCTION;
    }
    *pIdx = idx;
    return OK;
}

RC Thermal::GetChipTempsPerBJT
(
    map<string, ThermalSensors::TsenseBJTTempMap> *pBjtTemps
)
{
    MASSERT(pBjtTemps);
    MASSERT(m_pSbdevSensorsBjt);

    RC rc;
    vector<ThermalSensors::Sensor> sensorsPresent;
    bool gpuBjtSensorPresent = false;
    CHECK_RC(m_pSbdevSensorsBjt->IsSensorPresent(ThermalSensors::GPU_BJT, &gpuBjtSensorPresent));
    if (gpuBjtSensorPresent)
    {
        sensorsPresent.push_back(ThermalSensors::GPU_BJT);
    }

    bool lwlBjtSensorPresent = false;
    CHECK_RC(m_pSbdevSensorsBjt->IsSensorPresent(ThermalSensors::LWL_BJT, &lwlBjtSensorPresent));
    if (lwlBjtSensorPresent)
    {
        sensorsPresent.push_back(ThermalSensors::LWL_BJT);
    }

    bool sysBjtSensorPresent = false;
    CHECK_RC(m_pSbdevSensorsBjt->IsSensorPresent(ThermalSensors::SYS_BJT, &sysBjtSensorPresent));
    if (sysBjtSensorPresent)
    {
        sensorsPresent.push_back(ThermalSensors::SYS_BJT);
    }
    
    bool sciBjtSensorPresent = false;
    CHECK_RC(m_pSbdevSensorsBjt->IsSensorPresent(ThermalSensors::SCI_BJT, &sciBjtSensorPresent));
    if (sciBjtSensorPresent)
    {
        sensorsPresent.push_back(ThermalSensors::SCI_BJT);
    }    
    
    if (sensorsPresent.empty())
    {
        Printf(Tee::PriError, "BJT temperature sensor is only supported on Ampere and beyond\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(m_pSbdevSensorsBjt->GetTempSnapshot(pBjtTemps, sensorsPresent));
    return rc;
}

RC Thermal::GetDramTemps(ThermalSensors::DramTempMap *pTemp)
{
    MASSERT(pTemp);
    if (!m_pSbdevSensorsDramTemp)
    {
        Printf(Tee::PriError, "DRAM temperature sensor is only supported on GA10X+\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    bool sensorPresent = false;
    CHECK_RC(m_pSbdevSensorsDramTemp->IsSensorPresent(ThermalSensors::DRAM_TEMP, &sensorPresent));
    if (!sensorPresent)
    {
        Printf(Tee::PriError, "DRAM temperature sensor is not supported\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(m_pSbdevSensorsDramTemp->GetDramTempSnapshot(pTemp));

    return rc;
}

//-----------------------------------------------------------------------------
// OverTemp temperature monitor - checked at the end of each test
//-----------------------------------------------------------------------------
OverTempCounter::OverTempCounter(TestDevice *pTestDevice)
  : GpuErrCounter(pTestDevice)
{
    m_OverTempData.resize(NUM_TEMPS);
    for (UINT32 i = 0; i < NUM_TEMPS; i++)
    {
        m_OverTempData[i].bActive = false;
        m_OverTempData[i].OverTempCount = 0;
        m_OverTempData[i].ExpectedOverTempCount = 0;
        m_OverTempData[i].IsExpecting = false;

        // Set some ridilwlous limits that will never be hit (i.e. by default
        // range checking is disabled on all temperature types unless a limit
        // is specifically set)
        m_OverTempData[i].Min = -999;
        m_OverTempData[i].Max = 999;
    }

     m_OverTempData[RM_EVENT].bActive = true;
}

//-----------------------------------------------------------------------------
//! \brief Initialize the over temp counter
//!
//! \param pSub : Pointer to the GpuSubdevice where the counter is active
//!
//! \return OK if successfull, not OK otherwise
RC OverTempCounter::Initialize()
{
    // right now we have to rely on user to enable temperature background monitor
    // thread to read Volterra/Internal/External
    // Ideally, RM is able to read voltera temperature, and we can get RM to
    // signal an event for us to log OverTemp events.

    return ErrCounter::Initialize("OverTempCount",
                                  NUM_TEMPS, 0, nullptr,
                                  MODS_TEST, OVERTEMP_PRI);
}

//-----------------------------------------------------------------------------
//! \brief Update the count for a specific temperature type
//!
//! \param type : Temperature type to update
//! \param Temp : Temperature reading
//!
//! \return OK if successfull, not OK otherwise
RC OverTempCounter::Update(TempType type, INT32 Temp)
{
    if ((type >= NUM_TEMPS) || (type == RM_EVENT))
        return RC::SOFTWARE_ERROR;

    if (!m_OverTempData[type].bActive)
        return OK;

    if ((Temp > m_OverTempData[type].Max) || (Temp < m_OverTempData[type].Min))
    {
        if(m_OverTempData[type].IsExpecting)
        {
            m_OverTempData[type].ExpectedOverTempCount++;
            m_OverTempData[type].ExpectedTempData.push_back(Temp);
        }
        else
        {
            m_OverTempData[type].OverTempCount++;
            TempLimits limits = make_pair(m_OverTempData[type].Min, m_OverTempData[type].Max);
            map<TempLimits, vector<INT32>>& tempData = m_OverTempData[type].TempData;
            if (tempData.find(limits) == tempData.end())
            {
                tempData[limits] = vector<INT32>();
            }
            tempData[limits].push_back(Temp);
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Set the allowed range for a specific temperature type
//!
//! \param type : Type of temperature to set the range on
//! \param Min  : Minimum temperature
//! \param Max  : Maximum temperature
//!
//! \return OK if successfull, not OK otherwise
RC OverTempCounter::SetTempRange(TempType type, INT32 Min, INT32 Max)
{
    if ((type >= NUM_TEMPS) ||(type == RM_EVENT))
        return RC::SOFTWARE_ERROR;

    m_OverTempData[type].Min = Min;
    m_OverTempData[type].Max = Max;
    m_OverTempData[type].bActive = true;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the allowed range for a specific temperature type
//!
//! \param type  : Type of temperature to get the range from
//! \param pMin  : Returned minimum temperature
//! \param pMax  : Returned maximum temperature
//!
//! \return OK if successfull, not OK otherwise
RC OverTempCounter::GetTempRange(TempType type, INT32 * pMin, INT32 * pMax)
{
    MASSERT(pMin);
    MASSERT(pMax);

    if ((type >= NUM_TEMPS) || (type == RM_EVENT) ||
        (pMin == nullptr) || (pMax == nullptr))
    {
        return RC::SOFTWARE_ERROR;
    }

    *pMin = m_OverTempData[type].Min;
    *pMax = m_OverTempData[type].Max;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Enable or disable monitoring of temperature ranges (i.e. non
//!        RM_EVENT over temperature counters)
//!
//! \param bEnable : true to enable, false to disable
void OverTempCounter::EnableRangeMonitoring(bool bEnable)
{
    for (UINT32 i = 0; i < NUM_TEMPS; i++)
    {
        if (i != RM_EVENT)
        {
            m_OverTempData[i].bActive = true;
        }
    }
}

//--------------------------------------------------------------------
//! \brief Begin expecting errors on the indicated counter
//!
// Return true if we are expecting overtemp errors, false otherwise
bool OverTempCounter::IsExpecting(int idx)
{
    MASSERT(idx < NUM_TEMPS);
    return(m_OverTempData[idx].IsExpecting);
}

//--------------------------------------------------------------------
//! \brief Begin expecting errors on the indicated counter
//!
//! This method should be called at the start of any method that
//! deliberately injects overtemp errors, to let this object know that the
//! errors should be ignored until the corresponding
//! EndExpectingErrors() is called.  The caller should lock the mutex
//! before calling this method, and not free it until after calling
//! EndExpectingErrors().
//!
void OverTempCounter::BeginExpectingErrors(int idx)
{
    MASSERT(idx < NUM_TEMPS);
    MASSERT(!m_OverTempData[idx].IsExpecting);

    m_OverTempData[idx].IsExpecting = true;
}

//--------------------------------------------------------------------
//! \brief End expecting errors on the indicated counter
//!
//! This method should be called at the end of any method that
//! deliberately injects ECC errors.  See BeginExpectedErrors() for
//! more details.
//!
void OverTempCounter::EndExpectingErrors(int idx)
{
    MASSERT(idx < NUM_TEMPS);
    MASSERT(m_OverTempData[idx].IsExpecting);

    m_OverTempData[idx].IsExpecting = false;

}

//--------------------------------------------------------------------
//! \brief End expecting errors on all counters
//!
//! This method is equivalent to calling EndExpectingErrors() on all
//! indices that have had BeginExpectingErrors called on it.  Mostly
//! used as an argument to Utility::CleanupOnReturn() so that callers
//! can use RAII to do cleanup if the caller aborts.
//!
void OverTempCounter::EndExpectingAllErrors()
{
    for (UINT32 ii = 0; ii < NUM_TEMPS; ++ii)
    {
        m_OverTempData[ii].IsExpecting = false;
    }
}

//-----------------------------------------------------------------------------
//! \brief Read the error counts for the counter
//!
//! \param pCount : Pointer to the returned counts
//!
//! \return OK if successfull, not OK otherwise
RC OverTempCounter::ReadErrorCount(UINT64 * pCount)
{
    RC rc;
    for (UINT32 tt = INTERNAL_TEMP; tt < NUM_TEMPS; tt++)
    {
        // RM_EVENT are handled seperately
        if (tt != RM_EVENT)
        {
            pCount[tt] = m_OverTempData[tt].OverTempCount;
        }
    }

    UINT32 tmp;
    CHECK_RC(GetTestDevice()->GetOverTempCounter(&tmp));
    pCount[RM_EVENT] = static_cast<UINT64>(tmp);

    return RC::OK;
}
//-----------------------------------------------------------------------------
//! \brief Read the expected error counts for the counter
//!
//! \param pCount : Pointer to the returned counts
//!
//! \return OK if successfull, not OK otherwise
RC OverTempCounter::ReadExpectedErrorCount(UINT64 * pCount)
{
    RC rc;
    for (UINT32 tt = INTERNAL_TEMP; tt < NUM_TEMPS; tt++)
    {
        // RM_EVENT are handled seperately
        if (tt != RM_EVENT)
        {
            pCount[tt] = m_OverTempData[tt].ExpectedOverTempCount;
        }
    }

    UINT32 tmp;
    CHECK_RC(GetTestDevice()->GetExpectedOverTempCounter(&tmp));
    pCount[RM_EVENT] = static_cast<UINT64>(tmp);

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Callback for when the counter detects an error
//!
//! \param pData : Data associated with the error
RC OverTempCounter::OnError(const ErrorData *pData)
{
    bool bNewErrors = false;
    if (!pData->bCheckExitOnly)
    {
        TestDevice* pTestDevice = GetTestDevice();
        GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
        for (UINT32 ii = 0; ii < NUM_TEMPS; ++ii)
        {
            UINT64 NewOverTempCount = pData->pNewErrors[ii];

            if (NewOverTempCount)
            {
                Printf(Tee::PriError, "%s ",
                       pTestDevice->GetName().c_str());
                switch (ii)
                {
                    case INTERNAL_TEMP:
                        Printf(Tee::PriNormal,
                               "Internal Sensor - %llu new over temp events :\n",
                               NewOverTempCount);
                        break;
                    case EXTERNAL_TEMP:
                        Printf(Tee::PriNormal,
                               "External Sensor - %llu new over temp events :\n",
                               NewOverTempCount);
                        break;

                    case RM_EVENT:
                        Printf (Tee::PriNormal,
                                "RM Thermal Events: %llu new events\n",
                                NewOverTempCount);
                        MASSERT(pGpuSubdevice);
                        pGpuSubdevice->GetThermal()->PrintHwfsEventData();
                        break;
                    case VOLTERRA_TEMP_1:
                    case VOLTERRA_TEMP_2:
                    case VOLTERRA_TEMP_3:
                    case VOLTERRA_TEMP_4:
                        Printf(Tee::PriNormal,
                               "Volterra Sensor - %llu new over temp events "
                               "on slave %d :\n",
                               NewOverTempCount,
                               ii - VOLTERRA_TEMP_1 + 1);
                        break;
                    case SMBUS_TEMP:
                        Printf(Tee::PriNormal,
                               "Smbus Sensor - %llu new over temp events :\n",
                               NewOverTempCount);
                        break;
                    case IPMI_TEMP:
                        Printf(Tee::PriNormal,
                               "Ipmi Sensor - %llu new over temp events :\n",
                               NewOverTempCount);
                        break;
                    case MEMORY_TEMP:
                        Printf(Tee::PriNormal,
                               "Memory Sensor - %llu new over temp events :\n",
                               NewOverTempCount);
                        break;
                    case TSENSE_OFFSET_MAX_TEMP:
                        Printf(Tee::PriNormal,
                               "Tsense Offset Max Sensor - %llu new over temp events :\n",
                               NewOverTempCount);
                        break;
                    default:
                    {
                        MASSERT(!"Illegal switch statement in OverTempCounter");
                    }

                }

                const UINT32 TEMPS_PER_LINE = 8;
                for (auto it = m_OverTempData[ii].TempData.begin();
                     it != m_OverTempData[ii].TempData.end(); it++)
                {
                    const vector<INT32>& tempData = it->second;
                    const TempLimits& limits = it->first;
                    INT32 min = _INT32_MAX;
                    INT32 max = _INT32_MIN;

                    for (UINT32 ti = 0;
                         ti < (UINT32)tempData.size(); ti++)
                    {
                        if (!(ti % TEMPS_PER_LINE))
                        {
                            if (ti != 0)
                                Printf(Tee::PriNormal, "\n");
                            Printf(Tee::PriNormal, "    ");
                        }
                        Printf(Tee::PriNormal,
                               "%d  ", tempData[ti]);

                        if (tempData[ti] < min)
                        {
                            min = tempData[ti];
                        }
                        if (tempData[ti] > max)
                        {
                            max = tempData[ti];
                        }
                    }
                    Printf(Tee::PriNormal, "\n");
                    Printf(Tee::PriNormal,
                           "%u thermal events outside acceptable range.Detected Min = %d degrees,"
                           "Max = %d degrees. Allowed Range (%d degrees, %d degrees)\n",
                           (UINT32)tempData.size(), min, max,
                           limits.first, limits.second);
                }
                m_OverTempData[ii].TempData.clear();
            }
        }
    }

    for (UINT32 ii = 0; ii < NUM_TEMPS; ++ii)
    {
        if (pData->pNewErrors[ii])
        {
            bNewErrors = true;
            break;
        }
    }
    return bNewErrors ? RC::THERMAL_SENSOR_ERROR : OK;
}

//--------------------------------------------------------------------
//! \brief Override the base-class OnCheckpoint() method.
//!
/* virtual */ void OverTempCounter::OnCheckpoint
(
    const CheckpointData *pData
) const
{
    const UINT64 *pCount = pData->pCount;
    Printf(Tee::PriError, "Over Temp errors on %s:\n",
           GetTestDevice()->GetName().c_str());
    Printf(Tee::PriNormal,
           "\tInternal = %llu, External = %llu, RM = %llu\n",
           pCount[INTERNAL_TEMP], pCount[EXTERNAL_TEMP], pCount[RM_EVENT]);
    Printf(Tee::PriNormal,
           "\tVolterra (1) = %llu, Volterra (2) = %llu, "
           "Volterra (3) = %llu, Volterra (4) = %llu\n",
           pCount[VOLTERRA_TEMP_1], pCount[VOLTERRA_TEMP_2],
           pCount[VOLTERRA_TEMP_3], pCount[VOLTERRA_TEMP_4]);
    Printf(Tee::PriNormal, "\tSmbus = %llu\n", pCount[SMBUS_TEMP]);
    Printf(Tee::PriNormal, "\tIpmi = %llu\n", pCount[IPMI_TEMP]);
    Printf(Tee::PriNormal, "\tMemory = %llu\n", pCount[MEMORY_TEMP]);
}

/* static */ const char *Thermal::FanCoolerActiveControlUnitToString(UINT32 controlUnit)
{
    switch (controlUnit)
    {
        case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_NONE:
            return "NONE";
        case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_PWM:
            return "PWM";
        case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_RPM:
            return "RPM";
        default:
            MASSERT(!"Unknown Fan Cooler Active Control Unit");
            return "UNKNOWN";
    }
}

/* static */ const char *Thermal::FanArbiterCtrlActionToString(UINT32 fanCtrlAction)
{
    switch (fanCtrlAction)
    {
        case LW2080_CTRL_FAN_CTRL_ACTION_ILWALID:
            return "INVALID";
        case LW2080_CTRL_FAN_CTRL_ACTION_SPEED_CTRL:
            return "SPEED_CTRL";
        case LW2080_CTRL_FAN_CTRL_ACTION_STOP:
            return "STOP";
        case LW2080_CTRL_FAN_CTRL_ACTION_RESTART:
            return "RESTART";
        default:
            MASSERT(!"Unknown Fan Arbiter Ctrl Action");
            return "UNKNOWN";
    }
}

/* static */ const char *Thermal::FanArbiterModeToString(UINT32 mode)
{
    switch (mode)
    {
        case LW2080_CTRL_FAN_ARBITER_MODE_ILWALID:
            return "INVALID";
        case LW2080_CTRL_FAN_ARBITER_MODE_MAX:
            return "MAX";
        default:
            MASSERT(!"Unknown Fan Arbiter Mode");
            return "UNKNOWN";
    }
}
