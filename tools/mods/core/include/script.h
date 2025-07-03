/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// JavaScript wrapper functions to create objects, properties, and methods.

#pragma once

#include "rc.h"
#include "jsapi.h"
#include "massert.h"
#include "utility.h"
#include <string>
#include <vector>
#include <utility>
#include <tuple>
#include <type_traits>

class SProperty;
class SMethod;
class SObject;
class SVal;

typedef vector<SObject *>   SObjectVec;
typedef vector<SProperty *> SPropertyVec;
typedef vector<SMethod *>   SMethodVec;

void * GetPrivateAndComplainIfNull(JSContext * cx,
                                   JSObject * obj,
                                   const char * name,
                                   const char * jsClassName);

// Extern this here since every file that might need to
//  reference this function already includes script.h
extern JSBool C_Global_Help
(
      JSContext *    pContext,
      JSObject  *    pObject,
      uintN          NumArguments,
      jsval     *    pArguments,
      jsval     *    pReturlwalue
);

// Property/test/method should only be available in internal non-customer) builds.
#define MODS_INTERNAL_ONLY        0x10000000

// Property name is to be added to an object that holds a list of valid
// arguments for this test.
#define SPROP_VALID_TEST_ARG 0x20000000

/*
 *------------------------------------------------------------------------------
 * @class(SProperty)
 *
 * InitialValue:
 *    FLOAT64
 *    string
 *
 * Note: Objects do not require an initial value.
 *
 * JSPropertyOp:
 *
 *    JSBool (* JSPropertyOp)(JSContext * pContext, JSObject * pObject,
 *       jsval PropertyId, jsval * pValue);
 *
 * Flags:
 *
 *    JSPROP_ENUMERATE    Property is visible in for and in loops.
 *    JSPROP_READONLY     Property is read only.
 *    JSPROP_PERMANENT    Property cannot be deleted.
 *    JSPROP_EXPORTED     Property can be imported by other objects.
 *    JSPROP_INDEX        Property is actually an index into an array of
 *                        properties, and is cast to a const char *.
 *    MODS_INTERNAL_ONLY  This property should only be available in internal mods releases.
 *------------------------------------------------------------------------------
 */

class SProperty
{
   public:

      // Create a global property.
      SProperty(const char * Name, int8 Id, FLOAT64 InitialValue,
         JSPropertyOp GetMethod, JSPropertyOp SetMethod, uintN Flags,
         const char * HelpMessage);

      SProperty(const char * Name, int8 Id, string InitialValue,
         JSPropertyOp GetMethod, JSPropertyOp SetMethod, uintN Flags,
         const char * HelpMessage);

      SProperty(const char * Name, int8 Id, /* no initial value for object */
         JSPropertyOp GetMethod, JSPropertyOp SetMethod, uintN Flags,
         const char * HelpMessage);

      // Create an object property.
      SProperty(SObject & Object, const char * Name, int8 Id,
         FLOAT64 InitialValue, JSPropertyOp GetMethod, JSPropertyOp SetMethod,
         uintN Flags, const char * HelpMessage);

      SProperty(SObject & Object, const char * Name, int8 Id,
         string InitialValue, JSPropertyOp GetMethod, JSPropertyOp SetMethod,
         uintN Flags, const char * HelpMessage);

      SProperty(SObject & Object, const char * Name, int8 Id,
         /* no initial value for object */ JSPropertyOp GetMethod,
         JSPropertyOp SetMethod, uintN Flags, const char * HelpMessage);

      ~SProperty();

      void AddToSObject();
      RC DefineProperty(JSContext * pContext, JSObject * pJSObject);

      const char * GetObjectName() const;
      const char * GetName() const;
      const char * GetHelpMessage() const;
      const UINT32 GetFlags() const;

      RC ToJsval(jsval * pValue) const;

   private:
      SProperty(SObject * pSObject, const char * name, int8 id,
         SVal * pInitialValue, JSPropertyOp getMethod, JSPropertyOp setMethod,
         uintN flags, const char * helpMessage);

      SObject    * m_pSObject;
      const char * m_Name;
      int8         m_Id;
      SVal       * m_pInitialValue;
      JSPropertyOp m_GetMethod;
      JSPropertyOp m_SetMethod;
      uintN        m_Flags;
      const char * m_HelpMessage;
      JSContext  * m_pContext;
      JSObject   * m_pJSObject;

      // Do not allow copying.
      SProperty(const SProperty &);
      SProperty & operator= (const SProperty &);
};

/**
 *------------------------------------------------------------------------------
 * @class(SMethod)
 *
 * JSNative:
 *
 *    JSBool (* JSNative)(JSContext * pContext, JSObject * pObject,
 *       uintN NumArgumnets, jsval * pArguments, jsval * pReturlwalue);
 *
 *------------------------------------------------------------------------------
 */

class SMethod
{
   public:

      // Create a global method.
      SMethod(const char * Name, JSNative CallFunction, uintN NumArguments,
         const char * HelpMessage, uintN Flags = 0);

      // Create an object method.
      SMethod(SObject & Object, const char * Name, JSNative CallFunction,
         uintN NumArguments, const char * HelpMessage, uintN Flags = 0);

      virtual ~SMethod();

      void AddToSObject();
      RC DefineMethod(JSContext * pContext, JSObject * pJSObject);

      const char * GetObjectName() const;
      const char * GetName() const;
      const char * GetHelpMessage() const;

      enum MethodType
      {
         None,
         Function,
         Test
      };

      virtual MethodType GetMethodType() const;

   private:

      SObject    * m_pSObject;
      const char * m_Name;
      JSNative     m_CallFunction;
      uintN        m_NumArguments;
      const char * m_HelpMessage;
      uintN        m_Flags;

      // Do not allow copying.
      SMethod(const SMethod &);
      SMethod & operator= (const SMethod &);

};

/**
 *------------------------------------------------------------------------------
 * @class(STest)
 *
 * An STest is an SMethod whose native function returns an RC.
 * An STest can be logged.
 *
 * JSNative:
 *
 *    JSBool (* JSNative)(JSContext * pContext, JSObject * pObject,
 *       uintN NumArgumnets, jsval * pArguments, jsval * pReturlwalue);
 *
 *------------------------------------------------------------------------------
 */

class STest : public SMethod
{
   public:

      // Create a global test.
      STest(const char * Name, JSNative CallFunction, uintN NumArguments,
         const char * HelpMessage, uintN Flags = 0);

      // Create an object test.
      STest(SObject & Object, const char * Name, JSNative CallFunction,
         uintN NumArguments, const char * HelpMessage, uintN Flags = 0);

      // SMethod overrides.
      virtual MethodType GetMethodType() const;
};

/*
 *------------------------------------------------------------------------------
 * @class(SObject)
 *
 * Flags:
 *
 *    JSPROP_ENUMERATE    Property is visible in for and in loops.
 *    JSPROP_READONLY     Property is read only.
 *    JSPROP_PERMANENT    Property cannot be deleted.
 *    JSPROP_EXPORTED     Property can be imported by other objects.
 *    JSPROP_INDEX        Property is actually an index into an array of
 *                        properties, and is cast to a const char *.
 *    MODS_INTERNAL_ONLY  This object should only be available in internal mods releases.
 *------------------------------------------------------------------------------
 */

class SObject
{
   public:

      // Create simple global object.
      SObject(const char * Name, JSClass & Class, SObject * pPrototype,
         uintN Flags, const char * HelpMessage,
         /* obsolete */ bool AddNameProperty = true);

      // Create global class.
      SObject(const char * Name, JSClass & Class, SObject * pPrototype,
         uintN Flags, const char * HelpMessage,
         JSNative Constructor,
         /* obsolete */ bool AddNameProperty = true);

      // Create global object with initialization callback to be exelwted at
      // creation. Returning NULL from callback indicates error.
      SObject(const char * Name, JSClass & Class, SObject * pPrototype,
         uintN Flags, const char * HelpMessage,
         JSObjectOp GlobalInit,
         /* obsolete */ bool AddNameProperty = true);

      ~SObject();

      void AddProperty(const char * Name, SProperty * pProperty);
      void AddMethod(const char * Name, SMethod * pMethod);

      // This function should only be called when all of this SObject's
      // properties and methods were allocated with "new"  As of 9/27/05, this
      // only applies to JS that comes from external tests.
      void FreeMethodsAndProperties();

      RC DefineObject(JSContext * pContext, JSObject * pParent);

      JSObject * GetJSObject() const;

      const SPropertyVec & GetProperties() const;
      const SMethodVec   & GetMethods() const;
      const SMethodVec   & GetInheritedMethods() const;

      const char * GetHelpMessage() const;
      const char * GetName() const;
      const char * GetClassName() const;

      // Check whether this object describes JS class or namespace-like object.
      bool IsClass() const;
      // Check if *pObject is direct instance of JS class held by this SObject.
      // Warning: returns false for subclass instance.
      bool IsInstance(JSContext * pContext, JSObject * pObject) const;

      // Checks if this object exists in pObject prototype chain
      bool IsPrototypeOf(JSContext * pContext, JSObject * pObject) const;

   private:

      const char   * m_Name;
      JSClass      * m_pClass;
      SObject      * m_pPrototype;
      uintN          m_Flags;
      const char   * m_HelpMessage;
      JSNative       m_Constructor;
      JSObjectOp     m_GlobalInit;
      SPropertyVec   m_Properties;
      SMethodVec     m_Methods;
      SMethodVec     m_InheritedMethods;
      JSObject     * m_pJSObject;
      SProperty    * m_pNameProperty;

      // Do not allow copying.
      SObject(const SObject &);
      SObject & operator= (const SObject &);
};

//------------------------------------------------------------------------------
// Get registered properties, methods, and objects.
//------------------------------------------------------------------------------

extern SPropertyVec & GetGlobalSProperties();
extern SMethodVec   & GetGlobalSMethods();
extern SObjectVec   & GetSObjects();
extern SPropertyVec & GetNonGlobalSProperties();
extern SMethodVec   & GetNonGlobalSMethods();

// Nifty counter to initialize the containers before they are used.
class ContainerInit
{
   public:
      ContainerInit();
      ~ContainerInit();
};

static ContainerInit s_ContainerInit;

namespace Details
{
    struct _Tag_1 {};
    struct _Tag_2 {};

    // Tag1_t and Tag2_t are workarounds for Visual Studio 2015 SFINAE
    // limitations. VS 2015 parser substitutes bare decltype by 'unknown-type'
    // and then complains that the correspondent construct has already been
    // defined. These tags make it different for the parser.
    template <typename...>
    struct _Tagger_1 { using type = _Tag_1; };

    template <typename... Ts>
    using Tag1_t = typename _Tagger_1<Ts...>::type;

    template <typename...>
    struct _Tagger_2 { using type = _Tag_2; };

    template <typename... Ts>
    using Tag2_t = typename _Tagger_2<Ts...>::type;

    struct RcWrapFuncIlwoker
    {
        template <typename FunObj, typename Ret, typename Obj, typename Tuple, size_t... idx>
        static decltype(auto) CallWithTuple
        (
            Obj&& o,
            Ret FunObj::*f,
            Tuple&& t,
            index_sequence<idx...>,
            // make sure it is defined only if f is callable with these arguments
            decltype((declval<Obj>().*f)(get<idx>(declval<Tuple>())...))* = nullptr)
        {
            return (forward<Obj>(o).*f)(get<idx>(forward<Tuple>(t))...);
        }

        template <typename Func, typename Tuple, size_t... idx>
        static decltype(auto) CallWithTuple
        (
            Func&& f,
            Tuple&& t,
            index_sequence<idx...>,
            decltype((*f)(get<idx>(declval<Tuple>())...))* = nullptr)
        {
            return forward<Func>(f)(get<idx>(forward<Tuple>(t))...);
        }
    };

    // The trick is ReturnRc is always called with the last argument a pointer
    // to the out parameter. When f is a function that doesn't return the result
    // in the last out argument, it will have one argument less than ReturnRc
    // and therefore decltype(declval<F>()(declval<Args>()...)) won't have
    // sense, ReturnRC below will be disregarded by SFINAE and the second
    // ReturnRC will be called. For example let's have
    // struct A
    // {
    //     RC foo(int* pVal) const
    //     {
    //         *pVal = 5;
    //         return -1;
    //     }
    //     int bar()
    //     {
    //         return 6;
    //     }
    // };
    // We call it using
    // {
    //     RC rc;
    //     A aa;
    //     int a;
    //     rc = ReturnRC(aa, &decay_t<decltype(aa)>::bar, nullptr, &a);
    // }
    // The parameters pack below will be <A, int(), A&, int *&> and the third
    // argument will be decltype(bar(int *&)) *, which doesn't make sense,
    // because bar doesn't have arguments at all.

    template <typename FunObj, typename Ret, typename Obj, typename... Args>
    RC ReturnRC
    (
        Obj&& o,
        Ret FunObj::*f,
        // this ReturnRC will be used if the decltype below has sense, which
        // will be the case if FunObj::*f is callable with the Args... pack.
        Tag1_t<decltype((declval<Obj>().*f)(declval<Args>()...))>*,
        Args&&... args
    )
    {
        return (forward<Obj>(o).*f)(forward<Args>(args)...);
    }

    template <typename FunObj, typename Ret, typename Obj, typename... Args>
    RC ReturnRC
    (
        Obj&& o,
        Ret FunObj::*f,
        // this ReturnRC will be used if the decltype below has sense, which
        // will be the case if FunObj::*f is callable with the Args... pack
        // without the last argument of the pack
        Tag2_t<
            decltype(
                RcWrapFuncIlwoker::CallWithTuple(
                    declval<Obj>(),
                    f,
                    declval<tuple<Args...>>(),
                    make_index_sequence<sizeof...(Args)-1>()
                )
            )
        >*,
        Args&&... args
    )
    {
        // ilwoke f without the last arg
        auto ret = RcWrapFuncIlwoker::CallWithTuple(
            forward<Obj>(o),
            f,
            forward_as_tuple(args...),
            make_index_sequence<sizeof...(args)-1>()
        );
        // stuff the result into the last argument
        *get<sizeof...(args) - 1>(forward_as_tuple(args...)) = ret;

        return OK;
    }

    // similar to RC_WRAP, this is used for free functions
    // including lambda and function objects
    template <typename Func, typename... Args>
    RC ReturnRC
    (
        Func&& f,
        Tag1_t<decltype(declval<Func>()(declval<Args>()...))>*,
        Args&&... args)
    {
        return forward<Func>(f)(forward<Args>(args)...);
    }

    template <typename Func, typename... Args>
    RC ReturnRC
    (
        Func&& f,
        Tag2_t<
            decltype(
                RcWrapFuncIlwoker::CallWithTuple(
                    declval<Func>(),
                    declval<tuple<Args...>>(),
                    make_index_sequence<sizeof...(Args) - 1>()
                )
            )
        >*,
        Args&&... args)
    {
        auto ret = RcWrapFuncIlwoker::CallWithTuple(
            forward<Func>(f),
            forward_as_tuple(args...),
            make_index_sequence<sizeof...(args) - 1>()
        );
        *get<sizeof...(args) - 1>(forward_as_tuple(args...)) = ret;

        return OK;
    }
}

// RcWrap assumes that the last parameter of the arguments pack is an out
// parameter. If f can be called with the args, RcWrap calls it and returns what
// f returns. If f cannot be called with args, RcWrap will try to call f without
// the last argument supplied to RcWrap, then copy the f result to the last out
// parameter and return OK.
template <typename FunObj, typename Ret, typename Obj, typename... Args>
RC RcWrap(Obj&& o, Ret FunObj::*f, Args&&... args)
{
    return Details::ReturnRC(forward<Obj>(o), f, nullptr, forward<Args>(args)...);
}

template <typename Func, typename... Args>
RC RcWrap(Func&& f, Args... args)
{
    return Details::ReturnRC(forward<Func>(f), nullptr, forward<Args>(args)...);
}

#define RC_WRAP(obj, fun, ...) RcWrap(obj, &decay_t<decltype(obj)>::fun, __VA_ARGS__)

#define RC_WRAP_NAMESPACE(fun, ...) RcWrap(fun, __VA_ARGS__)

//------------------------------------------------------------------------------
// Helpful macros.
//------------------------------------------------------------------------------

#define JS_CLASS(c)           \
   static JSClass c##Class =  \
   {                          \
      #c,                     \
      0,                      \
      JS_PropertyStub,        \
      JS_PropertyStub,        \
      JS_PropertyStub,        \
      JS_PropertyStub,        \
      JS_EnumerateStub,       \
      JS_ResolveStub,         \
      JS_ColwertStub,         \
      JS_FinalizeStub         \
   }

#define JS_CLASS_WITH_FLAGS(c, flags) \
   static JSClass c##Class =          \
   {                                  \
      #c,                             \
      flags,                          \
      JS_PropertyStub,                \
      JS_PropertyStub,                \
      JS_PropertyStub,                \
      JS_PropertyStub,                \
      JS_EnumerateStub,               \
      JS_ResolveStub,                 \
      JS_ColwertStub,                 \
      JS_FinalizeStub                 \
   }

#define RETURN_RC(rc)                     \
      return (*pReturlwalue = INT_TO_JSVAL(rc)), \
      JS_TRUE                      \

#define C_(class_method)               \
   static JSBool C_##class_method      \
   (                                   \
      JSContext *    pContext,         \
      JSObject  *    pObject,          \
      uintN          NumArguments,     \
      jsval     *    pArguments,       \
      jsval     *    pReturlwalue      \
   )

#define P_(class_property)             \
   static JSBool class_property        \
   (                                   \
      JSContext *    pContext,         \
      JSObject  *    pObject,          \
      jsval          PropertyId,       \
      jsval     *    pValue            \
   )

#define C_CHECK_RC(f)         \
   do \
   {                       \
      if (OK != (rc = (f)))   \
         RETURN_RC(rc);       \
   } while (0)

#define C_CHECK_RC_CLEANUP(f) \
   do \
   {                       \
      if (OK != (rc = (f)))   \
         goto Cleanup;        \
   } while (0)

#define C_CHECK_RC_MSG(f, msg)              \
   do \
   {                                     \
      if (OK != (rc = (f)))                 \
      {                                     \
         JS_ReportWarning(pContext, msg);   \
         RETURN_RC(rc);                     \
      }                                     \
   } while (0)

#define C_CHECK_RC_THROW(f, msg)                \
    do                                              \
    {                                               \
        if (OK != (rc = (f)))                       \
        {                                           \
            pJs->Throw(pContext, rc, msg);          \
            return JS_FALSE;                        \
        }                                           \
    } while (0)

//! \brief Defines a basic property with no Get/Set functions
//!
//! \sa PROP_DESC
#define PROP_DESC_FULL(object_name,                         \
                       prop_name,                           \
                       value,                               \
                       flags,                               \
                       desc)                                \
   static SProperty object_name ## _ ## prop_name           \
   (                                                        \
      object_name##_Object,                                 \
      #prop_name,                                           \
      0,                                                    \
      value,                                                \
      0,                                                    \
      0,                                                    \
      flags,                                                \
      desc                                                  \
   )

//! \brief Defines a basic property of an object with no Get/Set functions and
//! does not support setting the flags property
//!
//! \sa PROP_DESC_FULL
#define PROP_DESC(object_name, prop_name, value, desc)          \
        PROP_DESC_FULL(object_name, prop_name, value, 0, desc)  \

//! \brief Defines a basic property of an object with no Get/Set functions that
//! is read only.
#define PROP_CONST(object_name, symbolic_const, value) \
   static SProperty object_name ## _ ## symbolic_const \
   (                                                   \
      object_name##_Object,                            \
      #symbolic_const,                                 \
      0,                                               \
      value,                                           \
      0,                                               \
      0,                                               \
      JSPROP_READONLY,                                 \
      "CONSTANT: " #symbolic_const                     \
   )

//! Defines a basic property of an object with no Get/Set functions that
//! is read only.
//!
//! \sa PROP_CONST
//! \note Not sure why PROP_CONST was defined the way it was way back when,
//! but I'm going to leave it alone.  PROP_CONST_DESC is useful if you want
//! to provide a meaningful help message
#define PROP_CONST_DESC(object_name, symbolic_const, value, desc) \
   static SProperty object_name ## _ ## symbolic_const \
   (                                                   \
      object_name##_Object,                            \
      #symbolic_const,                                 \
      0,                                               \
      value,                                           \
      0,                                               \
      0,                                               \
      JSPROP_READONLY,                                 \
      desc                                             \
   )

//! \brief Defines a property that has a Get function returning a RC defined by
//! the given C++ object.  If the Get function fails, or any other failure is
//! detected a JS exception is thrown
#define PROP_READONLY(objName, cppObj, funcName, resultType, helpmsg) \
   P_(objName##_Get_##funcName);                                            \
   P_(objName##_Get_##funcName)                                             \
   {                                                                        \
      resultType result{};                                                  \
      RC rc = RC_WRAP(*cppObj, funcName, &result);                          \
      JavaScriptPtr pJs;                                                    \
      if (rc != OK)                                                         \
      {                                                                     \
          pJs->Throw(pContext, rc, Utility::StrPrintf("Error getting %s.%s",\
                                   #objName, #funcName));                   \
          return JS_FALSE;                                                  \
      }                                                                     \
      rc = pJs->ToJsval(result, pValue);                                    \
      if (OK != rc)                                                         \
      {                                                                     \
          pJs->Throw(pContext, rc, Utility::StrPrintf(                      \
                     "Error colwerting result in %s.%s",                    \
                     #objName, #funcName));                                 \
          *pValue = JSVAL_NULL;                                             \
          return JS_FALSE;                                                  \
      }                                                                     \
      return JS_TRUE;                                                       \
   }                                                                        \
   static SProperty objName ## _ ## funcName                                \
   (                                                                        \
      objName ## _Object,                                                   \
      #funcName,                                                            \
      0,                                                                    \
      0,                                                                    \
      objName ## _Get_ ## funcName,                                         \
      0,                                                                    \
      0,                                                                    \
      helpmsg                                                               \
   )

//! \brief Defines a property that has a Get function returning a RC defined by
//! the given C++ object.  If object is NULL, returns 0/false.  If the Get
//! function fails, or any other failure is detected a JS exception is thrown
#define PROP_READONLY_CHECKNULL(objName, cppObj, funcName, resultType, helpmsg)\
   P_(objName##_Get_##funcName);                                            \
   P_(objName##_Get_##funcName)                                             \
   {                                                                        \
      RC rc;                                                                \
      resultType result{};                                                  \
      JavaScriptPtr pJs;                                                    \
      if (0 != (cppObj))                                                    \
      {                                                                     \
          rc = RC_WRAP(*cppObj, funcName, &result);                         \
          if (rc != OK)                                                     \
          {                                                                 \
              pJs->Throw(pContext, rc, Utility::StrPrintf(                  \
                         "Error getting %s.%s", #objName, #funcName));      \
              return JS_FALSE;                                              \
          }                                                                 \
      }                                                                     \
      rc = pJs->ToJsval(result, pValue);                                    \
      if (OK != rc)                                                         \
      {                                                                     \
          pJs->Throw(pContext, rc, Utility::StrPrintf(                      \
                     "Error colwerting result in %s.%s",                    \
                     #objName, #funcName));                                 \
          *pValue = JSVAL_NULL;                                             \
          return JS_FALSE;                                                  \
      }                                                                     \
      return JS_TRUE;                                                       \
   }                                                                        \
   static SProperty objName ## _ ## funcName                                \
   (                                                                        \
      objName ## _Object,                                                   \
      #funcName,                                                            \
      0,                                                                    \
      0,                                                                    \
      objName ## _Get_ ## funcName,                                         \
      0,                                                                    \
      0,                                                                    \
      helpmsg                                                               \
   )

//! \brief Defines a property that has Get/Set functions defined by
//! the given C++ object.
#define PROP_READWRITE(objName, cppObj, funcname, resulttype, helpmsg)   \
   P_(objName##_Get_##funcname);                                    \
   P_(objName##_Set_##funcname);                                    \
   P_(objName##_Get_##funcname)                                     \
   {                                                                \
      resulttype result{};                                          \
      RC rc = RC_WRAP(*cppObj, Get##funcname, &result);             \
      JavaScriptPtr pJs;                                            \
      if (rc != OK)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error getting %s.%s", #objName, #funcname));  \
          return JS_FALSE;                                          \
      }                                                             \
      rc = pJs->ToJsval(result, pValue);                            \
      if (OK != rc)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error colwerting result in %s.%s",            \
                     #objName, #funcname));                         \
          *pValue = JSVAL_NULL;                                     \
          return JS_FALSE;                                          \
      }                                                             \
      return JS_TRUE;                                               \
   }                                                                \
   P_(objName##_Set_##funcname)                                     \
   {                                                                \
      resulttype Value{};                                           \
      JavaScriptPtr pJs;                                            \
      RC rc = pJs->FromJsval(*pValue, &Value);                      \
      if (OK != rc)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error colwerting input in %s.%s",             \
                     #objName, #funcname));                         \
          return JS_FALSE;                                          \
      }                                                             \
                                                                    \
      rc = cppObj->Set##funcname(Value);                            \
      if (OK != rc)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error setting %s.%s", #objName, #funcname));  \
          *pValue = JSVAL_NULL;                                     \
          return JS_FALSE;                                          \
      }                                                             \
      return JS_TRUE;                                               \
   }                                                                \
   static SProperty objName ## _ ## funcname                        \
   (                                                                \
      objName ## _Object,                                           \
      #funcname,                                                    \
      0,                                                            \
      0,                                                            \
      objName ## _Get_ ## funcname,                                 \
      objName ## _Set_ ## funcname,                                 \
      0,                                                            \
      helpmsg                                                       \
   )

//! \brief Defines a property that has Get/Set functions defined in the
//! given namespace.  If the Get or Set function fails, or any other failure is
//! detected a JS exception is thrown
#define PROP_READWRITE_NAMESPACE(name, funcname, resulttype, helpmsg)   \
   P_(name##_Get_##funcname);                                       \
   P_(name##_Set_##funcname);                                       \
   P_(name##_Get_##funcname)                                        \
   {                                                                \
      resulttype result{};                                          \
      RC rc = RC_WRAP_NAMESPACE(name::Get##funcname, &result);      \
      JavaScriptPtr pJs;                                            \
      if (rc != OK)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error getting %s.%s", #name, #funcname));     \
          return JS_FALSE;                                          \
      }                                                             \
      rc = pJs->ToJsval(result, pValue);                            \
      if (OK != rc)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error colwerting result in %s.%s",            \
                     #name, #funcname));                            \
          *pValue = JSVAL_NULL;                                     \
          return JS_FALSE;                                          \
      }                                                             \
      return JS_TRUE;                                               \
   }                                                                \
   P_(name##_Set_##funcname)                                        \
   {                                                                \
      resulttype Value{};                                           \
      JavaScriptPtr pJs;                                            \
      RC rc = pJs->FromJsval(*pValue, &Value);                      \
      if (OK != rc)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error colwerting input in %s.%s",             \
                     #name, #funcname));                            \
          return JS_FALSE;                                          \
      }                                                             \
      rc = name::Set##funcname(Value);                              \
      if (OK != rc)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error setting %s.%s", #name, #funcname));     \
          *pValue = JSVAL_NULL;                                     \
          return JS_FALSE;                                          \
      }                                                             \
      return JS_TRUE;                                               \
   }                                                                \
   static SProperty name ## _ ## funcname                           \
   (                                                                \
      name ## _Object,                                              \
      #funcname,                                                    \
      0,                                                            \
      0,                                                            \
      name ## _Get_ ## funcname,                                    \
      name ## _Set_ ## funcname,                                    \
      0,                                                            \
      helpmsg                                                       \
   )

//! \brief Defines a property that has only a Get function defined in the
//! given namespace.  If the Get function fails, or any other failure is
//! detected a JS exception is thrown
#define PROP_READONLY_NAMESPACE(name, funcname, resulttype, helpmsg)   \
   P_(name##_Get_##funcname);                                       \
   P_(name##_Get_##funcname)                                        \
   {                                                                \
      resulttype result{};                                          \
      RC rc = RC_WRAP_NAMESPACE(name::Get##funcname, &result);      \
      JavaScriptPtr pJs;                                            \
      if (rc != OK)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error getting %s.%s", #name, #funcname));     \
          return JS_FALSE;                                          \
      }                                                             \
      rc = pJs->ToJsval(result, pValue);                            \
      if (OK != rc)                                                 \
      {                                                             \
          pJs->Throw(pContext, rc, Utility::StrPrintf(              \
                     "Error colwerting result in %s.%s",            \
                     #name, #funcname));                            \
          *pValue = JSVAL_NULL;                                     \
          return JS_FALSE;                                          \
      }                                                             \
      return JS_TRUE;                                               \
   }                                                                \
   static SProperty name ## _ ## funcname                           \
   (                                                                \
      name ## _Object,                                              \
      #funcname,                                                    \
      0,                                                            \
      0,                                                            \
      name ## _Get_ ## funcname,                                    \
      0,                                                            \
      0,                                                            \
      helpmsg                                                       \
   )

//! \brief Defines a property that has a Get function returning a RC defined by
//! a C++ class whose current instance is retrieved from the JS context.  If the
//! Get function fails, or any other failure is detected a JS exception is thrown
//!
//! \sa JS_GET_PRIVATE, CLASS_PROP_READONLY_THROW
#define CLASS_PROP_READONLY_FULL(objName,                                   \
                                       propName,                            \
                                       resulttype,                          \
                                       helpmsg,                             \
                                       flags,                               \
                                       initVal)                             \
   P_(objName##_Get_##propName);                                            \
   P_(objName##_Get_##propName)                                             \
   {                                                                        \
      MASSERT(pContext != 0);                                               \
      MASSERT(pValue   != 0);                                               \
                                                                            \
      objName * pTest = nullptr;                                            \
      if ((pTest = JS_GET_PRIVATE(objName, pContext, pObject, NULL)) != 0)  \
      {                                                                     \
          resulttype result{};                                              \
          RC rc = RC_WRAP(*pTest, Get##propName, &result);                  \
          JavaScriptPtr pJs;                                                \
          if (rc != OK)                                                     \
          {                                                                 \
              pJs->Throw(pContext, rc, Utility::StrPrintf(                  \
                         "Error getting %s.%s", #objName, #propName));      \
              return JS_FALSE;                                              \
          }                                                                 \
          rc = pJs->ToJsval(result, pValue);                                \
          if (OK != rc)                                                     \
          {                                                                 \
              pJs->Throw(pContext, rc, Utility::StrPrintf(                  \
                         "Error colwerting result in %s.%s",                \
                         #objName, #propName));                             \
              *pValue = JSVAL_NULL;                                         \
              return JS_FALSE;                                              \
          }                                                                 \
          return JS_TRUE;                                                   \
      }                                                                     \
      return JS_FALSE;                                                      \
   }                                                                        \
   static SProperty objName ## _ ## propName                                \
   (                                                                        \
      objName ## _Object,                                                   \
      #propName,                                                            \
      0,                                                                    \
      initVal,                                                              \
      objName ## _Get_ ## propName,                                         \
      0,                                                                    \
      JSPROP_READONLY | flags,                                              \
      helpmsg                                                               \
   )

//! \brief Defines a property that has a Get function returning a RC defined by
//! a C++ class whose current instance is retrieved from the JS context.  If the
//! Get function fails, or any other failure is detected a JS exception is thrown
//!
//! \note The flags property is not supported
//! \sa JS_GET_PRIVATE, CLASS_PROP_READONLY_FULL_THROW
#define CLASS_PROP_READONLY(objName, propName, resulttype, helpmsg)             \
        CLASS_PROP_READONLY_FULL(objName, propName, resulttype, helpmsg, 0, 0)

//! \brief Defines a property that has Get/Set functions returning a RC  defined by
//! a C++ class whose current instance is retrieved from the JS context.  If the
//! Get or Set function fails, or any other failure is detected a JS exception is
//! thrown
//!
//! \sa JS_GET_PRIVATE, CLASS_PROP_READWRITE_THROW
#define CLASS_PROP_READWRITE_FULL(objName,                                     \
                                  propName,                                    \
                                  resulttype,                                  \
                                  helpmsg,                                     \
                                  flags,                                       \
                                  initVal)                                     \
   P_(objName##_Get_##propName);                                               \
   P_(objName##_Set_##propName);                                               \
   P_(objName##_Get_##propName)                                                \
   {                                                                           \
      MASSERT(pContext != 0);                                                  \
      MASSERT(pValue   != 0);                                                  \
                                                                               \
      objName * pTest = nullptr;                                               \
      if ((pTest = JS_GET_PRIVATE(objName, pContext, pObject, NULL)) != 0)     \
      {                                                                        \
          resulttype result{};                                                 \
          RC rc = RC_WRAP(*pTest, Get##propName, &result);                     \
          JavaScriptPtr pJs;                                                   \
          if (rc != OK)                                                        \
          {                                                                    \
              pJs->Throw(pContext, rc, Utility::StrPrintf(                     \
                         "Error getting %s.%s", #objName, #propName));         \
              return JS_FALSE;                                                 \
          }                                                                    \
          rc = pJs->ToJsval(result, pValue);                                   \
          if (OK != rc)                                                        \
          {                                                                    \
              pJs->Throw(pContext, rc, Utility::StrPrintf(                     \
                         "Error colwerting result in %s.%s",                   \
                         #objName, #propName));                                \
              *pValue = JSVAL_NULL;                                            \
              return JS_FALSE;                                                 \
          }                                                                    \
          return JS_TRUE;                                                      \
      }                                                                        \
      return JS_FALSE;                                                         \
   }                                                                           \
   P_(objName##_Set_##propName)                                                \
   {                                                                           \
      MASSERT(pContext != 0);                                                  \
      MASSERT(pValue   != 0);                                                  \
                                                                               \
      resulttype Value{};                                                      \
      objName * pTest = nullptr;                                               \
      if ((pTest = JS_GET_PRIVATE(objName, pContext, pObject, NULL)) != 0)     \
      {                                                                        \
          JavaScriptPtr pJs;                                                   \
          RC rc = pJs->FromJsval(*pValue, &Value);                             \
          if (OK != rc)                                                        \
          {                                                                    \
              pJs->Throw(pContext, rc, Utility::StrPrintf(                     \
                         "Error colwerting input in %s.%s",                    \
                         #objName, #propName));                                \
              return JS_FALSE;                                                 \
          }                                                                    \
                                                                               \
          rc = pTest->Set##propName(Value);                                    \
          if ( OK != rc )                                                      \
          {                                                                    \
             pJs->Throw(pContext, rc, Utility::StrPrintf(                      \
                        "Error setting %s.%s", #objName, #propName));          \
             *pValue = JSVAL_NULL;                                             \
             return JS_FALSE;                                                  \
          }                                                                    \
          return JS_TRUE;                                                      \
      }                                                                        \
      return JS_FALSE;                                                         \
   }                                                                           \
   static SProperty objName ## _ ## propName                                   \
   (                                                                           \
      objName ## _Object,                                                      \
      #propName,                                                               \
      0,                                                                       \
      initVal,                                                                 \
      objName ## _Get_ ## propName,                                            \
      objName ## _Set_ ## propName,                                            \
      flags,                                                                   \
      helpmsg                                                                  \
   )

//! \brief Defines a property that has Get/Set functions returning a RC  defined by
//! a C++ class whose current instance is retrieved from the JS context.  If the
//! Get or Set function fails, or any other failure is detected a JS exception is
//! thrown
//!
//! \note The flags property is not supported
//! \sa JS_GET_PRIVATE, CLASS_PROP_READWRITE_FULL_THROW
#define CLASS_PROP_READWRITE(objName, propName, resulttype, helpmsg)           \
        CLASS_PROP_READWRITE_FULL(objName, propName, resulttype, helpmsg, SPROP_VALID_TEST_ARG, 0)

//! \brief Defines a property that allows custom Get/Set functions defined by
//! a C++ class whose current instance is retrieved from the JS context.
//!
//! \sa JS_GET_PRIVATE
#define CLASS_PROP_READWRITE_LWSTOM_FULL(objName,                              \
                                         propName,                             \
                                         helpmsg,                              \
                                         flags,                                \
                                         initVal)                              \
   P_(objName##_Get_##propName);                                               \
   P_(objName##_Set_##propName);                                               \
   static SProperty objName ## _ ## propName                                   \
   (                                                                           \
      objName ## _Object,                                                      \
      #propName,                                                               \
      0,                                                                       \
      initVal,                                                                 \
      objName ## _Get_ ## propName,                                            \
      objName ## _Set_ ## propName,                                            \
      flags,                                                                   \
      helpmsg                                                                  \
   )

#define CLASS_PROP_READWRITE_LWSTOM(objName, propName, helpmsg)\
        CLASS_PROP_READWRITE_LWSTOM_FULL(objName, propName, helpmsg, SPROP_VALID_TEST_ARG, 0)


// should only be called with JsArray as resulttype but I leave it in its more
// general form in case it becomes applicable elsewhere
#define CLASS_PROP_READWRITE_JSARRAY_FULL(objName, propName, resulttype, helpmsg, flags) \
   P_(objName##_Get_##propName);                                             \
   P_(objName##_Set_##propName);                                             \
   P_(objName##_Get_##propName)                                              \
   {                                                                         \
      MASSERT(pValue   != 0);                                                \
                                                                             \
      objName * pTest = nullptr;                                             \
      if ((pTest = JS_GET_PRIVATE(objName, pContext, pObject, NULL)) != 0)   \
      {                                                                      \
          resulttype result{};                                               \
          if (OK != pTest->Get##propName(&result) )                          \
          {                                                                  \
              JS_ReportError(pContext, "Failed to colwert %s.%s",            \
                             #objName, #propName);                           \
              *pValue = JSVAL_NULL;                                          \
              return JS_FALSE;                                               \
          }                                                                  \
          if (OK != JavaScriptPtr()->ToJsval(&result, pValue))               \
          {                                                                  \
              JS_ReportError(pContext, "Failed to get %s.%s",                \
                             #objName, #propName);                           \
              *pValue = JSVAL_NULL;                                          \
              return JS_FALSE;                                               \
          }                                                                  \
          return JS_TRUE;                                                    \
      }                                                                      \
      return JS_FALSE;                                                       \
   }                                                                         \
   P_(objName##_Set_##propName)                                              \
   {                                                                         \
      MASSERT(pValue   != 0);                                                \
                                                                             \
      resulttype Value{};                                                    \
      objName * pTest = nullptr;                                             \
      if ((pTest = JS_GET_PRIVATE(objName, pContext, pObject, NULL)) != 0)   \
      {                                                                      \
          if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))             \
          {                                                                  \
              JS_ReportError(pContext, "Failed to set %s.%s",                \
                             #objName, #propName);                           \
              return JS_FALSE;                                               \
          }                                                                  \
                                                                             \
          if (OK != pTest->Set##propName(&Value))                            \
          {                                                                  \
              JS_ReportError(pContext, "Error Setting %s", #propName);       \
              *pValue = JSVAL_NULL;                                          \
              return JS_FALSE;                                               \
          }                                                                  \
          return JS_TRUE;                                                    \
      }                                                                      \
      return JS_FALSE;                                                       \
   }                                                                         \
   static SProperty objName ## _ ## propName                                 \
   (                                                                         \
      objName ## _Object,                                                    \
      #propName,                                                             \
      0,                                                                     \
      0,                                                                     \
      objName ## _Get_ ## propName,                                          \
      objName ## _Set_ ## propName,                                          \
      flags,                                                                 \
      helpmsg                                                                \
   )

#define CLASS_PROP_READWRITE_JSARRAY(objName, propName, resulttype, helpmsg)\
        CLASS_PROP_READWRITE_JSARRAY_FULL(objName, propName, resulttype, helpmsg, SPROP_VALID_TEST_ARG)

//! \brief This macro will return a pointer to the C++ object instance
//! from JavaScript.
//! \note You should make sure that the pointer returned
//! is not NULL.
//! \note If the test is NULL a JS_ReportError will be called.
//! \example
//!   MyClass * pTest
//!   if ((pTest = JS_GET_PRIVATE(MyClass, pContext, pObject, "Class")) != 0)
//!   {
//!     // Perform operations on test here
//!     return JS_TRUE;
//!   }
//!   return JS_FALSE;
#define JS_GET_PRIVATE(className, pContext, pObject, jsClassName)           \
    (className *) GetPrivateAndComplainIfNull(pContext,                     \
                                              pObject,                      \
                                              #className,                   \
                                              jsClassName)

//! \brief Create the prototype for a JS Method. The function implementation
//! must follow immediately after this macro is used.
//!
//! \example
//! JS_SMETHOD_LWSTOM(MyClass, Foo, 1, "Foo function")
//! {
//!   ...
//!   return JS_TRUE;
//! }
//------------------------------------------------------------------------------
#define JS_SMETHOD_LWSTOM(className, func, args, desc)                  \
    C_( className##_##func );                                           \
    static SMethod className ## _ ## func                               \
    (                                                                   \
        className##_Object,                                             \
        #func,                                                          \
        C_##className##_##func,                                         \
        args,                                                           \
        desc                                                            \
    );                                                                  \
    C_( className##_##func )

//! \brief Create the prototype for a JS Test. The function implementation
//! must follow immediately after this macro is used.
//!
//! \example
//! JS_STEST_LWSTOM(MyClass, Bar, 0, "Bar function")
//! {
//!   ...
//!   RETURN_RC(OK);
//! }
//------------------------------------------------------------------------------
#define JS_STEST_LWSTOM(className, func, args, desc)                    \
    C_( className##_##func );                                           \
    static STest className ## _ ## func                                 \
    (                                                                   \
        className##_Object,                                             \
        #func,                                                          \
        C_##className##_##func,                                         \
        args,                                                           \
        desc                                                            \
    );                                                                  \
    C_( className##_##func )

//! \brief Create the prototype for a global JS Method. The function implementation
//! must follow immediately after this macro is used.
//!
//! \example
//! JS_SMETHOD_GLOBAL(Foo, 1, "Foo function")
//! {
//!   ...
//!   return JS_TRUE;
//! }
//------------------------------------------------------------------------------
#define JS_SMETHOD_GLOBAL(func, args, desc)                             \
    C_( Global_##func );                                                \
    static SMethod Global_ ## func                                      \
    (                                                                   \
        #func,                                                          \
        C_Global_##func,                                                \
        args,                                                           \
        desc                                                            \
    );                                                                  \
    C_( Global_##func )

//! \brief Create the prototype for a global JS Test. The function implementation
//! must follow immediately after this macro is used.
//!
//! \example
//! JS_STEST_GLOBAL(Bar, 0, "Bar function")
//! {
//!   ...
//!   RETURN_RC(OK);
//! }
//------------------------------------------------------------------------------
#define JS_STEST_GLOBAL(func, args, desc)                               \
    C_( Global_##func );                                                \
    static STest Global_ ## func                                        \
    (                                                                   \
        #func,                                                          \
        C_Global_##func,                                                \
        args,                                                           \
        desc                                                            \
    );                                                                  \
    C_( Global_##func )

//! \brief Create the prototype for a JS function. This can be used to create
//! a JS function that takes any number of args.
//! \note When taking 0, 1, or 2 args, it's better to use the ONE_ARG, etc
//! macros below.
//! \note You are responsible for the implementation of the function itself
//! when using this macro!
//------------------------------------------------------------------------------
#define JS_STEST_TEMPLATE(className, func, nArgs, desc) \
    C_( className##_##func); \
    static STest className##_##func \
    ( \
        className##_Object, \
        #func, \
        C_##className##_##func, \
        nArgs, \
        desc \
    )

// Colwenience macros for SMethod and STest functions.  Typical usage:
// JS_STEST_LWSTOM(JsGpuSubdevice, MyFunc, 2, "desc")
// {
//     STEST_HEADER(1, 2, "Usage: GpuSubdevice.MyFunc(arg1 [, arg2])");
//     STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
//     STEST_ARG(0, UINT32, arg1);
//     STEST_OPTIONAL_ARG(1, UINT32, arg2);
//     ...

// Declare usage & pJavaScript variables, and sanity-check the args
#define STEST_HEADER(minNumArgs, maxNumArgs, usageString)               \
    MASSERT(pContext     != nullptr);                                   \
    MASSERT(pArguments   != nullptr);                                   \
    MASSERT(pReturlwalue != nullptr);                                   \
    const int MinNumArguments = minNumArgs;                             \
    const uintN MaxNumArguments = maxNumArgs;                           \
    const char *usage = usageString;                                    \
    *pReturlwalue = JSVAL_NULL;                                         \
    JavaScriptPtr pJavaScript;                                          \
    if ((static_cast<int>(NumArguments) < MinNumArguments) ||           \
        (NumArguments > MaxNumArguments) ||                             \
        !pJavaScript.IsInstalled())                                     \
    {                                                                   \
        JS_ReportError(pContext, usage);                                \
        return JS_FALSE;                                                \
    }                                                                   \
    ct_assert(maxNumArgs >= minNumArgs)

// Declare local var that points to private data; abort if not found
#define STEST_PRIVATE(className, varName, jsClassName)                  \
    className *varName =                                                \
        JS_GET_PRIVATE(className, pContext, pObject, jsClassName);      \
    do                                                                  \
    {                                                                   \
        if (varName == nullptr)                                         \
        {                                                               \
            JS_ReportError(pContext, usage);                            \
            return JS_FALSE;                                            \
        }                                                               \
    } while (0)

// Declare local var for a JS arg, and parse the arg.  Abort on parse error.
#define STEST_ARG(argNum, argType, argName)                             \
    argType argName{};                                                  \
    if (OK != pJavaScript->FromJsval(pArguments[argNum], &argName))     \
    {                                                                   \
        JS_ReportError(pContext, "Invalid value for %s\n%s",            \
                       #argName, usage);                                \
        return JS_FALSE;                                                \
    }                                                                   \
    ct_assert(argNum < MinNumArguments)

// Same as STEST_ARG, but for optional args.
#define STEST_OPTIONAL_ARG(argNum, argType, argName, argDefault)        \
    argType argName = argDefault;                                       \
    if ((argNum < NumArguments) &&                                      \
        (OK != pJavaScript->FromJsval(pArguments[argNum], &argName)))   \
    {                                                                   \
        JS_ReportError(pContext, "Invalid value for %s\n%s",            \
                       #argName, usage);                                \
        return JS_FALSE;                                                \
    }                                                                   \
    ct_assert(argNum >= MinNumArguments);                               \
    ct_assert(argNum < MaxNumArguments)

#define STEST_PRIVATE_ARG(argNum, className, varName, jsClassName)      \
    STEST_ARG(argNum, JSObject*, varName##JSObj);                       \
    className *varName =                                                \
       JS_GET_PRIVATE(className, pContext, varName##JSObj, jsClassName);\
    if (varName == nullptr)                                             \
    {                                                                   \
        JS_ReportError(pContext, usage);                                \
        return JS_FALSE;                                                \
    }                                                                   \

#define STEST_PRIVATE_OPTIONAL_ARG(argNum, className, varName, jsClassName)\
    STEST_OPTIONAL_ARG(argNum, JSObject*, varName##JSObj, nullptr);     \
    className *varName = nullptr;                                       \
    if (varName##JSObj != nullptr)                                      \
    {                                                                   \
        varName = JS_GET_PRIVATE(className, pContext,                   \
                                 varName##JSObj, jsClassName);          \
        if (varName == nullptr)                                         \
        {                                                               \
            JS_ReportError(pContext, usage);                            \
            return JS_FALSE;                                            \
        }                                                               \
    }

//! Create a JS binding called func from a JS object of type className to its
//! private C++ object of type className
//------------------------------------------------------------------------------
#define JS_STEST_BIND_NO_ARGS(className, func, desc)                    \
    JS_STEST_LWSTOM(className, func, 0, desc)                           \
    {                                                                   \
        STEST_HEADER(0, 0, "Usage: " #className "." #func "()");        \
        STEST_PRIVATE(className, pObj, nullptr);                        \
        RETURN_RC(pObj->func());                                        \
    }

#define JS_STEST_BIND_ONE_ARG(className, func, argName, argType, desc)  \
    JS_STEST_LWSTOM(className, func, 1, desc)                           \
    {                                                                   \
        STEST_HEADER(1, 1, "Usage: " #className "." #func "(" #argName ")"); \
        STEST_PRIVATE(className, pObj, nullptr);                        \
        STEST_ARG(0, argType, argName);                                 \
        RETURN_RC(pObj->func(argName));                                 \
    }

#define JS_STEST_BIND_TWO_ARGS(className, func, argName1, argType1, argName2, argType2, desc) \
    JS_STEST_LWSTOM(className, func, 2, desc)                           \
    {                                                                   \
        STEST_HEADER(2, 2, "Usage: " #className "." #func "(" #argName1 ", " #argName2 ")"); \
        STEST_PRIVATE(className, pObj, nullptr);                        \
        STEST_ARG(0, argType1, argName1);                               \
        STEST_ARG(1, argType2, argName2);                               \
        RETURN_RC(pObj->func(argName1, argName2));                      \
    }

#define JS_STEST_BIND_THREE_ARGS(className, func, argName1, argType1, argName2, argType2, \
                                 argName3, argType3, desc) \
    JS_STEST_LWSTOM(className, func, 3, desc)                           \
    {                                                                   \
        STEST_HEADER(3, 3, "Usage: " #className "." #func "(" #argName1 ", " #argName2 ", " \
                     #argName3 ")"); \
        STEST_PRIVATE(className, pObj, nullptr);                        \
        STEST_ARG(0, argType1, argName1);                               \
        STEST_ARG(1, argType2, argName2);                               \
        STEST_ARG(2, argType3, argName3);                               \
        RETURN_RC(pObj->func(argName1, argName2, argName3));            \
    }

#define JS_STEST_BIND_NO_ARGS_NAMESPACE(className, func, desc)          \
    JS_STEST_LWSTOM(className, func, 0, desc)                           \
    {                                                                   \
        STEST_HEADER(0, 0, "Usage: " #className "." #func "()");        \
        RETURN_RC(className::func());                                   \
    }

#define JS_STEST_BIND_ONE_ARG_NAMESPACE(className, func, argType, argName, desc) \
    JS_STEST_LWSTOM(className, func, 1, desc)                           \
    {                                                                   \
        STEST_HEADER(1, 1, "Usage: " #className "." #func "(" #argName ")"); \
        STEST_ARG(0, argType, argName);                                 \
        RETURN_RC(className::func(argName));                            \
    }

#define JS_SMETHOD_BIND_NO_ARGS(className, func, desc)                      \
    JS_SMETHOD_LWSTOM(className, func, 0, desc)                             \
    {                                                                       \
        STEST_HEADER(0, 0, "Usage: " #className "." #func "()");            \
        STEST_PRIVATE(className, pObj, nullptr);                            \
        if (OK != pJavaScript->ToJsval(pObj->func(), pReturlwalue))         \
        {                                                                   \
            JS_ReportError(pContext, "Error oclwrred in %s.%s()",           \
                           #className, #func);                              \
            return JS_FALSE;                                                \
        }                                                                   \
        return JS_TRUE;                                                     \
    }

#define JS_SMETHOD_BIND_ONE_ARG(className, func, argName, argType, desc)     \
    JS_SMETHOD_LWSTOM(className, func, 1, desc)                              \
    {                                                                        \
        STEST_HEADER(1, 1, "Usage: " #className "." #func "(" #argName ")"); \
        STEST_PRIVATE(className, pObj, nullptr);                             \
        STEST_ARG(0, argType, argName);                                      \
        if (RC::OK != pJavaScript->ToJsval(pObj->func(argName), pReturlwalue))\
        {                                                                    \
            JS_ReportError(pContext, "Error oclwrred in %s.%s(%s)",          \
                           #className, #func, #argName);                     \
            return JS_FALSE;                                                 \
        }                                                                    \
        return JS_TRUE;                                                      \
    }

#define JS_SMETHOD_VOID_BIND_NO_ARGS(className, func, desc)             \
    JS_SMETHOD_LWSTOM(className, func, 0, desc)                         \
    {                                                                   \
        STEST_HEADER(0, 0, "Usage: " #className "." #func "()");        \
        STEST_PRIVATE(className, pObj, nullptr);                        \
        pObj->func();                                                   \
        return JS_TRUE;                                                 \
    }

#define JS_SMETHOD_VOID_BIND_ONE_ARG(className, func, argName, argType, desc) \
    JS_SMETHOD_LWSTOM(className, func, 1, desc)                               \
    {                                                                         \
        STEST_HEADER(1, 1, "Usage: " #className "." #func "(" #argName ")");  \
        STEST_PRIVATE(className, pObj, nullptr);                              \
        STEST_ARG(0, argType, argName);                                       \
        pObj->func(argName);                                                  \
        return JS_TRUE;                                                       \
    }

//! Template for a JS_CLASS that owns its own C++ object. No args to the ctor
//-----------------------------------------------------------------------------
#define JS_CLASS_NO_ARGS(className, desc, usage)                    \
   static JSBool C_##className##_constructor                        \
   (                                                                \
      JSContext *cx,                                                \
      JSObject *obj,                                                \
      uintN argc,                                                   \
      jsval *argv,                                                  \
      jsval *rval                                                   \
   )                                                                \
   {                                                                \
      if (argc != 0)                                                \
      {                                                             \
         JS_ReportError(cx, usage);                                 \
         return JS_FALSE;                                           \
      }                                                             \
      className *pClass = new className();                          \
      MASSERT(pClass);                                              \
                                                                    \
      return JS_SetPrivate(cx, obj, pClass);                        \
   }                                                                \
   static void C_##className##_finalize                             \
   (                                                                \
      JSContext *cx,                                                \
      JSObject *obj                                                 \
   )                                                                \
   {                                                                \
      void *pvClass = JS_GetPrivate(cx, obj);                       \
      if (pvClass)                                                  \
      {                                                             \
         className * pClass = (className *)pvClass;                 \
         JavaScriptPtr pJs;                                         \
         pJs->DeferredDelete(pClass);                               \
      }                                                             \
   }                                                                \
   static JSClass className##_class =                               \
   {                                                                \
      #className,                                                   \
      JSCLASS_HAS_PRIVATE,                                          \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_EnumerateStub,                                             \
      JS_ResolveStub,                                               \
      JS_ColwertStub,                                               \
      C_##className##_finalize                                      \
   };                                                               \
   static SObject className##_Object                                \
   (                                                                \
      #className,                                                   \
      className##_class,                                            \
      0,                                                            \
      0,                                                            \
      desc,                                                         \
      C_##className##_constructor                                   \
   )

#define JS_CLASS_INHERIT_FULL(className, parent, desc, flags)       \
   static JSBool C_##className##_constructor                        \
   (                                                                \
      JSContext *cx,                                                \
      JSObject *obj,                                                \
      uintN argc,                                                   \
      jsval *argv,                                                  \
      jsval *rval                                                   \
   )                                                                \
   {                                                                \
      if (argc != 0)                                                \
      {                                                             \
         JS_ReportError(cx, "Usage: new %s()", #className);         \
         return JS_FALSE;                                           \
      }                                                             \
      className *pClass = new className();                          \
      MASSERT(pClass);                                              \
      if (pClass->SetJavaScriptObject(cx, obj) != OK)               \
      {                                                             \
         JS_ReportError(cx, "%s: Unable to save JS Object",         \
                        #className);                                \
         delete pClass;                                             \
         return JS_FALSE;                                           \
      }                                                             \
                                                                    \
      if (pClass->AddExtraObjects(cx, obj) != OK)                   \
      {                                                             \
         JS_ReportError(cx, "%s: Unable to add extra JS Objects",   \
                        #className);                                \
         delete pClass;                                             \
         return JS_FALSE;                                           \
      }                                                             \
                                                                    \
      return JS_SetPrivate(cx, obj, pClass);                        \
   }                                                                \
   static void C_##className##_finalize                             \
   (                                                                \
      JSContext *cx,                                                \
      JSObject *obj                                                 \
   )                                                                \
   {                                                                \
      void *pvClass = JS_GetPrivate(cx, obj);                       \
      if (pvClass)                                                  \
      {                                                             \
         className * pClass = (className *)pvClass;                 \
         JavaScriptPtr pJs;                                         \
         pJs->DeferredDelete(pClass);                               \
      }                                                             \
   }                                                                \
   static JSClass className##_class =                               \
   {                                                                \
      #className,                                                   \
      JSCLASS_HAS_PRIVATE,                                          \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_EnumerateStub,                                             \
      JS_ResolveStub,                                               \
      JS_ColwertStub,                                               \
      C_##className##_finalize                                      \
   };                                                               \
   SObject className##_Object                                       \
   (                                                                \
      #className,                                                   \
      className##_class,                                            \
      &parent##_Object,                                             \
      flags,                                                        \
      desc,                                                         \
      C_##className##_constructor                                   \
   )

#define JS_CLASS_INHERIT(className, parent, desc)                   \
        JS_CLASS_INHERIT_FULL(className, parent, desc, 0)

// This macro should be called by intermediate base classes inheriting
// from ModsTest that aren't the actual tests.  For example, GpuTest,
// ExternalTest, WmpExternalTest
#define JS_CLASS_VIRTUAL_INHERIT(className, parent, desc)           \
   static JSBool C_##className##_constructor                        \
   (                                                                \
      JSContext *cx,                                                \
      JSObject *obj,                                                \
      uintN argc,                                                   \
      jsval *argv,                                                  \
      jsval *rval                                                   \
   )                                                                \
   {                                                                \
      MASSERT( false );                                             \
      return JS_FALSE;                                              \
   }                                                                \
   static void C_##className##_finalize                             \
   (                                                                \
      JSContext *cx,                                                \
      JSObject *obj                                                 \
   )                                                                \
   {                                                                \
      void *pvClass = JS_GetPrivate(cx, obj);                       \
      if (pvClass)                                                  \
      {                                                             \
         className * pClass = (className *)pvClass;                 \
         JavaScriptPtr pJs;                                         \
         pJs->DeferredDelete(pClass);                               \
      }                                                             \
   }                                                                \
   static JSClass className##_class =                               \
   {                                                                \
      #className,                                                   \
      JSCLASS_HAS_PRIVATE,                                          \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_EnumerateStub,                                             \
      JS_ResolveStub,                                               \
      JS_ColwertStub,                                               \
      C_##className##_finalize                                      \
   };                                                               \
   SObject className##_Object                                \
   (                                                                \
      #className,                                                   \
      className##_class,                                            \
      &parent##_Object,                                             \
      0,                                                            \
      desc,                                                         \
      C_##className##_constructor,                                  \
      false                                                         \
   )

#define SIMPLE_JS_CLASS_WITH_RUN(className, objDesc, runDesc)       \
   JS_CLASS( className );                                           \
   static SObject className##_Object                                \
   (                                                                \
       #className,                                                  \
       className##Class,                                            \
       0,                                                           \
       0,                                                           \
       objDesc                                                      \
   );                                                               \
   C_( className##_Run )                                            \
   {                                                                \
       MASSERT(pContext     != 0);                                  \
       MASSERT(pArguments   != 0);                                  \
       MASSERT(pReturlwalue != 0);                                  \
       if (NumArguments != 0)                                       \
       {                                                            \
           JS_ReportError(pContext, "Usage: %s.Run()", #className); \
           return JS_FALSE;                                         \
       }                                                            \
       RETURN_RC(className::Run(pObject));                          \
   }                                                                \
   static STest className##_Run                                     \
   (                                                                \
       className##_Object,                                          \
       "Run",                                                       \
       C_##className##_Run,                                         \
       0,                                                           \
       runDesc                                                      \
   )

#define JS_SET_MEMBER_VALUE_MACRO(jsobj, var, tmp, type, name)      \
        if (OK != pJavaScript->GetProperty(jsobj, name, &tmp))      \
        {                                                           \
            JS_ReportError(pContext, "Failed to get %s const!",     \
                           #name);                                  \
            return JS_FALSE;                                        \
        }                                                           \
        var = (type)tmp;

#define GLOBAL_JS_PROP_CONST(ConstName, ConstVal, Message)                     \
static SProperty Global_##ConstName                                            \
(                                                                              \
   ""#ConstName"",                                                             \
   0,                                                                          \
   ConstVal,                                                                   \
   0,                                                                          \
   0,                                                                          \
   JSPROP_READONLY,                                                            \
   Message                                                                     \
)
