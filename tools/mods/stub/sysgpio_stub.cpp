/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cheetah/include/sysutil.h"

//--------------------------------------------------------------
// GPIO Calls
// There is no system-level GPIO equivalent on x86 system right now
// Return OK to maintain transparency
//--------------------------------------------------------------
namespace SysUtil
{
    namespace Gpio
    {
        RC MaskWrite(UINT32 Reg, UINT32 Gpio, UINT32 value) { return OK; }
        RC Enable(UINT32 Gpio) { return OK; }
        RC Disable(UINT32 Gpio) { return OK; }
        RC Write(UINT32 Gpio, UINT32 Value) { return OK; }
        RC Read(UINT32 Gpio, UINT32 *pData) { return OK; }
        RC DirectionInput(UINT32 Gpio) { return OK; }
        RC DirectionOutput(UINT32 Gpio, UINT32 Value) { return OK; }
        RC InterruptDisable(UINT32 Gpio) { return OK; }
        RC InterruptEnable(UINT32 Gpio, UINT32 Type) { return OK; }
        UINT32 Compose(UINT32 bank, UINT32 port, UINT32 bit) { return OK; }
        RC Status(UINT32 Gpio) { return OK; }
        RC GetStat(UINT32 Gpio, bool *pIsGpio, bool *pIsOut, bool *pIsSet) { return OK; }

        // Return UNSUPPORTED_FUNCTION for search operations to indicate it to the user
        RC SearchKey(string Str, UINT32 *pGpioKey, bool IsPrint)
        {
            return RC::UNSUPPORTED_FUNCTION;
        }

        RC SearchAlias(string AliasName, string *pGpioName, UINT32 *pGpioKey, bool IsPrint, bool IsExact)
        {
            return RC::UNSUPPORTED_FUNCTION;
        }
    }
}
//--------------------------------------------------------------
