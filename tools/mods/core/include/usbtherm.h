/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_USBTHERM_H
#define INCLUDED_USBTHERM_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#ifndef INCLUDED_SETGET_H
#include "setget.h"
#endif

#ifndef INCLUDED_TEE
#include "tee.h"
#endif

struct usb_dev_handle;

//! \brief Provides a wrapper around temperature access functions
class UsbTemperatureSensor
{
public:
    UsbTemperatureSensor();
    ~UsbTemperatureSensor();

    RC Initialize();
    RC Close();

    RC OpenUsbDev();
    RC CloseUsbDev();
    RC GetTemperature(float *temperature);
    bool IsOpen();

    SETGET_PROP(UsbDev, usb_dev_handle *);

private:
    usb_dev_handle *m_UsbDev;
    static const int MAX_OPEN_RETRY_COUNT;
    static const float TEMPERATURE_SCALE;
    static const float TEMPERATURE_OFFSET;
};

#endif
