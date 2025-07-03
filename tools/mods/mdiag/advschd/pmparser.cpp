/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file  pmparser.cpp
//! \brief Policy file parser

#include "pmparser.h"
#include "core/include/channel.h"
#include "gpu/utility/chanwmgr.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl208f.h"
#include "ctrl/ctrl2080/ctrl2080power.h"
#include "class/cl2080.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jscript.h"
#include "jsdbgapi.h"
#include "core/include/platform.h"
#include "pmaction.h"
#include "pmevntmn.h"
#include "pmgilder.h"
#include "pmsurf.h"
#include "pmtrigg.h"
#include "gpu/perf/pmusub.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "rmpolicy.h"
#include "mdiag/subctx.h"
#include "mdiag/sysspec.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
#include "mdiag/vgpu_migration/vgpu_migration.h"

// Macros to define the policyfile classes, such as "Policy",
// "Trigger", "ActionBlock", "Action", etc.
//
// Some policyfile classes silently have "PolicyManager_" prepended to
// their names, to prevent name-collision with the "real" javascript
// classes.  There are #defines in default.h to hide the prefix, such
// as "#define Gpu PolicyManager_Gpu".  The "WITH_ALIAS" version of
// PM_CLASS and PM_BIND handle those name-manglings.
//
#define PM_CLASS_WITH_ALIAS(className, javascriptName)   \
    JS_CLASS(PolicyManager_##className);                 \
    static SObject PolicyManager_##className##_Object    \
    (                                                    \
        #javascriptName,                                 \
        PolicyManager_##className##Class,                \
        0,                                               \
        0,                                               \
        #className "(policyfile syntax)"                 \
    )

#define PM_CLASS(className) \
    PM_CLASS_WITH_ALIAS(className, className)

// Macro to bind a PmParser method to a javascript command.
//
#define PM_BIND_WITH_ALIAS(tokenType, tokenName, javascriptName,        \
                           numArgs, usage, desc)                        \
    C_(PolicyManager_##tokenType##_##tokenName )                        \
    {                                                                   \
        MASSERT(pContext != NULL);                                      \
        MASSERT(pArguments != NULL);                                    \
        MASSERT(pReturlwalue != NULL);                                  \
                                                                        \
        PmParser *pParser = PolicyManager::Instance()->GetParser();     \
        *pReturlwalue = JSVAL_NULL;                                     \
        pParser->SetErrInfo(pContext);                                  \
        if (!pParser->PreProcessToken(pContext, numArgs,                \
                    NumArguments, usage))                               \
        {                                                               \
            return JS_FALSE;                                            \
        }                                                               \
        JSBool status = pParser->tokenType##_##tokenName(               \
            PM_##numArgs##_ARG_ARGLIST pReturlwalue);                   \
        return pParser->PostProcessToken(pContext, pReturlwalue,        \
                                         status, usage);                \
    }                                                                   \
                                                                        \
    static SMethod PolicyManager_##tokenType##_##tokenName              \
    (                                                                   \
        PolicyManager_##tokenType##_Object,                             \
        #javascriptName,                                                \
        C_##PolicyManager_##tokenType##_##tokenName,                    \
        0,                                                              \
        desc                                                            \
    )

#define PM_BIND(tokenType, tokenName, numArgs, usage, desc) \
    PM_BIND_WITH_ALIAS(tokenType, tokenName, tokenName, numArgs, usage, desc)

// Macro to bind a PmParser method to a javascript command.
// Support the variable argument list
// Note: You need to sanity check the argument number via PM_CHECK_ARGS_NUM
//
#define PM_BIND_VAR_WITH_ALIAS(tokenType, tokenName, javascriptName,    \
                            numArgs, usage, desc)                       \
    C_(PolicyManager_##tokenType##_##tokenName )                        \
    {                                                                   \
        MASSERT(pContext != NULL);                                      \
        MASSERT(pArguments != NULL);                                    \
        MASSERT(pReturlwalue != NULL);                                  \
                                                                        \
        PmParser *pParser = PolicyManager::Instance()->GetParser();     \
        *pReturlwalue = JSVAL_NULL;                                     \
        pParser->SetErrInfo(pContext);                                  \
        PM_CHECK_ARGS_NUM(NumArguments, numArgs);                       \
        JSBool status = pParser->tokenType##_##tokenName(               \
            &pArguments[0], NumArguments, pReturlwalue);                \
        return pParser->PostProcessToken(pContext, pReturlwalue,        \
                                         status, usage);                \
    }                                                                   \
                                                                        \
    static SMethod PolicyManager_##tokenType##_##tokenName              \
    (                                                                   \
        PolicyManager_##tokenType##_Object,                             \
        #javascriptName,                                                \
        C_##PolicyManager_##tokenType##_##tokenName,                    \
        0,                                                              \
        desc                                                            \
    )

#define PM_BIND_VAR(tokenType, tokenName, numArgs, usage, desc) \
    PM_BIND_VAR_WITH_ALIAS(tokenType, tokenName, tokenName, numArgs, usage, desc)

#define PM_CHECK_ARGS_NUM(runtimeNumArgs, exceptedNumArgs)      \
{                                                               \
    if (runtimeNumArgs > exceptedNumArgs)                       \
    {                                                           \
        return JS_FALSE;                                        \
    }                                                           \
}

// Macros used to implement the PM_BIND macro
//
#define PM_0_ARG_ARGLIST
#define PM_1_ARG_ARGLIST                  pArguments[0],
#define PM_2_ARG_ARGLIST PM_1_ARG_ARGLIST pArguments[1],
#define PM_3_ARG_ARGLIST PM_2_ARG_ARGLIST pArguments[2],
#define PM_4_ARG_ARGLIST PM_3_ARG_ARGLIST pArguments[3],
#define PM_5_ARG_ARGLIST PM_4_ARG_ARGLIST pArguments[4],
#define PM_6_ARG_ARGLIST PM_5_ARG_ARGLIST pArguments[5],
#define PM_7_ARG_ARGLIST PM_6_ARG_ARGLIST pArguments[6],
#define PM_8_ARG_ARGLIST PM_7_ARG_ARGLIST pArguments[7],
#define PM_9_ARG_ARGLIST PM_8_ARG_ARGLIST pArguments[8],

// Macro to return an object from a PmParser method.  The method must
// have a pReturlwaule pass-by-reference argument.
//
#define RETURN_OBJECT(pObject)                          \
    do {                                                \
        RC rc;                                          \
        C_CHECK_RC(ToJsval(pObject, pReturlwalue));     \
        return JS_TRUE;                                 \
    } while (0)

// Macro to return an Action from a PmParser method.  If we are in an
// action-block scope, this macro appends the Action to the
// ActionBlock.  Otherwise, it returns the action.  (The latter case
// is used to support simple one-action Policy.Define() statements.)
//
#define RETURN_ACTION(pAction)                                            \
    do {                                                                  \
        if (CheckForActionBlockScope("") == OK) {                         \
            PolicyManagerMessageDebugger::Instance()->AddTeeModule(       \
                                m_Scopes.top().m_pActionBlock->GetName(), \
                                                   (pAction)->GetName()); \
            RETURN_RC(m_Scopes.top().m_pActionBlock->AddAction(pAction)); \
        }                                                                 \
        else{                                                             \
            PolicyManagerMessageDebugger::Instance()->AddTeeModule(       \
                                           "", (pAction)->GetName());     \
            RETURN_OBJECT(pAction);                                       \
        }                                                                 \
    } while (0)

// Macro to return a PmSurfaceDesc from a PmParser method.  If we are
// in a surface-definition scope, this macro sets a field in the
// PmSurfaceDesc.  Otherwise, it returns a PmSurfaceDesc with the
// single field set.  (The latter case is used to support simple
// surfaces in triggers and actions.)
//
#define RETURN_SURFACE(Field, oldValue, newValue)                       \
    do {                                                                \
        PmSurfaceDesc * pSurfaceDesc = nullptr;                         \
        CREATE_SURFACE(pSurfaceDesc);                                   \
        SET_SURFACE_ATTR(pSurfaceDesc, Field, oldValue, newValue);      \
        RETURN_OBJECT(pSurfaceDesc);                                    \
    } while (0)

//MARCO to create a PmSurfaceDesc. Different from RETURN_SURFACE, it will
// not set any fields and not return from the called scope

#define CREATE_SURFACE(pSurfaceDesc)                                    \
    do {                                                                \
        if (CheckForSurfaceScope("") == OK)                             \
        {                                                               \
            pSurfaceDesc = m_Scopes.top().m_pSurfaceDesc;               \
        }                                                               \
        else                                                            \
        {                                                               \
            pSurfaceDesc = new PmSurfaceDesc("");                       \
            C_CHECK_RC(GetEventManager()->AddSurfaceDesc(pSurfaceDesc));\
        }                                                               \
    } while (0)

#define SET_SURFACE_ATTR(pSurfaceDesc, Field, oldValue, newValue)       \
    do {                                                                \
       if (pSurfaceDesc->Get##Field() != oldValue)                      \
       {                                                                \
            ErrPrintf("%s: Surface."#Field" already set for \"%s\"\n",  \
                      GetErrInfo().c_str(),                             \
                      pSurfaceDesc->GetId().c_str());                   \
            RETURN_RC(RC::CANNOT_PARSE_FILE);                           \
        }                                                               \
        C_CHECK_RC(pSurfaceDesc->Set##Field(newValue));                 \
    } while (0)

// Macro to return a PmChannelDesc from a PmParser method.  If we are
// in a channel-definition scope, this macro sets a field in the
// PmChannelDesc.  Otherwise, it returns a PmChannelDesc with the
// single field set.  (The latter case is used to support simple
// channels in triggers and actions.)
//
#define RETURN_CHANNEL(Field, oldValue, newValue)                       \
    do {                                                                \
        PmChannelDesc * pChannelDesc = nullptr;                         \
        CREATE_CHANNEL(pChannelDesc);                                   \
        SET_CHANNEL_ATTR(pChannelDesc, Field, oldValue, newValue);      \
        RETURN_OBJECT(pChannelDesc);                                    \
    } while (0)

#define CREATE_CHANNEL(pChannelDesc)                                        \
    do {                                                                    \
        RC rc;                                                              \
        if (CheckForChannelScope("") == OK)                                 \
        {                                                                   \
            pChannelDesc = m_Scopes.top().m_pChannelDesc;                   \
        }                                                                   \
        else                                                                \
        {                                                                   \
            pChannelDesc = new PmChannelDesc("");                           \
            C_CHECK_RC(GetEventManager()->AddChannelDesc(pChannelDesc));    \
        }                                                                   \
    } while(0)

#define SET_CHANNEL_ATTR(pChannelDesc, Field, oldValue, newValue)       \
    do {                                                                \
        if (pChannelDesc->Get##Field() != oldValue)                     \
        {                                                               \
            ErrPrintf("%s: Channel."#Field" already set for \"%s\"\n",  \
                      GetErrInfo().c_str(),                             \
                      pChannelDesc->GetId().c_str());                   \
            RETURN_RC(RC::CANNOT_PARSE_FILE);                           \
        }                                                               \
        C_CHECK_RC(pChannelDesc->Set##Field(newValue));                 \
    } while(0)

// Macro to return a PmVfTestDesc from a PmParser method.  If we are
// in a gpu-definition scope, this macro sets a field in the
// PmVfTestDesc.  Otherwise, it returns a PmVfTestDesc with the
// single field set.  (The latter case is used to support simple
// gpus in triggers and actions.)
//
#define RETURN_VF(Field, oldValue, newValue)                                \
    do {                                                                    \
        if (CheckForVfScope("") == OK)                                      \
        {                                                                   \
            PmVfTestDesc *pVfDesc = m_Scopes.top().m_pVfDesc;               \
            if (pVfDesc->Get##Field() != oldValue)                          \
            {                                                               \
                ErrPrintf("%s: VF."#Field" already set for \"%s\"\n",       \
                          GetErrInfo().c_str(),                             \
                          pVfDesc->GetId().c_str());                        \
                RETURN_RC(RC::CANNOT_PARSE_FILE);                           \
            }                                                               \
            RETURN_RC(pVfDesc->Set##Field(newValue));                       \
        }                                                                   \
        else                                                                \
        {                                                                   \
            PmVfTestDesc *pVfDesc = new PmVfTestDesc("");                           \
            C_CHECK_RC(GetEventManager()->AddVfDesc(pVfDesc));              \
            C_CHECK_RC(pVfDesc->Set##Field(newValue));                      \
            RETURN_OBJECT(pVfDesc);                                         \
        }                                                                   \
    } while (0)

// Macro to return a PmVaSpaceDesc from a PmParser method.  If we are
// in a gpu-definition scope, this macro sets a field in the
// PmVaSpaceDesc.  Otherwise, it returns a PmVaSpaceDesc with the
// single field set.  (The latter case is used to support simple
// gpus in triggers and actions.)
//
#define CREATE_VASPACE(pVaSpaceDesc)                                        \
    do {                                                                    \
        RC rc;                                                              \
        if (CheckForVaSpaceScope("") == OK)                                 \
        {                                                                   \
            pVaSpaceDesc = m_Scopes.top().m_pVaSpaceDesc;                   \
        }                                                                   \
        else                                                                \
        {                                                                   \
            pVaSpaceDesc = new PmVaSpaceDesc("");                           \
            C_CHECK_RC(GetEventManager()->AddVaSpaceDesc(pVaSpaceDesc));    \
        }                                                                   \
    } while(0)

#define SET_VASPACE_ATTR(pVaSpaceDesc, Field, oldValue, newValue)       \
    do {                                                                \
        if (pVaSpaceDesc->Get##Field() != oldValue)                     \
        {                                                               \
            ErrPrintf("%s: VaSpace."#Field" already set for \"%s\"\n",  \
                      GetErrInfo().c_str(),                             \
                      pVaSpaceDesc->GetId().c_str());                   \
            RETURN_RC(RC::CANNOT_PARSE_FILE);                           \
        }                                                               \
        C_CHECK_RC(pVaSpaceDesc->Set##Field(newValue));                 \
    } while(0)

// Macro to return a PmSmcEngineDesc from a PmParser method.  If we are
// in a gpu-definition scope, this macro sets a field in the
// PmSmcEngineDesc.  Otherwise, it returns a PmSmcEngineDesc with the
// single field set.  (The latter case is used to support simple
// gpus in triggers and actions.)
//
#define RETURN_SMCENGINE(Field, oldValue, newValue)                                 \
    do {                                                                            \
        if (CheckForSmcEngineScope("") == OK)                                       \
        {                                                                           \
            PmSmcEngineDesc *pSmcEngineDesc = m_Scopes.top().m_pSmcEngineDesc;      \
            if (pSmcEngineDesc->Get##Field() != oldValue)                           \
            {                                                                       \
                ErrPrintf("%s: SmcEngine."#Field" already set for \"%s\"\n",        \
                          GetErrInfo().c_str(),                                     \
                          pSmcEngineDesc->GetId().c_str());                         \
                RETURN_RC(RC::CANNOT_PARSE_FILE);                                   \
            }                                                                       \
            RETURN_RC(pSmcEngineDesc->Set##Field(newValue));                        \
        }                                                                           \
        else                                                                        \
        {                                                                           \
            PmSmcEngineDesc *pSmcEngineDesc = new PmSmcEngineDesc("");              \
            C_CHECK_RC(GetEventManager()->AddSmcEngineDesc(pSmcEngineDesc));        \
            C_CHECK_RC(pSmcEngineDesc->Set##Field(newValue));                       \
            RETURN_OBJECT(pSmcEngineDesc);                                          \
        }                                                                           \
    } while (0)

// Macro to return a PmGpuDesc from a PmParser method.  If we are
// in a gpu-definition scope, this macro sets a field in the
// PmGpuDesc.  Otherwise, it returns a PmGpuDesc with the
// single field set.  (The latter case is used to support simple
// gpus in triggers and actions.)
//
#define RETURN_GPU(Field, oldValue, newValue)                           \
    do {                                                                \
        if (CheckForGpuScope("") == OK)                                 \
        {                                                               \
            PmGpuDesc *pGpuDesc = m_Scopes.top().m_pGpuDesc;            \
            if (pGpuDesc->Get##Field() != oldValue)                     \
            {                                                           \
                ErrPrintf("%s: Gpu."#Field" already set for \"%s\"\n",  \
                          GetErrInfo().c_str(),                         \
                          pGpuDesc->GetId().c_str());                   \
                RETURN_RC(RC::CANNOT_PARSE_FILE);                       \
            }                                                           \
            RETURN_RC(pGpuDesc->Set##Field(newValue));                  \
        }                                                               \
        else                                                            \
        {                                                               \
            PmGpuDesc *pGpuDesc = new PmGpuDesc("");                    \
            C_CHECK_RC(GetEventManager()->AddGpuDesc(pGpuDesc));        \
            C_CHECK_RC(pGpuDesc->Set##Field(newValue));                 \
            RETURN_OBJECT(pGpuDesc);                                    \
        }                                                               \
    } while (0)

// Macro to return a PmRunlistDesc from a PmParser method.  If we are
// in a runlist-definition scope, this macro sets a field in the
// PmRunlistDesc.  Otherwise, it returns a PmRunlistDesc with the
// single field set.  (The latter case is used to support simple
// runlists in triggers and actions.)
//
#define RETURN_RUNLIST(Field, oldValue, newValue)                         \
    do {                                                                  \
        if (CheckForRunlistScope("") == OK)                               \
        {                                                                 \
            PmRunlistDesc *pRunlistDesc = m_Scopes.top().m_pRunlistDesc;  \
            if (pRunlistDesc->Get##Field() != oldValue)                   \
            {                                                             \
                ErrPrintf("%s: Runlist."#Field" already set for \"%s\"\n",\
                          GetErrInfo().c_str(),                           \
                          pRunlistDesc->GetId().c_str());                 \
                RETURN_RC(RC::CANNOT_PARSE_FILE);                         \
            }                                                             \
            RETURN_RC(pRunlistDesc->Set##Field(newValue));                \
        }                                                                 \
        else                                                              \
        {                                                                 \
            PmRunlistDesc *pRunlistDesc = new PmRunlistDesc("");          \
            C_CHECK_RC(GetEventManager()->AddRunlistDesc(pRunlistDesc));  \
            C_CHECK_RC(pRunlistDesc->Set##Field(newValue));               \
            RETURN_OBJECT(pRunlistDesc);                                  \
        }                                                                 \
    } while (0)

// Macro to return a PmTestDesc from a PmParser method.  If we are
// in a test-definition scope, this macro sets a field in the
// PmTestDesc.  Otherwise, it returns a PmTestDesc with the
// single field set.  (The latter case is used to support simple
// tests in triggers and actions.)
//
#define RETURN_TEST(Field, oldValue, newValue)                          \
    do {                                                                \
        if (CheckForTestScope("") == OK)                                \
        {                                                               \
            PmTestDesc *pTestDesc = m_Scopes.top().m_pTestDesc;         \
            if (pTestDesc->Get##Field() != oldValue)                    \
            {                                                           \
                ErrPrintf("%s: Test."#Field" already set for \"%s\"\n", \
                          GetErrInfo().c_str(),                         \
                          pTestDesc->GetId().c_str());                  \
                RETURN_RC(RC::CANNOT_PARSE_FILE);                       \
            }                                                           \
            RETURN_RC(pTestDesc->Set##Field(newValue));                 \
        }                                                               \
        else                                                            \
        {                                                               \
            PmTestDesc *pTestDesc = new PmTestDesc("");                 \
            C_CHECK_RC(GetEventManager()->AddTestDesc(pTestDesc));      \
            C_CHECK_RC(pTestDesc->Set##Field(newValue));                \
            RETURN_OBJECT(pTestDesc);                                   \
        }                                                               \
    } while (0)

//--------------------------------------------------------------------
//! \brief PmParser constructor
//!
PmParser::PmParser(PolicyManager *pPolicyManager) :
    m_pPolicyManager(pPolicyManager),
    m_FileName(""),
    m_LineNum(0),
    m_Version(0)
{
    MASSERT(m_pPolicyManager != NULL);
}

//--------------------------------------------------------------------
//! \brief Parse the policy file
//!
RC PmParser::Parse(const string &fileName)
{
    RC rc;
    JavaScriptPtr pJs;

    Printf(Tee::PriLow, "Using policy file %s\n", fileName.c_str());

    m_FileName = Utility::DefaultFindFile(fileName, true);
    m_LineNum  = 0;
    m_Version  = 0;
    while (!m_Scopes.empty())
        m_Scopes.pop();
    PushScope(Scope::ROOT);

    // "Hide" the fact that we're using javascript by returning a
    // policy manager specific error code if we fail here.  People are
    // going to figure it out anyway, but I'd rather not have the word
    // "script" appear in the error message.  PolicyFiles should be
    // viewed as static lists of commands, not dynamically changing
    // things as the word "script" implies.
    //
    rc = pJs->Import(m_FileName);

    if (rc != OK)
    {
        ErrPrintf("%s: %s\n", GetErrInfo().c_str(), rc.Message());
        return RC::CANNOT_PARSE_FILE;
    }

    // Sanity checks at the end of parsing
    //
    if (m_Version == 0)
    {
        ErrPrintf("%s: Must have a Policy.Version\n",
                  GetErrInfo().c_str());
        return RC::CANNOT_PARSE_FILE;
    }

    MASSERT(!m_Scopes.empty());
    switch (m_Scopes.top().m_Type)
    {
        case Scope::ROOT:
            MASSERT(m_Scopes.size() == 1);
            break; // success
        case Scope::ACTION_BLOCK:
        case Scope::CONDITION:
        case Scope::ELSE_BLOCK:
        case Scope::ALLOW_OVERLAPPING_TRIGGERS:
            ErrPrintf("%s: Unterminated ActionBlock\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
        case Scope::SURFACE:
            ErrPrintf("%s: Unterminated Surface\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
        case Scope::CHANNEL:
            ErrPrintf("%s: Unterminated Channel\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
        case Scope::VF:
            ErrPrintf("%s: Unterminated VF\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
        case Scope::SMCENGINE:
            ErrPrintf("%s: Unterminated SMCENGINE\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
        case Scope::VASPACE:
            ErrPrintf("%s: Unterminated Gpu\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
        case Scope::GPU:
            ErrPrintf("%s: Unterminated Gpu\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
        case Scope::RUNLIST:
            ErrPrintf("%s: Unterminated Runlist\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
        case Scope::TEST:
            ErrPrintf("%s: Unterminated Test\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
        case Scope::MUTEX:
        case Scope::MUTEX_CONDITION:
            ErrPrintf("%s: Unterminated Mutex\n",
                      GetErrInfo().c_str());
            return RC::CANNOT_PARSE_FILE;
    }

    // Check that at least one trigger/action block pair has been
    // defined.  This requirement is meant to help people new to
    // writing policy files
    //
    if (GetEventManager()->GetNumTriggers() == 0)
    {
        ErrPrintf("%s: no Trigger/ActionBlock pairs.\n",
                  GetErrInfo().c_str());
        return RC::CANNOT_PARSE_FILE;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Set the error info (e.g. line number) from the context.
//!
//! \param pContext The current context, or NULL if none
//!
void PmParser::SetErrInfo(JSContext *pContext)
{
    m_LineNum = 0;

    // Try to pop stack frames until we find line-number info
    //
    if (pContext != NULL)
    {
        JSStackFrame *pFrame = NULL;
        JS_FrameIterator(pContext, &pFrame);
        while (pFrame != NULL && m_LineNum == 0)
        {
            jsbytecode *pc      = JS_GetFramePC(pContext, pFrame);
            JSScript   *pScript = JS_GetFrameScript(pContext, pFrame);
            if (pc != NULL && pScript != NULL) {
                m_LineNum = JS_PCToLineNumber(pContext, pScript, pc);
            }
            JS_FrameIterator(pContext, &pFrame);
        }
    }
}

//--------------------------------------------------------------------
//! \brief Return a string to use at the start of error messages.
//!
//! The returned string is something like "Error in foo.pcy line 5".
//! Intended usage is ErrPrintf("%s: blah blah\n",
//! GetErrInfo().c_str(), ...).
//!
string PmParser::GetErrInfo() const
{
    if (m_FileName == "")
    {
        return "PolicyFile error";
    }
    else if (m_LineNum == 0)
    {
        return "Error in " + m_FileName;
    }
    else
    {
        return Utility::StrPrintf("Error in %s line %d",
                                  m_FileName.c_str(), m_LineNum);
    }
}

//--------------------------------------------------------------------
//! This method is used by the C_(XXX) wrappers before the token has
//! been processed.  It sanity-checks the number of args, and on
//! error, prints the appropriate error and returns false.  The caller
//! should return JS_FALSE on failure.
//!
bool PmParser::PreProcessToken
(
    JSContext  *pContext,
    UINT32      correctNumArgs,
    UINT32      actualNumArgs,
    const char *usage
)
{
    if (correctNumArgs != actualNumArgs)
    {
        string errMsg = Utility::StrPrintf(
            "%s: Passing %d args to a function that needs %d args",
            GetErrInfo().c_str(), actualNumArgs, correctNumArgs);
        JS_ReportError(pContext, errMsg.c_str());
        JS_ReportError(pContext, usage);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------
//! This method is used by the C_(XXX) wrappers after the token has
//! been processed.  It looks at the return value of the
//! token-processing routine, prints out any appropriate error
//! messages, and returns an appropriate value to the JavaScript
//! interpreter.
//!
JSBool PmParser::PostProcessToken
(
    JSContext  *pContext,
    jsval      *pReturlwalue,
    JSBool      status,
    const char *usage
)
{
    MASSERT(status == JS_TRUE);
    MASSERT(!JSVAL_IS_NULL(*pReturlwalue));
    if (status != JS_TRUE || JSVAL_IS_NULL(*pReturlwalue))
    {
        JS_ReportError(pContext, usage);
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    if (JSVAL_IS_INT(*pReturlwalue))
    {
        RC rc = JSVAL_TO_INT(*pReturlwalue);
        if (rc != OK)
        {
            string errMsg = GetErrInfo() + ": " + rc.Message();
            JS_ReportWarning(pContext, errMsg.c_str());
            JS_ReportWarning(pContext, usage);
            *pReturlwalue = JSVAL_NULL;
            return JS_FALSE;
        }
    }

    return JS_TRUE;
}

//////////////////////////////////////////////////////////////////////
// Policy tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS(Policy);

//--------------------------------------------------------------------
//! \brief Implement Policy.Version() token
//!
JSBool PmParser::Policy_Version(jsval version, jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(CheckForRootScope("Policy.Version"));
    C_CHECK_RC(FromJsval(version, &m_Version, 1));
    if (m_Version < LOWEST_VERSION || m_Version > HIGHEST_VERSION)
    {
        ErrPrintf("%s: Invalid version %d\n",
                  GetErrInfo().c_str(), m_Version);
        rc = RC::CANNOT_PARSE_FILE;
    }
    RETURN_RC(rc);
}

PM_BIND(Policy, Version, 1, "Usage: Policy.Version(version_number)",
        "Specify which version of policyfile syntax you're using");

//--------------------------------------------------------------------
//! \brief Implement Policy.Define() token
//!
JSBool PmParser::Policy_Define
(
    jsval trigger,
    jsval action,
    jsval *pReturlwalue
)
{
    PmEventManager *pEventMgr = GetEventManager();
    PmTrigger      *pTrigger;
    PmAction       *pAction = NULL;
    string          actBlockName;
    PmActionBlock  *pActionBlock = NULL;
    RC              rc;

    C_CHECK_RC(CheckForRootScope("Policy.Define"));
    C_CHECK_RC(FromJsval(trigger, &pTrigger, 1));
    if (FromJsval(action, &pAction, 0) == OK)
    {
        C_CHECK_RC(pEventMgr->AddTrigger(pTrigger, pAction));
    }
    else if (FromJsval(action, &pActionBlock, 0) == OK)
    {
        C_CHECK_RC(pEventMgr->AddTrigger(pTrigger, pActionBlock));
    }
    else if (FromJsval(action, &actBlockName, 0) == OK &&
             (pActionBlock = pEventMgr->GetActionBlock(actBlockName)) != NULL)
    {
        C_CHECK_RC(pEventMgr->AddTrigger(pTrigger, pActionBlock));
    }
    else
    {
        ErrPrintf("%s: Expected an action or ActionBlock name in arg 2\n",
                  GetErrInfo().c_str());
        rc = RC::CANNOT_PARSE_FILE;
    }
    RETURN_RC(rc);
}

PM_BIND(Policy, Define, 2,
        "Usage: Policy.Define(trigger, action|actionBlockName)",
        "Define a action to execute whenever the specified trigger fires.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetOutOfBand() token
//!
JSBool PmParser::Policy_SetOutOfBand(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandMemOp = false;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetOutOfBand, 0,
        "Usage: Policy.SetOutOfBand()",
        "Use out-of-band methods when possible (default).\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetInband() token
//!
JSBool PmParser::Policy_SetInband(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandMemOp = true;
    m_pPolicyManager->SetInband();

    RETURN_RC(OK);
}

PM_BIND(Policy, SetInband, 0,
        "Usage: Policy.SetInband()",
        "Use inband methods when possible.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetOutOfBandMemOp() token
//!
JSBool PmParser::Policy_SetOutOfBandMemOp(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandMemOp = false;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetOutOfBandMemOp, 0,
        "Usage: Policy.SetOutOfBandMemOp()",
        "Use out-of-band methods when possible for mem op actions (default).\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetInbandMemOp() token
//!
JSBool PmParser::Policy_SetInbandMemOp(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandMemOp = true;
    m_pPolicyManager->SetInband();
    RETURN_RC(OK);
}

PM_BIND(Policy, SetInbandMemOp, 0,
        "Usage: Policy.SetInbandMemOp()",
        "Use in-band methods when possible for mem op actions.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetOutOfBandPte() token
//!
JSBool PmParser::Policy_SetOutOfBandPte(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandPte = false;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetOutOfBandPte, 0,
        "Usage: Policy.SetOutOfBandPte()",
        "Use out-of-band methods when possible for PTE-related actions (default).\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetInbandPte() token
//!
JSBool PmParser::Policy_SetInbandPte(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandPte = true;
    m_pPolicyManager->SetInband();
    RETURN_RC(OK);
}

PM_BIND(Policy, SetInbandPte, 0,
        "Usage: Policy.SetInbandPte()",
        "Use in-band methods when possible for PTE-related actions.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetOutOfBandSurfaceMove() token
//!
JSBool PmParser::Policy_SetOutOfBandSurfaceMove(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandSurfaceMove = false;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetOutOfBandSurfaceMove, 0,
        "Usage: Policy.SetOutOfBandSurfaceMove()",
        "Use out-of-band methods when possible for surface move actions (default).\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetInbandSurfaceMove() token
//!
JSBool PmParser::Policy_SetInbandSurfaceMove(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandSurfaceMove = true;
    m_pPolicyManager->SetInband();
    RETURN_RC(OK);
}

PM_BIND(Policy, SetInbandSurfaceMove, 0,
        "Usage: Policy.SetInbandSurfaceMove()",
        "Use in-band methods when possible for surface move actions.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetOutOfBandSurfaceClear() token
//!
JSBool PmParser::Policy_SetOutOfBandSurfaceClear(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandSurfaceClear = false;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetOutOfBandSurfaceClear, 0,
        "Usage: Policy.SetOutOfBandSurfaceClear()",
        "Use out-of-band methods when possible for surface clear actions (default).\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetInbandSurfaceClear() token
//!
JSBool PmParser::Policy_SetInbandSurfaceClear(jsval *pReturlwalue)
{
    m_Scopes.top().m_InbandSurfaceClear = true;
    m_pPolicyManager->SetInband();
    RETURN_RC(OK);
}

PM_BIND(Policy, SetInbandSurfaceClear, 0,
        "Usage: Policy.SetInbandSurfaceClear()",
        "Use in-band methods when possible for surface clear actions.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetInbandChannel() token
//!
JSBool PmParser::Policy_SetInbandChannel
(
    jsval channelArg,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelArg, &pChannelDesc, 1));
    m_Scopes.top().m_pInbandChannel = pChannelDesc;
    m_pPolicyManager->SetInband();

    RETURN_RC(OK);
}

PM_BIND(Policy, SetInbandChannel, 1,
        "Usage: Policy.SetInbandChannel(channel)",
        "Set the channel to use for inband methods.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetOptimalAlloc() token
//!
JSBool PmParser::Policy_SetOptimalAlloc(jsval *pReturlwalue)
{
    m_Scopes.top().m_AllocLocation = Memory::Optimal;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetOptimalAlloc, 0,
        "Usage: Policy.SetOptimalAlloc()",
        "Alloc surfaces/pages from framebuffer, or noncoherent if zero-fb.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetFramebufferAlloc() token
//!
JSBool PmParser::Policy_SetFramebufferAlloc(jsval *pReturlwalue)
{
    m_Scopes.top().m_AllocLocation = Memory::Fb;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetFramebufferAlloc, 0,
        "Usage: Policy.SetFramebufferAlloc()",
        "Allocate surfaces/pages from framebuffer.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetCoherentAlloc() token
//!
JSBool PmParser::Policy_SetCoherentAlloc(jsval *pReturlwalue)
{
    m_Scopes.top().m_AllocLocation = Memory::Coherent;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetCoherentAlloc, 0,
        "Usage: Policy.SetCoherentAlloc()",
        "Allocate surfaces/pages from coherent system memory.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetNonCoherentAlloc() token
//!
JSBool PmParser::Policy_SetNonCoherentAlloc(jsval *pReturlwalue)
{
    m_Scopes.top().m_AllocLocation = Memory::NonCoherent;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetNonCoherentAlloc, 0,
        "Usage: Policy.SetNonCoherentAlloc()",
        "Allocate surfaces/pages from noncoherent system memory.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPhysContig() token
//!
JSBool PmParser::Policy_SetPhysContig(jsval *pReturlwalue)
{
    m_Scopes.top().m_PhysContig = true;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPhysContig, 0,
        "Usage: Policy.SetPhysContig()",
        "Allocate surfaces with physically-contiguous pages.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.ClearPhysContig() token
//!
JSBool PmParser::Policy_ClearPhysContig(jsval *pReturlwalue)
{
    m_Scopes.top().m_PhysContig = false;
    RETURN_RC(OK);
}

PM_BIND(Policy, ClearPhysContig, 0,
        "Usage: Policy.ClearPhysContig()",
        "Allocate surfaces with possibly physically-noncontiguous pages.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetAlignment() token
//!
JSBool PmParser::Policy_SetAlignment(jsval alignment, jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(FromJsval(alignment, &m_Scopes.top().m_Alignment, 1));
    RETURN_RC(OK);
}

PM_BIND(Policy, SetAlignment, 1,
        "Usage: Policy.SetAlignment(num)",
        "Allocate surfaces such that the address is a multiple of the indicated alignment.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.ClearAlignment() token
//!
JSBool PmParser::Policy_ClearAlignment(jsval *pReturlwalue)
{
    m_Scopes.top().m_Alignment = 0;
    RETURN_RC(OK);
}

PM_BIND(Policy, ClearAlignment, 0,
        "Usage: Policy.ClearAlignment()",
        "Allocate surfaces with any memory alignment.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetDualPageSize() token
//!
JSBool PmParser::Policy_SetDualPageSize(jsval *pReturlwalue)
{
    m_Scopes.top().m_DualPageSize = true;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetDualPageSize, 0,
        "Usage: Policy.SetDualPageSize()",
        "Allocate surfaces with both small and big PTEs.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.ClearDualPageSize() token
//!
JSBool PmParser::Policy_ClearDualPageSize(jsval *pReturlwalue)
{
    m_Scopes.top().m_DualPageSize = false;
    RETURN_RC(OK);
}

PM_BIND(Policy, ClearDualPageSize, 0,
        "Usage: Policy.ClearDualPageSize()",
        "Allocate surfaces with just the PTEs that match the surface's page size.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetLoopBack() token
//!
JSBool PmParser::Policy_SetLoopBack(jsval *pReturlwalue)
{
    m_Scopes.top().m_LoopBackPolicy = PolicyManager::LOOPBACK;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetLoopBack, 0,
        "Usage: Policy.SetLoopBack()",
        "Allocate surfaces/pages in loopback mode.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.ClearLoopBack() token
//!
JSBool PmParser::Policy_ClearLoopBack(jsval *pReturlwalue)
{
    m_Scopes.top().m_LoopBackPolicy = PolicyManager::NO_LOOPBACK;
    RETURN_RC(OK);
}

PM_BIND(Policy, ClearLoopBack, 0,
        "Usage: Policy.ClearLoopBack()",
        "Allocate surfaces/pages not in loopback mode.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetDeleteMovedSurfaces() token
//!
JSBool PmParser::Policy_SetDeleteMovedSurfaces(jsval *pReturlwalue)
{
    m_Scopes.top().m_MovePolicy = PolicyManager::DELETE_MEM;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetDeleteMovedSurfaces, 0,
        "Usage: Policy.SetDeleteMovedSurfaces()",
        "When a surface is moved, delete the old phys mem.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetScrambleMovedSurfaces() token
//!
JSBool PmParser::Policy_SetScrambleMovedSurfaces(jsval *pReturlwalue)
{
    m_Scopes.top().m_MovePolicy = PolicyManager::SCRAMBLE_MEM;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetScrambleMovedSurfaces, 0,
        "Usage: Policy.SetScrambleMovedSurfaces()",
        "When a surface is moved, scramble & delete the old phys mem.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetDumpMovedSurfaces() token
//!
JSBool PmParser::Policy_SetDumpMovedSurfaces(jsval *pReturlwalue)
{
    m_Scopes.top().m_MovePolicy = PolicyManager::DUMP_MEM;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetDumpMovedSurfaces, 0,
        "Usage: Policy.SetDumpMovedSurfaces()",
        "When a surface is moved, write a hex-dump of the old phys mem"
        " into the gild file at the end of the test.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetCrcMovedSurfaces() token
//!
JSBool PmParser::Policy_SetCrcMovedSurfaces(jsval *pReturlwalue)
{
    m_Scopes.top().m_MovePolicy = PolicyManager::CRC_MEM;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetCrcMovedSurfaces, 0,
        "Usage: Policy.SetCrcMovedSurfaces()",
        "When a surface is moved, write a CRC of the old phys mem"
        " into the gild file at the end of the test.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.DisablePlcOnSurfaceMove() token
//!
JSBool PmParser::Policy_DisablePlcOnSurfaceMove
(
    jsval aFlag,
    jsval *pReturlwalue
)
{
    bool flag;
    RC rc;

    C_CHECK_RC(FromJsval(aFlag, &flag, 1));
    m_Scopes.top().m_DisablePlcOnSurfaceMove = flag;

    RETURN_RC(OK);
}

PM_BIND(Policy, DisablePlcOnSurfaceMove, 1,
        "Usage: Policy.DisablePlcOnSurfaceMove(flag)",
        "Tell in-band move surface and move page actions whether to disable"
        " post L2 compression.  Default: false.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetLwrrentPageSize() token
//!
JSBool PmParser::Policy_SetLwrrentPageSize(jsval *pReturlwalue)
{
    m_Scopes.top().m_PageSize = PolicyManager::LWRRENT_PAGE_SIZE;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetLwrrentPageSize, 0,
        "Usage: Policy.SetLwrrentPageSize()",
        "When modifying PDE, modify the bits for the current mapped page-size\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetAllPageSizes() token
//!
JSBool PmParser::Policy_SetAllPageSizes(jsval *pReturlwalue)
{
    m_Scopes.top().m_PageSize = PolicyManager::ALL_PAGE_SIZES;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetAllPageSizes, 0,
        "Usage: Policy.SetAllPageSizes()",
        "When modifying PDE, modify the bits for all page sizes\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetBigPageSize() token
//!
JSBool PmParser::Policy_SetBigPageSize(jsval *pReturlwalue)
{
    m_Scopes.top().m_PageSize = PolicyManager::BIG_PAGE_SIZE;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetBigPageSize, 0,
        "Usage: Policy.SetBigPageSize()",
        "When modifying PDE, modify the big-page bits\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetSmallPageSize() token
//!
JSBool PmParser::Policy_SetSmallPageSize(jsval *pReturlwalue)
{
    m_Scopes.top().m_PageSize = PolicyManager::SMALL_PAGE_SIZE;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetSmallPageSize, 0,
        "Usage: Policy.SetSmallPageSize()",
        "When modifying PDE, modify the small-page bits\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetHugePageSize() token
//!
JSBool PmParser::Policy_SetHugePageSize(jsval *pReturlwalue)
{
    m_Scopes.top().m_PageSize = PolicyManager::HUGE_PAGE_SIZE;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetHugePageSize, 0,
        "Usage: Policy.SetHugePageSize()",
        "When modifying PDE, modify the huge-page bits\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPowerWaitModelUs() token
//!
JSBool PmParser::Policy_SetPowerWaitModelUs
(
    jsval aPowerWaitModelUs,
    jsval *pReturlwalue
)
{
    RC rc;
    C_CHECK_RC(FromJsval(aPowerWaitModelUs,
                         &m_Scopes.top().m_PowerWaitModelUs, 1));
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPowerWaitModelUs, 1,
        "Usage: Policy.SetPowerWaitModelUs(us)",
        "Set the power gate wait time when running from a model in us.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPowerWaitRTLUs() token
//!
JSBool PmParser::Policy_SetPowerWaitRTLUs
(
    jsval aPowerWaitRTLUs,
    jsval *pReturlwalue
)
{
    RC rc;
    C_CHECK_RC(FromJsval(aPowerWaitRTLUs,
                         &m_Scopes.top().m_PowerWaitRTLUs, 1));
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPowerWaitRTLUs, 1,
        "Usage: Policy.SetPowerWaitRTLUs(us)",
        "Set the power gate wait time when running from RTL in us.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPowerWaitHWUs() token
//!
JSBool PmParser::Policy_SetPowerWaitHWUs
(
    jsval aPowerWaitHWUs,
    jsval *pReturlwalue
)
{
    RC rc;
    C_CHECK_RC(FromJsval(aPowerWaitHWUs,
                         &m_Scopes.top().m_PowerWaitHWUs, 1));
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPowerWaitHWUs, 1,
        "Usage: Policy.SetPowerWaitHWUs(us)",
        "Set the power gate wait time when running from HW in us.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPowerWaitBusy() token
//!
JSBool PmParser::Policy_SetPowerWaitBusy(jsval *pReturlwalue)
{
    m_Scopes.top().m_PowerWaitBusy = true;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPowerWaitBusy, 0,
        "Usage: Policy.SetPowerWaitBusy()",
        "Use a busy wait when waiting for power gating.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPowerWaitSleep() token
//!
JSBool PmParser::Policy_SetPowerWaitSleep(jsval *pReturlwalue)
{
    m_Scopes.top().m_PowerWaitBusy = false;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPowerWaitSleep, 0,
        "Usage: Policy.SetPowerWaitSleep()",
        "Use a yielding sleep when waiting for power gating.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPStateFallbackError() token
//!
JSBool PmParser::Policy_SetPStateFallbackError(jsval *pReturlwalue)
{
    m_Scopes.top().m_PStateFallback =
            LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPStateFallbackError, 0,
        "Usage: Policy.SetPStateFallbackError()",
        "If setting the requested PState fails, return an error.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPStateFallbackLower() token
//!
JSBool PmParser::Policy_SetPStateFallbackLower(jsval *pReturlwalue)
{
    m_Scopes.top().m_PStateFallback =
            LW2080_CTRL_PERF_PSTATE_FALLBACK_LOWER_PERF;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPStateFallbackLower, 0,
        "Usage: Policy.SetPStateFallbackLower()",
        "If setting the requested PState fails, choose a lower performace"
        " state.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPStateFallbackHigher() token
//!
JSBool PmParser::Policy_SetPStateFallbackHigher(jsval *pReturlwalue)
{
    m_Scopes.top().m_PStateFallback =
            LW2080_CTRL_PERF_PSTATE_FALLBACK_HIGHER_PERF;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPStateFallbackHigher, 0,
        "Usage: Policy.SetPStateFallbackHigher()",
        "If setting the requested PState fails, choose a higher performace"
        " state.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetReqEventWaitMs() token
//!
JSBool PmParser::Policy_SetReqEventWaitMs
(
    jsval aReqEventWaitMs,
    jsval *pReturlwalue
)
{
    RC     rc;
    UINT32 reqEventWaitMs;

    if (m_Scopes.size() > 1)
    {
        ErrPrintf("Policy.SetReqEventWaitMs() may only be used at the global scope\n");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }
    C_CHECK_RC(FromJsval(aReqEventWaitMs, &reqEventWaitMs, 1));

    m_pPolicyManager->GetGilder()->SetReqEventTimeout(reqEventWaitMs);

    RETURN_RC(OK);
}

PM_BIND(Policy, SetReqEventWaitMs, 1,
        "Usage: Policy.SetReqEventWaitMs(ms)",
        "Set the required number of Ms to wait for required events at the end of a test.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetReqEventEnableSimTimeout(bEnable) token
//!
JSBool PmParser::Policy_SetReqEventEnableSimTimeout
(
    jsval aReqEventEnableSimTimeout,
    jsval *pReturlwalue
)
{
    RC     rc;
    bool reqEventEnableSimTimeout;

    if (m_Scopes.size() > 1)
    {
        ErrPrintf("Policy.SetReqEventEnableSimTimeout() may only be used at the global scope\n");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }
    C_CHECK_RC(FromJsval(aReqEventEnableSimTimeout,
                         &reqEventEnableSimTimeout, 1));

    PmGilder * pGilder = m_pPolicyManager->GetGilder();
    pGilder->SetReqEventEnableSimTimeout(reqEventEnableSimTimeout);

    RETURN_RC(OK);
}

PM_BIND(Policy, SetReqEventEnableSimTimeout, 1,
        "Usage: Policy.SetReqEventEnableSimTimeout(bEnable)",
        "Enable required event timeouts on simulation (default = disabled).\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetRandomSeedDefault() token
//!
JSBool PmParser::Policy_SetRandomSeedDefault
(
    jsval *pReturlwalue
)
{
    if (m_pPolicyManager->IsRandomSeedSet())
    {
        Printf(Tee::PriLow,
               "Policy.SetRandomSeedDefault() : Ignoring! Using random seed "
               "from command line\n");
    }
    else
    {
        m_Scopes.top().m_bUseSeedString = true;
    }
    RETURN_RC(OK);
}

PM_BIND(Policy, SetRandomSeedDefault, 0,
        "Usage: Policy.SetRandomSeedDefault()",
        "Use the default random seed for any subsequent random fancy pickers "
        "(action block/channel name).\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetRandomSeedTime() token
//!
JSBool PmParser::Policy_SetRandomSeedTime
(
    jsval *pReturlwalue
)
{
    if (m_pPolicyManager->IsRandomSeedSet())
    {
        Printf(Tee::PriLow,
               "Policy.SetRandomSeedTime() : Ignoring! Using random seed "
               "from command line\n");
    }
    else
    {
        m_Scopes.top().m_bUseSeedString = false;
        m_Scopes.top().m_RandomSeed =
            static_cast<UINT32>(Platform::GetTimeMS());
    }
    RETURN_RC(OK);
}

PM_BIND(Policy, SetRandomSeedTime, 0,
        "Usage: Policy.SetRandomSeedTime()",
        "Use the current time as the random seed for any subsequent random "
        "fancy pickers.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetRandomSeed() token
//!
JSBool PmParser::Policy_SetRandomSeed
(
    jsval aRandomSeed,
    jsval *pReturlwalue
)
{
    RC     rc;
    if (m_pPolicyManager->IsRandomSeedSet())
    {
        Printf(Tee::PriLow,
               "Policy.SetRandomSeed() : Ignoring! Using random seed "
               "from command line\n");
    }
    else
    {
        UINT32 randomSeed;
        C_CHECK_RC(FromJsval(aRandomSeed, &randomSeed, 1));
        m_Scopes.top().m_bUseSeedString = false;
        m_Scopes.top().m_RandomSeed = randomSeed;
    }
    RETURN_RC(rc);
}

PM_BIND(Policy, SetRandomSeed, 1,
        "Usage: Policy.SetRandomSeed(<seed>)",
        "Use the specified random seed for any subsequent random fancy "
        "pickers.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetClearNewSurfacesOn() token
//!
JSBool PmParser::Policy_SetClearNewSurfacesOn
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_ClearNewSurfaces = true;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetClearNewSurfacesOn, 0,
        "Usage: Policy.SetClearNewSurfacesOn()",
        "Subsequent Action.CreateSurface calls will 0 fill surfaces after"
        " creation.\n ");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetClearNewSurfacesOff() token
//!
JSBool PmParser::Policy_SetClearNewSurfacesOff
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_ClearNewSurfaces = false;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetClearNewSurfacesOff, 0,
        "Usage: Policy.SetClearNewSurfacesOff()",
        "Subsequent Action.CreateSurface calls will not intialize the "
        "contents of the surface.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateTarget() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateTarget
(
    jsval aAperture,
    jsval *pReturlwalue
)
{
    RC rc;

    Printf(Tee::PriWarn, "%s is not available in Policy file.", __FUNCTION__);
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateTarget, 1,
        "Usage: Policy.SetTlbIlwalidateTarget(aperture)",
        "Set the target for Action.TlbIlwalidate(). Default: Aperture.All().");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidatePdbAll() token
//!
JSBool PmParser::Policy_SetTlbIlwalidatePdbAll
(
    jsval aFlag,
    jsval *pReturlwalue
)
{
    bool flag;
    RC rc;

    C_CHECK_RC(FromJsval(aFlag, &flag, 1));
    m_Scopes.top().m_TlbIlwalidatePdbAll = flag;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidatePdbAll, 1,
        "Usage: Policy.SetTlbIlwalidatePdbAll(flag)",
        "Tell Action.TlbIlwalidate() whether to ilwalidate all PDBs or just one."
        "  Default: true.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateGpc() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateGpc
(
    jsval aFlag,
    jsval *pReturlwalue
)
{
    bool flag;
    RC rc;

    C_CHECK_RC(FromJsval(aFlag, &flag, 1));
    m_Scopes.top().m_TlbIlwalidateGpc = flag;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateGpc, 1,
        "Usage: Policy.SetTlbIlwalidateGpc(flag)",
        "Tell Action.TlbIlwalidate() whether to ilwalidate GPC-MMU TLBs."
        "  Default: true.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateReplayNone() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateReplayNone
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_TlbIlwalidateReplay =
        PolicyManager::TLB_ILWALIDATE_REPLAY_NONE;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateReplayNone, 0,
        "Usage: Policy.SetTlbIlwalidateReplayNone()",
        "Tell Action.TlbIlwalidate() not to attempt a replay."
        "  (This is the default).");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateReplayStart() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateReplayStart
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_TlbIlwalidateReplay =
        PolicyManager::TLB_ILWALIDATE_REPLAY_START;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateReplayStart, 0,
        "Usage: Policy.SetTlbIlwalidateReplayStart()",
        "Tell Action.TlbIlwalidate() to attempt a replay.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateReplayCancelGlobal() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateReplayCancelGlobal
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_TlbIlwalidateReplay =
        PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_GLOBAL;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateReplayCancelGlobal, 0,
        "Usage: Policy.SetTlbIlwalidateReplayCancelGlobal()",
        "Tell Action.TlbIlwalidate() to cancel all SM existing replay attempt.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateReplayCancelTargeted() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateReplayCancelTargeted
(
    jsval aGPCID,
    jsval aClientUnitID,
    jsval *pReturlwalue
)
{
    FancyPicker GPCID;
    FancyPicker clientUnitID;
    RC rc;

    C_CHECK_RC(GPCID.FromJsval(aGPCID));
    C_CHECK_RC(clientUnitID.FromJsval(aClientUnitID));
    m_Scopes.top().m_TlbIlwalidateReplay =
        PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED;
    (*m_Scopes.top().m_pGPCID) = GPCID;
    (*m_Scopes.top().m_pClientUnitID) = clientUnitID;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateReplayCancelTargeted, 2,
        "Usage: Policy.SetTlbIlwalidateReplayCancelTargeted()",
        "Tell Action.TlbIlwalidate() to cancel a targeted SM existing replay attempt.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateReplayCancelVaGlobal() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateReplayCancelVaGlobal
(
    jsval aAccessType,
    jsval aVEID,
    jsval *pReturlwalue
)
{
    RC rc;
    string accessType;
    UINT32 VEID;

    C_CHECK_RC(FromJsval(aAccessType, &accessType, 1));
    C_CHECK_RC(FromJsval(aVEID, &VEID, 2));

    if("VIRT_READ" == accessType)
        m_Scopes.top().m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_READ;
    else if ("VIRT_WRITE" == accessType)
        m_Scopes.top().m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_WRITE;
    else if ("VIRT_ATOMIC_STRONG" == accessType)
        m_Scopes.top().m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_STRONG;
    else if ("VIRT_RSVRVD" == accessType)
        m_Scopes.top().m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_RSVRVD;
    else if ("VIRT_ATOMIC_WEAK" == accessType)
        m_Scopes.top().m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_WEAK;
    else if ("VIRT_ATOMIC_ALL" == accessType)
        m_Scopes.top().m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_ALL;
    else if ("VIRT_ATOMIC_WRITE_AND_ATOMIC" == accessType)
        m_Scopes.top().m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_WRITE_AND_ATOMIC;
    else if ("VIRT_ALL" == accessType)
        m_Scopes.top().m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ALL;
    else
        m_Scopes.top().m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_NONE;

    m_Scopes.top().m_VEID = VEID;
    m_Scopes.top().m_TlbIlwalidateReplay =
        PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateReplayCancelVaGlobal, 2,
        "Usage: Policy.SetTlbIlwalidateReplayCancelVaGlobal()",
        "Tell Action.TlbIlwalidateVA() to cancel all SM existing replay attempt which "
        "virtual address, accessType, VEID can match.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateReplayStartAckAll() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateReplayStartAckAll
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_TlbIlwalidateReplay =
        PolicyManager::TLB_ILWALIDATE_REPLAY_START_ACK_ALL;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateReplayStartAckAll, 0,
        "Usage: Policy.SetTlbIlwalidateReplayStartAckAll()",
        "Tell Action.TlbIlwalidate() to attempt a replay with acknowledgment.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateSysmembar() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateSysmembar
(
    jsval aFlag,
    jsval *pReturlwalue
)
{
    bool flag;
    RC rc;

    C_CHECK_RC(FromJsval(aFlag, &flag, 1));
    m_Scopes.top().m_TlbIlwalidateSysmembar = flag;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateSysmembar, 1,
        "Usage: Policy.SetTlbIlwalidateSysmembar(flag)",
        "Tell Action.TlbIlwalidate() whether to enable sysmembar."
        "  Default: false.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateAckTypeNone() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateAckTypeNone
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_TlbIlwalidateAckType =
        PolicyManager::TLB_ILWALIDATE_ACK_TYPE_NONE;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateAckTypeNone, 0,
        "Usage: Policy.SetTlbIlwalidateAckTypeNone()",
        "Tell Action.TlbIlwalidate() to use ACK_TYPE_NONE."
        "  (This is the default).");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateAckTypeGlobally() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateAckTypeGlobally
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_TlbIlwalidateAckType =
        PolicyManager::TLB_ILWALIDATE_ACK_TYPE_GLOBALLY;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateAckTypeGlobally, 0,
        "Usage: Policy.SetTlbIlwalidateAckTypeGlobally()",
        "Tell Action.TlbIlwalidate() to use ACK_TYPE_GLOBALLY.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateAckTypeIntranode() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateAckTypeIntranode
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_TlbIlwalidateAckType =
        PolicyManager::TLB_ILWALIDATE_ACK_TYPE_INTRANODE;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateAckTypeIntranode, 0,
        "Usage: Policy.SetTlbIlwalidateAckTypeIntranode()",
        "Tell Action.TlbIlwalidate() to use ACK_TYPE_INTRANODE.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateChannelPdb() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateChannelPdb
(
    jsval channelArg,
    jsval *pReturlwalue
)
{
     PmChannelDesc * pChannelDesc;
     RC rc;

     C_CHECK_RC(FromJsval(channelArg, &pChannelDesc, 1));
     m_Scopes.top().m_pTlbIlwalidateChannel = pChannelDesc;
     RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateChannelPdb, 1,
        "Usage: Policy.SetTlbIlwalidateChannelPdb(channel)",
        "Tell Action.TlbIlwalidate() to ilwalidate one channel PDB.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateBar1() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateBar1
(
    jsval *pReturlwalue
)
{
     m_Scopes.top().m_TlbIlwalidateBar1 = true;
     RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateBar1, 0,
        "Usage: Policy.SetTlbIlwalidateBar1()",
        "Tell Action.TlbIlwalidate() to ilwalidate bar1 PDB.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateBar2() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateBar2
(
    jsval *pReturlwalue
)
{
     m_Scopes.top().m_TlbIlwalidateBar2 = true;
     RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateBar2, 0,
        "Usage: Policy.SetTlbIlwalidateBar2()",
        "Tell Action.TlbIlwalidate() to ilwalidate bar2 PDB.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateLevel() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateLevel
(
    jsval aLevelNum,
    jsval *pReturlwalue
)
{
     string levelNumString;
     RC rc;

     C_CHECK_RC(FromJsval(aLevelNum, &levelNumString, 1));

     m_Scopes.top().m_TlbIlwalidateLevelNum =
         ColwertStrToMmuLevelType(levelNumString);

     RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateLevel, 1,
        "Usage: Policy.SetTlbIlwalidateLevel()",
        "Tell Action.TlbIlwalidate() to ilwalidate level PDE.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetTlbIlwalidateIlwalScope() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateIlwalScope
(
    jsval aIlwalScope,
    jsval *pReturlwalue
)
{
     string ilwalScopeNumString;
     RC rc;

     C_CHECK_RC(FromJsval(aIlwalScope, &ilwalScopeNumString, 1));

     m_Scopes.top().m_TlbIlwalidateIlwalScope =
         ColwertStrToMmuIlwalScope(ilwalScopeNumString);

     RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateIlwalScope, 1,
        "Usage: Policy.SetTlbIlwalidateIlwalScope()",
        "Tell Action.TlbIlwalidate() which TLBs to ilwalidate.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetGpuCacheable() token
//!
JSBool PmParser::Policy_SetGpuCacheable
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_SurfaceGpuCacheMode = Surface2D::GpuCacheOn;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetGpuCacheable, 0,
        "Usage: Policy.SetGpuCacheable()",
        "Subsequent Action.CreateSurface calls will attempt to create"
        " GPU cacheable surfaces.\n ");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetNonGpuCacheable() token
//!
JSBool PmParser::Policy_SetNonGpuCacheable
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_SurfaceGpuCacheMode = Surface2D::GpuCacheOff;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetNonGpuCacheable, 0,
        "Usage: Policy.SetNonGpuCacheable()",
        "Subsequent Action.CreateSurface calls will create surfaces that"
        " do not cache.\n ");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetDefaultGpuCacheMode() token
//!
JSBool PmParser::Policy_SetDefaultGpuCacheMode
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_SurfaceGpuCacheMode = Surface2D::GpuCacheDefault;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetDefaultGpuCacheMode, 0,
        "Usage: Policy.SetDefaultGpuCacheMode()",
        "Subsequent Action.CreateSurface calls will use the default cache"
        " mode.\n ");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetChannelSubdeviceMask() token
//!
JSBool PmParser::Policy_SetChannelSubdeviceMask
(
    jsval aSubdevMask,
    jsval *pReturlwalue
)
{
    UINT32 sudevMask;
    RC rc;
    C_CHECK_RC(FromJsval(aSubdevMask, &sudevMask, 1));
    m_Scopes.top().m_ChannelSubdevMask = sudevMask;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetChannelSubdeviceMask, 1,
        "Usage: Policy.SetChannelSubdeviceMask(mask)",
        "Tell all actions that write methods the subdevice mask to use."
        "  Default: Channel::AllSubdevicesMask.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetChannelAllSubdevicesMask() token
//!
JSBool PmParser::Policy_SetChannelAllSubdevicesMask
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_ChannelSubdevMask = Channel::AllSubdevicesMask;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetChannelAllSubdevicesMask, 0,
        "Usage: Policy.SetChannelAllSubdevicesMask()",
        "Tell all actions that write methods to write to all subdevice.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetSurfaceAccessRemoteGpu() token
//!
JSBool PmParser::Policy_SetSurfaceAccessRemoteGpu
(
    jsval  gpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    m_Scopes.top().m_pRemoteGpuDesc = pGpuDesc;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetSurfaceAccessRemoteGpu, 1,
        "Usage: Policy.SetSurfaceAccessRemoteGpu(gpu)",
        "Set which remote device/subdevice to use when accessing surfaces."
        "  Default: No remote access");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetSurfaceAccessLocal() token
//!
JSBool PmParser::Policy_SetSurfaceAccessLocal
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_pRemoteGpuDesc = NULL;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetSurfaceAccessLocal, 0,
        "Usage: Policy.SetSurfaceAccessLocal()",
        "Use non-remote access to surfaces.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetSemaphoreReleaseWithTime() token
//!
JSBool PmParser::Policy_SetSemaphoreReleaseWithTime
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_SemRelFlags |= Channel::FlagSemRelWithTime;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetSemaphoreReleaseWithTime, 0,
        "Usage: Policy.SetSemaphoreReleaseWithTime()",
        "Release semaphores with a timestamp (default).");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetSemaphoreReleaseWithoutTime() token
//!
JSBool PmParser::Policy_SetSemaphoreReleaseWithoutTime
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_SemRelFlags &= ~Channel::FlagSemRelWithTime;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetSemaphoreReleaseWithoutTime, 0,
        "Usage: Policy.SetSemaphoreReleaseWithoutTime()",
        "Release semaphores without a timestamp.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetBlockingCtxSwInt() token
//!
JSBool PmParser::Policy_SetBlockingCtxSwInt
(
    jsval *pReturlwalue
)
{
    m_pPolicyManager->SetCtxSwIntMode(true);
    RETURN_RC(OK);
}

PM_BIND(Policy, SetBlockingCtxSwInt, 0,
        "Usage: Policy.SetBlockingCtxSwInt()",
        "The intterupt caused by the OnContextSwitch trigger will block "
        "the channel until the associated action block has exelwted(default).\n ");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetNonBlockingCtxSwInt() token
//!
JSBool PmParser::Policy_SetNonBlockingCtxSwInt
(
    jsval *pReturlwalue
)
{
    m_pPolicyManager->SetCtxSwIntMode(false);
    RETURN_RC(OK);
}

PM_BIND(Policy, SetNonBlockingCtxSwInt, 0,
        "Usage: Policy.SetNonBlockingCtxSwInt()",
        "The intterupt caused by the OnContextSwitch trigger will not block "
        "the channel.\n ");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetSemaphoreReleaseWithWFI() token
//!
JSBool PmParser::Policy_SetSemaphoreReleaseWithWFI
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_SemRelFlags |= Channel::FlagSemRelWithWFI;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetSemaphoreReleaseWithWFI, 0,
        "Usage: Policy.SetSemaphoreReleaseWithWFI()",
        "Release semaphores with a WFI (default).");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetSemaphoreReleaseWithoutWFI() token
//!
JSBool PmParser::Policy_SetSemaphoreReleaseWithoutWFI
(
    jsval *pReturlwalue
)
{
    m_Scopes.top().m_SemRelFlags &= ~Channel::FlagSemRelWithWFI;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetSemaphoreReleaseWithoutWFI, 0,
        "Usage: Policy.SetSemaphoreReleaseWithoutWFI()",
        "Release semaphores without a WFI.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetDeferTlbIlwalidate() token
//!
JSBool PmParser::Policy_SetDeferTlbIlwalidate
(
    jsval aFlag,
    jsval *pReturlwalue
)
{
    bool flag;
    RC rc;

    C_CHECK_RC(FromJsval(aFlag, &flag, 1));
    m_Scopes.top().m_DeferTlbIlwalidate = flag;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetDeferTlbIlwalidate, 1,
        "Usage: Policy.SetDeferTlbIlwalidate(flag)",
        "Tell Action.MoveSurfaces() whether to defer TLBs ilwalidate."
        "  Default: false.");

//--------------------------------------------------------------------
//! \brief Implement Policy.EnableNonStallInt() token
//!
JSBool PmParser::Policy_EnableNonStallInt
(
    jsval aEnableSemaphore,
    jsval aEnableInt,
    jsval *pReturlwalue
)
{
    bool enableSemaphore;
    bool enableInt;
    RC rc;

    C_CHECK_RC(FromJsval(aEnableSemaphore, &enableSemaphore, 1));
    C_CHECK_RC(FromJsval(aEnableInt, &enableInt, 1));
    m_Scopes.top().m_EnableNsiSemaphore = enableSemaphore;
    m_Scopes.top().m_EnableNsiInt = enableInt;
    RETURN_RC(OK);
}

PM_BIND(Policy, EnableNonStallInt, 2,
        "Usage: Policy.EnableNonStallInt(enableSemaphore, enableInt)",
        "Tell Action.NonStallInt() to write the semaphore,"
        " the int, or both.  Default: both.");

//--------------------------------------------------------------------
//! \brief Implement Policy.EnableIntermediateSurfaceMove() token
//!
JSBool PmParser::Policy_SetSurfaceVirtualLocMove
(
    jsval aSetSurfaceVirtualLocMove,
    jsval *pReturlwalue
)
{
    RC rc;
    bool bVirtualLocationMove;

    C_CHECK_RC(FromJsval(aSetSurfaceVirtualLocMove, &bVirtualLocationMove, 1));
    m_Scopes.top().m_MoveSurfInDefaultMMU = !bVirtualLocationMove;

    RETURN_RC(OK);
}

PM_BIND(Policy, SetSurfaceVirtualLocMove, 1,
        "Usage: Policy.SetSurfaceVirtualLocMove(true or false) ",
        "true: SurfaceMove Action will take effect on smmu vaspace of gpu surface\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetGmmuVASpace() token
//!
JSBool PmParser::Policy_SetGmmuVASpace(jsval *pReturlwalue)
{
    m_Scopes.top().m_AddrSpaceType = PolicyManager::GmmuVASpace;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetGmmuVASpace, 0,
        "Usage: Policy.Policy_SetGmmuVASpace() ",
        "Subsequent Action will take effect on gmmu vaspace\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetDefaultVASpace() token
//!
JSBool PmParser::Policy_SetDefaultVASpace(jsval *pReturlwalue)
{
    m_Scopes.top().m_AddrSpaceType = PolicyManager::DefaultVASpace;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetDefaultVASpace, 0,
        "Usage: Policy.SetDefaultVASpace()",
        "Subsequent Action will take effect on default vaspace\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetSurfaceAllocationType() token
//!
JSBool PmParser::Policy_SetSurfaceAllocationType
(
    jsval aSurfAllocType,
    jsval *pReturlwalue
)
{
    RC rc;
    UINT32 type;

    C_CHECK_RC(FromJsval(aSurfAllocType, &type, 1));
    m_Scopes.top().m_SurfaceAllocType = (Surface2D::VASpace)type;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetSurfaceAllocationType, 1,
        "Usage: Policy.SetSurfaceAllocationType()",
        "Set surface allocation type for Action.CreateSurface. "
        "Surface allocation type: "
        "DefaultVASpace     = 0 "
        "GPUVASpace         = 1\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetPhysicalPageSize() token
//!
JSBool PmParser::Policy_SetPhysicalPageSize(jsval aPageSize, jsval *pReturlwalue)
{
    RC rc;
    string pageSize;

    C_CHECK_RC(FromJsval(aPageSize, &pageSize, 1));
    if(strcmp(pageSize.c_str(), "HUGE") == 0 )
    m_Scopes.top().m_PhysPageSize = PolicyManager::HUGE_PAGE_SIZE;
    else if(strcmp(pageSize.c_str(), "BIG") == 0)
    m_Scopes.top().m_PhysPageSize = PolicyManager::BIG_PAGE_SIZE;
    else if(strcmp(pageSize.c_str(), "SMALL") == 0)
    m_Scopes.top().m_PhysPageSize = PolicyManager::SMALL_PAGE_SIZE;
    else
    {
        ErrPrintf("Policy.SetPhysicalPageSize() parsed an unsupported pagesize\n");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }
    RETURN_RC(OK);
}

PM_BIND(Policy, SetPhysicalPageSize, 1,
        "Usage: Policy.SetPhysicalPageSize()",
        "Subsequent Action will set the physical pagesize on Action.CreateSurface()\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetCEAllocAsync() token
//!
JSBool PmParser::Policy_SetCEAllocAsync
(
    jsval aCEInst,
    jsval *pReturlwalue
)
{
    RC rc;
    UINT32 index;
    FancyPicker ceInstNum;

    if (OK != ceInstNum.FromJsval(aCEInst))
    {
        C_CHECK_RC(FromJsval(aCEInst, &index, 1));
        C_CHECK_RC(ceInstNum.ConfigConst(index));
    }

    m_ScopeFancyPickers.push_back(ceInstNum);

    m_Scopes.top().m_FpContext.LoopNum = 0;
    m_Scopes.top().m_FpContext.RestartNum = 0;
    m_Scopes.top().m_FpContext.pJSObj = 0;
    m_ScopeFancyPickers.back().SetContext(&m_Scopes.top().m_FpContext);

    m_Scopes.top().m_pCEAllocInst = &(m_ScopeFancyPickers.back());
    m_Scopes.top().m_CEAllocMode = PolicyManager::CEAllocAsync;

    RETURN_RC(OK);
}

PM_BIND(Policy, SetCEAllocAsync, 1,
        "Usage: Policy.SetCEAllocAsync(FancyPicker)",
        "FancyPicker generates the CE num. "
        "CE instance: "
        "CE0     = 0 "
        "CE1     = 1 "
        "CE2     = 2 "
        "CE3     = 3\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetCEAllocDefault() token
//!
JSBool PmParser::Policy_SetCEAllocDefault(jsval *pReturlwalue)
{
    m_Scopes.top().m_pCEAllocInst = NULL;
    m_Scopes.top().m_CEAllocMode = PolicyManager::CEAllocDefault;

    RETURN_RC(OK);
}

PM_BIND(Policy, SetCEAllocDefault, 0,
        "Usage: Policy.SetCEAllocDefault()",
        "RM selects the CE engine instance.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetVaSpace() token
//!
JSBool PmParser::Policy_SetVaSpace
(
    jsval vaSpace,
    jsval *pReturlwalue
)
{
    RC rc;
    PmVaSpaceDesc * pVaSpaceDesc;

    C_CHECK_RC(FromJsval(vaSpace, &pVaSpaceDesc, 1));
    m_Scopes.top().m_pVaSpaceDesc = pVaSpaceDesc;

    RETURN_RC(OK);
}

PM_BIND(Policy, SetVaSpace, 1,
        "Usage: Policy.SetVaSpace(vaSpace)",
        "Set the Virtual address Space name to specify allocate at this VaSpace.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetSmcEngine() token
//!
JSBool PmParser::Policy_SetSmcEngine
(
    jsval smcEngine,
    jsval *pReturlwalue
)
{
    RC rc;
    PmSmcEngineDesc * pSmcEngineDesc;

    C_CHECK_RC(FromJsval(smcEngine, &pSmcEngineDesc, 1));
    m_Scopes.top().m_pSmcEngineDesc = pSmcEngineDesc;

    RETURN_RC(OK);
}

PM_BIND(Policy, SetSmcEngine, 1,
        "Usage: Policy.SetSmcEngine(smcEngine)",
        "Set the SmcEngine name to the specified smcEngine.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetChannelEngine() token
//!
JSBool PmParser::Policy_SetChannelEngine
(
    jsval aEngineName,
    jsval *pReturlwalue
)
{
    RC rc;
    string engineName;

    C_CHECK_RC(FromJsval(aEngineName, &engineName, 1));
    m_Scopes.top().m_DefEngineName = engineName;

    RETURN_RC(OK);
}

PM_BIND(Policy, SetChannelEngine, 1,
        "Usage: Policy.SetChannelEngine(engineName)",
        "Set the default Engine name to the specified Engine.\n");


//--------------------------------------------------------------------
//! \brief Implement Policy.SetTargetRunlist() token
//!
JSBool PmParser::Policy_SetTargetRunlist
(
    jsval channel,
    jsval *pReturlwalue
)
{
    RC rc;
    PmChannelDesc *pChannelDesc;

    C_CHECK_RC(FromJsval(channel, &pChannelDesc, 1));
    m_Scopes.top().m_pChannelDesc = pChannelDesc;

    RETURN_RC(OK);
}
PM_BIND(Policy, SetTargetRunlist, 1,
        "Usage: Policy.SetTargetRunlist(channel)",
        "Set the runlist according to the specified channel.\n");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetAccessCounterNotificationWaitMs() token
//!
JSBool PmParser::Policy_SetAccessCounterNotificationWaitMs
(
    jsval aWaitMs,
    jsval *pReturlwalue
)
{
    RC     rc;
    FLOAT64 waitMs = 0;

    if (m_Scopes.size() > 1)
    {
        ErrPrintf("Policy.SetAccessCounterNotificationWaitMs() can only be used at the global scope\n");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    C_CHECK_RC(FromJsval(aWaitMs, &waitMs, 1));

    m_pPolicyManager->SetAccessCounterNotificationWaitMs(waitMs);

    RETURN_RC(OK);
}

PM_BIND(Policy, SetAccessCounterNotificationWaitMs, 1,
        "Usage: Policy.SetAccessCounterNotificationWaitMs(ms)",
        "Change the time to check WRITE_NACK_TRUE to the specified time in milliseconds.\n");


//////////////////////////////////////////////////////////////////////
// Trigger tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS(Trigger);

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnChannelReset() token
//!

JSBool PmParser::Trigger_OnChannelReset(jsval *pReturlwalue)
{
    RETURN_OBJECT(new PmTrigger_OnChannelReset());
}

PM_BIND(Trigger, OnChannelReset, 0, "Usage:Trigger.OnChannelReset()",
        "Trigger that before RM channel reset to call back to Mods\n");

//--------------------------------------------------------------------
//! \brief Implement Trigger.Start() token
//!
JSBool PmParser::Trigger_Start(jsval *pReturlwalue)
{
    RETURN_OBJECT(new PmTrigger_Start());
}

PM_BIND(Trigger, Start, 0, "Usage: Trigger.Start()",
        "Trigger that fires at the start of the test");

//--------------------------------------------------------------------
//! \brief Implement Trigger.End() token
//!
JSBool PmParser::Trigger_End(jsval *pReturlwalue)
{
    RETURN_OBJECT(new PmTrigger_End());
}

PM_BIND(Trigger, End, 0, "Usage: Trigger.End()",
        "Trigger that fires at the end of the test");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnPageFault() token
//!
JSBool PmParser::Trigger_OnPageFault
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    // SetNonReplayableFaultBufferNeeded wil be ilwoked if necessary
    // in Trigger.StartTest
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_OBJECT(new PmTrigger_PageFault(pChannelDesc));
}

PM_BIND(Trigger, OnPageFault, 1,
        "Usage: Trigger.OnPageFault(channels)",
        "Trigger that fires when a page fault oclwrs.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnResetChannel() token
//!
JSBool PmParser::Trigger_OnResetChannel
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_OBJECT(new PmTrigger_RobustChannel(
                      pChannelDesc, RC::RM_RCH_RESETCHANNEL_VERIF_ERROR));
}

PM_BIND(Trigger, OnResetChannel, 1,
        "Usage: Trigger.OnResetChannel(channels)",
        "Trigger that fires when a channel is reset.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnChannelRemoval() token
//!
JSBool PmParser::Trigger_OnChannelRemoval
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_OBJECT(new PmTrigger_RobustChannel(
                      pChannelDesc, RC::RM_RCH_PREEMPTIVE_REMOVAL));
}

PM_BIND(Trigger, OnChannelRemoval, 1,
        "Usage: Trigger.OnChannelRemoval(channels)",
        "Trigger that fires when a channel is removing.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnGrErrorSwNotify() token
//!
JSBool PmParser::Trigger_OnGrErrorSwNotify
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_OBJECT(new PmTrigger_RobustChannel(
                      pChannelDesc, RC::RM_RCH_GR_ERROR_SW_NOTIFY));
}

PM_BIND(Trigger, OnGrErrorSwNotify, 1,
        "Usage: Trigger.OnGrErrorSwNotify(channels)",
        "Trigger that fires when a channel gets a graphics error sw notify.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnGrFaultDuringCtxsw() token
//!
JSBool PmParser::Trigger_OnGrFaultDuringCtxsw
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_OBJECT(new PmTrigger_RobustChannel(
                      pChannelDesc, RC::RM_RCH_GR_FAULT_DURING_CTXSW));
}

PM_BIND(Trigger, OnGrFaultDuringCtxsw, 1,
        "Usage: Trigger.OnGrFaultDuringCtxsw(channels)",
        "Trigger that fires when a chan gets a graphics fault during ctxsw.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnCe0Error() token
//!
JSBool PmParser::Trigger_OnCeError
(
 jsval channelDesc,
 jsval aCeNum,
 jsval *pReturlwalue
 )
{
    PmChannelDesc *pChannelDesc;
    string ceNumStr;
    RC rc;
    RC errorType;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aCeNum, &ceNumStr, 2));

    int ceNum;
    if (sscanf(ceNumStr.c_str(), "copy%d", &ceNum) != 1)
    {
        ErrPrintf("Not a valid parameter %s. Should be something like \"copy0\". %s\n",
                  ceNumStr.c_str(), __FUNCTION__);
        RETURN_RC(RC::BAD_PARAMETER);
    }

    if ((ceNum < 0) || (ceNum >= LW2080_ENGINE_TYPE_COPY_SIZE))
    {
        ErrPrintf("Unsupported CE number %s in %s", ceNumStr.c_str(), __FUNCTION__);
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    switch (ceNum)
    {
        case 0:
            errorType = RC::RM_RCH_CE0_ERROR;
            break;
        case 1:
            errorType = RC::RM_RCH_CE1_ERROR;
            break;
        case 2:
            errorType = RC::RM_RCH_CE2_ERROR;
            break;
        case 3:
            errorType = RC::RM_RCH_CE3_ERROR;
            break;
        case 4:
            errorType = RC::RM_RCH_CE4_ERROR;
            break;
        case 5:
            errorType = RC::RM_RCH_CE5_ERROR;
            break;
        case 6:
            errorType = RC::RM_RCH_CE6_ERROR;
            break;
        case 7:
            errorType = RC::RM_RCH_CE7_ERROR;
            break;
        case 8:
            errorType = RC::RM_RCH_CE8_ERROR;
            break;
        case 9:
            errorType = RC::RM_RCH_CE9_ERROR;
            break;
        default:
            ErrPrintf("Unsupported RC for CE number %s in %s", ceNumStr.c_str(), __FUNCTION__);
            RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    RETURN_OBJECT(new PmTrigger_RobustChannel(
        pChannelDesc, errorType));
}

PM_BIND(Trigger, OnCeError, 2,
        "Usage: Trigger.OnCeError(channels, ceNum)",
        "Trigger that fires when ce got an error.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnNonStallInt() token
//!
JSBool PmParser::Trigger_OnNonStallInt
(
    jsval aIntNames,
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    RegExp intNames;
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(aIntNames,   &intNames,     1, RegExp::FULL));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 2));
    RETURN_OBJECT(new PmTrigger_NonStallInt(intNames, pChannelDesc));
}

PM_BIND(Trigger, OnNonStallInt, 2,
        "Usage: Trigger.OnNonStallInt(intNameRegex, channels)",
        "Trigger that fires when a non-stalling interrupt oclwrs.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnSemaphoreRelease() token
//!
JSBool PmParser::Trigger_OnSemaphoreRelease
(
    jsval surfaceDesc,
    jsval aOffset,
    jsval aPayload,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    FancyPicker offsetPicker;
    UINT32 offset;
    FancyPicker payloadPicker;
    UINT64 payload;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    if (OK != offsetPicker.FromJsval(aOffset))
    {
        C_CHECK_RC(FromJsval(aOffset, &offset, 2));
        C_CHECK_RC(offsetPicker.ConfigConst(offset));
    }
    if (OK != payloadPicker.FromJsval(aPayload))
    {
        C_CHECK_RC(FromJsval(aPayload, &payload, 3));
        C_CHECK_RC(payloadPicker.ConfigConst64(payload));
    }

    PrintRandomSeed("Trigger_OnSemaphoreRelease");
    RETURN_OBJECT(new PmTrigger_SemaphoreRelease(pSurfaceDesc,
                                                 offsetPicker, payloadPicker,
                                                 m_Scopes.top().m_bUseSeedString,
                                                 m_Scopes.top().m_RandomSeed));
}

PM_BIND(Trigger, OnSemaphoreRelease, 3,
        "Usage: Trigger.OnSemaphoreRelease(surfaces, offset, payload)",
        "Trigger that fires when a semaphore is released.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnWaitForIdle() token
//!
JSBool PmParser::Trigger_OnWaitForIdle
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_OBJECT(new PmTrigger_OnWaitForIdle(pChannelDesc));
}

PM_BIND(Trigger, OnWaitForIdle, 1, "Usage: Trigger.OnWaitForIdle(channels)",
        "Trigger that fires when a wait for idle oclwrs"
        " on the indicated channels");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnWaitForChipIdle() token
//!
JSBool PmParser::Trigger_OnWaitForChipIdle(jsval *pReturlwalue)
{
    RETURN_OBJECT(new PmTrigger_OnWaitForChipIdle());
}

PM_BIND(Trigger, OnWaitForChipIdle, 0, "Usage: Trigger.OnWaitForChipIdle()",
        "Trigger that fires when the entire chip (all channels)"
        " are idled");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnMethodWrite() token
//!
JSBool PmParser::Trigger_OnMethodWrite(jsval channelDesc,
                                       jsval aPicker,
                                       jsval *pReturlwalue)
{
    PmChannelDesc *pChannelDesc;
    RC rc;
    FancyPicker    fancyPicker;

    C_CHECK_RC(fancyPicker.FromJsval(aPicker));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));

    PrintRandomSeed("Trigger.OnMethodWrite");
    RETURN_OBJECT(new PmTrigger_OnMethodWrite(pChannelDesc,
                                              fancyPicker,
                                              m_Scopes.top().m_bUseSeedString,
                                              m_Scopes.top().m_RandomSeed));
}

PM_BIND(Trigger, OnMethodWrite, 2, "Usage: Trigger.OnMethodWrite"
        "(channelRegEx, methodsFancyPicker)",
        "Trigger that fires methods matching the fancy picker are written "
        "on channels matching the regular expression");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnPercentMethodsWritten() token
//!
JSBool PmParser::Trigger_OnPercentMethodsWritten(jsval channelDesc,
                                                 jsval aPicker,
                                                 jsval *pReturlwalue)
{
    PmChannelDesc *pChannelDesc;
    RC rc;
    FancyPicker    fancyPicker;

    C_CHECK_RC(fancyPicker.FromJsval(aPicker));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));

    PrintRandomSeed("Trigger.OnPercentMethodsWritten");
    RETURN_OBJECT(new PmTrigger_OnPercentMethodsWritten(pChannelDesc,
                                                        fancyPicker,
                                                        m_Scopes.top().m_bUseSeedString,
                                                        m_Scopes.top().m_RandomSeed));
}

PM_BIND(Trigger, OnPercentMethodsWritten, 2,
        "Usage: Trigger.OnPercentMethodsWritten(channelRegEx, "
        "methodsFancyPicker)",
        "Trigger that fires when the percentage of methods matching the fancy "
        "picker are written on channels matching the regular expression");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnTimeUs() token
//!
JSBool PmParser::Trigger_OnTimeUs
(
    jsval aTimerName,
    jsval aHwPicker,
    jsval aModelPicker,
    jsval aRtlPicker,
    jsval *pReturlwalue
)
{
    RC rc;
    string TimerName;
    FancyPicker hwPicker;
    FancyPicker modelPicker;
    FancyPicker rtlPicker;

    C_CHECK_RC(FromJsval(aTimerName, &TimerName, 1));
    C_CHECK_RC(hwPicker.FromJsval(aHwPicker));
    C_CHECK_RC(modelPicker.FromJsval(aModelPicker));
    C_CHECK_RC(rtlPicker.FromJsval(aRtlPicker));

    C_CHECK_RC(CheckForIdentifier(TimerName, 1));
    PrintRandomSeed("Trigger.OnTimeUs");
    RETURN_OBJECT(new PmTrigger_OnTimeUs(TimerName,
                                         hwPicker, modelPicker, rtlPicker,
                                         m_Scopes.top().m_bUseSeedString,
                                         m_Scopes.top().m_RandomSeed));
}

PM_BIND(Trigger, OnTimeUs, 4,
        "Usage: Trigger.OnTimeUs(timerName, hwPicker, modelPicker, rtlPicker)",
        "Trigger that fires when the specfied time in us has elapsed."
        "  FancyPickers specify the times for HW, amodel/fmodel, and RTL.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnMethodExelwte() token
//!
JSBool PmParser::Trigger_OnMethodExelwte(jsval channelDesc,
                                         jsval aPicker,
                                         jsval *pReturlwalue)
{
    RC             rc;
    PmChannelDesc *pChannelDesc;
    FancyPicker    fancyPicker;

    C_CHECK_RC(fancyPicker.FromJsval(aPicker));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));

    PrintRandomSeed("Trigger.OnMethodExelwte");

    RETURN_OBJECT(CreateOnMethodExelwteObj(pChannelDesc,
                                           &fancyPicker,
                                           m_Scopes.top().m_bUseSeedString,
                                           m_Scopes.top().m_RandomSeed,
                                           true,
                                           true));
}
PM_BIND(Trigger, OnMethodExelwte, 2, "Usage: Trigger.OnMethodExelwte"
        "(channelRegEx, methodsFancyPicker)",
        "Trigger that fires methods matching the fancy picker are exelwted "
        "on channels matching the regular expression");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnMethodExelwteEx() token
//!
JSBool PmParser::Trigger_OnMethodExelwteEx(jsval channelDesc,
                                           jsval aPicker,
                                           jsval aWfiOnRelease,
                                           jsval aWaitEventHandled,
                                           jsval *pReturlwalue)
{
    RC             rc;
    PmChannelDesc *pChannelDesc;
    FancyPicker    fancyPicker;
    bool           bWfiOnRelease = true;
    bool           bWaitEventHandled = true;

    C_CHECK_RC(fancyPicker.FromJsval(aPicker));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aWfiOnRelease, &bWfiOnRelease, 3));
    C_CHECK_RC(FromJsval(aWaitEventHandled, &bWaitEventHandled, 4));

    PrintRandomSeed("Trigger.OnMethodExelwteEx");

    RETURN_OBJECT(CreateOnMethodExelwteObj(pChannelDesc,
                                           &fancyPicker,
                                           m_Scopes.top().m_bUseSeedString,
                                           m_Scopes.top().m_RandomSeed,
                                           bWfiOnRelease,
                                           bWaitEventHandled));
}

PM_BIND(Trigger, OnMethodExelwteEx, 4, "Usage: Trigger.OnMethodExelwteEx"
        "(channelRegEx, methodsFancyPicker, bWfiOnRelease, bWaitEventHandled)",
        "This is an extended version based on OnMethodExelwte()."
        "bWfiOnRelease = true: wfi before releasing semaphore value"
        "bWaitEventHandled = true: stop host processing until event is handled");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnPercentMethodsExelwted() token
//!
JSBool PmParser::Trigger_OnPercentMethodsExelwted(jsval channelDesc,
                                                  jsval aPicker,
                                                  jsval *pReturlwalue)
{
    RC             rc;
    PmChannelDesc *pChannelDesc;
    FancyPicker    fancyPicker;

    C_CHECK_RC(fancyPicker.FromJsval(aPicker));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));

    PrintRandomSeed("Trigger.OnPercentMethodsExelwted");

    RETURN_OBJECT(CreateOnPercentMethodsExelwtedObj(
                                           pChannelDesc,
                                           &fancyPicker,
                                           m_Scopes.top().m_bUseSeedString,
                                           m_Scopes.top().m_RandomSeed,
                                           true,
                                           true));

}
PM_BIND(Trigger, OnPercentMethodsExelwted, 2,
        "Usage: Trigger.OnPercentMethodsExelwted (channelRegEx, "
        "methodsFancyPicker)",
        "Trigger that fires when the percentage of methods matching the fancy "
        "picker are exelwted on channels matching the regular expression");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnPercentMethodsExelwtedEx() token
//!
JSBool PmParser::Trigger_OnPercentMethodsExelwtedEx(jsval channelDesc,
                                                    jsval aPicker,
                                                    jsval aWfiOnRelease,
                                                    jsval aWaitEventHandled,
                                                    jsval *pReturlwalue)
{
    RC             rc;
    PmChannelDesc *pChannelDesc;
    FancyPicker    fancyPicker;
    bool           bWfiOnRelease = true;
    bool           bWaitEventHandled = true;

    C_CHECK_RC(fancyPicker.FromJsval(aPicker));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aWfiOnRelease, &bWfiOnRelease, 3));
    C_CHECK_RC(FromJsval(aWaitEventHandled, &bWaitEventHandled, 4));

    PrintRandomSeed("Trigger.OnPercentMethodsExelwtedEx");

    RETURN_OBJECT(CreateOnPercentMethodsExelwtedObj(
                                               pChannelDesc,
                                               &fancyPicker,
                                               m_Scopes.top().m_bUseSeedString,
                                               m_Scopes.top().m_RandomSeed,
                                               bWfiOnRelease,
                                               bWaitEventHandled));
}

PM_BIND(Trigger, OnPercentMethodsExelwtedEx, 4,
        "Usage: Trigger.OnPercentMethodsExelwtedEx (channelRegEx, "
        "methodsFancyPicker, bWfiOnRelease, bWaitEventHandled)",
        "This is an extended version based on OnPercentMethodsExelwted()."
        "bWfiOnRelease = true: wfi before releasing semaphore value"
        "bWaitEventHandled = true: stop host processing until event is handled");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnMethodIdWrite() token
//!
JSBool PmParser::Trigger_OnMethodIdWrite(jsval aChannelDesc,
                                         jsval aClassIds,
                                         jsval aMethods,
                                         jsval aAfterWrite,
                                         jsval *pReturlwalue)
{
    PmChannelDesc *pChannelDesc;
    vector<UINT32> ClassIds;
    string classType;
    vector<UINT32> Methods;
    bool AfterWrite;
    RC rc;

    C_CHECK_RC(FromJsval(aChannelDesc,  &pChannelDesc, 1));
    if (JSVAL_IS_NUMBER(aClassIds))
    {
        ClassIds.resize(1);
        C_CHECK_RC(FromJsval(aClassIds, &ClassIds[0],  2));
    }
    else if (JSVAL_IS_STRING(aClassIds))
    {
        // User provided the class type string for MODS to automatically get 
        // the first supported class
        C_CHECK_RC(FromJsval(aClassIds, &classType, 2));
    }
    else
    {
        C_CHECK_RC(FromJsval(aClassIds, &ClassIds,     2));
    }
    if (JSVAL_IS_NUMBER(aMethods))
    {
        Methods.resize(1);
        C_CHECK_RC(FromJsval(aMethods,  &Methods[0],   3));
    }
    else
    {
        C_CHECK_RC(FromJsval(aMethods,  &Methods,      3));
    }
    C_CHECK_RC(FromJsval(aAfterWrite,   &AfterWrite,   4));

    RETURN_OBJECT(new PmTrigger_OnMethodIdWrite(pChannelDesc, ClassIds,
                                                classType,
                                                Methods, AfterWrite));
}

PM_BIND(Trigger, OnMethodIdWrite, 4, "Usage: Trigger.OnMethodWrite"
        "(channelDesc, class[es], method[s], afterWrite)",
        "Trigger that fires when the class/method is written to the channel's "
        "pushbuffer.  Trigger before the write if afterWrite is false.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnPmuElpgEvent() token
//!
JSBool PmParser::Trigger_OnPmuElpgEvent(jsval gpuDesc,
                                        jsval aEngineId,
                                        jsval aInterruptStatus,
                                        jsval *pReturlwalue)
{
    PmGpuDesc * pGpuDesc;
    UINT32      engineId;
    UINT32      interruptStatus;
    RC          rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(aEngineId, &engineId, 2));
    C_CHECK_RC(FromJsval(aInterruptStatus, &interruptStatus, 3));
    RETURN_OBJECT(new PmTrigger_OnPmuElpgEvent(pGpuDesc,
                                               engineId,
                                               interruptStatus));
}

PM_BIND(Trigger, OnPmuElpgEvent, 3, "Usage: Trigger.OnPmuElpgEvent"
        "(gpu, engine, interruptStatus)",
        "Trigger that fires when a matching PMU ELPG event oclwrs");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnMethodWrite() token
//!
JSBool PmParser::Trigger_OnRmEvent(jsval jsGpuDesc,
                                   jsval jsEventId,
                                   jsval *pReturlwalue)
{
    RC          rc;
    PmGpuDesc * pGpuDesc;
    UINT32      EventId;

    C_CHECK_RC(FromJsval(jsGpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(jsEventId, &EventId, 2));

    RETURN_OBJECT(new PmTrigger_OnRmEvent(pGpuDesc, EventId));
}

PM_BIND(Trigger, OnRmEvent, 2, "Usage: Trigger.OnRmEvent(gpu, EventId)",
        "Trigger that fires when RM issues a RM Event through ModsDrv function");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnTraceEventCpu() token
//!
JSBool PmParser::Trigger_OnTraceEventCpu(jsval jsTraceEventName,
                                         jsval jsAfterTraceEvent,
                                         jsval *pReturlwalue)
{
    RC          rc;
    string      traceEventName;
    bool        afterTraceEvent = false;

    C_CHECK_RC(FromJsval(jsTraceEventName, &traceEventName, 1));

    C_CHECK_RC(FromJsval(jsAfterTraceEvent, &afterTraceEvent, 2));

    RETURN_OBJECT(new PmTrigger_OnTraceEventCpu(traceEventName, afterTraceEvent));
}

PM_BIND(Trigger, OnTraceEventCpu, 2, "Usage: Trigger.OnTraceEventCpu(traceEventName, afterTraceEvent)",
        "Trigger that fires before/after trace command EVENT/PMTRIGGER_EVENT/PMTRIGGER_SYNC_EVENT");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnReplayablePageFault() token
//!
JSBool PmParser::Trigger_OnReplayablePageFault
(
    jsval aGpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    m_pPolicyManager->SetFaultBufferNeeded();

    C_CHECK_RC(FromJsval(aGpuDesc, &pGpuDesc, 1));
    RETURN_OBJECT(new PmTrigger_ReplayablePageFault(pGpuDesc));
}

PM_BIND(Trigger, OnReplayablePageFault, 1,
        "Usage: Trigger.OnReplayablePageFault(gpu)",
        "Trigger that fires when a replayable page fault oclwrs.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnCERecoverableFault() token
//!
JSBool PmParser::Trigger_OnCERecoverableFault
(
    jsval aGpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    m_pPolicyManager->SetNonReplayableFaultBufferNeeded();

    C_CHECK_RC(FromJsval(aGpuDesc, &pGpuDesc, 1));
    RETURN_OBJECT(new PmTrigger_CeRecoverableFault(pGpuDesc));
}

PM_BIND(Trigger, OnCERecoverableFault, 1,
        "Usage: Trigger.OnCERecoverableFault(gpu)",
        "Trigger that fires when a non-replayable recoverable CE fault oclwrs.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnFaultBufferOverflow() token
//!
JSBool PmParser::Trigger_OnFaultBufferOverflow
(
    jsval aGpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(aGpuDesc, &pGpuDesc, 1));
    RETURN_OBJECT(new PmTrigger_FaultBufferOverflow(pGpuDesc));
}

PM_BIND(Trigger, OnFaultBufferOverflow, 1,
        "Usage: Trigger.OnFaultBufferOverflow(gpu)",
        "Trigger that fires when a fault buffer overflow oclwrs.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnAccessCounterNotification() token
//!
JSBool PmParser::Trigger_OnAccessCounterNotification
(
    jsval aGpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    m_pPolicyManager->SetAccessCounterBufferNeeded();

    C_CHECK_RC(FromJsval(aGpuDesc, &pGpuDesc, 1));
    RETURN_OBJECT(new PmTrigger_AccessCounterNotification(pGpuDesc));
}

PM_BIND(Trigger, OnAccessCounterNotification, 1,
        "Usage: Trigger.OnAccessCounterNotification(gpu)",
        "Trigger that fires when a access counter notification oclwrs.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.PluginEventTrigger() token
//!
JSBool PmParser::Trigger_PluginEventTrigger(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(FromJsval(aName, &name, 1));
    RETURN_OBJECT(new PmTrigger_PluginEventTrigger(name));
}

PM_BIND(Trigger, PluginEventTrigger, 1, "Usage: Trigger.PluginEventTrigger(name)",
        "Trigger that fires at plugin event oclwrs");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnErrorLoggerInterrupt() token
//!
JSBool PmParser::Trigger_OnErrorLoggerInterrupt
(
    jsval aGpuDesc,
    jsval aName,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    string name;
    RC rc;

    C_CHECK_RC(FromJsval(aGpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(aName, &name, 2));

    m_pPolicyManager->AddErrorLoggerInterruptName(name);

    RETURN_OBJECT(new PmTrigger_OnErrorLoggerInterrupt(pGpuDesc, name));
}

PM_BIND(Trigger, OnErrorLoggerInterrupt, 2,
        "Usage: Trigger.OnErrorLoggerInterrupt(gpu,interruptString)",
        "Trigger that fires when an interrupt is reported to the error logger.");

//--------------------------------------------------------------------
//! \brief Implement Trigger.OnT3dPluginEvent() token
//!
JSBool PmParser::Trigger_OnT3dPluginEvent(jsval jsTraceEventName,
                                         jsval *pReturlwalue)
{
    RC          rc;
    string      traceEventName;

    C_CHECK_RC(FromJsval(jsTraceEventName, &traceEventName, 1));

    RETURN_OBJECT(new PmTrigger_OnT3dPluginEvent(traceEventName));
}

PM_BIND(Trigger, OnT3dPluginEvent, 1, "Usage: Trigger.OnT3dPluginEvent(traceEventName)",
        "Trigger that fires when MODS is sending a trace event to trace3d plugin.");

//////////////////////////////////////////////////////////////////////
// ActionBlock tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS(ActionBlock);

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.Define() token
//!
JSBool PmParser::ActionBlock_Define(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;
    C_CHECK_RC(CheckForRootScope("ActionBlock.Define"));
    C_CHECK_RC(FromJsval(aName, &name, 1));

    PushScope(Scope::ACTION_BLOCK);
    m_Scopes.top().m_pActionBlock = new PmActionBlock(GetEventManager(), name);
    RETURN_RC(rc);
}

PM_BIND(ActionBlock, Define, 1, "Usage: ActionBlock.Define(actionBlockName)",
        "Create an action block.  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.End() token
//!
JSBool PmParser::ActionBlock_End(jsval *pReturlwalue)
{
    RC rc;

    // Verify in either an action block or condition
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.End"));

    // Do whatever's appropriate to close the current ActionBlock.* command
    //
    switch (m_Scopes.top().m_Type)
    {
        case Scope::ACTION_BLOCK:
            C_CHECK_RC(GetEventManager()->AddActionBlock(
                           m_Scopes.top().m_pActionBlock));
            break;
        case Scope::CONDITION:
        case Scope::ELSE_BLOCK:
            m_Scopes.top().m_pGotoAction->SetTarget(
                m_Scopes.top().m_pActionBlock->GetSize());
            break;
        case Scope::ALLOW_OVERLAPPING_TRIGGERS:
            C_CHECK_RC(m_Scopes.top().m_pActionBlock->AddAction(
                           new PmAction_AllowOverlappingTriggers(false)));
            break;
        case Scope::MUTEX_CONDITION:
            m_Scopes.top().m_pGotoAction->SetTarget(
                m_Scopes.top().m_pActionBlock->GetSize());
        case Scope::MUTEX:
            C_CHECK_RC(m_Scopes.top().m_pActionBlock->AddAction(
                        new PmAction_ReleaseMutex(m_Scopes.top().m_MutexName)));
            break;
        default:
            MASSERT(!"Illegal case in PmParser::ActionBlock_End()");
    }
    m_Scopes.pop();
    RETURN_RC(rc);
}

PM_BIND(ActionBlock, End, 0, "Usage: ActionBlock.End()",
        "End the definition of an action block.  Must follow ActionBlock.Define,"
        " OnTriggerCount, OnTestNum, or AllowOverlappingTriggers.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.Else() token
//!
JSBool PmParser::ActionBlock_Else(jsval *pReturlwalue)
{
    RC rc;
    MASSERT(!m_Scopes.empty());

    if ((m_Scopes.top().m_Type != Scope::CONDITION) && 
            (m_Scopes.top().m_Type != Scope::MUTEX_CONDITION))
    {
        ErrPrintf("%s: ActionBlock.Else must be inside a conditional ActionBlock\n",
                  GetErrInfo().c_str());
        RETURN_RC(RC::CANNOT_PARSE_FILE);
    }

    if (m_Scopes.top().m_Type == Scope::MUTEX_CONDITION)
    {
        PmAction_ReleaseMutex *releaseMutexAction = 
            new PmAction_ReleaseMutex(m_Scopes.top().m_MutexName);
        C_CHECK_RC(m_Scopes.top().m_pActionBlock->AddAction(releaseMutexAction));
    }
    // Create a PmAction_Else action.  This will be used to skip over the else
    // block when the preceding condition is true.
    PmAction_Else *elseAction = new PmAction_Else();
    elseAction->SetSource(m_Scopes.top().m_pActionBlock->GetSize());
    C_CHECK_RC(m_Scopes.top().m_pActionBlock->AddAction(elseAction));
    // register tee code
    PolicyManagerMessageDebugger::Instance()->AddTeeModule(
        m_Scopes.top().m_pActionBlock->GetName(), elseAction->GetName());

    // Set the target of the conditional action to just after the else action.
    // This way when the condition is false, the else block will be exelwted.
    m_Scopes.top().m_pGotoAction->SetTarget(m_Scopes.top().m_pActionBlock->GetSize());

    // Pop the previous CONDITION scope and replace it with an
    // ELSE_BLOCK scope.
    m_Scopes.pop();
    PushScope(Scope::ELSE_BLOCK);

    // Save the else action so that the target can be set when ActionBlock.End
    // is encountered.
    m_Scopes.top().m_pGotoAction = elseAction;

    RETURN_RC(rc);
}

PM_BIND(ActionBlock, Else, 0, "Usage: ActionBlock.Else()",
    "End the definition of a conditional action block and start a new action "
    "block that will be exelwted when the original condition is false.  Must "
    "follow a conditional action block (e.g., ActionBlock.OnTriggerCount or "
    "ActionBlock.OnTestNum).");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnTriggerCount() token
//!
JSBool PmParser::ActionBlock_OnTriggerCount(jsval aPicker, jsval *pReturlwalue)
{
    RC rc;
    FancyPicker fancyPicker;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnTriggerCount"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(fancyPicker.FromJsval(aPicker));
    m_Scopes.top().m_pGotoAction = new PmAction_OnTriggerCount(fancyPicker,
                                                               m_Scopes.top().m_bUseSeedString,
                                                               m_Scopes.top().m_RandomSeed);
    m_Scopes.top().m_pGotoAction->SetSource(
            m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnTriggerCount");
    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnTriggerCount, 1, "Usage: ActionBlock.OnTriggerCount(fancyPicker)",
        "Create a conditional action block based on trigger count."
        "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnTestNum() token
//!
JSBool PmParser::ActionBlock_OnTestNum(jsval aPicker, jsval *pReturlwalue)
{
    RC rc;
    FancyPicker fancyPicker;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnTestNum"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(fancyPicker.FromJsval(aPicker));
    m_Scopes.top().m_pGotoAction = new PmAction_OnTestNum(fancyPicker,
                                                          m_Scopes.top().m_bUseSeedString,
                                                          m_Scopes.top().m_RandomSeed);
    m_Scopes.top().m_pGotoAction->SetSource(
            m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnTestNum");
    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnTestNum, 1, "Usage: ActionBlock.OnTestNum(fancyPicker)",
        "Create a conditional action block based on test number."
        "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnTestId() token
//!
JSBool PmParser::ActionBlock_OnTestId(jsval aTestId, jsval *pReturlwalue)
{
    RC rc;
    UINT32 testId;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnTestId"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(aTestId, &testId, 1));
    m_Scopes.top().m_pGotoAction = new PmAction_OnTestId(testId);
    m_Scopes.top().m_pGotoAction->SetSource(
            m_Scopes.top().m_pActionBlock->GetSize());
    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnTestId, 1, "Usage: ActionBlock.OnTestId(testId)",
        "Create a conditional action block based on test id."
        "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnSurfaceIsFaulting() token
//!
JSBool PmParser::ActionBlock_OnSurfaceIsFaulting(jsval surfaceDesc, jsval *pReturlwalue)
{
    RC rc;
    PmSurfaceDesc *pSurfaceDesc;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnSurfaceIsFaulting"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    m_Scopes.top().m_pGotoAction = new PmAction_OnSurfaceIsFaulting(
        pSurfaceDesc);
    m_Scopes.top().m_pGotoAction->SetSource(
        m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnSurfaceIsFaulting");

    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnSurfaceIsFaulting, 1,
    "Usage: ActionBlock.OnSurfaceIsFaulting(surface)",
    "Create a conditional action block that will execute if "
    "one of the designated surfaces is lwrrently faulting."
    "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnPageIsFaulting() token
//!
JSBool PmParser::ActionBlock_OnPageIsFaulting
(
    jsval surfaceDesc,
    jsval aOffset,
    jsval aSize,
    jsval *pReturlwalue
)
{
    RC rc;
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnPageIsFaulting"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    m_Scopes.top().m_pGotoAction = new PmAction_OnPageIsFaulting(
        pSurfaceDesc, offset, size);
    m_Scopes.top().m_pGotoAction->SetSource(
        m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnPageIsFaulting");

    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnPageIsFaulting, 3,
    "Usage: ActionBlock.OnPageIsFaulting(surface, offset, size)",
    "Create a conditional action block that will execute if "
    "the designated page is lwrrently faulting."
    "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnClientIDMatchesFault() token
//!
JSBool PmParser::ActionBlock_OnClientIDMatchesFault
(
    jsval aClientID,
    jsval *pReturlwalue
)
{
    RC rc;
    UINT32 ClientID;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnClientIDMatchesFault"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(aClientID, &ClientID, 1));
    m_Scopes.top().m_pGotoAction = new PmAction_OnClientIDMatchesFault(ClientID);
    m_Scopes.top().m_pGotoAction->SetSource(
        m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnClientIDMatchesFault");

    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnClientIDMatchesFault, 1,
    "Usage: ActionBlock.OnClientIDMatchesFault(ClientID)",
    "Create a conditional action block that will execute if "
    "the designated client ID matches the client ID of the current replayable fault."
    "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnFaultTypeMatchesFault() token
//!
JSBool PmParser::ActionBlock_OnFaultTypeMatchesFault
(
    jsval aFaultType,
    jsval *pReturlwalue
)
{
    RC rc;
    string faultType;

    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnFaultTypeMatchesFault"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(aFaultType, &faultType, 1));
    m_Scopes.top().m_pGotoAction = new PmAction_OnFaultTypeMatchesFault(faultType);
    m_Scopes.top().m_pGotoAction->SetSource(
        m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnFaultTypeMatchesFault");

    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnFaultTypeMatchesFault, 1,
    "Usage: ActionBlock.OnFaultTypeMatchesFault(FaultType)",
    "Create a conditional action block that will execute if "
    "the designated fault type matches the fault type of the current replayable fault."
    "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnGPCIDMatchesFault() token
//!
JSBool PmParser::ActionBlock_OnGPCIDMatchesFault
(
    jsval aGPCID,
    jsval *pReturlwalue
)
{
    RC rc;
    UINT32 GPCID;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnGPCIDMatchesFault"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(aGPCID, &GPCID, 1));
    m_Scopes.top().m_pGotoAction = new PmAction_OnGPCIDMatchesFault(GPCID);
    m_Scopes.top().m_pGotoAction->SetSource(
        m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnGPCIDMatchesFault");

    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnGPCIDMatchesFault, 1,
    "Usage: ActionBlock.OnGPCIDMatchesFault(GPCID)",
    "Create a conditional action block that will execute if "
    "the designated GPCID matches the GPCID of the current replayable fault."
    "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OlwEIDMatchesFault() token
//!
JSBool PmParser::ActionBlock_OlwEIDMatchesFault
(
    jsval aTsgName,
    jsval aVEID,
    jsval *pReturlwalue
)
{
    RC rc;
    string tsgName;
    UINT32 VEID;

    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OlwEIDMatchesFault"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(aTsgName, &tsgName, 1));
    C_CHECK_RC(FromJsval(aVEID, &VEID, 2));
    m_Scopes.top().m_pGotoAction = new PmAction_OlwEIDMatchesFault(tsgName, VEID, m_Scopes.top().m_pSmcEngineDesc);
    m_Scopes.top().m_pGotoAction->SetSource(
        m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OlwEIDMatchesFault");

    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OlwEIDMatchesFault, 2,
    "Usage: ActionBlock.OlwEIDMatchesFault(tsgName, VEID)",
    "Create a conditional action block that will execute if "
    "the designated VEID in this tsg matches the VEID in faulting tsg of the current replayable fault."
    "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnNotificationFromSurface() token
//!
JSBool PmParser::ActionBlock_OnNotificationFromSurface(jsval surfaceDesc, jsval *pReturlwalue)
{
    RC rc;
    PmSurfaceDesc *pSurfaceDesc;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnNotificationFromSurface"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    m_Scopes.top().m_pGotoAction = new PmAction_OnNotificationFromPage(
        pSurfaceDesc, 0, 0);
    m_Scopes.top().m_pGotoAction->SetSource(
        m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnNotificationFromSurface");

    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnNotificationFromSurface, 1,
    "Usage: ActionBlock.OnNotificationFromSurface(surface)",
    "Create a conditional action block that will execute if "
    "one of the designated surfaces has sent a notification."
    "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnNotificationFromPage() token
//!
JSBool PmParser::ActionBlock_OnNotificationFromPage
(
    jsval surfaceDesc,
    jsval aOffset,
    jsval aSize,
    jsval *pReturlwalue
)
{
    RC rc;
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnNotificationFromPage"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    m_Scopes.top().m_pGotoAction = new PmAction_OnNotificationFromPage(
        pSurfaceDesc, offset, size);
    m_Scopes.top().m_pGotoAction->SetSource(
        m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnNotificationFromPage");

    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnNotificationFromPage, 3,
    "Usage: ActionBlock.OnNotificationFromPage(surface, offset, size)",
    "Create a conditional action block that will execute if "
    "the designated page has sent a notification."
    "  Must have a corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.AllowOverlappingTriggers() token
//!
JSBool PmParser::ActionBlock_AllowOverlappingTriggers(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.AllowOverlappingTriggers"));

    PushScope(Scope::ALLOW_OVERLAPPING_TRIGGERS);
    RETURN_ACTION(new PmAction_AllowOverlappingTriggers(true));
}

PM_BIND(ActionBlock, AllowOverlappingTriggers, 0,
        "Usage: ActionBlock.AllowOverlappingTriggers()",
        "Allow other triggers to fire in the middle of actions,"
        "  until the corresponding ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnAtsIsEnabledOnChannel() token
//!
JSBool PmParser::ActionBlock_OnAtsIsEnabledOnChannel(jsval channelDesc, jsval *pReturlwalue)
{
    RC rc;
    PmChannelDesc *pChannelDesc;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnAtsIsEnabledOnChannel"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    m_Scopes.top().m_pGotoAction = new PmAction_OnAtsIsEnabledOnChannel(
            pChannelDesc);
    m_Scopes.top().m_pGotoAction->SetSource(
        m_Scopes.top().m_pActionBlock->GetSize());
    PrintRandomSeed("ActionBlock.OnAtsIsEnabledOnChannel");

    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnAtsIsEnabledOnChannel, 1,
    "Usage: ActionBlock.OnAtsIsEnabledOnChannel(channels)",
    "Create a conditional action block that will execute if "
    "any of the specified channels support ATS."
    "  Must have a corresponding ActionBlock.End.");

//////////////////////////////////////////////////////////////////////
// Action tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS(Action);

//--------------------------------------------------------------------
//! \brief Implement Action.Block() token
//!
//! Unlike other Action.* tokens, this one returns an ActionBlock
//! instead of an Action.  It's only designed to be used in
//! Policy.Define() statements, so that the second argument can always
//! start with "Action.".
//!
JSBool PmParser::Action_Block
(
    jsval aName,
    jsval *pReturlwalue
)
{
    string name;
    RC rc;

    C_CHECK_RC(FromJsval(aName, &name, 1));
    PmActionBlock *pActionBlock = GetEventManager()->GetActionBlock(name);
    if (pActionBlock == NULL)
    {
        ErrPrintf("%s: \"%s\" is not a valid ActionBlock name\n",
                  GetErrInfo().c_str(), name.c_str());
        return RC::BAD_PARAMETER;
    }

    if (OK == CheckForActionBlockScope(""))
    {
        RETURN_ACTION(new PmAction_Block(pActionBlock));
    }
    else
    {
        RETURN_OBJECT(pActionBlock);
    }
}

PM_BIND(Action, Block, 1, "Usage: Action.Block(name)",
        "An ActionBlock that was defined by a previous"
        " ActionBlock.Define/ActionBlock.End.");

//--------------------------------------------------------------------
//! \brief Implement Action.Print() token
//!
JSBool PmParser::Action_Print
(
    jsval aPrintString,
    jsval *pReturlwalue
)
{
    string printString;
    RC rc;

    C_CHECK_RC(FromJsval(aPrintString, &printString, 1));
    RETURN_ACTION(new PmAction_Print(printString));
}

PM_BIND(Action, Print, 1, "Usage: Action.Print(string)",
        "Print a string.  Used for debugging.");

//--------------------------------------------------------------------
//! \brief Implement Action.TriggerUtlUserEvent() token
//!
JSBool PmParser::Action_TriggerUtlUserEvent
(
    jsval aData,
    jsval *pReturlwalue
)
{
    vector<string> data;
    RC rc;

    C_CHECK_RC(FromJsval(aData, &data, 1));
    if (data.size() % 2)
    {
        ErrPrintf("TriggerUtlUserEvent input list size must be a multiple of 2."
            "The list will be forwarded to UTL as key-value pairs\n");
        return RC::BAD_PARAMETER;
    }
    RETURN_ACTION(new PmAction_TriggerUtlUserEvent(data));
}

PM_BIND(Action, TriggerUtlUserEvent, 1, "Usage: Action.TriggerUtlUserEvent([key1, value1, key2...])",
        "Trigger UserEvent in UTL and transport data to the same event.");
//--------------------------------------------------------------------
//! \brief Implement Action.UnmapSurfaces() token
//!
JSBool PmParser::Action_UnmapSurfaces
(
    jsval  surfaceDesc,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    RETURN_ACTION(new PmAction_UnmapSurfaces(
        pSurfaceDesc,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_AddrSpaceType,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, UnmapSurfaces, 1, "Usage: Action.UnmapSurfaces(surfaces)",
        "Unmap the indicated surfaces.");

//--------------------------------------------------------------------
//! \brief Implement Action.UnmapPages() token
//!
JSBool PmParser::Action_UnmapPages
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_UnmapPages(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_AddrSpaceType,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, UnmapPages, 3,
        "Usage: Action.UnmapPages(surfaces, offset, size)",
        "Unmap pages within some surfaces");

//--------------------------------------------------------------------
//! \brief Implement Action.UnmapPde() token
//!
JSBool PmParser::Action_UnmapPde
(
    jsval aSurfaceDesc,
    jsval aOffset,
    jsval aSize,
    jsval aLevelNum,
    jsval *pReturlwalue
)
{
     PmSurfaceDesc * pSurfaceDesc;
     UINT64 offset;
     UINT64 size;
     string levelNum;
     RC rc;

     C_CHECK_RC(FromJsval(aSurfaceDesc, &pSurfaceDesc, 1));
     C_CHECK_RC(FromJsval(aOffset, &offset, 2));
     C_CHECK_RC(FromJsval(aSize, &size, 3));
     C_CHECK_RC(FromJsval(aLevelNum, &levelNum, 4));

     RETURN_ACTION(new PmAction_UnmapPde(pSurfaceDesc, offset, size,
                                         ColwertStrToMmuLevelType(levelNum),
                                         m_Scopes.top().m_InbandPte,
                                         m_Scopes.top().m_pInbandChannel,
                                         m_Scopes.top().m_AddrSpaceType,
                                         m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, UnmapPde, 4,
        "Usage: Action.UnmapPde(surfaces, offset, size, levelNum)",
        "Unmap pde within some surfaces.");

//--------------------------------------------------------------------
//! \brief Implement Action.SparsifyPages() token
//!
JSBool PmParser::Action_SparsifyPages
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SparsifyPages(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_AddrSpaceType,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SparsifyPages, 3,
        "Usage: Action.SparsifyPages(surfaces, offset, size)",
        "Sparsify pages within some surfaces");

//--------------------------------------------------------------------
//! \brief Implement Action.RemapSurfaces() token
//!
JSBool PmParser::Action_RemapSurfaces
(
    jsval  surfaceDesc,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    RETURN_ACTION(new PmAction_RemapSurfaces(
        pSurfaceDesc,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_AddrSpaceType,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, RemapSurfaces, 1, "Usage: Action.RemapSurfaces(surfaces)",
        "Remap the indicated surfaces.");

//--------------------------------------------------------------------
//! \brief Implement Action.RemapPages() token
//!
JSBool PmParser::Action_RemapPages
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_RemapPages(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_AddrSpaceType,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, RemapPages, 3,
        "Usage: Action.RemapPages(surfaces, offset, size)",
        "Remap pages within some surfaces");

//--------------------------------------------------------------------
//! \brief Implement Action.RemapPde() token
//!
JSBool PmParser::Action_RemapPde
(
    jsval aSurfaceDesc,
    jsval aOffset,
    jsval aSize,
    jsval aLevelNum,
    jsval *pReturlwalue
)
{
     PmSurfaceDesc * pSurfaceDesc;
     UINT64 offset;
     UINT64 size;
     string levelNum;
     RC rc;

     C_CHECK_RC(FromJsval(aSurfaceDesc, &pSurfaceDesc, 1));
     C_CHECK_RC(FromJsval(aOffset, &offset, 2));
     C_CHECK_RC(FromJsval(aSize, &size, 3));
     C_CHECK_RC(FromJsval(aLevelNum, &levelNum, 4));

     RETURN_ACTION(new PmAction_RemapPde(pSurfaceDesc, offset, size,
                                         ColwertStrToMmuLevelType(levelNum),
                                         m_Scopes.top().m_InbandPte,
                                         m_Scopes.top().m_pInbandChannel,
                                         m_Scopes.top().m_AddrSpaceType,
                                         m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, RemapPde, 4,
        "Usage: Action.RemapPde(surfaces, offset, size, levelNum)",
        "Remap pde within some surfaces.");

//--------------------------------------------------------------------
//! \brief Implement Action.RandomRemapIlwalidPages() token
//!
JSBool PmParser::Action_RandomRemapIlwalidPages
(
    jsval  surfaceDesc,
    jsval  aPercentage,
    jsval *pReturlwalue
)
{
    RC rc;
    UINT32 percentage;
    PmSurfaceDesc *pSurfaceDesc;
    FancyPicker    percentageFancyPicker;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));

    if (OK != percentageFancyPicker.FromJsval(aPercentage))
    {
        C_CHECK_RC(FromJsval(aPercentage, &percentage, 2));
        C_CHECK_RC(percentageFancyPicker.ConfigConst(percentage));
    }

    RETURN_ACTION(new PmAction_RandomRemapIlwalidPages(
        pSurfaceDesc,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_AddrSpaceType,
        percentageFancyPicker,
        m_Scopes.top().m_RandomSeed,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, RandomRemapIlwalidPages, 2,
        "Usage: Action.RandomRemapIlwalidPages(surfaces, percentage)",
        "Randomly remap a percentage of invalid pages in the indicated surfaces.");

//--------------------------------------------------------------------
//! \brief Implement Action.ChangePageSize() token
//!
JSBool PmParser::Action_ChangePageSize
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval  aPageSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    string strPagesize;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aPageSize, &strPagesize, 4));

    PolicyManager::PageSize targetPageSize;
    if (0 == Utility::strcasecmp(strPagesize.c_str(), "BIG"))
    {
        targetPageSize = PolicyManager::BIG_PAGE_SIZE;
    }
    else if (0 == Utility::strcasecmp(strPagesize.c_str(), "SMALL"))
    {
        targetPageSize = PolicyManager::SMALL_PAGE_SIZE;
    }
    else if (0 == Utility::strcasecmp(strPagesize.c_str(), "HUGE"))
    {
        targetPageSize = PolicyManager::HUGE_PAGE_SIZE;
    }
    else
    {
        ErrPrintf("Unknown Pagesize string %s. Only 'BIG', 'SMALL' and 'HUGE' are supported\n",
                  strPagesize.c_str());
        RETURN_RC(RC::ILWALID_ARGUMENT);
    }

    RETURN_ACTION(new PmAction_ChangePageSize(
        pSurfaceDesc,
        targetPageSize,
        offset,
        size,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_AddrSpaceType,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, ChangePageSize, 4,
        "Usage: Action.ChangePageSize(surfaces, offset, size, pagesize)",
        "Change page size of the pages within some surfaces");

//--------------------------------------------------------------------
//! \brief Implement Action.RemapFaultingSurface() token
//!
JSBool PmParser::Action_RemapFaultingSurface(jsval *pReturlwalue)
{
    RETURN_ACTION(new PmAction_RemapFaultingSurface(
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, RemapFaultingSurface, 0,
        "Usage: Action.RemapFaultingSurface()",
        "Remap the surface that just got a fault.");

//--------------------------------------------------------------------
//! \brief Implement Action.RemapFaultingPage() token
//!
JSBool PmParser::Action_RemapFaultingPage(jsval *pReturlwalue)
{
    RETURN_ACTION(new PmAction_RemapFaultingPage(
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, RemapFaultingPage, 0,
        "Usage: Action.RemapFaultingPage()",
        "Remap the page that just got a fault.");

//--------------------------------------------------------------------
//! \brief Implement Action.CreateVaSpace() token
//!
JSBool PmParser::Action_CreateVaSpace
(
    jsval  aName,
    jsval  aGpuDesc,
    jsval *pReturlwalue
)
{
    string name;
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(aName,    &name,     1));
    C_CHECK_RC(FromJsval(aGpuDesc, &pGpuDesc, 2));
    C_CHECK_RC(CheckForIdentifier(name, 1));
    RETURN_ACTION(new PmAction_CreateVaSpace(name, pGpuDesc));
}

PM_BIND(Action, CreateVaSpace, 2,
        "Usage: Action.CreateVaSpace(name, gpu)",
        "Create a vaspace on the indicated GPU.");

//--------------------------------------------------------------------
//! \brief Implement Action.CreateSurface() token
//!
JSBool PmParser::Action_CreateSurface
(
    jsval  aName,
    jsval  aSize,
    jsval  aGpuDesc,
    jsval *pReturlwalue
)
{
    string name;
    UINT32 size;
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(aName,    &name,     1));
    C_CHECK_RC(FromJsval(aSize,    &size,     2));
    C_CHECK_RC(FromJsval(aGpuDesc, &pGpuDesc, 3));
    C_CHECK_RC(CheckForIdentifier(name, 1));
    RETURN_ACTION(new PmAction_CreateSurface(name, size, pGpuDesc,
                                             m_Scopes.top().m_ClearNewSurfaces,
                                             m_Scopes.top().m_AllocLocation,
                                             m_Scopes.top().m_PhysContig,
                                             m_Scopes.top().m_Alignment,
                                             m_Scopes.top().m_DualPageSize,
                                             m_Scopes.top().m_LoopBackPolicy,
                                             m_Scopes.top().m_SurfaceGpuCacheMode,
                                             m_Scopes.top().m_PageSize,
                                             m_Scopes.top().m_GpuSmmuMode,
                                             m_Scopes.top().m_SurfaceAllocType,
                                             m_Scopes.top().m_InbandSurfaceClear,
                                             m_Scopes.top().m_pInbandChannel,
                                             m_Scopes.top().m_PhysPageSize,
                                             m_Scopes.top().m_pVaSpaceDesc,
                                             m_Scopes.top().m_pSmcEngineDesc));
}

PM_BIND(Action, CreateSurface, 3,
        "Usage: Action.CreateSurface(name, size, gpu)",
        "Create a surface on the indicated GPU.");

//--------------------------------------------------------------------
//! \brief Implement Action.MoveSurfaces() token
//!
JSBool PmParser::Action_MoveSurfaces
(
    jsval  surfaceDesc,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    RETURN_ACTION(new PmAction_MoveSurfaces(pSurfaceDesc,
                                            m_Scopes.top().m_AllocLocation,
                                            m_Scopes.top().m_MovePolicy,
                                            m_Scopes.top().m_LoopBackPolicy,
                                            m_Scopes.top().m_DeferTlbIlwalidate,
                                            m_Scopes.top().m_InbandSurfaceMove,
                                            m_Scopes.top().m_pInbandChannel,
                                            m_Scopes.top().m_MoveSurfInDefaultMMU,
                                            m_Scopes.top().m_SurfaceAllocType,
                                            m_Scopes.top().m_GpuSmmuMode,
                                            m_Scopes.top().m_DisablePlcOnSurfaceMove));
}

PM_BIND(Action, MoveSurfaces, 1, "Usage: Action.MoveSurfaces(surfaces)",
        "Move the indicated surfaces to a new physical address.");

//--------------------------------------------------------------------
//! \brief Implement Action.MovePages() token
//!
JSBool PmParser::Action_MovePages
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_MovePages("Action.MovePages",
                                         pSurfaceDesc, offset, size,
                                         m_Scopes.top().m_AllocLocation,
                                         m_Scopes.top().m_MovePolicy,
                                         m_Scopes.top().m_LoopBackPolicy,
                                         m_Scopes.top().m_InbandSurfaceMove,
                                         m_Scopes.top().m_pInbandChannel,
                                         m_Scopes.top().m_MoveSurfInDefaultMMU,
                                         m_Scopes.top().m_SurfaceAllocType,
                                         m_Scopes.top().m_GpuSmmuMode,
                                         NULL,
                                         0,
                                         m_Scopes.top().m_DeferTlbIlwalidate,
                                         m_Scopes.top().m_DisablePlcOnSurfaceMove));
}

PM_BIND(Action, MovePages, 3,
        "Usage: Action.MovePages(surfaces, offset, size)",
        "Move pages within some surfaces to a new physical address");

//--------------------------------------------------------------------
//! \brief Implement Action.MovePages() token
//!
JSBool PmParser::Action_MovePagesToDestSurf
(
 jsval  srcSurfaceDesc,
 jsval  aSrcOffset,
 jsval  aSrcSize,
 jsval  destSurfaceDesc,
 jsval  aDestOffset,
 jsval *pReturlwalue
 )
{
    PmSurfaceDesc *pSrcSurfaceDesc;
    UINT32 srcOffset;
    UINT32 srcSize;
    PmSurfaceDesc *pDestSurfaceDesc;
    UINT32 destOffset;
    RC rc;

    C_CHECK_RC(FromJsval(srcSurfaceDesc, &pSrcSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aSrcOffset, &srcOffset, 2));
    C_CHECK_RC(FromJsval(aSrcSize, &srcSize, 3));
    C_CHECK_RC(FromJsval(destSurfaceDesc, &pDestSurfaceDesc, 4));
    C_CHECK_RC(FromJsval(aDestOffset, &destOffset, 5));
    RETURN_ACTION(new PmAction_MovePages("Action.MovePagesToDestSurf",
        pSrcSurfaceDesc, srcOffset, srcSize,
        Memory::Optimal,   // Dst location has been decided by destSurfaceDesc
        m_Scopes.top().m_MovePolicy,
        m_Scopes.top().m_LoopBackPolicy,
        m_Scopes.top().m_InbandSurfaceMove,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_MoveSurfInDefaultMMU,
        m_Scopes.top().m_SurfaceAllocType,
        m_Scopes.top().m_GpuSmmuMode,
        pDestSurfaceDesc,
        destOffset,
        m_Scopes.top().m_DeferTlbIlwalidate,
        m_Scopes.top().m_DisablePlcOnSurfaceMove));
}

PM_BIND(Action, MovePagesToDestSurf, 5,
        "Usage: Action.MovePagesToDestSurf(srcSurfaces, srcOffset, srcSize, destSurface,"
        " destOffset)",
        "Move pages within some surfaces to a new pre-created surface");

//--------------------------------------------------------------------
//! \brief Implement Action.AliasPages() token
//!
JSBool PmParser::Action_AliasPages
(
    jsval  surfaceDesc,
    jsval  aAliasDstOffset,
    jsval  aSize,
    jsval  aAliasSrcOffset,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 aliasDstOffset;
    UINT32 size;
    UINT32 aliasSrcOffset;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aAliasDstOffset, &aliasDstOffset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aAliasSrcOffset, &aliasSrcOffset, 4));
    RETURN_ACTION(new PmAction_AliasPages(pSurfaceDesc, aliasDstOffset, size, aliasSrcOffset,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, AliasPages, 4,
        "Usage: Action.AliasPages(surfaces, aliasDstOffset, size, aliasSrcOffset)",
        "Remap the virtual range at aliasDstOffset to match the mappings from aliasSrcOffset");

//--------------------------------------------------------------------
//! \brief Implement Action.AliasPagesFromSurface() token
//!
JSBool PmParser::Action_AliasPagesFromSurface
(
    jsval  aDestSurfaceDesc,
    jsval  aAliasDstOffset,
    jsval  aSize,
    jsval aSourceSurfaceDesc,
    jsval  aAliasSrcOffset,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pDestSurfaceDesc;
    UINT32 aliasDstOffset;
    UINT32 size;
    PmSurfaceDesc *pSourceSurfaceDesc;
    UINT32 aliasSrcOffset;
    RC rc;

    C_CHECK_RC(FromJsval(aDestSurfaceDesc, &pDestSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aAliasDstOffset, &aliasDstOffset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aSourceSurfaceDesc, &pSourceSurfaceDesc, 4));
    C_CHECK_RC(FromJsval(aAliasSrcOffset, &aliasSrcOffset, 5));
    RETURN_ACTION(new PmAction_AliasPagesFromSurface(pDestSurfaceDesc, aliasDstOffset, size,
        pSourceSurfaceDesc, aliasSrcOffset, m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, AliasPagesFromSurface, 5,
        "Usage: Action.AliasPages(destSurfaces, aliasDstOffset, size, sourceSurfaces, aliasSrcOffset)",
        "Remap the virtual range of destSurfaces at aliasDstOffset to match the mappings from aliasSrcOffset of sourceSurfaces");

//--------------------------------------------------------------------
//! \brief Implement Action.MoveFaultingSurface() token
//!
JSBool PmParser::Action_MoveFaultingSurface(jsval *pReturlwalue)
{
    RETURN_ACTION(new PmAction_MoveFaultingSurface(
                      m_Scopes.top().m_AllocLocation,
                      m_Scopes.top().m_MovePolicy,
                      m_Scopes.top().m_LoopBackPolicy,
                      m_Scopes.top().m_InbandSurfaceMove,
                      m_Scopes.top().m_pInbandChannel,
                      m_Scopes.top().m_DeferTlbIlwalidate,
                      m_Scopes.top().m_DisablePlcOnSurfaceMove));
}

PM_BIND(Action, MoveFaultingSurface, 0,
        "Usage: Action.MoveFaultingSurface()",
        "Move the faulting surface to a new physical address.");

//--------------------------------------------------------------------
//! \brief Implement Action.MoveFaultingPage() token
//!
JSBool PmParser::Action_MoveFaultingPage(jsval *pReturlwalue)
{
    RETURN_ACTION(new PmAction_MoveFaultingPage(
                      m_Scopes.top().m_AllocLocation,
                      m_Scopes.top().m_MovePolicy,
                      m_Scopes.top().m_LoopBackPolicy,
                      m_Scopes.top().m_InbandSurfaceMove,
                      m_Scopes.top().m_pInbandChannel,
                      m_Scopes.top().m_DeferTlbIlwalidate,
                      m_Scopes.top().m_DisablePlcOnSurfaceMove));
}

PM_BIND(Action, MoveFaultingPage, 0,
        "Usage: Action.MoveFaultingPage()",
        "Move the faulting page to a new physical address.");

//--------------------------------------------------------------------
//! \brief Implement Action.PreemptRunlist() token
//!
JSBool PmParser::Action_PreemptRunlist(jsval runlistDesc, jsval *pReturlwalue)
{
    PmRunlistDesc *pRunlistDesc;
    RC rc;

    C_CHECK_RC(FromJsval(runlistDesc, &pRunlistDesc, 1));
    string engineName = pRunlistDesc->GetName();
    pRunlistDesc->SetName(
       PolicyManager::Instance()->ColwPm2GpuEngineName(pRunlistDesc->GetName()));
    RETURN_ACTION(new PmAction_PreemptRunlist(pRunlistDesc,
                                              m_Scopes.top().m_pSmcEngineDesc));
}

PM_BIND(Action, PreemptRunlist, 1,
        "Usage: Action.PreemptRunlist(runlistDesc)",
        "Preempt the indicated runlist.");

//--------------------------------------------------------------------
//! \brief Implement Action.PreemptChannel() token
//!
JSBool PmParser::Action_PreemptChannel(jsval channelDesc,
                                       jsval pollPreemptComplete,
                                       jsval timeoutMs,
                                       jsval *pReturlwalue)
{
    PmChannelDesc *pChannelDesc;
    bool   bPollPreemptComplete;
    FLOAT64 TimeoutMs;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(pollPreemptComplete, &bPollPreemptComplete, 2));
    C_CHECK_RC(FromJsval(timeoutMs, &TimeoutMs, 3));
    RETURN_ACTION(new PmAction_PreemptChannel(pChannelDesc,
                                              bPollPreemptComplete,
                                              TimeoutMs));
}

PM_BIND(Action, PreemptChannel, 3,
        "Usage: Action.PreemptChannel(channelDesc, pollPreemptComplete, timeoutMs)",
        "Preempt the indicated channel (wait for preemtion to compelte if "
        "pollPreemptComplete is true).");

//--------------------------------------------------------------------
//! \brief Implement Action.RemoveEntriesFromRunlist() token
//!
JSBool PmParser::Action_RemoveEntriesFromRunlist
(
    jsval runlistDesc,
    jsval aFirstEntry,
    jsval aNumEntries,
    jsval *pReturlwalue
)
{
    PmRunlistDesc *pRunlistDesc;
    UINT32 firstEntry;
    UINT32 numEntries;
    RC rc;

    C_CHECK_RC(FromJsval(runlistDesc, &pRunlistDesc, 1));
    C_CHECK_RC(FromJsval(aFirstEntry, &firstEntry, 2));
    C_CHECK_RC(FromJsval(aNumEntries, &numEntries, 3));
    RETURN_ACTION(new PmAction_RemoveEntriesFromRunlist(pRunlistDesc,
                                               firstEntry,numEntries));
}

PM_BIND(Action, RemoveEntriesFromRunlist, 3,
        "Usage: Action.RemoveEntriesFromRunlist(runlistDesc, firstEntry, numentries)",
        "Remove the n'th to (n+num)'th entries from preempted runlist.");

//--------------------------------------------------------------------
//! \brief Implement Action.RestoreEntriesToRunlist() token
//!
JSBool PmParser::Action_RestoreEntriesToRunlist
(
    jsval runlistDesc,
    jsval aInsertPos,
    jsval aFirstEntry,
    jsval aNumEntries,
    jsval *pReturlwalue
)
{
    PmRunlistDesc *pRunlistDesc;
    UINT32 firstEntry;
    UINT32 insertPos;
    UINT32 numEntries;
    RC rc;

    C_CHECK_RC(FromJsval(runlistDesc, &pRunlistDesc, 1));
    C_CHECK_RC(FromJsval(aInsertPos, &insertPos, 2));
    C_CHECK_RC(FromJsval(aFirstEntry, &firstEntry, 3));
    C_CHECK_RC(FromJsval(aNumEntries, &numEntries, 4));
    RETURN_ACTION(new PmAction_RestoreEntriesToRunlist(pRunlistDesc,
                                           insertPos,firstEntry,numEntries));
}

PM_BIND(Action, RestoreEntriesToRunlist, 4,
        "Usage: Action.RestoreEntriesToRunlist(runlistDesc, insertPos, firstEntry, numentries)",
        "Restore the n'th to (n+num)'th entry to runlist in indicated position.");

//--------------------------------------------------------------------
//! \brief Implement Action.RemoveChannelFromRunlist() token
//!
JSBool PmParser::Action_RemoveChannelFromRunlist
(
    jsval channelDesc,
    jsval aFirstEntry,
    jsval aNumEntries,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    UINT32 firstEntry;
    UINT32 numEntries;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aFirstEntry, &firstEntry, 2));
    C_CHECK_RC(FromJsval(aNumEntries, &numEntries, 3));
    RETURN_ACTION(new PmAction_RemoveChannelFromRunlist(pChannelDesc,
                                               firstEntry,numEntries));
}

PM_BIND(Action, RemoveChannelFromRunlist, 3,
        "Usage: Action.RemoveChannelFromRunlist(channelDesc, firstEntry, numentries)",
        "Remove the n'th to (n+num)'th entry for the specified channel.");

//--------------------------------------------------------------------
//! \brief Implement Action.RestoreChannelToRunlist() token
//!
JSBool PmParser::Action_RestoreChannelToRunlist
(
    jsval channelDesc,
    jsval aInsertPos,
    jsval aFirstEntry,
    jsval aNumEntries,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    UINT32 firstEntry;
    UINT32 insertPos;
    UINT32 numEntries;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aInsertPos, &insertPos, 2));
    C_CHECK_RC(FromJsval(aFirstEntry, &firstEntry, 3));
    C_CHECK_RC(FromJsval(aNumEntries, &numEntries, 4));
    RETURN_ACTION(new PmAction_RestoreChannelToRunlist(pChannelDesc,
                                           insertPos,firstEntry,numEntries));
}

PM_BIND(Action, RestoreChannelToRunlist, 4,
        "Usage: Action.RestoreChannelToRunlist(channelDesc, insertPos, firstEntry, numentries)",
        "Restore the n'th to (n+num)'th entry for the specified channel in indicated position.");

//--------------------------------------------------------------------
//! \brief Implement Action.MoveEntryInRunlist() token
//!
JSBool PmParser::Action_MoveEntryInRunlist
(
    jsval runlistDesc,
    jsval aEntryIndex,
    jsval aRelPri,
    jsval *pReturlwalue
)
{
    PmRunlistDesc *pRunlistDesc;
    UINT32 entryIndex;
    UINT32 relPri;
    RC rc;

    C_CHECK_RC(FromJsval(runlistDesc, &pRunlistDesc, 1));
    C_CHECK_RC(FromJsval(aEntryIndex, &entryIndex, 2));
    C_CHECK_RC(FromJsval(aRelPri, &relPri, 3));
    RETURN_ACTION(new PmAction_MoveEntryInRunlist(pRunlistDesc,
                                                  entryIndex,relPri));
}

PM_BIND(Action, MoveEntryInRunlist, 3,
        "Usage: Action.MoveEntryInRunlist(runlistDesc, entryIndex, relPri)",
        "Move the n'th entry in preempted runlist by relPri.");

//--------------------------------------------------------------------
//! \brief Implement Action.MoveChannelInRunlist() token
//!
JSBool PmParser::Action_MoveChannelInRunlist
(
    jsval channelDesc,
    jsval aEntryIndex,
    jsval aRelPri,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    UINT32 entryIndex;
    UINT32 relPri;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aEntryIndex, &entryIndex, 2));
    C_CHECK_RC(FromJsval(aRelPri, &relPri, 3));
    RETURN_ACTION(new PmAction_MoveChannelInRunlist(pChannelDesc,
                                                    entryIndex, relPri));
}

PM_BIND(Action, MoveChannelInRunlist, 3,
        "Usage: Action.MoveChannelInRunlist(channelDesc, aEntryIndex, relPri)",
        "Move the n'th entry matching indicated channel by relPri.");

//--------------------------------------------------------------------
//! \brief Implement Action.ResubmitRunlist() token
//!
JSBool PmParser::Action_ResubmitRunlist(jsval runlistDesc, jsval *pReturlwalue)
{
    PmRunlistDesc *pRunlistDesc;
    RC rc;

    C_CHECK_RC(FromJsval(runlistDesc, &pRunlistDesc, 1));
    string engineName = pRunlistDesc->GetName();
    pRunlistDesc->SetName(
       PolicyManager::Instance()->ColwPm2GpuEngineName(pRunlistDesc->GetName()));

    RETURN_ACTION(new PmAction_ResubmitRunlist(pRunlistDesc,
                                               m_Scopes.top().m_pSmcEngineDesc));
}

PM_BIND(Action, ResubmitRunlist, 1,
        "Usage: Action.ResubmitRunlist(runlistDesc)",
        "Resubmit the entries to the indicated runlist.");

//--------------------------------------------------------------------
//! \brief Implement Action.FreezeRunlist() token
//!
JSBool PmParser::Action_FreezeRunlist
(
    jsval runlistDesc,
    jsval *pReturlwalue
)
{
    PmRunlistDesc *pRunlistDesc;
    RC rc;

    C_CHECK_RC(FromJsval(runlistDesc, &pRunlistDesc, 1));
    RETURN_ACTION(new PmAction_FreezeRunlist(pRunlistDesc));
}

PM_BIND(Action, FreezeRunlist, 1, "Usage: Action.FreezeRunlist(runlist)",
        "Freeze the runlist, so that RL_PUT is not updated when entries are added.");

//--------------------------------------------------------------------
//! \brief Implement Action.UnfreezeRunlist() token
//!
JSBool PmParser::Action_UnfreezeRunlist
(
    jsval runlistDesc,
    jsval *pReturlwalue
)
{
    PmRunlistDesc *pRunlistDesc;
    RC rc;

    C_CHECK_RC(FromJsval(runlistDesc, &pRunlistDesc, 1));
    RETURN_ACTION(new PmAction_UnfreezeRunlist(pRunlistDesc));
}

PM_BIND(Action, UnfreezeRunlist, 1, "Usage: Action.UnfreezeRunlist(runlist)",
        "Unfreeze the runlist.  RL_PUT is advanced to include all entries.");

//--------------------------------------------------------------------
//! \brief Implement Action.WriteRunlistEntries() token
//!
JSBool PmParser::Action_WriteRunlistEntries
(
    jsval aRunlistDesc,
    jsval aNumEntries,
    jsval *pReturlwalue
)
{
    PmRunlistDesc *pRunlistDesc;
    UINT32 NumEntries;
    RC rc;

    C_CHECK_RC(FromJsval(aRunlistDesc, &pRunlistDesc, 1));
    C_CHECK_RC(FromJsval(aNumEntries, &NumEntries, 2));
    RETURN_ACTION(new PmAction_WriteRunlistEntries(pRunlistDesc, NumEntries));
}

PM_BIND(Action, WriteRunlistEntries, 2,
        "Usage: Action.WriteRunlistEntries(runlist, numEntries)",
        "Advance RL_PUT if the runlist is frozen, with queued-up entries.");

//--------------------------------------------------------------------
//! \brief Implement Action.Flush() token
//!
JSBool PmParser::Action_Flush(jsval channelDesc, jsval *pReturlwalue)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_ACTION(new PmAction_Flush(pChannelDesc));
}

PM_BIND(Action, Flush, 1, "Usage: Action.Flush(channels)",
        "Flush the indicated channel(s).");

//--------------------------------------------------------------------
//! \brief Implement Action.ResetChannel() token
//!
JSBool PmParser::Action_ResetChannel
(
    jsval aChannelDesc,
    jsval aEngineName,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    string EngineName;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(aChannelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aEngineName,  &EngineName,   2));
    RETURN_ACTION(new PmAction_ResetChannel(pChannelDesc, EngineName));
}

PM_BIND(Action, ResetChannel, 2,
        "Usage: Action.ResetChannel(channels, engineName)",
        "Reset the indicated channel(s) and the indicated engine.");

//--------------------------------------------------------------------
//! \brief Implement Action.EnableEngine() token
//!
JSBool PmParser::Action_EnableEngine
(
    jsval aGpuDesc,
    jsval aEngineType,
    jsval aHwinst,
    jsval enable,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    string EngineType;
    UINT32 hwinst;
    bool bEnable;
    RC rc;
    string RegName = "LW_PMC_DEVICE_ENABLE";

    C_CHECK_RC(FromJsval(aGpuDesc,      &pGpuDesc,   1));
    C_CHECK_RC(FromJsval(aEngineType,   &EngineType, 2));
    C_CHECK_RC(FromJsval(aHwinst,       &hwinst,     3));
    C_CHECK_RC(FromJsval(enable,        &bEnable,    4));

    vector<UINT32> RegIndexes;
    C_CHECK_RC(ParsePriReg(&RegName, &RegIndexes));
    RETURN_ACTION(new PmAction_EnableEngine(
        pGpuDesc,
        EngineType,
        hwinst,
        bEnable,
        m_Scopes.top().m_pSmcEngineDesc,
        m_Scopes.top().m_pChannelDesc,
        RegName,
        RegIndexes,
        m_Scopes.top().m_RegSpace));
}

PM_BIND(Action, EnableEngine, 4,
        "Usage: Action.EnableEngine(Gpu, engineType, hwInstance, enable)",
        "Enable/Disable indicated engine.");

//--------------------------------------------------------------------
//! \brief Implement Action.UnbindAndRebindChannel() token
//!
JSBool PmParser::Action_UnbindAndRebindChannel
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_ACTION(new PmAction_UnbindAndRebindChannel(pChannelDesc));
}

PM_BIND(Action, UnbindAndRebindChannel, 1,
        "Usage: Action.UnbindAndRebindChannel(channelDesc)",
        "Unbind and rebind a channel.");

//--------------------------------------------------------------------
//! \brief Implement Action.DisableChannel() token
//!
JSBool PmParser::Action_DisableChannel
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_ACTION(new PmAction_DisableChannel(pChannelDesc));
}

PM_BIND(Action, DisableChannel, 1,
        "Usage: Action.DisableChannel(channelDesc)",
        "Disable a channel, so that methods aren't written and no CRC.");

//--------------------------------------------------------------------
//! \brief Implement Action.BlockChannelFlush() token
//!
JSBool PmParser::Action_BlockChannelFlush
(
    jsval channelDesc,
    jsval blocked,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    bool           bBlockChannelFlush;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(blocked, &bBlockChannelFlush, 2));
    RETURN_ACTION(new PmAction_BlockChannelFlush(pChannelDesc, bBlockChannelFlush));
}

PM_BIND(Action, BlockChannelFlush, 2,
        "Usage: Action.BlockChannelFlush(channelDesc, bBlocked)",
        "pause a channel by preventing a flush - does not prevent channel writes");

//--------------------------------------------------------------------
//! \brief Implement Action.TlbIlwalidate() token
//!
JSBool PmParser::Action_TlbIlwalidate(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    if (  m_Scopes.top().m_TlbIlwalidatePdbAll
        && (m_Scopes.top().m_pTlbIlwalidateChannel != NULL
            || m_Scopes.top().m_TlbIlwalidateBar1
            || m_Scopes.top().m_TlbIlwalidateBar2) )
    {
        ErrPrintf("%s: Policy.SetTlbIlwalidatePdbAll is true, while Policy.SetTlbIlwalidateChannelPdb got set "
                  "or Policy.SetTlbIlwalidateBar1 got set or Policy.SetTlbIlwalidateBar2 got set\n",
                  GetErrInfo().c_str());
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    if (  !m_Scopes.top().m_TlbIlwalidatePdbAll
        && (m_Scopes.top().m_pTlbIlwalidateChannel == NULL
            && !m_Scopes.top().m_TlbIlwalidateBar1
            && !m_Scopes.top().m_TlbIlwalidateBar2) )
    {

        ErrPrintf("%s: Policy.SetTlbIlwalidatePdbAll is false, while Policy.SetTlbIlwalidateChannelPdb didn't set "
                  "and Policy.SetTlbIlwalidateBar1 didn't set and Policy.SetTlbIlwalidateBar2 didn't set\n",
                  GetErrInfo().c_str());
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_TargetedTlbIlwalidate(
                      pGpuDesc,
                      m_Scopes.top().m_TlbIlwalidateGpc,
                      m_Scopes.top().m_TlbIlwalidateReplay,
                      m_Scopes.top().m_TlbIlwalidateSysmembar,
                      m_Scopes.top().m_TlbIlwalidateAckType,
                      m_Scopes.top().m_InbandMemOp,
                      m_Scopes.top().m_pInbandChannel,
                      m_Scopes.top().m_ChannelSubdevMask,
                      m_Scopes.top().m_AddrSpaceType,
                      *(m_Scopes.top().m_pGPCID),
                      *(m_Scopes.top().m_pClientUnitID),
                      m_Scopes.top().m_pTlbIlwalidateChannel,
                      m_Scopes.top().m_TlbIlwalidateBar1,
                      m_Scopes.top().m_TlbIlwalidateBar2,
                      m_Scopes.top().m_TlbIlwalidateLevelNum,
                      m_Scopes.top().m_TlbIlwalidateIlwalScope));
}

PM_BIND(Action, TlbIlwalidate, 1,
        "Usage: Action.TlbIlwalidate(gpu)",
        "Ilwalidate the TLB entries.");

//--------------------------------------------------------------------
//! \brief Implement Action.TlbIlwalidateVA() token
//!
JSBool PmParser::Action_TlbIlwalidateVA
(
    jsval surfaceDesc,
    jsval aOffset,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT64 offset;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    RETURN_ACTION(new PmAction_TlbIlwalidateVA(pSurfaceDesc,
                                       offset,
                                       m_Scopes.top().m_InbandMemOp,
                                       m_Scopes.top().m_ChannelSubdevMask,
                                       m_Scopes.top().m_pInbandChannel,
                                       m_Scopes.top().m_TlbIlwalidateGpc,
                                       m_Scopes.top().m_TlbIlwalidateReplay,
                                       m_Scopes.top().m_TlbIlwalidateSysmembar,
                                       m_Scopes.top().m_TlbIlwalidateAckType,
                                       m_Scopes.top().m_AddrSpaceType,
                                       m_Scopes.top().m_pTlbIlwalidateChannel,
                                       m_Scopes.top().m_TlbIlwalidateBar1,
                                       m_Scopes.top().m_TlbIlwalidateBar2,
                                       m_Scopes.top().m_TlbIlwalidateLevelNum,
                                       m_Scopes.top().m_TlbIlwalidateIlwalScope,
                                       *(m_Scopes.top().m_pGPCID),
                                       *(m_Scopes.top().m_pClientUnitID),
                                       m_Scopes.top().m_AccessType,
                                       m_Scopes.top().m_VEID,
                                       m_Scopes.top().m_pSmcEngineDesc));
}

PM_BIND(Action, TlbIlwalidateVA, 2,
        "Usage: Action.TlbIlwalidateVA(surface, offset)",
        "tlb ilwalidate targeted virtual address.");

//--------------------------------------------------------------------
//! \brief Implement Action.L2Flush() token
//!
JSBool PmParser::Action_L2Flush(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_L2Flush(pGpuDesc, m_Scopes.top().m_InbandMemOp,
                                       m_Scopes.top().m_ChannelSubdevMask));
}

PM_BIND(Action, L2Flush, 1,
        "Usage: Action.L2Flush(gpu)",
        "Flush the GPU's L2 cache.");

//--------------------------------------------------------------------
//! \brief Implement Action.L2SysmemIlwalidate() token
//!
JSBool PmParser::Action_L2SysmemIlwalidate(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_L2SysmemIlwalidate(pGpuDesc,
                                          m_Scopes.top().m_InbandMemOp,
                                          m_Scopes.top().m_ChannelSubdevMask));
}

PM_BIND(Action, L2SysmemIlwalidate, 1,
        "Usage: Action.L2SysmemIlwalidate(gpu)",
        "Ilwalidate the GPU's L2 sysmem cache.");

//--------------------------------------------------------------------
//! \brief Implement Action.L2PeermemIlwalidate() token
//!
JSBool PmParser::Action_L2PeermemIlwalidate(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_L2PeermemIlwalidate(pGpuDesc,
                                          m_Scopes.top().m_InbandMemOp,
                                          m_Scopes.top().m_ChannelSubdevMask));
}

PM_BIND(Action, L2PeermemIlwalidate, 1,
        "Usage: Action.L2PeermemIlwalidate(gpu)",
        "Ilwalidate the GPU's L2 peermem cache.");

//--------------------------------------------------------------------
//! \brief Implement Action.L2WaitForSysPendingReads() token
//!
JSBool PmParser::Action_L2WaitForSysPendingReads
(
    jsval gpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_L2WaitForSysPendingReads(pGpuDesc,
                                            m_Scopes.top().m_InbandMemOp,
                                            m_Scopes.top().m_ChannelSubdevMask));
}

PM_BIND(Action, L2WaitForSysPendingReads, 1,
        "Usage: Action.L2WaitForSysPendingReads()",
        "L2 wait for sysmem pending read\n");

//--------------------------------------------------------------------
//! \brief Implement Action.L2VidmemIlwalidate() token
//!
JSBool PmParser::Action_L2VidmemIlwalidate(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_L2VidmemIlwalidate(pGpuDesc, m_Scopes.top().m_InbandMemOp,
                                       m_Scopes.top().m_ChannelSubdevMask));
}

PM_BIND(Action, L2VidmemIlwalidate, 1,
        "Usage: Action.L2VidmemIlwalidate(gpu)",
        "Ilwalidate the specified GPU's vidmem L2 cache.");

//--------------------------------------------------------------------
//! \brief Implement Action.HostMembar() token
//!
JSBool PmParser::Action_HostMembar
(
    jsval gpuDesc,
    jsval aIsMembar,
    jsval *pReturlwalue
)
{
    RC rc;
    PmGpuDesc * pGpuDesc;
    bool isMembar;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(aIsMembar, &isMembar, 2));

    RETURN_ACTION(new PmAction_HostMembar(pGpuDesc,
                                          isMembar,
                                          m_Scopes.top().m_pInbandChannel,
                                          m_Scopes.top().m_InbandMemOp));

}

PM_BIND(Action, HostMembar, 2,
        "Usage: Action.HostMembar()",
        "Wait for all write finish\n");

//--------------------------------------------------------------------
//! \brief Implement Action.ChannelWfi() token
//!
JSBool PmParser::Action_ChannelWfi
(
 jsval channelDesc,
 jsval aWfiType,
 jsval *pReturlwalue
 )
{
    RC rc;
    PmChannelDesc * pChannelDesc;
    string wfiTypeString;
    PolicyManager::Wfi wfiType = PolicyManager::WFI_UNDEFINED;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aWfiType, &wfiTypeString, 2));

    if ("WFI_SCOPE_ALL" == wfiTypeString)
    {
        wfiType = PolicyManager::WFI_SCOPE_ALL;
    }
    else if ("WFI_SCOPE_LWRRENT_SCG_TYPE" == wfiTypeString)
    {
        wfiType = PolicyManager::WFI_SCOPE_LWRRENT_SCG_TYPE;
    }
    else if ("WFI_POLL_HOST_SEMAPHORE" == wfiTypeString)
    {
        wfiType = PolicyManager::WFI_POLL_HOST_SEMAPHORE;
    }
    else
    {
        ErrPrintf("invalid WFI type for %s.\n", __FUNCTION__);
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    RETURN_ACTION(new PmAction_ChannelWfi(pChannelDesc, wfiType));

}

PM_BIND(Action, ChannelWfi, 2,
        "Usage: Action.ChannelWfi()",
        "wait for specific channel idel\n");

//--------------------------------------------------------------------
//! \brief Implement Action.SetReadOnly() token
//!
JSBool PmParser::Action_SetReadOnly
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SetReadOnly(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));

}

PM_BIND(Action, SetReadOnly, 3,
        "Usage: Action.SetReadOnly(surfaces, offset, size)",
        "Set the read-only bit in the PTEs for the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearReadOnly() token
//!
JSBool PmParser::Action_ClearReadOnly
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_ClearReadOnly(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, ClearReadOnly, 3,
        "Usage: Action.ClearReadOnly(surfaces, offset, size)",
        "Clear the read-only bit in the PTEs for the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.SetAtomicDisable() token
//!
JSBool PmParser::Action_SetAtomicDisable
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SetAtomicDisable(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));

}

PM_BIND(Action, SetAtomicDisable, 3,
        "Usage: Action.SetAtomicDisable(surfaces, offset, size)",
        "Set the ATOMIC_DISABLE bit in the PTEs for the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearAtomicDisable() token
//!
JSBool PmParser::Action_ClearAtomicDisable
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_ClearAtomicDisable(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, ClearAtomicDisable, 3,
        "Usage: Action.ClearAtomicDisable(surfaces, offset, size)",
        "Clear the ATOMIC_DISABLE bit in the PTEs for the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.SetShaderDefaultAccess() token
//!
JSBool PmParser::Action_SetShaderDefaultAccess
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SetShaderDefaultAccess(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SetShaderDefaultAccess, 3,
        "Usage: Action.SetShaderDefaultAccess(surfaces, offset, size)",
        "Reset the read/write-disable bits in the PTEs for the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.SetShaderReadOnly() token
//!
JSBool PmParser::Action_SetShaderReadOnly
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SetShaderReadOnly(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SetShaderReadOnly, 3,
        "Usage: Action.SetShaderReadOnly(surfaces, offset, size)",
        "Set the read/write-disable bits in the PTEs for the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.SetShaderWriteOnly() token
//!
JSBool PmParser::Action_SetShaderWriteOnly
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SetShaderWriteOnly(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SetShaderWriteOnly, 3,
        "Usage: Action.SetShaderWriteOnly(surfaces, offset, size)",
        "Set the read/write-disable bits in the PTEs for the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.SetShaderReadWrite() token
//!
JSBool PmParser::Action_SetShaderReadWrite
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SetShaderReadWrite(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SetShaderReadWrite, 3,
        "Usage: Action.SetShaderReadWrite(surfaces, offset, size)",
        "Set the read/write-disable bits in the PTEs for the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.SetPdeSize() token
//!
JSBool PmParser::Action_SetPdeSize
(
    jsval  aSurfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval  aPdeSize,
    jsval *pReturlwalue
)
{
    FLOAT64 PdeSize;
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 Offset;
    UINT32 Size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(aSurfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &Offset, 2));
    C_CHECK_RC(FromJsval(aSize, &Size, 3));
    C_CHECK_RC(FromJsval(aPdeSize, &PdeSize, 4));
    RETURN_ACTION(new PmAction_SetPdeSize(pSurfaceDesc, Offset, Size,
                                          PdeSize));
}

PM_BIND(Action, SetPdeSize, 4,
        "Usage: Action.SetPdeSize(surfaces, offset, size, pdeSize)",
        "Set the Size in the PDEs for the indicated pages to 1.0, 0.5, 0.25, or 0.125");

//--------------------------------------------------------------------
//! \brief Implement Action.SetPdeAperture() token
//!
JSBool PmParser::Action_SetPdeAperture
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval  aAperture,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    PolicyManager::Aperture aperture;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aAperture, &aperture, 4));

    if (aperture != PolicyManager::APERTURE_FB &&
        aperture != PolicyManager::APERTURE_COHERENT &&
        aperture != PolicyManager::APERTURE_NONCOHERENT &&
        aperture != PolicyManager::APERTURE_ILWALID)
    {
        ErrPrintf("%s: Aperture must be Framebuffer, Coherent, NonCoherent, or Invalid\n",
                  GetErrInfo().c_str());
        return RC::BAD_PARAMETER;
    }
    RETURN_ACTION(new PmAction_SetPdeAperture(pSurfaceDesc, offset, size,
                                              aperture,
                                              m_Scopes.top().m_PageSize));
}

PM_BIND(Action, SetPdeAperture, 4,
        "Usage: Action.SetPdeAperture(surfaces, offset, size, aperture)",
        "Set the Aperture in the PDEs for the indicated pages.");

//--------------------------------------------------------------------
//! \brief Implement Action.SparsifyPde() token
//!
JSBool PmParser::Action_SparsifyPde
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));

    // Obsolete action. Only supports 4k/64k pagesize, so targeted MMU level is PDE0
    RETURN_ACTION(new PmAction_SparsifyMmuLevel(pSurfaceDesc, offset,
                                                size,
                                                m_Scopes.top().m_PageSize,
                                                m_Scopes.top().m_InbandPte,
                                                m_Scopes.top().m_pInbandChannel,
                                                m_Scopes.top().m_AddrSpaceType,
                                                PolicyManager::LEVEL_PDE0,
                                                m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SparsifyPde, 3,
        "Usage: Action.SparsifyPde(surfaces, offset, size)",
        "Set PDEs to sparse for the indicated pages."
        "Obsolete action. Please try Action.SparsifyMmuLevel(surfaces, offset, size, levelName)");

//--------------------------------------------------------------------
//! \brief Implement Action.SparsifyMmuLevel() token
//!
JSBool PmParser::Action_SparsifyMmuLevel
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval  aLevelNum,
    jsval *pReturlwalue
)
{
    RC rc;
    UINT32 offset;
    UINT32 size;
    string levelName;
    PmSurfaceDesc *pSurfaceDesc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aLevelNum, &levelName, 4));

    // Obsolete action, only PDE0 is supported
    RETURN_ACTION(new PmAction_SparsifyMmuLevel(pSurfaceDesc, offset, size,
                                                m_Scopes.top().m_PageSize,
                                                m_Scopes.top().m_InbandPte,
                                                m_Scopes.top().m_pInbandChannel,
                                                m_Scopes.top().m_AddrSpaceType,
                                                ColwertStrToMmuLevelType(levelName),
                                                m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SparsifyMmuLevel, 4,
        "Usage: Action.SparsifyMmuLevel(surfaces, offset, size, mmuLevel)",
        "Set PDEs to sparse for the indicated pages. ");

//--------------------------------------------------------------------
//! \brief Implement Action.SetPteAperture() token
//!
JSBool PmParser::Action_SetPteAperture
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval  aAperture,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    PolicyManager::Aperture aperture;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aAperture, &aperture, 4));

    if (aperture != PolicyManager::APERTURE_FB &&
        aperture != PolicyManager::APERTURE_COHERENT &&
        aperture != PolicyManager::APERTURE_NONCOHERENT &&
        aperture != PolicyManager::APERTURE_PEER)
    {
        ErrPrintf("%s: Aperture must be Framebuffer, Coherent, NonCoherent, or Peer\n",
                  GetErrInfo().c_str());
        return RC::BAD_PARAMETER;
    }

    RETURN_ACTION(new PmAction_SetPteAperture(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        aperture,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SetPteAperture, 4,
        "Usage: Action.SetPteAperture(surfaces, offset, size, aperture)",
        "Set the Aperture in the PTEs for the indicated pages.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetVolatile() token
//!
JSBool PmParser::Action_SetVolatile
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_ClearVolatile(
        "Action.ClearVolatile",
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SetVolatile, 3,
        "Usage: Action.SetVolatile(surfaces, offset, size)",
        "Set the Volatile (GpuCacheable) bit in the PTEs for the indicated pages.");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearVolatile() token
//!
JSBool PmParser::Action_ClearVolatile
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SetVolatile(
        "Action.SetVolatile",
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, ClearVolatile, 3,
        "Usage: Action.ClearVolatile(surfaces, offset, size)",
        "Clear the Volatile (GpuCacheable) bit in the PTEs for the indicated pages.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetVolatileBit() token
//!
JSBool PmParser::Action_SetVolatileBit
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SetVolatile(
        "Action.SetVolatileBit",
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SetVolatileBit, 3,
        "Usage: Action.SetVolatileBit(surfaces, offset, size)",
        "Set the volatile bit in the PTEs for the indicated pages.");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearVolatileBit() token
//!
JSBool PmParser::Action_ClearVolatileBit
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_ClearVolatile(
        "Action.ClearVolatileBit",
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, ClearVolatileBit, 3,
        "Usage: Action.ClearVolatileBit(surfaces, offset, size)",
        "Clear the volatile bit in the PTEs for the indicated pages.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetKind() token
//!
JSBool PmParser::Action_SetKind
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval  aKind,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    UINT32 kind;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aKind, &kind, 4));
    RETURN_ACTION(new PmAction_SetKind(pSurfaceDesc, offset, size, kind));
}

PM_BIND(Action, SetKind, 4,
        "Usage: Action.SetKind(surfaces, offset, size, kind)",
        "Set the Kind for the indicated pages to the indicated number.");

//--------------------------------------------------------------------
//! \brief Implement Action.IlwalidatePdbTarget() token
//!
JSBool PmParser::Action_IlwalidatePdbTarget
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    RETURN_ACTION(new PmAction_IlwalidatePdbTarget(pChannelDesc));
}

PM_BIND(Action, IlwalidatePdbTarget, 1,
        "Usage: Action.IlwalidatePdbTarget(channels)",
        "Set the PDB target field to an invalid value for indicated channel(s)"
    );

//--------------------------------------------------------------------
//! \brief Implement Action.ResetEngineContext() token
//!
JSBool PmParser::Action_ResetEngineContext
(
    jsval aChannelDesc,
    jsval aEngineName,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RegExp EngineName;
    RC rc;

    C_CHECK_RC(FromJsval(aChannelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aEngineName,  &EngineName,   2, RegExp::FULL |
                                                         RegExp::ICASE));

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    RETURN_ACTION(new PmAction_ResetEngineContext(pChannelDesc, EngineName));
}

PM_BIND(Action, ResetEngineContext, 2,
        "Usage: Action.ResetEngineContext(channelDesc, engineName)",
        "Reset the engine context on the channel(s) to an invalid value.");

//--------------------------------------------------------------------
//! \brief Implement Action.ResetEngineContextNoPreempt() token
//!
JSBool PmParser::Action_ResetEngineContextNoPreempt
(
    jsval aChannelDesc,
    jsval aEngineName,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RegExp EngineName;
    RC rc;

    C_CHECK_RC(FromJsval(aChannelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aEngineName,  &EngineName,   2, RegExp::FULL |
                                                         RegExp::ICASE));

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    RETURN_ACTION(new PmAction_ResetEngineContextNoPreempt(pChannelDesc,
                                                           EngineName));
}

PM_BIND(Action, ResetEngineContextNoPreempt, 2,
        "Usage: Action.ResetEngineContextNoPreempt(channelDesc, engineName)",
        "Reset the engine context on the channel(s) to an invalid value,"
        " without preempting the channel.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetPrivSurfaces() token
//!
JSBool PmParser::Action_SetPrivSurfaces
(
    jsval  surfaceDesc,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    RETURN_ACTION(new PmAction_SetPrivSurfaces(
        pSurfaceDesc,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));

}

PM_BIND(Action, SetPrivSurfaces, 1,
        "Usage: Action.SetPrivSurfaces(surfaces)",
        "Set the priv bit in the indicated surfaces.");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearPrivSurfaces() token
//!
JSBool PmParser::Action_ClearPrivSurfaces
(
    jsval  surfaceDesc,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    RETURN_ACTION(new PmAction_ClearPrivSurfaces(
        pSurfaceDesc,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, ClearPrivSurfaces, 1,
        "Usage: Action.ClearPrivSurfaces(surfaces)",
        "Clear the priv bit in the indicated surfaces.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetPrivPages() token
//!
JSBool PmParser::Action_SetPrivPages
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_SetPrivPages(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SetPrivPages, 3,
        "Usage: Action.SetPrivPages(surfaces, offset, size)",
        "Set the priv bit in the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearPrivPages() token
//!
JSBool PmParser::Action_ClearPrivPages
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_ClearPrivPages(
        pSurfaceDesc,
        offset,
        size,
        m_Scopes.top().m_PageSize,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, ClearPrivPages, 3,
        "Usage: Action.ClearPrivPages(surfaces, offset, size)",
        "Clear the priv bit in the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.SetPrivChannels() token
//!
JSBool PmParser::Action_SetPrivChannels
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_ACTION(new PmAction_SetPrivChannels(pChannelDesc));
}

PM_BIND(Action, SetPrivChannels, 1,
        "Usage: Action.SetPrivChannels(channels)",
        "Set the priv bit in all GPFIFO entries until otherwise indicated.");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearPrivChannels() token
//!
JSBool PmParser::Action_ClearPrivChannels
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_ACTION(new PmAction_ClearPrivChannels(pChannelDesc));
}

PM_BIND(Action, ClearPrivChannels, 1,
        "Usage: Action.ClearPrivChannels(channels)",
        "Clear the priv bit in all GPFIFO entries until otherwise indicated.");

//--------------------------------------------------------------------
//! \brief Implement Action.HostSemaphoreAcquire() token
//!
JSBool PmParser::Action_HostSemaphoreAcquire
(
    jsval channelDesc,
    jsval surfaceDesc,
    jsval aOffset,
    jsval aPayload,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    PmSurfaceDesc *pSurfaceDesc;
    UINT64 offset;
    UINT64 payload;
    RC rc;
    FancyPicker    offsetFancyPicker;
    FancyPicker    payloadFancyPicker;

    ChannelWrapperMgr::Instance()->UseSemaphoreWrapper();
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 2));
    if (OK != offsetFancyPicker.FromJsval(aOffset))
    {
        C_CHECK_RC(FromJsval(aOffset, &offset, 3));
        C_CHECK_RC(offsetFancyPicker.ConfigConst64(offset));
    }
    if (OK != payloadFancyPicker.FromJsval(aPayload))
    {
        C_CHECK_RC(FromJsval(aPayload, &payload, 4));
        C_CHECK_RC(payloadFancyPicker.ConfigConst64(payload));
    }
    RETURN_ACTION(new PmAction_HostSemaphoreAcquire(pChannelDesc,
                                         pSurfaceDesc,
                                         offsetFancyPicker,
                                         payloadFancyPicker,
                                         m_Scopes.top().m_ChannelSubdevMask,
                                         m_Scopes.top().m_pRemoteGpuDesc));
}

PM_BIND(Action, HostSemaphoreAcquire, 4,
        "Usage: Action.HostSemaphoreAcquire(channels, surfaces, offset, payload)",
        "Insert a semaphore-acquire operation in the pushbuffer.");

//--------------------------------------------------------------------
//! \brief Implement Action.HostSemaphoreRelease() token
//!
JSBool PmParser::Action_HostSemaphoreRelease
(
    jsval channelDesc,
    jsval surfaceDesc,
    jsval aOffset,
    jsval aPayload,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    PmSurfaceDesc *pSurfaceDesc;
    UINT64 offset;
    UINT64 payload;
    RC rc;
    FancyPicker    offsetFancyPicker;
    FancyPicker    payloadFancyPicker;

    ChannelWrapperMgr::Instance()->UseSemaphoreWrapper();
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 2));
    if (OK != offsetFancyPicker.FromJsval(aOffset))
    {
        C_CHECK_RC(FromJsval(aOffset, &offset, 3));
        C_CHECK_RC(offsetFancyPicker.ConfigConst64(offset));
    }
    if (OK != payloadFancyPicker.FromJsval(aPayload))
    {
        C_CHECK_RC(FromJsval(aPayload, &payload, 4));
        C_CHECK_RC(payloadFancyPicker.ConfigConst64(payload));
    }
    RETURN_ACTION(new PmAction_HostSemaphoreRelease(pChannelDesc,
                                          pSurfaceDesc,
                                          offsetFancyPicker,
                                          payloadFancyPicker,
                                          m_Scopes.top().m_ChannelSubdevMask,
                                          m_Scopes.top().m_SemRelFlags,
                                          m_Scopes.top().m_pRemoteGpuDesc));
}

PM_BIND(Action, HostSemaphoreRelease, 4,
        "Usage: Action.HostSemaphoreRelease(channels, surfaces, offset, payload)",
        "Insert a host semaphore-release operation in the pushbuffer.");

//--------------------------------------------------------------------
//! \brief Implement Action.HostSemaphoreReduction() token
//!
JSBool PmParser::Action_HostSemaphoreReduction
(
    jsval channelDesc,
    jsval surfaceDesc,
    jsval aOffset,
    jsval aPayload,
    jsval aReduction,
    jsval aReductionType,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    PmSurfaceDesc *pSurfaceDesc;
    FancyPicker    offsetFancyPicker;
    FancyPicker    payloadFancyPicker;
    UINT32         reduction;
    UINT32         reductionType;
    RC rc;

    ChannelWrapperMgr::Instance()->UseSemaphoreWrapper();
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 2));
    if (OK != offsetFancyPicker.FromJsval(aOffset))
    {
        UINT64 offset;
        C_CHECK_RC(FromJsval(aOffset, &offset, 3));
        C_CHECK_RC(offsetFancyPicker.ConfigConst64(offset));
    }
    if (OK != payloadFancyPicker.FromJsval(aPayload))
    {
        UINT64 payload;
        C_CHECK_RC(FromJsval(aPayload, &payload, 4));
        C_CHECK_RC(payloadFancyPicker.ConfigConst64(payload));
    }
    C_CHECK_RC(FromJsval(aReduction, &reduction, 5));
    C_CHECK_RC(FromJsval(aReductionType, &reductionType, 6));

    RETURN_ACTION(new PmAction_HostSemaphoreReduction(
                      pChannelDesc, pSurfaceDesc, offsetFancyPicker,
                      payloadFancyPicker, (Channel::Reduction)reduction,
                      (Channel::ReductionType)reductionType,
                      m_Scopes.top().m_ChannelSubdevMask,
                      m_Scopes.top().m_SemRelFlags,
                      m_Scopes.top().m_pRemoteGpuDesc));
}

PM_BIND(Action, HostSemaphoreReduction, 6,
        "Usage: Action.HostSemaphoreReduction(chan, surf, offset, payload, reduction, reductionType)",
        "Insert a host semaphore-reduction operation in the pushbuffer.");

//--------------------------------------------------------------------
//! \brief Implement Action.EngineSemaphoreRelease() token
//!
JSBool PmParser::Action_EngineSemaphoreRelease
(
    jsval channelDesc,
    jsval surfaceDesc,
    jsval aOffset,
    jsval aPayload,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT64 payload;
    RC rc;
    FancyPicker    offsetFancyPicker;
    FancyPicker    payloadFancyPicker;

    ChannelWrapperMgr::Instance()->UseSemaphoreWrapper();
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 2));
    if (OK != offsetFancyPicker.FromJsval(aOffset))
    {
        C_CHECK_RC(FromJsval(aOffset, &offset, 3));
        C_CHECK_RC(offsetFancyPicker.ConfigConst(offset));
    }
    if (OK != payloadFancyPicker.FromJsval(aPayload))
    {
        C_CHECK_RC(FromJsval(aPayload, &payload, 4));
        C_CHECK_RC(payloadFancyPicker.ConfigConst64(payload));
    }
    RETURN_ACTION(new PmAction_EngineSemaphoreOperation(pChannelDesc,
                                          pSurfaceDesc,
                                          offsetFancyPicker,
                                          payloadFancyPicker,
                                          m_Scopes.top().m_ChannelSubdevMask,
                                          m_Scopes.top().m_SemRelFlags,
                                          m_Scopes.top().m_pRemoteGpuDesc,
                                          PmAction_EngineSemaphoreOperation::
                                          SEMAPHORE_OPERATION_RELEASE,
                                          "Action.EngineSemaphoreRelease"));
}

PM_BIND(Action, EngineSemaphoreRelease, 4,
        "Usage: Action.EngineSemaphoreRelease(channels, surfaces, offset, payload)",
        "Insert an engine semaphore-release operation in the pushbuffer.");

//--------------------------------------------------------------------
//! \brief Implement Action.EngineSemaphoreAcquire() token
//!
JSBool PmParser::Action_EngineSemaphoreAcquire
(
    jsval channelDesc,
    jsval surfaceDesc,
    jsval aOffset,
    jsval aPayload,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT64 payload;
    RC rc;
    FancyPicker    offsetFancyPicker;
    FancyPicker    payloadFancyPicker;

    ChannelWrapperMgr::Instance()->UseSemaphoreWrapper();
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 2));
    if (OK != offsetFancyPicker.FromJsval(aOffset))
    {
        C_CHECK_RC(FromJsval(aOffset, &offset, 3));
        C_CHECK_RC(offsetFancyPicker.ConfigConst(offset));
    }
    if (OK != payloadFancyPicker.FromJsval(aPayload))
    {
        C_CHECK_RC(FromJsval(aPayload, &payload, 4));
        C_CHECK_RC(payloadFancyPicker.ConfigConst64(payload));
    }
    RETURN_ACTION(new PmAction_EngineSemaphoreOperation(pChannelDesc,
                                          pSurfaceDesc,
                                          offsetFancyPicker,
                                          payloadFancyPicker,
                                          m_Scopes.top().m_ChannelSubdevMask,
                                          m_Scopes.top().m_SemRelFlags,
                                          m_Scopes.top().m_pRemoteGpuDesc,
                                          PmAction_EngineSemaphoreOperation::
                                          SEMAPHORE_OPERATION_ACQUIRE,
                                          "Action.EngineSemaphoreAcquire"));
}

PM_BIND(Action, EngineSemaphoreAcquire, 4,
        "Usage: Action.EngineSemaphoreAcquire(channels, surfaces, offset, payload)",
        "Insert an engine semaphore-acquire operation in the pushbuffer.");

//--------------------------------------------------------------------
//! \brief Implement Action.NonStallInt() token
//!
JSBool PmParser::Action_NonStallInt
(
    jsval aIntName,
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    string intName;
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(aIntName,    &intName,      1));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 2));
    C_CHECK_RC(CheckForIdentifier(intName, 1));
    RETURN_ACTION(new PmAction_NonStallInt(intName, pChannelDesc,
                                           m_Scopes.top().m_EnableNsiSemaphore,
                                           m_Scopes.top().m_EnableNsiInt));
}

PM_BIND(Action, NonStallInt, 2,
        "Usage: Action.NonStallInt(intName, channels)",
        "Write a non-stalling interrupt on the indicated channels.");

//--------------------------------------------------------------------
//! \brief Implement Action.EngineNonStallInt() token
//!
JSBool PmParser::Action_EngineNonStallInt
(
    jsval aIntName,
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    string intName;
    PmChannelDesc *pChannelDesc;
    RC rc;

    ChannelWrapperMgr::Instance()->UseSemaphoreWrapper();
    C_CHECK_RC(FromJsval(aIntName,    &intName,      1));
    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 2));
    C_CHECK_RC(CheckForIdentifier(intName, 1));
    RETURN_ACTION(new PmAction_EngineNonStallInt(intName, pChannelDesc));
}

PM_BIND(Action, EngineNonStallInt, 2,
        "Usage: Action.EngineNonStallInt(intName, channels)",
        "Write an engine-specific trap on the indicated channels.");

//--------------------------------------------------------------------
//! \brief Implement Action.InsertMethods() token
//!
JSBool PmParser::Action_InsertMethods
(
    jsval aChannelDesc,
    jsval aNumMethods,
    jsval aMethodPicker,
    jsval aDataPicker,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    UINT32 numMethods;
    FancyPicker methodPicker;
    FancyPicker dataPicker;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(aChannelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aNumMethods,  &numMethods,   2));
    C_CHECK_RC(methodPicker.FromJsval(aMethodPicker));
    C_CHECK_RC(dataPicker.FromJsval(aDataPicker));
    RETURN_ACTION(new PmAction_InsertMethods(
                      pChannelDesc, PmAction_InsertMethods::LWRRENT_SUBCH,
                      numMethods, methodPicker, dataPicker,
                      m_Scopes.top().m_ChannelSubdevMask));
}

PM_BIND(
    Action, InsertMethods, 4,
    "Usage: Action.InsertMethods(chan, numMethods, methodPicker, dataPicker)",
    "Write method/data pairs from the fpickers into the channel.");

//--------------------------------------------------------------------
//! \brief Implement Action.InsertSubchMethods() token
//!
JSBool PmParser::Action_InsertSubchMethods
(
    jsval aChannelDesc,
    jsval aSubch,
    jsval aNumMethods,
    jsval aMethodPicker,
    jsval aDataPicker,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    UINT32 subch;
    UINT32 numMethods;
    FancyPicker methodPicker;
    FancyPicker dataPicker;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    C_CHECK_RC(FromJsval(aChannelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(aSubch, &subch,              2));
    C_CHECK_RC(FromJsval(aNumMethods,  &numMethods,   3));
    C_CHECK_RC(methodPicker.FromJsval(aMethodPicker));
    C_CHECK_RC(dataPicker.FromJsval(aDataPicker));
    RETURN_ACTION(new PmAction_InsertMethods(
                      pChannelDesc, subch,
                      numMethods, methodPicker, dataPicker,
                      m_Scopes.top().m_ChannelSubdevMask));
}

PM_BIND(
    Action, InsertSubchMethods, 5,
    "Usage: Action.InsertSubchMethods(chan, subch, numMethods, methodPicker, dataPicker)",
    "Write method/data pairs from the fpickers to a channel/subchannel.");

//--------------------------------------------------------------------
//! \brief Implement Action.PowerWait() token
//!
JSBool PmParser::Action_PowerWait(jsval *pReturlwalue)
{
    RETURN_ACTION(new PmAction_PowerWait(
            m_Scopes.top().m_PowerWaitModelUs,
            m_Scopes.top().m_PowerWaitRTLUs,
            m_Scopes.top().m_PowerWaitHWUs,
            m_Scopes.top().m_PowerWaitBusy));
}

PM_BIND(Action, PowerWait, 0, "Usage: Action.PowerWait()",
        "Wait for the power to become gated.");

//--------------------------------------------------------------------
//! \brief Implement Action.EscapeRead() token
//!
JSBool PmParser::Action_EscapeRead
(
    jsval aPath,
    jsval aIndex,
    jsval aSize,
    jsval *pReturlwalue
)
{
    string path;
    UINT32 index;
    UINT32 size;
    RC rc;

    C_CHECK_RC(FromJsval(aPath,   &path,      1));
    C_CHECK_RC(FromJsval(aIndex,  &index,     2));
    C_CHECK_RC(FromJsval(aSize,   &size,      3));
    RETURN_ACTION(new PmAction_EscapeRead(NULL, path, index, size));
}

PM_BIND(Action, EscapeRead, 3, "Usage: Action.EscapeRead(path, index, size)",
        "Perform an escape read, results are logged by the gilder.");

//--------------------------------------------------------------------
//! \brief Implement Action.EscapeWrite() token
//!
JSBool PmParser::Action_EscapeWrite
(
    jsval aPath,
    jsval aIndex,
    jsval aSize,
    jsval aData,
    jsval *pReturlwalue
)
{
    string path;
    UINT32 index;
    UINT32 size;
    UINT32 data;
    RC rc;

    C_CHECK_RC(FromJsval(aPath,   &path,      1));
    C_CHECK_RC(FromJsval(aIndex,  &index,     2));
    C_CHECK_RC(FromJsval(aSize,   &size,      3));
    C_CHECK_RC(FromJsval(aData,   &data,      4));
    RETURN_ACTION(new PmAction_EscapeWrite(NULL, path, index, size, data));
}

PM_BIND(Action, EscapeWrite, 4, "Usage: Action.EscapeWrite(path, index, size, data)",
        "Perform an escape write.");

//--------------------------------------------------------------------
//! \brief Implement Action.GpuEscapeRead() token
//!
JSBool PmParser::Action_GpuEscapeRead
(
    jsval gpuDesc,
    jsval aPath,
    jsval aIndex,
    jsval aSize,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    string path;
    UINT32 index;
    UINT32 size;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc,  1));
    C_CHECK_RC(FromJsval(aPath,   &path,      2));
    C_CHECK_RC(FromJsval(aIndex,  &index,     3));
    C_CHECK_RC(FromJsval(aSize,   &size,      4));
    RETURN_ACTION(new PmAction_EscapeRead(pGpuDesc, path, index, size));
}

PM_BIND(Action, GpuEscapeRead, 4, "Usage: Action.GpuEscapeRead(gpus, path, index, size)",
        "Perform an escape read (on a specific gpu), results are logged by the gilder.");

//--------------------------------------------------------------------
//! \brief Implement Action.GpuEscapeWrite() token
//!
JSBool PmParser::Action_GpuEscapeWrite
(
    jsval gpuDesc,
    jsval aPath,
    jsval aIndex,
    jsval aSize,
    jsval aData,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    string path;
    UINT32 index;
    UINT32 size;
    UINT32 data;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc,  1));
    C_CHECK_RC(FromJsval(aPath,   &path,      2));
    C_CHECK_RC(FromJsval(aIndex,  &index,     3));
    C_CHECK_RC(FromJsval(aSize,   &size,      4));
    C_CHECK_RC(FromJsval(aData,   &data,      5));
    RETURN_ACTION(new PmAction_EscapeWrite(pGpuDesc, path, index, size, data));
}

PM_BIND(Action, GpuEscapeWrite, 5, "Usage: Action.GpuEscapeWrite(gpus, path, index, size, data)",
        "Perform an escape write on a specific gpu.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetPState() token
//!
JSBool PmParser::Action_SetPState
(
    jsval gpuDesc,
    jsval aPstate,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    UINT32     pstate;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(aPstate, &pstate,   2));
    RETURN_ACTION(new PmAction_SetPState(pGpuDesc, pstate,
                                         m_Scopes.top().m_PStateFallback));
}

PM_BIND(Action, SetPState, 2, "Usage: Action.SetPState(gpus, pstate)",
        "Set the current performance state.");

//--------------------------------------------------------------------
//! \brief Implement Action.AbortTests() token
//!
JSBool PmParser::Action_AbortTests(jsval testDesc, jsval *pReturlwalue)
{
    PmTestDesc *pTestDesc;
    RC rc;

    C_CHECK_RC(FromJsval(testDesc, &pTestDesc,  1));
    RETURN_ACTION(new PmAction_AbortTests(pTestDesc));
}

PM_BIND(Action, AbortTests, 1, "Usage: Action.AbortTest(test)",
        "Abort the indicated test(s).");

//--------------------------------------------------------------------
//! \brief Implement Action.AbortTest() token
//!
JSBool PmParser::Action_AbortTest(jsval *arglist, uint32 numArgs, jsval *pReturlwalue)
{
    RC rc;
    UINT32 ScriptRC = 0;

    switch (numArgs)
    {
        case 0:
            break;
        case 1:
            C_CHECK_RC(FromJsval(arglist[0], &ScriptRC, 1));
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: Action.AbortTest(<rc>)\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }

    RETURN_ACTION(new PmAction_AbortTest());
}

PM_BIND_VAR(Action, AbortTest, 1, "Usage: Action.AbortTest(<rc>)",
        "Abort the faulting test optionally specify rc failure.");

//--------------------------------------------------------------------
//! \brief Implement Action.WaitForSemaphoreRelease() token
//!
JSBool PmParser::Action_WaitForSemaphoreRelease
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aValue,
    jsval  aTimeout,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32         offset;
    UINT64         value;
    FLOAT64        timeoutMs;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset,           2));
    C_CHECK_RC(FromJsval(aValue, &value,             3));
    C_CHECK_RC(FromJsval(aTimeout, &timeoutMs,       4));
    RETURN_ACTION(new PmAction_WaitForSemaphoreRelease(pSurfaceDesc,
                                                       offset,
                                                       value,
                                                       timeoutMs));
}

PM_BIND(Action, WaitForSemaphoreRelease, 4,
        "Usage: Action.WaitForSemaphoreRelease(surfaces, offset, payload, timeoutMs)",
        "Wait for a specified 32-bit or 64-bit value to be present on all surfaces");

//--------------------------------------------------------------------
//! \brief Implement Action.FreezeHost() token
//!
JSBool PmParser::Action_FreezeHost
(
    jsval gpuDesc,
    jsval aTimeout,
    jsval *pReturlwalue
 )
{
    PmGpuDesc *pGpuDesc;
    FLOAT64 timeoutMs;
    RC rc;
    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(aTimeout, &timeoutMs, 2));
    RETURN_ACTION(new PmAction_FreezeHost(pGpuDesc, timeoutMs));
}

PM_BIND(Action, FreezeHost, 2, "Usage: Action.FreezeHost(gpusubDev, timeoutMs)",
        "Freeze host via LW_PFIFO_FREEZE register and wait freeze done.");

//--------------------------------------------------------------------
//! \brief Implement Action.UnFreezeHost() token
//!
JSBool PmParser::Action_UnFreezeHost
(
    jsval gpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;
    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_UnFreezeHost(pGpuDesc));
}

PM_BIND(Action, UnFreezeHost, 1, "Usage: Action.UnFreezeHost(gpusubDev)",
        "UnFreeze host via LW_PFIFO_FREEZE register.");

//--------------------------------------------------------------------
//! \brief Implement Action.SwReset() token
//!
JSBool PmParser::Action_SwReset(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableGpuReset(true));
    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_ResetGpuDevice(
                      "Action.SwReset", pGpuDesc,
                      LW0080_CTRL_BIF_RESET_FLAGS_TYPE_SW_RESET));
}

PM_BIND(Action, SwReset, 1,
        "Usage: Action.SwReset(gpu)",
        "Reset the GPU device.  Usually requires aborting test.");

//--------------------------------------------------------------------
//! \brief Implement Action.SecondaryBusReset() token
//!
JSBool PmParser::Action_SecondaryBusReset(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableGpuReset(true));
    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_ResetGpuDevice(
                      "Action.SecondaryBusReset", pGpuDesc,
                      LW0080_CTRL_BIF_RESET_FLAGS_TYPE_SBR));
}

PM_BIND(Action, SecondaryBusReset, 1,
        "Usage: Action.SecondaryBusReset(gpu)",
        "Reset the GPU device.  Usually requires aborting test.");

//--------------------------------------------------------------------
//! \brief Implement Action.FundamentalReset() token
//!
JSBool PmParser::Action_FundamentalReset(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableGpuReset(true));
    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_ResetGpuDevice(
                      "Action.FundamentalReset", pGpuDesc,
                      LW0080_CTRL_BIF_RESET_FLAGS_TYPE_FUNDAMENTAL));
}

PM_BIND(Action, FundamentalReset, 1,
        "Usage: Action.FundamentalReset(gpu)",
        "Reset the GPU device.  Usually requires aborting test.");

//--------------------------------------------------------------------
//! \brief Implement Action.PmAction_PhysWr32() token
//!
JSBool PmParser::Action_PhysWr32
(
    jsval addr,
    jsval value,
    jsval *pReturlwalue
)
{
    RC rc;
    UINT64 physAddr;
    UINT32 data;

    C_CHECK_RC(FromJsval(addr, &physAddr, 1));
    C_CHECK_RC(FromJsval(value, &data, 2));
    RETURN_ACTION(new PmAction_PhysWr32(physAddr, data));
}

PM_BIND(Action, PhysWr32, 2, "Usage: Action.PhysWr32(addr, value)",
        "Write data to specified physical address.");

//--------------------------------------------------------------------
//! \brief Implement Action.PriWriteReg32() token
//!
JSBool PmParser::Action_PriWriteReg32(jsval *arglist, uint32 numArgs, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    string RegName;
    vector<UINT32> RegIndexes;
    UINT32 RegVal;
    string RegSpace = "";
    PmChannelDesc *pChanDesc = nullptr;
    RC rc;

    switch(numArgs)
    {
        case 3:
            C_CHECK_RC(FromJsval(arglist[0], &pGpuDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &RegName, 2));
            C_CHECK_RC(FromJsval(arglist[2], &RegVal, 3));
            pChanDesc = m_Scopes.top().m_pChannelDesc;
            break;
        case 4:
            C_CHECK_RC(FromJsval(arglist[0], &pGpuDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &RegName, 2));
            C_CHECK_RC(FromJsval(arglist[2], &RegVal, 3));
            C_CHECK_RC(FromJsval(arglist[3], &RegSpace, 4));
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: Action.PriWriteReg32(gpus, regname, value, <regspace>). "
                      "And regspace is alternative.\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }

   C_CHECK_RC(ParsePriReg(&RegName, &RegIndexes));
   RETURN_ACTION(new PmAction_PriWriteReg32(pGpuDesc, m_Scopes.top().m_pSmcEngineDesc, pChanDesc, RegName, RegIndexes,
                                             RegVal, RegSpace));
}

PM_BIND_VAR(Action, PriWriteReg32, 4, "Usage: Action.PriWriteReg32(gpus, regname, value, <regspace>)",
        "Write data to PRI register specified by name.");

//--------------------------------------------------------------------
//! \brief Implement Action.PriWriteField32() token
//!

JSBool PmParser::Action_PriWriteField32(jsval *arglist, uint32 numArgs, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    string RegName;
    vector<UINT32> RegIndexes;
    string RegFieldName;
    UINT32 FieldVal;
    string RegSpace = "";
    PmChannelDesc *pChanDesc = nullptr;
    RC rc;

    switch(numArgs)
    {
        case 4:
            C_CHECK_RC(FromJsval(arglist[0], &pGpuDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &RegName, 2));
            C_CHECK_RC(FromJsval(arglist[2], &RegFieldName, 3));
            C_CHECK_RC(FromJsval(arglist[3], &FieldVal, 4));
            pChanDesc = m_Scopes.top().m_pChannelDesc;
            break;
        case 5:
            C_CHECK_RC(FromJsval(arglist[0], &pGpuDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &RegName, 2));
            C_CHECK_RC(FromJsval(arglist[2], &RegFieldName, 3));
            C_CHECK_RC(FromJsval(arglist[3], &FieldVal, 4));
            C_CHECK_RC(FromJsval(arglist[4], &RegSpace, 5));
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: Action.PriWriteField32(gpus, regname, fieldname, value, <regspace>)"
                      "And regspace is alternative.\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }

    C_CHECK_RC(ParsePriReg(&RegName, &RegIndexes));
    RETURN_ACTION(new PmAction_PriWriteField32(pGpuDesc, m_Scopes.top().m_pSmcEngineDesc, pChanDesc, RegName, RegIndexes,
                                               RegFieldName, FieldVal, RegSpace));
}

PM_BIND_VAR(Action, PriWriteField32, 5,
        "Usage: Action.PriWriteField32(gpus, regname, fieldname, value, <regspace>)",
        "Write data to PRI register field specified by name.");

//--------------------------------------------------------------------
//! \brief Implement Action.PriWriteMask32() token
//!

JSBool PmParser::Action_PriWriteMask32(jsval *arglist, uint32 numArgs, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    string RegName;
    vector<UINT32> RegIndexes;
    UINT32 AndMask;
    UINT32 RegVal;
    string RegSpace = "";
    PmChannelDesc *pChanDesc = nullptr;
    RC rc;

    switch(numArgs)
    {
        case 4:
            C_CHECK_RC(FromJsval(arglist[0], &pGpuDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &RegName, 2));
            C_CHECK_RC(FromJsval(arglist[2], &AndMask, 3));
            C_CHECK_RC(FromJsval(arglist[3], &RegVal, 4));
            pChanDesc = m_Scopes.top().m_pChannelDesc;
            break;
        case 5:
            C_CHECK_RC(FromJsval(arglist[0], &pGpuDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &RegName, 2));
            C_CHECK_RC(FromJsval(arglist[2], &AndMask, 3));
            C_CHECK_RC(FromJsval(arglist[3], &RegVal, 4));
            C_CHECK_RC(FromJsval(arglist[4], &RegSpace, 5));
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: Action.PriWriteMask32(gpus, regname, fieldname, value, <regspace>)"
                      "And regspace is alternative.\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }

    C_CHECK_RC(ParsePriReg(&RegName, &RegIndexes));
    RETURN_ACTION(new PmAction_PriWriteMask32(pGpuDesc, m_Scopes.top().m_pSmcEngineDesc, pChanDesc, RegName, RegIndexes,
                                               AndMask, RegVal, RegSpace));
}

PM_BIND_VAR(Action, PriWriteMask32, 5,
        "Usage: Action.PriWriteMask32(gpus, regname, andmask, value, <regspace>)",
        "Write masked data to PRI register specified by name.");

//--------------------------------------------------------------------
//! \brief Implement Action.WaitPriReg32() token
//!
JSBool PmParser::Action_WaitPriReg32(jsval *arglist, uint32 numArgs, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    string RegName;
    vector<UINT32> RegIndexes;
    UINT32 RegMask;
    UINT32 RegVal;
    FLOAT64 TimeoutMs;
    string RegSpace = "";
    PmChannelDesc *pChanDesc = nullptr;
    RC rc;

    switch(numArgs)
    {
        case 5:
            C_CHECK_RC(FromJsval(arglist[0], &pGpuDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &RegName, 2));
            C_CHECK_RC(FromJsval(arglist[2], &RegMask, 3));
            C_CHECK_RC(FromJsval(arglist[3], &RegVal, 4));
            C_CHECK_RC(FromJsval(arglist[4], &TimeoutMs, 5));
            pChanDesc = m_Scopes.top().m_pChannelDesc;
            break;
        case 6:
            C_CHECK_RC(FromJsval(arglist[0], &pGpuDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &RegName, 2));
            C_CHECK_RC(FromJsval(arglist[2], &RegMask, 3));
            C_CHECK_RC(FromJsval(arglist[3], &RegVal, 4));
            C_CHECK_RC(FromJsval(arglist[4], &TimeoutMs, 5));
            C_CHECK_RC(FromJsval(arglist[5], &RegSpace, 6));
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: Action.WaitPriReg32(gpus, regname, mask, value, timeoutms, <regspace>)"
                      "And regspace is alternative.\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }

    C_CHECK_RC(ParsePriReg(&RegName, &RegIndexes));
    RETURN_ACTION(new PmAction_WaitPriReg32(pGpuDesc, m_Scopes.top().m_pSmcEngineDesc, pChanDesc, RegName, RegIndexes,
                                            RegMask, RegVal, TimeoutMs, RegSpace));
}

PM_BIND_VAR(Action, WaitPriReg32, 6,
        "Usage: Action.WaitPriReg32(gpus, regname, mask, value, timeoutms, <regspace>)",
        "Wait for a PRI register to be the desired masked value within "
        "the timeout.");

//--------------------------------------------------------------------
//! \brief Implement Action.StartTimer() token
//!
JSBool PmParser::Action_StartTimer
(
    jsval aTimerName,
    jsval *pReturlwalue
)
{
    string TimerName;
    RC rc;

    C_CHECK_RC(FromJsval(aTimerName, &TimerName, 1));
    C_CHECK_RC(CheckForIdentifier(TimerName, 1));
    RETURN_ACTION(new PmAction_StartTimer(TimerName));
}

PM_BIND(Action, StartTimer, 1, "Usage: Action.StartTimer(timerName)",
        "Start the indicated timer.  Used with Trigger.OnTimeUs().");

//--------------------------------------------------------------------
//! \brief Implement Action.FillSurface() token
//!
JSBool PmParser::Action_FillSurface
(
    jsval surfaceDesc,
    jsval aOffset,
    jsval aSize,
    jsval aData,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32         size;
    FancyPicker    dataPicker;
    UINT32         data;
    FancyPicker    offsetPicker;
    UINT32         offset;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    if (OK != offsetPicker.FromJsval(aOffset))
    {
        C_CHECK_RC(FromJsval(aOffset, &offset, 2));
        C_CHECK_RC(offsetPicker.ConfigConst(offset));
    }
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    if(OK != dataPicker.FromJsval(aData))
    {
        C_CHECK_RC(FromJsval(aData, &data, 4));
        C_CHECK_RC(dataPicker.ConfigConst(data));
    }
    RETURN_ACTION(new PmAction_FillSurface(
        pSurfaceDesc, offsetPicker, size, dataPicker,
        m_Scopes.top().m_InbandSurfaceClear,
        m_Scopes.top().m_pInbandChannel));
}

PM_BIND(Action, FillSurface, 4,"Usage: Action.FillSurface(surfaces,offset,size,data)",
        "Write the provided data to the surface range specified by offset and size.");

//--------------------------------------------------------------------
//! \brief Implement Action.CpuMapSurfacesDirect() token
//!
JSBool PmParser::Action_CpuMapSurfacesDirect
(
    jsval surfaceDesc,
    jsval *pReturlwalue
)
{
    RC rc;
    PmSurfaceDesc *pSurfaceDesc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));

    RETURN_ACTION(new PmAction_CpuMapSurfacesDirect(pSurfaceDesc));
}

PM_BIND(Action, CpuMapSurfacesDirect, 1,"Usage: Action.CpuMapSurfacesDirect(surfaces)",
        "Use direct-map mode to map cpu address of the specified surface.");

//--------------------------------------------------------------------
//! \brief Implement Action.CpuMapSurfacesReflected() token
//!
JSBool PmParser::Action_CpuMapSurfacesReflected
(
    jsval surfaceDesc,
    jsval *pReturlwalue
)
{
    RC rc;
    PmSurfaceDesc *pSurfaceDesc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));

    RETURN_ACTION(new PmAction_CpuMapSurfacesReflected(pSurfaceDesc));
}

PM_BIND(Action, CpuMapSurfacesReflected, 1,"Usage: Action.CpuMapSurfacesReflected(surfaces)",
        "Use reflected-map mode to map cpu address of the specified surface.");

//--------------------------------------------------------------------
//! \brief Implement Action.CpuMapSurfacesDefault() token
//!
JSBool PmParser::Action_CpuMapSurfacesDefault
(
    jsval surfaceDesc,
    jsval *pReturlwalue
)
{
    RC rc;
    PmSurfaceDesc *pSurfaceDesc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));

    RETURN_ACTION(new PmAction_CpuMapSurfacesDefault(pSurfaceDesc));
}

PM_BIND(Action, CpuMapSurfacesDefault, 1,"Usage: Action.CpuMapSurfacesDefault(surfaces)",
        "Use default-map mode to map cpu address of the specified surface.");

//--------------------------------------------------------------------
//! \brief Implement Action.GpuSetClock() token
//!
JSBool PmParser::Action_GpuSetClock
(
    jsval gpuDesc,
    jsval aClkDomainName,
    jsval aTargetHz,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    string clkDomainName;
    UINT32 targetHz;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(aClkDomainName, &clkDomainName, 2));
    C_CHECK_RC(FromJsval(aTargetHz, &targetHz, 3));
    RETURN_ACTION(new PmAction_GpuSetClock(pGpuDesc, clkDomainName, targetHz));
}

PM_BIND(Action, GpuSetClock, 3, "Usage: Action.GpuSetClock(gpus,clkdmnname,valueHz)",
        "Set clk(Hz) in sepcified clock domain.");

//--------------------------------------------------------------------
//! \brief Implement Action.StopFb() token
//!
JSBool PmParser::Action_StopFb
(
    jsval gpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_FbSetState("Action.StopFb", pGpuDesc,
                                          LW208F_CTRL_FB_SET_STATE_STOPPED));
}

PM_BIND(Action, StopFb, 1, "Usage: Action.StopFb(gpu)",
        "Stop/halt the fb engine on the indicated gpu(s).");

//--------------------------------------------------------------------
//! \brief Implement Action.RestartFb() token
//!
JSBool PmParser::Action_RestartFb
(
    jsval gpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_FbSetState("Action.RestartFb", pGpuDesc,
                                          LW208F_CTRL_FB_SET_STATE_RESTART));
}

PM_BIND(Action, RestartFb, 1, "Usage: Action.RestartFb(gpu)",
        "Restart the fb engine from a stopped state on the indicated gpu(s).");

//--------------------------------------------------------------------
//! \brief Implement Action.EnableElpg(Gpu, Engine, On/Off) token
//!
JSBool PmParser::Action_EnableElpg
(
    jsval gpuDesc,
    jsval aEngineId,
    jsval aState,
    jsval *pReturlwalue
)
{
    RC rc;
    PmGpuDesc *pGpuDesc;
    UINT32     EngineId;
    bool       ToEnableElpg;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(aEngineId, &EngineId, 2));
    C_CHECK_RC(FromJsval(aState, &ToEnableElpg, 3));
    RETURN_ACTION(new PmAction_EnableElpg("Action.EnableElpg",
                                          pGpuDesc,
                                          EngineId,
                                          ToEnableElpg));
}

PM_BIND(Action, EnableElpg, 3, "Usage: Action.EnableElpg(gpus,EngineId,ToEnableElpg)",
        "Enable or disable an ELPG on GPU");

//--------------------------------------------------------------------
//! \brief Implement Action.RailGateGpu() token
//!
JSBool PmParser::Action_RailGateGpu
(
    jsval gpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_GpuRailGateAction("Action.RailGateGpu",
        pGpuDesc, LW2080_CTRL_GPU_POWER_ON_OFF_RG_SAVE));
}

PM_BIND(Action, RailGateGpu, 1, "Usage: Action.RailGateGpu(gpu)",
        "Perform railgate on the indicated gpu(s).");

//--------------------------------------------------------------------
//! \brief Implement Action.UnRailGateGpu() token
//!
JSBool PmParser::Action_UnRailGateGpu
(
    jsval gpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_GpuRailGateAction("Action.UnRailGateGpu",
        pGpuDesc, LW2080_CTRL_GPU_POWER_ON_OFF_RG_RESTORE));
}

PM_BIND(Action, UnRailGateGpu, 1, "Usage: Action.UnRailGateGpu(gpu)",
        "Perform un-railgate on the indicated gpu(s).");

//--------------------------------------------------------------------
//! \brief Implement Action.SetGpioUp() token
//!
JSBool PmParser::Action_SetGpioUp
(
    jsval channelDesc,
    jsval aGpioPorts,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    vector<UINT32> gpioPorts;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    if (JSVAL_IS_NUMBER(aGpioPorts))
    {
        gpioPorts.resize(1);
        C_CHECK_RC(FromJsval(aGpioPorts, &gpioPorts[0], 2));
    }
    else
    {
        C_CHECK_RC(FromJsval(aGpioPorts, &gpioPorts, 2));
    }

    RETURN_ACTION(new PmAction_SetGpioUp(pChannelDesc, gpioPorts));
}

PM_BIND(Action, SetGpioUp, 2,
        "Usage: Action.SetGpioUp(channelDesc, gpioPorts)",
        "Set Gpio ports up.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetGpioDown() token
//!
JSBool PmParser::Action_SetGpioDown
(
    jsval channelDesc,
    jsval aGpioPorts,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    vector<UINT32> gpioPorts;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    if (JSVAL_IS_NUMBER(aGpioPorts))
    {
        gpioPorts.resize(1);
        C_CHECK_RC(FromJsval(aGpioPorts, &gpioPorts[0], 2));
    }
    else
    {
        C_CHECK_RC(FromJsval(aGpioPorts, &gpioPorts, 2));
    }

    RETURN_ACTION(new PmAction_SetGpioDown(pChannelDesc, gpioPorts));
}

PM_BIND(Action, SetGpioDown, 2,
        "Usage: Action.SetGpioDown(channelDesc, gpioPorts)",
        "Set Gpio ports down.");

//--------------------------------------------------------------------
//! \brief Implement Action.CreateChannel() token
//!
JSBool PmParser::Action_CreateChannel
(
    jsval  aName,
    jsval  aGpuDesc,
    jsval  aTestDesc,
    jsval *pReturlwalue
)
{
    string name;
    PmGpuDesc *pGpuDesc;
    PmTestDesc *pTestDesc;
    RC rc;

    C_CHECK_RC(FromJsval(aName,              &name,                1));
    C_CHECK_RC(FromJsval(aGpuDesc,           &pGpuDesc,            2));
    C_CHECK_RC(FromJsval(aTestDesc,          &pTestDesc,           3));
    C_CHECK_RC(CheckForIdentifier(name, 1));

    RETURN_ACTION(new PmAction_CreateChannel(name,
                pGpuDesc,
                pTestDesc,
                m_Scopes.top().m_pVaSpaceDesc,
                m_Scopes.top().m_DefEngineName,
                m_Scopes.top().m_pSmcEngineDesc));
}

PM_BIND(Action, CreateChannel, 3,
        "Usage: Action.CreateChannel(name, gpu, test)",
        "Create a channel.");

//--------------------------------------------------------------------
//! \brief Implement Action.CreateSubchannel() token
//!
JSBool PmParser::Action_CreateSubchannel
(
    jsval  aChannelDesc,
    jsval  aHwClass,
    jsval  aSubchannelNumber,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    UINT32 hwClass;
    UINT32 subchannelNumber;
    RC rc;

    C_CHECK_RC(FromJsval(aChannelDesc,       &pChannelDesc,        1));
    C_CHECK_RC(FromJsval(aHwClass,           &hwClass,             2));
    C_CHECK_RC(FromJsval(aSubchannelNumber,  &subchannelNumber,    3));

    RETURN_ACTION(new PmAction_CreateSubchannel(
                    pChannelDesc,
                    hwClass,
                    subchannelNumber,
                    m_Scopes.top().m_pSmcEngineDesc));
}

PM_BIND(Action, CreateSubchannel, 3,
        "Usage: Action.CreateSubchannel(name, HW class, subchannel number)",
        "Create a subchannel on an existing channel.");

//--------------------------------------------------------------------
//! \brief Implement Action.CreateCESubchannel() token
//!
JSBool PmParser::Action_CreateCESubchannel
(
    jsval  aChannelDesc,
    jsval *pReturlwalue
)
{
    RC rc;
    PmChannelDesc *pChannelDesc;

    C_CHECK_RC(FromJsval(aChannelDesc, &pChannelDesc, 1));

    UINT32 ceAllocInst = 0;
    if (m_Scopes.top().m_CEAllocMode == PolicyManager::CEAllocAsync)
    {
        MASSERT(m_Scopes.top().m_pCEAllocInst != NULL);
        ceAllocInst = m_Scopes.top().m_pCEAllocInst->Pick();
        m_Scopes.top().m_FpContext.LoopNum ++;
    }

    RETURN_ACTION(new PmAction_CreateCESubchannel(
                    pChannelDesc,
                    ceAllocInst,
                    m_Scopes.top().m_CEAllocMode,
                    m_Scopes.top().m_pSmcEngineDesc));
}

PM_BIND(Action, CreateCESubchannel, 1,
        "Usage: Action.CreateCESubchannel(PmChannelDesc)",
        "Create a CE subchannel on an existing channel.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetFaultBufferGetPointerUpdate() token
//!
JSBool PmParser::Action_SetUpdateFaultBufferGetPointer
(
    jsval aFlag,
    jsval *pReturlwalue
)
{
    bool flag;
    RC rc;

    C_CHECK_RC(FromJsval(aFlag, &flag, 1));
    RETURN_ACTION(new PmAction_SetUpdateFaultBufferGetPointer(flag));
}

PM_BIND(Action, SetUpdateFaultBufferGetPointer, 1,
        "Usage: Policy.SetUpdateFaultBufferGetPointer(flag)",
        "Enable/disable updating the fault buffer get pointer when "
        "processing replayable faults."
        "  Default: true");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearFaultBuffer() token
//!
JSBool PmParser::Action_ClearFaultBuffer
(
    jsval gpuDesc,
    jsval *pReturlwalue
)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_ACTION(new PmAction_ClearFaultBuffer(pGpuDesc));
}

PM_BIND(Action, ClearFaultBuffer, 1, "Usage: Action.ClearFaultBuffer(gpu)",
    "Clear the fault event buffer by updating the get pointer to be "
    "equal to the put pointer.");

//--------------------------------------------------------------------
//! \brief Implement Action.SendTraceEvent() token
//!
JSBool PmParser::Action_SendTraceEvent
(
    jsval aeventName,
    jsval surfaceDesc,
    jsval *pReturlwalue
)
{
    string eventName;
    PmSurfaceDesc *pSurfaceDesc;
    RC rc;

    C_CHECK_RC(FromJsval(aeventName,&eventName,1));
    C_CHECK_RC(FromJsval(surfaceDesc,&pSurfaceDesc,2));

    RETURN_ACTION(new PmAction_SendTraceEvent(eventName,pSurfaceDesc));
}

PM_BIND(Action, SendTraceEvent, 2, "Usage: Action.SendTraceEvent(eventName,surface)",
    "Send trace event to plugin.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetAtsReadPermission() token
//!
JSBool PmParser::Action_SetAtsReadPermission
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval  aPermission,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    bool permission;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aPermission, &permission, 4));
    RETURN_ACTION(new PmAction_SetAtsPermission(
        pSurfaceDesc,
        offset,
        size,
        "read",
        permission));
}

PM_BIND(Action, SetAtsReadPermission, 4,
        "Usage: Action.SetAtsReadPermission(surfaces, offset, size, flag)",
        "Set the read permission for the indicated ATS pages");

//--------------------------------------------------------------------
//! \brief Implement Action.SetAtsWritePermission() token
//!
JSBool PmParser::Action_SetAtsWritePermission
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval  aPermission,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    bool permission;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aPermission, &permission, 4));
    RETURN_ACTION(new PmAction_SetAtsPermission(
        pSurfaceDesc,
        offset,
        size,
        "write",
        permission));
}

PM_BIND(Action, SetAtsWritePermission, 4,
        "Usage: Action.SetAtsWritePermission(surfaces, offset, size, flag)",
        "Set the write permission for the indicated ATS pages");

//--------------------------------------------------------------------
//! \brief Implement Action.RemapAtsPages() token
//!
JSBool PmParser::Action_RemapAtsPages
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_UpdateAtsPages(
        "Action.RemapAtsPages",
        pSurfaceDesc,
        offset,
        size,
        IommuDrv::RemapPage));
}

PM_BIND(Action, RemapAtsPages, 3,
        "Usage: Action.RemapAtsPages(surfaces, offset, size)",
        "Remap the indicated ATS pages");

//--------------------------------------------------------------------
//! \brief Implement Action.UnmapAtsPages() token
//!
JSBool PmParser::Action_UnmapAtsPages
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT32 offset;
    UINT32 size;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_UpdateAtsPages(
        "Action.UnmapAtsPages",
        pSurfaceDesc,
        offset,
        size,
        IommuDrv::UnmapPage));
}

PM_BIND(Action, UnmapAtsPages, 3,
        "Usage: Action.UnmapAtsPages(surfaces, offset, size)",
        "Unmap the indicated ATS pages");

//--------------------------------------------------------------------
//! \brief Implement Action.RemapFaultingAtsPage() token
//!
JSBool PmParser::Action_RemapFaultingAtsPage
(
    jsval *pReturlwalue
)
{
    RC rc;

    RETURN_ACTION(new PmAction_UpdateFaultingAtsPage(
        "Action.RemapFaultingAtsPage",
        IommuDrv::RemapPage));
}

PM_BIND(Action, RemapFaultingAtsPage, 0,
        "Usage: Action.RemapAtsFaultingPage()",
        "Remap the lwrrently faulting ATS page");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearEngineFaultedChannel() token
//!
JSBool PmParser::Action_RestartEngineFaultedChannel
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_ACTION(new PmAction_RestartEngineFaultedChannel(pChannelDesc,
                  m_Scopes.top().m_InbandMemOp, m_Scopes.top().m_pInbandChannel));
}

PM_BIND(Action, RestartEngineFaultedChannel, 1,
        "Usage: Action.RestartEngineFaultedChannel(channels)",
        "Clear LW_PCCSR_CHANNEL_ENG_FAULTED for given channels.");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearEngineFaultedChannel() token
//!
JSBool PmParser::Action_RestartPbdmaFaultedChannel
(
    jsval channelDesc,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_ACTION(new PmAction_RestartPbdmaFaultedChannel(pChannelDesc,
                  m_Scopes.top().m_InbandMemOp, m_Scopes.top().m_pInbandChannel));
}

PM_BIND(Action, RestartPbdmaFaultedChannel, 1,
        "Usage: Action.RestartPbdmaFaultedChannel(channels)",
        "Clear LW_PCCSR_CHANNEL_PBDMA_FAULTED for given channels.");

//--------------------------------------------------------------------
//! \brief Implement Action.ClearAccessCounter() token
//!
JSBool PmParser::Action_ClearAccessCounter(jsval gpuDesc,
        jsval counterType, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    string type;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(counterType, &type, 2));
    RETURN_ACTION(new PmAction_ClearAccessCounter(pGpuDesc,
        m_Scopes.top().m_InbandMemOp, type));
}

PM_BIND(Action, ClearAccessCounter, 2,
        "Usage: Action.ClearAccessCounter(gpu, counterType)",
        "Clear access counter of specified type; type could be "
        "'mimc', 'momc', 'all', 'targeted' for given GPU.");

//--------------------------------------------------------------------
//! \brief Implement Action.ResetAccessCounterCachedGetPointer() token
//!
JSBool PmParser::Action_ResetAccessCounterCachedGetPointer(jsval gpuDesc, jsval *pReturlwalue)
{
    RC rc;

    PmGpuDesc *pGpuDesc;
    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));

    RETURN_ACTION(new PmAction_ResetAccessCounterCachedGetPointer(pGpuDesc));
}

PM_BIND(Action, ResetAccessCounterCachedGetPointer, 1,
        "Usage: Action.ResetAccessCounterCachedGetPointer(gpu)",
        "Reset cached GET pointer for notification buffer reset.");

//--------------------------------------------------------------------
//! \brief Implement Action.SetNonReplayableFaultBufferFull() token
//!
JSBool PmParser::Action_ForceNonReplayableFaultBufferOverflow
(
    jsval *pReturlwalue
)
{
    RC rc;

    RETURN_ACTION(new PmAction_ForceNonReplayableFaultBufferOverflow());
}

PM_BIND(Action, ForceNonReplayableFaultBufferOverflow, 0,
        "Usage: Action.ForceNonReplayableFaultBufferOverflow()",
        "Set GET = PUT + 1 % SIZE for all the subdevice, "
        "then next fault will cause overflow. "
        "This is used for non-replayable fault buffer overflow testing.");

//! \brief Implement Action.SetPhysAddrBits() token
//!
JSBool PmParser::Action_SetPhysAddrBits
(
    jsval  surfaceDesc,
    jsval  aOffset,
    jsval  aSize,
    jsval  aAddrValue,
    jsval  aAddrMask,
    jsval *pReturlwalue
)
{
    PmSurfaceDesc *pSurfaceDesc;
    UINT64 offset;
    UINT64 size;
    UINT64 addrValue;
    UINT64 addrMask;
    RC rc;

    C_CHECK_RC(FromJsval(surfaceDesc, &pSurfaceDesc, 1));
    C_CHECK_RC(FromJsval(aOffset, &offset, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    C_CHECK_RC(FromJsval(aAddrValue, &addrValue, 4));
    C_CHECK_RC(FromJsval(aAddrMask, &addrMask, 5));
    RETURN_ACTION(new PmAction_SetPhysAddrBits(
        pSurfaceDesc,
        offset,
        size,
        addrValue,
        addrMask,
        m_Scopes.top().m_InbandPte,
        m_Scopes.top().m_pInbandChannel,
        m_Scopes.top().m_DeferTlbIlwalidate));
}

PM_BIND(Action, SetPhysAddrBits, 5,
        "Usage: Action.SetPhysAddrBits(surfaces, offset, size, value, mask)",
        "Set the specified physical address bits of the indicated pages");

//--------------------------------------------------------------------
//! \brief Implement Action.WaitErrorLoggerInterrupt() token
//!
JSBool PmParser::Action_WaitErrorLoggerInterrupt
(
    jsval gpuDesc,
    jsval name,
    jsval timeoutMs,
    jsval *pReturlwalue
)
{
    RC rc;
    PmGpuDesc *pGpuDesc;
    std::string Name;
    FLOAT64 TimeoutMs;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    C_CHECK_RC(FromJsval(name, &Name, 2));
    C_CHECK_RC(FromJsval(timeoutMs, &TimeoutMs, 3));

    m_pPolicyManager->AddErrorLoggerInterruptName(Name);

    RETURN_ACTION(new PmAction_WaitErrorLoggerInterrupt(pGpuDesc, Name, TimeoutMs));
}

PM_BIND(Action, WaitErrorLoggerInterrupt, 3,
        "Usage: Action.WaitErrorLoggerInterrupt(gpus,name,timeoutms)",
        "Wait for an interrupt with matching a name to be reported to the "
        "error logger within the timeout.");

//--------------------------------------------------------------------
//! \brief Implement Action.SaveCHRAM() token
//!
JSBool PmParser::Action_SaveCHRAM
(
    jsval channelDesc,
    jsval filename,
    jsval *pReturlwalue
)
{
    RC rc;
    PmChannelDesc *pChannelDesc;
    std::string name;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(filename, &name, 2));
    RETURN_ACTION(new PmAction_SaveCHRAM(pChannelDesc, name));
}

PM_BIND(Action, SaveCHRAM, 2,
        "Usage: Action.SaveCHRAM(channelDesc, filename)",
        "Source channel save channel RAM to a file specified by filename.");

//--------------------------------------------------------------------
//! \brief Implement Action.RestoreCHRAM() token
//!
JSBool PmParser::Action_RestoreCHRAM
(
    jsval channelDesc,
    jsval filename,
    jsval *pReturlwalue
)
{
    RC rc;
    PmChannelDesc *pChannelDesc;
    std::string name;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    C_CHECK_RC(FromJsval(filename, &name, 2));
    RETURN_ACTION(new PmAction_RestoreCHRAM(pChannelDesc, name));
}

PM_BIND(Action, RestoreCHRAM, 2,
        "Usage: Action.RestoreCHRAM(channelDesc, filename)",
        "Dst channel load channel RAM to a file specified by filename.");

//////////////////////////////////////////////////////////////////////
// Surface tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS_WITH_ALIAS(Surface, PolicyManager_Surface);

//--------------------------------------------------------------------
//! \brief Implement Surface.Define() token
//!
JSBool PmParser::Surface_Define(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(CheckForRootScope("Surface.Define"));
    C_CHECK_RC(FromJsval(aName, &name, 1));

    PushScope(Scope::SURFACE);
    m_Scopes.top().m_pSurfaceDesc = new PmSurfaceDesc(name);
    RETURN_RC(rc);
}

PM_BIND(Surface, Define, 1, "Usage: Surface.Define(surfaceId)",
    "Create a surface description.  Must have a corresponding Surface.End.");

//--------------------------------------------------------------------
//! \brief Implement Surface.End() token
//!
JSBool PmParser::Surface_End(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(CheckForSurfaceScope("Surface.End"));
    C_CHECK_RC(
        GetEventManager()->AddSurfaceDesc(m_Scopes.top().m_pSurfaceDesc));
    m_Scopes.pop();
    RETURN_RC(rc);
}

PM_BIND(Surface, End, 0, "Usage: Surface.End()",
        "End a surface description.  Must come after Surface.Define.");

//--------------------------------------------------------------------
//! \brief Implement Surface.Defined() token
//!
JSBool PmParser::Surface_Defined(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(FromJsval(aName, &name, 1));
    PmSurfaceDesc *pSurfaceDesc = GetEventManager()->GetSurfaceDesc(name);
    if (pSurfaceDesc == NULL)
    {
        ErrPrintf("%s: \"%s\" is not a valid surface ID\n",
                  GetErrInfo().c_str(), name.c_str());
        return RC::BAD_PARAMETER;
    }
    RETURN_OBJECT(pSurfaceDesc);
}

PM_BIND(Surface, Defined, 1, "Usage: Surface.Defined(surfaceId)",
    "A surface that was defined by a previous Surface.Define/Surface.End.");

//--------------------------------------------------------------------
//! \brief Implement Surface.Name() token
//!
JSBool PmParser::Surface_Name(jsval * arglist, uint32 numArgs, jsval *pReturlwalue)
{
    string regex;
    PmTestDesc * pTestDesc = nullptr;
    PmSurfaceDesc * pSurfaceDesc = nullptr;
    RC rc;

    switch(numArgs)
    {
        case 1:
            CREATE_SURFACE(pSurfaceDesc);
            C_CHECK_RC(FromJsval(arglist[0], &regex, 1));
            SET_SURFACE_ATTR(pSurfaceDesc, Name, "", regex);
            break;
        case 2:
            CREATE_SURFACE(pSurfaceDesc);
            C_CHECK_RC(FromJsval(arglist[0], &regex, 1));
            C_CHECK_RC(FromJsval(arglist[1], &pTestDesc, 2));
            SET_SURFACE_ATTR(pSurfaceDesc, Name, "", regex);
            SET_SURFACE_ATTR(pSurfaceDesc, TestDesc, nullptr, pTestDesc);
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: Surface.Name(regex, <TestDesc>). TestDesc is optional.\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }

    RETURN_OBJECT(pSurfaceDesc);
}

PM_BIND_VAR(Surface, Name, 2, "Usage: Surface.Name(regex, <TestDesc>)",
        "Specify that the surface name must match regex, TestDesc is optional");

//--------------------------------------------------------------------
//! \brief Implement Surface.NotName() token
//!
JSBool PmParser::Surface_NotName(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_SURFACE(NotName, "", regex);
}

PM_BIND(Surface, NotName, 1, "Usage: Surface.NotName(regex)",
        "Specify that the surface name must not match regex.");

//--------------------------------------------------------------------
//! \brief Implement Surface.Type() token
//!
JSBool PmParser::Surface_Type(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_SURFACE(Type, "", regex);
}

PM_BIND(Surface, Type, 1, "Usage: Surface.Type(regex)",
        "Specify that the surface type must match regex.");

//--------------------------------------------------------------------
//! \brief Implement Surface.NotType() token
//!
JSBool PmParser::Surface_NotType(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_SURFACE(NotType, "", regex);
}

PM_BIND(Surface, NotType, 1, "Usage: Surface.NotType(regex)",
        "Specify that the surface type must not match regex.");

//--------------------------------------------------------------------
//! \brief Implement Surface.Gpu() token
//!
JSBool PmParser::Surface_Gpu(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_SURFACE(GpuDesc, NULL, pGpuDesc);
}

PM_BIND_WITH_ALIAS(
    Surface, Gpu, PolicyManager_Gpu, 1, "Usage: Surface.Gpu(gpus)",
    "Specify that the surface must be on one of the indicated gpus.");

//--------------------------------------------------------------------
//! \brief Implement Surface.Faulting() token
//!
JSBool PmParser::Surface_Faulting(jsval *pReturlwalue)
{
    RC rc;
    RETURN_SURFACE(Faulting, false, true);
}

PM_BIND(Surface, Faulting, 0, "Usage: Surface.Faulting()",
        "Specify the faulting surface.");

//--------------------------------------------------------------------
//! \brief Implement Surface.All() token
//!
JSBool PmParser::Surface_All(jsval *pReturlwalue)
{
    RC rc;
    RETURN_SURFACE(Name, "", ".*");
}

PM_BIND(Surface, All, 0, "Usage: Surface.All()", "Matches all surfaces.");

//////////////////////////////////////////////////////////////////////
// VaSpace tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS_WITH_ALIAS(VaSpace, PolicyManager_VaSpace);

//--------------------------------------------------------------------
//! \brief Implement VaSpace.Name() token
//!
JSBool PmParser::VaSpace_Name(jsval * arglist, uint32 numArgs, jsval *pReturlwalue)
{
    string regex;
    RC rc;
    PmTestDesc * pTestDesc = nullptr;
    PmVaSpaceDesc *pVaSpaceDesc = nullptr;

    switch(numArgs)
    {
        case 1:
            CREATE_VASPACE(pVaSpaceDesc);
            C_CHECK_RC(FromJsval(arglist[0], &regex, 1));
            SET_VASPACE_ATTR(pVaSpaceDesc, Name, "", regex);
            break;
        case 2:
            CREATE_VASPACE(pVaSpaceDesc);
            C_CHECK_RC(FromJsval(arglist[0], &regex, 1));
            C_CHECK_RC(FromJsval(arglist[1], &pTestDesc, 2));
            SET_VASPACE_ATTR(pVaSpaceDesc, Name, "", regex);
            SET_VASPACE_ATTR(pVaSpaceDesc, TestDesc, nullptr, pTestDesc);
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: VaSpace.Name(regex, <TestDesc>). And TestDesc is alternative.\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }
    RETURN_OBJECT(pVaSpaceDesc);
}

PM_BIND_VAR(VaSpace, Name, 2, "Usage: VaSpace.Name(regex, <TestDesc>)",
        "There are two usages and TestDesc is alternative."
        "1. Specify that the vaspace name must match regex."
        "2. Specify that the vaspace name can match regex and TestDesc also.");

//////////////////////////////////////////////////////////////////////
// VF tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS_WITH_ALIAS(VF, PolicyManager_VF);

//--------------------------------------------------------------------
//! \brief Implement VF.SeqId() token
//!
JSBool PmParser::VF_SeqId(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_VF(SeqId, "", regex);
    RETURN_RC(rc);
}

PM_BIND(VF, SeqId, 1, "Usage: VF.SeqId(regex)",
        "Specify that the VF whose sequence Id at the vf_testlist.yml"
        " must match regex.");

//--------------------------------------------------------------------
//! \brief Implement VF.GFID() token
//!
JSBool PmParser::VF_GFID(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_VF(GFID, "", regex);
    RETURN_RC(rc);
}

PM_BIND(VF, GFID, 1, "Usage: VF.GFID(regex)",
        "Specify that the VF whose GFID must match regex.");

//--------------------------------------------------------------------
//! \brief Implement VF.All() token
//!
JSBool PmParser::VF_All(jsval *pReturlwalue)
{
    RC rc;

    RETURN_VF(GFID, "", ".*");
    RETURN_RC(rc);
}

PM_BIND(VF, All, 0, "Usage: VF.All()", "Matches all VFs.");

//////////////////////////////////////////////////////////////////////
// SmcEngine tokens
//////////////////////////////////////////////////////////////////////


PM_CLASS_WITH_ALIAS(SmcEngine, PolicyManager_SmcEngine);

//--------------------------------------------------------------------
//! \brief Implement SmcEngine.SysPipe() token
//!
JSBool PmParser::SmcEngine_SysPipe(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_SMCENGINE(SysPipe, "", regex);
    RETURN_RC(rc);
}

PM_BIND(SmcEngine, SysPipe, 1, "Usage: SmcEngine.SysPipe(regex)",
        "Specify that SmcEngine whose SysPipe must match regex.");

//--------------------------------------------------------------------
//! \brief Implement SmcEngine.Test() token
//!
JSBool PmParser::SmcEngine_Test(jsval aTestDesc, jsval *pReturlwalue)
{
    PmTestDesc* pTestDesc;
    RC rc;
    C_CHECK_RC(FromJsval(aTestDesc, &pTestDesc, 1));
    RETURN_SMCENGINE(TestDesc, nullptr, pTestDesc);
    RETURN_RC(rc);
}

PM_BIND_WITH_ALIAS(SmcEngine, Test, PolicyManager_Test, 1,
        "Usage: SmcEngine.Test(TestDesc)",
        "Specify that SmcEngine which is running the specified test.");

//--------------------------------------------------------------------
//! \brief Implement SmcEngine.Name() token
//!
JSBool PmParser::SmcEngine_Name(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_SMCENGINE(Name, "", regex);
    RETURN_RC(rc);
}

PM_BIND(SmcEngine, Name, 1, "Usage: SmcEngine.Name(regex)",
        "Specify that SmcEngine whose Name must match regex.");

//////////////////////////////////////////////////////////////////////
// Channel tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS_WITH_ALIAS(Channel, PolicyManager_Channel);

//--------------------------------------------------------------------
//! \brief Implement Channel.Define() token
//!
JSBool PmParser::Channel_Define(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(CheckForRootScope("Channel.Define"));
    C_CHECK_RC(FromJsval(aName, &name, 1));

    PushScope(Scope::CHANNEL);
    m_Scopes.top().m_pChannelDesc = new PmChannelDesc(name);
    RETURN_RC(rc);
}

PM_BIND(Channel, Define, 1, "Usage: Channel.Define(channelId)",
    "Create a channel description.  Must have a corresponding Channel.End.");

//--------------------------------------------------------------------
//! \brief Implement Channel.End() token
//!
JSBool PmParser::Channel_End(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(CheckForChannelScope("Channel.End"));
    C_CHECK_RC(
        GetEventManager()->AddChannelDesc(m_Scopes.top().m_pChannelDesc));
    m_Scopes.pop();
    RETURN_RC(rc);
}

PM_BIND(Channel, End, 0, "Usage: Channel.End()",
        "End a channel description.  Must come after Channel.Define.");

//--------------------------------------------------------------------
//! \brief Implement Channel.Defined() token
//!
JSBool PmParser::Channel_Defined(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(FromJsval(aName, &name, 1));
    PmChannelDesc *pChannelDesc = GetEventManager()->GetChannelDesc(name);
    if (pChannelDesc == NULL)
    {
        ErrPrintf("%s: \"%s\" is not a valid channel ID\n",
                  GetErrInfo().c_str(), name.c_str());
        return RC::BAD_PARAMETER;
    }
    RETURN_OBJECT(pChannelDesc);
}

PM_BIND(Channel, Defined, 1, "Usage: Channel.Defined(channelId)",
    "A channel that was defined by a previous Channel.Define/Channel.End.");

//--------------------------------------------------------------------
//! \brief Implement Channel.Name() token
//!
JSBool PmParser::Channel_Name(jsval * arglist, uint32 numArgs, jsval *pReturlwalue)
{
    string regex;
    PmTestDesc * pTestDesc = nullptr;
    PmChannelDesc * pChannelDesc = nullptr;
    RC rc;

    switch(numArgs)
    {
        case 1:
            CREATE_CHANNEL(pChannelDesc);
            C_CHECK_RC(FromJsval(arglist[0], &regex, 1));
            SET_CHANNEL_ATTR(pChannelDesc, Name, "", regex);
            break;
        case 2:
            CREATE_CHANNEL(pChannelDesc);
            C_CHECK_RC(FromJsval(arglist[0], &regex, 1));
            C_CHECK_RC(FromJsval(arglist[1], &pTestDesc, 2));
            SET_CHANNEL_ATTR(pChannelDesc, Name, "", regex);
            SET_CHANNEL_ATTR(pChannelDesc, TestDesc, nullptr, pTestDesc);
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: Channel.Name(regex, <TestDesc>). And TestDesc is alternative.\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }

    RETURN_OBJECT(pChannelDesc);
}

PM_BIND_VAR(Channel, Name, 2, "Usage: Channel.Name(regex, <TestDesc>)",
        "There are two usages and TestDesc is alternative."
        "1. Specify that the channel name must match regex."
        "2. Specify that the channel name can match regex and TestDesc also.");

//--------------------------------------------------------------------
//! \brief Implement Channel.Number() token
//!
JSBool PmParser::Channel_Number(jsval aChannelNumbers, jsval *pReturlwalue)
{
    vector<UINT32> channelNumbers;
    vector<UINT32> emptyArray;  // Used by RETURN_CHANNEL
    RC rc;

    C_CHECK_RC(FromJsval(aChannelNumbers, &channelNumbers, 1));
    RETURN_CHANNEL(ChannelNumbers, emptyArray, channelNumbers);
}

PM_BIND(Channel, Number, 1, "Usage: Channel.Number([num1, num2, ...]",
        "Specify that the channel number must be one of the listed numbers.");

//--------------------------------------------------------------------
//! \brief Implement Channel.Gpu() token
//!
JSBool PmParser::Channel_Gpu(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_CHANNEL(GpuDesc, NULL, pGpuDesc);
}

PM_BIND_WITH_ALIAS(
    Channel, Gpu, PolicyManager_Gpu, 1, "Usage: Channel.Gpu(gpus)",
    "Specify that the channel must be on one of the indicated gpus.");

//--------------------------------------------------------------------
//! \brief Implement Channel.Faulting() token
//!
JSBool PmParser::Channel_Faulting(jsval *pReturlwalue)
{
    RC rc;
    RETURN_CHANNEL(Faulting, false, true);
}

PM_BIND(Channel, Faulting, 0, "Usage: Channel.Faulting()",
        "Specify the faulting channel.");

//--------------------------------------------------------------------
//! \brief Implement Channel.All() token
//!
JSBool PmParser::Channel_All(jsval *pReturlwalue)
{
    RC rc;
    RETURN_CHANNEL(Name, "", ".*");
}

PM_BIND(Channel, All, 0, "Usage: Channel.All()", "Matches all channels.");

//////////////////////////////////////////////////////////////////////
// Gpu tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS_WITH_ALIAS(Gpu, PolicyManager_Gpu);

//--------------------------------------------------------------------
//! \brief Implement Gpu.Define() token
//!
JSBool PmParser::Gpu_Define(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(CheckForRootScope("Gpu.Define"));
    C_CHECK_RC(FromJsval(aName, &name, 1));

    PushScope(Scope::GPU);
    m_Scopes.top().m_pGpuDesc = new PmGpuDesc(name);
    RETURN_RC(rc);
}

PM_BIND(Gpu, Define, 1, "Usage: Gpu.Define(gpuId)",
    "Create a GPU description.  Must have a corresponding Gpu.End.");

//--------------------------------------------------------------------
//! \brief Implement Gpu.End() token
//!
JSBool PmParser::Gpu_End(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(CheckForGpuScope("Gpu.End"));
    C_CHECK_RC(
        GetEventManager()->AddGpuDesc(m_Scopes.top().m_pGpuDesc));
    m_Scopes.pop();
    RETURN_RC(rc);
}

PM_BIND(Gpu, End, 0, "Usage: Gpu.End()",
        "End a GPU description.  Must come after Gpu.Define.");

//--------------------------------------------------------------------
//! \brief Implement Gpu.Defined() token
//!
JSBool PmParser::Gpu_Defined(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(FromJsval(aName, &name, 1));
    PmGpuDesc *pGpuDesc = GetEventManager()->GetGpuDesc(name);
    if (pGpuDesc == NULL)
    {
        ErrPrintf("%s: \"%s\" is not a valid Gpu ID\n",
                  GetErrInfo().c_str(), name.c_str());
        return RC::BAD_PARAMETER;
    }
    RETURN_OBJECT(pGpuDesc);
}

PM_BIND(Gpu, Defined, 1, "Usage: Gpu.Defined(gpuId)",
    "A GPU that was defined by a previous Gpu.Define/Gpu.End.");

//--------------------------------------------------------------------
//! \brief Implement Gpu.Inst() token
//!
JSBool PmParser::Gpu_Inst
(
    jsval aDevRegex,
    jsval aSubdevRegex,
    jsval *pReturlwalue
)
{
    string devRegex;
    string subdevRegex;
    RC rc;

    C_CHECK_RC(FromJsval(aDevRegex,    &devRegex,    1));
    C_CHECK_RC(FromJsval(aSubdevRegex, &subdevRegex, 2));

    if (CheckForGpuScope("") == OK)
    {
        PmGpuDesc *pGpuDesc = m_Scopes.top().m_pGpuDesc;
        if (pGpuDesc->GetDevInst() != "")
        {
            InfoPrintf("%s: Gpu.DevInst already set for \"%s\"\n",
                       GetErrInfo().c_str(),
                       pGpuDesc->GetId().c_str());
        }
        if (pGpuDesc->GetSubdevInst() != "")
        {
            InfoPrintf("%s: Gpu.SubdevInst already set for \"%s\"\n",
                       GetErrInfo().c_str(),
                       pGpuDesc->GetId().c_str());
        }
        C_CHECK_RC(pGpuDesc->SetDevInst(devRegex));
        RETURN_RC(pGpuDesc->SetSubdevInst(subdevRegex));
    }
    else
    {
        PmGpuDesc *pGpuDesc = new PmGpuDesc("");
        C_CHECK_RC(GetEventManager()->AddGpuDesc(pGpuDesc));
        C_CHECK_RC(pGpuDesc->SetDevInst(devRegex));
        C_CHECK_RC(pGpuDesc->SetSubdevInst(subdevRegex));
        RETURN_OBJECT(pGpuDesc);
    }
}

PM_BIND(Gpu, Inst, 2, "Usage: Gpu.Inst(devRegex, subdevRegex)",
        "Same as specifying both Gpu.DevInst() and Gpu.SubdevInst().");

//--------------------------------------------------------------------
//! \brief Implement Gpu.DevInst() token
//!
JSBool PmParser::Gpu_DevInst(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_GPU(DevInst, "", regex);
}

PM_BIND(Gpu, DevInst, 1, "Usage: Gpu.DevInst(regex)",
        "Specify that the GPU's device-instance number must match regex.");

//--------------------------------------------------------------------
//! \brief Implement Gpu.SubdevInst() token
//!
JSBool PmParser::Gpu_SubdevInst(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_GPU(SubdevInst, "", regex);
}

PM_BIND(Gpu, SubdevInst, 1, "Usage: Gpu.SubdevInst(regex)",
        "Specify that the GPU's subdevice-instance number must match regex.");

//--------------------------------------------------------------------
//! \brief Implement Gpu.Faulting() token
//!
JSBool PmParser::Gpu_Faulting(jsval *pReturlwalue)
{
    RC rc;
    RETURN_GPU(Faulting, false, true);
}

PM_BIND(Gpu, Faulting, 0, "Usage: Gpu.Faulting()",
        "Specify the faulting gpu.");

//--------------------------------------------------------------------
//! \brief Implement Gpu.All() token
//!
JSBool PmParser::Gpu_All(jsval *pReturlwalue)
{
    RC rc;
    RETURN_GPU(DevInst, "", ".*");
}

PM_BIND(Gpu, All, 0, "Usage: Gpu.All()", "Matches all Gpus.");

//////////////////////////////////////////////////////////////////////
// Runlist tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS_WITH_ALIAS(Runlist, PolicyManager_Runlist);

//--------------------------------------------------------------------
//! \brief Implement Runlist.Define() token
//!
JSBool PmParser::Runlist_Define(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(CheckForRootScope("Runlist.Define"));
    C_CHECK_RC(FromJsval(aName, &name, 1));

    PushScope(Scope::RUNLIST);
    m_Scopes.top().m_pRunlistDesc = new PmRunlistDesc(name);
    RETURN_RC(rc);
}

PM_BIND(Runlist, Define, 1, "Usage: Runlist.Define(runlistId)",
    "Create a runlist description.  Must have a corresponding Runlist.End.");

//--------------------------------------------------------------------
//! \brief Implement Runlist.End() token
//!
JSBool PmParser::Runlist_End(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(CheckForRunlistScope("Runlist.End"));
    C_CHECK_RC(
        GetEventManager()->AddRunlistDesc(m_Scopes.top().m_pRunlistDesc));
    m_Scopes.pop();
    RETURN_RC(rc);
}

PM_BIND(Runlist, End, 0, "Usage: Runlist.End()",
        "End a runlist description.  Must come after Runlist.Define.");

//--------------------------------------------------------------------
//! \brief Implement Runlist.Defined() token
//!
JSBool PmParser::Runlist_Defined(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(FromJsval(aName, &name, 1));
    PmRunlistDesc *pRunlistDesc = GetEventManager()->GetRunlistDesc(name);
    if (pRunlistDesc == NULL)
    {
        ErrPrintf("%s: \"%s\" is not a valid runlist ID\n",
                  GetErrInfo().c_str(), name.c_str());
        return RC::BAD_PARAMETER;
    }
    RETURN_OBJECT(pRunlistDesc);
}

PM_BIND(Runlist, Defined, 1, "Usage: Runlist.Defined(runlistId)",
    "A runlist that was defined by a previous Runlist.Define/Runlist.End.");

//--------------------------------------------------------------------
//! \brief Implement Runlist.Name() token
//!
JSBool PmParser::Runlist_Name(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_RUNLIST(Name, "", regex);
}

PM_BIND(Runlist, Name, 1, "Usage: Runlist.Name(regex)",
        "Specify that the runlist name must match regex.");

//--------------------------------------------------------------------
//! \brief Implement Runlist.Channel() token
//!
JSBool PmParser::Runlist_Channel(jsval channelDesc, jsval *pReturlwalue)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_RUNLIST(ChannelDesc, NULL, pChannelDesc);
}

PM_BIND_WITH_ALIAS(
    Runlist, Channel, PolicyManager_Channel, 1,
    "Usage: Runlist.Channel(channels)",
    "Specify that the runlist must be used for one of the indicated channels.");

//--------------------------------------------------------------------
//! \brief Implement Runlist.Gpu() token
//!
JSBool PmParser::Runlist_Gpu(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_RUNLIST(GpuDesc, NULL, pGpuDesc);
}

PM_BIND_WITH_ALIAS(
    Runlist, Gpu, PolicyManager_Gpu, 1, "Usage: Runlist.Gpu(gpus)",
    "Specify that the runlist must be on one of the indicated gpus.");

//--------------------------------------------------------------------
//! \brief Implement Runlist.All() token
//!
JSBool PmParser::Runlist_All(jsval *pReturlwalue)
{
    RC rc;
    RETURN_RUNLIST(Name, "", ".*");
}

PM_BIND(Runlist, All, 0, "Usage: Runlist.All()", "Matches all runlists.");

//////////////////////////////////////////////////////////////////////
// Test tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS_WITH_ALIAS(Test, PolicyManager_Test);

//--------------------------------------------------------------------
//! \brief Implement Test.Define() token
//!
JSBool PmParser::Test_Define(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(CheckForRootScope("Test.Define"));
    C_CHECK_RC(FromJsval(aName, &name, 1));

    PushScope(Scope::TEST);
    m_Scopes.top().m_pTestDesc = new PmTestDesc(name);
    RETURN_RC(rc);
}

PM_BIND(Test, Define, 1, "Usage: Test.Define(testId)",
    "Create a test description.  Must have a corresponding Test.End.");

//--------------------------------------------------------------------
//! \brief Implement Test.End() token
//!
JSBool PmParser::Test_End(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(CheckForTestScope("Test.End"));
    C_CHECK_RC(
        GetEventManager()->AddTestDesc(m_Scopes.top().m_pTestDesc));
    m_Scopes.pop();
    RETURN_RC(rc);
}

PM_BIND(Test, End, 0, "Usage: Test.End()",
        "End a test description.  Must come after Test.Define.");

//--------------------------------------------------------------------
//! \brief Implement Test.Defined() token
//!
JSBool PmParser::Test_Defined(jsval aName, jsval *pReturlwalue)
{
    string name;
    RC rc;

    C_CHECK_RC(FromJsval(aName, &name, 1));
    PmTestDesc *pTestDesc = GetEventManager()->GetTestDesc(name);
    if (pTestDesc == NULL)
    {
        ErrPrintf("%s: \"%s\" is not a valid test ID\n",
                  GetErrInfo().c_str(), name.c_str());
        return RC::BAD_PARAMETER;
    }
    RETURN_OBJECT(pTestDesc);
}

PM_BIND(Test, Defined, 1, "Usage: Test.Defined(testId)",
    "A test that was defined by a previous Test.Define/Test.End.");

//--------------------------------------------------------------------
//! \brief Implement Test.Name() token
//!
JSBool PmParser::Test_Name(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_TEST(Name, "", regex);
}

PM_BIND(Test, Name, 1, "Usage: Test.Name(regex)",
        "Specify that the test name must match regex.");

//--------------------------------------------------------------------
//! \brief Implement Test.Id() token
//!
JSBool PmParser::Test_Id(jsval * arglist, uint32 numArgs, jsval *pReturlwalue)
{
    UINT32 id;
    RC rc;

    C_CHECK_RC(FromJsval(arglist[0], &id, 1));
    RETURN_TEST(TestId, static_cast<UINT32>(~0x0), id);
}

PM_BIND_VAR(Test, Id, 1, "Usage: Test.Id(testid)",
        "Specify that the test Id.");

//--------------------------------------------------------------------
//! \brief Implement Test.Type() token
//!
JSBool PmParser::Test_Type(jsval aRegex, jsval *pReturlwalue)
{
    string regex;
    RC rc;

    C_CHECK_RC(FromJsval(aRegex, &regex, 1));
    RETURN_TEST(Type, "", regex);
}

PM_BIND(Test, Type, 1, "Usage: Test.Type(regex)",
        "Specify that the test type must match regex.");

//--------------------------------------------------------------------
//! \brief Implement Test.Channel() token
//!
JSBool PmParser::Test_Channel(jsval channelDesc, jsval *pReturlwalue)
{
    PmChannelDesc *pChannelDesc;
    RC rc;

    C_CHECK_RC(FromJsval(channelDesc, &pChannelDesc, 1));
    RETURN_TEST(ChannelDesc, NULL, pChannelDesc);
}

PM_BIND_WITH_ALIAS(
    Test, Channel, PolicyManager_Channel, 1,
    "Usage: Test.Channel(channels)",
    "Specify that the test must be used for one of the indicated channels.");

//--------------------------------------------------------------------
//! \brief Implement Test.Gpu() token
//!
JSBool PmParser::Test_Gpu(jsval gpuDesc, jsval *pReturlwalue)
{
    PmGpuDesc *pGpuDesc;
    RC rc;

    C_CHECK_RC(FromJsval(gpuDesc, &pGpuDesc, 1));
    RETURN_TEST(GpuDesc, NULL, pGpuDesc);
}

PM_BIND_WITH_ALIAS(
    Test, Gpu, PolicyManager_Gpu, 1, "Usage: Test.Gpu(gpus)",
    "Specify that the test must be on one of the indicated gpus.");

//--------------------------------------------------------------------
//! \brief Implement Test.Faulting() token
//!
JSBool PmParser::Test_Faulting(jsval *pReturlwalue)
{
    RC rc;
    RETURN_TEST(Faulting, false, true);
}

PM_BIND(Test, Faulting, 0, "Usage: Test.Faulting()",
        "Specify the faulting test.");

//--------------------------------------------------------------------
//! \brief Implement Test.All() token
//!
JSBool PmParser::Test_All(jsval *pReturlwalue)
{
    RC rc;
    RETURN_TEST(Name, "", ".*");
}

PM_BIND(Test, All, 0, "Usage: Test.All()", "Matches all tests.");

//////////////////////////////////////////////////////////////////////
// Aperture tokens
//////////////////////////////////////////////////////////////////////

PM_CLASS(Aperture);

//--------------------------------------------------------------------
//! \brief Implement Aperture.Framebuffer() token
//!
JSBool PmParser::Aperture_Framebuffer(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(ToJsval(PolicyManager::APERTURE_FB, pReturlwalue));
    return JS_TRUE;
}

PM_BIND(Aperture, Framebuffer, 0, "Usage: Aperture.Framebuffer()",
        "Framebuffer (video) memory.");

//--------------------------------------------------------------------
//! \brief Implement Aperture.Coherent() token
//!
JSBool PmParser::Aperture_Coherent(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(ToJsval(PolicyManager::APERTURE_COHERENT, pReturlwalue));
    return JS_TRUE;
}

PM_BIND(Aperture, Coherent, 0, "Usage: Aperture.Coherent()",
        "System coherent memory.");

//--------------------------------------------------------------------
//! \brief Implement Aperture.NonCoherent() token
//!
JSBool PmParser::Aperture_NonCoherent(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(ToJsval(PolicyManager::APERTURE_NONCOHERENT, pReturlwalue));
    return JS_TRUE;
}

PM_BIND(Aperture, NonCoherent, 0, "Usage: Aperture.NonCoherent()",
        "System noncoherent memory.");

//--------------------------------------------------------------------
//! \brief Implement Aperture.Peer() token
//!
JSBool PmParser::Aperture_Peer(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(ToJsval(PolicyManager::APERTURE_PEER, pReturlwalue));
    return JS_TRUE;
}

PM_BIND(Aperture, Peer, 0, "Usage: Aperture.Peer()", "Peer memory.");

//--------------------------------------------------------------------
//! \brief Implement Aperture.Invalid() token
//!
JSBool PmParser::Aperture_Ilwalid(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(ToJsval(PolicyManager::APERTURE_ILWALID, pReturlwalue));
    return JS_TRUE;
}

PM_BIND(Aperture, Invalid, 0, "Usage: Aperture.Invalid()",
        "Used to set aperture fields to an invalid value.");

//--------------------------------------------------------------------
//! \brief Implement Aperture.All() token
//!
JSBool PmParser::Aperture_All(jsval *pReturlwalue)
{
    RC rc;
    C_CHECK_RC(ToJsval(PolicyManager::APERTURE_ALL, pReturlwalue));
    return JS_TRUE;
}

PM_BIND(Aperture, All, 0, "Usage: Aperture.All()", "Matches all apertures.");

//////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief Push a new scope of the indicated type onto m_Scopes.
//!
//! Except for the type, the new scope is a copy of the previous
//! scope.
//!
void PmParser::PushScope(Scope::Type scopeType)
{
    Scope newScope = {};
    if (m_Scopes.size() > 0)
    {
        // Copy previous scope
        newScope = m_Scopes.top();
    }
    else
    {
        // If no previous scope, then use default values for new scope
        newScope.m_AllocLocation = Memory::Optimal;
        newScope.m_PhysContig = false;
        newScope.m_Alignment = 0;
        newScope.m_DualPageSize = false;
        newScope.m_LoopBackPolicy = PolicyManager::NO_LOOPBACK;
        newScope.m_MovePolicy = PolicyManager::DELETE_MEM;
        newScope.m_PageSize = PolicyManager::LWRRENT_PAGE_SIZE;
        newScope.m_PowerWaitModelUs =
            PmAction_PowerWait::DEFAULT_MODEL_WAIT_US;
        newScope.m_PowerWaitRTLUs =
            PmAction_PowerWait::DEFAULT_RTL_WAIT_US;
        newScope.m_PowerWaitHWUs =
            PmAction_PowerWait::DEFAULT_HW_WAIT_US;
        newScope.m_PowerWaitBusy = false;
        newScope.m_PStateFallback =
                LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;
        newScope.m_bUseSeedString = !m_pPolicyManager->IsRandomSeedSet();
        newScope.m_RandomSeed = m_pPolicyManager->IsRandomSeedSet() ?
                                    m_pPolicyManager->GetRandomSeed() : 0;
        newScope.m_ClearNewSurfaces = true;
        newScope.m_TlbIlwalidateGpc = true;
        newScope.m_TlbIlwalidateReplay =
            PolicyManager::TLB_ILWALIDATE_REPLAY_NONE;
        newScope.m_TlbIlwalidateSysmembar = false;;
        newScope.m_TlbIlwalidateAckType =
            PolicyManager::TLB_ILWALIDATE_ACK_TYPE_NONE;
        newScope.m_SurfaceGpuCacheMode = Surface2D::GpuCacheDefault;
        newScope.m_ChannelSubdevMask = Channel::AllSubdevicesMask;
        newScope.m_pRemoteGpuDesc = NULL;
        newScope.m_SemRelFlags = Channel::FlagSemRelDefault;
        newScope.m_DeferTlbIlwalidate = false;
        newScope.m_TlbIlwalidatePdbAll = true;
        newScope.m_DisablePlcOnSurfaceMove = false;
        newScope.m_EnableNsiSemaphore = true;
        newScope.m_EnableNsiInt = true;
        newScope.m_AddrSpaceType = PolicyManager::DefaultVASpace;
        newScope.m_SurfaceAllocType = Surface2D::DefaultVASpace;
        newScope.m_GpuSmmuMode = Surface2D::GpuSmmuOff;
        newScope.m_MoveSurfInDefaultMMU = true;
        newScope.m_pGPCID = new FancyPicker();
        newScope.m_pGPCID->ConfigRandom();
        newScope.m_pClientUnitID = new FancyPicker();
        newScope.m_pClientUnitID->ConfigRandom();
        newScope.m_pCEAllocInst = NULL;
        newScope.m_FpContext.LoopNum = 0;
        newScope.m_FpContext.RestartNum = 0;
        newScope.m_FpContext.pJSObj = 0;
        newScope.m_CEAllocMode = PolicyManager::CEAllocDefault;
        newScope.m_pTlbIlwalidateChannel = NULL;
        newScope.m_TlbIlwalidateBar1 = false;
        newScope.m_TlbIlwalidateBar2 = false;
        newScope.m_TlbIlwalidateLevelNum = PolicyManager::LEVEL_MAX;
        newScope.m_TlbIlwalidateIlwalScope = PolicyManager::TlbIlwalidateIlwalScope::ALL_TLBS;
        newScope.m_PhysPageSize = PolicyManager::ALL_PAGE_SIZES;
        newScope.m_pVaSpaceDesc = NULL;
        newScope.m_pVfDesc = NULL;
        newScope.m_pSmcEngineDesc = NULL;
        newScope.m_AccessType = PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_NONE;
        newScope.m_VEID = SubCtx::VEID_END;
        newScope.m_DefEngineName = string();
        newScope.m_RegSpace = string();
    }
    newScope.m_Type = scopeType;
    newScope.m_MutexName = RegExp();
    m_Scopes.push(newScope);
}

//--------------------------------------------------------------------
//! \brief Report error if string is not a C identifier
//!
RC PmParser::CheckForIdentifier(const string &value, UINT32 argNum)
{
    static const char validChars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
    static const char ilwalidFirstChars[] = "0123456789";

    if (value.empty() || value.find_first_not_of(validChars) != string::npos ||
        value.find_first_of(ilwalidFirstChars) == 0)
    {
        if (argNum > 0)
        {
            ErrPrintf("%s: arg %d must be an alphanumeric string starting with a letter or _",
                      GetErrInfo().c_str(), argNum);
        }
        return RC::CANNOT_PARSE_FILE;
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not at the root scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//!
RC PmParser::CheckForRootScope(const char *tokenName)
{
    MASSERT(!m_Scopes.empty());
    switch (m_Scopes.top().m_Type)
    {
        case Scope::ROOT:
            break;
        case Scope::ACTION_BLOCK:
        case Scope::CONDITION:
        case Scope::ELSE_BLOCK:
        case Scope::ALLOW_OVERLAPPING_TRIGGERS:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside an ActionBlock\n",
                          GetErrInfo().c_str(), tokenName);
            }
            return RC::CANNOT_PARSE_FILE;
        case Scope::SURFACE:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside a Surface\n",
                          GetErrInfo().c_str(), tokenName);
            }
        case Scope::VF:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside a Vf\n",
                          GetErrInfo().c_str(), tokenName);
            }
            return RC::CANNOT_PARSE_FILE;
        case Scope::SMCENGINE:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside a SmcEngine\n",
                          GetErrInfo().c_str(), tokenName);
            }
            return RC::CANNOT_PARSE_FILE;
        case Scope::VASPACE:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside a VaSpace\n",
                          GetErrInfo().c_str(), tokenName);
            }
            return RC::CANNOT_PARSE_FILE;
        case Scope::CHANNEL:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside a Channel.Name\n",
                          GetErrInfo().c_str(), tokenName);
            }
            return RC::CANNOT_PARSE_FILE;
        case Scope::GPU:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside a Gpu\n",
                          GetErrInfo().c_str(), tokenName);
            }
            return RC::CANNOT_PARSE_FILE;
        case Scope::RUNLIST:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside a Runlist\n",
                          GetErrInfo().c_str(), tokenName);
            }
            return RC::CANNOT_PARSE_FILE;
        case Scope::TEST:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside a Test\n", GetErrInfo().c_str(), tokenName);
            }
            return RC::CANNOT_PARSE_FILE;
        case Scope::MUTEX:
        case Scope::MUTEX_CONDITION:
            if (strcmp(tokenName, "") != 0)
            {
                ErrPrintf("%s: %s must not be inside a Mutex\n", GetErrInfo().c_str(), tokenName);
            }
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not in an ActionBlock scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//! \param strict If true, must not be inside any sub-scopes (e.g. CONDITION)
//!
RC PmParser::CheckForActionBlockScope
(
    const char *tokenName
)
{
    RC rc;
    MASSERT(!m_Scopes.empty());
    switch (m_Scopes.top().m_Type)
    {
        case Scope::ACTION_BLOCK:
        case Scope::CONDITION:
        case Scope::ELSE_BLOCK:
        case Scope::ALLOW_OVERLAPPING_TRIGGERS:
        case Scope::MUTEX:
        case Scope::MUTEX_CONDITION:
            break;
        default:
            rc = RC::CANNOT_PARSE_FILE;
            break;
    }

    if (rc != OK && strcmp(tokenName, "") != 0)
    {
        ErrPrintf("%s: %s must come after ActionBlock.Define, OnTriggerCount, OnTestNum, or AllowOverlappingTriggers\n",
                  GetErrInfo().c_str(), tokenName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not in a Surface scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//!
RC PmParser::CheckForSurfaceScope(const char *tokenName)
{
    RC rc;
    MASSERT(!m_Scopes.empty());
    if (m_Scopes.top().m_Type != Scope::SURFACE)
    {
        rc = RC::CANNOT_PARSE_FILE;
    }
    if (rc != OK && strcmp(tokenName, "") != 0)
    {
        ErrPrintf("%s: %s must be inside a Surface\n",
                  GetErrInfo().c_str(), tokenName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not in a Channel scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//!
RC PmParser::CheckForChannelScope(const char *tokenName)
{
    RC rc;
    MASSERT(!m_Scopes.empty());
    if (m_Scopes.top().m_Type != Scope::CHANNEL)
    {
        rc = RC::CANNOT_PARSE_FILE;
    }
    if (rc != OK && strcmp(tokenName, "") != 0)
    {
        ErrPrintf("%s: %s must be inside a Channel\n",
                  GetErrInfo().c_str(), tokenName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not in a Vf scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//!
RC PmParser::CheckForVfScope(const char *tokenName)
{
    RC rc;
    MASSERT(!m_Scopes.empty());
    if (m_Scopes.top().m_Type != Scope::VF)
    {
        rc = RC::CANNOT_PARSE_FILE;
    }
    if (rc != OK && strcmp(tokenName, "") != 0)
    {
        ErrPrintf("%s: %s must be inside a Vf\n",
                  GetErrInfo().c_str(), tokenName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not in a SmcEngine scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//!
RC PmParser::CheckForSmcEngineScope(const char *tokenName)
{
    RC rc;
    MASSERT(!m_Scopes.empty());
    if (m_Scopes.top().m_Type != Scope::SMCENGINE)
    {
        rc = RC::CANNOT_PARSE_FILE;
    }
    if (rc != OK && strcmp(tokenName, "") != 0)
    {
        ErrPrintf("%s: %s must be inside a SmcEngine\n",
                  GetErrInfo().c_str(), tokenName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not in a VaSpace scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//!
RC PmParser::CheckForVaSpaceScope(const char *tokenName)
{
    RC rc;
    MASSERT(!m_Scopes.empty());
    if (m_Scopes.top().m_Type != Scope::VASPACE)
    {
        rc = RC::CANNOT_PARSE_FILE;
    }
    if (rc != OK && strcmp(tokenName, "") != 0)
    {
        ErrPrintf("%s: %s must be inside a VaSpace\n",
                  GetErrInfo().c_str(), tokenName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not in a Gpu scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//!
RC PmParser::CheckForGpuScope(const char *tokenName)
{
    RC rc;
    MASSERT(!m_Scopes.empty());
    if (m_Scopes.top().m_Type != Scope::GPU)
    {
        rc = RC::CANNOT_PARSE_FILE;
    }
    if (rc != OK && strcmp(tokenName, "") != 0)
    {
        ErrPrintf("%s: %s must be inside a Gpu\n",
                  GetErrInfo().c_str(), tokenName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not in a Runlist scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//!
RC PmParser::CheckForRunlistScope(const char *tokenName)
{
    RC rc;
    MASSERT(!m_Scopes.empty());
    if (m_Scopes.top().m_Type != Scope::RUNLIST)
    {
        rc = RC::CANNOT_PARSE_FILE;
    }
    if (rc != OK && strcmp(tokenName, "") != 0)
    {
        ErrPrintf("%s: %s must be inside a Runlist\n",
                  GetErrInfo().c_str(), tokenName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Report error if policyfile is not in a Test scope
//!
//! \param tokenName Name of the token being parsed.  Used for error
//!     message.  Set to "" to suppress the error message.
//!
RC PmParser::CheckForTestScope(const char *tokenName)
{
    RC rc;
    MASSERT(!m_Scopes.empty());
    if (m_Scopes.top().m_Type != Scope::TEST)
    {
        rc = RC::CANNOT_PARSE_FILE;
    }
    if (rc != OK && strcmp(tokenName, "") != 0)
    {
        ErrPrintf("%s: %s must be inside a Test\n",
                  GetErrInfo().c_str(), tokenName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a string, with error-reporting.
//!
//! This method assumes that an empty string is an error.
//!
RC PmParser::FromJsval(jsval value, string *pString, UINT32 argNum)
{
    RC rc = JavaScriptPtr()->FromJsval(value, pString);
    if (rc != OK)
    {
        if (argNum > 0)
        {
            ErrPrintf("%s: expected a string in arg %d\n",
                      GetErrInfo().c_str(), argNum);
        }
        return rc;
    }

    if (pString->empty())
    {
        if (argNum > 0)
        {
            ErrPrintf("%s: arg %d must not be an empty string\n",
                      GetErrInfo().c_str(), argNum);
        }
        return RC::CANNOT_PARSE_FILE;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a bool, with error-reporting.
//!
RC PmParser::FromJsval(jsval value, bool *pBool, UINT32 argNum)
{
    RC rc = JavaScriptPtr()->FromJsval(value, pBool);
    if (rc != OK && argNum > 0)
    {
        ErrPrintf("%s: expected a boolean in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a UINT32, with error-reporting.
//!
RC PmParser::FromJsval(jsval value, UINT32 *pUint32, UINT32 argNum)
{
    RC rc = JavaScriptPtr()->FromJsval(value, pUint32);
    if (rc != OK && argNum > 0)
    {
        ErrPrintf("%s: expected an integer in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a UINT64, with error-reporting.
//!
RC PmParser::FromJsval(jsval value, UINT64 *pUint64, UINT32 argNum)
{
    RC rc = JavaScriptPtr()->FromJsval(value, pUint64);
    if (rc != OK && argNum > 0)
    {
        ErrPrintf("%s: expected an integer in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a FLOAT64, with error-reporting.
//!
RC PmParser::FromJsval(jsval value, FLOAT64 *pFloat64, UINT32 argNum)
{
    RC rc = JavaScriptPtr()->FromJsval(value, pFloat64);
    if (rc != OK && argNum > 0)
    {
        ErrPrintf("%s: expected a floating-point number in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmTrigger, with error-reporting.
//!
RC PmParser::FromJsval(jsval value, PmTrigger **ppTrigger, UINT32 argNum)
{
    RC rc = FromJsvalWrapper(value, (void**)ppTrigger, "PmTrigger");
    if (rc != OK && argNum > 0)
    {
        ErrPrintf("%s: expected a trigger in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmAction, with error-reporting.
//!
RC PmParser::FromJsval(jsval value, PmAction **ppAction, UINT32 argNum)
{
    RC rc = FromJsvalWrapper(value, (void**)ppAction, "PmAction");
    if (rc != OK && argNum > 0)
    {
        ErrPrintf("%s: expected an action in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmActionBlock, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PmActionBlock **ppActionBlock,
    UINT32 argNum
)
{
    RC rc = FromJsvalWrapper(value, (void**)ppActionBlock, "PmActionBlock");
    if (rc != OK && argNum > 0)
    {
        ErrPrintf("%s: expected an ActionBlock in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmVaSpaceDesc, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PmVaSpaceDesc **ppVaSpaceDesc,
    UINT32 argNum
)
{
    if (FromJsvalWrapper(value, (void**)ppVaSpaceDesc, "PmVaSpaceDesc") == OK)
    {
        return OK;
    }

    string vaspaceId;
    if (JavaScriptPtr()->FromJsval(value, &vaspaceId) == OK)
    {
        *ppVaSpaceDesc =
            m_pPolicyManager->GetEventManager()->GetVaSpaceDesc(vaspaceId);
        if (*ppVaSpaceDesc != NULL)
            return OK;
    }

    if (argNum > 0)
    {
        ErrPrintf("%s: expected a vaspace or vaspace ID in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return RC::BAD_PARAMETER;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmVfTestDesc, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PmVfTestDesc **ppVfDesc,
    UINT32 argNum
)
{
    if (FromJsvalWrapper(value, (void**)ppVfDesc, "PmVfTestDesc") == OK)
    {
        return OK;
    }

    string VfId;
    if (JavaScriptPtr()->FromJsval(value, &VfId) == OK)
    {
        *ppVfDesc =
            m_pPolicyManager->GetEventManager()->GetVfDesc(VfId);
        if (*ppVfDesc != NULL)
            return OK;
    }

    if (argNum > 0)
    {
        ErrPrintf("%s: expected a Vf or Vf ID in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return RC::BAD_PARAMETER;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmSmcEngineDesc, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PmSmcEngineDesc **ppSmcEngineDesc,
    UINT32 argNum
)
{
    if (FromJsvalWrapper(value, (void**)ppSmcEngineDesc, "PmSmcEngineDesc") == OK)
    {
        return OK;
    }

    string SmcEngineId;
    if (JavaScriptPtr()->FromJsval(value, &SmcEngineId) == OK)
    {
        *ppSmcEngineDesc =
            m_pPolicyManager->GetEventManager()->GetSmcEngineDesc(SmcEngineId);
        if (*ppSmcEngineDesc != NULL)
            return OK;
    }

    if (argNum > 0)
    {
        ErrPrintf("%s: expected a SmcEngine or SmcEngine ID in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return RC::BAD_PARAMETER;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmSurfaceDesc, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PmSurfaceDesc **ppSurfaceDesc,
    UINT32 argNum
)
{
    if (FromJsvalWrapper(value, (void**)ppSurfaceDesc, "PmSurfaceDesc") == OK)
    {
        return OK;
    }

    string surfaceId;
    if (JavaScriptPtr()->FromJsval(value, &surfaceId) == OK)
    {
        *ppSurfaceDesc =
            m_pPolicyManager->GetEventManager()->GetSurfaceDesc(surfaceId);
        if (*ppSurfaceDesc != NULL)
            return OK;
    }

    if (argNum > 0)
    {
        ErrPrintf("%s: expected a surface or surface ID in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return RC::BAD_PARAMETER;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmChannelDesc, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PmChannelDesc **ppChannelDesc,
    UINT32 argNum
)
{
    if (FromJsvalWrapper(value, (void**)ppChannelDesc, "PmChannelDesc") == OK)
    {
        return OK;
    }

    string channelId;
    if (JavaScriptPtr()->FromJsval(value, &channelId) == OK)
    {
        *ppChannelDesc =
            m_pPolicyManager->GetEventManager()->GetChannelDesc(channelId);
        if (*ppChannelDesc != NULL)
            return OK;
    }

    if (argNum > 0)
    {
        ErrPrintf("%s: expected a channel or channel ID in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return RC::BAD_PARAMETER;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmGpuDesc, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PmGpuDesc **ppGpuDesc,
    UINT32 argNum
)
{
    if (FromJsvalWrapper(value, (void**)ppGpuDesc, "PmGpuDesc") == OK)
    {
        return OK;
    }

    string gpuId;
    if (JavaScriptPtr()->FromJsval(value, &gpuId) == OK)
    {
        *ppGpuDesc =
            m_pPolicyManager->GetEventManager()->GetGpuDesc(gpuId);
        if (*ppGpuDesc != NULL)
            return OK;
    }

    if (argNum > 0)
    {
        ErrPrintf("%s: expected a gpu or gpu ID in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return RC::BAD_PARAMETER;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmRunlistDesc, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PmRunlistDesc **ppRunlistDesc,
    UINT32 argNum
)
{
    if (FromJsvalWrapper(value, (void**)ppRunlistDesc, "PmRunlistDesc") == OK)
    {
        return OK;
    }

    string runlistId;
    if (JavaScriptPtr()->FromJsval(value, &runlistId) == OK)
    {
        *ppRunlistDesc =
            m_pPolicyManager->GetEventManager()->GetRunlistDesc(runlistId);
        if (*ppRunlistDesc != NULL)
            return OK;
    }

    if (argNum > 0)
    {
        ErrPrintf("%s: expected a runlist or runlist ID in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return RC::BAD_PARAMETER;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a PmTestDesc, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PmTestDesc **ppTestDesc,
    UINT32 argNum
)
{
    if (FromJsvalWrapper(value, (void**)ppTestDesc, "PmTestDesc") == OK)
    {
        return OK;
    }

    string testId;
    if (JavaScriptPtr()->FromJsval(value, &testId) == OK)
    {
        *ppTestDesc = m_pPolicyManager->GetEventManager()->GetTestDesc(testId);
        if (*ppTestDesc != NULL)
            return OK;
    }

    if (argNum > 0)
    {
        ErrPrintf("%s: expected a test or test ID in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    return RC::BAD_PARAMETER;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to a RegExp, with error-reporting.
//!
//! \param value The jsval to be colwerted to a RegExp
//! \param[out] pRegExp Points to a RegExp object that will be on success.
//! \param argNum The argument number.  Used in an error message if
//!     value cannot be colwerted to a RegExp.
//! \param flags RegExp::Flags values used to determine how the
//!     regular expression will be matched.  (RegExp::FULL) is the
//!     usual setting, or (RegExp::FULL | RegExp::ICASE) to do
//!     case-insensitive matching.
//!
RC PmParser::FromJsval
(
    jsval value,
    RegExp *pRegExp,
    UINT32 argNum,
    UINT32 flags
)
{
    MASSERT(pRegExp != NULL);
    string regexString;
    RC rc;

    rc = JavaScriptPtr()->FromJsval(value, &regexString);
    if (rc != OK)
    {
        if (argNum > 0)
        {
            ErrPrintf("%s: expected a regex string in arg %d\n",
                      GetErrInfo().c_str(), argNum);
        }
        return rc;
    }

    rc = pRegExp->Set(regexString, flags);
    if (rc != OK)
    {
        if (argNum > 0)
        {
            ErrPrintf("%s: bad regular expression in arg %d: \"%s\"\n",
                      GetErrInfo().c_str(), argNum, regexString.c_str());
        }
        return rc;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to an array of UINT32, with error-reporting.
//!
RC PmParser::FromJsval(jsval value, vector<UINT32> *pArray, UINT32 argNum)
{
    JavaScriptPtr pJs;
    JsArray arr;
    RC rc;

    rc = pJs->FromJsval(value, &arr);
    if (rc != OK)
    {
        if (argNum > 0)
        {
            ErrPrintf("%s: expected a list of integers in arg %d, of the form [num1, num2, ...]\n",
                      GetErrInfo().c_str(), argNum);
        }
        return rc;
    }

    pArray->resize(arr.size());
    for (UINT32 ii = 0; ii < arr.size(); ++ii)
    {
        rc = pJs->FromJsval(arr[ii], &((*pArray)[ii]));
        if (rc != OK)
        {
            if (argNum > 0)
            {
                ErrPrintf("%s: expected a list of integers in arg %d, but element #%d is not an integer\n",
                          GetErrInfo().c_str(), argNum, ii + 1);
            }
            return rc;
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to an array of string, with error-reporting.
//!
RC PmParser::FromJsval(jsval value, vector<string> *pArray, UINT32 argNum)
{
    JavaScriptPtr pJs;
    JsArray arr;
    RC rc;

    rc = pJs->FromJsval(value, &arr);
    if (rc != OK)
    {
        if (argNum > 0)
        {
            ErrPrintf("%s: expected a list of strings in arg %d, of the form [string, string ...]\n",
                      GetErrInfo().c_str(), argNum);
        }
        return rc;
    }

    pArray->resize(arr.size());
    for (UINT32 ii = 0; ii < arr.size(); ++ii)
    {
        rc = pJs->FromJsval(arr[ii], &((*pArray)[ii]));
        if (rc != OK)
        {
            if (argNum > 0)
            {
                ErrPrintf("%s: expected a list of strings in arg %d, but element #%d is not an string\n",
                          GetErrInfo().c_str(), argNum, ii + 1);
            }
            return rc;
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a jsval to an aperture, with error-reporting.
//!
RC PmParser::FromJsval
(
    jsval value,
    PolicyManager::Aperture *pAperture,
    UINT32 argNum
)
{
    UINT32 IntValue;
    RC rc = FromJsvalEnum(value, &IntValue, "Aperture");
    if (rc != OK && argNum > 0)
    {
        ErrPrintf("%s: expected an Aperture in arg %d\n",
                  GetErrInfo().c_str(), argNum);
    }
    *pAperture = (PolicyManager::Aperture)IntValue;
    return rc;
}

//--------------------------------------------------------------------
//! Colwert a PmTrigger to a jsval.
//!
RC PmParser::ToJsval(PmTrigger *pTrigger, jsval *pValue)
{
    return ToJsvalWrapper(pTrigger, pValue, "PmTrigger");
}

//--------------------------------------------------------------------
//! Colwert a PmAction to a jsval.
//!
RC PmParser::ToJsval(PmAction *pAction, jsval *pValue)
{
    return ToJsvalWrapper(pAction, pValue, "PmAction");
}

//--------------------------------------------------------------------
//! Colwert a PmActionBlock to a jsval.
//!
RC PmParser::ToJsval(PmActionBlock *pActionBlock, jsval *pValue)
{
    return ToJsvalWrapper(pActionBlock, pValue, "PmActionBlock");
}

//--------------------------------------------------------------------
//! Colwert a PmSurfaceDesc to a jsval.
//!
RC PmParser::ToJsval(PmSurfaceDesc *pSurfaceDesc, jsval *pValue)
{
    return ToJsvalWrapper(pSurfaceDesc, pValue, "PmSurfaceDesc");
}

//--------------------------------------------------------------------
//! Colwert a PmChannelDesc to a jsval.
//!
RC PmParser::ToJsval(PmChannelDesc *pChannelDesc, jsval *pValue)
{
    return ToJsvalWrapper(pChannelDesc, pValue, "PmChannelDesc");
}

//--------------------------------------------------------------------
//! Colwert a PmGpuDesc to a jsval.
//!
RC PmParser::ToJsval(PmGpuDesc *pGpuDesc, jsval *pValue)
{
    return ToJsvalWrapper(pGpuDesc, pValue, "PmGpuDesc");
}

//--------------------------------------------------------------------
//! Colwert a PmRunlistDesc to a jsval.
//!
RC PmParser::ToJsval(PmRunlistDesc *pRunlistDesc, jsval *pValue)
{
    return ToJsvalWrapper(pRunlistDesc, pValue, "PmRunlistDesc");
}

//--------------------------------------------------------------------
//! Colwert a PmVaSpaceDesc to a jsval.
//!
RC PmParser::ToJsval(PmVaSpaceDesc *pVaSpaceDesc, jsval *pValue)
{
    return ToJsvalWrapper(pVaSpaceDesc, pValue, "PmVaSpaceDesc");
}

//--------------------------------------------------------------------
//! Colwert a PmVfTestDesc to a jsval.
//!
RC PmParser::ToJsval(PmVfTestDesc *pVfDesc, jsval *pValue)
{
    return ToJsvalWrapper(pVfDesc, pValue, "PmVfTestDesc");
}

//--------------------------------------------------------------------
//! Colwert a PmSmcEngineDesc to a jsval.
//!
RC PmParser::ToJsval(PmSmcEngineDesc *pSmcEngineDesc, jsval *pValue)
{
    return ToJsvalWrapper(pSmcEngineDesc, pValue, "PmSmcEngineDesc");
}

//--------------------------------------------------------------------
//! Colwert a PmTestDesc to a jsval.
//!
RC PmParser::ToJsval(PmTestDesc *pTestDesc, jsval *pValue)
{
    return ToJsvalWrapper(pTestDesc, pValue, "PmTestDesc");
}

//--------------------------------------------------------------------
//! Colwert an Aperture to a jsval.
//!
//! Note: unlike most of the other ToJsval methods, the Aperture is
//! stored by value instead of by reference.
//!
RC PmParser::ToJsval(PolicyManager::Aperture aperture, jsval *pValue)
{
    return ToJsvalEnum((UINT32)aperture, pValue, "Aperture");
}

//--------------------------------------------------------------------
//! \brief Wrap a void* into a jsval
//!
//! This method is used to implement several ToJsval() methods.  It's
//! mostly used in cases like "Policy.XXX(Trigger.YYY(), ...);", in
//! which one Trigger.YYY allocates a PmTrigger, which it wants to
//! pass to Policy.XXX.
//!
//! \sa FromJsvalWrapper()
//!
//! \param pObject The void* that is being colwerted
//! \param pValue The jsval output
//! \param className Type of object being wrapped.  Used for error-
//!     checking when we unwrap the jsval later.
//!
RC PmParser::ToJsvalWrapper
(
    void *pObject,
    jsval *pValue,
    const char *className
)
{
    // Colwert the pointer+className to a javascript string.  (I
    // suppose an object with private data would be more elegant, but
    // this works well enough.)
    //
    MASSERT(pObject != NULL);
    string objDesc = Utility::StrPrintf("__%p__%s", pObject, className);
    return JavaScriptPtr()->ToJsval(objDesc, pValue);
}

//--------------------------------------------------------------------
//! \brief Extract a void* from a jsval wrapper
//!
//! This method is used to implement several FromJsval() methods.
//! \sa ToJsvalWrapper()
//!
//! \param value The jsval input
//! \param ppObject On exit, *ppObject holds the wrapped void*
//! \param className Type of object that we expect to be wrapped.
//!     Must match the className passed to ToJsvalWrapper().
//!
RC PmParser::FromJsvalWrapper
(
    jsval value,
    void **ppObject,
    const char *className
)
{
    MASSERT(ppObject != NULL);
    string str;
    char buff[100];
    RC rc;

    *ppObject = NULL;
    CHECK_RC(JavaScriptPtr()->FromJsval(value, &str));
    if (sscanf(str.c_str(), "__%p__%s", ppObject, buff) != 2 ||
        *ppObject == NULL ||
        strcmp(buff, className) != 0)
    {
        CHECK_RC(RC::BAD_PARAMETER);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Wrap an enum into a jsval
//!
//! This method is used to implement several ToJsval() methods.
//! It's mostly used for enumerated types.
//!
//! \sa FromJsvalEnum()
//!
//! \param integer The enumerated integer that is being colwerted
//! \param pValue The jsval output
//! \param className Type of enum being wrapped.  Used for error-
//!     checking when we unwrap the jsval later.
//!
RC PmParser::ToJsvalEnum
(
    UINT32 integer,
    jsval *pValue,
    const char *enumName
)
{
    // Colwert the integer+enumName to a javascript string.
    //
    string enumDesc = Utility::StrPrintf("__%u__%s", integer, enumName);
    return JavaScriptPtr()->ToJsval(enumDesc, pValue);
}

//--------------------------------------------------------------------
//! \brief Extract an enum from a jsval wrapper
//!
//! This method is used to implement several FromJsval() methods.
//! \sa ToJsvalEnum()
//!
//! \param value The jsval input
//! \param pInteger On exit, *pInteger holds the wrapped enum value
//! \param className Type of enum that we expect to be wrapped.
//!     Must match the enumName passed to ToJsvalEnum().
//!
RC PmParser::FromJsvalEnum
(
    jsval value,
    UINT32 *pInteger,
    const char *enumName
)
{
    MASSERT(pInteger != NULL);
    string str;
    char buff[100];
    RC rc;

    CHECK_RC(JavaScriptPtr()->FromJsval(value, &str));
    if (sscanf(str.c_str(), "__%u__%s", pInteger, buff) != 2 ||
        strcmp(buff, enumName) != 0)
    {
        CHECK_RC(RC::BAD_PARAMETER);
    }
    return rc;
}

namespace
{
    // Trim whitespace from the start and end of *pString.
    // Used by ParsePriReg.
    void TrimWhitespace(string *pString)
    {
        auto firstNonspace = find_if(pString->begin(), pString->end(),
                                     [](char ch) { return !isspace(ch); });
        pString->erase(pString->begin(), firstNonspace);
        auto lastNonspace = find_if(pString->rbegin(), pString->rend(),
                                    [](char ch) { return !isspace(ch); });
        pString->erase(lastNonspace.base(), pString->end());
    }
}

//--------------------------------------------------------------------
//! \brief Parse a register into name and indexes
//!
//! On entry, pName points at a register name, which can be one of the
//! following:
//! - No index, such as "LW_PTOP_SCAL_NUM_GPCS"
//! - Comma-separated indexes in parentheses, such as
//!   "LW_PTOP_FS_STATUS_GPC_ZLWLL_IDX(1, 2)"
//!
//! On exit, *pName contains the register name with the indexes
//! stripped off, and *pIndexes contains the index values.
//!
RC PmParser::ParsePriReg(string *pName, vector<UINT32> *pIndexes)
{
    RC rc;

    pIndexes->clear();
    TrimWhitespace(pName);
    auto leftParen = pName->find('(');
    auto rightParen = pName->find(')');
    if (leftParen != string::npos && rightParen == pName->size() - 1)
    {
        vector<string> tokens;
        CHECK_RC(Utility::Tokenizer(pName->substr(leftParen + 1,
                                                  rightParen - leftParen - 1),
                                    ",", &tokens));
        for (string &token: tokens)
        {
            TrimWhitespace(&token);
            char *pEnd;
            unsigned long index = strtoul(token.c_str(), &pEnd, 0);
            if (token.empty() || *pEnd != '\0')
            {
                Printf(Tee::PriError, "Bad index in register \"%s\"\n",
                       pName->c_str());
                return RC::BAD_PARAMETER;
            }
            pIndexes->push_back(static_cast<UINT32>(index));
        }
        pName->erase(leftParen);
        TrimWhitespace(pName);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Print the random seed used with the specified token so
//!        the test may be recreated
//!
//! \param tokenName The token name using the random seed
//!
void PmParser::PrintRandomSeed(const char *tokenName)
{
    MASSERT(m_FileName != "");
    string seedMsg;
    if (m_LineNum == 0)
    {
        seedMsg = Utility::StrPrintf("%s : %s using",
                                     m_FileName.c_str(), tokenName);
    }
    else
    {
        seedMsg = Utility::StrPrintf("%s(%d) : %s using",
                                     m_FileName.c_str(), m_LineNum, tokenName);
    }

    if (m_Scopes.top().m_bUseSeedString)
    {
        seedMsg += " default random seed";
    }
    else
    {
        seedMsg += Utility::StrPrintf(" random seed 0x%08x",
                                      m_Scopes.top().m_RandomSeed);
    }
    InfoPrintf("%s\n", seedMsg.c_str());
}

//--------------------------------------------------------------------
//! \brief Private function to create PmTrigger_OnMethodExelwte object
//!        This function is called by Trigger_OnMethodExelwte() and
//!        Trigger_OnMethodExelwteEx()
PmTrigger_OnMethodExelwte *PmParser::CreateOnMethodExelwteObj
(
    PmChannelDesc *pChannelDesc,
    FancyPicker   *pFancyPicker,
    bool           bUseSeedString,
    UINT32         randomSeed,
    bool           bWfiOnRelease,
    bool           bWaitEventHandled
)
{
    PmEventManager *pEventMgr = GetEventManager();
    PmTrigger      *pNewTrigger;
    PmAction       *pNewAction;

    // Create an OnMethodWrite trigger with the action to insert a PmMethodInt
    // at that point in the method stream so that when the method is exelwted
    // a method interrupt will occur
    pNewTrigger = new PmTrigger_OnMethodWrite(pChannelDesc,
                                              *pFancyPicker,
                                              bUseSeedString,
                                              randomSeed);
    pNewAction =  new PmAction_MethodInt(bWfiOnRelease, bWaitEventHandled);
    if (OK != pEventMgr->AddTrigger(pNewTrigger, pNewAction))
    {
        ErrPrintf("%s: Add trigger to event manager failed! \n", __FUNCTION__);
        return NULL;
    }

    return (new PmTrigger_OnMethodExelwte(pChannelDesc,
                                          *pFancyPicker,
                                          bUseSeedString,
                                          randomSeed));
}

//--------------------------------------------------------------------
//! \brief Private function to create PmTrigger_OnPercentMethodsExelwted object
//!        This function is called by Trigger_OnPercentMethodsExelwted() and
//!        Trigger_OnPercentMethodsExelwtedEx()
PmTrigger_OnPercentMethodsExelwted *PmParser::CreateOnPercentMethodsExelwtedObj
(
        PmChannelDesc *pChannelDesc,
        FancyPicker   *pFancyPicker,
        bool           bUseSeedString,
        UINT32         randomSeed,
        bool           bWfiOnRelease,
        bool           bWaitEventHandled
)
{
    PmEventManager *pEventMgr = GetEventManager();
    PmTrigger      *pNewTrigger;
    PmAction       *pNewAction;

    // Create an OnPercentMethodsWritten trigger with the action to insert a
    // PmMethodInt at that point in the method stream so that when the method
    // is exelwted a method interrupt will occur
    pNewTrigger = new PmTrigger_OnPercentMethodsWritten(
                                               pChannelDesc,
                                               *pFancyPicker,
                                               bUseSeedString,
                                               randomSeed);
    pNewAction =  new PmAction_MethodInt(bWfiOnRelease, bWaitEventHandled);
    if (OK != pEventMgr->AddTrigger(pNewTrigger, pNewAction))
    {
        ErrPrintf("%s: Add trigger to event manager failed! \n", __FUNCTION__);
        return NULL;
    }

    return (new PmTrigger_OnPercentMethodsExelwted(
                                               pChannelDesc,
                                               *pFancyPicker,
                                               bUseSeedString,
                                               randomSeed));
}

//-----------------------------------------------------------------------------
//! \brief Private functions to colwert a string input by user into mmu level
/*static*/ PolicyManager::Level PmParser::ColwertStrToMmuLevelType
(
    const string& levelName
)
{
    PolicyManager::Level levelType = PolicyManager::LEVEL_MAX;

    if     (levelName == "ALL")   levelType = PolicyManager::LEVEL_ALL;
    else if(levelName == "PTE")   levelType = PolicyManager::LEVEL_PTE;
    else if(levelName == "PDE0")  levelType = PolicyManager::LEVEL_PDE0;
    else if(levelName == "PDE1")  levelType = PolicyManager::LEVEL_PDE1;
    else if(levelName == "PDE2")  levelType = PolicyManager::LEVEL_PDE2;
    else if(levelName == "PDE3")  levelType = PolicyManager::LEVEL_PDE3;
    else
    {
        ErrPrintf("%s: Invalid level string %s\n",
                  __FUNCTION__, levelName.c_str());
        MASSERT(!"Invalid level Name\n");
        levelType = PolicyManager::LEVEL_MAX;
    }

    return levelType;
}

//-----------------------------------------------------------------------------
//! \brief Private function to colwert a string input into ilwal scope
/*static*/ PolicyManager::TlbIlwalidateIlwalScope PmParser::ColwertStrToMmuIlwalScope
(
    const string& ilwalScopeName
)
{
    PolicyManager::TlbIlwalidateIlwalScope ilwalScope;

    if (ilwalScopeName == "ALL_TLBS")
    {
        ilwalScope = PolicyManager::TlbIlwalidateIlwalScope::ALL_TLBS;
    }
    else if (ilwalScopeName == "LINK_TLBS")
    {
        ilwalScope = PolicyManager::TlbIlwalidateIlwalScope::LINK_TLBS;
    }
    else if (ilwalScopeName == "NON_LINK_TLBS")
    {
        ilwalScope = PolicyManager::TlbIlwalidateIlwalScope::NON_LINK_TLBS;
    }
    else
    {
        ErrPrintf("%s: Invalid ilwalScope string %s\n",
                  __FUNCTION__, ilwalScopeName.c_str());
        MASSERT(!"Invalid ilwalScopeName\n");
        ilwalScope = PolicyManager::TlbIlwalidateIlwalScope::ALL_TLBS;
    }

    return ilwalScope;
}

//----------------------------------------------------------------------------
//! Global Policy setting
//----------------------------------------------------------------------------
JSBool PmParser::Policy_SetInbandCELaunchFlushEnable
(
    jsval aFlag,
    jsval *pReturlwalue
)
{
    bool flag;
    RC rc;

    if(m_Scopes.size() > 1)
    {
        ErrPrintf("Policy.SetInbandCELaunchFlushEnable just support as a global configuration. "
                  "It should be have the same scope as the Policy.Define.\n");
        MASSERT(0);
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    C_CHECK_RC(FromJsval(aFlag, &flag, 1));

    m_pPolicyManager->SetInbandCELaunchFlushEnable(flag);

    RETURN_RC(OK);
}

PM_BIND(Policy, SetInbandCELaunchFlushEnable, 1,
        "Usage: Policy.SetInbandCELaunchFlushEnable(flag)",
        "Enable or Disable CE flush");

//--------------------------------------------------------------------
//! \brief Implement Action.StartVFTest() token
//!
JSBool PmParser::Action_StartVFTest
(
    jsval aVf,
    jsval *pReturlwalue
)
{
    RC rc;
    PmVfTestDesc * pVfDesc;

    C_CHECK_RC(FromJsval(aVf, &pVfDesc, 1));
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetStartVfTestInPolicyManger(true);
    RETURN_ACTION(new PmAction_StartVFTest(pVfDesc));
}

PM_BIND(Action, StartVFTest, 1, "Usage: Action.StartVFTest(VF)",
        "Start the indicated VF test.");

//--------------------------------------------------------------------
//! \brief Implement Action.WaitProcEvent() token
//!
JSBool PmParser::Action_WaitProcEvent
(
    jsval aMessage,
    jsval aVf,
    jsval *pReturlwalue
)
{
    RC rc;
    PmVfTestDesc * pVfDesc;
    string message;

    C_CHECK_RC(FromJsval(aMessage, &message, 1));
    C_CHECK_RC(FromJsval(aVf, &pVfDesc, 2));
    RETURN_ACTION(new PmAction_WaitProcEvent(pVfDesc, message));
}

PM_BIND(Action, WaitProcEvent, 2, "Usage: Action.WaitProcEvent(message, VF)",
        "Wait for the message from the indicated VF test.");

//--------------------------------------------------------------------
//! \brief Implement Action.SendProcEvent() token
//!
JSBool PmParser::Action_SendProcEvent
(
    jsval aMessage,
    jsval aVf,
    jsval *pReturlwalue
)
{
    RC rc;
    PmVfTestDesc * pVfDesc;
    string message;

    C_CHECK_RC(FromJsval(aMessage, &message, 1));
    C_CHECK_RC(FromJsval(aVf, &pVfDesc, 2));
    RETURN_ACTION(new PmAction_SendProcEvent(pVfDesc, message));
}

PM_BIND(Action, SendProcEvent, 2, "Usage: Action.SendProcEvent(message, VF)",
        "Send the message to the indicated VF test.");

//! \brief Implement Trigger.OnNonfatalPoisonError() token
//!
JSBool PmParser::Trigger_OnNonfatalPoisonError
(
    jsval *arglist,
    uint32 numArgs,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    string type = "";
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    m_pPolicyManager->SetPoisonErrorNeeded();
    // SetNonReplayableFaultBufferNeeded wil be ilwoked if necessary
    // in Trigger.StartTest
    switch (numArgs)
    {
        case 1:
            C_CHECK_RC(FromJsval(arglist[0], &pChannelDesc, 1));
            break;
        case 2:
            C_CHECK_RC(FromJsval(arglist[0], &pChannelDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &type, 2));
            if ((type != "E10") &&
                (type != "E12") &&
                (type != "E13") &&
                (type != "E16") &&
                (type != "E17"))
            {
                ErrPrintf("%s: Invalid poison type string\n", __FUNCTION__);
                return RC::BAD_PARAMETER;
            }
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: Trigger.OnNonfatalPoisonError(channels, <poisonType>)\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }

    RETURN_OBJECT(new PmTrigger_NonfatalPoisonError(pChannelDesc, type));
}

PM_BIND_VAR(Trigger, OnNonfatalPoisonError, 2,
        "Usage: Trigger.OnNonfatalPoisonError(channels, <poisonType>)",
        "Trigger that fires when a none fatal poison error oclwrs "
        "and matches the expected poison type.");

//! \brief Implement Trigger.OnFatalPoisonError() token
//!
JSBool PmParser::Trigger_OnFatalPoisonError
(
    jsval *arglist,
    uint32 numArgs,
    jsval *pReturlwalue
)
{
    PmChannelDesc *pChannelDesc;
    string type = "";
    RC rc;

    C_CHECK_RC(m_pPolicyManager->SetEnableRobustChannel(true));
    m_pPolicyManager->SetPoisonErrorNeeded();
    // SetNonReplayableFaultBufferNeeded wil be ilwoked if necessary
    // in Trigger.StartTest
    switch (numArgs)
    {
        case 1:
            C_CHECK_RC(FromJsval(arglist[0], &pChannelDesc, 1));
            break;
        case 2:
            C_CHECK_RC(FromJsval(arglist[0], &pChannelDesc, 1));
            C_CHECK_RC(FromJsval(arglist[1], &type, 2));
            if ((type != "E06") &&
                (type != "E09"))
            {
                ErrPrintf("%s: Invalid poison type string\n", __FUNCTION__);
                return RC::BAD_PARAMETER;
            }
            break;
        default:
            ErrPrintf("%s: Arguments number error. Please check the policy file. "
                      "Usage: Trigger.OnFatalPoisonError(channels, <poisonType>)\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
    }

    RETURN_OBJECT(new PmTrigger_FatalPoisonError(pChannelDesc, type));
}

PM_BIND_VAR(Trigger, OnFatalPoisonError, 2,
        "Usage: Trigger.OnFatalPoisonError(channels, <poisonType>)",
        "Trigger that fires when a fatal poison error oclwrs "
        "and matches the expected poison type.");

//-----------------------------------------------------------------------------
//! \brief Constants that are usable in Policy files
PM_CLASS(Constants);
// For Trigger.OnRmEvent
PROP_CONST(PolicyManager_Constants, ON_POWER_DOWN_GRAPHICS_ENTER, ON_POWER_DOWN_GRAPHICS_ENTER);
PROP_CONST(PolicyManager_Constants, ON_POWER_DOWN_GRAPHICS_COMPLETE, ON_POWER_DOWN_GRAPHICS_COMPLETE);
PROP_CONST(PolicyManager_Constants, ON_POWER_UP_GRAPHICS_ENTER, ON_POWER_UP_GRAPHICS_ENTER);
PROP_CONST(PolicyManager_Constants, ON_POWER_UP_GRAPHICS_COMPLETE, ON_POWER_UP_GRAPHICS_COMPLETE);
PROP_CONST(PolicyManager_Constants, ON_POWER_DOWN_VIDEO_ENTER, ON_POWER_DOWN_VIDEO_ENTER);
PROP_CONST(PolicyManager_Constants, ON_POWER_DOWN_VIDEO_COMPLETE, ON_POWER_DOWN_VIDEO_COMPLETE);
PROP_CONST(PolicyManager_Constants, ON_POWER_UP_VIDEO_ENTER, ON_POWER_UP_VIDEO_ENTER);
PROP_CONST(PolicyManager_Constants, ON_POWER_UP_VIDEO_COMPLETE, ON_POWER_UP_VIDEO_COMPLETE);
PROP_CONST(PolicyManager_Constants, ON_POWER_DOWN_VIC_ENTER, ON_POWER_DOWN_VIC_ENTER);
PROP_CONST(PolicyManager_Constants, ON_POWER_DOWN_VIC_COMPLETE, ON_POWER_DOWN_VIC_COMPLETE);
PROP_CONST(PolicyManager_Constants, ON_POWER_UP_VIC_ENTER, ON_POWER_UP_VIC_ENTER);
PROP_CONST(PolicyManager_Constants, ON_POWER_UP_VIC_COMPLETE, ON_POWER_UP_VIC_COMPLETE);
PROP_CONST(PolicyManager_Constants, ON_POWER_DOWN_MSPG_ENTER, ON_POWER_DOWN_MSPG_ENTER);
PROP_CONST(PolicyManager_Constants, ON_POWER_DOWN_MSPG_COMPLETE, ON_POWER_DOWN_MSPG_COMPLETE);
PROP_CONST(PolicyManager_Constants, ON_POWER_UP_MSPG_ENTER, ON_POWER_UP_MSPG_ENTER);
PROP_CONST(PolicyManager_Constants, ON_POWER_UP_MSPG_COMPLETE, ON_POWER_UP_MSPG_COMPLETE);

// For Action.HostSemaphoreReduction
//
PROP_CONST(PolicyManager_Constants, REDUCTION_MIN, Channel::REDUCTION_MIN);
PROP_CONST(PolicyManager_Constants, REDUCTION_MAX, Channel::REDUCTION_MAX);
PROP_CONST(PolicyManager_Constants, REDUCTION_XOR, Channel::REDUCTION_XOR);
PROP_CONST(PolicyManager_Constants, REDUCTION_AND, Channel::REDUCTION_AND);
PROP_CONST(PolicyManager_Constants, REDUCTION_OR,  Channel::REDUCTION_OR);
PROP_CONST(PolicyManager_Constants, REDUCTION_ADD, Channel::REDUCTION_ADD);
PROP_CONST(PolicyManager_Constants, REDUCTION_INC, Channel::REDUCTION_INC);
PROP_CONST(PolicyManager_Constants, REDUCTION_DEC, Channel::REDUCTION_DEC);
PROP_CONST(PolicyManager_Constants, REDUCTION_UNSIGNED, Channel::REDUCTION_UNSIGNED);
PROP_CONST(PolicyManager_Constants, REDUCTION_SIGNED, Channel::REDUCTION_SIGNED);
//--------------------------------------------------------------------
//! brief Implement Action.VirtFunctionLevelReset() token
//!
JSBool PmParser::Action_VirtFunctionLevelReset
(
    jsval VF_Descriptor,
    jsval *pReturlwalue
)
{
    PmVfTestDesc *pVfTestDesc;

    RC rc;

    C_CHECK_RC(FromJsval(VF_Descriptor, &pVfTestDesc,  1));

    RETURN_ACTION(new PmAction_VirtFunctionLevelReset(pVfTestDesc));

}
PM_BIND(Action, VirtFunctionLevelReset, 1, "Usage: Action.VirtFunctionLevelReset(VF_Descriptor)",
    "Trigger VF FLR");

//--------------------------------------------------------------------
//! brief Implement Action.WaitFlrDone() token
//!
JSBool PmParser::Action_WaitFlrDone
(
    jsval VF_Descriptor,
    jsval *pReturlwalue
)
{
    PmVfTestDesc *pVfTestDesc;

    RC rc;

    C_CHECK_RC(FromJsval(VF_Descriptor, &pVfTestDesc,  1));

    RETURN_ACTION(new PmAction_WaitFlrDone(pVfTestDesc));

}
PM_BIND(Action, WaitFlrDone, 1, "Usage: Action.WaitFlrDone(test)",
    "Waiting FLR_PENDING bit clear if it's not cleared by user; wait vgpu plugin is unloaded");

//--------------------------------------------------------------------
//! brief Implement Action.ClearFlrPendingBit() token
//!
JSBool PmParser::Action_ClearFlrPendingBit
(
    jsval VF_Descriptor,
    jsval *pReturlwalue
)
{
    PmVfTestDesc *pVfTestDesc;

    RC rc;

    C_CHECK_RC(FromJsval(VF_Descriptor, &pVfTestDesc,  1));

    RETURN_ACTION(new PmAction_ClearFlrPendingBit(pVfTestDesc));

}
PM_BIND(Action, ClearFlrPendingBit, 1, "Usage: Action.ClearFlrPendingBit(test)",
    "Clear FLR_PENDING bit; Mods will set RM registry key to avoid RM clears the bit once this action is seen.");

//--------------------------------------------------------------------
//! brief Implement Action.ExitLwrrentVfProcess() token
//!
JSBool PmParser::Action_ExitLwrrentVfProcess
(
    jsval *pReturlwalue
)
{
    RC rc;

    RETURN_ACTION(new PmAction_ExitLwrrentVfProcess());
}

PM_BIND(Action, ExitLwrrentVfProcess, 0, "Usage: Action.ExitLwrrentVfProcess()",
    "Exit current vf process");

//--------------------------------------------------------------------
//! brief Implement ActionBlock.OnEngineIsExisting() token
//!
JSBool PmParser::ActionBlock_OnEngineIsExisting
(
    jsval aEngineName,
    jsval *pReturlwalue
)
{
    RC rc;

    string engineName;
    C_CHECK_RC(CheckForActionBlockScope("ActionBlock.OnEngineIsExisting"));

    PushScope(Scope::CONDITION);

    C_CHECK_RC(FromJsval(aEngineName, &engineName,  1));
    m_Scopes.top().m_pGotoAction = new PmAction_OnEngineIsExisting(
            engineName,
            m_Scopes.top().m_pSmcEngineDesc
            );
    m_Scopes.top().m_pGotoAction->SetSource(
                        m_Scopes.top().m_pActionBlock->GetSize());
    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, OnEngineIsExisting, 1, "Usage: Action.OnEngineIsExisting(engineName)",
    "Create a conditional action block that will execute if the specified engine existed");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.TryMutex() token
//!
JSBool PmParser::ActionBlock_TryMutex
(
    jsval name,
    jsval *pReturlwalue
)
{
    RC rc;
    RegExp mutexName;

    PushScope(Scope::MUTEX_CONDITION);

    C_CHECK_RC(FromJsval(name, &mutexName, 1, RegExp::FULL |
                                              RegExp::ICASE));
    m_Scopes.top().m_MutexName = mutexName;
    m_Scopes.top().m_pGotoAction = new PmAction_TryMutex(mutexName);
    m_Scopes.top().m_pGotoAction->SetSource(
            m_Scopes.top().m_pActionBlock->GetSize());
    RETURN_ACTION(m_Scopes.top().m_pGotoAction);
}

PM_BIND(ActionBlock, TryMutex, 1,
        "Usage: ActionBlock.TryMutex(regExp)",
        "Acquire the mutex at a condition actionblock, this is non-block action. If the acquire is failed, "
        "Policy Manager will go ahead to execute other action.");

//--------------------------------------------------------------------
//! \brief Implement ActionBlock.OnMutex() token
//!
JSBool PmParser::ActionBlock_OnMutex
(
    jsval name,
    jsval *pReturlwalue
)
{
    RC rc;
    RegExp mutexName;

    PushScope(Scope::MUTEX);

    C_CHECK_RC(FromJsval(name, &mutexName, 1, RegExp::FULL |
                                              RegExp::ICASE));

    m_Scopes.top().m_MutexName = mutexName;
    RETURN_ACTION(new PmAction_OnMutex(mutexName));
}

PM_BIND(ActionBlock, OnMutex, 1,
        "Usage: ActionBlock.OnMutex(regExp)",
        "Set the mutex to an actionblock which can mutex with UTL. This is block action. It will not execute "
        "other action until this acquire success.");

//--------------------------------------------------------------------
//! brief Implement Policy.SetTlbIlwalidateReplayCancelTargetedFaulting() token
//!
JSBool PmParser::Policy_SetTlbIlwalidateReplayCancelTargetedFaulting
(
    jsval *pReturlwalue
)
{
    RC rc;

    m_Scopes.top().m_TlbIlwalidateReplay =
                PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED_FAULTING;
    RETURN_RC(OK);
}

PM_BIND(Policy, SetTlbIlwalidateReplayCancelTargetedFaulting, 0, "Usage: Policy.SetTlbIlwalidateReplayCancelTargetedFaulting()",
    "Replay cancel the faulting request from the fault buffer which GPCId and ClientId can matched");

//--------------------------------------------------------------------
//! \brief Implement Action.SaveFB() token
//!
JSBool PmParser::Action_SaveFB
(
    jsval aVm,
    jsval aFile,
    jsval *pReturlwalue
)
{
    RC rc;
    PmVfTestDesc* pVfDesc = nullptr;
    PmSmcEngineDesc* pSmcEngDesc = nullptr;
    string file;

    rc = FromJsval(aVm, &pSmcEngDesc, 1);
    if (rc != OK)
    {
        pSmcEngDesc = nullptr;
        C_CHECK_RC(FromJsval(aVm, &pVfDesc, 1));
    }
    C_CHECK_RC(FromJsval(aFile, &file, 2));
    RETURN_ACTION(new PmAction_FbCopy("Action.SaveFB", true, pVfDesc, pSmcEngDesc, file));
}

PM_BIND(Action, SaveFB, 2, "Usage: Action.SaveFB(VmId, file)",
        "Save FB mem of an SMC partition having the SmcEngine, or of an SRIOV VF "
        "to a specified file for later restoring back.");

//--------------------------------------------------------------------
//! \brief Implement Action.RestoreFB() token
//!
JSBool PmParser::Action_RestoreFB
(
    jsval aVm,
    jsval aFile,
    jsval *pReturlwalue
)
{
    RC rc;
    PmVfTestDesc* pVfDesc = nullptr;
    PmSmcEngineDesc* pSmcEngDesc = nullptr;
    string file;

    rc = FromJsval(aVm, &pSmcEngDesc, 1);
    if (rc != OK)
    {
        pSmcEngDesc = nullptr;
        C_CHECK_RC(FromJsval(aVm, &pVfDesc, 1));
    }
    C_CHECK_RC(FromJsval(aFile, &file, 2));
    RETURN_ACTION(new PmAction_FbCopy("Action.RestoreFB", false, pVfDesc, pSmcEngDesc, file));
}

PM_BIND(Action, RestoreFB, 2, "Usage: Action.RestoreFB(VmId, file)",
        "Restore FB mem from a specified file back to FB mem for an SMC partition having "
        "the SmcEngine, or of an SRIOV VF.");

//--------------------------------------------------------------------
//! \brief Implement Action.SaveFBSeg() token
//!
JSBool PmParser::Action_SaveFBSeg
(
    jsval aFile,
    jsval aPa,
    jsval aSize,
    jsval *pReturlwalue
)
{
    RC rc;
    string file;
    UINT64 paOff = 0;
    UINT64 size = 0;

    C_CHECK_RC(FromJsval(aFile, &file, 1));
    C_CHECK_RC(FromJsval(aPa, &paOff, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_FbCopySeg("Action.SaveFBSeg", true, file,
            paOff, size));
}

PM_BIND(Action, SaveFBSeg, 3, "Usage: Action.SaveFBSeg(fileName, paOff, size)",
        "Save FB mem to a file specified by fileName.");

//--------------------------------------------------------------------
//! \brief Implement Action.RestoreFBSeg() token
//!
JSBool PmParser::Action_RestoreFBSeg
(
    jsval aFile,
    jsval aPa,
    jsval aSize,
    jsval *pReturlwalue
)
{
    RC rc;
    string file;
    UINT64 paOff = 0;
    UINT64 size = 0;

    C_CHECK_RC(FromJsval(aFile, &file, 1));
    C_CHECK_RC(FromJsval(aPa, &paOff, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_FbCopySeg("Action.RestoreFBSeg", false, file,
            paOff, size));
}

PM_BIND(Action, RestoreFBSeg, 3, "Usage: Action.RestoreFBSeg(fileName, paOff, size)",
        "Load FB mem from file specified by fileName.");

//--------------------------------------------------------------------
//! \brief Implement Action.SaveFBSegSurf() token
//!
JSBool PmParser::Action_SaveFBSegSurf
(
    jsval aFile,
    jsval aPa,
    jsval aSize,
    jsval *pReturlwalue
)
{
    RC rc;
    string file;
    UINT64 paOff = 0;
    UINT64 size = 0;

    C_CHECK_RC(FromJsval(aFile, &file, 1));
    C_CHECK_RC(FromJsval(aPa, &paOff, 2));
    C_CHECK_RC(FromJsval(aSize, &size, 3));
    RETURN_ACTION(new PmAction_FbCopySegSurf("Action.SaveFBSegSurf", true, file,
            paOff, size));
}

PM_BIND(Action, SaveFBSegSurf, 3, "Usage: Action.SaveFBSegSurf(fileName, paOff, size)",
        "Save FB mem to a file specified by fileName.");

//--------------------------------------------------------------------
//! \brief Implement Action.SaveRunlist() token
//!
JSBool PmParser::Action_SaveRunlist
(
    jsval channel,
    jsval *pReturlwalue
)
{
    RC rc;
    PmChannelDesc* pDesc;

    C_CHECK_RC(FromJsval(channel, &pDesc, 1));
    RETURN_ACTION(new PmAction_FbCopyRunlist("Action.SaveRunlist", true, pDesc, ""));
}

PM_BIND(Action, SaveRunlist, 1, "Usage: Action.SaveRunlist(channel)",
        "Save a runlist FB buffer to local buffer.");

//--------------------------------------------------------------------
//! \brief Implement Action.RestoreRunlist() token
//!
JSBool PmParser::Action_RestoreRunlist
(
    jsval channel,
    jsval *pReturlwalue
)
{
    RC rc;
    PmChannelDesc* pDesc;

    C_CHECK_RC(FromJsval(channel, &pDesc, 1));

    RETURN_ACTION(new PmAction_FbCopyRunlist("Action.RestoreRunlist", false, pDesc, ""));
}

PM_BIND(Action, RestoreRunlist, 1, "Usage: Action.RestoreRunlist(channel)",
        "Restore a runlist FB buffer from local buffer.");

//--------------------------------------------------------------------
//! \brief Implement Action.DmaCopyFB() token
//!
JSBool PmParser::Action_DmaCopyFB
(
    jsval aVmFrom,
    jsval aVmTo,
    jsval *pReturlwalue
)
{
    RC rc;
    PmVfTestDesc* pVfDescFrom = nullptr;
    PmVfTestDesc* pVfDescTo = nullptr;
    PmSmcEngineDesc* pSmcEngDescFrom = nullptr;
    PmSmcEngineDesc* pSmcEngDescTo = nullptr;

    rc = FromJsval(aVmFrom, &pSmcEngDescFrom, 1);
    if (rc != OK)
    {
        pSmcEngDescFrom = nullptr;
        C_CHECK_RC(FromJsval(aVmFrom, &pVfDescFrom, 1));
        C_CHECK_RC(FromJsval(aVmTo, &pVfDescTo, 2));
    }
    else
    {
        C_CHECK_RC(FromJsval(aVmTo, &pSmcEngDescTo, 2));
    }

    RETURN_ACTION(new PmAction_DmaCopyFb(pVfDescFrom,
            pVfDescTo,
            pSmcEngDescFrom,
            pSmcEngDescTo
            ));
}

PM_BIND(Action, DmaCopyFB, 2, "Usage: Action.DmaCopyFB(VmFrom, VmTo)",
        "CE Copy FB mem of a VM or of an SMC partition to another one.");

//--------------------------------------------------------------------
//! \brief Implement Action.DmaSaveFB() token
//!
JSBool PmParser::Action_DmaSaveFB
(
    jsval aVm,
    jsval *pReturlwalue
)
{
    RC rc;
    PmVfTestDesc* pVfDesc = nullptr;

    C_CHECK_RC(FromJsval(aVm, &pVfDesc, 1));

    RETURN_ACTION(new PmAction_2StepsDmaCopyFb(
            "Action.DmaSaveFB",
            pVfDesc,
            true,
            false
            ));
}

PM_BIND(Action, DmaSaveFB, 1, "Usage: Action.DmaSaveFB(Vm)",
        "CE Copy FB mem of a VM to vidmem.");

//--------------------------------------------------------------------
//! \brief Implement Action.DmaRestoreFB() token
//!
JSBool PmParser::Action_DmaRestoreFB
(
    jsval aVm,
    jsval *pReturlwalue
)
{
    RC rc;
    PmVfTestDesc* pVfDesc = nullptr;

    C_CHECK_RC(FromJsval(aVm, &pVfDesc, 1));

    RETURN_ACTION(new PmAction_2StepsDmaCopyFb(
            "Action.DmaRestoreFB",
            pVfDesc,
            false,
            false
            ));
}

PM_BIND(Action, DmaRestoreFB, 1, "Usage: Action.DmaRestoreFB(Vm)",
        "CE Copy FB mem of a VM from vidmem.");

//--------------------------------------------------------------------
//! \brief Implement Action.DmaSaveFBSeg() token
//!
JSBool PmParser::Action_DmaSaveFBSeg
(
    jsval aPa,
    jsval aSize,
    jsval *pReturlwalue
)
{
    RC rc;
    UINT64 paOff = 0;
    UINT64 size = 0;

    C_CHECK_RC(FromJsval(aPa, &paOff, 1));
    C_CHECK_RC(FromJsval(aSize, &size, 2));
    RETURN_ACTION(new PmAction_DmaCopySeg("Action.DmaSaveFBSeg", true,
            paOff, size));
}

PM_BIND(Action, DmaSaveFBSeg, 2, "Usage: Action.DmaSaveFBSeg(paOff, size)",
        "DMA save FB mem seg to vidmem as copy.");

//--------------------------------------------------------------------
//! \brief Implement Action.DmaRestoreFBSeg() token
//!
JSBool PmParser::Action_DmaRestoreFBSeg
(
    jsval aPa,
    jsval aSize,
    jsval *pReturlwalue
)
{
    RC rc;
    UINT64 paOff = 0;
    UINT64 size = 0;

    C_CHECK_RC(FromJsval(aPa, &paOff, 1));
    C_CHECK_RC(FromJsval(aSize, &size, 2));
    RETURN_ACTION(new PmAction_DmaCopySeg("Action.DmaRestoreFBSeg", false,
            paOff, size));
}

PM_BIND(Action, DmaRestoreFBSeg, 2, "Usage: Action.DmaRestoreFBSeg(paOff, size)",
        "DMA load FB mem seg from saved vidmem copy.");

//--------------------------------------------------------------------
//! \brief Implement Action.SaveSmcPartFbInfo() token
//!
JSBool PmParser::Action_SaveSmcPartFbInfo
(
    jsval aFile,
    jsval *pReturlwalue
)
{
    RC rc;
    string file;

    C_CHECK_RC(FromJsval(aFile, &file, 1));
    RETURN_ACTION(new PmAction_SaveSmcPartFbInfo(file));
}

PM_BIND(Action, SaveSmcPartFbInfo, 1, "Usage: Action.SaveSmcPartFbInfo(file)",
        "Save all SMC partition FB mem info including start offset and size as a list to a specified file.");

//--------------------------------------------------------------------
//! \brief Implement Policy.SetFbCopyVerif() token
//!
JSBool PmParser::Policy_SetFbCopyVerif(jsval *pReturlwalue)
{
    MDiagvGpuMigration* pVm = GetMDiagvGpuMigration();
    Printf(Tee::PriDebug,
        "%s, " vGpuMigrationTag ", FbCopy read back verify set to True.\n",
        __FUNCTION__);
    pVm->SetFbCopyVerif(true);
    RETURN_RC(OK);
}

PM_BIND(Policy, SetFbCopyVerif, 0,
        "Usage: Policy.SetFbCopyVerif()",
        "Verify read back when restore FB mem.\n");

