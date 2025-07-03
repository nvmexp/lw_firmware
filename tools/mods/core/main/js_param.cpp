/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/js_param.h"
#include "core/include/argread.h"
#include "core/include/utility.h"
#include "core/include/script.h"
#include "core/include/version.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "core/include/gpu.h"
#include "core/include/cmdline.h"

#include "jsstr.h"

#include <string>
#include <map>
#include <set>
#include <string.h>

map<UINT32, string> JsParamDecl::s_OsToString;
const UINT32 JsParamDecl::s_IlwalidParamInst = ~0;

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
//! \param ArgName         : Parameter name (option provided on the command
//!                          line)
//! \param ArgFlags        : ParamDecl flags for the parameter
//! \param ArgMin          : Minimum value for ranged parameter
//! \param ArgMax          : Maximum value for ranged parameter
//! \param ArgHelp         : Help string for the parameter
//! \param JsAction        : Javascript action to take when handling the
//!                          parameter
//! \param JsType          : Javascript type for the parameter
//! \param NonSupportFlags : Flags indicating where the parameter is NOT
//!                          supported
//! \param bDeviceParam    : true if the parameter is device specific
JsParamDecl::JsParamDecl(string       ArgName,
                         UINT32       ArgFlags,
                         FLOAT64      ArgMin,
                         FLOAT64      ArgMax,
                         string       ArgHelp,
                         string       JsAction,
                         JsParamTypes JsType,
                         UINT32       NonSupportFlags,
                         bool         bDeviceParam)
:   m_pParamDecl(NULL),
    m_bDeviceParam(bDeviceParam),
    m_JsAction(JsAction),
    m_JsType(JsType),
    m_NonSupportFlags(NonSupportFlags),
    m_bEnforceRange(!!(ArgFlags & ParamDecl::PARAM_ENFORCE_RANGE)),
    m_ParamMin(ArgMin),
    m_ParamMax(ArgMax),
    m_pJsParamObj(NULL)
{
    // Create the OS to string mapping if it doesn't exist
    if (s_OsToString.empty())
    {
        s_OsToString[OS_NONE]     = "Unknown OS";
        s_OsToString[OS_DOS]      = "Dos";
        s_OsToString[OS_WIN]      = "Win";
        s_OsToString[OS_MAC]      = "Mac";
        s_OsToString[OS_MACRM]    = "MacRM";
        s_OsToString[OS_WINSIM]   = "WinSim";
        s_OsToString[OS_LINUXSIM] = "LinuxSim";
        s_OsToString[OS_LINUX]    = "Linux";
        s_OsToString[OS_LINUXRM]  = "LinuxRM";
        s_OsToString[OS_ANDROID]  = "Android";
        s_OsToString[OS_WINMFG]   = "WinMfg";
        s_OsToString[OS_VISTARM]  = "VistaRM";
        s_OsToString[OS_WINDA]    = "WinDA";
        s_OsToString[OS_LINDA]    = "LinDA";
    }

    m_pParamDecl = new ParamDecl;
    MASSERT(m_pParamDecl);
    memset(m_pParamDecl, 0, sizeof(ParamDecl));

    // Translate the parameters to a ParamDecl structure.
    // NOTE : Any strings set with Utility::StrDup must be freed in the
    // destructor
    m_pParamDecl->pParam = Utility::StrDup(ArgName.c_str());
    MASSERT(m_pParamDecl->pParam);

    switch(m_JsType)
    {
        case FLAGT:
        case FLAGF:
        case EXEC_JS:
        case FUNC0:
        case DELIM:
            m_pParamDecl->pArgTypes = NULL;
            break;

        case VALI:
        case VALF:
        case VALFM:
        case VALS:
        case FUNC1:
            m_pParamDecl->pArgTypes = Utility::StrDup("t");
            break;

        case FUNC2:
            m_pParamDecl->pArgTypes = Utility::StrDup("tt");
            break;

        case FUNC3:
            m_pParamDecl->pArgTypes = Utility::StrDup("ttt");
            break;

        case FUNC4:
            m_pParamDecl->pArgTypes = Utility::StrDup("tttt");
            break;

        case FUNC5:
            m_pParamDecl->pArgTypes = Utility::StrDup("ttttt");
            break;

        case FUNC6:
            m_pParamDecl->pArgTypes = Utility::StrDup("tttttt");
            break;

        case FUNC7:
            m_pParamDecl->pArgTypes = Utility::StrDup("ttttttt");
            break;

        default:
            break;
    }

    // Range enforcement for JS params is not handled in ArgReader since
    // ArgReader cannot mimic JS colwersion of strings to numbers all
    // numerical colwersion is done in the JsParamDecl class and all
    // JS args appear to ArgReader as strings
    m_pParamDecl->flags = ArgFlags & ~ParamDecl::PARAM_ENFORCE_RANGE;

    if (ArgHelp.size() > 0)
    {
        m_pParamDecl->pDescription = Utility::StrDup(ArgHelp.c_str());
        MASSERT(m_pParamDecl->pDescription);
    }
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
JsParamDecl::~JsParamDecl()
{
    if (m_pParamDecl)
    {
        if (m_pParamDecl->pParam)
            delete[] m_pParamDecl->pParam;
        if (m_pParamDecl->pArgTypes)
            delete[] m_pParamDecl->pArgTypes;
        if (m_pParamDecl->pDescription)
            delete[] m_pParamDecl->pDescription;

        delete m_pParamDecl;
    }

    set<ParamDecl *>::const_iterator groupIter = m_GroupDecls.begin();

    while (groupIter != m_GroupDecls.end())
    {
        delete[] (*groupIter)->pParam;
        delete[] (*groupIter)->pArgTypes;
        if ((*groupIter)->pDescription)
            delete[] (*groupIter)->pDescription;

        delete (*groupIter);
        groupIter++;
    }
    m_GroupDecls.clear();
}

//------------------------------------------------------------------------------
//! \brief Return the parameter name.
//!
//! \return The parameter name.  We should never have a NULL pointer to a
//!         ParamDecl when calling this function.
string JsParamDecl::GetName() const
{
    MASSERT(m_pParamDecl);
    return string(m_pParamDecl->pParam);
}

//------------------------------------------------------------------------------
//! \brief Return the parameter description.
//!
//! \return The parameter description.  We should never have a NULL pointer to a
//!         ParamDecl when calling this function.
string JsParamDecl::GetDescription() const
{
    MASSERT(m_pParamDecl);

    if (m_pParamDecl->pDescription)
        return string(m_pParamDecl->pDescription);

    return "";
}

//------------------------------------------------------------------------------
//! \brief Set the parameter description.
//!
void JsParamDecl::SetDescription(string description)
{
    MASSERT(m_pParamDecl);

    if (m_pParamDecl->pDescription)
    {
        delete[] m_pParamDecl->pDescription;
        m_pParamDecl->pDescription = NULL;
    }

    if (description.size() > 0)
    {
        m_pParamDecl->pDescription = Utility::StrDup(description.c_str());
        MASSERT(m_pParamDecl->pDescription);
    }
}

//------------------------------------------------------------------------------
//! \brief Return the associated ParamDecl.
//!
//! \return The ParamDecl that is being wrapped by this JsParamDecl.  We
//!         should never have a NULL pointer to a ParamDecl when calling this
//!         function.
const ParamDecl * JsParamDecl::GetParamDecl() const
{
    MASSERT(m_pParamDecl);
    return m_pParamDecl;
}

//------------------------------------------------------------------------------
//! \brief Check if the parameter is a group parameter.
//!
//! \return true if the parameter is a group parameter, false otherwise.
bool JsParamDecl::GetIsGroupParam() const
{
    MASSERT(m_pParamDecl);

    return (m_pParamDecl->flags & ParamDecl::GROUP_START) ? true : false;
}

//------------------------------------------------------------------------------
//! \brief Add a group member to the parameter.
//!
//! \param argName     : Arg name for the group member
//! \param value       : String used as the value when the group member is
//!                      encountered
//! \param description : Description for the group member
//!
//! \return OK if successful, not OK otherwise.
RC JsParamDecl::AddGroupMember(string argName, string value, string description)
{
    ParamDecl * pNewParamDecl;
    set<ParamDecl *>::iterator groupIter = m_GroupDecls.begin();

    MASSERT(m_pParamDecl);
    MASSERT(argName.size());
    MASSERT(value.size());

    if (!GetIsGroupParam())
    {
        Printf(Tee::PriError,
               "%s is not a group parameter, unable to add %s as a group member\n",
               m_pParamDecl->pParam, argName.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    while (groupIter != m_GroupDecls.end())
    {
        if (argName == string((*groupIter)->pParam))
        {
            if (value != string((*groupIter)->pArgTypes))
            {
                Printf(Tee::PriError,
                       "Group member %s already declared with different value\n",
                       argName.c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;

            }
            else if (description != string((*groupIter)->pDescription))
            {
                Printf(Tee::PriNormal,
                       "Warning : Updating group member %s description\n"
                       "          Old description : %s\n"
                       "          New description : %s\n",
                       argName.c_str(),
                       (*groupIter)->pDescription,
                       description.c_str());

                delete[] (*groupIter)->pDescription;
                (*groupIter)->pDescription = Utility::StrDup(description.c_str());
                MASSERT((*groupIter)->pDescription);
            }

            // The group member is identical (or only different in the
            // description - which was updated), return success
            return OK;
        }

        groupIter++;
    }

    pNewParamDecl = new ParamDecl;
    MASSERT(pNewParamDecl);
    memset(pNewParamDecl, 0, sizeof(ParamDecl));

    // Translate the parameters to a ParamDecl structure.
    // NOTE : Any strings set with Utility::StrDup must be freed in the
    // destructor
    pNewParamDecl->pParam = Utility::StrDup(argName.c_str());
    MASSERT(pNewParamDecl->pParam);

    pNewParamDecl->pArgTypes = Utility::StrDup(value.c_str());
    MASSERT(pNewParamDecl->pArgTypes);

    if (description.size() > 0)
    {
        pNewParamDecl->pDescription = Utility::StrDup(description.c_str());
        MASSERT(pNewParamDecl->pDescription);
    }
    pNewParamDecl->flags = ParamDecl::GROUP_MEMBER;

    m_GroupDecls.insert(pNewParamDecl);

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the value associated with the group member parameter
//!
//! \param groupMember : Group member to get the value for
//!
//! \return String representation of the value associated with the group member.
//!         An empty string if the specified group member is not a member of the
//!         group.
string JsParamDecl::GetGroupMemberValue(string groupMember)
{
    ParamDecl *pGroupMember = NULL;

    MASSERT(m_pParamDecl);
    MASSERT(groupMember.size());

    if (!GetIsGroupParam())
    {
        Printf(Tee::PriError,
               "%s is not a group parameter, unable to get group "
               "member value\n",
               m_pParamDecl->pParam);
        return "";
    }

    set<ParamDecl *>::iterator groupIter = m_GroupDecls.begin();
    while ((pGroupMember == NULL) && (groupIter != m_GroupDecls.end()))
    {
        if (groupMember == string((*groupIter)->pParam))
        {
            pGroupMember = *groupIter;
        }
        groupIter++;
    }

    if (pGroupMember == NULL)
    {
        Printf(Tee::PriError,
               "%s is not a member of the group parameter %s\n",
               groupMember.c_str(), m_pParamDecl->pParam);
        return "";
    }

    return string(pGroupMember->pArgTypes);
}

//-----------------------------------------------------------------------------
//! \brief Get the first ParamDecl structure in the group member list
//!
//! \return Pointer to the ParamDecl structure of the first group member in the
//!         list.  NULL if no members exist
ParamDecl *  JsParamDecl::GetFirstGroupMemberDecl() const
{
    if (!m_GroupDecls.empty())
        return *(m_GroupDecls.begin());

    return NULL;
}

//-----------------------------------------------------------------------------
//! \brief Get the next ParamDecl structure in the group member list
//!
//! \param pPrev : Pointer to the previous group member in the list
//!
//! \return Pointer to the next ParamDecl in the group member list
ParamDecl *  JsParamDecl::GetNextGroupMemberDecl(ParamDecl * pDecl) const
{
    set<ParamDecl *>::const_iterator groupIter = m_GroupDecls.find(pDecl);

    if (groupIter != m_GroupDecls.end())
        groupIter++;

    if (groupIter != m_GroupDecls.end())
        return *groupIter;

    return NULL;
}

//-----------------------------------------------------------------------------
//! \brief Handle the paratilwlar (non-delimiting) parameter
//!
//! \param pArgReader  : Pointer to an ArgReader containing the parsed command
//!                      line arguments
//! \param devInst     : Device instance that the arguments are targeting,
//!                      Gpu::MaxNumDevices if the argments are global
//!
//! \return OK if parameter handling succeeded, not OK otherwise
RC JsParamDecl::HandleArg(ArgReader *pArgReader, UINT32 devInst) const
{
    UINT32 paramCount = pArgReader->ParamPresent(m_pParamDecl->pParam);
    jsval  jsVal = JSVAL_NULL;
    RC     rc;
    JavaScriptPtr pJs;

    // This parameter was not present in the reader
    if (paramCount == 0)
        return OK;

    if ((devInst != Gpu::MaxNumDevices) && !m_bDeviceParam)
    {
        Printf(Tee::PriNormal, "%s may not be specified per-device, "
                               "applying to all devices\n",
               m_pParamDecl->pParam);
        devInst = Gpu::MaxNumDevices;
    }

    // If the parameter was specified more than once and the parameter
    // does not handle multiple instances, only handle the parameter once
    // which will use the last value provided on the command line
    if ((paramCount > 1) &&
        !(m_pParamDecl->flags & ParamDecl::PARAM_MULTI_OK))
    {
        paramCount = 1;
    }

    if(!IsSupported(false))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    for (UINT32 paramInst = 0; paramInst < paramCount; paramInst++)
    {
        // We have a valid argument.  Handle it.
        switch (m_JsType)
        {
            case FUNC0:
            case FUNC1:
            case FUNC2:
            case FUNC3:
            case FUNC4:
            case FUNC5:
            case FUNC6:
            case FUNC7:
            {
                vector<string> args;
                for (UINT32 i = 0; i < (UINT32)(m_JsType - FUNC0); i++)
                {
                    if (paramCount > 1)
                    {
                        args.push_back(
                            pArgReader->ParamNStr(m_pParamDecl->pParam, paramInst, i));
                    }
                    else
                    {
                        args.push_back(
                            pArgReader->ParamNStr(m_pParamDecl->pParam, i));
                    }
                }
                CHECK_RC(ExecJsFunc(args, devInst));
                break;
            }
            case VALI:
            case VALF:
            case VALFM:
            case VALS:
            {
                const char* sVal = nullptr;
                if (paramCount > 1)
                {
                    sVal = pArgReader->ParamNStr(m_pParamDecl->pParam, paramInst, 0);
                }
                else
                {
                    sVal = pArgReader->ParamStr(m_pParamDecl->pParam);
                }

                // If sVal is NULL the parameter was passed without an
                // argument as a placeholder (which is apparently legal)
                // In this case ArgReader will print a warning message so
                // just continue (in case later instances of the parameter
                // actually have valid data to handle)
                if (sVal == nullptr)
                {
                    continue;
                }

                if (m_JsType == VALS)
                {
                    CHECK_RC(pJs->ToJsval(sVal, &jsVal));
                }
                else
                {
                    CHECK_RC(GetNumberJsVal(sVal, &jsVal));
                }
                CHECK_RC(SetParameterJs(jsVal, devInst));
                break;
            }
            case FLAGT:
                CHECK_RC(pJs->ToJsval(true, &jsVal));
                CHECK_RC(SetParameterJs(jsVal, devInst));
                break;

            case FLAGF:
                CHECK_RC(pJs->ToJsval(false, &jsVal));
                CHECK_RC(SetParameterJs(jsVal, devInst));
                break;

            case EXEC_JS:
                CHECK_RC(ExecJsEval());
                break;

            case DELIM:
                Printf(Tee::PriError, "Use HandleDelim for delim parameters\n");
                return RC::SOFTWARE_ERROR;

            default:
                Printf(Tee::PriError, "Internal error parsing arguments\n");
                return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Handle the edge-case for the last use of -dev
//!
//! \param sVal  : The string value for the parameter
//!
//! \return OK if parameter handling succeeded, not OK otherwise
RC JsParamDecl::HandleFunc1Arg(string sVal) const
{
    RC rc;

    if (m_JsType != FUNC1)
    {
        Printf(Tee::PriError, "%s takes a single function argument\n",
               m_pParamDecl->pParam);
        return RC::SOFTWARE_ERROR;
    }

    vector<string> sVals;
    sVals.emplace_back(move(sVal));
    CHECK_RC(ExecJsFunc(sVals, Gpu::MaxNumDevices));

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Handle a delimiting parameter parameter
//!
//! \param delimIndex  : The command line index where the delimiting parameter
//!                      oclwrred
//!
//! \return OK if parameter handling succeeded, not OK otherwise
RC JsParamDecl::HandleDelim(UINT32 delimIndex) const
{
    JavaScriptPtr pJs;
    JsArray jsArgs;
    RC     rc;
    jsval  jsArg;
    jsval  RetValJs = JSVAL_NULL;
    UINT32 JSRetVal = RC::SOFTWARE_ERROR;

    MASSERT(m_JsType == DELIM);

    CHECK_RC(pJs->ToJsval(delimIndex + 1, &jsArg));
    jsArgs.push_back(jsArg);

    CHECK_RC( pJs->CallMethod(pJs->GetGlobalObject(), m_JsAction, jsArgs, &RetValJs));
    CHECK_RC( pJs->FromJsval(RetValJs, &JSRetVal));
    CHECK_RC(JSRetVal);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Check if two parameters are equal
//!
//! \param p : Parameter to check equality on
//!
//! \return true if equal, false otherwise
bool JsParamDecl::operator==(JsParamDecl &p)
{
    if ((m_pParamDecl->flags != p.m_pParamDecl->flags) ||
        (m_pParamDecl->rangeMin != p.m_pParamDecl->rangeMin) ||
        (m_pParamDecl->rangeMax != p.m_pParamDecl->rangeMax) ||
        (m_bDeviceParam != p.m_bDeviceParam) ||
        (m_JsAction != p.m_JsAction) ||
        (m_JsType != p.m_JsType) ||
        (m_NonSupportFlags != p.m_NonSupportFlags))
    {
        return false;
    }

    if (strcmp(m_pParamDecl->pParam, p.m_pParamDecl->pParam))
        return false;

    if ((m_pParamDecl->pArgTypes != NULL) &&
        (p.m_pParamDecl->pArgTypes != NULL))
    {
        if (strcmp(m_pParamDecl->pArgTypes, p.m_pParamDecl->pArgTypes))
            return false;
    }
    else if (m_pParamDecl->pArgTypes != p.m_pParamDecl->pArgTypes)
    {
        return false;
    }

    // Do not compare descriptions since the description does not affect how
    // the parameter is handled.

    return true;
}

//-----------------------------------------------------------------------------
//! \brief Check if two parameters are not equal
//!
//! \param p : Parameter to check inequality on
//!
//! \return true if not equal, false otherwise
bool JsParamDecl::operator!=(JsParamDecl &p)
{
    return (!(*this == p));
}

//-----------------------------------------------------------------------------
//! \brief Check the parameter is supported
//!
//! \param bQuiet : When true, error prints for non-support are suppressed
//!
//! \return  true if the parameter is supported on the current OS/build,
//!          false otherwise
bool JsParamDecl::IsSupported(bool bQuiet) const
{
    UINT32 Os = OS_NONE;
    UINT64 version = 0;

    switch (Xp::GetOperatingSystem())
    {
        case Xp::OS_DOS:
            Os = OS_DOS;
            break;

        case Xp::OS_WINDOWS:
           // Fetching the windows type: vista or NT
           if (Xp::GetOSVersion(&version) != OK)
           {
               Printf(Tee::PriError, "in Xp::GetWindowsType\n");
               return false;
           }

           // Fetching the Major version
           if (((UINT32)(version>>48))>= 6)
           {
               Os = OS_VISTARM;
           }
           else
           {
               Os = OS_WIN;
           }
           break;

        case Xp::OS_WINMFG:
            Os = OS_WINMFG;
            break;

        case Xp::OS_MAC:
            Os = OS_MAC;
            break;

        case Xp::OS_MACRM:
            Os = OS_MACRM;
            break;

        case Xp::OS_LINUX:
            Os = OS_LINUX;
            break;

        case Xp::OS_LINUXRM:
            Os = OS_LINUXRM;
            break;

        case Xp::OS_ANDROID:
            Os = OS_ANDROID;
            break;

        case Xp::OS_WINSIM:
            Os = OS_WINSIM;
            break;

        case Xp::OS_LINUXSIM:
            Os = OS_LINUXSIM;
            break;

        case Xp::OS_WINDA:
            Os = OS_WINDA;
            break;

        case Xp::OS_LINDA:
            Os = OS_LINDA;
            break;

        case Xp::OS_NONE:
        default:
           // We should never reach this code.
           MASSERT(!"Unknown OS");
           break;
    }

    if (m_NonSupportFlags & Os)
    {
        if (!bQuiet)
        {
            Printf(Tee::PriError, "%s is not supported in %s.\n",
                   m_pParamDecl->pParam, s_OsToString[Os].c_str());
        }
        return false;
    }

    if ((Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK) &&
        (m_NonSupportFlags & US_EXTERNAL))
    {
        if (!bQuiet)
        {
            Printf(Tee::PriError, "%s is not supported.\n",
                   m_pParamDecl->pParam);
        }
        return false;
    }

    if (Platform::IsVirtFunMode() && (m_NonSupportFlags & US_VF))
    {
        if (!bQuiet)
        {
            Printf(Tee::PriError, "%s is not supported in VF MODS.\n",
                   m_pParamDecl->pParam);
        }
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
//! \brief Execute a JS function
//!
//! \param args     : Vector containing the function arguments
//! \param devInst  : Device instance that the arguments are targeting,
//!                   Gpu::MaxNumDevices if the argments are global
//!
//! \return OK if exelwting the JS function succeeded, not OK otherwise
RC   JsParamDecl::ExecJsFunc(vector<string> &args, UINT32 devInst) const
{
    JavaScriptPtr pJs;
    JsArray jsArgs;
    jsval  RetValJs = JSVAL_NULL;
    UINT32 JSRetVal = RC::SOFTWARE_ERROR;
    RC     rc;
    jsval  jsArg;

    if (args.size() < (UINT32)(m_JsType - FUNC0))
    {
        Printf(Tee::PriError,
               "ExecJsFunc : Expected %d args, only %d args found\n",
               (m_JsType - FUNC0), (UINT32)args.size());
        return RC::SOFTWARE_ERROR;
    }

    // If the parameter is device specific, the first parameter to any of the
    // handling functions must be the device instance, if the device is
    // unspecified, execute the handling function on all devices
    if (m_bDeviceParam)
    {
        if (devInst == Gpu::MaxNumDevices)
        {
            for (UINT32 i = 0; i < devInst; i++)
            {
                CHECK_RC(ExecJsFunc(args, i));
            }
            return rc;
        }
        CHECK_RC(pJs->ToJsval(devInst, &jsArg));
        jsArgs.push_back(jsArg);
    }

    for (UINT32 i = 0; i < (UINT32)(m_JsType - FUNC0); i++)
    {
        CHECK_RC(pJs->ToJsval(args[i], &jsArg));
        jsArgs.push_back(jsArg);
    }

    CHECK_RC( pJs->CallMethod(pJs->GetGlobalObject(), m_JsAction, jsArgs, &RetValJs));
    CHECK_RC( pJs->FromJsval(RetValJs, &JSRetVal));
    CHECK_RC(JSRetVal);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Evaluate the JS expression in m_JsAction
//!
//! \param devInst     : Device instance that the arguments are targeting,
//!                      Gpu::MaxNumDevices if the argments are global
//!
//! \return OK if exelwting the JS function succeeded, not OK otherwise
RC JsParamDecl::ExecJsEval() const
{
    RC      rc;
    jsval   jsArg;
    JsArray jsArgs;
    JavaScriptPtr pJs;

    string evalStr = m_JsAction + ";0;";
    CHECK_RC(pJs->ToJsval(evalStr, &jsArg));
    jsArgs.push_back(jsArg);

    CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "EvalArg", jsArgs));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Sets a JS global parameter
//!
//! \param jsVal       : JS value to set to set the JS variable to.
//! \param devInst     : Device instance that the arguments are targeting,
//!                      Gpu::MaxNumDevices if the argments are global
//!
//! \return OK if exelwting the JS function succeeded, not OK otherwise
RC JsParamDecl::SetParameterJs(const jsval& jsVal, UINT32 devInst) const
{
    RC rc;
    jsval   jsArg;
    JsArray jsArgs;
    JavaScriptPtr pJs;

    MASSERT(!JSVAL_IS_NULL(jsVal));
    string propName = m_JsAction;

    // If the argument is per-device the JS variable we are setting is an array element
    if (m_bDeviceParam)
    {
        // If the device is unspecified perform the variable set for all available devices
        if (devInst == Gpu::MaxNumDevices)
        {
            for (UINT32 i = 0; i < devInst; i++)
            {
                CHECK_RC(SetParameterJs(jsVal, i));
            }
            return rc;
        }
        // If the parameter is device specific, the JS variable being set must
        // be an array indexed by the device instance
        else
        {
            propName += Utility::StrPrintf("[%d]", devInst);
        }
    }

    // For now we have to call eval since we can't easily set nested properites from C++
    CHECK_RC(pJs->ToJsval(propName, &jsArg));
    jsArgs.push_back(jsArg);
    jsArgs.push_back(jsVal);
    CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "EvalToValue", jsArgs));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Parse and check Number parameters
//!
//! \param sVal  : The string value for the parameter from the command line
//! \param pJsval : Pointer to the returned jsval that the parameter will be set to
//!
//! \return OK if parsing to a JS number succeeds (and the value passes
//!         range/type checking), not OK otherwise
RC JsParamDecl::GetNumberJsVal(const char* sVal, jsval* pJsVal) const
{
    RC rc;

    JavaScriptPtr pJs;
    JsArray jsArgs(1);
    CHECK_RC(pJs->ToJsval(sVal, &jsArgs[0]));

    jsval retValJs = JSVAL_NULL;
    double fVal = 0.0;
    switch (m_JsType)
    {
        case VALI:
            CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "parseInt", jsArgs, &retValJs));
            CHECK_RC(pJs->FromJsval(retValJs, &fVal));
            break;
        case VALF:
            CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "parseFloat", jsArgs, &retValJs));
            CHECK_RC(pJs->FromJsval(retValJs, &fVal));
            break;
        case VALFM:
            CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "parseFloat", jsArgs, &retValJs));
            CHECK_RC(pJs->FromJsval(retValJs, &fVal));

            // Scale returned JS Value
            fVal *= 1e6;
            CHECK_RC(pJs->ToJsval(fVal, &retValJs));
            break;
        default:
            MASSERT(!"UNSUPPORTED JsParamType");
            return RC::SOFTWARE_ERROR;
    }

    // Check the type of the argument.
    // If the argument parses to NaN consider the type invalid.
    if (std::isnan(fVal))
    {
        Printf(Tee::PriError,
               "\"%s\" parses to %f, which is invalid for parameter \"%s\"\n",
               sVal, fVal, m_pParamDecl->pParam);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Check the argument range if requested
    if (m_bEnforceRange &&
        ((fVal < m_ParamMin) || (fVal > m_ParamMax)))
    {
        Printf(Tee::PriError,
               "%s was out of range (%f) must be between %f and %f\n",
               m_pParamDecl->pParam, fVal, m_ParamMin, m_ParamMax);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    *pJsVal = retValJs;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Javascript class for JsParamDecl
//!
static JSClass JsParamDecl_class =
{
    "JsParamDecl",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    JS_FinalizeStub
};

//-----------------------------------------------------------------------------
static SObject JsParamDecl_Object
(
    "JsParamDecl",
    JsParamDecl_class,
    0,
    0,
    "JsParamDecl JS Object"
);

//-----------------------------------------------------------------------------
//! \brief Create a JSObject for the Parameter
//!
//! \param cx    : Pointer to the javascript context.
//! \param obj   : Pointer to the javascript object.
//!
//! \return OK if the object was successfuly created, not OK otherwise
//!
RC JsParamDecl::CreateJSObject(JSContext *pContext, JSObject *pObject)
{
    //! Only create one JSObject per ArgDatabase
    if (m_pJsParamObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this parameter.\n");
        return OK;
    }

    m_pJsParamObj = JS_DefineObject(pContext,
                                    pObject,
                                    m_pParamDecl->pParam + 1, // Property name
                                    &JsParamDecl_class,
                                    JsParamDecl_Object.GetJSObject(),
                                    JSPROP_READONLY);

    if (!m_pJsParamObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    // Store the current JsParamDecl instance into the private area
    // of the new JSOBject.  This will tie the two together.
    if (JS_SetPrivate(pContext, m_pJsParamObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of parameter.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Return the type of parameter
//!
//! Return the type of parameter
CLASS_PROP_READONLY(JsParamDecl, Type, UINT32, "Return the type of parameter");

//-----------------------------------------------------------------------------
//! \brief Return the name of parameter
//!
//! Return the name of parameter
CLASS_PROP_READONLY(JsParamDecl, Name, string, "Return the name of parameter");

//-----------------------------------------------------------------------------
//! \brief Return the description of the parameter
//!
//! Return the description of the parameter
CLASS_PROP_READONLY(JsParamDecl, Description, string, "Return the description of the parameter");

//-----------------------------------------------------------------------------
//! \brief Return whether the parameter is a group parameter
//!
//! Return true if the parameter is a group parameter, false otherwise
CLASS_PROP_READONLY(JsParamDecl, IsGroupParam, bool,
                    "Return whether the parameter is a group parameter");

JS_SMETHOD_BIND_ONE_ARG(JsParamDecl, GetGroupMemberValue, GroupMember, string,
                        "Get the value associated with a group member");

//-----------------------------------------------------------------------------
//! \brief Constructor, destroy all added parameters
JsParamList::JsParamList() : m_bUpdateParams(true), m_pJsParamListObj(NULL)
{
    ParamConstraints nullConstraint = { 0, 0, 0, 0, 0, 0 };

    m_ParamContraints.push_back(nullConstraint);
}

//-----------------------------------------------------------------------------
//! \brief Destructor, destroy all added parameters
JsParamList::~JsParamList()
{
    set<JsParamDecl *>::iterator declIter = m_JsParams.begin();
    while (declIter != m_JsParams.end())
    {
        delete (*declIter);
        declIter++;
    }
    m_JsParams.clear();

    for (UINT32 i = 0; i < (m_ParamContraints.size() - 1) ; i++)
    {
        // Since only mutually exclusive constraints are supported, it is only
        // necessary to delete the parameter strings
        delete[] m_ParamContraints[i].pParam1;
        delete[] m_ParamContraints[i].pParam2;
    }
    m_ParamContraints.clear();
}

//-----------------------------------------------------------------------------
//! \brief Add a parameter to the parameter list.  Will not add duplicate
//!        parameters with different configurations
//!
//! \param pJsParam : Pointer to the parameter to add
//!
//! \return OK if successful, BAD_COMMAND_LINE_ARGUMENT if the parameter
//!         already exists with a different configuration
RC JsParamList::Add(unique_ptr<JsParamDecl> &pJsParam)
{
    JsParamDecl *pExistingParam = GetParameter(pJsParam->GetName());

    if (pExistingParam != NULL)
    {
        if (*pExistingParam != *pJsParam)
        {
            Printf(Tee::PriError,
                   "Parameter %s already declared with different options\n",
                   pJsParam->GetName().c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;

        }
        else if (pExistingParam->GetDescription() != pJsParam->GetDescription())
        {
            Printf(Tee::PriNormal,
                   "Warning : Updating parameter %s description\n"
                   "          Old description : %s\n"
                   "          New description : %s\n",
                   pJsParam->GetName().c_str(),
                   pExistingParam->GetDescription().c_str(),
                   pJsParam->GetDescription().c_str());
            pExistingParam->SetDescription(pJsParam->GetDescription());
            m_bUpdateParams = true;
        }

        // The parameter is identical (or only different in the
        // description - which was updated), return success
        return OK;
    }

    // Release the unique_ptr since JSParamList now owns the pointer
    m_bUpdateParams = true;
    m_JsParams.insert(pJsParam.release());

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Adds a parameter that is part of a group
//!
//! \param groupParam  : Parameter name for the group start parameter
//! \param groupMember : Parameter name for the group member parameter
//! \param value       : Value for the group member
//! \param description : Help string for the group member
//!
//! \return OK if the parameter was successfully added, not OK otherwise
//!
RC JsParamList::AddGroupMemberParam(string groupParam, string groupMember,
                                    string value, string description)
{
    JsParamDecl * pJsParamDecl = GetParameter(groupParam);
    JsParamDecl * pJsMemberParamDecl = GetParameter(groupMember);

    if (pJsParamDecl == NULL)
    {
        Printf(Tee::PriError, "Group parameter %s not found\n",
               groupParam.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // If the group member parameter was already defined (and not as part
    // of the specified group), then return an error
    if ((pJsMemberParamDecl != NULL) &&
        (pJsMemberParamDecl->GetName() != groupParam))
    {
        Printf(Tee::PriError, "Group member parameter %s already defined\n",
               groupMember.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    RC rc = pJsParamDecl->AddGroupMember(groupMember, value, description);

    if (rc == OK)
        m_bUpdateParams = true;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Adds a mutually exclusive parameter constraint between two
//!        parameters
//!
//! \param param1 : string for the first parameter
//! \param param1 : string for the second parameter
//!
//! \return OK if successful, not OK otherwise
RC JsParamList::AddMutExlusiveConstraint(string param1, string param2)
{
    ParamConstraints constraint = { 0 };

    MASSERT(param1.size() && param2.size());

    constraint.pParam1 = Utility::StrDup(param1.c_str());
    constraint.pParam2 = Utility::StrDup(param2.c_str());

    if ((constraint.pParam1 == NULL) || (constraint.pParam2 == NULL))
        return RC::CANNOT_ALLOCATE_MEMORY;

    constraint.flags1 = ParamConstraints::PARAM_EXISTS;
    constraint.flags2 = ParamConstraints::PARAM_DOESNT_EXIST;

    m_ParamContraints.insert(m_ParamContraints.begin(), constraint);

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Remove a parameter from the list
//!
//! \param param : string for the parameter to remove
//!
//! \return OK
RC JsParamList::Remove(string param)
{
    set<JsParamDecl *>::iterator declIter = m_JsParams.begin();

    while (declIter != m_JsParams.end())
    {
        if ((*declIter)->GetName() == param)
        {
            delete (*declIter);
            m_JsParams.erase(declIter);
            m_bUpdateParams = true;
            return OK;
        }
        declIter++;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the first parameter in the list
//!
//! \return Pointer to the first parameter in the list
JsParamDecl *JsParamList::GetFirstParameter() const
{
    if (!m_JsParams.empty())
        return *(m_JsParams.begin());

    return NULL;
}

//-----------------------------------------------------------------------------
//! \brief Get the next parameter in the list
//!
//! \param pPrev : Pointer to the previous parameter in the list
//!
//! \return Pointer to the next parameter in the list
JsParamDecl *JsParamList::GetNextParameter(JsParamDecl *pPrev) const
{
    set<JsParamDecl *>::const_iterator paramIter = m_JsParams.find(pPrev);

    if (paramIter != m_JsParams.end())
        paramIter++;

    if (paramIter != m_JsParams.end())
        return *paramIter;

    return NULL;
}

//-----------------------------------------------------------------------------
//! \brief Get the param structure for the specified parameter.  If the
//!        parameter is found as part of a group, then the controlling group
//!        start parameter is returned
//!
//! \param param : Parameter to get the declaration for
//!
//! \return Pointer to the desired parameter (NULL if not found)
JsParamDecl *JsParamList::GetParameter(string param) const
{
    set<JsParamDecl *>::const_iterator paramIter = m_JsParams.begin();

    while (paramIter != m_JsParams.end())
    {
        if ((*paramIter)->GetName() == param)
        {
            return (*paramIter);
        }
        else if ((*paramIter)->GetIsGroupParam())
        {
            ParamDecl *pGroupMember;
            pGroupMember = (*paramIter)->GetFirstGroupMemberDecl();
            while (pGroupMember != NULL)
            {
                if (string(pGroupMember->pParam) == param)
                {
                    return (*paramIter);
                }
                pGroupMember =
                    (*paramIter)->GetNextGroupMemberDecl(pGroupMember);
            }
        }
        paramIter++;
    }

    return NULL;
}

//-----------------------------------------------------------------------------
//! \brief Get the param structure for the specified parameter
//!
//! \param type    : Type of parameter to retrieve
//! \param pParams : Pointer to vector for storing all parameters matching
//!                  that match the type
//!
//! \return Pointer to the desired parameter (NULL if not found)
void JsParamList::GetParameterType(JsParamDecl::JsParamTypes type,
                                   vector<JsParamDecl *> *pParams) const
{
    set<JsParamDecl *>::const_iterator paramIter = m_JsParams.begin();

    while (paramIter != m_JsParams.end())
    {
        if ((*paramIter)->GetType() == type)
        {
            pParams->push_back(*paramIter);
        }
        paramIter++;
    }
}

//-----------------------------------------------------------------------------
//! \brief Describe all supported parameters in the list
//!
void JsParamList::DescribeParams() const
{
    vector<ParamDecl> supportedParams;
    set<JsParamDecl *>::const_iterator declIter = m_JsParams.begin();

    while (declIter != m_JsParams.end())
    {
        if ((*declIter)->IsSupported())
        {
            supportedParams.push_back(*((*declIter)->GetParamDecl()));
            if ((*declIter)->GetIsGroupParam())
            {
                ParamDecl * pGroupMember;
                pGroupMember = (*declIter)->GetFirstGroupMemberDecl();
                while (pGroupMember != NULL)
                {
                    supportedParams.push_back(*pGroupMember);
                    pGroupMember =
                        (*declIter)->GetNextGroupMemberDecl(pGroupMember);
                }
            }
        }
        declIter++;
    }

    ParamDecl         nullParam = { 0, 0, 0, 0, 0, 0 };
    supportedParams.push_back(nullParam);

    ArgReader::DescribeParams(&(supportedParams[0]));
}

//-----------------------------------------------------------------------------
//! \brief Get a pointer to an array of ParamDecl * variables that will work
//!        with an Arg reader
//!
//! \return Pointer to the param decl array
ParamDecl *JsParamList::GetParamDeclArray()
{
    if (m_bUpdateParams)
    {
        ParamDecl         nullParam = { 0, 0, 0, 0, 0, 0 };
        set<JsParamDecl *>::const_iterator declIter = m_JsParams.begin();

        // Recreate the ParamDecl array if something has changed.
        m_Params.clear();

        while (declIter != m_JsParams.end())
        {
            m_Params.push_back(*((*declIter)->GetParamDecl()));
            if ((*declIter)->GetIsGroupParam())
            {
                ParamDecl * pGroupMember;
                pGroupMember = (*declIter)->GetFirstGroupMemberDecl();
                while (pGroupMember != NULL)
                {
                    m_Params.push_back(*pGroupMember);
                    pGroupMember =
                        (*declIter)->GetNextGroupMemberDecl(pGroupMember);
                }
            }
            declIter++;
        }

        m_Params.push_back(nullParam);
    }

    return &m_Params[0];
}

//-----------------------------------------------------------------------------
//! \brief Javascript class for JsParamList
//!
static JSClass JsParamList_class =
{
    "JsParamList",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    JS_FinalizeStub
};

//-----------------------------------------------------------------------------
//! \brief Javascript objcet for JsParamList
//!
static SObject JsParamList_Object
(
    "JsParamList",
    JsParamList_class,
    0,
    0,
    "Parameter list"
);

//-----------------------------------------------------------------------------
//! \brief Create a JSObject for the Parameter List
//!
//! \param cx    : Pointer to the javascript context.
//! \param obj   : Pointer to the javascript object.
//!
//! \return OK if the object was successfuly created, not OK otherwise
//!
RC JsParamList::CreateJSObject(JSContext *pContext, JSObject *pObject)
{
    //! Only create one JSObject per ArgDatabase
    if (m_pJsParamListObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this parameter list.\n");
        return OK;
    }

    m_pJsParamListObj = JS_DefineObject(pContext,
                                        pObject,     // Global object
                                        "g_ParamList",     // Property name
                                        &JsParamList_class,
                                        JsParamList_Object.GetJSObject(),
                                        JSPROP_READONLY);

    if (!m_pJsParamListObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    // Store the current ArgDatabase instance into the private area
    // of the new JSOBject.  This will tie the two together.
    if (JS_SetPrivate(pContext, m_pJsParamListObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of parameter list.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Add a parameter to the parameter list
//!
//! \param ArgName         : Parameter name (option provided on the command
//!                          line)
//! \param ArgRange        : Two entry array containing the min/max values for
//!                          the parameter.  May be NULL if no range check is
//!                          required
//! \param ArgFlags        : ParamDecl flags for the parameter
//! \param ArgHelp         : Help string for the parameter
//! \param JsAction        : Javascript action to take when handling the
//!                          parameter
//! \param JsType          : Javascript type for the parameter
//! \param NonSupportFlags : Flags indicating where the parameter is NOT
//!                          supported
//! \param DeviceParam     : true if the parameter is device specific
//!
//! \return OK if the parameter was successfully added, not OK otherwise
//!
JS_STEST_LWSTOM(JsParamList, Add, 8, "Add a parameter to the list")
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    const char usage[] = "Usage: JsParamList.Add(ArgName, ArgRange, ArgFlags, "
                         "ArgHelp, JsAction, JsType, NonSupportFlags, "
                         "DeviceParam)";

    if (NumArguments != 8)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJs;
    string        ArgName;
    JsArray       ArgRange;
    FLOAT64       ArgMin = 0.0;
    FLOAT64       ArgMax = 0.0;
    UINT32        ArgFlags;
    string        ArgHelp;
    string        JsAction;
    UINT32        JsType;
    UINT32        NonSupportFlags;
    bool          bDeviceParam;

    JsParamList *pJsParamList = JS_GET_PRIVATE(JsParamList, pContext,
                                               pObject, "JsParamList");
    if (pJsParamList == NULL)
    {
        Printf(Tee::PriError,
               "JsParamList.Add : Unable to retrieve parameter list\n");
        JS_ReportWarning(pContext, usage);
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    if ((OK != pJs->FromJsval(pArguments[0], &ArgName)) ||
        (OK != pJs->FromJsval(pArguments[2], &ArgFlags)) ||
        (OK != pJs->FromJsval(pArguments[3], &ArgHelp)) ||
        (OK != pJs->FromJsval(pArguments[4], &JsAction)) ||
        (OK != pJs->FromJsval(pArguments[5], &JsType)) ||
        (OK != pJs->FromJsval(pArguments[6], &NonSupportFlags)) ||
        (OK != pJs->FromJsval(pArguments[7], &bDeviceParam)))
    {
        JS_ReportWarning(pContext, usage);
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    // If the range is present (and the parameter type supports a range check,
    // get the range from the array
    if (((JsType == JsParamDecl::VALI) ||
         (JsType == JsParamDecl::VALF) ||
         (JsType == JsParamDecl::VALFM)) &&
        (pArguments[1] != JSVAL_NULL) &&
        ((OK != pJs->FromJsval(pArguments[1], &ArgRange)) ||
         (ArgRange.size() != 2) ||
         (OK != pJs->FromJsval(ArgRange[0], &ArgMin)) ||
         (OK != pJs->FromJsval(ArgRange[1], &ArgMax))))
    {
        Printf(Tee::PriError,
               "JsParamList.Add : Invalid range specification\n");
        JS_ReportWarning(pContext, usage);
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    // If the range is present, be sure it is enforced
    if (ArgRange.size() == 2)
    {
        ArgFlags |= ParamDecl::PARAM_ENFORCE_RANGE;
    }

    //! Create a JsParamDecl object
    unique_ptr<JsParamDecl> pJsParamDecl(new JsParamDecl(ArgName,
                                           ArgFlags,
                                           ArgMin,
                                           ArgMax,
                                           ArgHelp,
                                           JsAction,
                                           (JsParamDecl::JsParamTypes)JsType,
                                           NonSupportFlags,
                                           bDeviceParam));
    MASSERT(pJsParamDecl.get());

    if (OK != pJsParamList->Add(pJsParamDecl))
    {
        JS_ReportError(pContext,
                       "JsParamList.Add : Invalid duplicate argument");
        return JS_FALSE;
    }

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
//! \brief Add a member to a group parameter
//!
//! \param GroupName       : Parameter name for the group start parameter
//! \param GroupMemberName : Parameter name for the group member parameter
//! \param Value           : Value for the group member
//! \param GroupMemberHelp : Help string for the group member
//!
//! \return OK if the parameter was successfully added, not OK otherwise
//!
JS_STEST_LWSTOM(JsParamList, AddGroupMember, 4,
                "Add a group member to a parameter group")
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    const char usage[] = "Usage: JsParamList.AddGroupMember(GroupName, "
                         "GroupMemberName, Value, GroupMemberHelp)";

    if (NumArguments != 4)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJs;
    string        GroupName;
    string        GroupMemberName;
    string        Value;
    string        GroupMemberHelp;

    JsParamList *pJsParamList = JS_GET_PRIVATE(JsParamList, pContext,
                                               pObject, "JsParamList");
    if (pJsParamList == NULL)
    {
        Printf(Tee::PriError,
               "JsParamList.AddGroupMember : Unable to retrieve parameter list\n");
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if ((OK != pJs->FromJsval(pArguments[0], &GroupName)) ||
        (OK != pJs->FromJsval(pArguments[1], &GroupMemberName)) ||
        (OK != pJs->FromJsval(pArguments[2], &Value)) ||
        (OK != pJs->FromJsval(pArguments[3], &GroupMemberHelp)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (OK != pJsParamList->AddGroupMemberParam(GroupName, GroupMemberName,
                                                Value, GroupMemberHelp))
    {
        JS_ReportError(pContext,
                 "JsParamList.AddGroupMember : Invalid group member argument");
        return JS_FALSE;
    }

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
//! \brief Remove a parameter to the parameter list
//!
//! \param ArgName         : Parameter name (option provided on the command
//!                          line) to remove
//!
//! \return OK if the parameter was successfully removed, not OK otherwise
//!
JS_STEST_BIND_ONE_ARG(JsParamList, Remove,
                      ArgName, string, "Delete a parameter from the list");

//-----------------------------------------------------------------------------
//! \brief Find the argument in the parameter list
//!
//! \param ParamName : Parameter name to find
//!
//! \return JsParamDecl if successful, NULL if the parameter is not found
//!
JS_SMETHOD_LWSTOM(JsParamList, FindParam, 1, "Get the JsParamDecl for the argument")
{

    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    const char usage[] = "Usage: JsParamList.FindArg(ArgName)";
    string ParamName;
    JavaScriptPtr pJs;
    RC rc;

    *pReturlwalue = JSVAL_NULL;

    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &ParamName)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsParamList *pJsParamList = JS_GET_PRIVATE(JsParamList, pContext,
                                               pObject, "JsParamList");
    if (pJsParamList == NULL)
    {
        Printf(Tee::PriError,
               "JsParamList.FindArg : Unable to retrieve parameter list\n");
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsParamDecl *pParamDecl = pJsParamList->GetParameter(ParamName);

    if (pParamDecl != NULL)
    {
        C_CHECK_RC(pParamDecl->CreateJSObject(pContext, pObject));

        if (OK != pJs->ToJsval(pParamDecl->GetJSObject(), pReturlwalue))
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }

    return JS_TRUE;
}

//-----------------------------------------------------------------------------
//! \brief Describe all parameters in the list
//!
JS_SMETHOD_VOID_BIND_NO_ARGS(JsParamList, DescribeParams,
                             "Describe all parameters in the list");

//-----------------------------------------------------------------------------
//! \brief Add a mutually exclusive parameter constraint
//!
//! \param Param1 : Parameter name of first mutually exclusive parameter
//! \param Param2 : Parameter name of second mutually exclusive parameter
//!
//! \return OK if the parameter constraint was successfully added, not OK otherwise
//!
JS_STEST_BIND_TWO_ARGS(JsParamList, AddMutExlusiveConstraint,
                       Param1, string, Param2, string,
                       "Add a mutually exclusive parameter constraint");

//-----------------------------------------------------------------------------
//! \brief ParamConst javascript interface class
//!
//! Create a JS class to declare parameter constants on
//!
JS_CLASS(ParamConst);

//-----------------------------------------------------------------------------
//! \brief ParamConst javascript interface object
//!
//! ParamConst javascript interface
//!
static SObject ParamConst_Object
(
    "ParamConst",
    ParamConstClass,
    0,
    0,
    "ParamConst JS Object"
);

//-----------------------------------------------------------------------------
// Constants from ParamDecl structure (in argread.h)
//-----------------------------------------------------------------------------
PROP_CONST_DESC(ParamConst, PARAM_MULTI_OK,
                ParamDecl::PARAM_MULTI_OK,
                "multiple uses of param shouldn't issue warnings");
PROP_CONST_DESC(ParamConst, PARAM_ENFORCE_RANGE,
                ParamDecl::PARAM_ENFORCE_RANGE,
                "require first arg to fall in specified range");
PROP_CONST_DESC(ParamConst, PARAM_FEWER_OK,
                ParamDecl::PARAM_FEWER_OK,
                "if the =,,, mode is used, allows fewer args");
PROP_CONST_DESC(ParamConst, PARAM_REQUIRED,
                ParamDecl::PARAM_REQUIRED,
                "param MUST be specified - report an error if not");
PROP_CONST_DESC(ParamConst, GROUP_START,
                ParamDecl::GROUP_START,
                "start of param group (multiple params touching one result "
                "data location)");
PROP_CONST_DESC(ParamConst, GROUP_MEMBER,
                ParamDecl::GROUP_MEMBER,
                "member of param group (argtypes contains value)");
PROP_CONST_DESC(ParamConst, GROUP_OVERRIDE_OK,
                ParamDecl::GROUP_OVERRIDE_OK,
                "don't warn if multiple params from same group specified");
PROP_CONST_DESC(ParamConst, PARAM_SUBDECL_PTR,
                ParamDecl::PARAM_SUBDECL_PTR,
                "this is not a param - it's a pointer to another decl list "
                "(hidden in 'param')");
PROP_CONST_DESC(ParamConst, GROUP_MEMBER_UNSIGNED,
                ParamDecl::GROUP_MEMBER_UNSIGNED,
                "treat argtype for groupmember as an unsigend value, not a "
                "string");
PROP_CONST_DESC(ParamConst, ALIAS_START,
                ParamDecl::ALIAS_START,
                "start of param alias (multiple params touching one result "
                "data location)");
PROP_CONST_DESC(ParamConst, ALIAS_MEMBER,
                ParamDecl::ALIAS_MEMBER, "member of param alias");
PROP_CONST_DESC(ParamConst, ALIAS_OVERRIDE_OK,
                ParamDecl::ALIAS_OVERRIDE_OK,
                "don't warn if multiple params from same alias specified");
PROP_CONST_DESC(ParamConst, PARAM_PROCESS_ONCE,
               ParamDecl::PARAM_PROCESS_ONCE,
               "only process the parameter (and any arguments) the first time");
PROP_CONST_DESC(ParamConst, PARAM_TEST_ESCAPE,
               ParamDecl::PARAM_TEST_ESCAPE,
               "usage of the parameter can contitute a test escape");

//-----------------------------------------------------------------------------
// JsParamDecl::JsFlags constants
//-----------------------------------------------------------------------------
PROP_CONST(ParamConst, OS_DOS,      JsParamDecl::OS_DOS);
PROP_CONST(ParamConst, OS_WIN,      JsParamDecl::OS_WIN);
PROP_CONST(ParamConst, OS_MAC,      JsParamDecl::OS_MAC);
PROP_CONST(ParamConst, OS_MACRM,    JsParamDecl::OS_MACRM);
PROP_CONST(ParamConst, OS_WINSIM,   JsParamDecl::OS_WINSIM);
PROP_CONST(ParamConst, OS_LINUXSIM, JsParamDecl::OS_LINUXSIM);
PROP_CONST(ParamConst, OS_LINUX,    JsParamDecl::OS_LINUX);
PROP_CONST(ParamConst, OS_LINUXRM,  JsParamDecl::OS_LINUXRM);
PROP_CONST(ParamConst, OS_ANDROID,  JsParamDecl::OS_ANDROID);
PROP_CONST(ParamConst, OS_WINMFG,   JsParamDecl::OS_WINMFG);
PROP_CONST(ParamConst, OS_VISTARM,  JsParamDecl::OS_VISTARM);
PROP_CONST(ParamConst, OS_UEFI,     JsParamDecl::OS_UEFI);
PROP_CONST(ParamConst, OS_WINDA,    JsParamDecl::OS_WINDA);
PROP_CONST(ParamConst, OS_LINDA,    JsParamDecl::OS_LINDA);
PROP_CONST(ParamConst, OS_NONE,     JsParamDecl::OS_NONE);
PROP_CONST(ParamConst, US_EXTERNAL, JsParamDecl::US_EXTERNAL);
PROP_CONST(ParamConst, US_VF,       JsParamDecl::US_VF);

//-----------------------------------------------------------------------------
// JsParamDecl::JsParamTypes constants
//-----------------------------------------------------------------------------
PROP_CONST(ParamConst, FUNC0,   JsParamDecl::FUNC0);
PROP_CONST(ParamConst, FUNC1,   JsParamDecl::FUNC1);
PROP_CONST(ParamConst, FUNC2,   JsParamDecl::FUNC2);
PROP_CONST(ParamConst, FUNC3,   JsParamDecl::FUNC3);
PROP_CONST(ParamConst, FUNC4,   JsParamDecl::FUNC4);
PROP_CONST(ParamConst, FUNC5,   JsParamDecl::FUNC5);
PROP_CONST(ParamConst, FUNC6,   JsParamDecl::FUNC6);
PROP_CONST(ParamConst, FUNC7,   JsParamDecl::FUNC7);
PROP_CONST(ParamConst, VALI,    JsParamDecl::VALI);
PROP_CONST(ParamConst, VALF,    JsParamDecl::VALF);
PROP_CONST(ParamConst, VALFM,   JsParamDecl::VALFM);
PROP_CONST(ParamConst, VALS,    JsParamDecl::VALS);
PROP_CONST(ParamConst, FLAGT,   JsParamDecl::FLAGT);
PROP_CONST(ParamConst, FLAGF,   JsParamDecl::FLAGF);
PROP_CONST(ParamConst, EXEC_JS, JsParamDecl::EXEC_JS);
PROP_CONST(ParamConst, DELIM,   JsParamDecl::DELIM);

