/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/macros.h"
#include "spdm_cppintfc.h"

/*!
 * @brief Handler function called whenever ASSERT(x) macro is triggered in
 * libspdm code. Clearly warns of failure, and potentially triggers MODS assert.
 * 
 * Since functional MASSERT(x) macro requires DEBUG flag defined during MODS
 * build, and this library is built separately from MODS, we cannot assume it
 * will be present. Therefore, will usually be compiled out - leave just in case.
 */
void debug_assert(const char8 *file_name, uintn line_number, const char8 *description)
{
    Printf(Tee::PriError, "Assert hit in libspdm!\n");
    if (file_name != nullptr)
    {
        Printf(Tee::PriError, "Assert location: file %s line %lld.\n", file_name, line_number);
    }
    if (description != nullptr)
    {
        Printf(Tee::PriError, "Assert description: %s.\n", description);
    }

    MASSERT(0);
}

/*!
 * @brief Logging function for libspdm code. Colwert logging priority to
 * Tee::Priority levels, so that we can control granularity same as other logs.
 */
void debug_print(uintn error_level, const char8 *format, ...)
{
    Tee::Priority priority;

    // Colwert libspdm priority to Tee::Priority.
    switch (error_level)
    {
        case DEBUG_VERBOSE:
            priority = Tee::PriLow;
            break;
        case DEBUG_INFO:
            priority = Tee::PriNormal;
            break;
        case DEBUG_ERROR:
            priority = Tee::PriError;
            break;
        default:
            // Invalid priorty level, assume low.
            priority = Tee::PriLow;
            break;
    }

    va_list arguments;
    va_start(arguments, format);

    // Must use ModsExterlwAPrintf, since we have variable argument list.
    ModsExterlwAPrintf(priority, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL,
                       format, arguments);

    va_end(arguments);
}
