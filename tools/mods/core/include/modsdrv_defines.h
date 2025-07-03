/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   modsdrv_defines.h
 * @brief  Definitions shared between MODS and other modules
 *
 * Declares enums and structures usable in both C and C++. The
 * enums and structures defined here are also used in modsdrv.h
 * by other modules (written in C).
 *
 */

#pragma once

typedef enum
{
     MODS_CLKDEV_UNKNOWN
    ,MODS_CLKDEV_TEGRA_DC
    ,MODS_CLKDEV_TEGRA_HDMI
    ,MODS_CLKDEV_TEGRA_SOR
    ,MODS_CLKDEV_TEGRA_DPAUX
    ,MODS_CLKDEV_TEGRA_DSI
    ,MODS_CLKDEV_TEGRA_PWM
    ,MODS_CLKDEV_TEGRA_PLL_P
    ,MODS_CLKDEV_TEGRA_PLL_P_OUT
    ,MODS_CLKDEV_TEGRA_PLL_D
    ,MODS_CLKDEV_TEGRA_PLL_D_OUT
    ,MODS_CLKDEV_TEGRA_HOST1X
    ,MODS_CLKDEV_TEGRA_PLL_C
    ,MODS_CLKDEV_TEGRA_PLL_C4
    ,MODS_CLKDEV_TEGRA_PLL_C_OUT
    ,MODS_CLKDEV_TEGRA_PLL_C_CBUS
    ,MODS_CLKDEV_TEGRA_FLOOR_CBUS
    ,MODS_CLKDEV_TEGRA_KFUSE
    ,MODS_CLKDEV_TEGRA_3D
    ,MODS_CLKDEV_TEGRA_2D
    ,MODS_CLKDEV_TEGRA_EPP
    ,MODS_CLKDEV_TEGRA_2D_EMC
    ,MODS_CLKDEV_TEGRA_2D_CBUS
    ,MODS_CLKDEV_TEGRA_I2C
    ,MODS_CLKDEV_TEGRA_SPI
    ,MODS_CLKDEV_TEGRA_QSPI
    ,MODS_CLKDEV_TEGRA_APBDMA
    ,MODS_CLKDEV_TEGRA_VDE
    ,MODS_CLKDEV_TEGRA_BSEV
    ,MODS_CLKDEV_TEGRA_MSENC
    ,MODS_CLKDEV_TEGRA_TSEC
    ,MODS_CLKDEV_TEGRA_EMC
    ,MODS_CLKDEV_TEGRA_ISO_EMC
    ,MODS_CLKDEV_TEGRA_FLOOR_EMC
    ,MODS_CLKDEV_TEGRA_CLK_M
    ,MODS_CLKDEV_TEGRA_PLL_DP
    ,MODS_CLKDEV_TEGRA_PLLG_REF_OUT
    ,MODS_CLKDEV_TEGRA_PLL_X
} ModsClockDomain;

typedef enum
{
     MODS_OS_NONE
    ,MODS_OS_DOS
    ,MODS_OS_WINDOWS
    ,MODS_OS_LINUX
    ,MODS_OS_LINUXRM
    ,MODS_OS_MAC
    ,MODS_OS_MACRM
    ,MODS_OS_WINSIM
    ,MODS_OS_LINUXSIM
    ,MODS_OS_WINMFG
    ,MODS_OS_UEFI
    ,MODS_OS_ANDROID
    ,MODS_OS_WINDA
    ,MODS_OS_LINDA
} ModsOperatingSystem;

typedef enum
{
     PRI_DEBUG = 1
    ,PRI_LOW
    ,PRI_NORMAL
    ,PRI_WARN
    ,PRI_HIGH
    ,PRI_ERR
    ,PRI_ALWAYS
} ModsPrintPri;

