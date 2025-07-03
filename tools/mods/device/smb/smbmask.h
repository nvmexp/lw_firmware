/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file smbmask.h
//! \brief Header file with class for Smbus Mask
//!

//! Contains class definition for Smbus Mask This class will control
//! the toggling of a mask bit for the smbus controller.  It is only
//! needed when dealing with Intel chipsets.  Some Intel chipsets hide
//! the Smbus controller.  When toggled on, the necessary masking bit
//! is shut off to allow the Smbus controller to be accessed.
//! Toggling off turns the masking bit back on, hiding the Smbus
//! controller.

#ifndef INCLUDED_SMBMASK_H
#define INCLUDED_SMBMASK_H

#ifndef INCLUDED_RC_H
    #include "core/include/rc.h"
#endif
#ifndef INCLUDED_TYPES_H
    #include "core/include/types.h"
#endif
#ifndef INCLUDED_TEE_H
    #include "core/include/tee.h"
#endif

//! \brief Class definition for Smbus Mask
//-----------------------------------------------------------------------------

class SmbusMask
{
 public:
    RC ToggleOn();
    void ToggleOff();
 private:
    static bool IsToggled;
};

#endif //INCLUDED_SMBMASK_H
