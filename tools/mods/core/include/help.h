/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef __HELP_H__
#define __HELP_H__

#include "rc.h"
#include <regex>

struct JSObject;
struct JSContext;

namespace Help {

    RC JsObjHelp
    (
        JSObject   * pObject,
        JSContext  * pContext,
        regex      * pRegex,
        UINT32       IndentLevel,
        const char * pObjectName,
        bool       * pbObjectFound
    );

    RC JsPropertyHelp
    (
        JSObject   * pObject,
        JSContext  * pContext,
        regex      * pregex,
        UINT32       IndentLevel
    );

} // namespace Help

#endif // __HELP_H__ is defined
