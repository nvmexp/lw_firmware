/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "js_c2c.h"
#include "core/include/jscript.h"
#include "core/include/script.h"

//-----------------------------------------------------------------------------
JsC2C::JsC2C(C2C* pC2C)
: m_pC2C(pC2C)
{
    MASSERT(m_pC2C);
}

//-----------------------------------------------------------------------------
static void C_JsC2C_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsC2C * pJsC2C;
    //! Delete the C++
    pJsC2C = (JsC2C *)JS_GetPrivate(cx, obj);
    delete pJsC2C;
};

//-----------------------------------------------------------------------------
static JSClass JsC2C_class =
{
    "C2C",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsC2C_finalize
};

//-----------------------------------------------------------------------------
static SObject JsC2C_Object
(
    "C2C",
    JsC2C_class,
    0,
    0,
    "C2C JS Object"
);

//-----------------------------------------------------------------------------
RC JsC2C::CreateJSObject(JSContext* cx, JSObject* obj)
{
    //! Only create one JSObject per Perf object
    if (m_pJsC2CObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsC2C.\n");
        return RC::OK;
    }

    m_pJsC2CObj = JS_DefineObject(cx,
                                  obj, // device object
                                  "C2C", // Property name
                                  &JsC2C_class,
                                  JsC2C_Object.GetJSObject(),
                                  JSPROP_READONLY);

    if (!m_pJsC2CObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsC2C instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsC2CObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsC2C.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsC2CObj, "Help", &C_Global_Help, 1, 0);

    return RC::OK;
}

RC JsC2C::DumpRegs() const
{
    return m_pC2C->DumpRegs();
}

RC JsC2C::GetErrorCounts(map<UINT32, C2C::ErrorCounts> * pErrorCounts) const
{
    return m_pC2C->GetErrorCounts(pErrorCounts);
}

UINT32 JsC2C::GetLinksPerPartition() const
{
    return m_pC2C->GetLinksPerPartition();
}

UINT32 JsC2C::GetMaxLinks() const
{
    return m_pC2C->GetMaxLinks();
}

UINT32 JsC2C::GetMaxPartitions() const
{
    return m_pC2C->GetMaxPartitions();
}

string JsC2C::GetRemoteDeviceString(UINT32 partitionId) const
{
    return m_pC2C->GetRemoteDeviceString(partitionId);
}

bool   JsC2C::IsPartitionActive(UINT32 partitionId) const
{
    return m_pC2C->IsPartitionActive(partitionId);
}

void JsC2C::PrintTopology(UINT32 pri)
{
    m_pC2C->PrintTopology(static_cast<Tee::Priority>(pri));
}

CLASS_PROP_READONLY(JsC2C,  LinksPerPartition, UINT32, "Get the number of links per C2C partition");         //$
CLASS_PROP_READONLY(JsC2C,  MaxLinks,          UINT32, "Get the maximum number of C2C links");          //$
CLASS_PROP_READONLY(JsC2C,  MaxPartitions,     UINT32, "Get the maximum number of C2C partitions");          //$

JS_SMETHOD_BIND_ONE_ARG(JsC2C, GetRemoteDeviceString, partitionId, UINT32, "Get the remote device string for the specified partition"); //$
JS_SMETHOD_BIND_ONE_ARG(JsC2C, IsPartitionActive,     partitionId, UINT32, "Check whether a partition is active");                      //$
JS_SMETHOD_VOID_BIND_ONE_ARG(JsC2C, PrintTopology,    priority,    UINT32, "Print the C2C topology for the device");                    //$
JS_STEST_BIND_NO_ARGS(JsC2C, DumpRegs, "Dump the C2C registers");

//-----------------------------------------------------------------------------
JS_CLASS(Dummy);
JS_STEST_LWSTOM(JsC2C,
                GetErrorCounts,
                1,
                "Read the C2C error counters")
{
    STEST_HEADER(1, 1, "Usage:\n"
                       "var vals = new Object\n"
                       "TestDevice.C2C.GetErrorCounts(vals)\n");
    STEST_ARG(0, JSObject*, pJsErrors);
    STEST_PRIVATE(JsC2C, pJsC2C, "C2C");
    RC rc;

    map<UINT32, C2C::ErrorCounts> errCounts;
    C_CHECK_RC(pJsC2C->GetErrorCounts(&errCounts));

    for (auto const & lwrLinkCounts : errCounts)
    {
        JSObject *pErrorCountObj = JS_NewObject(pContext, &DummyClass, 0, 0);

        // Objects created with JS_NewObject do not have the root property by default
        // which means they can be garbage collected.  Flag the error object as a root
        // property until it is added to the return object (at which point it inherits
        // the root property from the return object) to prevent it from being garbage
        // collected
        if (JS_TRUE != JS_AddRoot(pContext, &pErrorCountObj))
        {
            Printf(Tee::PriError, "Could not AddRoot on args on C2C error flag object");
            RETURN_RC(RC::COULD_NOT_CREATE_JS_OBJECT);
        }
        DEFER { JS_RemoveRoot(pContext, &pErrorCountObj); };

        bool bAnyErrorsValid = false;
        for (UINT32 nErrIdx = 0; nErrIdx < C2C::ErrorCounts::NUM_ERR_COUNTERS; nErrIdx++)
        {
            C2C::ErrorCounts::ErrId errId =
                static_cast<C2C::ErrorCounts::ErrId>(nErrIdx);
            if (lwrLinkCounts.second.IsValid(errId))
            {
                C_CHECK_RC(pJavaScript->SetElement(pErrorCountObj,
                                                   errId,
                                                   lwrLinkCounts.second.GetCount(errId)));
                bAnyErrorsValid = true;
            }
        }

        if (bAnyErrorsValid)
        {
            C_CHECK_RC(pJavaScript->SetElement(pJsErrors,
                                               lwrLinkCounts.first,
                                               pErrorCountObj));
        }
    }

    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
// LwLinkDev constants
//-----------------------------------------------------------------------------
JS_CLASS(C2CConst);
static SObject C2CConst_Object
(
    "C2CConst",
    C2CConstClass,
    0,
    0,
    "C2CConst JS Object"
);

PROP_CONST(C2CConst, DR_TEST,           C2C::DR_TEST);
PROP_CONST(C2CConst, DR_TEST_ERROR,     C2C::DR_TEST_ERROR);
PROP_CONST(C2CConst, DR_ERROR,          C2C::DR_ERROR);
PROP_CONST(C2CConst, DR_POST_TRAINING,  C2C::DR_POST_TRAINING);

PROP_CONST(C2CConst, C2C_RX_CRC_ID,             C2C::ErrorCounts::RX_CRC_ID);
PROP_CONST(C2CConst, C2C_TX_REPLAY_ID,          C2C::ErrorCounts::TX_REPLAY_ID);
PROP_CONST(C2CConst, C2C_TX_B2B_FIR_REPLAY_ID,  C2C::ErrorCounts::TX_REPLAY_B2B_FID_ID);
PROP_CONST(C2CConst, C2C_NUM_ERR_COUNTERS,      C2C::ErrorCounts::NUM_ERR_COUNTERS);

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(C2CConst,
                  ErrorIndexToString,
                  1,
                  "Get the string associated with an error index")
{
    STEST_HEADER(1, 1, "Usage: C2CConst.ErrorIndexToString(ErrIndex)\n");
    STEST_ARG(0, UINT32, nErrIdx);

    C2C::ErrorCounts::ErrId errIdx = static_cast<C2C::ErrorCounts::ErrId>(nErrIdx);
    if (pJavaScript->ToJsval(C2C::ErrorCounts::GetName(errIdx), pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(C2CConst,
                  ErrorIndexToJsonTag,
                  1,
                  "Get the JSON tag associated with an error index")
{
    STEST_HEADER(1, 1, "Usage: C2CConst.ErrorIndexToJsonTag(ErrIndex)\n");
    STEST_ARG(0, UINT32, nErrIdx);

    C2C::ErrorCounts::ErrId errIdx = static_cast<C2C::ErrorCounts::ErrId>(nErrIdx);
    if (pJavaScript->ToJsval(C2C::ErrorCounts::GetJsonTag(errIdx), pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}
