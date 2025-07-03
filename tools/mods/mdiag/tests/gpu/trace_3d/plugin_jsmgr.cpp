/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2011,2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/utils/types.h"
#include "t3plugin.h"
#include "core/include/jscript.h"
#include "core/include/cmdline.h"
#include "core/include/js_param.h"
#include "core/include/gpu.h"
#include "core/include/argpproc.h"
#include "core/include/utility.h"

#include <memory>

namespace T3dPlugin
{

// JavaScript manager
class JavaScriptManagerMods_impl : public JavaScriptManagerMods
{
public:

    friend JavaScriptManagerMods *JavaScriptManagerMods::Create(HostMods * pHost);

    explicit JavaScriptManagerMods_impl(HostMods *pHost)
    {
    }

    virtual ~JavaScriptManagerMods_impl() {}

    virtual INT32 LoadJavaScript(const char *fileName)
    {
        RC rc = OK;
        CHECK_RC(m_JavaScript->GetInitRC());
        CHECK_RC(m_JavaScript->Import(fileName));
        return rc;
    }

    virtual INT32 ParseArgs(const char *cmdLine)
    {
        RC rc = OK;
        JsParamList *jsParamList = CommandLine::ParamList();
        ParamDecl *paramDecl = jsParamList->GetParamDeclArray();
        ParamConstraints *paramConstraints = jsParamList->GetParamConstraintsArray();
        ArgReader reader(paramDecl, paramConstraints);
        ArgDatabase database;
        database.AddArgLine(cmdLine);
        if (!reader.ParseArgs(&database))
        {
            return -1;
        }

        // Handle all global arguments
        JsParamDecl *lwrParam = jsParamList->GetFirstParameter();
        while (lwrParam != NULL)
        {
            CHECK_RC(lwrParam->HandleArg(&reader, ::Gpu::MaxNumDevices));
            lwrParam = jsParamList->GetNextParameter(lwrParam);
        }

        // Handle all arguments in the device scope
        ArgPreProcessor argPreProc;
        for (UINT32 devInst = argPreProc.GetFirstDevScope();
            devInst != ::Gpu::MaxNumDevices;
            devInst = argPreProc.GetNextDevScope(devInst))
        {
            ArgReader devReader(paramDecl, paramConstraints);
            string devScope = Utility::StrPrintf("dev%d", devInst);

            if (!devReader.ParseArgs(&database, devScope.c_str(), false))
            {
                return -1;
            }

            lwrParam = jsParamList->GetFirstParameter();
            while (lwrParam != NULL)
            {
                CHECK_RC(lwrParam->HandleArg(&devReader, devInst));
                lwrParam = jsParamList->GetNextParameter(lwrParam);
            }
        }

        return rc;
    }

    virtual INT32 CallVoidMethod(const char *methodName,
        const char **argValue,
        UINT32 argCount)
    {
        RC rc = OK;
        jsval jsRetValue = JSVAL_NULL;
        CHECK_RC(InternalCallMethod(methodName,
            argValue,
            argCount,
            &jsRetValue));
        return rc;
    }

    virtual INT32 CallINT32Method(const char *methodName,
        const char **argValue,
        UINT32 argCount,
        INT32 *retValue)
    {
        RC rc = OK;
        jsval jsRetValue = JSVAL_NULL;
        CHECK_RC(InternalCallMethod(methodName,
            argValue,
            argCount,
            &jsRetValue));
        CHECK_RC(m_JavaScript->FromJsval(jsRetValue, retValue));
        return rc;
    }

    virtual INT32 CallUINT32Method(const char *methodName,
        const char **argValue,
        UINT32 argCount,
        UINT32 *retValue)
    {
        RC rc = OK;
        jsval jsRetValue = JSVAL_NULL;
        CHECK_RC(InternalCallMethod(methodName,
            argValue,
            argCount,
            &jsRetValue));
        CHECK_RC(m_JavaScript->FromJsval(jsRetValue, retValue));
        return rc;
    }

protected:

    virtual RC InternalCallMethod(const char *methodName,
        const char **argValue,
        UINT32 argCount,
        jsval *retValue)
    {
        RC rc = OK;
        JSObject *pGlobObj = m_JavaScript->GetGlobalObject();
        JsArray jsArgs;
        for (UINT32 i = 0; i < argCount; ++i)
        {
            const char *arg = argValue[i];
            jsval jsArg;
            CHECK_RC(m_JavaScript->ToJsval((arg == NULL) ? "" : string(arg), &jsArg));
            jsArgs.push_back(jsArg);
        }
        CHECK_RC(m_JavaScript->CallMethod(pGlobObj, methodName, jsArgs, retValue));
        return rc;
    }

private:
    JavaScriptPtr m_JavaScript;
};

JavaScriptManagerMods * JavaScriptManagerMods::Create( HostMods * pHost )
{
    JavaScriptManagerMods_impl * pJavaScriptManager = new JavaScriptManagerMods_impl( pHost );
    return pJavaScriptManager;
}

} // namespace T3dPlugin

