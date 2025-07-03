/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2013, 2015,2017-2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// $Id: $

//--------------------------------------------------------------------
//!
//! \file js_param.h
//! \brief Javascript interface for parameter declaration
//!
//! Used to create Javascript command line options.
//!
//--------------------------------------------------------------------

#ifndef INCLUDED_JS_PARAM_H
#define INCLUDED_JS_PARAM_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_STL_STRING
#define INCLUDED_STL_STRING
#include <string>
#endif
#ifndef INCLUDED_STL_MAP
#define INCLUDED_STL_MAP
#include <map>
#endif
#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif
#ifndef INCLUDED_STL_SET
#define INCLUDED_STL_SET
#include <set>
#endif
#ifndef INCLUDED_STL_MEMORY
#define INCLUDED_STL_MEMORY
#include <memory>
#endif
#ifndef INCLUDED_ARGREAD_H
#include "argread.h"
#endif
#include "core/include/jscript.h"

// Forward declarations
struct JSObject;
struct JSContext;
class ArgReader;

//--------------------------------------------------------------------
//! \brief Javascript parameter declaration
//!
//! Encapsulates a ParamDecl structure as well as Javascript information
//! for parameter handling
class JsParamDecl
{
public:

    //-------------------------------------------------------------------------
    //! This enumeration describes javascript parameter specific flags for the
    //! parameter (used to indicate which OS/Builds are not supported)
    enum JsFlags
    {
        OS_NONE     =  (0),        //!< No (unknown) OS
        OS_DOS      =  (1 << 0),   //!< DOS OS non-support flag
        OS_WIN      =  (1 << 1),   //!< Windows NT OS non-support flag
        OS_MACRM    =  (1 << 2),   //!< Flavor of MacMODS build (BUILD_OS=macosx)
                                   //!< which uses kernel-side RM
        OS_WINSIM   =  (1 << 3),   //!< Sim OS non-support flag
        OS_LINUXSIM =  (1 << 4),   //!< Sim OS non-support flag
        OS_LINUX    =  (1 << 5),   //!< Linux OS non-support flag
        OS_WINMFG   =  (1 << 6),   //!< Dedicated WinMfgMODS build
                                   //!< (BUILD_OS=winmfg) with the same
                                   //!< capabilities as OS_DOS non-support flag
        OS_VISTARM  =  (1 << 7),   //!< Flavor of WinMODS build (BUILD_OS=win32)
                                   //!< run under Vista OS non-support flag
        OS_LINUXRM  =  (1 << 8),   //!< Flavor of LinuxMODS build (BUILD_OS=linux)
                                   //!< which uses kernel-side RM
        OS_UEFI     =  (1 << 9),   //!< UEFI OS non-support flag
        OS_ANDROID  =  (1 << 10),  //!< Android OS non-support flag
        OS_MAC      =  (1 << 11),  //!< MAC OS non-support flag (BUILD_OS=macosxmfg)
        OS_WINDA    =  (1 << 12),  //!< Direct Amodel on Windows
        OS_LINDA    =  (1 << 13),  //!< Direct Amodel on Linux
        US_EXTERNAL =  (1 << 20),  //!< Unsupported in external mods flag
        US_VF       =  (1 << 21)   //!< Unsupported in VF MODS
    };

    //-------------------------------------------------------------------------
    //! This enumeration describes javascript parameter types (indicate how the
    //! parameter should be handled in javascript)
    enum JsParamTypes
    {
        FUNC0 = 0,  //!< Call a function with 0 parameters
        FUNC1,      //!< Call a function with 1 parameter
        FUNC2,      //!< Call a function with 2 parameters
        FUNC3,      //!< Call a function with 3 parameters
        FUNC4,      //!< Call a function with 4 parameters
        FUNC5,      //!< Call a function with 5 parameters
        FUNC6,      //!< Call a function with 6 parameters
        FUNC7,      //!< Call a function with 7 parameters

        // NOTE : All FUNC parameters must be defined sequentially.  If any
        // additional FUNC* parameters are added they must be added
        // immediately above this line and must be contiguous WRT numbering

        VALI,       //!< Set an integer value
        VALF,       //!< Set a floating point value (NaN is unsupported)
        VALFM,      //!< Set a floating point value * 1e6 (NaN is unsupported)
        VALS,       //!< Set a string value
        FLAGT,      //!< Set a flag to true
        FLAGF,      //!< Set a flag to false
        EXEC_JS,    //!< Execute a JS action
        DELIM       //!< Delimiting arg (all parameters after the delimiter are
                    //!< passed to a custom arg handling function
    };

    JsParamDecl(string       ArgName,
                UINT32       ArgFlags,
                FLOAT64      ArgMin,
                FLOAT64      ArgMax,
                string       ArgHelp,
                string       JsAction,
                JsParamTypes JsType,
                UINT32       NonSupportFlags,
                bool         bDeviceParam);
    ~JsParamDecl();

    bool   IsSupported() const { return IsSupported(true); }
    string GetName() const;
    string GetDescription() const;
    void   SetDescription(string description);
    const string & GetJsAction() const { return m_JsAction; }
    void SetJsAction(const string &newAction) { m_JsAction = newAction; }

    JsParamTypes      GetType() const { return m_JsType; }
    const ParamDecl * GetParamDecl() const;

    bool GetIsGroupParam() const;
    RC AddGroupMember(string param, string value, string description);
    string GetGroupMemberValue(string groupMember);
    ParamDecl *  GetFirstGroupMemberDecl() const;
    ParamDecl *  GetNextGroupMemberDecl(ParamDecl * pDecl) const;

    RC HandleArg(ArgReader *pArgReader, UINT32 devInst) const;
    RC HandleFunc1Arg(string sVal) const;
    RC HandleDelim(UINT32 delimIndex) const;

    bool operator==(JsParamDecl &p);
    bool operator!=(JsParamDecl &p);

    RC     CreateJSObject(JSContext *pContext, JSObject *pObject);
    JSObject * GetJSObject() {return m_pJsParamObj;}

private:
    bool IsSupported(bool bQuiet) const;
    RC   ExecJsFunc(vector<string> &args, UINT32 devInst) const;
    RC   ExecJsEval() const;
    RC   SetParameterJs(const jsval& jsVal, UINT32 devInst) const;
    RC   GetNumberJsVal(const char *s, jsval* pJsVal) const;

    ParamDecl   *m_pParamDecl;      //!< ParamDecl structure associated with
                                    //!< the javascript parameter
    bool         m_bDeviceParam;    //!< True if the parameter is per device
    string       m_JsAction;        //!< Javascript action to take to handle
                                    //!< the parameter
    JsParamTypes m_JsType;          //!< Javascript type for the parameter
    UINT32       m_NonSupportFlags; //!< Flags indicating where the parameter
                                    //!< is not supported
    bool         m_bEnforceRange;   //!< True if parameter range should be
                                    //!< enforced
    FLOAT64      m_ParamMin;        //!< Minimum value for the parameter
    FLOAT64      m_ParamMax;        //!< Maximum value for the parameter

    set<ParamDecl *> m_GroupDecls;  //!< If the parameter is a group this
                                    //!< contains the parameter decl structures
                                    //!< for the group members

    static map<UINT32, string> s_OsToString;    //!< OsTo string map for
                                                //!< printing error messages
    static const UINT32 s_IlwalidParamInst;     //!< Constant indicating a
                                                //!< parameter is not instanced

    //! Javascript representation for the parameter
    JSObject *m_pJsParamObj;
};

//--------------------------------------------------------------------
//! \brief Javascript parameter list declaration
//!
//! Encapsulates a list of JS parameters
class JsParamList
{
public:
    JsParamList();
    ~JsParamList();

    RC Add(unique_ptr<JsParamDecl> &pJsParam);
    RC AddGroupMemberParam(string groupParam, string groupMember,
                           string value, string description);
    RC AddMutExlusiveConstraint(string param1, string param2);
    RC Remove(string param);
    JsParamDecl *GetFirstParameter() const;
    JsParamDecl *GetNextParameter(JsParamDecl *pPrev) const;
    JsParamDecl *GetParameter(string param) const;
    void GetParameterType(JsParamDecl::JsParamTypes type,
                          vector<JsParamDecl *> *pParams) const;
    void DescribeParams() const;
    ParamDecl *GetParamDeclArray();
    ParamConstraints *GetParamConstraintsArray() { return &m_ParamContraints[0]; }

    RC     CreateJSObject(JSContext *pContext, JSObject *pObject);
    JSObject * GetJSObject() {return m_pJsParamListObj;}
private:
    set<JsParamDecl *> m_JsParams; //!< List of JS parameters
    vector<ParamDecl>  m_Params;   //!< ParamDecl vector for returning an
                                   //!< array that will work with an ArgReader
    bool               m_bUpdateParams; //!< true if m_Params needs to be updated
                                        //!< updated the next time
                                        //!< GetParamDeclArray is called

    vector<ParamConstraints> m_ParamContraints;
    //!< ParamConstraints vector for returning anarray that will work with
    //!< an ArgReader

    JSObject *m_pJsParamListObj;
};

#endif // !INCLUDED_JS_PARAM_H
