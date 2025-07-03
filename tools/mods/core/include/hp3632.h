/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_HP3632_H
#define INCLUDED_HP3632_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

namespace Hp3632
{
   // public functions
   RC Initialize();
   RC Uninitialize();
   RC Identify(string &Returlwal);
   RC SetVoltage(FLOAT64 Voltage);
   RC SetLwrrent(FLOAT64 Current);
   RC GetVoltage(FLOAT64 *Voltage);
   RC GetLwrrent(FLOAT64 *Current);
   RC EnableOutput(bool Enable);
   RC Range15();
   RC Range30();
   RC DisplayText(string &Text);
   RC SetLwrrentProtection(FLOAT64 Current);
   RC SetVoltageProtection(FLOAT64 Current);
}

#endif // ! INCLUDED_HP3632_H
