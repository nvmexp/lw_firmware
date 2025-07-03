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

#include "gpu/reghal/js_reghal.h"
#include "gpu/reghal/reghal.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/rc.h"
#include "ctrl/ctrl2080.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "core/include/deprecat.h"
#include "core/include/js_uint64.h"

JsRegHal::JsRegHal(RegHal* pRegHal)
    : m_pJsRegHalObj(nullptr)
    , m_pRegHal(pRegHal)
{
    return;
}

//-----------------------------------------------------------------------------
static void C_JsRegHal_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsRegHal * pJsRegHal;
    // Delete the C++
    pJsRegHal = (JsRegHal *)JS_GetPrivate(cx, obj);
    delete pJsRegHal;
};

//-----------------------------------------------------------------------------
static JSClass JsRegHal_class =
{
    "JsRegHal",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsRegHal_finalize
};

//-----------------------------------------------------------------------------
static SObject JsRegHal_Object
(
    "JsRegHal",
    JsRegHal_class,
    0,
    0,
    "RegHal JS Object"
);

//------------------------------------------------------------------------------
// Create a JS Object representation of the current associated
// RegHal object
RC JsRegHal::CreateJSObject(JSContext *cx, JSObject *obj)
{
    // Only create one JSObject per RegHal object
    if (m_pJsRegHalObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsRegHal.\n");
        return OK;
    }

    m_pJsRegHalObj = JS_DefineObject(cx,
                                  obj, // GpuSubdevice object
                                  "Regs", // Property name
                                  &JsRegHal_class,
                                  JsRegHal_Object.GetJSObject(),
                                  JSPROP_READONLY);

    if (!m_pJsRegHalObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    // Store the current JsRegHal instance into the private area
    // of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsRegHalObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsRegHal.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsRegHalObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//------------------------------------------------------------------------------
// Return the private JS data - C++ Perf object.
//
RegHal * JsRegHal::GetRegHalObj()
{
    MASSERT(m_pRegHal);
    return m_pRegHal;
}

//------------------------------------------------------------------------------
// Wrappers for public reghal functions from reghal.cpp
// Many of these functions are overloaded to take in different
// enumerations for the first argument, and/or differnet numbers
// of arguments. The if/else structure in these functions
// is to deal with that.
// Note that most of these functions will hit assertion failures
// within reghal.cpp if an invalid enumeration value is given.

// Read a register.
// 1-4 args. First arg can be optional domain index,
// followed by address, field, or value enum,
// followed by optional index or array of indexes,
// followed by optional 32-bit or 64-bit value (only for address or field enum).
namespace
{
#define CHECK_JS(f) if (JS_FALSE == (f)) return JS_FALSE;
    //------------------------------------------------------------------------------
    JSBool ParseRegHalParams(
        JSContext       * pContext
        ,uintN            NumArguments
        ,jsval          * pArguments
        ,UINT32         * pDomainIndex
        ,UINT32         * pSpaceEnum
        ,UINT32         * pRegEnum
        ,bool           * pbDomainIdxPresent
        ,bool           * pbSpaceEnumPresent
        ,vector<UINT32> * pRegisterIndexes
        ,UINT32         * pFinalUINT32
        ,UINT64         * pFinalUINT64
    )
    {
        MASSERT(!pFinalUINT32 || !pFinalUINT64); // Use at most 1
        JavaScriptPtr pJavaScript;
        UINT32 lwrArg = 0;
        UINT32 numRemainingArgs = NumArguments;

        // Parse the first value, which is usually a register enum
        if (OK != pJavaScript->FromJsval(pArguments[lwrArg], pRegEnum))
        {
            pJavaScript->Throw(pContext,
                               RC::BAD_PARAMETER,
                               "Unable to parse first reghal argument");
            return JS_FALSE;
        }
        ++lwrArg;

        // If a domainIndex is allowed with this command, and the
        // first value is not a register type enum, then it must be a
        // domainIndex
        if (pDomainIndex && pbDomainIdxPresent)
        {
            *pDomainIndex       = 0;
            *pbDomainIdxPresent = false;

            if ((*pRegEnum & MODS_REGISTER_TYPE_MASK) == 0)
            {
                if (NumArguments == 1)
                {
                    pJavaScript->Throw(pContext,
                                       RC::BAD_PARAMETER,
                                       "No register enum specified with domain");
                    return JS_FALSE;
                }

                *pDomainIndex  = *pRegEnum;
                if (OK != pJavaScript->FromJsval(pArguments[lwrArg], pRegEnum))
                {
                    pJavaScript->Throw(pContext,
                                       RC::BAD_PARAMETER,
                                       "Unable to parse reghal enum value");
                    return JS_FALSE;
                }
                *pbDomainIdxPresent = true;
                ++lwrArg;
            }
        }

        if (pSpaceEnum && pbSpaceEnumPresent)
        {
            *pSpaceEnum         = 0;
            *pbSpaceEnumPresent = false;

            if ((*pRegEnum & MODS_REGISTER_TYPE_MASK) == 0)
            {
                if (NumArguments == 2)
                {
                    pJavaScript->Throw(pContext,
                                       RC::BAD_PARAMETER,
                                       "No register enum specified with domain and space");
                    return JS_FALSE;
                }

                *pSpaceEnum  = *pRegEnum;
                if (OK != pJavaScript->FromJsval(pArguments[lwrArg], pRegEnum))
                {
                    pJavaScript->Throw(pContext,
                                       RC::BAD_PARAMETER,
                                       "Unable to parse reghal enum value");
                    return JS_FALSE;
                }
                *pbSpaceEnumPresent = true;
                ++lwrArg;
            }
        }

        if (!(*pRegEnum & (MODS_REGISTER_ADDRESS | MODS_REGISTER_FIELD | MODS_REGISTER_VALUE)))
        {
            pJavaScript->Throw(pContext,
                               RC::BAD_PARAMETER,
                               "Unknown register enum type in reghal access");
            return JS_FALSE;
        }

        // If this is an address or field register enum, and the
        // caller expects a final 32-bit or 64-bit "value" argument,
        // then parse the final argument.
        if (*pRegEnum & (MODS_REGISTER_ADDRESS | MODS_REGISTER_FIELD))
        {
            if (pFinalUINT32)
            {
                if (OK != pJavaScript->FromJsval(pArguments[numRemainingArgs-1],
                                                 pFinalUINT32))
                {
                    pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                                       "Error parsing JsRegHal parameters");
                    return JS_FALSE;
                }
                --numRemainingArgs;
            }
            else if (pFinalUINT64)
            {
                JSObject* pObj = nullptr;
                JsUINT64* pJsUINT64 = nullptr;
                if (OK == pJavaScript->FromJsval(pArguments[numRemainingArgs-1],
                                                 &pObj))
                {
                    JSClass* pJsClass = JS_GetClass(pObj);
                    if ((pJsClass->flags & JSCLASS_HAS_PRIVATE) &&
                        strcmp(pJsClass->name, "UINT64") == 0)
                    {
                        pJsUINT64 = static_cast<JsUINT64*>(JS_GetPrivate(pContext, pObj));
                    }
                }
                if (!pJsUINT64)
                {
                    pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                                       "Error parsing JsRegHal parameters");
                    return JS_FALSE;
                }
                *pFinalUINT64 = pJsUINT64->GetValue();
                --numRemainingArgs;
            }
        }

        // Parse the array indexes, if any
        if (pRegisterIndexes)
        {
            pRegisterIndexes->clear();

            if (lwrArg < numRemainingArgs)
            {
                // One index or index array parameter
                UINT32 temp = 0;
                RC tmpRc;

                pRegisterIndexes->clear();
                if (JSVAL_IS_NUMBER(pArguments[lwrArg]))
                {
                    tmpRc = pJavaScript->FromJsval(pArguments[lwrArg], &temp);
                    pRegisterIndexes->push_back(temp);
                }
                else
                {
                    tmpRc = pJavaScript->FromJsval(pArguments[lwrArg],
                                                   pRegisterIndexes);
                }
                ++lwrArg;

                if (tmpRc != OK)
                {
                    pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                                       "Error parsing JsRegHal register index");
                    return JS_FALSE;
                }
            }
        }

        // Make sure we've parsed all the args
        if (lwrArg != numRemainingArgs)
        {
            if (lwrArg < numRemainingArgs)
            {
                pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                                   "Too many JsRegHal parameters");
            }
            else
            {
                pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                                   "Too few JsRegHal parameters");
            }
            return JS_FALSE;
        }

        return JS_TRUE;
    }
}

CLASS_PROP_READWRITE_LWSTOM_FULL
(
    JsRegHal,
    RegPrivCheckMode,
    "RegHal's register privilege checking mode",
    0,
    0
);

P_(JsRegHal_Get_RegPrivCheckMode)
{
    UINT32 result = RegHal::GetDebugPrivCheckMode();
    RC rc = JavaScriptPtr()->ToJsval(result, pValue);
    if (rc != RC::OK)
    {
        JavaScriptPtr()->Throw(pContext, rc, "Failed to get JsRegHal.RegPrivCheckMode");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(JsRegHal_Set_RegPrivCheckMode)
{
    UINT32 Value;
    RC rc = JavaScriptPtr()->FromJsval(*pValue, &Value);
    if (rc != RC::OK)
    {
        JavaScriptPtr()->Throw(pContext, rc, "Failed to set JsRegHal.RegPrivCheckMode");
        return JS_FALSE;
    }
    rc = RegHal::SetDebugPrivCheckMode(Value);
    if (rc != RC::OK)
    {
        JavaScriptPtr()->Throw(pContext, rc, "Error Setting JsRegHal.RegPrivCheckMode");
        *pValue = JSVAL_NULL;
        return JS_TRUE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsRegHal, Read32, 3, "read a register")
{
    STEST_HEADER(1, 3,
                 "Usage: RegHal.Read32((domainIndex), ModsGpuRegAddress/Field, (idx/idx_array)))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex;
    UINT32 enumVal;
    bool   bDomainIdxPresent;
    vector<UINT32> registerIndexes;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               nullptr,
                               &enumVal,
                               &bDomainIdxPresent,
                               nullptr,
                               &registerIndexes,
                               nullptr,
                               nullptr));

    if (enumVal & MODS_REGISTER_VALUE)
    {
        pJavaScript->Throw(pContext,
                           RC::BAD_PARAMETER,
                           "Error oclwrred in Read32: Values may not be read");
        return JS_FALSE;
    }

    UINT32 regVal = 0;
    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            regVal = pRegs->Read32(domainIndex,
                                   static_cast<ModsGpuRegAddress>(enumVal),
                                   registerIndexes);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            regVal = pRegs->Read32(domainIndex,
                                   static_cast<ModsGpuRegField>(enumVal),
                                   registerIndexes);
        }
    }
    else
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            regVal = pRegs->Read32(static_cast<ModsGpuRegAddress>(enumVal),
                                   registerIndexes);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            regVal = pRegs->Read32(static_cast<ModsGpuRegField>(enumVal),
                                   registerIndexes);
        }
    }

    const RC rc = pJavaScript->ToJsval(regVal, pReturlwalue);
    if (OK != rc)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in Read32: Unable to colwert result to jsval");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsRegHal, Read64, 3, "Read a 64 bit register")
{
    STEST_HEADER(1, 3,
                 "Usage: RegHal.Read64((domainIndex), ModsGpuRegAddress/Field, (idx/idx_array)))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex;
    UINT32 enumVal;
    bool   bDomainIdxPresent;
    vector<UINT32> registerIndexes;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               nullptr,
                               &enumVal,
                               &bDomainIdxPresent,
                               nullptr,
                               &registerIndexes,
                               nullptr,
                               nullptr));

    if (enumVal & MODS_REGISTER_VALUE)
    {
        pJavaScript->Throw(pContext,
                           RC::BAD_PARAMETER,
                           "Error oclwrred in Read64: Values may not be read");
        return JS_FALSE;
    }

    UINT64 regVal = 0;
    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            regVal = pRegs->Read64(domainIndex,
                                   static_cast<ModsGpuRegAddress>(enumVal),
                                   registerIndexes);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            regVal = pRegs->Read64(domainIndex,
                                   static_cast<ModsGpuRegField>(enumVal),
                                   registerIndexes);
        }
    }
    else
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            regVal = pRegs->Read64(static_cast<ModsGpuRegAddress>(enumVal),
                                   registerIndexes);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            regVal = pRegs->Read64(static_cast<ModsGpuRegField>(enumVal),
                                   registerIndexes);
        }
    }

    JsUINT64* pJsUINT64 = new JsUINT64(regVal);
    MASSERT(pJsUINT64);
    RC rc;
    if ((rc = pJsUINT64->CreateJSObject(pContext)) != OK ||
        (rc = pJavaScript->ToJsval(pJsUINT64->GetJSObject(), pReturlwalue)) != OK)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in Read64: Unable to create return value");
        return JS_FALSE;
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Write a register.
// 1-4 arguments, first can be any enumeration,
// next two arguments can be indeces,
// last argument is a value unless the first
// argument was a value enum.
JS_SMETHOD_LWSTOM(JsRegHal, Write32, 5, "Write 32 bit address")
{
    STEST_HEADER(1, 5,
        "Usage: RegHal.Write32((domainIndex), (spaceEnum), ModsGpuRegAddress/Field/Value, "
        "(idx/idx_array), (value))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex = 0, spaceVal = 0;
    UINT32 enumVal = 0;
    bool   bDomainIdxPresent = false, bSpaceEnumPresent = false;
    vector<UINT32> registerIndexes;
    UINT32 value = 0;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               &spaceVal,
                               &enumVal,
                               &bDomainIdxPresent,
                               &bSpaceEnumPresent,
                               &registerIndexes,
                               &value,
                               nullptr));

    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (bSpaceEnumPresent)
        {
            if (enumVal & MODS_REGISTER_ADDRESS)
            {
                pRegs->Write32(domainIndex,
                               static_cast<RegHalTable::RegSpace>(spaceVal),
                               static_cast<ModsGpuRegAddress>(enumVal),
                               registerIndexes,
                               value);
            }
            else if (enumVal & MODS_REGISTER_FIELD)
            {
                pRegs->Write32(domainIndex,
                               static_cast<RegHalTable::RegSpace>(spaceVal),
                               static_cast<ModsGpuRegField>(enumVal),
                               registerIndexes,
                               value);
            }
            else if (enumVal & MODS_REGISTER_VALUE)
            {
                pRegs->Write32(domainIndex,
                               static_cast<RegHalTable::RegSpace>(spaceVal),
                               static_cast<ModsGpuRegValue>(enumVal),
                               registerIndexes);
            }
        }
        else
        {
            if (enumVal & MODS_REGISTER_ADDRESS)
            {
                pRegs->Write32(domainIndex,
                               static_cast<ModsGpuRegAddress>(enumVal),
                               registerIndexes,
                               value);
            }
            else if (enumVal & MODS_REGISTER_FIELD)
            {
                pRegs->Write32(domainIndex,
                               static_cast<ModsGpuRegField>(enumVal),
                               registerIndexes,
                               value);
            }
            else if (enumVal & MODS_REGISTER_VALUE)
            {
                pRegs->Write32(domainIndex,
                               static_cast<ModsGpuRegValue>(enumVal),
                               registerIndexes);
            }
        }
    }
    else
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            pRegs->Write32(static_cast<ModsGpuRegAddress>(enumVal),
                           registerIndexes,
                           value);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            pRegs->Write32(static_cast<ModsGpuRegField>(enumVal),
                           registerIndexes,
                           value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            pRegs->Write32(static_cast<ModsGpuRegValue>(enumVal),
                           registerIndexes);
        }
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Write a 64 bit register.
// 1-4 arguments, first can be any enumeration,
// next two arguments can be indeces,
// last argument is a value unless the first
// argument was a value enum.
JS_SMETHOD_LWSTOM(JsRegHal, Write64, 5, "Write a 64 bit address")
{
    STEST_HEADER(1, 5,
        "Usage: RegHal.Write64((domainIndex), (spaceEnum), ModsGpuRegAddress/Field/Value, "
        "(idx/idx_array), (UINT64)");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex = 0, spaceVal = 0;
    UINT32 enumVal = 0;
    bool   bDomainIdxPresent = false, bSpaceEnumPresent = false;
    vector<UINT32> registerIndexes;
    UINT64 value = 0LLU;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               &spaceVal,
                               &enumVal,
                               &bDomainIdxPresent,
                               &bSpaceEnumPresent,
                               &registerIndexes,
                               nullptr,
                               &value));

    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (bSpaceEnumPresent)
        {
            if (enumVal & MODS_REGISTER_ADDRESS)
            {
                pRegs->Write64(domainIndex,
                               static_cast<RegHalTable::RegSpace>(spaceVal),
                               static_cast<ModsGpuRegAddress>(enumVal),
                               registerIndexes,
                               value);
            }
            else if (enumVal & MODS_REGISTER_FIELD)
            {
                pRegs->Write64(domainIndex,
                               static_cast<RegHalTable::RegSpace>(spaceVal),
                               static_cast<ModsGpuRegField>(enumVal),
                               registerIndexes,
                               value);
            }
            else if (enumVal & MODS_REGISTER_VALUE)
            {
                pRegs->Write64(domainIndex,
                               static_cast<RegHalTable::RegSpace>(spaceVal),
                               static_cast<ModsGpuRegValue>(enumVal),
                               registerIndexes);
            }
        }
        else
        {
            if (enumVal & MODS_REGISTER_ADDRESS)
            {
                pRegs->Write64(domainIndex,
                               static_cast<ModsGpuRegAddress>(enumVal),
                               registerIndexes,
                               value);
            }
            else if (enumVal & MODS_REGISTER_FIELD)
            {
                pRegs->Write64(domainIndex,
                               static_cast<ModsGpuRegField>(enumVal),
                               registerIndexes,
                               value);
            }
            else if (enumVal & MODS_REGISTER_VALUE)
            {
                pRegs->Write64(domainIndex,
                               static_cast<ModsGpuRegValue>(enumVal),
                               registerIndexes);
            }
        }
    }
    else
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            pRegs->Write64(static_cast<ModsGpuRegAddress>(enumVal),
                           registerIndexes,
                           value);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            pRegs->Write64(static_cast<ModsGpuRegField>(enumVal),
                           registerIndexes,
                           value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            pRegs->Write64(static_cast<ModsGpuRegValue>(enumVal),
                           registerIndexes);
        }
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Write a broadcast register.
// 1-4 arguments, first can be any enumeration,
// next two arguments can be indeces,
// last argument is a value unless the first
// argument was a value enum.
JS_SMETHOD_LWSTOM(JsRegHal, Write32Broadcast, 5, "Write 32 bit broadcast address")
{
    STEST_HEADER(1, 5,
        "Usage: RegHal.Write32Broadcast((domainIndex), (spaceEnum), ModsGpuRegAddress, "
        "(idx/idx_array), (value))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex = 0, spaceVal = 0;
    UINT32 enumVal = 0;
    bool   bDomainIdxPresent = false, bSpaceEnumPresent = false;
    vector<UINT32> registerIndexes;
    UINT32 value = 0;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               &spaceVal,
                               &enumVal,
                               &bDomainIdxPresent,
                               &bSpaceEnumPresent,
                               &registerIndexes,
                               &value,
                               nullptr));

    if (!(enumVal & MODS_REGISTER_ADDRESS))
    {
        pJavaScript->Throw(pContext,
                           RC::BAD_PARAMETER,
                           "Error oclwrred in Write32Broadcast: Only addresses may be broadcast to");
        return JS_FALSE;
    }

    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (bSpaceEnumPresent)
        {
            pRegs->Write32Broadcast(domainIndex,
                                    static_cast<RegHalTable::RegSpace>(spaceVal),
                                    static_cast<ModsGpuRegAddress>(enumVal),
                                    registerIndexes,
                                    value);
        }
        else
        {
            pRegs->Write32Broadcast(domainIndex,
                                    RegHalTable::UNICAST,
                                    static_cast<ModsGpuRegAddress>(enumVal),
                                    registerIndexes,
                                    value);
        }
    }
    else
    {
        pRegs->Write32Broadcast(0,
                                RegHalTable::UNICAST,
                                static_cast<ModsGpuRegAddress>(enumVal),
                                registerIndexes,
                                value);
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Write a broadcast register.
// 1-4 arguments, first can be any enumeration,
// next two arguments can be indeces,
// last argument is a value unless the first
// argument was a value enum.
JS_SMETHOD_LWSTOM(JsRegHal, Write64Broadcast, 5, "Write 64 bit broadcast address")
{
    STEST_HEADER(1, 5,
        "Usage: RegHal.Write64Broadcast((domainIndex), (spaceEnum), ModsGpuRegAddress, "
        "(idx/idx_array), (value))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex = 0, spaceVal = 0;
    UINT32 enumVal = 0;
    bool   bDomainIdxPresent = false, bSpaceEnumPresent = false;
    vector<UINT32> registerIndexes;
    UINT64 value = 0LLU;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               &spaceVal,
                               &enumVal,
                               &bDomainIdxPresent,
                               &bSpaceEnumPresent,
                               &registerIndexes,
                               nullptr,
                               &value));

    if (!(enumVal & MODS_REGISTER_ADDRESS))
    {
        pJavaScript->Throw(pContext,
                           RC::BAD_PARAMETER,
                           "Error oclwrred in Write64Broadcast: Only addresses may be broadcast to");
        return JS_FALSE;
    }

    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (bSpaceEnumPresent)
        {
            pRegs->Write64Broadcast(domainIndex,
                                    static_cast<RegHalTable::RegSpace>(spaceVal),
                                    static_cast<ModsGpuRegAddress>(enumVal),
                                    registerIndexes,
                                    value);
        }
        else
        {
            pRegs->Write64Broadcast(domainIndex,
                                    RegHalTable::UNICAST,
                                    static_cast<ModsGpuRegAddress>(enumVal),
                                    registerIndexes,
                                    value);
        }
    }
    else
    {
        pRegs->Write64Broadcast(0,
                                RegHalTable::UNICAST,
                                static_cast<ModsGpuRegAddress>(enumVal),
                                registerIndexes,
                                value);
    }

    return JS_TRUE;
}


//------------------------------------------------------------------------------
// Write a register.
// Same argument rules as Write32
JS_SMETHOD_LWSTOM(JsRegHal, Write32Sync, 4, "Write a register")
{
    STEST_HEADER(1, 4,
        "Usage: RegHal.Write32Sync((domainIndex), ModsGpuRegAddress/Field/Value, "
        "(idx/idx_array), (value))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex;
    UINT32 enumVal;
    bool   bDomainIdxPresent;
    vector<UINT32> registerIndexes;
    UINT32 value = 0;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               nullptr,
                               &enumVal,
                               &bDomainIdxPresent,
                               nullptr,
                               &registerIndexes,
                               &value,
                               nullptr));

    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            pRegs->Write32Sync(domainIndex,
                               static_cast<ModsGpuRegAddress>(enumVal),
                               registerIndexes,
                               value);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            pRegs->Write32Sync(domainIndex,
                               static_cast<ModsGpuRegField>(enumVal),
                               registerIndexes,
                               value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            pRegs->Write32Sync(domainIndex,
                               static_cast<ModsGpuRegValue>(enumVal),
                               registerIndexes);
        }
    }
    else
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            pRegs->Write32Sync(static_cast<ModsGpuRegAddress>(enumVal),
                               registerIndexes,
                               value);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            pRegs->Write32Sync(static_cast<ModsGpuRegField>(enumVal),
                               registerIndexes,
                               value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            pRegs->Write32Sync(static_cast<ModsGpuRegValue>(enumVal),
                               registerIndexes);
        }
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Write a 64 bit register.
// Same argument rules as Write64
JS_SMETHOD_LWSTOM(JsRegHal, Write64Sync, 4, "Write a 64 bit register")
{
    STEST_HEADER(1, 4,
        "Usage: RegHal.Write64Sync((domainIndex), ModsGpuRegAddress/Field/Value, "
        "(idx/idx_array), ([valueLo, valueHi]))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex = 0;
    UINT32 enumVal = 0;
    bool   bDomainIdxPresent = false;
    vector<UINT32> registerIndexes;
    UINT64 value = 0;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               nullptr,
                               &enumVal,
                               &bDomainIdxPresent,
                               nullptr,
                               &registerIndexes,
                               nullptr,
                               &value));

    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            pRegs->Write64Sync(domainIndex,
                               static_cast<ModsGpuRegAddress>(enumVal),
                               registerIndexes,
                               value);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            pRegs->Write64Sync(domainIndex,
                               static_cast<ModsGpuRegField>(enumVal),
                               registerIndexes,
                               value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            pRegs->Write64Sync(domainIndex,
                               static_cast<ModsGpuRegValue>(enumVal),
                               registerIndexes);
        }
    }
    else
    {
        if (enumVal & MODS_REGISTER_ADDRESS)
        {
            pRegs->Write64Sync(static_cast<ModsGpuRegAddress>(enumVal),
                               registerIndexes,
                               value);
        }
        else if (enumVal & MODS_REGISTER_FIELD)
        {
            pRegs->Write64Sync(static_cast<ModsGpuRegField>(enumVal),
                               registerIndexes,
                               value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            pRegs->Write64Sync(static_cast<ModsGpuRegValue>(enumVal),
                               registerIndexes);
        }
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Determine whether a priv-protected register is readable
JS_SMETHOD_LWSTOM(JsRegHal, HasReadAccess, 3,
                  "Check if a priv-protected register is readable")
{
    STEST_HEADER(1, 3,
                 "Usage: RegHal.HasReadAccess((domainIndex), ModsGpuRegAddress, (idx/idx_array)))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex;
    UINT32 enumVal;
    bool   bDomainIdxPresent;
    vector<UINT32> registerIndexes;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               nullptr,
                               &enumVal,
                               &bDomainIdxPresent,
                               nullptr,
                               &registerIndexes,
                               nullptr,
                               nullptr));

    if (!(enumVal & MODS_REGISTER_ADDRESS))
    {
        pJavaScript->Throw(pContext,
                           RC::BAD_PARAMETER,
                           "Error oclwrred in HasReadAccess: Only addresses may be checked");
        return JS_FALSE;
    }

    const RegHal* pRegs = pJsRegHal->GetRegHalObj();
    const bool hasReadAccess = pRegs->HasReadAccess(
            bDomainIdxPresent ? domainIndex : 0,
            static_cast<ModsGpuRegAddress>(enumVal),
            registerIndexes);

    const RC rc = pJavaScript->ToJsval(hasReadAccess, pReturlwalue);
    if (OK != rc)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in HasReadAccess: Unable to colwert result to jsval");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Determine whether a priv-protected register is readable & writable
JS_SMETHOD_LWSTOM(JsRegHal, HasRWAccess, 3,
                  "Check if a priv-protected register is readable & writable")
{
    STEST_HEADER(1, 3,
                 "Usage: RegHal.HasRWAccess((domainIndex), ModsGpuRegAddress, (idx/idx_array)))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex;
    UINT32 enumVal;
    bool   bDomainIdxPresent;
    vector<UINT32> registerIndexes;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               nullptr,
                               &enumVal,
                               &bDomainIdxPresent,
                               nullptr,
                               &registerIndexes,
                               nullptr,
                               nullptr));

    if (!(enumVal & MODS_REGISTER_ADDRESS))
    {
        pJavaScript->Throw(pContext,
                           RC::BAD_PARAMETER,
                           "Error oclwrred in HasRWAccess: Only addresses may be checked");
        return JS_FALSE;
    }

    const RegHal* pRegs = pJsRegHal->GetRegHalObj();
    const bool hasRWAccess = pRegs->HasRWAccess(
            bDomainIdxPresent ? domainIndex : 0,
            static_cast<ModsGpuRegAddress>(enumVal),
            registerIndexes);

    const RC rc = pJavaScript->ToJsval(hasRWAccess, pReturlwalue);
    if (OK != rc)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in HasRWAccess: Unable to colwert result to jsval");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Check whether a register field has the specified value.
// Return false if the register/field/value isn't supported on this GPU.
// 1-4 args, first is field or value, then 1 or 2 optional indeces,
// and a value if the first argument was a field.
JS_SMETHOD_LWSTOM(JsRegHal, Test32, 4, "Check whether a register field has the specified value.")
{
    STEST_HEADER(1, 4,
        "Usage: RegHal.Test32((domainIndex), ModsGpuRegField/Value, "
        "(idx/idx_array), value*)\n*if field enum given for first input");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex;
    UINT32 enumVal;
    bool   bDomainIdxPresent;
    vector<UINT32> registerIndexes;
    UINT32 value = 0;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               nullptr,
                               &enumVal,
                               &bDomainIdxPresent,
                               nullptr,
                               &registerIndexes,
                               &value,
                               nullptr));

    if (enumVal & MODS_REGISTER_ADDRESS)
    {
        pJavaScript->Throw(pContext,
                           RC::BAD_PARAMETER,
                           "Error oclwrred in Test32: Addresses may not be tested");
        return JS_FALSE;
    }

    bool bTestResult = false;
    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (enumVal & MODS_REGISTER_FIELD)
        {
            bTestResult = pRegs->Test32(domainIndex,
                                        static_cast<ModsGpuRegField>(enumVal),
                                        registerIndexes,
                                        value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            bTestResult = pRegs->Test32(domainIndex,
                                        static_cast<ModsGpuRegValue>(enumVal),
                                        registerIndexes);
        }
    }
    else
    {
        if (enumVal & MODS_REGISTER_FIELD)
        {
            bTestResult = pRegs->Test32(static_cast<ModsGpuRegField>(enumVal),
                                        registerIndexes,
                                        value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            bTestResult = pRegs->Test32(static_cast<ModsGpuRegValue>(enumVal),
                                        registerIndexes);
        }
    }

    const RC rc = pJavaScript->ToJsval(bTestResult, pReturlwalue);
    if (OK != rc)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in Test32: Unable to colwert result to jsval");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Check whether a register field has the specified value.
// Return false if the register/field/value isn't supported on this GPU.
// 1-4 args, first is field or value, then 1 or 2 optional indeces,
// and a value if the first argument was a field.
JS_SMETHOD_LWSTOM(JsRegHal, Test64, 4, "Check whether a 64 bit register field has the specified value.")
{
    STEST_HEADER(1, 4,
        "Usage: RegHal.Test64((domainIndex), ModsGpuRegField/Value, "
        "(idx/idx_array), UINT64*)\n*if field enum given for first input");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 domainIndex = 0;
    UINT32 enumVal = 0;
    bool   bDomainIdxPresent = false;
    vector<UINT32> registerIndexes;
    UINT64 value = 0;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               &domainIndex,
                               nullptr,
                               &enumVal,
                               &bDomainIdxPresent,
                               nullptr,
                               &registerIndexes,
                               nullptr,
                               &value));

    if (enumVal & MODS_REGISTER_ADDRESS)
    {
        pJavaScript->Throw(pContext,
                           RC::BAD_PARAMETER,
                           "Error oclwrred in Test64: Addresses may not be tested");
        return JS_FALSE;
    }

    bool bTestResult = false;
    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    if (bDomainIdxPresent)
    {
        if (enumVal & MODS_REGISTER_FIELD)
        {
            bTestResult = pRegs->Test64(domainIndex,
                                        static_cast<ModsGpuRegField>(enumVal),
                                        registerIndexes,
                                        value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            bTestResult = pRegs->Test64(domainIndex,
                                        static_cast<ModsGpuRegValue>(enumVal),
                                        registerIndexes);
        }
    }
    else
    {
        if (enumVal & MODS_REGISTER_FIELD)
        {
            bTestResult = pRegs->Test64(static_cast<ModsGpuRegField>(enumVal),
                                        registerIndexes,
                                        value);
        }
        else if (enumVal & MODS_REGISTER_VALUE)
        {
            bTestResult = pRegs->Test64(static_cast<ModsGpuRegValue>(enumVal),
                                        registerIndexes);
        }
    }

    const RC rc = pJavaScript->ToJsval(bTestResult, pReturlwalue);
    if (OK != rc)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in Test64: Unable to colwert result to jsval");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Get a field from a pre-read register value.
JS_SMETHOD_LWSTOM(JsRegHal, GetField, 2, "Get a field from a pre-read register value.")
{
    STEST_HEADER(2, 2, "Usage: RegHal.GetField(regValue, ModsGpuRegField)");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, UINT32, regValue);
    STEST_ARG(1, UINT32, enumField);
    if (enumField & MODS_REGISTER_FIELD)
    {
        pJavaScript->ToJsval(pJsRegHal->GetRegHalObj()->GetField(regValue, static_cast<ModsGpuRegField>(enumField)), pReturlwalue);
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in GetField: input enum was not of any valid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Get a 64 bit field from a pre-read 64 bit register value.
JS_SMETHOD_LWSTOM(JsRegHal, GetField64, 2, "Get a 64 bit field from a pre-read register value.")
{
    STEST_HEADER(2, 2, "Usage: RegHal.GetField64(regValueUINT64, ModsGpuRegField)");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, JSObject*, regValue);
    STEST_ARG(1, UINT32, enumField);
    JsUINT64 *pJsUINT64 =
            (JsUINT64 *) GetPrivateAndComplainIfNull(pContext,
                                                  regValue,
                                                  "JsUINT64",
                                                  "UINT64");
    if (pJsUINT64 == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    UINT64 value = pJsUINT64->GetValue();
    if (enumField & MODS_REGISTER_FIELD)
    {
        UINT64 retVal = pJsRegHal->GetRegHalObj()->GetField64(value, static_cast<ModsGpuRegField>(enumField));

        JsUINT64* pJsUINT64 = new JsUINT64(retVal);
        MASSERT(pJsUINT64);
        RC rc;
        if ((rc = pJsUINT64->CreateJSObject(pContext)) != OK ||
            (rc = pJavaScript->ToJsval(pJsUINT64->GetJSObject(), pReturlwalue)) != OK)
        {
            pJavaScript->Throw(pContext,
                               rc,
                               "Error oclwrred in GetField64: Unable to create return value");
            return JS_FALSE;
        }
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in GetField64: input enum was not of any valid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Check whether a field has the specified value in a pre-read register.
// Return false if the field or value isn't supported on this GPU.
// 2-3 args, regValue first, field or value enum second, then, then a
// value input third if a field enum was given.
JS_SMETHOD_LWSTOM(JsRegHal, TestField, 3, "Check whether a field has the specified value in a pre-read register.\n Return false if the field or value isn't supported on this GPU.")
{
    STEST_HEADER(2, 3, "Usage: RegHal.TestField(regValue, ModsGpuRegField/Value, fieldVal(only with Field 2nd input)");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, UINT32, regValue);
    STEST_ARG(1, UINT32, enumVal);
    if (enumVal & MODS_REGISTER_FIELD)
    {
        STEST_OPTIONAL_ARG(2, UINT32, fieldVal, 0);
        pJavaScript->ToJsval(pJsRegHal->GetRegHalObj()->TestField(regValue, static_cast<ModsGpuRegField>(enumVal), fieldVal), pReturlwalue);
    }
    else if (enumVal & MODS_REGISTER_VALUE)
    {
        pJavaScript->ToJsval(pJsRegHal->GetRegHalObj()->TestField(regValue, static_cast<ModsGpuRegValue>(enumVal)), pReturlwalue);
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in TestField: input enum was not of any valid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Check whether a 64 bit field has the specified 64 bit value in a pre-read 64 bit register.
// Return false if the field or value isn't supported on this GPU.
// 2-3 args, regValue first, field or value enum second, then, then a
// value input third if a field enum was given.
JS_SMETHOD_LWSTOM(JsRegHal, TestField64, 3, "Check whether a 64 bit field has the specified 64 bit value in a pre-read 64 bit register.\n Return false if the field or value isn't supported on this GPU.")
{
    STEST_HEADER(2, 3, "Usage: RegHal.TestField64(regValueUINT64, ModsGpuRegField/Value, fieldValUINT64(only with Field 2nd input)");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, JSObject*, regValue);
    STEST_ARG(1, UINT32, enumVal);
    JsUINT64 *pJsUINT64 =
            (JsUINT64 *) GetPrivateAndComplainIfNull(pContext,
                                                  regValue,
                                                  "JsUINT64",
                                                  "UINT64");
    if (pJsUINT64 == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    UINT64 regValue64 = pJsUINT64->GetValue();
    if (enumVal & MODS_REGISTER_FIELD)
    {
        STEST_OPTIONAL_ARG(2, JSObject*, fieldVal, nullptr);
        pJsUINT64 =
                (JsUINT64 *) GetPrivateAndComplainIfNull(pContext,
                                                      fieldVal,
                                                      "JsUINT64",
                                                      "UINT64");
        if (pJsUINT64 == nullptr)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        UINT64 fieldVal64 = pJsUINT64->GetValue();
        pJavaScript->ToJsval(pJsRegHal->GetRegHalObj()->TestField64(regValue64, static_cast<ModsGpuRegField>(enumVal), fieldVal64), pReturlwalue);
    }
    else if (enumVal & MODS_REGISTER_VALUE)
    {
        pJavaScript->ToJsval(pJsRegHal->GetRegHalObj()->TestField64(regValue64, static_cast<ModsGpuRegValue>(enumVal)), pReturlwalue);
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in TestField64: input enum was not of any valid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
//! Colwert a value enum to a field enum
JS_SMETHOD_LWSTOM(JsRegHal, ColwertToField, 1,
                  "Colwert a value enum to a field enum")
{
    STEST_HEADER(1, 1, "Usage: RegHal.ColwertToField(ModsGpuRegValue)");
    STEST_ARG(0, UINT32, enumValue);
    if (enumValue & MODS_REGISTER_VALUE)
    {
        const ModsGpuRegField regField =
            RegHal::ColwertToField(static_cast<ModsGpuRegValue>(enumValue));
        pJavaScript->ToJsval(static_cast<UINT32>(regField), pReturlwalue);
    }
    else
    {
        JS_ReportError(pContext,
                       "Error oclwrred in ColwertToField: invalid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Colwert a field or value enum to an address enum
JS_SMETHOD_LWSTOM(JsRegHal, ColwertToAddress, 1,
                  "Colwert a field or value enum to an address enum")
{
    STEST_HEADER(1, 1, "Usage: RegHal.ColwertToAddress(ModsGpuRegField/Value)");
    STEST_ARG(0, UINT32, enumVal);
    if (enumVal & MODS_REGISTER_FIELD)
    {
        const ModsGpuRegAddress regAddress =
            RegHal::ColwertToAddress(static_cast<ModsGpuRegField>(enumVal));
        pJavaScript->ToJsval(static_cast<UINT32>(regAddress), pReturlwalue);
    }
    else if (enumVal & MODS_REGISTER_VALUE)
    {
        const ModsGpuRegAddress regAddress =
            RegHal::ColwertToAddress(static_cast<ModsGpuRegValue>(enumVal));
        pJavaScript->ToJsval(static_cast<UINT32>(regAddress), pReturlwalue);
    }
    else
    {
        JS_ReportError(pContext,
                       "Error oclwrred in ColwertToField: invalid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Look up register address.
// 1-3 args, address enum for the first, and optional indeces
JS_SMETHOD_LWSTOM(JsRegHal, LookupAddress, 2, "Look up register address.")
{
    STEST_HEADER(1, 2, "Usage: RegHal.LookupAddress(ModsGpuRegAddress, (idx/idx_array))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 enumVal;
    vector<UINT32> registerIndexes;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               nullptr,
                               nullptr,
                               &enumVal,
                               nullptr,
                               nullptr,
                               &registerIndexes,
                               nullptr,
                               nullptr));

    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    UINT32 addr = 0;
    if (enumVal & MODS_REGISTER_ADDRESS)
    {
        addr = pRegs->LookupAddress(static_cast<ModsGpuRegAddress>(enumVal),
                                    registerIndexes);
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in LookupAddress: input enum was not of any valid enum type");
        return JS_FALSE;
    }

    const RC rc = pJavaScript->ToJsval(addr, pReturlwalue);
    if (OK != rc)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in IsSupported: Unable to colwert result to jsval");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Look up size of register array (dimension must be 1 or 2).
JS_SMETHOD_LWSTOM(JsRegHal, LookupArraySize, 2, "Look up size of register array (dimension must be 1 or 2).")
{
    STEST_HEADER(2, 2, "Usage: RegHal.LookupArraySize(ModsGpuRegAddress, dimension)");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, UINT32, enumAddr);
    STEST_ARG(1, UINT32, dimension);

    if(dimension != 1 && dimension != 2)
    {
        JS_ReportError(pContext, "Error oclwrred in LookupArraySize: input dimension was not 1 or 2");
        return JS_FALSE;
    }
    if (enumAddr & MODS_REGISTER_ADDRESS)
    {
        pJavaScript->ToJsval(pJsRegHal->GetRegHalObj()->LookupArraySize(static_cast<ModsGpuRegAddress>(enumAddr), dimension), pReturlwalue);
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in LookupArraySize: input enum was not of any valid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Loop up the shifted mask corresponding to a register field
JS_SMETHOD_LWSTOM(JsRegHal, LookupMask, 1, "Look up the shifted mask corresponding to a register field")
{
    STEST_HEADER(1, 1, "Usage: RegHal.LookupMask(ModsGpuRegField)");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, UINT32, enumField);
    if (enumField & MODS_REGISTER_FIELD)
    {
        pJavaScript->ToJsval(pJsRegHal->GetRegHalObj()->LookupMask(static_cast<ModsGpuRegField>(enumField)), pReturlwalue);
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in LookupMask: input enum was not of any valid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Look up register field value.
JS_SMETHOD_LWSTOM(JsRegHal, LookupValue, 1, "Look up register field value.")
{
    STEST_HEADER(1, 1, "Usage: RegHal.LookupValue(ModsGpuRegValue)");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, UINT32, enumValue);
    if (enumValue & MODS_REGISTER_VALUE)
    {
        pJavaScript->ToJsval(pJsRegHal->GetRegHalObj()->LookupValue(static_cast<ModsGpuRegValue>(enumValue)), pReturlwalue);
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in LookupValue: input enum was not of any valid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Check whether a register is supported on this GPU
// 1-3 arguments, first is any enum, followed by optional
// indeces if the given enum was an address.
JS_SMETHOD_LWSTOM(JsRegHal, IsSupported, 2, "Check whether a register is supported on this GPU")
{
    STEST_HEADER(1, 2, "Usage: RegHal.IsSupported(ModsGpuRegAddress/Field/Value, (idx*/idx_array*))\n*Only if addr given for first arg.");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");

    UINT32 enumVal;
    vector<UINT32> registerIndexes;
    CHECK_JS(ParseRegHalParams(pContext,
                               NumArguments,
                               pArguments,
                               nullptr,
                               nullptr,
                               &enumVal,
                               nullptr,
                               nullptr,
                               &registerIndexes,
                               nullptr,
                               nullptr));

    RegHal * pRegs = pJsRegHal->GetRegHalObj();
    bool bSupported = false;
    if (enumVal & MODS_REGISTER_ADDRESS)
    {
        bSupported = pRegs->IsSupported(static_cast<ModsGpuRegAddress>(enumVal),
                                        registerIndexes);
    }
    else if (enumVal & MODS_REGISTER_FIELD)
    {
        if (NumArguments != 1)
        {
            JS_ReportError(pContext, "Error oclwrred in IsSupported: Too may arguments for field enum type");
            return JS_FALSE;
        }
        bSupported = pRegs->IsSupported(static_cast<ModsGpuRegField>(enumVal));
    }
    else if (enumVal & MODS_REGISTER_VALUE)
    {
        if (NumArguments != 1)
        {
            JS_ReportError(pContext, "Error oclwrred in IsSupported: Too may arguments for field enum type");
            return JS_FALSE;
        }
        bSupported = pRegs->IsSupported(static_cast<ModsGpuRegValue>(enumVal));
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in IsSupported: Unrecognized enum type");
        return JS_FALSE;
    }

    const RC rc = pJavaScript->ToJsval(bSupported, pReturlwalue);
    if (OK != rc)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in IsSupported: Unable to colwert result to jsval");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// Set a field in a pre-read register value
//------------------------------------------------------------------------------
// Look up register field value.
JS_SMETHOD_LWSTOM(JsRegHal, SetField, 3, "Set a field in a pre-read register value.")
{
    STEST_HEADER(2, 3, "Usage: RegHal.SetField(pRegValue, ModsGpuRegField/Value, (fieldVal))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0,  JSObject*, pRegValue);
    STEST_ARG(1, UINT32, enumVal);
    if (enumVal & MODS_REGISTER_FIELD)
    {
        if (NumArguments == 3)
        {
            STEST_OPTIONAL_ARG(2, UINT32, fieldVal, 0);
            pJsRegHal->GetRegHalObj()->SetField((UINT32*)pRegValue, static_cast<ModsGpuRegField>(enumVal), fieldVal);
        }
        else
        {
            JS_ReportError(pContext, "Error oclwrred in SetField: Wrong number of arguments for field enum");
            return JS_FALSE;
        }
    }
    else if(enumVal & MODS_REGISTER_VALUE)
    {
        if (NumArguments == 2)
        {
            pJsRegHal->GetRegHalObj()->SetField((UINT32*)pRegValue, static_cast<ModsGpuRegValue>(enumVal));
        }
        else
        {
        JS_ReportError(pContext, "Error oclwrred in SetField: Wrong number of arguments for value enum");
        return JS_FALSE;
        }

    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in SetField: invalid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// Set a 64 bit field in a pre-read 64 bit register value
//------------------------------------------------------------------------------
// Look up register field value.
JS_SMETHOD_LWSTOM(JsRegHal, SetField64, 3, "Set a 64 bit field in a pre-read 64 bit register value.")
{
    STEST_HEADER(2, 3, "Usage: RegHal.SetField(pRegValueUINT64, ModsGpuRegField/Value, (fieldValUINT64))");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, JSObject*, regValue);
    STEST_ARG(1, UINT32, enumVal);
    JsUINT64 *pJsUINT64 =
            (JsUINT64 *) GetPrivateAndComplainIfNull(pContext,
                                                  regValue,
                                                  "JsUINT64",
                                                  "UINT64");
    if (pJsUINT64 == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    UINT64 regValue64 = pJsUINT64->GetValue();
    if (enumVal & MODS_REGISTER_FIELD)
    {
        if (NumArguments == 3)
        {
            STEST_OPTIONAL_ARG(2, JSObject*, fieldVal, nullptr);
            JsUINT64 *pJsFieldUINT64 =
                    (JsUINT64 *) GetPrivateAndComplainIfNull(pContext,
                                                          fieldVal,
                                                          "JsUINT64",
                                                          "UINT64");
            if (pJsFieldUINT64 == nullptr)
            {
                JS_ReportError(pContext, usage);
                return JS_FALSE;
            }
            UINT64 fieldVal64 = pJsFieldUINT64->GetValue();
            pJsRegHal->GetRegHalObj()->SetField64(&regValue64, static_cast<ModsGpuRegField>(enumVal), fieldVal64);
            pJsUINT64->SetValue(regValue64);
        }
        else
        {
            JS_ReportError(pContext, "Error oclwrred in SetField64: Wrong number of arguments for field enum");
            return JS_FALSE;
        }
    }
    else if(enumVal & MODS_REGISTER_VALUE)
    {
        if (NumArguments == 2)
        {
            pJsRegHal->GetRegHalObj()->SetField64(&regValue64, static_cast<ModsGpuRegValue>(enumVal));
            pJsUINT64->SetValue(regValue64);
        }
        else
        {
        JS_ReportError(pContext, "Error oclwrred in SetField64: Wrong number of arguments for value enum");
        return JS_FALSE;
        }

    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in SetField64: invalid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// Return a register value with one field set.  Replaces DRF_DEF & DRF_NUM.
JS_SMETHOD_LWSTOM(JsRegHal, FieldValue, 2, "Return a register value with one field set.  Replaces DRF_DEF & DRF_NUM.")
{
    STEST_HEADER(1, 2, "Usage: RegHal.FieldValue(ModsGpuRegField/Value, [fieldVal])");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, UINT32, enumVal);
    STEST_OPTIONAL_ARG(1, UINT32, fieldVal, 0);
    RegHal* pRegs = pJsRegHal->GetRegHalObj();

    if (enumVal & MODS_REGISTER_FIELD)
    {
        if (NumArguments == 2)
        {
            ModsGpuRegField regField = static_cast<ModsGpuRegField>(enumVal);
            UINT32 returlwalue = pRegs->SetField(regField, fieldVal);
            pJavaScript->ToJsval(returlwalue, pReturlwalue);
        }
        else
        {
            JS_ReportError(pContext, "Error oclwrred in SetField: Wrong number of arguments for field enum");
        }
    }
    else if (enumVal & MODS_REGISTER_VALUE)
    {
        if (NumArguments == 1)
        {
            ModsGpuRegValue regValue = static_cast<ModsGpuRegValue>(enumVal);
            UINT32 returlwalue = pRegs->SetField(regValue);
            pJavaScript->ToJsval(returlwalue, pReturlwalue);
        }
        else
        {
            JS_ReportError(pContext, "Error oclwrred in SetField: Wrong number of arguments for value enum");
            return JS_FALSE;
        }
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in FieldValue: invalid enum type");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// Return a 64 bit register value with one field set.  Replaces DRF_DEF & DRF_NUM.
JS_SMETHOD_LWSTOM(JsRegHal, FieldValue64, 2, "Return a 64 bit register value with one field set.  Replaces DRF_DEF & DRF_NUM.")
{
    STEST_HEADER(1, 2, "Usage: RegHal.FieldValue64(ModsGpuRegField/Value, [fieldValUINT64])");
    STEST_PRIVATE(JsRegHal, pJsRegHal, "JsRegHal");
    STEST_ARG(0, UINT32, enumVal);
    STEST_OPTIONAL_ARG(1, JSObject*, fieldVal, nullptr);
    RegHal* pRegs = pJsRegHal->GetRegHalObj();
    UINT64 retVal = 0;

    if (enumVal & MODS_REGISTER_FIELD)
    {
        if (NumArguments == 2)
        {
            JsUINT64* pJsFieldUINT64 = static_cast<JsUINT64*>(
                    GetPrivateAndComplainIfNull(pContext,
                                                fieldVal,
                                                "JsUINT64",
                                                "UINT64"));
            if (pJsFieldUINT64 == nullptr)
            {
                JS_ReportError(pContext, usage);
                return JS_FALSE;
            }
            retVal = pRegs->SetField64(static_cast<ModsGpuRegField>(enumVal),
                                       pJsFieldUINT64->GetValue());
        }
        else
        {
            JS_ReportError(pContext, "Error oclwrred in SetField64: Wrong number of arguments for field enum");
            return JS_FALSE;
        }
    }
    else if (enumVal & MODS_REGISTER_VALUE)
    {
        if (NumArguments == 1)
        {
            retVal = pRegs->SetField64(static_cast<ModsGpuRegValue>(enumVal));
        }
        else
        {
            JS_ReportError(pContext, "Error oclwrred in SetField64: Wrong number of arguments for value enum");
            return JS_FALSE;
        }
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred in FieldValue64: invalid enum type");
        return JS_FALSE;
    }

    JsUINT64* pJsUINT64 = new JsUINT64(retVal);
    MASSERT(pJsUINT64);
    RC rc;
    if ((rc = pJsUINT64->CreateJSObject(pContext)) != OK ||
        (rc = pJavaScript->ToJsval(pJsUINT64->GetJSObject(), pReturlwalue)) != OK)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in FieldValue64: Unable to create return value");
        return JS_FALSE;
    }
    return JS_TRUE;
}
