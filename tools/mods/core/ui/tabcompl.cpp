/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2013-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Tab completion for MODS interactive interface

#include "core/include/tabcompl.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "jsapi.h"

#include<algorithm>
#include<string>
#include<vector>
#include <ctype.h>

#define PADDING 2

// Parse JS call path (identifiers separated by dots), starting from end of
// Input. Result is never empty. Path elements are returned in reverse order
// and excessive characters are dropped, i.e. :
// "HPrint(CheetAh.RegRd" -> ["RegRd", "CheetAh"]
static void ParsePath(string Input, vector<string>* pResult)
{
    pResult->clear();

    for (INT32 i = static_cast<INT32>(Input.size()) - 1; i >= 0; --i)
    {
        if (isalnum(Input[i]) || '_' == Input[i] || '$' == Input[i])
            continue;

        pResult->push_back(Input.substr(i + 1, Input.size()));

        if (Input[i] == '.')
        {
            Input.erase(i);
        }
        else
        {
            return;
        }
    }

    pResult->push_back(Input);
}

// Find longest common prefix of set of strings.
static string CommonPrefix(const vector<string>& Words)
{
    if (Words.empty()) return "";

    size_t common = Words[0].size(); //number of common characters

    for (vector<string>::const_iterator ipWord = Words.begin();
            ipWord != Words.end(); ++ipWord)
    {
        for (UINT32 chr = 0; chr < ipWord->size() && chr < common; ++chr)
        {
            if (Words[0][chr] != (*ipWord)[chr])
                common = chr;
        }
    }

    return Words[0].substr(0, common);
}

// Check properties of either one object or global scope.
static void CompleteNativeNames(const string& Pattern,
                        const SObjectVec& objects, const SMethodVec& methods,
                        const SPropertyVec& props, vector<string>* pResult)
{
    for (UINT32 i = 0; i < props.size(); ++i)
    {
        const char* name = props[i]->GetName();
        if (!strncmp(name, Pattern.c_str(), Pattern.size()))
                pResult->push_back(name);
    }

    for (UINT32 i = 0; i < methods.size(); ++i)
    {
        const char* name = methods[i]->GetName();
        if (!strncmp(name, Pattern.c_str(), Pattern.size()))
                pResult->push_back(string(name) + '(');
    }

    for (UINT32 i = 0; i < objects.size(); ++i)
    {
        const char* name = objects[i]->GetName();
        if (!strncmp(name, Pattern.c_str(), Pattern.size()))
                pResult->push_back(string(name) +
                                   (objects[i]->IsClass() ? '(' : '.'));
    }
}

// For given JS object, check native properties accessible from it.
static void InstanceComplete(JSContext* pContext, JSObject* pObject,
                             const string& Pattern, vector<string>* pResult)
{
    // Introspect SObjects to find JSClass associated with *pObject.
    for (SObjectVec::const_iterator ipObject = GetSObjects().begin();
                                    ipObject != GetSObjects().end(); ++ipObject)
    {
        if ((*ipObject)->IsInstance(pContext, pObject))
            CompleteNativeNames(Pattern, SObjectVec(),
              (*ipObject)->GetMethods(), (*ipObject)->GetProperties(), pResult);
    }
}

// Resolve JS instance, then check its properties (also user-defined).
static void CompleteJSNames(const vector<string>& Path, vector<string>* pResult)
{
    JSContext* pContext = NULL;
    JavaScriptPtr()->GetContext(&pContext);
    if (!pContext) return;

    JSObject* pObject = JS_GetGlobalObject(pContext);
    if (!pObject) return;

    // Go down the object tree according to Path (excluding Path[0]).
    for (size_t index = Path.size() - 1; index > 0; --index)
    {
        jsval Next;
        if (!JS_GetProperty(pContext, pObject, Path[index].c_str(), &Next))
            return;

        if (!JSVAL_IS_OBJECT(Next) || (Next == JSVAL_NULL))
            return;

        pObject = JSVAL_TO_OBJECT(Next);
    }

    while (pObject)
    {
        InstanceComplete(pContext, pObject, Path[0], pResult);

        JSPropertiesEnumerator jsPropEnum(pContext, pObject);
        JSIdArray* pProps = jsPropEnum.GetProperties();
        if (!pProps) return;

        for (INT32 i = 0; i < pProps->length; ++i)
        {
            jsval idVal;
            if (!JS_IdToValue(pContext, pProps->vector[i], &idVal))
                return;

            string id;
            if (OK != JavaScriptPtr()->FromJsval(idVal, &id))
                return;

            if (!strncmp(id.c_str(), Path[0].c_str(), Path[0].size()))
            {
                // We will print out this variable. Check its type to put
                // dot (for class) or bracket (for function) after its name.
                jsval MemberVal;
                if (!JS_GetProperty(pContext, pObject, id.c_str(), &MemberVal))
                    return;
                // JS_ObjectIsFunction is only defined in newer JSAPI
                if (JSVAL_IS_OBJECT(MemberVal) && (JSVAL_NULL != MemberVal))
                {
                    if (JS_ObjectIsFunction(pContext, JSVAL_TO_OBJECT(MemberVal)))
                        id += '(';
                    else
                        id += '.';
                }
                pResult->push_back(id);
            }
        }
        // Go up the prototype chain, since JSPropertiesEnumerator lists
        // only own properties.
        pObject = JS_GetPrototype(pContext, pObject);
    }
}

// HACK: This method utilizes three different ways of name resolution.
// This is necessary because MODS names are not enumerable
// (flag JSPROP_ENUMERATE is not passed on SObject framework classes creation).
// If enumeration was fixed, CompleteJSNames would be sufficient.
string TabCompletion::Complete(const string& Input, vector<string>* pResult)
{
    pResult->clear();

    vector<string> path;
    ParsePath(Input, &path);

    if (path.size() == 1) // check global names
        CompleteNativeNames(path[0], GetSObjects(), GetGlobalSMethods(),
                                               GetGlobalSProperties(), pResult);

    CompleteJSNames(path, pResult);

    sort(pResult->begin(), pResult->end());
    pResult->erase(unique(pResult->begin(), pResult->end()), pResult->end());

    return pResult->empty() ? "" :
        CommonPrefix(*pResult).substr(path[0].size());
}

void TabCompletion::Format(const vector<string>& Words, UINT32 Columns,
                                                        vector<string>* pResult)
{
    string::size_type maxLen = 1;

    for (vector<string>::const_iterator ipWord = Words.begin();
                                        ipWord != Words.end(); ++ipWord)
    {
       maxLen = max(maxLen, ipWord->size());
    }

    size_t wordsPerLine = max(Columns / (maxLen + PADDING),
                              (string::size_type) 1);

    string line;
    for (UINT32 word = 0; word < Words.size(); ++word)
    {
        if (word && (word % wordsPerLine == 0))
        {
            pResult->push_back(line);
            line.erase();
        }
        line += Words[word];
        line += string(maxLen + PADDING - Words[word].size(), ' ');
    }

    pResult->push_back(line);
}
