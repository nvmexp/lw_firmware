/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef DEVICE_INTERFACES_H
#define DEVICE_INTERFACES_H

#include "core/include/device.h"
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>
#include <boost/preprocessor/facilities/empty.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>

// PCIE
#include "gpu/pcie/fermipcie.h"
#include "gpu/pcie/keplerpcie.h"
#include "gpu/pcie/maxwellpcie.h"
#include "gpu/pcie/pascalpcie.h"
#include "gpu/pcie/turingpcie.h"
#include "gpu/pcie/voltapcie.h"
#include "gpu/pcie/amperepcie.h"
#include "gpu/pcie/ga10xpcie.h"
#include "gpu/pcie/hopperpcie.h"
#include "gpu/pcie/ad10bpcie.h"
#include "gpu/pcie/socpcie.h"
#include "gpu/pcie/lwswitchpcie.h"
#include "gpu/pcie/limerockpcie.h"
#include "gpu/pcie/lagunapcie.h"
#include "gpu/pcie/ibmnpupcie.h"
#include "gpu/pcie/simpcie.h"

// Lwlink
#include "gpu/lwlink/pascallwlink.h"
#include "gpu/lwlink/voltalwlink.h"
#include "gpu/lwlink/amperelwlink.h"
#include "gpu/lwlink/ga10xlwlink.h"
#if LWCFG(GLOBAL_ARCH_HOPPER)
    #include "gpu/lwlink/hopperlwlink.h"
#endif
#include "gpu/lwlink/xavier_tegra_lwlink.h"
#include "gpu/lwlink/xavier_mfg_lwlink.h"
#include "gpu/lwlink/turinglwlink.h"
#include "gpu/lwlink/lwswitchlwlink.h"
#include "gpu/lwlink/limerocklwlink.h"
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
    #include "gpu/lwlink/lagunalwlink.h"
#endif
#include "gpu/lwlink/ibmnpulwlink.h"
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
    #include "gpu/lwlink/ibmnpulwlink_p9p.h"
    #include "gpu/lwlink/trexlwlink.h"
#endif
#include "gpu/lwlink/simlwlink.h"
#include "gpu/lwlink/simlwswitchlwlink.h"
#include "gpu/lwlink/simlimerocklwlink.h"
#include "gpu/lwlink/simebridgelwlink.h"

// LwlinkThroughputCounters
#include "gpu/lwlink/tpcounters/lwlthroughputcounters_v1.h"
#include "gpu/lwlink/tpcounters/lwlthroughputcounters_v2.h"
#include "gpu/lwlink/tpcounters/lwlthroughputcounters_v2sw.h"
#include "gpu/lwlink/tpcounters/lwlthroughputcounters_v3.h"
#include "gpu/lwlink/tpcounters/lwlthroughputcounters_v4.h"

// Xusb Host Controller
#include "gpu/usb/hostctrl/xusbhostctrlimpl.h"
#include "gpu/usb/hostctrl/voltaxusb.h"
#include "gpu/usb/hostctrl/turingxusb.h"

// Port Policy Controller
#include "gpu/usb/portpolicyctrl/portpolicyctrlimpl.h"
#include "gpu/usb/portpolicyctrl/turingppc.h"
#include "gpu/usb/portpolicyctrl/voltappc.h"

// I2c
#include "gpu/i2c/simi2c.h"
#include "gpu/i2c/gpui2c.h"
#include "gpu/i2c/lwswitchi2c.h"

// Gpio
#include "gpu/gpio/gpugpio.h"
#include "gpu/gpio/lwswitchgpio.h"

// C2C
#include "gpu/c2c/hopperc2c.h"

// Devices cannot inherit from a DeviceInterfaces if the interfaces are only forward declared,
// so all the devices would need to know exactly which interface implementations they use so
// they can include the proper header files. This ruins the abstraction granted by DeviceInterfaces.
// So instead, we need to include all of the interface implementations here so that
// they are all available to the devices that use DeviceInterfaces, so then which implementations
// they use can be easily altered completely inside gpulist.h.

// InterfaceList represents a list of interface classes implemented by a particular GPU
// Using variadic templates, the list can also be empty
//
// InterfaceList inherits from all of the interfaces listed, and then a particular GPU will
// inherit from that InterfaceList and therefore also inherit from all those interfaces.
template <typename... Interfaces>
class InterfaceList : public Interfaces...
{};

// DeviceInterfaces is overridden below for each GPU, each defining its own InterfaceList
// which may or may not be empty
//
// Then each GpuSubdevice derived class inherits from both GpuSubdevice and
// GpusubInterface<Device::GpuId>::interfaces which is a typedef of an InterfaceList for that GPU.
//
// For example, GV100GpuSubdevice will inherit from GpusubInterface<Device::GV100::interfaces
// which is the same as InterfaceList<VoltaLwLink> which inherits from VoltaLwLink,
// so GV100GpuSubdevice will be implementing the VoltaLwLink interface
template <Device::LwDeviceId id>
struct DeviceInterfaces
{
    typedef InterfaceList<> interfaces;
};

#if defined(INCLUDE_LWLINK)
    #define INCLUDE_LWLINK_SET 1
#else
    #define INCLUDE_LWLINK_SET 0
#endif

#if defined(INCLUDE_XUSB)
    #define INCLUDE_XUSB_SET 1
#else
    #define INCLUDE_XUSB_SET 0
#endif

// Filter out interfaces that require specific #defines to be set
#define INCLUDE_INTERFACE(include_set, interface) BOOST_PP_IF(include_set, BOOST_PP_IF(BOOST_PP_IS_EMPTY(interface), BOOST_PP_EMPTY, interface BOOST_PP_COMMA), BOOST_PP_EMPTY)()
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant, Family,  \
                       SubdeviceType, FrameBufferType, IsmType, FuseType,      \
                       FsType, GCxType, HBMType, HwrefDir,                     \
                       DispHwrefDir, LwLinkType, XusbHostCtrlType,             \
                       PortPolicyCtrlType, LwLinkThroughputCountersType, ... ) \
    template <> struct DeviceInterfaces<Device::GpuId>                         \
    { typedef InterfaceList<INCLUDE_INTERFACE(INCLUDE_LWLINK_SET, LwLinkType)  \
                            INCLUDE_INTERFACE(INCLUDE_XUSB_SET, XusbHostCtrlType) \
                            INCLUDE_INTERFACE(INCLUDE_XUSB_SET, PortPolicyCtrlType) \
                            INCLUDE_INTERFACE(INCLUDE_LWLINK_SET, LwLinkThroughputCountersType) \
                            __VA_ARGS__> interfaces; };
#define DEFINE_DUP_GPU(DevIdStart, DevIdEnd, ChipId, GpuId) // don't care
#define DEFINE_OBS_GPU(DevIdStart, DevIdEnd, ChipId, GpuId,     \
                       Constant, Family) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU

#define DEFINE_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, Constant, \
                       HwrefDir, DispDir, ...)                       \
    template <> struct DeviceInterfaces<Device::LwId>                \
    { typedef InterfaceList<__VA_ARGS__> interfaces; };
#define DEFINE_OBS_LWL_DEV(...) // don't care
#include "core/include/lwlinklist.h"
#undef DEFINE_LWL_DEV

#endif

#undef INCLUDE_INTERFACE
#undef INCLUDE_XUSB_SET
#undef INCLUDE_LWLINK_SET
