/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// JavaScript language interface.

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_JSCRIPT_H
#define INCLUDED_JSCRIPT_H

#include "jscallbacks.h"
#include "types.h"
#include "rc.h"
#include "singlton.h"
#include "massert.h"
#include "jsapi.h"
#include "jsstr.h"
#include "jsdbgapi.h"
#include <string>
#include <vector>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/core/noncopyable.hpp>
#include <boost/type_traits/function_traits.hpp>

#include "jsprvtd.h"

#if defined(INCLUDE_BYTECODE)
#include "jsobjectid.h"
#endif

class SObject;
class SProperty;

typedef vector<jsval> JsArray;

// The error reporting mechanism used by JS
void ErrorReporter
(
    JSContext     * /* pContext */,
    const char    *    Message,
    JSErrorReport *    pReport
);

// PIMPL idiom to avoid include <map>
class BeforeClosureMap;

//! Colwerts a JSString to an ASCII string. Wide characters will be either
//! truncated or colwerted to UTF8 depending on whether the JavaScript
//! engine was compiled the JS_C_STRINGS_ARE_UTF8 macro defined or not. The
//! engine is compiled with this macro for the exelwtion of encrypted
//! byte-code.
string DeflateJSString(JSContext *cx, JSString *inStr);

#if defined(INCLUDE_BYTECODE)
// PIMPL idiom to avoid include <map>
class ObjectIdsMapper;
#endif

class JavaScript
{
public:

    JavaScript(size_t RuntimeMaxBytes, size_t ContextStackSize);
    ~JavaScript();

    RC GetInitRC() const;

    // Colwert to a jsval.
    // JavaScript has only one Number format, double-precision float.
    RC ToJsval(bool,          jsval *);

    template <class Num>
    enable_if_t<
        is_colwertible<Num, double>::value && !is_same<double, remove_cv_t<Num>>::value
      , RC
      >
    ToJsval(Num x, jsval *pjv)
    {
        return ToJsval(static_cast<double>(x), pjv);
    }

    RC ToJsval(double,        jsval *);
    RC ToJsval(const string&, jsval *);
    RC ToJsval(const char*,   jsval *);
    RC ToJsval(JsArray *,     jsval *);
    RC ToJsval(JSObject *,    jsval *);
    template<typename T>
    RC ToJsval(const vector<T>&, jsval*);

    // Colwert from a jsval.
    RC FromJsval(jsval, bool*);
    RC FromJsval(jsval, INT08* );
    RC FromJsval(jsval, UINT08* );
    RC FromJsval(jsval, INT16* );
    RC FromJsval(jsval, UINT16* );
    RC FromJsval(jsval, INT32*);
    RC FromJsval(jsval, UINT32*);
    RC FromJsval(jsval, INT64* );
    RC FromJsval(jsval, UINT64*);
    RC FromJsval(jsval, FLOAT32*);
    RC FromJsval(jsval, double*);
    RC FromJsval(jsval, string*);
    RC FromJsval(jsval, JSString**);
    RC FromJsval(jsval, JsArray*);
    RC FromJsval(jsval, JSObject**);
    RC FromJsval(jsval, JSFunction**);
    template<typename T>
    RC FromJsval(jsval, vector<T> *);

    //! \brief Unpack a sequence.
    //!
    //! Format is a string containing one character per item.
    //!    ? bool
    //!    b INT08
    //!    B UINT08
    //!    h INT16
    //!    H UINT16
    //!    i INT32
    //!    I UINT32
    //!    l INT64
    //!    L UINT64
    //!    f FLOAT32
    //!    d FLOAT64
    //!    s string (C++ string class)
    //!    a JsArray
    //!    o JSObject
    //!
    //! Arguments after Format are pointers to bool, UINT16, etc to match
    //! the Format string exactly.
    //!
    //! Returns the number of items unpacked, which will be zero if the jsval
    //! does not contain an array, or if the first item did not colwert.
    //!
    //! WARNING -- for 's' you must pass a string*, not a char*.
    //! The compiler can't catch this for you, it will cause a crash at runtime.
    //!
    size_t UnpackArgs   (const jsval * pArgs, size_t nArgs, const char * Format, ...);
    size_t UnpackJsArray(const JsArray & src,               const char * Format, ...);
    size_t UnpackJsval  (jsval src,                         const char * Format, ...);
    size_t UnpackVA     (const jsval * pJsvals, size_t nJsvals,
                         const char * Format, va_list *args);

    //! \brief Unpack a JS Object with named properties.
    //!
    //! Format is a string containing one character per item, see UnpackArgs.
    //!
    //! OPTIONAL FIELDS: can be preceeded by a ~ (tilde) in the \a Format
    //! argument. If an optional field is missing or cannot colwert, it is
    //! ignored and the destination C++ pointer is not assigned.
    //!
    //! Arguments after Format are alternating pairs of:
    //!   const char * fieldName, pointer to (type matching Format symbol).
    //!
    //! WARNING -- for 's' you must pass a string*, not a char*.
    //! The compiler can't catch this for you, it will cause a crash at runtime.
    //!
    RC UnpackFields(JSObject * pObj, const char * Format, ...);

    //! Just like UnpackFields, but all fields are optional and ~ is ignored.
    RC UnpackFieldsOptional(JSObject * pObj, const char * Format, ...);

    //! \brief Pack a JsArray with mixed values.
    //!
    //! Format is a string containing one character per item.
    //!    ? bool
    //!    b INT08
    //!    B UINT08
    //!    h INT16
    //!    H UINT16
    //!    i INT32
    //!    I UINT32
    //!    l INT64
    //!    L UINT64
    //!    f FLOAT32
    //!    d FLOAT64
    //!    s string (const char *)  WARNING!
    //!    a JsArray
    //!    o JsObject
    //!
    //! Note the 's' format takes a const char*!  The Unpack functions take a
    //! string* here, but Pack works like Printf.  Don't get this wrong, or
    //! it will cause a crash at runtime.
    //!
    //! The JsArray is appended to, so you may call this multiple times to
    //! build a long array.
    //!
    RC PackJsArray(JsArray * pAry, const char * Format, ...);

    //! \brief Pack a JS Object with named properties.
    //!
    //! Format is a string containing one character per item, see PackJsArray.
    //!
    //! Arguments after Format are alternating pairs of:
    //! const char * fieldName, value of type matching the Format char.
    //!
    //! Only the named fields (i.e. properties) of the parent object are set.
    //! Properties not named are unaffected.  If a property does not exist
    //! it will be created.
    //!
    //! Note the 's' format takes a const char*!  The Unpack functions take a
    //! string* here, but Pack works like Printf.  Don't get this wrong, or
    //! it will cause a crash at runtime.
    //!
    RC PackFields(JSObject * pObj, const char * Format, ...);

    //! Property/Element Get/Set, non-colwerting.
    RC GetPropertyJsval(const SObject &, const SProperty &, jsval *);
    RC SetPropertyJsval(const SObject &, const SProperty &, jsval);
    RC GetPropertyJsval(JSObject *,      const char *,      jsval *);
    RC SetPropertyJsval(JSObject *,      const char *,      jsval);
    RC GetPropertyJsval(JSObject *,      JSString *,        jsval *);
    RC SetPropertyJsval(JSObject *,      JSString *,        jsval);
    RC SetElementJsval (JSObject *,      INT32 idx,         jsval);
    RC GetElementJsval (JSObject *,      INT32 idx,         jsval *);

    //! Property/Element Get/Set, with type colwersion.
    template <typename T>
    RC GetProperty(const SObject &, const SProperty &, T *);
    template <typename T>
    RC SetProperty(const SObject &, const SProperty &, T);

    // Allows to specialize a template only for const char * and JSString *.
    // The idea is that if the first parameter of enable_if is resolved to true,
    // then enable_if<true, void>::type will be equal to void. If the first
    // parameter is false, then enable_if<false, void>::type is not defined at
    // all. Therefore EnableIfString<StringType>::type is defined only for const
    // char * and JSString *.
    template <typename StringType>
    using EnableIfString = enable_if_t<
        is_same<remove_cv_t<StringType>, const char *>::value ||
        is_same<remove_cv_t<StringType>, JSString *>::value
      , void
      >;

    template <typename T, typename StringType>
    RC GetProperty(JSObject *obj, StringType prop, T *pVal,
                   EnableIfString<StringType> *dummy = 0)
    {
        MASSERT(pVal);
        jsval tmp;
        RC rc = GetPropertyJsval(obj, prop, &tmp);
        if (OK == rc)
        {
            rc = FromJsval(tmp, pVal);
        }
        return rc;
    }

    template <typename T, typename StringType>
    RC SetProperty(JSObject *obj, StringType prop, T val,
                   EnableIfString<StringType> *dummy = 0)
    {
        jsval tmp;
        RC rc = ToJsval(val, &tmp);
        if (OK == rc)
        {
            rc = SetPropertyJsval(obj, prop, tmp);
        }
        return rc;
    }

    template <typename T>
    RC SetElement(JSObject *,       INT32 idx,         T);
    template <typename T>
    RC GetElement(JSObject *,       INT32 idx,         T *);
    RC SetElement (JSObject *,      INT32 idx,         jsval);

    // Create an empty JSObject property.
    RC DefineProperty(JSObject * pObject, const char * PropertyName,
          JSObject ** ppNewObject);

    // Get all the object's properties.
    RC GetProperties(JSObject * pObject, vector<string> * pProperties);
    RC GetProperties(JSObject * pObject, vector<JSString *> * pProperties);

    // Run a script.
    // BUG: unless pReturlwalue is a bool or small integer, GC will kill it.
    RC RunScript(string Script, jsval * pReturlwalue = 0);

    // Import a file (preprocess, compile, and execute).
    RC Import(string FileName, JSObject * pReturlwalues = 0, bool mustExist = true);

    // Eval a string (preprocess, compile, and execute).
    RC Eval(string Str, JSObject * pReturlwalues = 0);

    //! In a bound (restricted) exelwtable, run the compressed, compiled-in
    //! javascript file.  Decrypts differently than non-bound JS files.
    RC RunBoundScript();

    // Call a JavaScript method.
    RC CallMethod(JSObject * pObject, string Method,
          const JsArray & Arguments = JsArray(),
          jsval * pReturlwalue = 0);
    RC CallMethod(JSObject * pObject, JSFunction * Method,
          const JsArray & Arguments = JsArray(),
          jsval * pReturlwalue = 0);
    RC CallMethod(JSFunction * Method, const JsArray & Arguments = JsArray(),
                  jsval * pReturlwalue = 0);
    RC CallMethod(JSContext *pContext, JSFunction * Method,
                  const JsArray & Arguments = JsArray(),
                  jsval * pReturlwalue = 0);

    // Print the JavaScript callstack.
    void PrintStackTrace(INT32 pri);

    // Get the global object.
    JSObject *  GetGlobalObject() const;

    // Get the context for the current thread (creates one if one has not
    // been created)
    RC GetContext(JSContext **ppContext);

    // Create a JS context
    RC CreateContext(const size_t StackSize, JSContext **ppNewContext);

    // Get the runtime
    JSRuntime * Runtime() const;

    //! On JS1.7 and above
    void MaybeCollectGarbage(JSContext * pContext);

    //! Force the JS garbage collector to run.
    void CollectGarbage(JSContext * pContext);

    class GenericDeleterBase
    {
        public:
            GenericDeleterBase() = default;
            virtual ~GenericDeleterBase() { }

            GenericDeleterBase(const GenericDeleterBase&)            = delete;
            GenericDeleterBase& operator=(const GenericDeleterBase&) = delete;
    };

    template<typename T>
    class GenericDeleter: public GenericDeleterBase
    {
        public:
            explicit GenericDeleter(T* pObject)
                : m_pObject(pObject)
            {
            }
            virtual ~GenericDeleter()
            {
                delete m_pObject;
            }

        private:
            T* m_pObject;
    };

    //! Defer deleting object until GC ends.
    //!
    //! When finalize functions are ilwoked during GC, they should call
    //! this function instead of the delete operator to destroy any
    //! heap-allocated objects.  This function postpones deleting the
    //! object until the end of GC.
    template<typename T>
    void DeferredDelete(T* pObject)
    {
        DeferredDelete(make_unique<GenericDeleter<T>>(pObject));
    }

    void DeferredDelete(unique_ptr<GenericDeleterBase> pObject);

    void ClearZombies();

    //! Hooks Ctrl-C handler. To be ilwoked from Platform::Initialize().
    void HookCtrlC();

    //! Unhooks Ctrl-C handler letting the user to hard-kill MODS.
    void UnhookCtrlC();

    //! Copy the content of JsArray to a UINT32/FLOAT32 vector.
    template<typename T>
    RC FromJsArray
    (
        const JsArray &Array
       ,vector<T> *pVector
       ,Tee::Priority msgPri = Tee::PriNormal
    );

    //! Copy the content of UINT32/FLOAT32 vector to JsArray.
    template<typename T>
    RC ToJsArray
    (
        const vector<T> &Vector
       ,JsArray *pArray
       ,Tee::Priority msgPri = Tee::PriNormal
    );
    
#if defined(INCLUDE_BYTECODE)
    JSObject *& operator[](JSObjectId id);
    JSObject* GetObjectById(JSObjectId id);
#endif

    //#########################################################################
    // Interface for registering C++ functions in JS
    //#########################################################################

    template<typename T, T fun>
    class JSWrapper;

    //! The wrapper for C++ functions, which is exported to JS
    template<typename T, T fun>
    static JSBool JSWrapperFun(JSContext* pContext,
                               JSObject*  pObject,
                               uintN      numArgs,
                               jsval*     pArgs,
                               jsval*     pRetVal);

    //! Reads a particular argument from JS arguments array
    template <int i, typename T>
    remove_cv_t<remove_reference_t<T>>
    ExtractArg(jsval* pArgs);

    //! Checks if the number of arguments provided meets the required minimum
    template<int reqNumArgs>
    void CheckNumArgs(uintN numArgs);

    //! "Stores" a sequence of integers as template parameters
    template<int...>
    struct Seq
    {
    };

    //! Generates a sequence of integers from 0 to n-1
    template<int n, int... indices>
    struct IdxSeq: IdxSeq<n-1, n-1, indices...>
    {
    };

    template<int... indices>
    struct IdxSeq<0, indices...>
    {
        typedef Seq<indices...> Type;
    };

    void Throw(JSContext *cx, RC rc, string msg);
    string GetStackTrace();

    RC GetRcException(JSContext *pContext);
    void ReportAndClearException(JSContext *pContext);
private:
    template<typename Ret, typename... Args, int... indices>
    jsval IlwokeInternal(Ret (*fun)(Args...), JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>); //$

    template<typename... Args, int... indices>
    jsval IlwokeInternal(void (*fun)(Args...), JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>); //$

    template<typename T, typename Ret, typename... Args, int... indices>
    jsval IlwokeInternal(Ret (T::*fun)(Args...), JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>); //$

    template<typename T, typename... Args, int... indices>
    jsval IlwokeInternal(void (T::*fun)(Args...), JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>); //$

    template<typename T, typename Ret, typename... Args, int... indices>
    jsval IlwokeInternal(Ret (T::*fun)(Args...) const, JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>); //$

    template<typename T, typename... Args, int... indices>
    jsval IlwokeInternal(void (T::*fun)(Args...) const, JSContext* pContext, JSObject* pObject, jsval* pArgs, Seq<indices...>); //$

public:
    //! Ilwokes a regular native function, passing args from JS
    template<typename Ret, typename... Args>
    jsval Ilwoke(Ret (*fun)(Args...), JSContext* pContext, JSObject* pObject, uintN numArgs, jsval* pArgs); //$

    //! Ilwokes a non-const method, passing args from JS
    template<typename T, typename Ret, typename... Args>
    jsval Ilwoke(Ret (T::*fun)(Args...), JSContext* pContext, JSObject* pObject, uintN numArgs, jsval* pArgs); //$

    //! Ilwokes a const method, passing args from JS
    template<typename T, typename Ret, typename... Args>
    jsval Ilwoke(Ret (T::*fun)(Args...) const, JSContext* pContext, JSObject* pObject, uintN numArgs, jsval* pArgs); //$

private:
    void * CheckJSCalls(JSContext *cx, JSStackFrame *fp, JSBool before, JSBool *ok);

    class JSCallHook
      : public JSHookBase<JSCallHook, JS_SetCallHookType, JS_SetCallHook>
    {
        friend class JSHookAccess;

        typedef void * (JavaScript::*CallHookPtr)(
            JSContext *cx, JSStackFrame *fp, JSBool before, JSBool *ok
        );
    public:
        JSCallHook() = default;
        // Because of unique_ptr<BeforeClosureMap> we have to have the
        // constructor be defined where BeforeClosureMap is complete, not here
        // in the header.
        JSCallHook(JavaScript *js, CallHookPtr jsMethod);

    private:
        static constexpr bool HasInit = true;
        static constexpr bool HasShutDown = true;
        typedef unique_ptr<BeforeClosureMap> BeforeClosureMapPtr;

        void * Hook(JSContext *cx, JSStackFrame *fp, JSBool before, JSBool *ok);
        void Init(JSRuntime *rt, JavaScript *js, CallHookPtr jsMethod);
        void ShutDown();

        BeforeClosureMapPtr  m_BeforeClosure;
        JavaScript          *m_Js = nullptr;
        CallHookPtr          m_JsMethod = nullptr;
    };

    const size_t m_RuntimeMaxBytes;
    const size_t m_ContextStackSize;
    JSRuntime  * m_pRuntime;
    JSObject   * m_pGlobalObject;
    bool         m_IsCtrlCHooked;
    void      (* m_OriginalCtrlCHandler)(int);
    INT32        m_ImportDepth;
    string       m_ImportPath;
    RC           m_InitRC;
    bool         m_bDebuggerStarted;
    UINT32       m_RunScriptJsdIndex;
    bool         m_bStrict;
    JSCallHook   m_CheckJSCallsHook;

    // Objects which were meant to be destroyed by finalize function during GC,
    // they land in this container and are deleted after GC is finished.
    vector<unique_ptr<GenericDeleterBase>> m_ZombieObjects;
    void* m_ZombieMutex = nullptr;

#if defined(INCLUDE_BYTECODE)
    unique_ptr<ObjectIdsMapper> m_objectIdMapImpl;
#endif

    // Do not allow copying.
    JavaScript(const JavaScript &);
    JavaScript & operator= (const JavaScript &);

    string GetDebugFileName(string FileName);
    RC WriteDebugFile(string FileName, const vector<char> &FileBuffer);
    RC WriteDebugFile(string FileName, const string &FileData);
    RC UnpackFieldsVA
    (
        bool        allOptional
      , JSObject   *pObj
      , const char *Format
      , va_list    *pRestOfArgs
    );
};

SINGLETON( JavaScript );

//! \brief RAII Class to enumerate the JS properties of an object.  Ensures
//!        that the properties get correctly destroyed when passed out of
//!        context
//!
class JSPropertiesEnumerator
{
public:
    JSPropertiesEnumerator(JSContext * pContext, JSObject * pObject) :
        m_pContext(pContext)
    {
        m_pProperties = JS_Enumerate(pContext, pObject);
    }
    ~JSPropertiesEnumerator()
    {
        if (m_pProperties)
            JS_DestroyIdArray(m_pContext, m_pProperties);
    }

    JSIdArray * GetProperties() { return m_pProperties; }

private:
    JSContext * m_pContext;
    JSIdArray * m_pProperties;
};

template <typename T>
RC JavaScript::GetProperty(const SObject &obj, const SProperty &prop, T* pVal)
{
    MASSERT(pVal);
    jsval tmp;
    RC rc = GetPropertyJsval(obj, prop, &tmp);
    if (OK == rc)
        rc = FromJsval(tmp, pVal);
    return rc;
}

template <typename T>
RC JavaScript::SetProperty(const SObject &obj, const SProperty &prop, T val)
{
    jsval tmp;
    RC rc = ToJsval(val, &tmp);
    if (OK == rc)
        rc = SetPropertyJsval(obj, prop, tmp);
    return rc;
}

template <typename T>
RC JavaScript::SetElement(JSObject * obj, INT32 Index, T val)
{
    jsval tmp;
    RC rc = ToJsval(val, &tmp);
    if (OK == rc)
        rc = SetElementJsval(obj, Index, tmp);
    return rc;
}

template <typename T>
RC JavaScript::GetElement(JSObject * obj, INT32 Index, T* pVal)
{
    jsval tmp;
    RC rc = GetElementJsval(obj, Index, &tmp);
    if (OK == rc)
        rc = FromJsval(tmp, pVal);
    return rc;
}

namespace std
{
    // specialize less for JSString, so JSString could be used in map, set, etc.
    template <>
    struct less<JSString *>
    {
        bool operator()(JSString *s1, JSString *s2) const
        {
            return 0 > js_CompareStrings(s1, s2);
        }
    };
}

#endif // ! INCLUDED_JSCRIPT_H
