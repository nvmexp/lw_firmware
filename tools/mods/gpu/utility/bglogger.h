/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   bglogger.h
 *
 * @brief  Header for background logging utility
 *
 */

#pragma once

#include "core/include/types.h"
#include "core/include/rc.h"

class BgMonitorFactories;
struct JSContext;
struct JSObject;

//! BgLogger used to periodically monitor various system parameters such
//! as temperature, speedos, power, etc.
namespace BgLogger
{
    RC Shutdown();

    void PauseLogging();
    void UnpauseLogging();
    bool IsPaused();
    RC   WaitTillPaused();
    RC   WaitTillPaused(FLOAT64 timeout);

    //! RAII class for pausing the background monitors
    class PauseBgLogger
    {
    public:
        PauseBgLogger();
        ~PauseBgLogger();

        RC Wait();
    private:
        // Non-copyable
        PauseBgLogger(const PauseBgLogger&);
        PauseBgLogger& operator=(const PauseBgLogger&);
    };

    FLOAT64 GetReadIntervalMs();

    extern bool d_Verbose;
    extern bool d_MleOnly;

    // JavaScript pointers needed to access Json logging.
    // Updated after calling StartMonitor().
    extern JSContext * d_pJSContext;
    extern JSObject  * d_pJSObject;
    enum BgPrintMask
    {
        FLAG_NO_PRINTS = 0x1, // no logger prints
        FLAG_DUMPSTATS = 0x2, // dump stats
    };
}

// Identifier values for the supported BgMontortypes
// Note: The numeric values for these should not
// change as tools like MLA depend on the number
// to correctly identify the monitor type.
enum BgMonitorType : UINT08
{
    GPUTEMP_SENSOR = 0,
    INT_TEMP_SENSOR = 1,
    MEM_TEMP_SENSOR = 2,
    EXT_TEMP_SENSOR = 3,
    SMBUS_TEMP_SENSOR = 4,
    IPMI_TEMP_SENSOR = 5,
    SOC_TEMP_SENSOR = 6,
    FAN_RPM_SENSOR = 7,
    POWER_SENSOR = 8,
    VOLTERRA_SENSOR = 9,
    SPEEDO = 10,
    SPEEDO_TSOSC = 11,
    GPU_VOLTAGE = 13,
    GPU_CLOCKS = 14,
    GPU_PART_CLOCKS = 15,
    GPC_LIMITS = 16,
    THERMAL_SLOWDOWN = 17,
    TEST_FIXTURE_TEMPERATURE = 18,
    TEST_FIXTURE_RPM = 19,
    PCIE_SPEED = 20,
    LWRRENT_SENSOR = 21,
    LWSWITCH_TEMP_SENSOR = 22,
    LWSWITCH_VOLT_SENSOR = 23,
    TEST_NUM = 24,
    TEMP_CTRL = 25,
    GR_STATUS = 26,
    DGX_BMC_SENSOR = 27,
    CPU_USAGE = 28,
    EDPP = 29,
    BJT_TEMP_SENSOR = 30,
    VRM_POWER = 31,
    GPU_USAGE = 32,
    IPMI_LWSTOM_SENSOR = 33,
    DRAM_TEMP_SENSOR = 34,
    FAN_COOLER_STATUS = 35,
    FAN_POLICY_STATUS = 36,
    FAN_ARBITER_STATUS = 37,
    MULTIRAILCWC_LIMITS = 38,
    CABLE_CONTROLLER_TEMP_SENSOR = 39,
    SPEEDO_CPM = 40,
    ADC_SENSED_VOLTAGE = 41,
    RAM_ASSIST_ENGAGED = 42,
    POWER_POLICIES_LOGGER = 43,
    GPIO = 44,
    NUM_MONITORS
};
