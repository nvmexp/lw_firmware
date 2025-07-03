/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011      by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _GPIO_ENGINE_H_
#define _GPIO_ENGINE_H_

// The gpio_engine allows MODS mdiag tests access to monitor and control
// of the GPIOs through Escape reads/writes.  These values need to stay
// consistent with TOT/diag/test/gpio_engine.vx.
// The following Escape mappings are supported (<X> is the Gpio number
// from 0 through 31, inclusive, although not all 32 Gpio may be connected
// to the GPU):
//
//     Gpio_<X>_pull
//         This configures the pullup/pulldown on the Gpio.  Supported values
//         are UP, DOWN, or NONE.  Typically, this should be configured to
//         either UP or DOWN.  However, NONE may be useful for testing the
//         weakpull in the Gpio pad.  The gpio_engine defaults to UP or DOWN,
//         depending on the Gpio.
#define GPIO_PULL_NONE        0
#define GPIO_PULL_UP          1
#define GPIO_PULL_DOWN        2

//
//     Gpio_<X>_drv
//         This configures the drive mode of the Gpio by the gpio_engine.
//         Supported values are NONE, OPEN_DRAIN, OPEN_SOURCE, or FULL.  This
//         should be configured to be compatible with other drivers (including
//         GPU) on the Gpio.  When there are 2 or more drivers on the Gpio,
//         all drivers should be configured to OPEN_DRAIN or OPEN_SOURCE and
//         the appropriate pullup/pulldown applied to the GPIO.  If the GPU is
//         the sole driver, the gpio_engine should configure the drive to NONE.
//         If the gpio_engine is the sole driver, it should be configured to
//         FULL.  The gpio_engine defaults to NONE.
#define GPIO_DRV_NONE         0
#define GPIO_DRV_OPEN_DRAIN   1
#define GPIO_DRV_OPEN_SOURCE  2
#define GPIO_DRV_FULL         3

//
//     Gpio_<x>_state
//         This configures the value that the gpio_engine will drive the Gpio
//         to.  Supported values are 1 and 0.  Note that the gpio_engine uses
//         the drive mode to determine whether high impedance (Z) should be
//         driven instead of a 1 or 0.  The gpio_engine defaults to 0, but
//         this is a dont care since the drive mode defaults to NONE.
#define GPIO_STATE_LOW        0
#define GPIO_STATE_HIGH       1

//
//     Gpio_<X>
//         This mapping is intended as read-only and reflects the current state
//         of the Gpio.  This includes drivers and pull values from the GPU,
//         testbench, and gpio_engine.  Typically, the GPIO should be 1 (HIGH)
//         or 0 (LOW).  However, there may be times when there are no drivers
//         or pull on the Gpio (HIZ) or conflicting drivers (X).  This
//         usually indicates a setup or programming error.  Although these
//         cases are not expected, they are supported by the gpio_engine and
//         provide a way for the mdiag test to identify these error conditions.
#define GPIO_RD_LOW           0
#define GPIO_RD_HIGH          1
#define GPIO_RD_HIZ           2
#define GPIO_RD_X             3

#endif  // _GPIO_ENGINE_H_
