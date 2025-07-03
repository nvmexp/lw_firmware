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

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Standard Output sink.
// Will be enabled when mods console takes over.

#include "core/include/stdoutsink.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include <stdio.h>

StdoutSink::StdoutSink() :
    m_IsEnabled(false)
{
    // Enable StdoutSink during early setup until ConsoleManager can take over
    Enable();
}

/* virtual */ void StdoutSink::DoPrint
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
    MASSERT(str != 0);

    if (!m_IsEnabled)
        return;

    if (fwrite(str, 1, strLen, stdout) != strLen)
        return;
    fflush(stdout);
}

RC StdoutSink::Enable()
{
    m_IsEnabled = true;
    return OK;
}

RC StdoutSink::Disable()
{
    m_IsEnabled = false;
    return OK;
}

/* virtual */ bool StdoutSink::DoPrintBinary
(
    const UINT08* data,
    size_t        size
)
{
    MASSERT(size);

    if (!m_IsEnabled)
        return false;

    const size_t wrote = fwrite(data, 1, size, stdout);

    return wrote == size;
}

