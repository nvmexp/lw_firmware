/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Macros for defining set/get properties

#ifndef INCLUDED_SETGET_H
#define INCLUDED_SETGET_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

// Macro to create basic property setter and getter functions
#define SETGET_PROP(propName, resulttype)                               \
    resulttype Get##propName() const { return m_##propName; }           \
    RC Set##propName(resulttype val) { m_##propName = move(val); return OK; }

#define SETGET_PROP_ENUM(propName, podType, enumType, enumMax)                            \
    podType Get##propName() const { return static_cast<podType>(m_##propName); }          \
    RC Set##propName(podType val)                                                         \
    {                                                                                     \
        if (val > static_cast<podType>(enumMax))                                          \
        {                                                                                 \
            Printf(Tee::PriError,                                                         \
                   "%s : " #propName " out of range, requested = %u, maximum = %u\n",     \
                   GetName().c_str(),                                                     \
                   static_cast<UINT32>(val),                                              \
                   static_cast<UINT32>(enumMax));                                         \
            return RC::BAD_PARAMETER;                                                     \
        }                                                                                 \
        m_##propName = static_cast<enumType>(val);                                        \
        return RC::OK;                                                                    \
    }

// Macro to create basic property getter functions
#define GET_PROP(propName, resulttype)                          \
    resulttype Get##propName() const { return m_##propName; }

// Macro to create basic property setter functions
#define SET_PROP(propName, resulttype)                                  \
    RC Set##propName(resulttype val) { m_##propName = move(val); return OK; }

// Macro to allow custom setter/getter functions
#define SETGET_PROP_LWSTOM(propName, resulttype)        \
    resulttype Get##propName() const;                   \
    RC Set##propName(resulttype val);

// Macro to allow custom property getter functions
#define GET_PROP_LWSTOM(propName, resulttype)   \
    resulttype Get##propName() const;

// Macro to allow custom property setter function
#define SET_PROP_LWSTOM(propName, resulttype)   \
    RC Set##propName(resulttype val);

// Macro to allow custom setter/getter functions for JsArrays
#define SETGET_JSARRAY_PROP_LWSTOM(propName)    \
    RC Get##propName(JsArray *val) const;       \
    RC Set##propName(JsArray *val);

// Macro to allow setter/getter functions from a custom variable
#define SETGET_PROP_VAR(propName, resultType, cVar)             \
    resultType Get##propName() const { return cVar; }           \
    RC Set##propName(resultType val){ cVar = move(val); return OK; }

// Macro to allow setter/getter functions from custom functions
#define SETGET_PROP_FUNC(propName, resultType, setFunc, getFunc) \
    resultType Get##propName() const { return getFunc(); }       \
    RC Set##propName(resultType val)                             \
        { RC rc; CHECK_RC(setFunc(val)); return rc; }

#endif // !INCLUDED_SETGET_H
