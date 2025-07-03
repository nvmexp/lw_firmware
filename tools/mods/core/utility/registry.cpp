/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007, 2016-2017, 2019 by LWPU Corporation. All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation. Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Cross platform (XP) interface.

#include "core/include/registry.h"
#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include "core/include/utility.h"
#include "lwRmReg.h"
#include <map>
#include <functional>
#include <set>

//------------------------------------------------------------------------------
static bool s_LoggingIsEnabled = false;
static map<string, int> s_SuccessfulReadAttempts;
static map<string, int> s_UnsuccessfulReadAttempts;

namespace Registry
{
    RC ReadCommon(const char * Node, const char * Key, jsval * pResult);

    const vector<string> s_InternalOnlyKeys =
    {
        LW_REG_STR_ENABLE_TESLA_OVERVOLTAGING
    };
};

//------------------------------------------------------------------------------
// Read a 'Key' from the 'Node' in the registry.
//
JS_CLASS(Registry);
SObject Registry_Object
(
    "Registry",
    RegistryClass,
    0,
    JSPROP_ENUMERATE,
    "MODS registry."
);
static SProperty Registry_ResourceManager
(
    Registry_Object,
    "ResourceManager",
    0,
    0,
    0,
    JSPROP_ENUMERATE,
    ""
);
static SProperty Registry_OpenGL
(
    Registry_Object,
    "OpenGL",
    0,
    0,
    0,
    JSPROP_ENUMERATE,
    ""
);
static SProperty Registry_LWMEM
(
    Registry_Object,
    "LWMEM",
    0,
    0,
    0,
    JSPROP_ENUMERATE,
    ""
);
static SProperty Registry_LWDA
(
    Registry_Object,
    "Lwca",
    0,
    0,
    0,
    JSPROP_ENUMERATE,
    ""
);
static SProperty Registry_LwSwitch
(
    Registry_Object,
    "LwSwitch",
    0,
    0,
    0,
    JSPROP_ENUMERATE,
    ""
);
static SProperty Registry_Vmiop  // Used by vgpu plugin at drivers/vgpu/mods
(
    Registry_Object,
    "Vmiop",
    0,
    0,
    0,
    JSPROP_ENUMERATE,
    ""
);
C_(Registry_EnableLogging);
static SMethod Registry_EnableLogging
(
    Registry_Object,
    "EnableLogging",
    C_Registry_EnableLogging,
    0,
    "Begin logging of Registry accesses by drivers.",
    MODS_INTERNAL_ONLY
);

C_(Registry_DumpAccessLog);
static SMethod Registry_DumpAccessLog
(
    Registry_Object,
    "DumpAccessLog",
    C_Registry_DumpAccessLog,
    0,
    "Print a summation of Registry access by drivers.",
    MODS_INTERNAL_ONLY
);

C_(Registry_EnableLogging)
{
    // Enable logging.
    s_LoggingIsEnabled = true;

    // Reset counters.
    s_SuccessfulReadAttempts.clear();
    s_UnsuccessfulReadAttempts.clear();

    *pReturlwalue = JSVAL_ZERO;
    return JS_TRUE;
}

C_(Registry_DumpAccessLog)
{
    RC rc;
    *pReturlwalue = JSVAL_ZERO;

    if (!s_LoggingIsEnabled)
    {
        return JS_TRUE;
    }

    map<string, int>::iterator iter;

    Printf(Tee::PriNormal,
            "\nRegistry keys that were read by a driver: (read-count, name)\n");
    for (iter = s_SuccessfulReadAttempts.begin();
            iter != s_SuccessfulReadAttempts.end();
                ++iter)
    {
        Printf(Tee::PriNormal, "\t%3d Registry.%s\n",
                iter->second, iter->first.c_str());
    }

    Printf(Tee::PriNormal,
            "\nRegistry keys that were searched-for by a driver, but were not set:\n");
    for (iter = s_UnsuccessfulReadAttempts.begin();
            iter != s_UnsuccessfulReadAttempts.end();
                ++iter)
    {
        Printf(Tee::PriNormal, "\t%3d Registry.%s\n",
                iter->second, iter->first.c_str());
    }

    Printf(Tee::PriNormal,
            "\nRegistry keys that were set but never read by any driver:\n");

    JavaScriptPtr pJs;
    vector<string> nodes;
    JSObject * pRegistryObject = Registry_Object.GetJSObject();
    rc = pJs->GetProperties(pRegistryObject, &nodes);
    if (OK != rc)
        return JS_TRUE;

    vector<string>::iterator nodeIter;
    for (nodeIter = nodes.begin(); nodeIter != nodes.end(); ++nodeIter)
    {
        string node = *nodeIter;
        if ((node == "EnableLogging") || (node == "DumpAccessLog"))
            continue;

        JSObject * pNodeObj;
        rc = pJs->GetProperty(pRegistryObject, node.c_str(), &pNodeObj);
        if (OK != rc)
            continue;

        vector<string> keys;
        rc = pJs->GetProperties(pNodeObj, &keys);
        if (OK != rc)
            continue;

        vector<string>::iterator keyIter;
        for (keyIter = keys.begin(); keyIter != keys.end(); ++keyIter)
        {
            string tmp = node + "[\"" + *keyIter + "\"]";

            iter = s_SuccessfulReadAttempts.find(tmp);
            if ((iter == s_SuccessfulReadAttempts.end()) || (iter->second < 1))
            {
                Printf(Tee::PriNormal, "\tRegistry.%s\n", tmp.c_str());
            }
        }
    }

    Printf(Tee::PriNormal, "\n");
    return JS_TRUE;
}

RC Registry::Read
(
    const char * Node,
    const char * Key,
    UINT32 *     pData
)
{
    MASSERT(pData != 0);

    jsval jsval;
    RC rc = ReadCommon(Node, Key, &jsval);
    if (OK == rc)
    {
        rc = JavaScriptPtr()->FromJsval(jsval, pData);
    }
    return rc;
}

RC Registry::Read
(
    const char * Node,
    const char * Key,
    string *     pData
)
{
    MASSERT(pData != 0);

    jsval jsval;
    RC rc = ReadCommon(Node, Key, &jsval);
    if (OK == rc)
    {
        rc = JavaScriptPtr()->FromJsval(jsval, pData);
    }
    return rc;
}

RC Registry::Read
(
    const char *     Node,
    const char *     Key,
    vector<UINT08> * pData
)
{
    MASSERT(pData != 0);

    jsval jsval;
    RC rc = ReadCommon(Node, Key, &jsval);
    if (OK == rc)
    {
        JsArray Array;
        JavaScriptPtr pJs;
        CHECK_RC(pJs->FromJsval(jsval, &Array));

        for (JsArray::size_type i = 0; i < Array.size(); ++i)
        {
            UINT32 data;
            RC rc;
            CHECK_RC(pJs->FromJsval(Array[i], &data));
            pData->push_back((UINT08)(data & 0xFF));
        }
    }
    return rc;
}

#ifndef LW_OFFSETOF
#define LW_OFFSETOF(s,m) ((size_t)(&(((s*)0)->m)))
#endif

RC Registry::PackRegistryEntries
(
    const char * node,
    PACKED_REGISTRY_TABLE  *pRegTable,
    UINT32 *pSize
)
{
    if (pSize == nullptr || node == nullptr)
    {
        return RC::ILWALID_ARGUMENT;
    }

    JavaScriptPtr pJs;
    JSObject * pRegistryObject = Registry_Object.GetJSObject();
    JSObject * nodeObj;
    RC rc;
    UINT32 totalSize;
    UINT32 numEntries;

    rc = pJs->GetProperty(pRegistryObject, node, &nodeObj);
    if (RC::OK != rc)
    {
        return rc;
    }

    vector<string> keys;
    rc = pJs->GetProperties(nodeObj, &keys);
    if (RC::OK != rc)
    {
        return rc;
    }

    numEntries = static_cast<UINT32>(keys.size());
    totalSize = static_cast<UINT32>(LW_OFFSETOF(PACKED_REGISTRY_TABLE, entries)) +
                (sizeof(PACKED_REGISTRY_ENTRY) * numEntries);

    if (numEntries == 0)
    {
        if (pRegTable != nullptr)
        {
            return RC::LWRM_NOTHING_TO_DO;
        }
        else
        {
            *pSize = totalSize;
            return RC::OK;
        }
    }

    UINT32 entryIndex = 0;
    UINT32 dataOffset = totalSize;
    UINT08 *pByte = reinterpret_cast<UINT08*>(pRegTable);

    for (const string& key : keys)
    {
        jsval value;
        rc = ReadCommon(node, key.c_str(), &value);
        if (RC::OK != rc)
        {
            return rc;
        }

        UINT32 keyLength = static_cast<UINT32>(key.length()) + 1;
        totalSize += keyLength;

        PACKED_REGISTRY_ENTRY *pEntry = nullptr;
        if (pRegTable != nullptr)
        {
            if (totalSize > *pSize)
            {
                Printf(Tee::PriError, "Registry entries overflow RPC record\n");
                return RC::LWRM_BUFFER_TOO_SMALL;
            }

            pEntry = &pRegTable->entries[entryIndex];
            pEntry->nameOffset = dataOffset;
            memcpy(&pByte[dataOffset], key.c_str(), keyLength);
            dataOffset += keyLength;
        }

        if (JSVAL_IS_OBJECT(value))
        {
            JsArray jsArray;
            rc = pJs->FromJsval(value, &jsArray);
            if (RC::OK == rc)
            {
                UINT32 objectSize = static_cast<UINT32>(jsArray.size());
                totalSize += objectSize;

                if (pRegTable != nullptr)
                {
                    if (totalSize > *pSize)
                    {
                        Printf(Tee::PriError, "Registry entries overflow RPC record\n");
                        return RC::LWRM_BUFFER_TOO_SMALL;
                    }

                    vector<UINT08> vectorData;
                    for (JsArray::size_type i = 0; i < jsArray.size(); ++i)
                    {
                        UINT32 data;
                        RC rc;
                        CHECK_RC(pJs->FromJsval(jsArray[i], &data));
                        vectorData.push_back(static_cast<UINT08>(data & 0xFF));
                    }

                    pEntry->type   = REGISTRY_TABLE_ENTRY_TYPE_BINARY;
                    pEntry->data   = dataOffset;
                    pEntry->length = objectSize;

                    memcpy(&pByte[dataOffset], &vectorData[0], objectSize);
                    dataOffset += pEntry->length;
                }
            }
            else
            {
                Printf(Tee::PriError, "Unable to form jsArray from reg value at %s\n", key.c_str());
                return rc;
            }

        }
        else if (JSVAL_IS_NUMBER(value) || JSVAL_IS_BOOLEAN(value))
        {
            //Int value is packed into entry directly, it does not affect packed registry size
            if (pRegTable != nullptr)
            {
                UINT32 intValue;
                rc = pJs->FromJsval(value, &intValue);
                if (RC::OK == rc)
                {
                    pEntry->type   = REGISTRY_TABLE_ENTRY_TYPE_DWORD;
                    pEntry->data   = intValue;
                    pEntry->length = sizeof(UINT32);
                }
                else
                {
                    Printf(Tee::PriError, "Unable to form int value from reg value at %s\n", key.c_str());
                    return rc;
                }
            }
        }
        else if (JSVAL_IS_STRING(value))
        {
            string stringValue;
            rc = pJs->FromJsval(value, &stringValue);
            if (RC::OK == rc)
            {
                UINT32 stringLength = static_cast<UINT32>(stringValue.length()) + 1;
                totalSize += stringLength;

                if (pRegTable != nullptr)
                {
                    if (totalSize > *pSize)
                    {
                        Printf(Tee::PriError, "Registry entries overflow RPC record\n");
                        return RC::LWRM_BUFFER_TOO_SMALL;
                    }

                    pEntry->type = REGISTRY_TABLE_ENTRY_TYPE_STRING;
                    pEntry->data = dataOffset;
                    pEntry->length = stringLength;

                    memcpy(&pByte[dataOffset], stringValue.c_str(), stringLength);
                    dataOffset += stringLength;
                }
            }
            else
            {
                Printf(Tee::PriError, "Unable to form string value from reg value at %s\n", key.c_str());
                return rc;
            }
        }
        else
        {
            Printf(Tee::PriError, "Unable to determine regkey type for %s\n", key.c_str());
            return RC::LWRM_ILWALID_DATA;
        }

        entryIndex++;
    }

    if (pRegTable != nullptr)
    {
        pRegTable->size = totalSize;
        pRegTable->numEntries = numEntries;
    }

    *pSize = totalSize;

    return rc;
}

RC Registry::ReadCommon
(
    const char * Node,
    const char * Key,
    jsval *      pResult
)
{
    RC rc;
    JavaScriptPtr pJs;
    JSObject * Registry = Registry_Object.GetJSObject();
    JSObject * NodeObj;

    MASSERT(pResult);
    if (OK != pJs->GetProperty(Registry, Node, &NodeObj))
    {
        rc = RC::REGISTRY_KEY_NOT_FOUND;
    }
    else if (OK != pJs->GetPropertyJsval(NodeObj, Key, pResult))
    {
        rc = RC::REGISTRY_KEY_NOT_FOUND;
    }

    if ((Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK) &&
        (find(s_InternalOnlyKeys.begin(),
              s_InternalOnlyKeys.end(), string(Key)) != s_InternalOnlyKeys.end()))
    {
        Printf(Tee::PriWarn, "Registry key %s is not supported\n", Key);
        rc = RC::REGISTRY_KEY_NOT_FOUND;
    }

    if (s_LoggingIsEnabled)
    {
        string tmp = Node;
        tmp.append("[\"");
        tmp.append(Key);
        tmp.append("\"]");
        if (OK == rc)
            s_SuccessfulReadAttempts[tmp] ++;
        else
            s_UnsuccessfulReadAttempts[tmp] ++;
    }

    return rc;
}

RC Registry::Write
(
    const char * Node,
    const char * Key,
    UINT32       Data
)
{
    JavaScriptPtr pJs;
    JSObject * Registry = Registry_Object.GetJSObject();
    JSObject * NodeObj, *KeyObj;
    if (OK != pJs->GetProperty(Registry, Node, &NodeObj))
        return RC::REGISTRY_KEY_NOT_FOUND;
    RC rc = pJs->GetProperty(NodeObj, Key, &KeyObj);
    if((OK != rc) || !KeyObj)
    {
        rc.Clear();
        rc = pJs->DefineProperty(NodeObj, Key, &KeyObj);
    }
    if (OK != rc)
        return RC::REGISTRY_KEY_NOT_FOUND;
    CHECK_RC(pJs->SetProperty(NodeObj, Key, Data));

    return OK;
}

RC Registry::Write
(
    const char * Node,
    const char * Key,
    const string & Data
)
{
    JavaScriptPtr pJs;
    JSObject * Registry = Registry_Object.GetJSObject();
    JSObject * NodeObj, *KeyObj;
    if (OK != pJs->GetProperty(Registry, Node, &NodeObj))
        return RC::REGISTRY_KEY_NOT_FOUND;
    RC rc = pJs->GetProperty(NodeObj, Key, &KeyObj);
    if((OK != rc) || !KeyObj)
    {
        rc.Clear();
        rc = pJs->DefineProperty(NodeObj, Key, &KeyObj);
    }
    if (OK != rc)
        return RC::REGISTRY_KEY_NOT_FOUND;
    CHECK_RC(pJs->SetProperty(NodeObj, Key, Data));

    return OK;
}

RC Registry::Write
(
    const char * Node,
    const char * Key,
    const vector<UINT08> &Data
)
{
    RC rc;
    JavaScriptPtr pJs;
    JSObject * Registry = Registry_Object.GetJSObject();
    JSObject * NodeObj;
    if (OK != pJs->GetProperty(Registry, Node, &NodeObj))
        return RC::REGISTRY_KEY_NOT_FOUND;

    JsArray jsa;
    for ( vector<UINT08>::size_type i = 0; i < Data.size(); ++i)
    {
        jsval data;
        CHECK_RC(pJs->ToJsval((UINT32)Data[i], &data));
        jsa.push_back(data);
    }

    jsval arrayVal;
    CHECK_RC(pJs->ToJsval(&jsa, &arrayVal));
    CHECK_RC(pJs->SetPropertyJsval(NodeObj, Key, arrayVal));

    return OK;
}
