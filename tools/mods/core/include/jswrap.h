/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// JavaScript wrapper functions to create objects, properties, and methods.

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_JSWRAP_H
#define INCLUDED_JSWRAP_H

#include <type_traits>

#include "script.h"
#include "jscript.h"

template<typename T, T fun>
JSBool JavaScript::JSWrapperFun(JSContext* pContext,
                                JSObject*  pObject,
                                uintN      numArgs,
                                jsval*     pArgs,
                                jsval*     pRetVal)
try
{
    MASSERT(pContext);
    MASSERT(!numArgs || pArgs);
    MASSERT(pRetVal);
    *pRetVal = JavaScriptPtr()->Ilwoke(fun, pContext, pObject, numArgs, pArgs);
    return JS_TRUE;
}
catch (const RC& rc)
{
    jsval value;
    JavaScriptPtr()->ToJsval(rc.Get(), &value);
    JS_SetPendingException(pContext, value);
    return JS_FALSE;
}
catch (const exception& e)
{
    jsval value;
    JavaScriptPtr()->ToJsval(e.what(), &value);
    JS_SetPendingException(pContext, value);
    return JS_FALSE;
}
catch (...)
{
    jsval value;
    JavaScriptPtr()->ToJsval("Exception", &value);
    JS_SetPendingException(pContext, value);
    return JS_FALSE;
}

template <int i, typename T>
remove_cv_t<remove_reference_t<T>>
JavaScript::ExtractArg(jsval* pArgs)
{
    remove_cv_t<remove_reference_t<T>> value;
    const RC rc = FromJsval(pArgs[i], &value);
    if (rc != RC::OK)
        throw rc;
    return value;
}

template<int reqNumArgs>
void JavaScript::CheckNumArgs(uintN numArgs)
{
    if (static_cast<int>(numArgs) < reqNumArgs)
    {
        Printf(Tee::PriNormal, "Not enough arguments passed from JS.  Need %d but only %d provided.\n",
               reqNumArgs, static_cast<int>(numArgs));
        throw RC(RC::SCRIPT_FAILED_TO_EXELWTE);
    }
}

template<typename Ret, typename... Args, int... indices>
jsval JavaScript::IlwokeInternal(Ret (*fun)(Args...), JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>)
{
    jsval value;
    const RC rc = ToJsval(fun(ExtractArg<indices, Args>(pArgs)...), &value);
    if (rc != RC::OK)
        throw rc;
    return value;
}

template<typename... Args, int... indices>
jsval JavaScript::IlwokeInternal(void (*fun)(Args...), JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>)
{
    fun(ExtractArg<indices, Args>(pArgs)...);
    return JSVAL_VOID;
}

template<typename T, typename Ret, typename... Args, int... indices>
jsval JavaScript::IlwokeInternal(Ret (T::*fun)(Args...), JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>)
{
    T* const pCppObj = static_cast<T*>(JS_GetPrivate(pContext, pObject));
    jsval value;
    const RC rc = ToJsval((pCppObj->*fun)(ExtractArg<indices, Args>(pArgs)...), &value);
    if (rc != RC::OK)
        throw rc;
    return value;
}

template<typename T, typename... Args, int... indices>
jsval JavaScript::IlwokeInternal(void (T::*fun)(Args...), JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>)
{
    T* const pCppObj = static_cast<T*>(JS_GetPrivate(pContext, pObject));
    (pCppObj->*fun)(ExtractArg<indices, Args>(pArgs)...);
    return JSVAL_VOID;
}

template<typename T, typename Ret, typename... Args, int... indices>
jsval JavaScript::IlwokeInternal(Ret (T::*fun)(Args...) const, JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>)
{
    T* const pCppObj = static_cast<T*>(JS_GetPrivate(pContext, pObject));
    jsval value;
    const RC rc = ToJsval((pCppObj->*fun)(ExtractArg<indices, Args>(pArgs)...), &value);
    if (rc != RC::OK)
        throw rc;
    return value;
}

template<typename T, typename... Args, int... indices>
jsval JavaScript::IlwokeInternal(void (T::*fun)(Args...) const, JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>)
{
    T* const pCppObj = static_cast<T*>(JS_GetPrivate(pContext, pObject));
    (pCppObj->*fun)(ExtractArg<indices, Args>(pArgs)...);
    return JSVAL_VOID;
}

template<typename Ret, typename... Args>
jsval JavaScript::Ilwoke(Ret (*fun)(Args...), JSContext* pContext, JSObject* pObject, uintN numArgs, jsval* pArgs)
{
    CheckNumArgs<sizeof...(Args)>(numArgs);
    return IlwokeInternal(fun, pContext, pObject, pArgs, typename IdxSeq<sizeof...(Args)>::Type());
}

template<typename T, typename Ret, typename... Args>
jsval JavaScript::Ilwoke(Ret (T::*fun)(Args...), JSContext* pContext, JSObject* pObject, uintN numArgs, jsval* pArgs)
{
    CheckNumArgs<sizeof...(Args)>(numArgs);
    return IlwokeInternal(fun, pContext, pObject, pArgs, typename IdxSeq<sizeof...(Args)>::Type());
}

template<typename T, typename Ret, typename... Args>
jsval JavaScript::Ilwoke(Ret (T::*fun)(Args...) const, JSContext* pContext, JSObject* pObject, uintN numArgs, jsval* pArgs)
{
    CheckNumArgs<sizeof...(Args)>(numArgs);
    return IlwokeInternal(fun, pContext, pObject, pArgs, typename IdxSeq<sizeof...(Args)>::Type());
}

template<typename T, T fun>
class JavaScript::JSWrapper: public SMethod
{
    public:
        JSWrapper(const char* name,
                  const char* helpMessage,
                  uintN       flags = 0)
        : SMethod(name,
                  JavaScript::JSWrapperFun<T, fun>,
                  CountArgs(fun),
                  helpMessage,
                  flags)
        {
        }

        JSWrapper(SObject&    object,
                  const char* name,
                  const char* helpMessage,
                  uintN       flags = 0)
        : SMethod(object,
                  name,
                  JavaScript::JSWrapperFun<T, fun>,
                  CountArgs(fun),
                  helpMessage,
                  flags)
        {
        }

        //! Selects correct method type based on return type of fun
        //!
        //! The method type is used to detect errors in JS when the C++ function
        //! fails.  The error checking will only be done for C++ functions which
        //! return RC.  C++ functions which return other types are not checked.
        MethodType GetMethodType() const override
        {
            using Ret = decltype(DetermineReturlwalue(fun));
            return is_same<Ret, RC>::value ? SMethod::Test : SMethod::Function;
        }

        template<typename Ret, typename... Args>
        constexpr static uintN CountArgs(Ret (*)(Args...)) { return sizeof...(Args); }

        template<typename TObj, typename Ret, typename... Args>
        constexpr static uintN CountArgs(Ret (TObj::*)(Args...)) { return sizeof...(Args); }

        template<typename TObj, typename Ret, typename... Args>
        constexpr static uintN CountArgs(Ret (TObj::*)(Args...) const) { return sizeof...(Args); }

        template<typename Ret, typename... Args>
        constexpr static Ret DetermineReturlwalue(Ret (*)(Args...));

        template<typename TObj, typename Ret, typename... Args>
        constexpr static Ret DetermineReturlwalue(Ret (TObj::*)(Args...));

        template<typename TObj, typename Ret, typename... Args>
        constexpr static Ret DetermineReturlwalue(Ret (TObj::*)(Args...) const);
};

#define JS_REGISTER_FUNCTION(funName, desc)                  \
    static JavaScript::JSWrapper<decltype(funName), funName> \
    _global_ ## funName                                      \
    (                                                        \
        #funName,                                            \
        desc                                                 \
    );

#define JS_REGISTER_METHOD(className, funName, desc)                                 \
    static JavaScript::JSWrapper<decltype(&className::funName), &className::funName> \
    _ ## className ## _ ## funName                                                   \
    (                                                                                \
        className##_Object,                                                          \
        #funName,                                                                    \
        desc                                                                         \
    );

#endif
