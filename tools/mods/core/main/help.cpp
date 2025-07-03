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

// Online help.

#include "core/include/filesink.h"
#include "core/include/help.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/xp.h"
#include "jsdbgapi.h"
#include <string>
#include <string.h>

RC ObjectHelpHelper(JSObject   * pObject,
                    JSContext  * pContext,
                    regex      * pRegex,
                    UINT32       IndentLevel,
                    const char * pObjectName,
                    bool       * pbObjectFound,
                    bool         bPropertiesOnly);

RC Help::JsObjHelp
(
    JSObject   * pObject,
    JSContext  * pContext,
    regex      * pRegex,
    UINT32       IndentLevel,
    const char * pObjectName,
    bool       * pbObjectFound
)
{
    return ObjectHelpHelper(pObject,pContext,pRegex,IndentLevel,pObjectName,pbObjectFound,false);
}

RC Help::JsPropertyHelp
(
    JSObject   * pObject,
    JSContext  * pContext,
    regex      * pRegex,
    UINT32       IndentLevel
)
{
    return ObjectHelpHelper(pObject,pContext,pRegex,IndentLevel,nullptr,nullptr,true);
}

//! \brief Prints a common format about a specific property
//!
//! \param property - C++ Description of the property
//! \param value - jsval representation of the property
static void PrintPropertyHelper
(
   const SProperty & property,
   const jsval value,
   const UINT32 indentLevel
)
{
    for (UINT32 ii = 0; ii < indentLevel; ii++)
        Printf(Tee::PriNormal, "    ");

    string printVal;
    if (JSVAL_IS_OBJECT(value))
    {
        printVal = "object";
    }
    else
    {
        JavaScriptPtr()->FromJsval(value, &printVal);
    }

    string hexadecimalVal;
    if (JSVAL_IS_NUMBER(value))
    {
        FLOAT64 floatVal;
        JavaScriptPtr()->FromJsval(value, &floatVal);
        INT64 intVal = static_cast<INT64>(floatVal);
        if (intVal == floatVal && intVal >= 0)
        {
            hexadecimalVal = Utility::StrPrintf(" (0x%llx)", intVal);
        }
    }

    Printf(Tee::PriNormal, "    %s = %s%s : %s\n" ,
           property.GetName(), printVal.c_str(),
           hexadecimalVal.c_str(), property.GetHelpMessage());
}

//! \brief Prints the information of a given property of a dynamic
//!        or non global SObject
//!
//! \param pObject - The JS Object this property is contained in
//! \param pContext - The JS Context we are running in
//! \param Property - C++ Description of the property
static void PrintDynamicPropertyHelp
(
    JSObject * pObject,
    JSContext * pContext,
    const SProperty & property,
    UINT32 indentLevel
)
{
    jsval value;
    if (!JS_GetProperty(pContext, pObject, property.GetName(), &value))
    {
        value = JSVAL_VOID;
    }
    PrintPropertyHelper(property, value, indentLevel);
}

//! \brief Prints the information of a given property of the global object
//!
//! \param Property - C++ Description of the property
static void PrintGlobalPropertyHelp(const SProperty & property)
{
    jsval value;
    if (OK != property.ToJsval(&value))
        value = JSVAL_VOID;
    PrintPropertyHelper(property, value, 0);
}

static void PrintMethodHelp
(
   const SMethod & Method,
   UINT32          IndentLevel
)
{
   for (UINT32 i = 0; i < IndentLevel; i++)
       Printf(Tee::PriNormal, "    ");
   Printf(Tee::PriNormal, "    %s : %s\n",
          Method.GetName(), Method.GetHelpMessage());
}

static RC PrintJsval
(
   JSContext *  pContext,
   jsval        Val,
   string       Id,
   string       Property
)
{
   MASSERT(pContext != 0);

   static INT32 Level = 1;

   RC rc = OK;
   INT32 i;
   JavaScriptPtr pJavaScript;

   switch (JS_TypeOfValue(pContext, Val))
   {
      case JSTYPE_VOID:
      {
         for (i = 0; i < Level; i++)
            Printf(Tee::PriNormal, "    ");

         Printf(Tee::PriNormal, "%s = void\n", Id.c_str());
         break;
      }

      case JSTYPE_OBJECT:
      {
         // Enumerate object.
         JSObject * pObject = 0;
         if
         (
               !JS_ValueToObject(pContext, Val, &pObject)
            || (0 == pObject)
         )
         {
            for (i = 0; i < Level; i++)
                Printf(Tee::PriNormal, "    ");
            Printf(Tee::PriNormal, "%s = %s\n", Id.c_str(), Property.c_str());
            break;
         }

         bool bObjectFound;
         
         regex compiledRegExp = regex(".*",
            regex_constants::extended | regex_constants::nosubs | regex_constants::icase);

         CHECK_RC(Help::JsObjHelp(pObject,         pContext,
                                   &compiledRegExp, Level,
                                   Id.c_str(),      &bObjectFound));

         if (!bObjectFound)
         {
             for (i = 0; i < Level; i++)
                 Printf(Tee::PriNormal, "    ");
             Printf(Tee::PriNormal, "%s : object\n", Id.c_str());
         }
         ++Level;

         JSPropertiesEnumerator jsPropEnum(pContext, pObject);
         JSIdArray * pProperties = jsPropEnum.GetProperties();
         if (0 == pProperties)
         {
            if (!bObjectFound)
            {
                for (i = 0; i < Level; i++)
                    Printf(Tee::PriNormal, "    ");
                Printf(Tee::PriNormal, "%s = %s\n", Id.c_str(), Property.c_str());
            }
            --Level;
            break;
         }

         for (i = 0; i < pProperties->length; ++i)
         {
            jsval Val;
            if (!JS_IdToValue(pContext, pProperties->vector[i], &Val))
               return RC::CANNOT_GET_ELEMENT;

            string Id;
            if (OK != (rc = pJavaScript->FromJsval(Val, &Id)))
               return rc;

            if (!JS_GetProperty(pContext, pObject, Id.c_str(), &Val))
               return RC::CANNOT_GET_ELEMENT;

            string Property;
            if (OK != (rc = pJavaScript->FromJsval(Val, &Property)))
               return rc;

            if (OK != (rc = PrintJsval(pContext, Val, Id, Property)))
               return rc;
         }
         --Level;
         break;
      }

      case JSTYPE_STRING:
      case JSTYPE_NUMBER:
      case JSTYPE_BOOLEAN:
      {
         for (i = 0; i < Level; i++)
            Printf(Tee::PriNormal, "    ");

         Printf(Tee::PriNormal, "%s = %s", Id.c_str(), Property.c_str());

         if (JSVAL_IS_INT(Val))
         {
            UINT32 UintVal;
            if (OK == pJavaScript->FromJsval(Val, &UintVal))
               Printf(Tee::PriNormal, "(0x%8x)", UintVal);
         }
         Printf(Tee::PriNormal, "\n");
         break;
      }

      case JSTYPE_FUNCTION:
      {
         JSFunction * pFunction = JS_ValueToFunction(pContext, Val);
         if (0 == pFunction)
            return RC::CANNOT_COLWERT_JSVAL_TO_FUNCTION;

         JSScript * pScript = JS_GetFunctionScript(pContext, pFunction);
         if (0 == pScript)
         {
             for (i = 0; i < Level; i++)
                Printf(Tee::PriNormal, "    ");
             Printf(Tee::PriNormal, " %s : native function\n", Id.c_str());
         }
         else
         {
             if (JS_GetScriptFilename(pContext, pScript))
             {
                for (i = 0; i < Level; i++)
                   Printf(Tee::PriNormal, "    ");

                Printf(Tee::PriNormal, "%s : %s:%u\n", Id.c_str() ,
                   JS_GetScriptFilename(pContext, pScript),
                   JS_GetScriptBaseLineNumber(pContext, pScript));
             }
             else
             {
                for (i = 0; i < Level; i++)
                   Printf(Tee::PriNormal, "    ");
                Printf(Tee::PriNormal, " %s : script function\n", Id.c_str());
             }
         }
         break;
      }

      default:
         MASSERT(false);
         return RC::SOFTWARE_ERROR;
   }

   return OK;
}

//! \brief Print information about all properties and methods of a
//!        non-global or dynamic JS object that match the given
//!        regular expression.
//!
//! \note Since there is not a specific SObject for a dynamically created
//!       JSObject, the Class name is matched to the global SObject
//!       that isn't tied to a specific JS Object.
RC ObjectHelpHelper
(
   JSObject   * pObject,
   JSContext  * pContext,
   regex      * pRegex,
   UINT32       IndentLevel,
   const char * pObjectName,
   bool       * pbObjectFound,
   bool         bPropertiesOnly
)
{
   JSClass * pClass = JS_GetClass(pObject);

   if(pbObjectFound)
        *pbObjectFound = false;

   // Objects, methods, and properties.
   const SObjectVec & Objects = GetSObjects();
   for (SObjectVec::const_iterator ipObject = Objects.begin();
                                   ipObject != Objects.end();
                                   ++ipObject)
   {
      // Check to see if this SObject class matches the JS Object class
      if (strcmp(pClass->name, (*ipObject)->GetClassName()) == 0)
      {
         if(pbObjectFound)
            *pbObjectFound = true;

         for (UINT32 i = 0; i < IndentLevel; i++)
            Printf(Tee::PriNormal, "    ");

         if (pObjectName != NULL)
         {
             Printf(Tee::PriNormal, "%s : %s object : %s\n",
                    pObjectName, pClass->name, (*ipObject)->GetHelpMessage());
         }
         else
         {
             Printf(Tee::PriNormal, "%s : %s\n",
                    (*ipObject)->GetName(), (*ipObject)->GetHelpMessage());
         }

        // Print help and value for the object's properties.
        const SPropertyVec & Properties = (*ipObject)->GetProperties();

        for (SPropertyVec::const_iterator ipProperty = Properties.begin();
                                          ipProperty != Properties.end();
                                          ++ipProperty)
        {
           if(regex_search((*ipProperty)->GetName(), *pRegex))
           {
              PrintDynamicPropertyHelp(pObject, pContext, (**ipProperty), IndentLevel);
           }
        }

        if (!bPropertiesOnly)
        {

             // Print help for the object's methods.
             const SMethodVec & Methods = (*ipObject)->GetInheritedMethods();
             for (SMethodVec::const_iterator ipMethod = Methods.begin();
                                             ipMethod != Methods.end();
                                             ++ipMethod)
             {
                if(regex_search((*ipMethod)->GetName(), *pRegex))
                {
                   PrintMethodHelp(**ipMethod, IndentLevel);
                }
             }
        }
        break;
      }
   }

   return OK;
}

//! \brief Print information about all properties and methods of any
//!        non-dynamic SObject or global object there is.
static RC GlobalHelpHelper(JSObject * pObject, JSContext * pContext, regex *pRegex)
{
   RC rc = OK;
   JavaScriptPtr pJavaScript;

   // Script properties and methods.
   JSPropertiesEnumerator jsPropEnum(pContext, pObject);
   JSIdArray * pScriptProperties = jsPropEnum.GetProperties();
   if (0 == pScriptProperties)
      return RC::CANNOT_ENUMERATE_OBJECT;

   bool GlobalPrinted = false;

   for (INT32 i = 0; i < pScriptProperties->length; ++i)
   {
      jsval Val;
      if (!JS_IdToValue(pContext, pScriptProperties->vector[i], &Val))
         return RC::CANNOT_GET_ELEMENT;

      string Id;
      if (OK != (rc = pJavaScript->FromJsval(Val, &Id)))
         return rc;

      if(regex_search(Id, *pRegex))
      {
          string Property;

         if (!GlobalPrinted)
         {
            Printf(Tee::PriNormal, "Global : script\n");
            GlobalPrinted = true;
         }

         if (!JS_GetProperty(pContext, pObject, Id.c_str(), &Val))
            return RC::CANNOT_GET_ELEMENT;

         if (OK != (rc = pJavaScript->FromJsval(Val, &Property)))
            return rc;

         if (OK != (rc = PrintJsval(pContext, Val, Id, Property)))
            return rc;
      }
   }

   // Global properties.
   GlobalPrinted = false;
   const SPropertyVec & GlobalProperties = GetGlobalSProperties();
   SPropertyVec::const_iterator ipProperty;
   for (ipProperty = GlobalProperties.begin(); ipProperty != GlobalProperties.end(); ++ipProperty)
   {
      if(regex_search((*ipProperty)->GetName(), *pRegex))
      {
         if (!GlobalPrinted)
         {
            Printf(Tee::PriNormal, "Global : object\n");
            GlobalPrinted = true;
         }
         PrintGlobalPropertyHelp(**ipProperty);
      }
   }

   // Global methods.
   const SMethodVec & GlobalMethods = GetGlobalSMethods();
   SMethodVec::const_iterator ipMethod;
   for (ipMethod = GlobalMethods.begin(); ipMethod != GlobalMethods.end(); ++ipMethod)
   {
      if(regex_search((*ipMethod)->GetName(), *pRegex))
      {
         if (!GlobalPrinted)
         {
            Printf(Tee::PriNormal, "Global : object\n");
            GlobalPrinted = true;
         }
         PrintMethodHelp(**ipMethod, 0);
      }
   }

   // Objects, methods, and properties.
   const SObjectVec & Objects = GetSObjects();
   SObjectVec::const_iterator ipObject;
   for (ipObject = Objects.begin(); ipObject != Objects.end(); ++ipObject)
   {
      // If the object's name matches the regular expression, print the
      // help message for all its properties and methods.
      if(regex_search((*ipObject)->GetName(), *pRegex))
      {
         Printf(Tee::PriNormal, "%s : %s\n", (*ipObject)->GetName(), (*ipObject)->GetHelpMessage());

         // Print help and value for the object's properties.
         const SPropertyVec & Properties = (*ipObject)->GetProperties();
         for (ipProperty = Properties.begin(); ipProperty != Properties.end(); ++ipProperty)
         {
            PrintGlobalPropertyHelp(**ipProperty);
         }

         // Print help for the object's methods.
         const SMethodVec & Methods = (*ipObject)->GetInheritedMethods();
         for (ipMethod = Methods.begin(); ipMethod != Methods.end(); ++ipMethod)
         {
            PrintMethodHelp(**ipMethod, 0);
         }
      }
      // If the object's name did not match the regular expression, print the
      // help message for any properties and methods that match match the
      // regular expression.
      else
      {
         bool ObjectNamePrinted = false;

         // Check properties.
         const SPropertyVec & Properties = (*ipObject)->GetProperties();
         for (ipProperty = Properties.begin(); ipProperty != Properties.end(); ++ipProperty)
         {
            if(regex_search((*ipProperty)->GetName(), *pRegex))
            {
               if (!ObjectNamePrinted)
               {
                  Printf(Tee::PriNormal, "%s : %s\n", (*ipObject)->GetName(), (*ipObject)->GetHelpMessage());
                  ObjectNamePrinted = true;
               }
               PrintGlobalPropertyHelp(**ipProperty);
            }
         }

         // Check Methods.
         const SMethodVec & Methods = (*ipObject)->GetInheritedMethods();
         for (ipMethod = Methods.begin(); ipMethod != Methods.end(); ++ipMethod)
         {
            if(regex_search((*ipMethod)->GetName(), *pRegex))
            {
               if (!ObjectNamePrinted)
               {
                  Printf(Tee::PriNormal, "%s : %s\n", (*ipObject)->GetName(), (*ipObject)->GetHelpMessage());
                  ObjectNamePrinted = true;
               }
               PrintMethodHelp(**ipMethod, 0);
            }
         }
      }
   } // For each object.

   return OK;
}

// STest
JSBool C_Global_Help
(
      JSContext *    pContext,
      JSObject  *    pObject,
      uintN          NumArguments,
      jsval     *    pArguments,
      jsval     *    pReturlwalue
)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check the arguments.
   JavaScriptPtr pJavaScript;
   RC rc;

   string regExp;
   if
   (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &regExp))
   )
   {
      JS_ReportError(pContext, "Usage: Help(\"regular expression\")");
      return JS_FALSE;
   }

   // Compile the regular expression.
   regex compiledRegExp;
   try
   {
      compiledRegExp = regex(regExp,
         regex_constants::extended | regex_constants::nosubs | regex_constants::icase);
   }
   catch (const std::regex_error& e)
   {
      Printf(Tee::PriError, "Bad regular expression '%s': %s\n",
         regExp.c_str(), e.what());
      return RC::BAD_HELP_STRING;
   }

   // Check to see if we are running on the global JSObject or not
   if (JS_GetGlobalObject(pContext) == pObject)
      rc = GlobalHelpHelper(pObject, pContext, &compiledRegExp);
   else // A non-global JS Object
   {
       bool bObjectFound = false;
       rc = Help::JsObjHelp(pObject, pContext, &compiledRegExp, 0, NULL, &bObjectFound);
   }

   RETURN_RC(rc);
}

static STest Global_Help
(
   "Help",
   C_Global_Help,
   1,
   "Print help message."
);

// Helpers for DumpJSHTML
typedef RC(*WritePageFunc)(JSContext *pContext, vector<string> *fileSinks);
#define DEFAULT_FOOTER  \
"Automatically generated from MODS source<br>" \
"Email <a href=\"mailto:bmartinusen@lwida.com?subject=Comments on MODS JS docs\">" \
"Bill Martinusen</a> with comments"

// Flow
static RC DumpAllJSObjectsToHTML(JSContext *pContext);
static RC PushFileSinkAndSwitchTo(vector<string> *fileSinks, const char *next);
static RC PopFileSink(vector<string> *fileSinks);

static RC WriteSubPage
(
    JSContext *pContext,
    vector<string> *fileSinks,
    const char *name,
    const char *link,
    WritePageFunc wpFunc
);

static RC WriteObjectSubPage
(
    JSContext *pContext,
    vector<string> *fileSinks,
    SObjectVec::const_iterator ipObject
);

// Write Page Functions
static RC WriteTopPage(JSContext *pContext, vector<string> *fileSinks);
static RC WriteScriptPropMethPage(JSContext *pContext, vector<string> *fileSinks);
static RC WriteGlobalPropertiesPage(JSContext *pContext, vector<string> *fileSinks);
static RC WriteGlobalMethodsPage(JSContext *pContext, vector<string> *fileSinks);
static RC WriteGlobalObjectsPage(JSContext *pContext, vector<string> *fileSinks);

// HTML
static RC OutputHead(const char *title);
static RC OutputTail(const char *footer);
static RC WriteLink(const char *name, const char *page);

C_(Global_DumpJSHTML);
static STest Global_DumpJSHTML
(
   "DumpJSHTML",
   C_Global_DumpJSHTML,
   0,
   "Dump ALL JS information to HTML web pages"
);

// STest
C_(Global_DumpJSHTML)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if(NumArguments != 0)
   {
       JS_ReportError(pContext, "Usage: DumpJSHTML()");
       return JS_FALSE;
   }

   /*
   Printf(Tee::PriNormal, "WARNING: DumpJSHTML takes a long time to run, are you sure you want to proceed?\n");
   Xp::VirtualKey response = Xp::GetKey();

   if(response != Xp::VKC_LOWER_Y && response != Xp::VKC_CAPITAL_Y)
   {
       Printf(Tee::PriNormal, "Cancelled DumpJSHTML()\n");
       RETURN_RC(OK);
   }
   */

   RETURN_RC(DumpAllJSObjectsToHTML(pContext));
}

static RC DumpAllJSObjectsToHTML(JSContext *pContext)
{
    RC rc;
    vector<string> fileSinks; // stack to keep track of file sinks

    CHECK_RC(PushFileSinkAndSwitchTo(&fileSinks, "index.html"));
    CHECK_RC(OutputHead("Top Page"));
    CHECK_RC(WriteTopPage(pContext, &fileSinks));
    CHECK_RC(OutputTail(DEFAULT_FOOTER));
    CHECK_RC(PopFileSink(&fileSinks));

    Printf(Tee::PriNormal, "Finished HTML\n");
    return OK;
}

static RC WriteTopPage(JSContext *pContext, vector<string> *fileSinks)
{
    RC rc;

    Printf(Tee::PriNormal, "<UL>\n");

    CHECK_RC( WriteSubPage(pContext, fileSinks,
        "Global script properties and methods",
        "script_properties_and_methods.html",
        WriteScriptPropMethPage
    ));

    CHECK_RC( WriteSubPage(pContext, fileSinks,
        "Global Properties",
        "global_properties.html",
        WriteGlobalPropertiesPage
    ));

    CHECK_RC( WriteSubPage(pContext, fileSinks,
        "Global Methods",
        "global_methods.html",
        WriteGlobalMethodsPage
    ));

    CHECK_RC( WriteSubPage(pContext, fileSinks,
        "Global Objects",
        "global_objects.html",
        WriteGlobalObjectsPage
    ));

    Printf(Tee::PriNormal, "</UL>\n");

    return rc;
}

static RC WriteScriptPropMethPage(JSContext *pContext, vector<string> *fileSinks)
{
    RC rc;
    JavaScriptPtr pJs;

    // Script properties and methods.
    JSObject  * pGlobalObject     = JS_GetGlobalObject(pContext);
    JSPropertiesEnumerator jsPropEnum(pContext, pGlobalObject);
    JSIdArray * pScriptProperties = jsPropEnum.GetProperties();
    if (0 == pScriptProperties)
       return RC::CANNOT_ENUMERATE_OBJECT;

    bool GlobalPrinted = false;

    for (INT32 i = 0; i < pScriptProperties->length; ++i)
    {
        jsval Val;
        if (!JS_IdToValue(pContext, pScriptProperties->vector[i], &Val))
            return RC::CANNOT_GET_ELEMENT;

        string Id;
        if (OK != (rc = pJs->FromJsval(Val, &Id)))
            return rc;

        if (!GlobalPrinted)
        {
            Printf(Tee::PriNormal, "Global : script\n");
            GlobalPrinted = true;
        }

        if (!JS_GetProperty(pContext, pGlobalObject, Id.c_str(), &Val))
            return RC::CANNOT_GET_ELEMENT;

        string Property;
        if (OK != (rc = pJs->FromJsval(Val, &Property)))
            return rc;

        if (OK != (rc = PrintJsval(pContext, Val, Id, Property)))
        {
            if (rc == RC::CANNOT_ENUMERATE_OBJECT)
            {
                Printf(Tee::PriNormal, "      *** COULDN'T ENUMERATE ***\n");
                rc = OK;
            }
            else
            {
                return rc;
            }
        }
    }

    return rc;
}

static RC WriteGlobalPropertiesPage(JSContext *pContext, vector<string> *fileSinks)
{
    RC rc;

    // Global properties.
    bool GlobalPrinted = false;
    const SPropertyVec & GlobalProperties = GetGlobalSProperties();
    SPropertyVec::const_iterator ipProperty;
    for (ipProperty = GlobalProperties.begin(); ipProperty != GlobalProperties.end(); ++ipProperty)
    {
        if (!GlobalPrinted)
        {
            Printf(Tee::PriNormal, "Global : object\n");
            GlobalPrinted = true;
        }
        PrintGlobalPropertyHelp(**ipProperty);
    }

    return rc;
}

static RC WriteGlobalMethodsPage(JSContext *pContext, vector<string> *fileSinks)
{
    RC rc;

    // Global methods.
    bool GlobalPrinted = false;
    const SMethodVec & GlobalMethods = GetGlobalSMethods();
    SMethodVec::const_iterator ipMethod;
    for (ipMethod = GlobalMethods.begin(); ipMethod != GlobalMethods.end(); ++ipMethod)
    {
        if (!GlobalPrinted)
        {
            Printf(Tee::PriNormal, "Global : object\n");
            GlobalPrinted = true;
        }
        PrintMethodHelp(**ipMethod, 0);
    }

    return rc;
}

static RC WriteGlobalObjectsPage(JSContext *pContext, vector<string> *fileSinks)
{
    RC rc;

    Printf(Tee::PriNormal, "<UL>\n");

    // Objects, methods, and properties.
    const SObjectVec & Objects = GetSObjects();
    SObjectVec::const_iterator ipObject;
    for (ipObject = Objects.begin(); ipObject != Objects.end(); ++ipObject)
    {
        // Print the help message for all its properties and methods.
        CHECK_RC( WriteObjectSubPage(pContext, fileSinks, ipObject));
    }

    Printf(Tee::PriNormal, "</UL>\n");

    return rc;
}

static RC PushFileSinkAndSwitchTo(vector<string> *fileSinks, const char *next)
{
    FileSink *pFileSink = Tee::GetFileSink();
    string lwrLog = pFileSink->GetFileName();

    fileSinks->push_back(lwrLog);
    return pFileSink->Open(next, FileSink::Truncate);
}

static RC PopFileSink(vector<string> *fileSinks)
{
    RC rc;
    FileSink *pFileSink = Tee::GetFileSink();
    string poppedLog = fileSinks->back();

    CHECK_RC(pFileSink->Open(poppedLog, FileSink::Append));

    fileSinks->pop_back();

    return rc;
}

static RC WriteSubPage
(
    JSContext *pContext,
    vector<string> *fileSinks,
    const char *name,
    const char *link,
    WritePageFunc wpFunc
)
{
    RC rc;

    CHECK_RC(WriteLink(name, link));
    CHECK_RC(PushFileSinkAndSwitchTo(fileSinks, link));

    CHECK_RC(OutputHead(name));
    CHECK_RC( wpFunc(pContext, fileSinks) );
    CHECK_RC(OutputTail(DEFAULT_FOOTER));

    CHECK_RC(PopFileSink(fileSinks));

    return rc;
}

static RC WriteObjectSubPage
(
    JSContext *pContext,
    vector<string> *fileSinks,
    SObjectVec::const_iterator ipObject
)
{
    RC rc;

    string name((*ipObject)->GetName());
    name += " Object";

    string link((*ipObject)->GetName());
    link += "_object.html";

    CHECK_RC(WriteLink(name.c_str(), link.c_str()));
    CHECK_RC(PushFileSinkAndSwitchTo(fileSinks, link.c_str()));

    CHECK_RC(OutputHead(name.c_str()));

    SPropertyVec::const_iterator ipProperty;
    SMethodVec::const_iterator ipMethod;

    Printf(Tee::PriNormal, "%s : %s\n", (*ipObject)->GetName(), (*ipObject)->GetHelpMessage());

    // Print help and value for the object's properties.
    const SPropertyVec & Properties = (*ipObject)->GetProperties();
    for (ipProperty = Properties.begin(); ipProperty != Properties.end(); ++ipProperty)
    {
        PrintGlobalPropertyHelp(**ipProperty);
    }

    // Print help for the object's methods.
    const SMethodVec & Methods = (*ipObject)->GetInheritedMethods();
    for (ipMethod = Methods.begin(); ipMethod != Methods.end(); ++ipMethod)
    {
        PrintMethodHelp(**ipMethod, 0);
    }

    CHECK_RC(OutputTail(DEFAULT_FOOTER));
    CHECK_RC(PopFileSink(fileSinks));

    return rc;
}

static RC OutputHead(const char *title)
{
    Printf(Tee::PriNormal, "<HTML>\n");
    Printf(Tee::PriNormal, "<HEAD>\n");
    Printf(Tee::PriNormal, "<TITLE>%s</TITLE>\n", title);
    Printf(Tee::PriNormal, "</HEAD>\n");
    Printf(Tee::PriNormal, "<BODY>\n");
    Printf(Tee::PriNormal, "<PRE>\n");

    return OK;
}

static RC OutputTail(const char *footer)
{
    Printf(Tee::PriNormal, "<HR>\n");
    Printf(Tee::PriNormal, "<br>%s<br>\n", footer);
    Printf(Tee::PriNormal, "</PRE>\n");
    Printf(Tee::PriNormal, "</BODY>\n");
    Printf(Tee::PriNormal, "</HTML>\n");

    return OK;
}

static RC WriteLink(const char *name, const char *page)
{
    Printf(Tee::PriNormal, "<LI><a href=\"%s\">%s</a></LI>\n", page, name);

    return OK;
}
