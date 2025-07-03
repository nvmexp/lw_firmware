/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// JavaScript wrapper functions to create objects, properties, and methods.

#include "jsobj.h"

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include <string.h>
#include <memory>
#include <functional>
#include <map>

namespace
{
    const char *s_ValidTestArgs = ENCJSENT("ValidTestArgs");
}

//! \brief Glocal function for getting the test class associated with a JS object
void* GetPrivateAndComplainIfNull
(
    JSContext  * cx
   ,JSObject   * obj
   ,const char * name
   ,const char * jsClassName
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JSClass* pJsClass = JS_GetClass(obj);
    if (!(pJsClass->flags & JSCLASS_HAS_PRIVATE))
    {
        return NULL;
    }

    if (jsClassName && (strcmp(pJsClass->name, jsClassName) != 0))
    {
        JS_ReportWarning(
            cx,
            "JSClass name mismatch for %s. Expected '%s' got '%s'.",
            name,
            jsClassName,
            pJsClass->name);

        return NULL;
    }

    void* pv = JS_GetPrivate(cx, obj);
    if (!pv)
    {
        JS_ReportWarning(cx, "Null GetPrivate for %s!", name);
        MASSERT(!"GetPrivate returned null");
    }
    return pv;
}

//------------------------------------------------------------------------------
// Containers for registered properties, methods, and objects.
//------------------------------------------------------------------------------

static SPropertyVec * s_pGlobalSProperties;
static SMethodVec   * s_pGlobalSMethods;
static SObjectVec   * s_pSObjects;
static SPropertyVec * s_pNonGlobalSProperties;
static SMethodVec   * s_pNonGlobalSMethods;

//------------------------------------------------------------------------------
// SVal - Used to hold initial value of an SProperty
//------------------------------------------------------------------------------

class SVal
{
public:
    virtual ~SVal() { }
    virtual RC ToJsval(JSContext * pContext, jsval * pJsval) const = 0;
};

class NumberSVal : public SVal
{
public:
    NumberSVal(FLOAT64 val) : m_Val(val) { }
    RC ToJsval(JSContext * pContext, jsval * pJsval) const override;
private:
    FLOAT64 m_Val;
};

class StringSVal : public SVal
{
public:
    StringSVal(string val) : m_Val(val) { }
    RC ToJsval(JSContext * pContext, jsval * pJsval) const override;
private:
    string m_Val;
};

class ObjectSVal : public SVal
{
public:
    RC ToJsval(JSContext * pContext, jsval * pJsval) const override;
};

RC NumberSVal::ToJsval(JSContext * pContext, jsval * pJsval) const // override
{
    MASSERT(pContext != 0);
    MASSERT(pJsval != 0);

    if (!JS_NewNumberValue(pContext, m_Val, pJsval))
    {
        return RC::CANNOT_COLWERT_FLOAT_TO_JSVAL;
    }

    return OK;
}

RC StringSVal::ToJsval(JSContext * pContext, jsval * pJsval) const // override
{
    MASSERT(pContext != 0);
    MASSERT(pJsval != 0);

    JSString* pJSString = JS_NewStringCopyZ(pContext, m_Val.c_str());
    if (nullptr == pJSString)
    {
        return RC::CANNOT_COLWERT_STRING_TO_JSVAL;
    }

    *pJsval = STRING_TO_JSVAL(pJSString);

    return OK;
}

JS_CLASS(Object);
RC ObjectSVal::ToJsval(JSContext * pContext, jsval * pJsval) const // override
{
    MASSERT(pContext != 0);
    MASSERT(pJsval != 0);

    JSObject* pObject = JS_NewObject(pContext, &ObjectClass, 0, 0);

    if (nullptr == pObject)
    {
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    *pJsval = OBJECT_TO_JSVAL(pObject);

    return OK;
}

//------------------------------------------------------------------------------
// SProperty
//------------------------------------------------------------------------------

/* private */ SProperty::SProperty
(
    SObject    * pSObject
   ,const char * name
   ,int8         id
   ,SVal       * pInitialValue
   ,JSPropertyOp getMethod
   ,JSPropertyOp setMethod
   ,uintN        flags
   ,const char * helpMessage
) :
    m_pSObject(pSObject)
   ,m_Name(name)
   ,m_Id(id)
   ,m_pInitialValue(pInitialValue)
   ,m_GetMethod(getMethod)
   ,m_SetMethod(setMethod)
   ,m_Flags(flags)
   ,m_HelpMessage(helpMessage)
   ,m_pContext(nullptr)
   ,m_pJSObject(nullptr)
{
    MASSERT(m_Name != nullptr);
    MASSERT(m_pInitialValue != nullptr);
    MASSERT(m_HelpMessage != nullptr);

    // Register the property for later creation.
    if (pSObject == nullptr)
    {
        s_pGlobalSProperties->push_back(this);
    }
    else
    {
        s_pNonGlobalSProperties->push_back(this);
    }
}

//
// Create a global property.
//
SProperty::SProperty
(
    const char*  name
   ,int8         id
   ,FLOAT64      initialValue
   ,JSPropertyOp getMethod
   ,JSPropertyOp setMethod
   ,uintN        flags
   ,const char*  helpMessage
) :
    SProperty(nullptr, name, id, new NumberSVal(initialValue),
              getMethod, setMethod, flags, helpMessage)
{
}

SProperty::SProperty
(
    const char*  name
   ,int8         id
   ,string       initialValue
   ,JSPropertyOp getMethod
   ,JSPropertyOp setMethod
   ,uintN        flags
   ,const char*  helpMessage
) :
    SProperty(nullptr, name, id, new StringSVal(initialValue),
              getMethod, setMethod, flags, helpMessage)
{
}

SProperty::SProperty
(
    const char*  name
   ,int8         id
    /* no initial value for object */
   ,JSPropertyOp getMethod
   ,JSPropertyOp setMethod
   ,uintN        flags
   ,const char*  helpMessage
) :
    SProperty(nullptr, name, id, new ObjectSVal(),
              getMethod, setMethod, flags, helpMessage)
{
}

//
// Create an object property.
//
SProperty::SProperty
(
    SObject&     object
   ,const char*  name
   ,int8         id
   ,FLOAT64      initialValue
   ,JSPropertyOp getMethod
   ,JSPropertyOp setMethod
   ,uintN        flags
   ,const char*  helpMessage
) :
    SProperty(&object, name, id, new NumberSVal(initialValue),
              getMethod, setMethod, flags, helpMessage)
{
}

SProperty::SProperty
(
    SObject&     object
   ,const char*  name
   ,int8         id
   ,string       initialValue
   ,JSPropertyOp getMethod
   ,JSPropertyOp setMethod
   ,uintN        flags
   ,const char*  helpMessage
) :
    SProperty(&object, name, id, new StringSVal(initialValue),
              getMethod, setMethod, flags, helpMessage)
{
}

SProperty::SProperty
(
    SObject&     object
   ,const char*  name
   ,int8         id
    /* no initial value for object */
   ,JSPropertyOp getMethod
   ,JSPropertyOp setMethod
   ,uintN        flags
   ,const char*  helpMessage
) :
    SProperty(&object, name, id, new ObjectSVal(),
              getMethod, setMethod, flags, helpMessage)
{
}

SProperty::~SProperty()
{
    delete m_pInitialValue;
}

void SProperty::AddToSObject()
{
    if ((m_Flags & MODS_INTERNAL_ONLY) &&
        (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK))
    {
        return;
    }
    m_pSObject->AddProperty(m_Name, this);
}

RC SProperty::DefineProperty
(
    JSContext * pContext,
    JSObject  * pJSObject
)
{
    if ((m_Flags & MODS_INTERNAL_ONLY) &&
        (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK))
    {
        return OK;
    }

    MASSERT(pContext != 0);
    MASSERT(pJSObject != 0);

    RC rc = OK;

    // Check if a property with the same name already exists.
    JSBool found;
    uintN attrs;
    if ((JS_FALSE ==
         JS_GetPropertyAttributes(pContext, pJSObject, m_Name, &attrs, &found))
        || (JS_TRUE == found))
    {
        Printf(Tee::PriHigh,
               "Failed to create %s.%s property because it already exists.\n",
               GetObjectName(), m_Name);
        return RC::COULD_NOT_CREATE_JS_PROPERTY;
    }

    jsval InitialValue;
    CHECK_RC(m_pInitialValue->ToJsval(pContext, &InitialValue));

    if (JS_FALSE == JS_DefinePropertyWithTinyId(pContext, pJSObject, m_Name,
                                                m_Id, InitialValue, m_GetMethod,
                                                m_SetMethod, m_Flags))
    {
        Printf(Tee::PriHigh, "Failed to create property %s.%s.\n",
               GetObjectName(), m_Name);
        return RC::COULD_NOT_CREATE_JS_PROPERTY;
    }

    if (0 != (m_Flags & SPROP_VALID_TEST_ARG))
    {
        // Don't use MODS JavaScript class here, since SProperty::DefineProperty
        // is called during JavaScript construction.
        jsval validTestArgJval;
        JSObject *validTestArgObj = nullptr;
        JS_GetProperty(pContext, pJSObject, s_ValidTestArgs, &validTestArgJval);
        if (JSVAL_VOID == validTestArgJval)
        {
            validTestArgObj = JS_NewObject(pContext, &js_ObjectClass, nullptr, nullptr);
            validTestArgJval = OBJECT_TO_JSVAL(validTestArgObj);
            JS_DefineProperty(pContext, pJSObject, s_ValidTestArgs, validTestArgJval,
                              nullptr, nullptr, 0);
        }
        else
        {
            JS_ValueToObject(pContext, validTestArgJval, &validTestArgObj);
        }

        if (nullptr != validTestArgObj)
        {
            JSString* pJSString = JS_NewStringCopyZ(pContext, GetHelpMessage());
            if (nullptr == pJSString)
            {
                return RC::CANNOT_COLWERT_STRING_TO_JSVAL;
            }

            jsval arbValue = STRING_TO_JSVAL(pJSString);
            JS_SetProperty(pContext, validTestArgObj, m_Name, &arbValue);
        }
    }

    m_pContext = pContext;
    m_pJSObject = pJSObject;

    return OK;
}

const char* SProperty::GetObjectName() const
{
    return m_pSObject ? m_pSObject->GetName() : ENCJSENT("Global");
}

const char* SProperty::GetName() const
{
    return m_Name;
}

const char* SProperty::GetHelpMessage() const
{
    return m_HelpMessage;
}

const UINT32 SProperty::GetFlags() const
{
    return m_Flags;
}

RC SProperty::ToJsval(jsval* pValue) const
{
    MASSERT(pValue != nullptr);

    // If the associated JSObject has private data, ensure that the
    // private data is not NULL before attempting to get the
    // property
    if ((JS_GetClass(m_pJSObject)->flags & JSCLASS_HAS_PRIVATE) &&
        (NULL == JS_GetPrivate(m_pContext, m_pJSObject)))
    {
        return RC::ILWALID_OBJECT_PROPERTY;
    }

    if (!JS_GetProperty(m_pContext, m_pJSObject, m_Name, pValue))
    {
        return RC::ILWALID_OBJECT_PROPERTY;
    }

    // If the property is not defined in the object's scope,
    // or in its prototype links, Value is set to JSVAL_VOID.
    if (JSVAL_VOID == *pValue)
    {
        return RC::ILWALID_OBJECT_PROPERTY;
    }

    return OK;
}

//------------------------------------------------------------------------------
// SMethod
//------------------------------------------------------------------------------

//
// Create a global method.
//
SMethod::SMethod
(
    const char* Name
   ,JSNative    CallFunction
   ,uintN       NumArguments
   ,const char* HelpMessage
   ,uintN       Flags      /* =0 */
) :
    m_pSObject(0)
   ,m_Name(Name)
   ,m_CallFunction(CallFunction)
   ,m_NumArguments(NumArguments)
   ,m_HelpMessage(HelpMessage)
   ,m_Flags(Flags)
{
    MASSERT(m_Name != 0);
    MASSERT(m_CallFunction != 0);
    MASSERT(m_HelpMessage != 0);

    // Register the method for later creation.
    s_pGlobalSMethods->push_back(this);
}

//
// Create an object method.
//
SMethod::SMethod
(
    SObject&    Object
   ,const char* Name
   ,JSNative    CallFunction
   ,uintN       NumArguments
   ,const char* HelpMessage
   ,uintN       Flags    /* =0 */
) :
    m_pSObject(&Object)
   ,m_Name(Name)
   ,m_CallFunction(CallFunction)
   ,m_NumArguments(NumArguments)
   ,m_HelpMessage(HelpMessage)
   ,m_Flags(Flags)
{
    MASSERT(m_pSObject != 0);
    MASSERT(m_Name != 0);
    MASSERT(m_CallFunction != 0);
    MASSERT(m_HelpMessage != 0);

    //
    // Add the method to the list of object methods.
    // We can't call our SObject's AddMethod() yet, in case the SObject
    // is a global in a different file, which might cause link-order dependance.
    //
    s_pNonGlobalSMethods->push_back(this);
}

//
// SMethod destructor.  Mostly defined to avoid compiler warnings
// about SMethod having virtual methods but a non-virtual destructor.
//
SMethod::~SMethod()
{
}

void SMethod::AddToSObject()
{
    if ((m_Flags & MODS_INTERNAL_ONLY) &&
        (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK))
    {
        return;
    }
    m_pSObject->AddMethod(m_Name, this);
}

RC SMethod::DefineMethod
(
    JSContext * pContext,
    JSObject  * pJSObject
)
{
    if ((m_Flags & MODS_INTERNAL_ONLY) &&
        (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK))
    {
        return OK;
    }

    MASSERT(pContext != 0);
    MASSERT(pJSObject != 0);

    // Check if a method with the same name already exists.
    JSBool found;
    uintN attrs;
    if ((JS_FALSE ==
         JS_GetPropertyAttributes(pContext, pJSObject, m_Name, &attrs, &found))
        || (JS_TRUE == found))
    {
        Printf(Tee::PriHigh,
               "Failed to create %s.%s method because it already exists.\n",
               GetObjectName(), m_Name);
        return RC::COULD_NOT_CREATE_JS_METHOD;
    }

    if (NULL == JS_DefineFunction(pContext, pJSObject, m_Name, m_CallFunction,
                                  m_NumArguments, 0 /* flags */ ))
    {
        Printf(Tee::PriHigh, "Failed to create method %s.%s.\n",
               GetObjectName(), m_Name);
        return RC::COULD_NOT_CREATE_JS_METHOD;
    }

    return OK;
}

const char* SMethod::GetObjectName() const
{
    return m_pSObject ? m_pSObject->GetName() : ENCJSENT("Global");
}

const char* SMethod::GetName() const
{
    return m_Name;
}

const char* SMethod::GetHelpMessage() const
{
    return m_HelpMessage;
}

/* virtual */ SMethod::MethodType SMethod::GetMethodType() const
{
    return Function;
}

//------------------------------------------------------------------------------
// STest
//------------------------------------------------------------------------------

//
// Create a global test.
//
STest::STest
(
    const char* Name
   ,JSNative    CallFunction
   ,uintN       NumArguments
   ,const char* HelpMessage
   ,uintN       Flags  /* =0 */
) :
    SMethod(Name, CallFunction, NumArguments, HelpMessage, Flags)
{
}

//
// Create an object method.
//
STest::STest
(
    SObject&    Object
   ,const char* Name
   ,JSNative    CallFunction
   ,uintN       NumArguments
   ,const char* HelpMessage
   ,uintN       Flags        /* =0 */
) :
    SMethod(Object, Name, CallFunction, NumArguments, HelpMessage, Flags)
{
}

/* virtual */ SMethod::MethodType STest::GetMethodType() const
{
    return Test;
}

//------------------------------------------------------------------------------
// SObject
//------------------------------------------------------------------------------

SObject::SObject
(
    const char* Name
   ,JSClass&    Class
   ,SObject*    pPrototype
   ,uintN       Flags
   ,const char* HelpMessage
   ,bool        AddNameProperty    /* = true */
) :
    m_Name(Name)
   ,m_pClass(&Class)
   ,m_pPrototype(pPrototype)
   ,m_Flags(Flags)
   ,m_HelpMessage(HelpMessage)
   ,m_Constructor(0)
   ,m_GlobalInit(0)
   ,m_Properties()
   ,m_Methods()
   ,m_pJSObject(0)
   ,m_pNameProperty(0)
{
    MASSERT(m_Name != 0);
    MASSERT(m_pClass != 0);
    MASSERT(m_HelpMessage != 0);

    // Register the SObject for later creation.
    s_pSObjects->push_back(this);

    // AddNameProperty is OBSOLETE and should always be left default.
    // It used to prevent name clashes in inheritance hierarchies,
    // but is no longer needed since we made overloading work correctly.
    if (AddNameProperty)
    {
        const uintN nameFlags = JSPROP_READONLY |
                                ((m_Flags & MODS_INTERNAL_ONLY) ? MODS_INTERNAL_ONLY : 0);
        // Add a read only property "name" that conatains the name of the object.
        m_pNameProperty =
            //TODO: make modsencsrc recognize SProperty constructor
            new SProperty(*this, ENCJSENT("name"), 0, Name, 0, 0, nameFlags,
                          ENCJSEMPTYSTR("object's name"));
        MASSERT(m_pNameProperty != 0);
    }
}

SObject::SObject
(
    const char* Name
   ,JSClass&    Class
   ,SObject*    pPrototype
   ,uintN       Flags
   ,const char* HelpMessage
   ,JSNative    Constructor
   ,bool        AddNameProperty      /* = true */
) :
    m_Name(Name)
   ,m_pClass(&Class)
   ,m_pPrototype(pPrototype)
   ,m_Flags(Flags)
   ,m_HelpMessage(HelpMessage)
   ,m_Constructor(Constructor)
   ,m_GlobalInit(0)
   ,m_Properties()
   ,m_Methods()
   ,m_pJSObject(0)
   ,m_pNameProperty(0)
{
    MASSERT(m_Name != 0);
    MASSERT(m_pClass != 0);
    MASSERT(m_HelpMessage != 0);
    MASSERT(m_Constructor != 0);

    // See above ctor for detailed comment
    if (AddNameProperty)
    {
        const uintN nameFlags = JSPROP_READONLY |
                                ((m_Flags & MODS_INTERNAL_ONLY) ? MODS_INTERNAL_ONLY : 0);
        m_pNameProperty =
            //TODO: make modsencsrc recognize SProperty constructor
            new SProperty(*this, ENCJSENT("name"), 0, Name, 0, 0, nameFlags,
                          ENCJSEMPTYSTR("object's name"));
    }
    // Register the SObject for later creation.
    s_pSObjects->push_back(this);
}

SObject::SObject
(
    const char* Name
   ,JSClass&    Class
   ,SObject*    pPrototype
   ,uintN       Flags
   ,const char* HelpMessage
   ,JSObjectOp  GlobalInit
   ,bool        AddNameProperty /* = true */
) :
    m_Name(Name)
   ,m_pClass(&Class)
   ,m_pPrototype(pPrototype)
   ,m_Flags(Flags)
   ,m_HelpMessage(HelpMessage)
   ,m_Constructor(0)
   ,m_GlobalInit(GlobalInit)
   ,m_Properties()
   ,m_Methods()
   ,m_pJSObject(0)
   ,m_pNameProperty(0)
{
    MASSERT(m_Name != 0);
    MASSERT(m_pClass != 0);
    MASSERT(m_HelpMessage != 0);
    MASSERT(m_GlobalInit != 0);

    // See above ctor for detailed comment
    if (AddNameProperty)
    {
        const uintN nameFlags = JSPROP_READONLY |
                                ((m_Flags & MODS_INTERNAL_ONLY) ? MODS_INTERNAL_ONLY : 0);
        //TODO: make modsencsrc recognize SProperty constructor
        m_pNameProperty = new SProperty(*this, ENCJSENT("name"), 0, Name, 0, 0,
                                        nameFlags, ENCJSEMPTYSTR("object's name"));
    }
    // Register the SObject for later creation.
    s_pSObjects->push_back(this);
}

SObject::~SObject()
{
    delete m_pNameProperty;
}

// See script.h for comment on usage of this function (be careful)
//------------------------------------------------------------------------------
void SObject::FreeMethodsAndProperties()
{
    for
    (
        SMethodVec::iterator ipSMeth = m_Methods.begin();
        ipSMeth != m_Methods.end();
        ++ipSMeth
    )
    {
        delete (*ipSMeth);
    }
    m_Methods.clear();

    for
    (
        SPropertyVec::iterator ipSProp = m_Properties.begin();
        ipSProp != m_Properties.end();
        ++ipSProp
    )
    {
        delete (*ipSProp);
    }
    m_Properties.clear();
}

void SObject::AddProperty
(
    const char * Name
   ,SProperty  * pProperty
)
{
    MASSERT(Name != 0);
    MASSERT(pProperty != 0);

    m_Properties.push_back(pProperty);
}

void SObject::AddMethod
(
    const char * Name
   ,SMethod    * pMethod
)
{
    MASSERT(Name != 0);
    MASSERT(pMethod != 0);

    m_Methods.push_back(pMethod);
}

RC SObject::DefineObject
(
    JSContext * pContext
   ,JSObject  * pParent
)
{
    if ((m_Flags & MODS_INTERNAL_ONLY) &&
        (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK))
    {
        return OK;
    }

    MASSERT(pContext != 0);
    MASSERT(pParent != 0);

    // Harmlessly exit early if we are double-Defined (due to the out-of-order
    // inheritance hack below).
    if (NULL != m_pJSObject)
    {
        return OK;
    }

    map<JSString *, jsval> validTestArgs;

    JSObject* pPrototype;
    if (m_pPrototype)
    {
        pPrototype = m_pPrototype->GetJSObject();
        if (NULL == pPrototype)
        {
            // We are trying to inherit from another SObject, but that SObject
            // hasn't run DefineObject yet.  Define our parent first.
            RC rc = m_pPrototype->DefineObject(pContext, pParent);
            if (OK != rc)
            {
                return rc;
            }
            pPrototype = m_pPrototype->GetJSObject();
        }

        // Copy instead of inherit ValidTestArgs. Otherwise ValidTestArgs object
        // will be shared by all objects and will contain all possible test
        // arguments for all tests.
        // Don't use MODS JavaScript class here, since SObject::DefineObject is
        // called during JavaScript construction.
        jsval validTestArgsVal;
        JSObject *validTestArgsObj;
        JS_GetProperty(pContext, pPrototype, s_ValidTestArgs, &validTestArgsVal);
        if (JSVAL_VOID != validTestArgsVal)
        {
            JS_ValueToObject(pContext, validTestArgsVal, &validTestArgsObj);
            JSIdArray* propIds = JS_Enumerate(pContext, validTestArgsObj);
            DEFER
            {
                JS_DestroyIdArray(pContext, propIds);
            };
            for (jsint i = 0; propIds->length > i; ++i)
            {
                jsval propVal;
                if (JS_IdToValue(pContext, propIds->vector[i], &propVal) &&
                    JSVAL_IS_STRING(propVal))
                {
                    JSString* propJsStr = JS_ValueToString(pContext, propVal);
                    size_t strLength = JSSTRING_LENGTH(propJsStr);
                    jschar* strChars = JSSTRING_CHARS(propJsStr);
                    string propStr;

                    // it's a design flaw in the JS engine: js_DeflateStringToBuffer has a
                    // different contract when UTF-8 encoding is on
                    #if defined(JS_C_STRINGS_ARE_UTF8)
                        size_t size = 0;
                        js_DeflateStringToBuffer(pContext, strChars, strLength, 0, &size);
                        propStr.resize(size);
                        js_DeflateStringToBuffer(pContext, strChars, strLength, &propStr[0], &size);
                    #else
                        propStr.resize(strLength);
                        size_t size = strLength;
                        js_DeflateStringToBuffer(pContext, strChars, strLength, &propStr[0], &size);
                    #endif
                    jsval propStrVal;
                    JS_GetProperty(pContext, validTestArgsObj, propStr.c_str(), &propStrVal);
                    validTestArgs[propJsStr] = propStrVal;
                }
            }
        }
    }
    else
    {
        pPrototype = NULL;
    }

    if (m_Constructor)
    {
        // We are to create a JS class:
        // Create a global Function object NNN that hooks to m_Constructor.
        // Crate a global Object NNN.prototype to hold the list
        // of methods and properties that all objects will inherit.

        m_pJSObject = JS_InitClass(
            pContext,
            pParent,       // This is the "Global" object.
            pPrototype,    // parent class proto object for inheritance
                           // (will inherit from Object)
            m_pClass,      // JSClass structure
            m_Constructor, // JSNative constructor function
            0,             // ctor num args
            NULL,          // prototype (each instance) property list
            NULL,          // prototype (each instance) method list
            NULL,          // ctor (class i.e. static) property list
            NULL);         // ctor (class i.e. static) method list
    }
    else
    {
        // Check if an object with the same name already exists.
        JSBool found;
        uintN attrs;
        if ((JS_FALSE ==
             JS_GetPropertyAttributes
             (pContext, pParent, m_Name, &attrs, &found)) || (JS_TRUE == found))
        {
            Printf(Tee::PriHigh,
                   "Failed to create %s object because it already exists.\n",
                   m_Name);
            return RC::COULD_NOT_CREATE_JS_OBJECT;
        }
        // Create a simple global object.
        m_pJSObject = JS_DefineObject(pContext, pParent, m_Name, m_pClass,
                                      pPrototype, m_Flags);

        // Perform initialization.
        if (m_pJSObject && m_GlobalInit)
        {
            m_pJSObject = m_GlobalInit(pContext, m_pJSObject);
        }
    }
    if (0 == m_pJSObject)
    {
        Printf(Tee::PriHigh, "Failed to create object %s.\n", m_Name);
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    if (!validTestArgs.empty())
    {
        JSObject *validTestArgObj = JS_NewObject(pContext, &js_ObjectClass, nullptr, nullptr);
        for (auto argPair : validTestArgs)
        {
            JS_SetUCProperty(pContext, validTestArgObj, JSSTRING_CHARS(argPair.first), JSSTRING_LENGTH(argPair.first),
                             &argPair.second);
        }
        jsval validTestArgJval = OBJECT_TO_JSVAL(validTestArgObj);
        JS_SetProperty(pContext, m_pJSObject, s_ValidTestArgs, &validTestArgJval);
    }

    RC rc;

    // Add the .Help() to this JSObject
    JS_DefineFunction(pContext, m_pJSObject, "Help", &C_Global_Help, 1, 0);

    // Create the object's properties.
    for (SPropertyVec::iterator ipProperty = m_Properties.begin();
         ipProperty != m_Properties.end(); ++ipProperty)
    {
        CHECK_RC((*ipProperty)->DefineProperty(pContext, m_pJSObject));
    }

    // Create the object's methods.
    for (SMethodVec::iterator ipMethod = m_Methods.begin();
         ipMethod != m_Methods.end(); ++ipMethod)
    {
        CHECK_RC((*ipMethod)->DefineMethod(pContext, m_pJSObject));
    }

    // Store all methods accessible through prototype chain (useful shortlwt)
    if (m_pPrototype)
    {
        m_InheritedMethods = m_pPrototype->GetInheritedMethods();
    }

    m_InheritedMethods.insert(m_InheritedMethods.end(),
                              m_Methods.begin(), m_Methods.end());
    return OK;
}

JSObject* SObject::GetJSObject() const
{
    return m_pJSObject;
}

const SPropertyVec& SObject::GetProperties() const
{
    return m_Properties;
}

const SMethodVec& SObject::GetMethods() const
{
    return m_Methods;
}

const SMethodVec& SObject::GetInheritedMethods() const
{
    return m_InheritedMethods;
}

const char* SObject::GetHelpMessage() const
{
    return m_HelpMessage;
}

const char* SObject::GetName() const
{
    return m_Name;
}

const char* SObject::GetClassName() const
{
    return m_pClass->name;
}

bool SObject::IsClass() const
{
    return (m_Constructor != 0);
}

bool SObject::IsInstance
(
    JSContext * pContext
   ,JSObject  * pObject
) const
{
    return (JS_InstanceOf(pContext, pObject, m_pClass, NULL) != 0);
}

bool SObject::IsPrototypeOf
(
    JSContext * pContext
   ,JSObject  * pObject
) const
{
    JSObject * pLwrObject = pObject;
    while (pLwrObject)
    {
        if (IsInstance(pContext, pLwrObject))
            return true;
        pLwrObject = JS_GetPrototype(pContext, pLwrObject);
    }
    return false;
}

//------------------------------------------------------------------------------
// Get registered properties, methods, and objects.
//------------------------------------------------------------------------------

SPropertyVec& GetGlobalSProperties()
{
    return *s_pGlobalSProperties;
}

SMethodVec& GetGlobalSMethods()
{
    return *s_pGlobalSMethods;
}

SObjectVec& GetSObjects()
{
    return *s_pSObjects;
}

SPropertyVec& GetNonGlobalSProperties()
{
    return *s_pNonGlobalSProperties;
}

SMethodVec& GetNonGlobalSMethods()
{
    return *s_pNonGlobalSMethods;
}

//
// Nifty counter to initialize the containers before they are used.
//

static INT32 s_RefCount = 0;

ContainerInit::ContainerInit()
{
    if (0 == s_RefCount++)
    {
        s_pGlobalSProperties = new SPropertyVec;
        MASSERT(s_pGlobalSProperties != 0);

        s_pGlobalSMethods = new SMethodVec;
        MASSERT(s_pGlobalSMethods != 0);

        s_pSObjects = new SObjectVec;
        MASSERT(s_pSObjects != 0);

        s_pNonGlobalSProperties = new SPropertyVec;
        MASSERT(s_pNonGlobalSProperties);

        s_pNonGlobalSMethods = new SMethodVec;
        MASSERT(s_pNonGlobalSMethods);
    }
}

ContainerInit::~ContainerInit()
{
    if (0 == --s_RefCount)
    {
        delete s_pSObjects;
        delete s_pGlobalSMethods;
        delete s_pGlobalSProperties;
        delete s_pNonGlobalSProperties;
        delete s_pNonGlobalSMethods;
    }
}

