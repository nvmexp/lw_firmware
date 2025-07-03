/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_CALLBACK_H
#define INCLUDED_CALLBACK_H

#ifndef INCLUDED_JSCRIPT_H
#include "jscript.h"
#endif
#ifndef INCLUDED_SCRIPT_H
#include "script.h"
#endif
#ifndef INCLUDED_TEE_H
#include "tee.h"
#endif
#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif
#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

//-----------------------------------------------------------------------------
//! \brief Contains stack of callback arguments.
//!
//! Simple wrapper around JsArray to push/pop arguments to/from callbacks.
//!
class CallbackArguments
{
public:
    //! Clear all callback arguments.
    void Clear()
    {
        m_Array.clear();
    }

    //! Push a value onto the argument stack.
    template <typename T>
    void Push(T arg)
    {
        jsval target;
        if (OK != JavaScriptPtr()->ToJsval(arg, &target))
        {
            // Should never get here
            MASSERT(!"Using callbacks after JavaScript shutdown.");
            Printf(Tee::PriHigh, "Using callbacks after JavaScript shutdown.\n");
            target = JSVAL_ZERO;
        }

        m_Array.push_back(target);
    }

    //! Fetch an argument at the given index.
    template <typename T>
    RC At(size_t index, T *arg) const
    {
        if (index >= Size())
            return RC::BAD_PARAMETER;

        RC rc;
        CHECK_RC(JavaScriptPtr()->FromJsval(m_Array[index], arg));
        return OK;
    }

    //! Get the argument stack size.
    size_t Size() const
    {
        return m_Array.size();
    }

    //! Return a const reference to the inner STL object.
    const JsArray &Ref() const
    {
        return m_Array;
    }

private:
    JsArray m_Array;
};

//-----------------------------------------------------------------------------
//! \brief Represents a generic, ilwokable callback of unknown type.
//!
//! A callback interface that may be ilwoked like a function. All callback types
//! implement this interface, allowing their internal representation to be
//! abstracted. A callback may be a function, method or javascript function.
//!
class ICallback
{
public:
    virtual ~ICallback() {}

    //! Ilwoke the callback.
    virtual RC operator()(const CallbackArguments &args) = 0;
};

//-----------------------------------------------------------------------------
//! \brief Callback implementation for class member functions.
//!
//! Member function implemention of the form: RC Class::Method(UINT32, void *)
//!
template <typename T>
class MemberCallback : public ICallback
{
public:
    typedef RC (T::*Type)(const CallbackArguments &args);

    MemberCallback(Type mem, T *obj)
    : m_Member(mem), m_Object(obj)
    {
    }

    virtual ~MemberCallback() {}

    RC operator()(const CallbackArguments &args)
    {
        return (m_Object->*(m_Member))(args);
    }

private:
    Type m_Member;
    T *m_Object;
};

//-----------------------------------------------------------------------------
//! \brief Represents a callback with any number of ilwokable targets.
//!
//! Represents a collection of zero or more callbacks. A fire will sequentially
//! ilwoke each connected callback.
//!
class Callbacks
{
public:
    typedef RC (*FunctionType)(const CallbackArguments &args);
    typedef std::list<ICallback *> cb_list;
    typedef std::map<JSFunction *, ICallback *> jsf_map;
    typedef void *Handle;
    enum Flags { STOP_ON_ERROR, RUN_ON_ERROR };

    Callbacks();
    virtual ~Callbacks();

    //! Connect a class member function.
    template <typename T>
    Handle Connect(RC (T::*callback)(const CallbackArguments &args), T *object){
        return Connect(new MemberCallback<T>(callback, object));
    }

    //! Connect a function.
    Handle Connect(FunctionType callback);

    //! Connect a JavaScript function.
    void Connect(JSObject *pObj, JSFunction *pFunc);

    //! Connect a JavaScript function through a jsval.
    void Connect(JSObject *pObj, jsval func);

    //! Disconnect a connected callback.
    void Disconnect(Handle hConnection);

    //! Disconnect a connected JavaScript function.
    void Disconnect(JSFunction *pFunc);

    //! Disconnect a connected JavaScript function through a jsval.
    void Disconnect(jsval func);

    //! Disconnect all connected callbacks.
    void Disconnect();

    //! Fire all attached callbacks.
    RC Fire(Flags flags);

    //! Fire all attached callbacks.
    RC Fire(Flags flags, const CallbackArguments &args);

    //! Fire all attached callbacks, non-RC return.
    bool FireBool(Flags flags);

    //! Fire all attached callbacks, non-RC return.
    bool FireBool(Flags flags, const CallbackArguments &args);

    //! Return the number of connected callbacks.
    size_t GetCount();

private:
    //! Connect a generic callback object. This is private because Callbacks
    //! manages its own memory, and a non-heap allocated callback could
    //! potentially be connected.
    Handle Connect(ICallback *callback);

    cb_list m_Callbacks;
    jsf_map m_JsFuncs;
};

#endif // INCLUDED_CALLBACK_H

