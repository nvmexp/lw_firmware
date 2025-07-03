/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// $Id: $

//--------------------------------------------------------------------
//!
//! \file argread.h
//! \brief Create an argument database reader
//!
//! The command line reader will parse options from an argument database
//! that match a list of options that are provided in the constructor of
//! the ArgReader class.
//!
//--------------------------------------------------------------------
//

#ifndef INCLUDED_ARGREAD_H
#define INCLUDED_ARGREAD_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif
#ifndef INCLUDED_STL_STRING
#define INCLUDED_STL_STRING
#include <string>
#endif

// Forward declarations
class ArgDatabase;

//--------------------------------------------------------------------
//! This structure describes an individual entry in an array of parameter
//! declarations.  Each parameter declaration describes a parameter and
//! the associated type/number of arguments associated with the parameter
struct ParamDecl
{
    const char *pParam;  //!< param string (should probably start with - or +)
                         //!<  (e.g. '-info' '+config_file')
                         //!<  a value of 0 here ends a param declaration list

    const char *pArgTypes;//!< string containing one character for each expected arg
                          //!< two formats are supported:
                          //!<   -param [arg1 [arg2 [...]]]
                          //!<   -param=arg1[,arg2[,...]]
                          //!<
                          //!< acceptable type characters are:
                          //!<   't' - ascii text
                          //!<   'u' - unsigned number (any base)
                          //!<   's' - signed number (any base)
                          //!<   'x' - unsigned number in hex ('0x' optional)
                          //!<   'L' - 64-bit unsigned number in hex ('0x' optional)
                          //!<   'f' - floating point
                          //!<
                          //!< also used for value for param group members

    UINT32      flags;    //!< choose flags from:

    //-------------------------------------------------------------------------
    //! This enumeration describes the flags available for each parameter
    enum
    {
         PARAM_MULTI_OK        = (1 <<  0), //!< multiple uses of param
                                            //!< shouldn't issue warnings
         PARAM_ENFORCE_RANGE   = (1 <<  1), //!< require first arg to fall in
                                            //!< specified range
         PARAM_FEWER_OK        = (1 <<  2), //!< if the =,,, mode is used,
                                            //!< allows fewer args
         PARAM_REQUIRED        = (1 <<  3), //!< param MUST be specified -
                                            //!< report an error if not
         GROUP_START           = (1 <<  4), //!< start of param group (multiple
                                            //!< params touching one result data
                                            //!< location
         GROUP_MEMBER          = (1 <<  5), //!< member of param group (argtypes
                                            //!< contains value)
         GROUP_OVERRIDE_OK     = (1 <<  6), //!< don't warn if multiple params
                                            //!< from same group specified
         PARAM_SUBDECL_PTR     = (1 <<  7), //!< this is not a param - it's a
                                            //!< pointer to another decl list
                                            //!< (hidden in 'param')
         GROUP_MEMBER_UNSIGNED = (1 <<  8), //!< treat argtype for groupmember
                                            //!< as an unsigend value, not a
                                            //!< string
         ALIAS_START           = (1 <<  9), //!< start of param alias (multiple
                                            //!< params touching one result data
                                            //!< location)
         ALIAS_MEMBER          = (1 << 10), //!< member of param alias
         ALIAS_OVERRIDE_OK     = (1 << 11), //!< don't warn if multiple params
                                            //!< from same alias specified
         PARAM_PROCESS_ONCE    = (1 << 12), //!< Only process the parameter /
                                            //!< arguments once (by the first
                                            //!< reader)
         PARAM_TEST_ESCAPE     = (1 << 13), //!< The presence of this argument could
                                            //!< create a test escape condition
    };

    UINT32 rangeMin;    //!< Minimum value for the argument (only used PARAM_ENFORCE_RANGE is specified)
    UINT32 rangeMax;    //!< Maximum value for the argument (only used PARAM_ENFORCE_RANGE is specified)

    const char *pDescription;  //!< Description of the argument
};

//--------------------------------------------------------------------
//! This structure describes constraints that occur between two parameters.
//! I.e. if one parameter exists, then the other may not exists, etc.
//! The constraints for both parameters must be specified:
//! existance, !existance, a specific value, min/max range.
struct ParamConstraints
{
    const char *pParam1;     //!< Paramter 1 string
    const char *pParam2;     //!< Paramter 2 string

    const char *pValue1;     //!< Parameter 1 value (query ArgReader for param
                             //!< type to colwert the value)
    const char *pValue2;     //!< Parameter 2 value (query ArgReader for param
                             //!< type to colwert the value)

    UINT32 rangeMin1;        //!< Parameter 1 min value (only with PARAM_ENFORCE_RANGE)
    UINT32 rangeMax1;        //!< Parameter 1 max value (only with PARAM_ENFORCE_RANGE)

    UINT32 rangeMin2;        //!< Parameter 2 min value (only with PARAM_ENFORCE_RANGE)
    UINT32 rangeMax2;        //!< Parameter 2 max value (only with PARAM_ENFORCE_RANGE)

    UINT32 flags1;           //!< Parameter 1 constraint flags
    UINT32 flags2;           //!< Parameter 2 constraint flags

    //-------------------------------------------------------------------------
    //! This enumeration describes constraints that must be enforced for each
    //! parameter (used in flags# member)
    enum
    {
        PARAM_EXISTS = 1,           //!< Parameter must exist
        PARAM_DOESNT_EXIST = 2,     //!< Parameter must not exist
        PARAM_ENFORCE_RANGE = 4     //!< Parameter must be in the specified range
    };
};

//-------------------------------------------------------------------------
// The following macros aid in the construction of ParamDecl arrays

//! Declare a parameter that takes no arguments
#define SIMPLE_PARAM(param,desc)      { param,   0, 0, 0, 0, desc }

//! Declare a parameter that takes a single unsigned integer
#define UNSIGNED_PARAM(param,desc)    { param, "u", 0, 0, 0, desc }

//! Declare a parameter that takes a single 64-big unsigned integer
#define UNSIGNED64_PARAM(param,desc)    { param, "L", 0, 0, 0, desc }

//! Declare a parameter that takes a single signed integer
#define SIGNED_PARAM(param,desc)      { param, "s", 0, 0, 0, desc }

//! Declare a parameter that takes a single floating point value
#define FLOAT_PARAM(param,desc)       { param, "f", 0, 0, 0, desc }

//! Declare a parameter that takes a single string value
#define STRING_PARAM(param,desc)      { param, "t", 0, 0, 0, desc }

//! Declare a parameter that takes a single string value (and is a required
//! parameter
#define REQD_STRING_PARAM(param,desc)         \
    { param, "t", ParamDecl::PARAM_REQUIRED, 0, 0, desc }

//! Declare a parameter that takes a two unsigned integers that fall within
//! the specified range
#define TWO_ARGS_IN_RANGE(param,min,max,desc) \
    { param, "uu", ParamDecl::PARAM_ENFORCE_RANGE, (min), (max), desc }

//! Import an existing parameter array into the current array
#define PARAM_SUBDECL(subdecl)                \
    { ((const char *)(subdecl)), 0, ParamDecl::PARAM_SUBDECL_PTR, 0, 0, 0 }

//! Declare the '-forever' parameter
#define FOREVER_PARAM                         \
    SIMPLE_PARAM("-forever", "run test until error or another test finishes")

//! Sentinal value must always be present as the last value in any parameter
//! array
#define LAST_PARAM { 0, 0, 0, 0, 0, 0 }

//-------------------------------------------------------------------------
// The following macros aid in the construction of ParamConstraints arrays

//! Define a constraint for two mutually exclusive parameters (i.e. if one
//! is specified, the other must not be specified
#define MUTUAL_EXCLUSIVE_PARAM(param1,param2) \
    { param1, param2, 0, 0, 0, 0, 0, 0,       \
      ParamConstraints::PARAM_EXISTS, ParamConstraints::PARAM_DOESNT_EXIST }

//! Sentinal value must always be present as the last value in any parameter
//! constraint array
#define LAST_CONSTRAINT { 0, 0, 0, 0, 0, 0 }

//--------------------------------------------------------------------
//! \brief Command line argument reader class
//!
//! Parses arguments out of an ArgDatabase that match a parameter list
class ArgReader
{
public:
    ArgReader(const ParamDecl        *pParamList,
              const ParamConstraints *pConstraints = NULL);
    ~ArgReader();

    UINT32 ParseArgs(ArgDatabase *pArgDb,
                     const char  *pScope = NULL,
                     bool         bIncludeGlobalScope = true);
    UINT32 ValidateConstraints(const ParamConstraints *inConstraints) const;

    static UINT32 ParamDefined(const ParamDecl *pParamList,
                               const char      *pParam);
    UINT32 ParamDefined(const char *pParam) const;

    //-------------------------------------------------------------------------
    // Once an ArgDatabase has been parsed, the following routines may be used
    // to determine which parameters were set and what their arguments were
    //-------------------------------------------------------------------------

    UINT32 ParamPresent(const char *pParam) const;
    bool   ParamBool(const char *pParam, bool bDefault = false) const;
    const char *ParamStr(const char *pParam, const char *pDefault = NULL) const;
    UINT32  ParamUnsigned(const char *pParam, UINT32 defVal = 0) const;
    UINT64  ParamUnsigned64(const char *pParam, UINT64 defVal = 0) const;
    INT32   ParamSigned(const char *pParam, INT32 defVal = 0) const;
    FLOAT64 ParamFloat(const char *pParam, FLOAT64 defVal = 0.0) const;

    const vector<string>* ParamStrList(const char     *pParam,
                                       vector<string> *pDefault = NULL) const;

    UINT32 ParamNArgs(const char *pParam) const;
    UINT32 ParamNArgs(const char *pParam, UINT32 paramInst) const;

    const char *ParamNStr(const char *pParam, UINT32 argNum) const;
    const char *ParamNStr(const char *pParam,
                          UINT32      paramInst,
                          UINT32      argNum) const;

    UINT32 ParamNUnsigned(const char *pParam, UINT32 argNum) const;
    UINT32 ParamNUnsigned(const char *pParam,
                          UINT32      paramInst,
                          UINT32      argNum) const;
    UINT64 ParamNUnsigned64(const char *pParam, UINT32 argNum) const;
    UINT64 ParamNUnsigned64(const char *pParam,
                          UINT32      paramInst,
                          UINT32      argNum) const;
    INT32  ParamNSigned(const char *pParam, UINT32 argNum) const;
    INT32  ParamNSigned(const char *pParam,
                        UINT32      paramInst,
                        UINT32      argNum) const;
    FLOAT64 ParamNFloat(const char *pParam, UINT32 argNum) const;
    FLOAT64 ParamNFloat(const char *pParam,
                        UINT32      paramInst,
                        UINT32      argNum) const;

    void PrintParsedArgs(Tee::Priority pri = Tee::PriNormal) const;
    static void DescribeParams(const ParamDecl *param_list, bool bPrintUndescribed = true);
    INT32 MatchParam( const string& paramName, vector<string> paramValues  ) const;
    void SetStopOnUnknownArg(bool bStop) { m_StopOnUnknownArg = bStop; }
    string GetFirstUnknownArg() const { return m_FirstUnknownArg; }
    void Reset();

private:
    UINT32 ParseArgsRelwrsion(ArgDatabase *pArgDb,
                              const char  *pScope,
                              bool         bIncludeGlobalScope);
    static const ParamDecl *StaticFindParam
    (
        const ParamDecl  *pParamList,
        const char       *pParam,
        UINT32           *pParamNum,
        UINT32           *pValueStart,
        const ParamDecl **ppGroupStart,
        const ParamDecl **ppAliasStart,
        UINT32           *pGroupIdx = NULL
    ); // static version of FindParam, maybe removed in the future
    const ParamDecl *FindParam(const ParamDecl  *pParamList,
                               const char       *pParam,
                               UINT32           *pParamNum,
                               UINT32           *pValueStart,
                               const ParamDecl **ppGroupStart,
                               const ParamDecl **ppAliasStart,
                               UINT32           *pGroupIdx = NULL) const;
    bool CheckParamType(char        type,
                        const char *pVal,
                        bool        bCheckRange,
                        UINT32      rangeMin,
                        UINT32      rangeMax);
    bool CheckParamList(const ParamDecl *pParamList,
                        UINT32          *pParamCount,
                        UINT32          *pArgCount);
    UINT32 CheckRequiredParams(const ParamDecl  *pParamList,
                               UINT32            usageCountIdx);
    bool ParamValid(const ParamDecl  *pParamDecl, const char *pParam,
                    bool bArgRequired) const;
    bool ParamLWalid(const ParamDecl  *pParamDecl, const char *pParam,
                     UINT32 argNum, UINT32 argIndex) const;
    void PrintParsedArgsRelwrsion(Tee::Priority    pri,
                                  const ParamDecl *pParamList,
                                  UINT32          *pUsageCountIdx,
                                  UINT32          *pValuesIdx,
                                  bool            *pbFirstArg) const;

    const ParamDecl        *m_pParamList;    //!< Array of parameters to read
                                             //!< from the database
    const ParamConstraints *m_pConstraints;  //!< Contraints that apply to
                                             //!< m_pParamList
    vector<UINT32>          m_UsageCounts;   //!< Usage counts for each
                                             //!< parameter in m_pParamList
    vector<bool>            m_ParamScoped;   //!< Array that marks whether a
                                             //!< parameter was parsed inside a
                                             //!< scope
    vector<vector<string>>  m_ParamValues;   //!< Parameter values
    ArgDatabase            *m_pArgDb;        //!< Argument database to parse from

    // using hash table to speed up arg-finding.
    struct ArgHashItem
    {
        UINT32           keySize;    //!< Size of argument string
        const ParamDecl *pParamDecl; //!< Pointer to the parameter declaration
                                     //!< stored in m_pParamList array
        const ParamDecl *pGroupStart;//!< Point to the parameter declaration
                                     //!< for the group
        const ParamDecl *pAliasStart;//!< Point to the parameter declaration
                                     //!< for the alias
        UINT32           groupIdx;   //!< Offset in the parameter array from
                                     //!< the group or alias start
        UINT32           paramNum;   //!< Index where parameter is tracked in
                                     //!< m_UsageCounts and m_pParamScoped arrays
        UINT32           valueStart; //!< Index where the parameter values are
                                     //!< tracked in the m_pParamValues array
        ArgHashItem()
        {
            keySize     = 0;
            pParamDecl  = nullptr;
            pGroupStart = nullptr;
            pAliasStart = nullptr;
            groupIdx    = 0;
            paramNum    = 0;
            valueStart  = 0;
        }
    };
    typedef vector<ArgHashItem> ArgHashItemList;

    vector<ArgHashItemList> m_ArgHashTable;    //!< Argument hash table
    UINT32           m_ParamNum;         //!< Number of parameters
                                         //!< (inlwlding subdeclarations)
    UINT32           m_ArgNum;           //!< Number of values
    bool             m_StopOnUnknownArg; //!< Whether to stop parsing as soon as an
                                         //!< unknown argument was read
    string           m_FirstUnknownArg;  //!< First unknown argument

    bool GenArgHashTable(const ParamDecl  *pParamList,
                         UINT32           *pParamCount,
                         UINT32           *pArgCount);
    UINT32 CallwlateHashIndex(const char* str) const;
};

#endif
