/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLPYTHON_H
#define INCLUDED_UTLPYTHON_H

// This file contains the headers necessary for calling into Python.
// This should be included in any UTL source file that needs to call
// the Python interpreter or reference a Python object.

#include "pybind11/embed.h"
#include "pybind11/stl.h"
#include "pybind11/stl_bind.h"

namespace py = pybind11;

#define UTL_BIND_CONSTANT(cls, value, name)    \
    cls.def_property_readonly_static(name,     \
        []                                     \
        (                                      \
            py::object                         \
        )                                      \
        {                                      \
            return static_cast<UINT32>(value); \
        });                                    \

#define UTL_DOCSTRING(functionName, functionDesc)                                       \
    (string(functionDesc) + kwargs->GetDocstring(functionName, functionDesc)).c_str()   \

#endif
