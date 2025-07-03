/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file voltaxusb.h -- Implemetation class for XUSB Host Controller on Volta

// Volta is used for testing USB features prior to Bring-up
// XusbHostCtrlImpl covers all functionality needed for Volta
// This class is added to follow MODS interface naming convention
#pragma once

#include "gpu/usb/hostctrl/xusbhostctrlimpl.h"

class VoltaXusb : public XusbHostCtrlImpl
{
public:
    VoltaXusb() = default;
    virtual ~VoltaXusb() {}
};
