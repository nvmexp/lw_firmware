/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/dbgsink.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/xp.h"

DebuggerSink::DebuggerSink()
{
}

/* virtual */ void DebuggerSink::DoPrint
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
   MASSERT(str != nullptr);

   Xp::DebugPrint(str, strLen);
}
