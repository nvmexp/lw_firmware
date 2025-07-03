/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008,2010,2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/callback.h"
#include "core/include/globals.h"
#include <algorithm>

// Wrapper for standard function callbacks
class FunctionCallback : public ICallback
{
public:
    FunctionCallback(Callbacks::FunctionType fun);
    virtual ~FunctionCallback() {}

    RC operator()(const CallbackArguments &args);

private:
    Callbacks::FunctionType m_Function;
};

// Wrapper for JavaScript function callbacks
class JScriptCallback : public ICallback
{
public:
    JScriptCallback(JSObject *obj, JSFunction *pFunc);
    virtual ~JScriptCallback() {}

    RC operator()(const CallbackArguments &args);

private:
    JSFunction *m_pJsFunction;
    JSObject   *m_pJsObj;
};

// FunctionCallback implementation
FunctionCallback::FunctionCallback(Callbacks::FunctionType fun)
: m_Function(fun)
{
}

RC FunctionCallback::operator()(const CallbackArguments &args)
{
    return m_Function(args);
}

// JScriptCallback implementation
JScriptCallback::JScriptCallback(JSObject *obj, JSFunction *pFunc)
: m_pJsFunction(pFunc), m_pJsObj(obj)
{
    MASSERT(obj != NULL);
    MASSERT(pFunc != NULL);
}

RC JScriptCallback::operator()(const CallbackArguments &args)
{
    RC rc = OK;
    jsval jsResult;
    JavaScriptPtr pJs;

    if (GetCriticalEvent())
    {
        // JavaScript::CallMethod has the side-effect of clearing the
        // "critical event" flag we use to force all RC to fail after
        // the user presses ctrl-C!
        return rc;
    }

    UINT32 ret;
    CHECK_RC(pJs->CallMethod(m_pJsObj, m_pJsFunction, args.Ref(), &jsResult));
    CHECK_RC(pJs->FromJsval(jsResult, &ret));

    return RC(ret);
}

// Callbacks implementation
Callbacks::Callbacks()
{
}

Callbacks::~Callbacks()
{
    Disconnect();
}

Callbacks::Handle Callbacks::Connect(ICallback *cb)
{
    m_Callbacks.push_back(cb);
    return (Handle)cb;
}

Callbacks::Handle Callbacks::Connect(FunctionType callback)
{
    return Connect(new FunctionCallback(callback));
}

void Callbacks::Connect(JSObject *pObj, JSFunction *pFunc)
{
    // Do not allow duplicate JSFunction connections
    if (m_JsFuncs.find(pFunc) != m_JsFuncs.end())
    {
        Printf(Tee::PriDebug, "Connected duplicate JS function (ignoring).\n");
        return;
    }

    JScriptCallback *callback = new JScriptCallback(pObj, pFunc);
    m_JsFuncs.insert(make_pair(pFunc, callback));

    Connect(callback);
}

void Callbacks::Connect(JSObject *pObj, jsval func)
{
    // Extract JSFunction from jsval and connect
    JSFunction *pFunc;
    if (JavaScriptPtr()->FromJsval(func, &pFunc) != OK)
    {
        Printf(Tee::PriHigh, "Connecting non-JSfunction object.\n");
        return;
    }

    Connect(pObj, pFunc);
}

void Callbacks::Disconnect(Handle hConnection)
{
    JsArray array;
    ICallback *cb = (ICallback *)hConnection;

    // make sure we own this connection
    cb_list::iterator i = find(m_Callbacks.begin(), m_Callbacks.end(), cb);
    if (i == m_Callbacks.end())
    {
        Printf(Tee::PriDebug, "Disconnecting invalid connection handle.\n");
        return;
    }

    // destroy the actual callback object, since we've verified we own it
    m_Callbacks.erase(i);
    delete cb;
    cb = NULL;
}

void Callbacks::Disconnect(JSFunction *pFunc)
{
    jsf_map::iterator i = m_JsFuncs.find(pFunc);
    if (i == m_JsFuncs.end())
    {
        Printf(Tee::PriHigh, "Disconnecting invalid JSFunction.\n");
        return;
    }

    ICallback *callback = i->second;

    m_JsFuncs.erase(i);
    Disconnect(callback);
}

void Callbacks::Disconnect(jsval func)
{
    // Extract JSFunction from jsval and disconnect
    JSFunction *pFunc;
    if (JavaScriptPtr()->FromJsval(func, &pFunc) != OK)
    {
        Printf(Tee::PriHigh, "Disconnecting non-JSfunction object.\n");
        return;
    }

    Disconnect(pFunc);
}

void Callbacks::Disconnect()
{
    for (cb_list::iterator i = m_Callbacks.begin(); i != m_Callbacks.end(); i++)
    {
        if (*i)
        {
            delete *i;
            *i = NULL;
        }
    }
    m_Callbacks.clear();
    m_JsFuncs.clear();
}

RC Callbacks::Fire(Flags flags)
{
    CallbackArguments args;
    return Fire(flags, args);
}

RC Callbacks::Fire(Flags flags, const CallbackArguments &args)
{
    StickyRC firstRc;
    for (cb_list::iterator i = m_Callbacks.begin(); i != m_Callbacks.end(); i++)
    {
        firstRc = (**i)(args);
        if (firstRc != OK && flags == STOP_ON_ERROR)
        {
            // We can either immediately fail, or ilwoke every callback
            break;
        }
    }

    return firstRc;
}

bool Callbacks::FireBool(Flags flags)
{
    CallbackArguments args;
    return FireBool(flags, args);
}

bool Callbacks::FireBool(Flags flags, const CallbackArguments &args)
{
    bool ret = true;
    for (cb_list::iterator i = m_Callbacks.begin(); i != m_Callbacks.end(); i++)
    {
        if ((**i)(args) == OK) // ie. error
        {
            ret = false;
            if (flags == STOP_ON_ERROR)
            {
                // We can either immediately fail, or ilwoke every callback
                break;
            }
        }
    }

    return ret;
}

size_t Callbacks::GetCount()
{
    return m_Callbacks.size();
}

