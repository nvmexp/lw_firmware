/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2017,2019-2020 by LWPU Corporation. All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_REGISTRY_H
#define INCLUDED_REGISTRY_H

#include "core/include/script.h"
#include "core/include/jscript.h"

extern SObject Registry_Object;

//! Registry access
namespace Registry
{
    //
    // Structures for packaging/unpackaging registry for GSP load
    //
    typedef struct PACKED_REGISTRY_ENTRY
    {
        LwU32                   nameOffset;
        LwU8                    type;
        LwU32                   data;
        LwU32                   length;
    } PACKED_REGISTRY_ENTRY;

    typedef struct PACKED_REGISTRY_TABLE
    {
        LwU32                   size;
        LwU32                   numEntries;
        PACKED_REGISTRY_ENTRY   entries[1];
    } PACKED_REGISTRY_TABLE;

    //
    // Values for PACKED_REGISTRY_ENTRY::type
    //
    #define REGISTRY_TABLE_ENTRY_TYPE_UNKNOWN  0
    #define REGISTRY_TABLE_ENTRY_TYPE_DWORD    1
    #define REGISTRY_TABLE_ENTRY_TYPE_BINARY   2
    #define REGISTRY_TABLE_ENTRY_TYPE_STRING   3


   //@{
   //! Read a registry entry.
   RC Read(const char * Node, const char * Key, UINT32 * pData);
   RC Read(const char * Node, const char * Key, string * pData);
   RC Read(const char * Node, const char * Key, vector<UINT08> * pData);
   //@}

   //@{
   //! Write a registry entry.
   RC Write(const char * Node, const char * Key, UINT32  Data);
   RC Write(const char * Node, const char * Key, const string &Data);
   RC Write(const char * Node, const char * Key, const vector<UINT08> &Data);
   //@}

   RC PackRegistryEntries(const char * Node, PACKED_REGISTRY_TABLE  *pRegTable, UINT32 *pSize);

   template<typename T>
   RC ForEachEntry(const char* node, const char* property, T cbFunc)
   {
        RC rc;
        JavaScriptPtr pJs;
        JSContext *cx;
        CHECK_RC(pJs->GetContext(&cx));
        JSObject *pRegistryObject = Registry_Object.GetJSObject();

        vector<string> nodes;
        if (node && node[0])
        {
            nodes.push_back(node);
        }
        else
        {
            CHECK_RC(pJs->GetProperties(pRegistryObject, &nodes));
        }

        vector<string>::iterator nodeIter;
        for (nodeIter = nodes.begin(); nodeIter != nodes.end(); ++nodeIter)
        {
            string node = *nodeIter;
            JSObject * pNodeObj;
            CHECK_RC(pJs->GetProperty(pRegistryObject, node.c_str(), &pNodeObj));
            
            if (property && property[0])
            {
                jsval propVal;
                CHECK_RC(pJs->GetPropertyJsval(pNodeObj, property, &propVal));
                if (JSVAL_IS_INT(propVal))
                {
                    CHECK_RC(cbFunc(node.c_str(), property, JSVAL_TO_INT(propVal)));
                }
            }
            else
            {
                auto idDestroy = bind(JS_DestroyIdArray, cx, placeholders::_1);
                unique_ptr<JSIdArray, decltype(idDestroy)> propIds(JS_Enumerate(cx, pNodeObj), idDestroy);
                for (jsint i = 0; propIds->length > i; ++i)
                {
                    jsval propNameVal;
                    if (JS_IdToValue(cx, propIds->vector[i], &propNameVal) &&
                        JSVAL_IS_STRING(propNameVal))
                    {
                        auto jsStr = JSVAL_TO_STRING(propNameVal);
                        auto propName = JS_GetStringBytes(jsStr);

                        jsval propVal;
                        JS_GetUCProperty(cx, pNodeObj, JSSTRING_CHARS(jsStr), JSSTRING_LENGTH(jsStr), &propVal);
                        if (JSVAL_IS_NUMBER(propVal))
                        {
                            UINT32 data;
                            if (JSVAL_IS_DOUBLE(propVal))
                            {
                                data = static_cast<UINT32>(*JSVAL_TO_DOUBLE(propVal));
                            }
                            else // IsInt
                            {
                                data = JSVAL_TO_INT(propVal);
                            }
                            CHECK_RC(cbFunc(node.c_str(), propName, data));
                        }
                    }
                }
            }
        }

        return rc;
   }
}

#endif
