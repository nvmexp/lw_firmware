
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/argread.h"
#include "core/include/argdb.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

// $Id: $

//-------------------------------------------------------------------------
//!
//! \file argread.cpp
//! \brief Implementation for the argument database reader
//!
//! The command line reader will parse options from an argument database
//! that match a list of options that are provided in the constructor of
//! the ArgReader class.
//!
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
//! \brief Constructor - Creates an ArgReader for the specified parameters
//!
//! \param pParamList   : Pointer to a list of expected command-line parameters
//!                       with the number and type of arguments they take
//! \param pConstraints : Optional pointer to a list of constraints for the
//!                       parameter list.  If present it specifies parameter
//!                       interaction rules (i.e. if paramA is present, paramB
//!                       must not be preset, etc.)
//!
ArgReader::ArgReader(const ParamDecl        *pParamList,
                     const ParamConstraints *pConstraints) :
    m_pParamList(NULL),
    m_pConstraints(NULL),
    m_pArgDb(NULL),
    m_StopOnUnknownArg(false)
{
    UINT32 paramCount, argCount;

    this->m_pParamList = pParamList;
    this->m_pConstraints = pConstraints;

    // count arguments
    paramCount = 0;
    argCount = 0;
    if (!CheckParamList(pParamList, &paramCount, &argCount))
    {
        MASSERT(!"Parameter list is invalid!");
        Printf(Tee::PriLow, "Parameter list is invalid!\n");
    }

    if (paramCount)
    {
        m_UsageCounts.resize(paramCount, 0);
        m_ParamScoped.resize(paramCount, false);
    }

    if (argCount)
    {
        m_ParamValues.resize(argCount);
    }

    // Generate hash table
    m_ParamNum = 0;
    m_ArgNum = 0;
    m_ArgHashTable.resize(paramCount*2);
    GenArgHashTable(pParamList, &m_ParamNum, &m_ArgNum);
    MASSERT(m_ParamNum <= paramCount);
}

//-------------------------------------------------------------------------
//! \brief Destructor - Delete the database and any allocated data
//!
//! Be sure to decrement any reference counts for all parsed databases
ArgReader::~ArgReader()
{
    m_UsageCounts.clear();
    m_ParamScoped.clear();
    m_ParamValues.clear();
    m_ArgHashTable.clear();

    while(m_pArgDb)
    {
        m_pArgDb->m_RefCount--;
        m_pArgDb = m_pArgDb->m_pParentDb;
    }
}

//-----------------------------------------------------------------------------
//! \brief Read through a database of command-line args, pulling out args that
//!        match those specified in the parameter list provided in the
//!        constructor
//!
//! \param pArgDb : Pointer to the argument database to read parameters out of.
//! \param pScope : Optional scope to read parameters based on.  If a scope is
//!                 specified then which arguments from the database that are
//!                 included will be controlled by the bIncludeGlobalScope
//!                 parameter
//! \param bIncludeGlobalScope : Only has an effect when a valid scope is
//!                              specified.  When true both arguments within
//!                              the specified scope and arguments without a
//!                              scope are included when parasing (a scoped
//!                              argument always overrides a duplicate global
//!                              argument.  When false, only arguments within
//!                              the specified scope are considered for parsing
//!
//! \return 1 on success, 0 on failure
//!
UINT32 ArgReader::ParseArgs(ArgDatabase *pArgDb,
                            const char  *pScope /*= NULL*/,
                            bool         bIncludeGlobalScope /* = true*/)
{
    // do actual parsing of args
    if (!ParseArgsRelwrsion(pArgDb, pScope, bIncludeGlobalScope)) return(0);

    if (!ValidateConstraints(m_pConstraints)) return (0);

    // gG through and complain loudly if any params had PARAM_REQUIRED set, and
    // were not found
    return (CheckRequiredParams(m_pParamList, 0));
}

//-----------------------------------------------------------------------------
//! \brief Validates that the arguments that were parsed do not violate the
//!        provided constraints.  Note : This must be called after ParseArgs.
//!
//! \param pInConstraints : The constraints to validate.
//!
//! \return 1 if the constraints are satisfied
//!         0 if the parsed parameters violate the constraints
//!
UINT32 ArgReader::ValidateConstraints(const ParamConstraints *pInConstraints) const
{
    const ParamConstraints *pLwrConstraint = pInConstraints;
    while (pLwrConstraint && pLwrConstraint->pParam1 && pLwrConstraint->pParam2)
    {
        UINT32 param1Present = ParamPresent(pLwrConstraint->pParam1);
        UINT32 param2Present = ParamPresent(pLwrConstraint->pParam2);

        if ((!param1Present || !param2Present) &&
            (pLwrConstraint->flags1 == ParamConstraints::PARAM_EXISTS) &&
            (pLwrConstraint->flags2 == ParamConstraints::PARAM_EXISTS))
        {
            Printf(Tee::PriHigh, "Param %s and %s must be specified together.\n",
                   pLwrConstraint->pParam1, pLwrConstraint->pParam2);
            return 0;
        }

        if ((param1Present && param2Present) &&
            (((pLwrConstraint->flags1 == ParamConstraints::PARAM_EXISTS) &&
              (pLwrConstraint->flags2 == ParamConstraints::PARAM_DOESNT_EXIST)) ||
             ((pLwrConstraint->flags1 == ParamConstraints::PARAM_DOESNT_EXIST) &&
              (pLwrConstraint->flags2 == ParamConstraints::PARAM_EXISTS))))
        {
            Printf(Tee::PriHigh, "Param %s and %s are mutually exclusive.\n",
                   pLwrConstraint->pParam1, pLwrConstraint->pParam2);
            return 0;
        }

        if (pLwrConstraint->pValue1 || pLwrConstraint->pValue2 ||
            pLwrConstraint->rangeMin1 || pLwrConstraint->rangeMax1 ||
            pLwrConstraint->rangeMin2 || pLwrConstraint->rangeMax2)
        {
            Printf(Tee::PriHigh, "Functionality not supported yet.\n");
            return 0;
        }

        pLwrConstraint++;
    }

    return 1;
}

//-----------------------------------------------------------------------------
//! \brief Checks to see if a particular parameter is specified within the
//!        provided parameter list
//!
//! \param pParamList : Pointer to parameter list to search for the parameter.
//! \param pParam     : Pointer to the parameter to search for
//!
//! \return 1 if the parameter is defined, 0 if undefined
//!
/* static */ UINT32 ArgReader::ParamDefined(const ParamDecl *pParamList,
                                            const char      *pParam)
{
    // defend against null pointers for 'param_list'
    if (pParamList)
        return (StaticFindParam(pParamList, pParam, 0, 0, 0, 0) ? 1 : 0);
    else
        return (0);
}

//-----------------------------------------------------------------------------
//! \brief Checks to see if a particular parameter is specified within the
//!        parameter list attached to the ArgReader
//!
//! \param pParam : Pointer to the parameter to search for
//!
//! \return 1 if the parameter is defined, 0 if undefined
//!
UINT32 ArgReader::ParamDefined(const char *pParam) const
{
    return (ParamDefined(m_pParamList, pParam));
}

//-----------------------------------------------------------------------------
//! \brief Checks to see if a particular parameter is present in the parsed
//!        arguments (only thing to do on a param with 0 args).
//!
//! \param pParam : Pointer to the parameter to check for presence in the
//!                 database
//!
//! \return Number of times the parameter was specified
//!
UINT32 ArgReader::ParamPresent(const char *pParam) const
{
    const ParamDecl *pParamDecl;
    UINT32           paramIndex;

    pParamDecl = FindParam(m_pParamList, pParam, &paramIndex, 0, 0, 0);
    if (ParamValid(pParamDecl, pParam, false))
    {
        return (m_UsageCounts[paramIndex]);
    }

    return (0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieves the value for a boolean argument from the parsed values.
//!        If the parameter was specified multiple times, only the last value
//!        is returned
//!
//! \param pParam   : Pointer to the parameter to retrieve a boolean argument
//! \param bDefault : Default value for the boolean argument
//!
//! \return Value for the parameter (if specified), if the parameter was not
//!         specified then the default value is returned
//!
bool ArgReader::ParamBool(const char *pParam, bool bDefault /* = false */) const
{
    return (ParamUnsigned(pParam, (UINT32)bDefault) > 0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieves the value for a string argument from the parsed values.
//!        If the parameter was specified multiple times, only the last value
//!        is returned
//!
//! \param pParam   : Pointer to the parameter to retrieve a string argument
//! \param pDefault : Default value for the string argument
//!
//! \return Value for the parameter (if specified), if the parameter was not
//!         specified then the default value is returned
//!
const char *ArgReader::ParamStr(const char *pParam,
                                const char *pDefault /* = NULL*/) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    if (ParamValid(pParamDecl, pParam, true))
    {
        auto lastValue = m_ParamValues[argIndex].rbegin();
        if (!m_ParamValues[argIndex].empty())
        {
            if (strcmp(lastValue->c_str(), "") != 0)
                return (lastValue->c_str());
        }

        return (pDefault);
    }

    return (pDefault);
}

//-----------------------------------------------------------------------------
//! \brief Retrieves the value for a unsigned argument from the parsed values.
//!        If the parameter was specified multiple times, only the last value
//!        is returned
//!
//! \param pParam : Pointer to the parameter to retrieve an unsigned argument
//! \param defVal : Default value for the unsigned argument
//!
//! \return Value for the parameter (if specified), if the parameter was not
//!         specified then the default value is returned
//!
UINT32 ArgReader::ParamUnsigned(const char *pParam,
                                UINT32      defVal /* = 0*/) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    if (ParamValid(pParamDecl, pParam, true))
    {
        const char* lastStr = NULL;

        auto lastValue = m_ParamValues[argIndex].rbegin();
        if (!m_ParamValues[argIndex].empty())
             lastStr = lastValue->c_str();

        if (lastStr && (strcmp(lastStr, "") == 0))
        {
            Printf(Tee::PriHigh, "Parameter '%s's first argument is empty!\n",
                   pParam);
            return (defVal);
        }
        else if (pParamDecl->pArgTypes[0] == 'u')
            return (lastStr ? strtoul(lastStr, 0, 0) : defVal);
        else if (pParamDecl->pArgTypes[0] == 'x')
            return (lastStr ? strtoul(lastStr, 0, 16) : defVal);
        else
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s's first argument isn't unsigned (%c)!\n",
                   pParam, pParamDecl->pArgTypes[0]);
            return (defVal);
        }
    }

    return (defVal);
}

//-----------------------------------------------------------------------------
//! \brief Retrieves the value for a 64-bit unsigned argument from the parsed values.
//!        If the parameter was specified multiple times, only the last value
//!        is returned
//!
//! \param pParam : Pointer to the parameter to retrieve an unsigned argument
//! \param defVal : Default value for the 64-bit unsigned argument
//!
//! \return Value for the parameter (if specified), if the parameter was not
//!         specified then the default value is returned
//!
UINT64 ArgReader::ParamUnsigned64(const char *pParam,
                                UINT64      defVal /* = 0*/) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    if (ParamValid(pParamDecl, pParam, true))
    {
        const char* lastStr = NULL;

        auto lastValue = m_ParamValues[argIndex].rbegin();
        if (!m_ParamValues[argIndex].empty())
             lastStr = lastValue->c_str();

        if (lastStr && (strcmp(lastStr, "") == 0))
        {
            Printf(Tee::PriHigh, "Parameter '%s's first argument is empty!\n",
                   pParam);
            return (defVal);
        }
        else if (pParamDecl->pArgTypes[0] == 'L')
            return (lastStr ? Utility::Strtoull(lastStr, 0, 0) : defVal);
        else
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s's first argument isn't unsigned long (%c)!\n",
                   pParam, pParamDecl->pArgTypes[0]);
            return (defVal);
        }
    }

    return (defVal);
}

//-----------------------------------------------------------------------------
//! \brief Retrieves the value for a signed argument from the parsed values.
//!        If the parameter was specified multiple times, only the last value
//!        is returned
//!
//! \param pParam : Pointer to the parameter to retrieve a signed argument
//! \param defVal : Default value for the signed argument
//!
//! \return Value for the parameter (if specified), if the parameter was not
//!         specified then the default value is returned
//!
INT32 ArgReader::ParamSigned(const char *pParam, INT32 defVal /* = 0*/) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    if (ParamValid(pParamDecl, pParam, true))
    {
        const char* lastStr = NULL;

        auto lastValue = m_ParamValues[argIndex].rbegin();
        if (!m_ParamValues[argIndex].empty())
             lastStr = lastValue->c_str();

        if (lastStr && (strcmp(lastStr, "") == 0))
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s's first argument is empty!\n",
                   pParam);
            return (defVal);
        }
        else if (pParamDecl->pArgTypes[0] == 's')
            return (lastStr ? strtol(lastStr, 0, 0) : defVal);
        else
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s's first argument isn't signed (%c)!\n",
                   pParam, pParamDecl->pArgTypes[0]);
            return (defVal);
        }
    }

    return (defVal);
}

//-----------------------------------------------------------------------------
//! \brief Retrieves the value for a float argument from the parsed values.
//!        If the parameter was specified multiple times, only the last value
//!        is returned
//!
//! \param pParam : Pointer to the parameter to retrieve a float argument
//! \param defVal : Default value for the float argument
//!
//! \return Value for the parameter (if specified), if the parameter was not
//!         specified then the default value is returned
//!
FLOAT64 ArgReader::ParamFloat(const char *pParam, FLOAT64 defVal /* = 0.0*/) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    if (ParamValid(pParamDecl, pParam, true))
    {
        const char* lastStr = NULL;

        auto lastValue = m_ParamValues[argIndex].rbegin();
        if (!m_ParamValues[argIndex].empty())
             lastStr = lastValue->c_str();

        if (lastStr && (strcmp(lastStr, "") == 0))
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s's first argument is empty!\n",
                   pParam);
            return (defVal);
        }
        else if (pParamDecl->pArgTypes[0] == 'f')
            return (lastStr ? strtod(lastStr, 0) : defVal);
        else
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s's first argument isn't real (%c)!\n",
                   pParam, pParamDecl->pArgTypes[0]);
            return (defVal);
        }
    }

    return (defVal);
}

//-----------------------------------------------------------------------------
//! \brief Retrieves the the list of strings associated with a parameter.
//!        If the parameter was specified multiple times, it will return all of
//!        the values that were specified as seperate entried in the returned
//!        vector
//!
//! \param pParam   : Pointer to the parameter to retrieve the vector of
//!                   strings
//! \param pDefault : Default value for the string vector of values
//!
//! \return All values for the parameter (if specified), if the parameter was
//!         not specified then the default value is returned
//!
const vector<string>* ArgReader::ParamStrList
(
    const char     *pParam,
    vector<string> *pDefault /* = NULL */
) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    if (ParamValid(pParamDecl, pParam, true))
    {
        if ((pParamDecl->pArgTypes[0] == 't') ||
            (pParamDecl->pArgTypes[0] == 'b'))
            return &m_ParamValues[argIndex];
        else
        {
            Printf(Tee::PriHigh, "Parameter '%s's is not a string argument (%c)!\n",
                pParam, pParamDecl->pArgTypes[0]);
            return (pDefault);
        }
    }

    return (pDefault);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the smallest number of arguments that were provided to a
//!        particular parameter.
//!
//! \param pParam   : Pointer to the parameter to retrieve the number of args
//!                   specified
//!
//! \return The minimum number of arguments (across all instances of the
//!         parameter) that were supplied
//!
UINT32 ArgReader::ParamNArgs(const char *pParam) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex, paramIndex, count;
    UINT32           maxArgs = 0;
    UINT32           maxParamInst = 0;

    // see how many of this option we have on the command line
    pParamDecl = FindParam(m_pParamList, pParam, &paramIndex, &argIndex, 0, 0);
    if (ParamValid(pParamDecl, pParam, false) &&
        m_UsageCounts[paramIndex] &&
        pParamDecl->pArgTypes)
    {
        maxParamInst = m_UsageCounts[paramIndex];
        maxArgs = (UINT32)strlen(pParamDecl->pArgTypes);

        UINT32* counts = new UINT32[maxParamInst];
        UINT32  lwrArg, lwrParamInst;

        memset(counts, 0, maxParamInst * sizeof(UINT32));

        // Construct the counts array so that each entry in the counts
        // array represents a particular parameter specfication ie.:
        //
        //     -param arg1 ... argN
        //
        // Entry 0 in the counts array contains the number of arguments
        // that were specified the first time the parameter was encountered
        // Entry 1 contains the number of arguments the second time it was
        // encountered, etc.
        for (lwrArg = 0; lwrArg < maxArgs; lwrArg++)
        {
            auto options = m_ParamValues[argIndex + lwrArg].begin();
            lwrParamInst = 0;
            while (options != m_ParamValues[argIndex + lwrArg].cend())
            {
                if (strcmp(options->c_str(), "") != 0)
                    counts[lwrParamInst]++;
                options++;
                lwrParamInst++;
            }
        }

        // Hopefully the number of arguments will be the same every time
        // that the parameter was used, if not, because of the way the
        // database is parsed, the last entry in the counts array will
        // always contain the least number of arguments that were passed
        // to a particular parameter, return that value here
        count = counts[maxParamInst - 1];
        delete [] counts;
        return (count);
    }

    return (0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the number of arguments that were provided to a particular
//!        instance of a parameter.
//!
//! \param pParam    : Pointer to the parameter to retrieve the number of args
//!                    specified
//! \param paramInst : The instance number of the parameter to retrieve the
//!                    number of args for (instance numbers begin at 0)
//!
//! \return The number of arguments provided to the particular parameter
//!         instance
//!
UINT32 ArgReader::ParamNArgs(const char *pParam, UINT32 paramInst) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex, paramIndex, count;
    UINT32           maxArgs = 0;

    // see how many of this option we have on the command line
    pParamDecl = FindParam(m_pParamList, pParam, &paramIndex, &argIndex, 0, 0);
    if (ParamValid(pParamDecl, pParam, false) &&
        m_UsageCounts[paramIndex] &&
        pParamDecl->pArgTypes)
    {
        UINT32  lwrArg, lwrParamInst;

        maxArgs = (UINT32)strlen(pParamDecl->pArgTypes);

        count = 0;
        for(lwrArg = 0; lwrArg < maxArgs; lwrArg++)
        {
            auto options = m_ParamValues[argIndex + lwrArg].begin();
            lwrParamInst = 0;
            while (options != m_ParamValues[argIndex + lwrArg].end())
            {
                if ((lwrParamInst == paramInst) &&
                    (strcmp(options->c_str(), "") != 0))
                {
                    count++;
                }
                options++;
                lwrParamInst++;
            }
        }

        return (count);
    }

    return (0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the string argument at the specified argument number to the
//!        specified parameter. If the parameter was specified multiple times,
//!        only the value for the last instance of the parameter is returned
//!
//! \param pParam : Pointer to the parameter to retrieve the string argument
//! \param argNum : The argument number to retrieve the string argument for
//!                 All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then NULL is returned
//!
const char *ArgReader::ParamNStr(const char *pParam, UINT32 argNum) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        const char* lastStr = NULL;

        auto lastValue = m_ParamValues[argIndex].rbegin();

        if (!m_ParamValues[argIndex].empty())
            lastStr = lastValue->c_str();

        // just in case args was filled as blank for tracking
        if (lastStr == NULL || strcmp(lastStr, "") == 0)
        {
            Printf(Tee::PriHigh, "Parameter '%s' argument %d is empty (%c)!\n",
                   pParam, argNum, pParamDecl->pArgTypes[argNum]);
            return NULL;
        }
        else
            return (lastStr);
    }

    return NULL;
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the string argument at the specified argument number from a
//!        particular instance of a parameter.
//!
//! \param pParam    : Pointer to the parameter to retrieve the string argument
//! \param paramInst : The instance of the parameter to retrieve the argument.
//!                    All parameter instances start at 0.
//! \param argNum    : The argument number to retrieve the string argument for
//!                    All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then NULL is returned
//!
const char *ArgReader::ParamNStr(const char *pParam,
                                 UINT32      paramInst,
                                 UINT32      argNum) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        auto options = m_ParamValues[argIndex].begin();
        UINT32 lwrParamInst = 0;

        while (options != m_ParamValues[argIndex].cend())
        {
            if (lwrParamInst == paramInst)
            {
                if (strcmp(options->c_str(), "") == 0)
                {
                    Printf(Tee::PriHigh,
                           "Parameter '%s' #%d argument %d is empty!\n",
                           pParam, paramInst, argNum);
                    return NULL;
                }
                else
                    return (options->c_str());
            }
            options++;
            lwrParamInst++;
        }

        // in case they asked for an option that we don't have
        Printf(Tee::PriHigh, "Parameter '%s' #%d does not have %d arguments\n",
               pParam, paramInst, argNum);
    }

    return NULL;
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the unsigned argument at the specified argument number to
//!        the specified parameter. If the parameter was specified multiple
//!        times, only the value for the last instance of the parameter is
//!        returned
//!
//! \param pParam : Pointer to the parameter to retrieve the unsigned argument
//! \param argNum : The argument number to retrieve the unsigned argument for
//!                 All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then 0 is returned
//!
UINT32 ArgReader::ParamNUnsigned(const char *pParam, UINT32 argNum) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        const char* lastStr = NULL;

        auto lastValue = m_ParamValues[argIndex].rbegin();

        if (!m_ParamValues[argIndex].empty())
            lastStr = lastValue->c_str();

        // just in case args was filled as blank for tracking
        if (lastStr == NULL || strcmp(lastStr, "") == 0)
        {
            Printf(Tee::PriHigh, "Parameter '%s' argument %d is empty!\n",
                   pParam, argNum);
            return (0);
        }
        else if (pParamDecl->pArgTypes[argNum] == 'u')
            return (strtoul(lastStr, 0, 0));
        else if (pParamDecl->pArgTypes[argNum] == 'x')
            return (strtoul(lastStr, 0, 16));
        else
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s' argument %d isn't real (%c) != (u,x)!\n",
                   pParam, argNum, pParamDecl->pArgTypes[argNum]);
            return (0);
        }
    }

    return (0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the unsigned argument at the specified argument number from
//!        a particular instance of a parameter.
//!
//! \param pParam    : Pointer to the parameter to retrieve the unsigned
//!                    argument
//! \param paramInst : The instance of the parameter to retrieve the argument.
//!                    All parameter instances start at 0.
//! \param argNum    : The argument number to retrieve the unsigned argument.
//!                    All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then 0 is returned
//!
UINT32 ArgReader::ParamNUnsigned(const char *pParam,
                                 UINT32      paramInst,
                                 UINT32      argNum) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        auto options = m_ParamValues[argIndex].cbegin();
        UINT32 lwrParamInst = 0;

        while (options != m_ParamValues[argIndex].end())
        {
            if (lwrParamInst == paramInst)
            {
                if (strcmp(options->c_str(), "") == 0)
                {
                    Printf(Tee::PriHigh,
                           "Parameter '%s' #%d argument %d is empty!\n",
                           pParam, paramInst, argNum);
                    return (0);
                }
                else
                {
                    if (pParamDecl->pArgTypes[argNum] == 'u')
                        return (strtoul(options->c_str(), 0, 0));
                    else if (pParamDecl->pArgTypes[argNum] == 'x')
                        return (strtoul(options->c_str(), 0, 16));
                    else
                    {
                        Printf(Tee::PriHigh,
                               "Parameter '%s' #%d argument %d isn't real (%c) != (u,x)!\n",
                               pParam, paramInst, argNum, pParamDecl->pArgTypes[argNum]);
                        return(0);
                    }
                }
            }
            options++;
            lwrParamInst++;
        }

        // in case they asked for an option that we don't have
        Printf(Tee::PriHigh, "Parameter '%s' #%d does not have %d arguments\n",
               pParam, paramInst, argNum);
    }

    return (0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the unsigned argument at the specified argument number to
//!        the specified parameter. If the parameter was specified multiple
//!        times, only the value for the last instance of the parameter is
//!        returned
//!
//! \param pParam : Pointer to the parameter to retrieve the unsigned argument
//! \param argNum : The argument number to retrieve the unsigned argument for
//!                 All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then 0 is returned
//!
UINT64 ArgReader::ParamNUnsigned64
(
    const char *pParam,
    UINT32      argNum
) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        const char* lastStr = NULL;

        auto lastValue = m_ParamValues[argIndex].rbegin();

        if (!m_ParamValues[argIndex].empty())
            lastStr = lastValue->c_str();

        // just in case args was filled as blank for tracking
        if (lastStr == NULL || strcmp(lastStr, "") == 0)
        {
            Printf(Tee::PriHigh, "Parameter '%s' argument %d is empty!\n",
                   pParam, argNum);
            return (0);
        }
        else if (pParamDecl->pArgTypes[argNum] == 'L')
            return (Utility::Strtoull(lastStr, 0, 0));
        else
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s' argument %d isn't real (%c) != (u,x)!\n",
                   pParam, argNum, pParamDecl->pArgTypes[argNum]);
            return (0);
        }
    }

    return (0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the unsigned argument at the specified argument number from
//!        a particular instance of a parameter.
//!
//! \param pParam    : Pointer to the parameter to retrieve the unsigned
//!                    argument
//! \param paramInst : The instance of the parameter to retrieve the argument.
//!                    All parameter instances start at 0.
//! \param argNum    : The argument number to retrieve the unsigned argument.
//!                    All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then 0 is returned
//!
UINT64 ArgReader::ParamNUnsigned64
(
    const char *pParam,
    UINT32      paramInst,
    UINT32      argNum
) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        auto options = m_ParamValues[argIndex].begin();
        UINT32 lwrParamInst = 0;

        while (options != m_ParamValues[argIndex].end())
        {
            if (lwrParamInst == paramInst)
            {
                if (strcmp(options->c_str(), "") == 0)
                {
                    Printf(Tee::PriHigh,
                           "Parameter '%s' #%d argument %d is empty!\n",
                           pParam, paramInst, argNum);
                    return (0);
                }
                else
                {
                    if (pParamDecl->pArgTypes[argNum] == 'L')
                        return (Utility::Strtoull(options->c_str(), 0, 0));
                    else
                    {
                        Printf(Tee::PriHigh,
                               "Parameter '%s' #%d argument %d isn't real (%c) != (u,x)!\n",
                               pParam, paramInst, argNum, pParamDecl->pArgTypes[argNum]);
                        return(0);
                    }
                }
            }
            options++;
            lwrParamInst++;
        }

        // in case they asked for an option that we don't have
        Printf(Tee::PriHigh, "Parameter '%s' #%d does not have %d arguments\n",
               pParam, paramInst, argNum);
    }

    return (0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the signed argument at the specified argument number to
//!        the specified parameter. If the parameter was specified multiple
//!        times, only the value for the last instance of the parameter is
//!        returned
//!
//! \param pParam : Pointer to the parameter to retrieve the signed argument
//! \param argNum : The argument number to retrieve the signed argument for
//!                 All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then 0 is returned
//!
INT32 ArgReader::ParamNSigned(const char *pParam, UINT32 argNum) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        const char* lastStr = NULL;

        auto lastValue = m_ParamValues[argIndex].rbegin();

        if (!m_ParamValues[argIndex].empty())
            lastStr = lastValue->c_str();

        // just in case args was filled as blank for tracking
        if (lastStr == NULL || strcmp(lastStr, "") == 0)
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s' argument %d is empty (%c)!\n",
                   pParam, argNum, pParamDecl->pArgTypes[argNum]);
            return (0);
        }
        else if (pParamDecl->pArgTypes[argNum] == 's')
            return (strtol(lastStr, 0, 0));
        else
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s' argument %d isn't real (%c) != (s)!\n",
                   pParam, argNum, pParamDecl->pArgTypes[argNum]);
            return (0);
        }
    }

    return (0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the signed argument at the specified argument number from
//!        a particular instance of a parameter.
//!
//! \param pParam    : Pointer to the parameter to retrieve the signed
//!                    argument
//! \param paramInst : The instance of the parameter to retrieve the argument.
//!                    All parameter instances start at 0.
//! \param argNum    : The argument number to retrieve the signed argument.
//!                    All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then 0 is returned
//!
INT32 ArgReader::ParamNSigned(const char *pParam,
                              UINT32      paramInst,
                              UINT32      argNum) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        auto options = m_ParamValues[argIndex].begin();
        UINT32 lwrParamInst = 0;

        while (options != m_ParamValues[argIndex].end())
        {
            if (lwrParamInst == paramInst)
            {
                if (strcmp(options->c_str(), "") == 0)
                {
                    Printf(Tee::PriHigh,
                           "Parameter '%s' #%d argument %d is empty!\n",
                           pParam, paramInst, argNum);
                    return (0);
                }
                else
                {
                    if (pParamDecl->pArgTypes[argNum] == 's')
                        return (strtol(options->c_str(), 0, 0));
                    else
                    {
                        Printf(Tee::PriHigh,
                               "Parameter '%s' #%d argument %d isn't real (%c) != (s)!\n",
                               pParam, paramInst, argNum, pParamDecl->pArgTypes[argNum]);
                        return (0);
                    }
                }
            }
            options++;
            lwrParamInst++;
        }

        // in case they asked for an option that we don't have
        Printf(Tee::PriHigh, "Parameter '%s' #%d does not have %d arguments\n",
               pParam, paramInst, argNum);
    }

    return(0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the float argument at the specified argument number to
//!        the specified parameter. If the parameter was specified multiple
//!        times, only the value for the last instance of the parameter is
//!        returned
//!
//! \param pParam : Pointer to the parameter to retrieve the float argument
//! \param argNum : The argument number to retrieve the float argument for
//!                 All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then 0 is returned
//!
FLOAT64 ArgReader::ParamNFloat(const char *pParam, UINT32 argNum) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        const char* lastStr = NULL;

        auto lastValue = m_ParamValues[argIndex].rbegin();

        if (!m_ParamValues[argIndex].empty())
            lastStr = lastValue->c_str();

        // just in case args was filled as blank for tracking
        if (lastStr == NULL || strcmp(lastStr, "") == 0)
        {
            Printf(Tee::PriHigh, "Parameter '%s's %d argument is empty!\n",
                   pParam, argNum);
            return(0);
        }
        else if (pParamDecl->pArgTypes[argNum] == 'f')
            return (strtod(lastStr, 0));
        else
        {
            Printf(Tee::PriHigh,
                   "Parameter '%s' argument %d isn't real (%c) != (f)!\n",
                   pParam, argNum, pParamDecl->pArgTypes[argNum]);
            return(0);
        }
    }

    return(0);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the float argument at the specified argument number from
//!        a particular instance of a parameter.
//!
//! \param pParam    : Pointer to the parameter to retrieve the float
//!                    argument
//! \param paramInst : The instance of the parameter to retrieve the argument.
//!                    All parameter instances start at 0.
//! \param argNum    : The argument number to retrieve the float argument.
//!                    All argument numbers start at 0.
//!
//! \return Value of the argument (if specified), if the argument or parameter
//!         was not specified then 0 is returned
//!
FLOAT64 ArgReader::ParamNFloat(const char *pParam,
                               UINT32      paramInst,
                               UINT32      argNum) const
{
    const ParamDecl *pParamDecl;
    UINT32           argIndex = 0;

    pParamDecl = FindParam(m_pParamList, pParam, 0, &argIndex, 0, 0);
    argIndex += argNum;
    if (ParamLWalid(pParamDecl, pParam, argNum, argIndex))
    {
        auto options = m_ParamValues[argIndex].begin();
        UINT32 lwrParamInst = 0;

        while (options != m_ParamValues[argIndex].end())
        {
            if (lwrParamInst == paramInst)
            {
                if (strcmp(options->c_str(), "") == 0)
                {
                    Printf(Tee::PriHigh,
                           "Parameter '%s' #%d argument %d is empty!\n",
                           pParam, paramInst, argNum);
                    return (0);
                }
                else
                {
                    if (pParamDecl->pArgTypes[argNum] == 'f')
                        return (strtod(options->c_str(), 0));
                    else
                    {
                        Printf(Tee::PriHigh,
                               "Parameter '%s' #%d argument %d isn't real (%c) != (f)!\n",
                               pParam, paramInst, argNum, pParamDecl->pArgTypes[argNum]);
                        return(0);
                    }
                }
            }
            options++;
            lwrParamInst++;
        }

        // in case they asked for an option that we don't have
        Printf(Tee::PriHigh, "Parameter '%s' #%d does not have %d arguments\n",
               pParam, paramInst, argNum);
    }

    return(0);
}

//-----------------------------------------------------------------------------
//! \brief Prints out args that were recognized.
//!
void ArgReader::PrintParsedArgs(Tee::Priority pri) const
{
    bool   bFirstArg   = true;
    UINT32 usageIndex  = 0;
    UINT32 valuesIndex = 0;

    PrintParsedArgsRelwrsion(pri, m_pParamList, &usageIndex, &valuesIndex, &bFirstArg);
}

//-----------------------------------------------------------------------------
//! \brief Describes the parameters in the parameter list.
//!
//! \param pParamList        : Pointer to the parameter list to describe
//! \param bPrintUndescribed : Print parameters without a description (default = true)
//!
/* static */ void ArgReader::DescribeParams
(
    const ParamDecl *pParamList
   ,bool bPrintUndescribed
)
{
    char             buffer[1024];
    const ParamDecl *pGroupStart;
    const char      *pType;

    pGroupStart = 0;
    while (pParamList->pParam)
    {
        if (pParamList->flags & ParamDecl::PARAM_SUBDECL_PTR)
        {
            // print sublist
            DescribeParams((const ParamDecl *)(pParamList->pParam));
        }
        else
        {
            strcpy(buffer, pParamList->pParam);

            if (pParamList->flags & ParamDecl::GROUP_START)
                pGroupStart = pParamList;

            if (pParamList->flags & ParamDecl::ALIAS_START)
            {
                ++pParamList;
                continue;
            }

            if (pParamList->flags & ParamDecl::GROUP_MEMBER)
            {
                if (!pGroupStart)
                {
                    MASSERT(!"processed GROUP_MEMBER before GROUP_START");
                    return;
                }

                if (strlen(buffer) < 10)
                {
                    strcat(buffer, "          ");
                    buffer[10] = 0;
                }
                if (pParamList->flags & ParamDecl::GROUP_MEMBER_UNSIGNED)
                {
                    // Need to differentiate between NULL and not on pArgTypes
                    // for group arguments since sprintf will actually print
                    // "(nil)" instead of "0x0" for NULL pointers
                    if (pParamList->pArgTypes == NULL)
                    {
                        sprintf(buffer + strlen(buffer), " (== %s 0x0)",
                               pGroupStart->pParam);
                    }
                    else
                    {
                        sprintf(buffer + strlen(buffer), " (== %s %p)",
                               pGroupStart->pParam, pParamList->pArgTypes);
                    }
                }
                else
                    sprintf(buffer + strlen(buffer), " (== %s %s)",
                            pGroupStart->pParam, pParamList->pArgTypes);
            }
            else
            {
                if (pParamList->pArgTypes)
                {
                    for (pType = pParamList->pArgTypes; *pType; pType++)
                    {
                        switch (*pType)
                        {
                        case 't': strcat(buffer, " <string>");
                            break;

                        case 'b': strcat(buffer, " <{string}>");
                            break;

                        case 'u':
                            if (pParamList->flags & ParamDecl::PARAM_ENFORCE_RANGE)
                            {
                                sprintf(buffer + strlen(buffer), " <%u - %u>",
                                    pParamList->rangeMin, pParamList->rangeMax);
                            }
                            else
                                strcat(buffer, " <unsigned>");
                            break;

                        case 'L':
                            if (pParamList->flags & ParamDecl::PARAM_ENFORCE_RANGE)
                            {
                                sprintf(buffer + strlen(buffer), " <%u - %u>",
                                    pParamList->rangeMin, pParamList->rangeMax);
                            }
                            else
                                strcat(buffer, " <unsigned long long>");
                            break;

                        case 's':
                            if (pParamList->flags & ParamDecl::PARAM_ENFORCE_RANGE)
                            {
                                sprintf(buffer + strlen(buffer), " <%d - %d>",
                                    pParamList->rangeMin, pParamList->rangeMax);
                            }
                            else
                                strcat(buffer, " <signed>");
                            break;

                        case 'x':
                            if (pParamList->flags & ParamDecl::PARAM_ENFORCE_RANGE)
                            {
                                sprintf(buffer + strlen(buffer), " <0x%x - 0x%x>",
                                    pParamList->rangeMin, pParamList->rangeMax);
                            }
                            else
                                strcat(buffer, " <hex>");
                            break;

                        case 'f':
                            if (pParamList->flags & ParamDecl::PARAM_ENFORCE_RANGE)
                            {
                                sprintf(buffer + strlen(buffer), " <%d.0 - %d.0>",
                                    pParamList->rangeMin, pParamList->rangeMax);
                            }
                            else
                                strcat(buffer, " <float>");
                            break;

                        default:
                            strcat(buffer, " <?unknown?>");
                            break;
                        }
                    }
                }
            }

            // Treat undescribed parameters as hidden
            if (bPrintUndescribed ||
                (pParamList->pDescription && pParamList->pDescription[0]))
            {
                Printf(Tee::PriNormal, "  %-35s %s %s%s\n", buffer,
                    ((pParamList->flags & ParamDecl::GROUP_MEMBER) ? " +" : ":"),
                    ((pParamList->pDescription && pParamList->pDescription[0]) ?
                          pParamList->pDescription :
                          "NO PARAMETER DESCRIPTION PROVIDED!  BAD!"),
                    ((pParamList->flags & ParamDecl::PARAM_REQUIRED) ?
                          " (REQUIRED)" : ""));
            }
        }

        pParamList++;
    }
}

//-----------------------------------------------------------------------------
//! \brief Given a parameter string '<param name>' and its value vector, check
//!        if this parameter exists and matches the values in the command line
//!
//! \param param_name : String describing the parameter name to check for a match
//! \param param_values : Vector of string describing the parameter values
//!
//! \return  -1  (parameter does not exist)
//!           0  (at lease 1 value does not match <param_val>)
//!           1  (value matches <param_val>)
INT32 ArgReader::MatchParam( const string& paramName, vector<string> paramValues ) const
{
    UINT32  paramNum, valueStart;
    UINT32 groupIdx;
    const ParamDecl* pGroupStart = NULL;
    const ParamDecl* pAliasStart = NULL;
    const ParamDecl* pLwrParam = FindParam(m_pParamList, paramName.c_str(),
                                           &paramNum, &valueStart,
                                           &pGroupStart, &pAliasStart,
                                           &groupIdx);

    if (!pLwrParam)
        return -1;              // not found

    if (m_UsageCounts[paramNum] == 0) // Parameter not present
        return 0;

    string paramVal;
    if (!paramValues.empty())
    {
        paramVal = paramValues[0];
    }

    if ((pLwrParam->flags & ParamDecl::GROUP_MEMBER) && pGroupStart)
    {
        if (paramVal.empty())
        {
            if (pLwrParam->flags & ParamDecl::GROUP_MEMBER_UNSIGNED)
            {
                paramVal = Utility::StrPrintf("%p", pLwrParam->pArgTypes);
                if (paramVal.substr(0,2) != "0x")
                    paramVal.insert(0, "0x");
            }
            else
                paramVal = pLwrParam->pArgTypes;

            paramValues.clear();
            paramValues.push_back(paramVal);
        }
        pLwrParam = pGroupStart;
    }
    else if ((pLwrParam->flags & ParamDecl::ALIAS_MEMBER) && pAliasStart)
    {
        pLwrParam = pAliasStart;
    }

    if (!pLwrParam->pArgTypes)
    {
        if (paramVal.empty())
        {
            return 1;
        }
        else
        {
            Printf(Tee::PriHigh, "Parameter %s does not take arguments\n", paramName.c_str());
            return 0;
        }
    }

    const vector<string>::size_type size = paramValues.size();
    vector<string>::size_type index = 0;

    while (index < size)
    {
        //Remove quotes in the value
        string value = paramValues[index];
        if( (value[0] == '"') && (*(value.rbegin()) == '"') )
        {
            value.erase( 0, 1 );
            value.erase( value.size()-1, 1 );
        }

        const vector<string>& paramVec = m_ParamValues[valueStart + static_cast<UINT32>(index)];
        auto itor = std::find(paramVec.begin(), paramVec.end(), value);

        if (itor == paramVec.end())
            break;
        ++index;
    }

    // All values for paramName match with those in cmdline.
    if (index == size)
        return 1;

    // One more try: do the value comparison
    switch (*(pLwrParam->pArgTypes))
    {
        case 'u':
        case 'x':
        {
            UINT32 cmdVal = ParamUnsigned(pLwrParam->pParam);
            return ((strtoul(paramVal.c_str(), 0, 0) == cmdVal) ? 1 : 0);
        }

        case 's':
        {
            INT32 cmdVal = ParamSigned(pLwrParam->pParam);
            return ((strtol(paramVal.c_str(), 0, 0) == cmdVal) ? 1 : 0);
        }

        case 'f':
            Printf(Tee::PriDebug, "Floating type argument is not supported by ECov checking\n");
            return 0;

        default:
            Printf(Tee::PriDebug, "Unknown type for parameter %s\n", paramName.c_str());
            return 0;
    }
}

//-----------------------------------------------------------------------------
void ArgReader::Reset()
{
    m_UsageCounts.assign(m_UsageCounts.size(), 0);
    m_ParamScoped.assign(m_ParamScoped.size(), false);
    // An arg reader object for mods arguments is allocated in OneTimeInit
    // since it must be available early.  However additional memory is allocated
    // in the object after leak checking starts that must be deleted prior to
    // checking for leaks on exit.  The entire object cannot be deleted without
    // creating negative leaks so this function needs to reset the state of the
    // object to how it was after constructing and before parsing.
    //
    // Simply clearing a vector or string will not change the actual allocation
    // capacity.  To accomplish that it is necessary to swap with an empty
    // entity 
    for (auto & lwrParamVals : m_ParamValues)
    {
        vector<string>().swap(lwrParamVals);
    }
    string().swap(m_FirstUnknownArg);

    while (m_pArgDb)
    {
        m_pArgDb->m_RefCount--;
        m_pArgDb = m_pArgDb->m_pParentDb;
    }
}

// ****************************************************************************
// Private functions
// ****************************************************************************

//-----------------------------------------------------------------------------
//! \brief Relwrsively parse arguments from the database (ParseArgs doesn't
//!        quite have tail relwrsion)
//!
//! \param pArgDb : ArgDatabase to parse arguments from
//! \param pScope : If not NULL, the scope to parse the arguments for, if NULL
//!                 then only arguments from the global scope are parsed
//! \param bIncludeGlobalScope   : If pScope is not NULL, then this argument
//!                                specifies whether to include global arguments
//!                                or only arguments within the specified scope
//!                                when parsing
//!
//! \return  1 if successful, 0 if failure
UINT32 ArgReader::ParseArgsRelwrsion(ArgDatabase *pArgDb,
                                     const char  *pScope,
                                     bool         bIncludeGlobalScope)
{
    struct ArgDatabaseEntry *pDbEntry;
    UINT32 paramIndex, argIndex;
    const ParamDecl *pParamDecl, *pGroupStart, *pAliasStart;
    const char *pArgTypes;
    UINT32 result;
    bool bScoped;

    // get us back to our real class from this abstract pointer
    ArgDatabase *pRealArgDb = reinterpret_cast<ArgDatabase *>(pArgDb);

    // if the given arg database is chained, do the parent first (relwrsively)
    if (pRealArgDb->m_pParentDb)
    {
        if (!ParseArgsRelwrsion(pRealArgDb->m_pParentDb,
                                pScope, bIncludeGlobalScope))
            return (0);
    }

    m_pArgDb = pArgDb;
    pRealArgDb->m_RefCount++;

    result = 1;
    pDbEntry = pRealArgDb->m_pHead;
    while (pDbEntry)
    {
        bScoped = false;

        if (pScope && pDbEntry->pScope)
            bScoped = !strcmp(pScope, pDbEntry->pScope);

        if (!bScoped && (pDbEntry->pScope || !bIncludeGlobalScope))
        {
            // Skip arguments that are not in our scope
            pDbEntry = pDbEntry->pNext;
            continue;
        }

        pParamDecl = FindParam(m_pParamList, pDbEntry->pArg, &paramIndex,
                               &argIndex, &pGroupStart, &pAliasStart);
        if (pParamDecl)
        {

            pDbEntry->usageCount++;

            if (m_ParamScoped[paramIndex] && !bScoped)
            {
                pDbEntry = pDbEntry->pNext;
                continue;
            }

            const bool bGroupTestEscape = (pParamDecl->flags & ParamDecl::GROUP_MEMBER) &&
                                          (pGroupStart->flags & ParamDecl::PARAM_TEST_ESCAPE);
            const bool bAliasTestEscape = (pParamDecl->flags & ParamDecl::ALIAS_MEMBER) &&
                                          (pAliasStart->flags & ParamDecl::PARAM_TEST_ESCAPE);
            const bool bParamTestEscape = (pParamDecl->flags & ParamDecl::PARAM_TEST_ESCAPE);

            if (bParamTestEscape || bGroupTestEscape || bAliasTestEscape)
            {
                pDbEntry->flags |= ArgDatabase::ARG_TEST_ESCAPE;
            }

            // If we are here the parameter will be handled and processed in
            // the current scope.  Only allow each version of the parameter ot
            // be processed once if so flagged
            if (pParamDecl->flags & ParamDecl::PARAM_PROCESS_ONCE)
            {
                Printf(Tee::PriLow,
                       "Parameter %s is flagged as PARAM_PROCESS_ONCE, "
                       "removing it from the database!\n",
                       pParamDecl->pParam);
                ArgDatabaseEntry *pNextDbEntry = pDbEntry->pNext;
                if (OK != pRealArgDb->RemoveEntry(pDbEntry))
                {
                    Printf(Tee::PriHigh,
                           "Removing parameter %s from the database failed, "
                           "exiting!\n",
                           pParamDecl->pParam);
                }
                pDbEntry = pNextDbEntry;
            }
            else
            {
                pDbEntry = pDbEntry->pNext;
            }

            if (bScoped &&
                !m_ParamScoped[paramIndex] &&
                m_UsageCounts[paramIndex])
            {
                m_UsageCounts[paramIndex] = 0;
                for (int i = 0; i < (int)strlen(pParamDecl->pArgTypes); i++)
                    m_ParamValues[argIndex + i].clear();
            }

            // remember that we saw this arg
            m_UsageCounts[paramIndex]++;
            m_ParamScoped[paramIndex] = bScoped;

            if (pParamDecl->flags & ParamDecl::GROUP_MEMBER)
            {
                // groups are handled differently

                // check for arg duplication, it is never legal for group or
                // alias members to occur more than once, allow parsing to
                // continue so that all duplicated arguments are reported
                if (!(pGroupStart->flags & ParamDecl::GROUP_OVERRIDE_OK) &&
                    (m_UsageCounts[paramIndex] > 1))
                {
                    Printf(Tee::PriHigh,
                           "Multiple oclwrrences of parameters modifying '%s'"
                           "without GROUP_OVERRIDE_OK, will process last "
                           "oclwrence only!\n",
                           pGroupStart->pParam);
                }

                // decl's 'argtypes' has value to use for group, see if it is a
                // string or an unsigned value
                if (pParamDecl->flags & ParamDecl::GROUP_MEMBER_UNSIGNED)
                {
                    string newVal = "0x0";

                    // Need to differentiate between NULL and not on pArgTypes
                    // for group arguments since sprintf will actually print
                    // "(nil)" instead of "0x0" for NULL pointers, in addition
                    // force the addition of "0x" at the start if it is not
                    // there (windows platforms do not put "0x" at the start
                    // of a "%p" colwersion) so that when passed through
                    // strtoul the value is not interpreted as octal
                    if (pParamDecl->pArgTypes)
                    {
                        newVal = Utility::StrPrintf("%p", pParamDecl->pArgTypes);
                        if (newVal.substr(0,2) != "0x")
                            newVal.insert(0, "0x");
                    }

                    m_ParamValues[argIndex].push_back(newVal);
                }
                else
                    m_ParamValues[argIndex].push_back(string(pParamDecl->pArgTypes));
            }
            else
            {
                if (pParamDecl->flags & ParamDecl::ALIAS_MEMBER)
                {
                    // check for arg duplication, it is never legal for group or
                    // alias members to occur more than once, allow parsing to
                    // continue so that all duplicated arguments are reported
                    if (!(pAliasStart->flags & ParamDecl::ALIAS_OVERRIDE_OK) &&
                        (m_UsageCounts[paramIndex] > 1))
                    {
                        Printf(Tee::PriHigh,
                               "Multiple oclwrrences of parameters modifying '%s'"
                               "without ALIAS_OVERRIDE_OK, will process last "
                               "oclwrence only!\n",
                               pAliasStart->pParam);
                    }
                }
                else
                {
                    // check for invalid arg duplication, allow parsing to
                    // continue so that all duplicated arguments are reported
                    if (!(pParamDecl->flags & ParamDecl::PARAM_MULTI_OK) &&
                        (m_UsageCounts[paramIndex] == 2)) // only complain once
                    {
                        Printf(Tee::PriHigh,
                              "Multiple oclwrrences of '%s' without "
                              "PARAM_MULTI_OK, will process last oclwrence only "
                              "- check your parameter definition!\n",
                              pParamDecl->pParam);
                    }
                }

                // read any args
                if (pParamDecl->pArgTypes)
                {
                    pArgTypes = pParamDecl->pArgTypes;
                    while (*pArgTypes)
                    {
                        // make sure we haven't run out of args
                        if (!pDbEntry)
                        {
                            while (*pArgTypes)
                            {
                                m_ParamValues[argIndex++].push_back(string(""));
                                pArgTypes++;
                            }
                            // if it is OK to have fewer args specified, then
                            // quit processing this set
                            if (pParamDecl->flags & ParamDecl::PARAM_FEWER_OK)
                                break;

                            Printf(Tee::PriHigh,
                                   "Param '%s' requires %d arguments!\n",
                                   pParamDecl->pParam,
                                   (int)strlen(pParamDecl->pArgTypes));
                            return(0);
                        }

                        if (!CheckParamType(*pArgTypes, pDbEntry->pArg,
                            (pParamDecl->flags & ParamDecl::PARAM_ENFORCE_RANGE) ?
                                        true : false,
                                            pParamDecl->rangeMin,
                                            pParamDecl->rangeMax))
                        {
                            // if it is OK to have fewer args specified,
                            // then quit processing this set
                            if (pParamDecl->flags & ParamDecl::PARAM_FEWER_OK)
                            {
                                while (*pArgTypes)
                                {
                                    m_ParamValues[argIndex++].push_back(string(""));
                                    pArgTypes++;
                                }
                                break;
                            }

                            Printf(Tee::PriHigh,
                                   "Arg #%d ('%s') for param '%s' is not of type '%c'%s!\n",
                                   (int) (strlen(pParamDecl->pArgTypes) - strlen(pArgTypes)),
                                   pDbEntry->pArg, pParamDecl->pParam, *pArgTypes,
                                   ((pParamDecl->flags & ParamDecl::PARAM_ENFORCE_RANGE) ?
                                        " or is not in range" : ""));
                            result = 0; // don't return yet (parse all args), but remember error
                        }

                        m_ParamValues[argIndex++].push_back(string(pDbEntry->pArg));

                        // If the argument was part of a process once parameter,
                        // remove it from the database
                        if (pParamDecl->flags & ParamDecl::PARAM_PROCESS_ONCE)
                        {
                            Printf(Tee::PriLow,
                                   "Argument %s parsed as part of parameter %s "
                                   "flagged as PARAM_PROCESS_ONCE, removing %s "
                                   "from the database!\n",
                                   pDbEntry->pArg, pParamDecl->pParam,
                                   pDbEntry->pArg);
                            ArgDatabaseEntry *pNextDbEntry = pDbEntry->pNext;
                            const string argStr(pDbEntry->pArg);
                            if (OK != pRealArgDb->RemoveEntry(pDbEntry))
                            {
                                Printf(Tee::PriHigh,
                                       "Removing argument %s from the database "
                                       "failed, exiting!\n",
                                       argStr.c_str());
                            }
                            pDbEntry = pNextDbEntry;
                        }
                        else
                        {
                            pDbEntry->usageCount++;
                            pDbEntry = pDbEntry->pNext;
                        }

                        pArgTypes++;
                    }
                }
            }
        }
        else
        {
            // if this arg needed to be used, complain and fail
            if (pDbEntry->flags & ArgDatabase::ARG_MUST_USE)
            {
                Printf(Tee::PriHigh, "Unrecognized option '%s'\n", pDbEntry->pArg);
                return 0;
            }

            if (m_FirstUnknownArg.empty())
                m_FirstUnknownArg = pDbEntry->pArg;

            if (m_StopOnUnknownArg)
                return 1;

            // otherwise, just skip over it
            pDbEntry = pDbEntry->pNext;
        }
    }

    return (result);
}

//-----------------------------------------------------------------------------
//! \brief Finds the entry in the parameter list that matched the provided
//!        parameter.
//!
//! \param pParamList   : Pointer to the parameter list to search for the
//!                       parameter
//! \param pParam       : Pointer to the parameter to search for
//! \param pParamNum    : If not NULL, then if the parameter is found, the
//!                       index where the parameter is tracked in the
//!                       m_UsageCounts and m_ParamScoped arrays is returned.
//!                       If the parameter is not found, then the number of
//!                       parameters (inlwlding subdeclarations) is returned.
//! \param pValueStart  : If not NULL, then if the parameter is found, the
//!                       index where the parameter values are tracked in the
//!                       m_ParamValues vector array is returned.  If the
//!                       parameter is not found, then the number of values
//! \param ppGroupStart : If not NULL, then if the parameter is found and is
//!                       part of a parameter group, this will point to the
//!                       parameter declaration for the group the parameter is
//!                       part of
//! \param ppAliasStart : If not NULL, then if the parameter is found and is
//!                       part of a parameter alias, this will point to the
//!                       parameter declaration that the parameter is an alias
//!                       of
//! \param pGroupIdx    : If not NULL and the parameter is found and is part of
//!                       either a group or alias set, the this will be the
//!                       offset in the parameter array from the group or alias
//!                       start where the parameter was found
//!
//! \return Pointer to the matching parameter declaration if the parameter was
//!         found, NULL if parameter was not found
//!
/* static */ const ParamDecl *ArgReader::StaticFindParam
(
    const ParamDecl  *pParamList,
    const char       *pParam,
    UINT32           *pParamNum,
    UINT32           *pValueStart,
    const ParamDecl **ppGroupStart,
    const ParamDecl **ppAliasStart,
    UINT32           *pGroupIdx /* = NULL */
)
{
    UINT32 paramCount, argCount, subParamCount, subArgCount;
    const ParamDecl *pSubdecl;

    paramCount = 0;
    argCount = 0;
    while(pParamList->pParam)
    {
        if(pParamList->flags & ParamDecl::PARAM_SUBDECL_PTR)
        {
            // relwrse into subdecl list
            pSubdecl = StaticFindParam((const ParamDecl *)(pParamList->pParam),
                                 pParam,
                                 &subParamCount,
                                 &subArgCount,
                                 ppGroupStart,
                                 ppAliasStart);
            paramCount += subParamCount;
            argCount += subArgCount;

            if(pSubdecl)
            {
                if (pParamNum) *pParamNum = paramCount;
                if (pValueStart) *pValueStart = argCount;
                return (pSubdecl);
            }
        }
        else
        {
            if(ppGroupStart && (pParamList->flags & ParamDecl::GROUP_START))
            {
                *ppGroupStart = pParamList;
                if (pGroupIdx)
                    *pGroupIdx = 0;
            }

            if(ppAliasStart && (pParamList->flags & ParamDecl::ALIAS_START))
            {
                *ppAliasStart = pParamList;
                if (pGroupIdx)
                    *pGroupIdx = 0;
            }

            if(!strcmp(pParamList->pParam, pParam))
            {
                if(pParamNum)
                {
                    if (pParamList->flags & ParamDecl::GROUP_MEMBER)
                        *pParamNum = paramCount - 1;
                    else if (pParamList->flags & ParamDecl::ALIAS_MEMBER)
                        *pParamNum = paramCount - 1;
                    else
                        *pParamNum = paramCount;
                }

                if(pValueStart)
                {
                    if (pParamList->flags & ParamDecl::GROUP_MEMBER)
                        *pValueStart = argCount - 1;
                    else if (pParamList->flags & ParamDecl::ALIAS_MEMBER)
                        *pValueStart = argCount - 1;
                    else
                        *pValueStart = argCount;
                }

                return (pParamList);
            }

            if (!(pParamList->flags & ParamDecl::GROUP_MEMBER) &&
                !(pParamList->flags & ParamDecl::ALIAS_MEMBER))
            {
                paramCount++;
                if (pParamList->pArgTypes)
                    argCount += (UINT32)strlen(pParamList->pArgTypes);
            }
            else if (pGroupIdx)
            {
                (*pGroupIdx)++;
            }
        }

        pParamList++;
    }

    if (pParamNum) *pParamNum = paramCount;
    if (pValueStart) *pValueStart = argCount;

    return (0);
}

const ParamDecl *ArgReader::FindParam(const ParamDecl  *pParamList,
                                      const char       *pParam,
                                      UINT32           *pParamNum,
                                      UINT32           *pValueStart,
                                      const ParamDecl **ppGroupStart,
                                      const ParamDecl **ppAliasStart,
                                      UINT32           *pGroupIdx /* = NULL */) const
{
    if (m_ArgHashTable.empty()) return 0; // for no argument test.

    UINT32 argIndex = CallwlateHashIndex(pParam) % m_ArgHashTable.size();
    ArgHashItemList hashTableEntry = m_ArgHashTable[argIndex];

    for(UINT32 i = 0; i < hashTableEntry.size(); i++)
    {
        ArgHashItem item = hashTableEntry[i];
        if (item.keySize == strlen(pParam)) //compare keysize first
        {
            const ParamDecl *pTargetParamDecl = item.pParamDecl;
            if (!memcmp(pTargetParamDecl->pParam, pParam, item.keySize))
            {
                // Find it!
                if (ppGroupStart)
                    *ppGroupStart = item.pGroupStart;

                if (ppAliasStart)
                    *ppAliasStart = item.pAliasStart;

                if (pGroupIdx)
                    *pGroupIdx = item.groupIdx;

                if (pParamNum)
                    *pParamNum = item.paramNum;

                if (pValueStart)
                    *pValueStart = item.valueStart;

                return pTargetParamDecl;
            }
        }
    }

    // NOT Find
    if (pParamNum) *pParamNum = m_ParamNum;
    if (pValueStart) *pValueStart = m_ArgNum;

    return 0;
}

//-----------------------------------------------------------------------------
//! \brief Check the type (and potentially range of a parameter)
//!
//! \param type        : The type of parameter to check (from the pArgTypes
//!                      member of the associated ParamDecl struct)
//! \param pVal        : Pointer to a string representing the value of the
//!                      parameter
//! \param bCheckRange : true if the range of the parameter should be checked
//! \param rangeMin    : The minimum value for the parameter
//! \param rangeMax    : The maximum value for the parameter
//!
//! \return  true if the parameter type/range is valid, false otherwise
bool ArgReader::CheckParamType(char        type,
                               const char *pVal,
                               bool        bCheckRange,
                               UINT32      rangeMin,
                               UINT32      rangeMax)
{
    const char *pEnd;
    UINT32  uVal;
    UINT64  lVal;
    INT32   sVal;
    FLOAT64 fVal;

    switch (type)
    {
        case 'b' :
            // don't allow a string block arg to eat the next argument,
            // it must be followed by {}
            if(pVal[0] != '{')
                pEnd = "X";
            else
                pEnd = "";
            break;

        case 't' :
            // ANYTHING can be a string
            pEnd = "";
            break;

        case 'u' :
            // unsigned (any base)
            errno = 0;
            uVal = strtoul(pVal, const_cast<char **>(&pEnd), 0);
            if (errno != 0) pEnd = "X";
            if (bCheckRange && ((uVal < rangeMin) || (uVal > rangeMax)))
            {
                pEnd = "X";
            }
            break;

        case 'x' :
            // unsigned (hex)
            errno = 0;
            if ((pVal[0] == '0') &&
                ((pVal[1] == 'x') || (pVal[1] == 'X')))
            {
                pVal += 2;
            }
            uVal = strtoul(pVal, const_cast<char **>(&pEnd), 16);
            if (errno != 0) pEnd = "X";
            if (bCheckRange && ((uVal < rangeMin) || (uVal > rangeMax)))
            {
                pEnd = "X";
            }
            break;

        case 'L' :
            // 64-bit unsigned (hex)
            errno = 0;
            if ((pVal[0] == '0') &&
                ((pVal[1] == 'x') || (pVal[1] == 'X')))
            {
                pVal += 2;
            }
            lVal = Utility::Strtoull(pVal, const_cast<char **>(&pEnd), 16);
            if (errno != 0) pEnd = "X";
            if (bCheckRange && ((lVal < rangeMin) || (lVal > rangeMax)))
            {
                pEnd = "X";
            }
            break;

        case 's' :
            // signed (any base)
            errno = 0;
            sVal = strtol(pVal, const_cast<char **>(&pEnd), 0);
            if (errno != 0) pEnd = "X";
            if (bCheckRange &&
                ((sVal < (int)rangeMin) || (sVal > (int)rangeMax)))
            {
                pEnd = "X";
            }
            break;

        case 'f' :
            // floating point (double)
            errno = 0;
            fVal = strtod(pVal, const_cast<char **>(&pEnd));
            if (errno != 0) pEnd = "X";
            if (bCheckRange &&
                ((fVal < (int)rangeMin) || (fVal > (int)rangeMax)))
            {
                pEnd = "X";
            }
            break;

        default:
            // anything else fails automatically
            pEnd = "X";
            break;
    }

    return (*pEnd == 0);
}

//-----------------------------------------------------------------------------
//! \brief Check a parameter list for errors (which are fatal errors because
//!        they should be a compile time issue), and count the number of
//!        parameters and arguments to the parameters
//!
//! \param pParamList  : Pointer to the parameter list to check
//! \param pParamCount : Pointer to the returned parameter count
//! \param pArgCount   : Pointer to the returned argument count
//!
//! \return true if the parameter list is valid, false otherwise
//!
bool ArgReader::CheckParamList(const ParamDecl *pParamList,
                               UINT32          *pParamCount,
                               UINT32          *pArgCount)
{
    const ParamDecl *pGroupStart;
    const ParamDecl *pAliasStart;
    const char      *pType;

    pGroupStart = 0;
    pAliasStart = 0;
    while (pParamList->pParam)
    {
        if (pParamList->flags & ParamDecl::PARAM_SUBDECL_PTR)
        {
            // check the sublist
            if (!CheckParamList((const ParamDecl *)(pParamList->pParam),
                                pParamCount, pArgCount))
            {
                return false;
            }
        }
        else
        {
            (*pParamCount)++;

            if (pParamList->flags & ParamDecl::GROUP_START)
            {
                pGroupStart = pParamList;

                // a group start must have exactly one argument expected
                if (!pParamList->pArgTypes ||
                    (strlen(pParamList->pArgTypes) != 1))
                {
                    Printf(Tee::PriHigh,
                           "param '%s' must have exactly one arg "
                           "because it's a group start!\n",
                           pParamList->pParam);
                    return false;
                }
            }
            else if (pParamList->flags & ParamDecl::ALIAS_START)
            {
                pAliasStart = pParamList;

                // a group start must have exactly one argument expected
                if (!pParamList->pArgTypes ||
                    (strlen(pParamList->pArgTypes) != 1))
                {
                    Printf(Tee::PriHigh,
                           "param '%s' must have exactly one arg because "
                           "it's a alias start!\n",
                           pParamList->pParam);
                    return false;
                }
            }
            else
            {
                if (pParamList->flags & ParamDecl::GROUP_MEMBER)
                {
                    // make sure we're attached to a group start, and have a
                    // parsable piece of data
                    if (!pGroupStart)
                    {
                        Printf(Tee::PriHigh,
                               "group member param '%s' must come after a "
                               "group start!\n",
                               pParamList->pParam);
                        return false;
                    }

                    if (pParamList->flags & ParamDecl::GROUP_MEMBER_UNSIGNED)
                    {
                        string newVal = "0x0";

                        // Need to differentiate between NULL and not on pArgTypes
                        // for group arguments since sprintf will actually print
                        // "(nil)" instead of "0x0" for NULL pointers, in addition
                        // force the addition of "0x" at the start if it is not
                        // there (windows platforms do not put "0x" at the start
                        // of a "%p" colwersion) so that when passed through
                        // strtoul the value is not interpreted as octal
                        if (pParamList->pArgTypes)
                        {
                            newVal = Utility::StrPrintf("%p", pParamList->pArgTypes);
                            if (newVal.substr(0,2) != "0x")
                                newVal.insert(0, "0x");
                        }

                        if(!CheckParamType(pGroupStart->pArgTypes[0], newVal.c_str(),
                               (pGroupStart->flags & ParamDecl::PARAM_ENFORCE_RANGE) ?
                                       true : false,
                                           pGroupStart->rangeMin,
                                           pGroupStart->rangeMax))
                        {
                            Printf(Tee::PriHigh,
                                   "group member param '%s' has unparsable "
                                   "data in argtypes!\n",
                                   pParamList->pParam);
                            return false;
                        }
                    }
                    else
                    {
                        if(!pParamList->pArgTypes ||
                            !CheckParamType(pGroupStart->pArgTypes[0],
                                            pParamList->pArgTypes,
                                (pGroupStart->flags & ParamDecl::PARAM_ENFORCE_RANGE) ?
                                        true : false,
                                            pGroupStart->rangeMin,
                                            pGroupStart->rangeMax))
                        {
                            Printf(Tee::PriHigh,
                                   "group member param '%s' has unparsable "
                                   "data in argtypes!\n",
                                   pParamList->pParam);
                            return false;
                        }
                    }
                }
                else if (pParamList->flags & ParamDecl::ALIAS_MEMBER)
                {
                    // make sure we're attached to a group start, and have a
                    // parsable piece of data
                    if (!pAliasStart)
                    {
                        Printf(Tee::PriHigh,
                               "alias member param '%s' must come after an "
                               "alias start!\n",
                               pParamList->pParam);
                        return false;
                    }

                    if (!pParamList->pArgTypes ||
                        (pAliasStart->pArgTypes[0] != pParamList->pArgTypes[0]))
                    {
                        Printf(Tee::PriHigh,
                               "alias member param '%s' has unparsable data "
                               "in argtypes!\n",
                               pParamList->pParam);
                        return false;
                    }
                }
            }

            // as long as we're not a group/alias member, we need storage
            if (!(pParamList->flags & ParamDecl::GROUP_MEMBER) &&
                !(pParamList->flags & ParamDecl::ALIAS_MEMBER))
            {
                pType = (pParamList->pArgTypes ? pParamList->pArgTypes : "");
                while (*pType)
                {
                    if ((*pType != 't') && (*pType != 'u') && (*pType != 's') &&
                        (*pType != 'x') && (*pType != 'f') && (*pType != 'b') && (*pType != 'L'))
                    {
                        Printf(Tee::PriHigh,
                               "Invalid pType character in param '%s' (%s)\n",
                               pParamList->pParam, pParamList->pArgTypes);
                        return false;
                    }

                    pType++;
                    (*pArgCount)++;
                }
            }
        }

        pParamList++;
    }

    return true;
}

//-----------------------------------------------------------------------------
//! \brief Checks the parameter list for required parameters and complains if
//!        they are not present.
//!
//! \param pParamList    : Pointer to the parameter list to check for required
//!                        parameters
//! \param usageCountIdx : Index to start in the usage count array
//!
//! \return 1 if all required parameters are present
//!         0 if a any missing required parameters were found
//!
UINT32 ArgReader::CheckRequiredParams(const ParamDecl  *pParamList,
                                      UINT32            usageCountIdx)
{
    UINT32 errorCount;

    errorCount = 0;
    while(pParamList->pParam)
    {
        if(pParamList->flags & ParamDecl::PARAM_SUBDECL_PTR)
        {
            if(!CheckRequiredParams((const ParamDecl *)(pParamList->pParam),
                                    usageCountIdx))
                errorCount++;
        }
        else
        {
            if((pParamList->flags & ParamDecl::PARAM_REQUIRED) &&
                (m_UsageCounts[usageCountIdx] == 0))
            {
                Printf(Tee::PriHigh, "Required parameter '%s' not present!\n",
                       pParamList->pParam);
                errorCount++;
            }

            if (!(pParamList->flags & ParamDecl::GROUP_MEMBER) &&
                !(pParamList->flags & ParamDecl::ALIAS_MEMBER))
            {
                usageCountIdx++;
            }
        }

        pParamList++;
    }

    return((errorCount > 0) ? 0 : 1);
}

//-----------------------------------------------------------------------------
//! \brief Checks whether a parameter declaration and its associated parameter
//!        (that can potentially take a single argument) is valid.
//!
//! \param pParamDecl   : Pointer to the parameter declaration to validate
//! \param pParam       : Pointer to the parameter string to validate
//! \param bArgRequired : true if the parameter requires an argument, false
//!                       otherwise
//!
//! \return true if the parameter declaration/parameter is valid, false
//!         otherwise
//!
bool ArgReader::ParamValid(const ParamDecl *pParamDecl,
                           const char      *pParam,
                           bool             bArgRequired) const
{

    if (pParamDecl)
    {
        if ((pParamDecl->flags & ParamDecl::GROUP_MEMBER) ||
            (pParamDecl->flags & ParamDecl::ALIAS_MEMBER))
        {
            Printf(Tee::PriHigh,
                   "Can't query group/alias member '%s' directly!\n",
                   pParam);
            return false;
        }
        else
        {
            if (!bArgRequired ||
                (pParamDecl->pArgTypes && pParamDecl->pArgTypes[0]))
            {
                return true;
            }
            else
            {
                Printf(Tee::PriHigh, "Parameter '%s' doesn't take arguments!\n",
                       pParam);
                return false;
            }
        }
    }

    Printf(Tee::PriError, "Unlisted parameter '%s'!\n", pParam);
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Checks whether a parameter declaration and its associated parameter
//!        (that can take multiple arguments) is valid.
//!
//! \param pParamDecl : Pointer to the parameter declaration to validate
//! \param pParam     : Pointer to the parameter string to validate
//! \param argNum     : Argument number of the parameter to validate
//! \param argIndex   : Argument index in the values array
//!
//! \return true if the parameter declaration/parameter is valid, false
//!         otherwise
//!
bool ArgReader::ParamLWalid(const ParamDecl *pParamDecl,
                            const char      *pParam,
                            UINT32           argNum,
                            UINT32           argIndex) const
{

    if (pParamDecl)
    {
        if ((pParamDecl->flags & ParamDecl::GROUP_MEMBER) ||
            (pParamDecl->flags & ParamDecl::ALIAS_MEMBER))
        {
            Printf(Tee::PriHigh,
                   "Can't query group/alias member '%s' directly!\n",
                   pParam);
            return false;
        }
        else
        {
            if (pParamDecl->pArgTypes &&
                (strlen(pParamDecl->pArgTypes) > argNum))
            {
                if (!m_ParamValues[argIndex].empty())
                {
                    return true;
                }
                else
                {
                    Printf(Tee::PriHigh, "Arg #%d of param '%s' wasn't set!\n",
                           argNum, pParam);
                    return false;
                }
            }
            else
            {
                Printf(Tee::PriHigh,
                       "Parameter '%s' does not have an arg #%d!\n",
                       pParam, argNum);
                return false;
            }
        }
    }

    Printf(Tee::PriHigh, "Unlisted parameter '%s'!\n", pParam);
    MASSERT(0);
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Relwrsively print out the arguments that were parsed from the
//!        database.
//!
//! \param pri            : Priority for the argument print
//! \param pParamList     : Pointer to the parameter list to print
//! \param pUsageCountIdx : Pointer to the current usage count index
//! \param pValuesIdx     : Pointer to the current values index
//! \param pbFirstArg     : Pointer to a boolean status of whether the argument
//!                         being printed is the first argument
//!
void ArgReader::PrintParsedArgsRelwrsion(Tee::Priority    pri,
                                         const ParamDecl *pParamList,
                                         UINT32          *pUsageCountIdx,
                                         UINT32          *pValuesIdx,
                                         bool            *pbFirstArg) const
{
    UINT32 i, j;

    while (pParamList->pParam)
    {
        if (pParamList->flags & ParamDecl::PARAM_SUBDECL_PTR)
        {
            // relwrse into subdecl list
            PrintParsedArgsRelwrsion(pri,
                                     (const ParamDecl *)(pParamList->pParam),
                                     pUsageCountIdx,
                                     pValuesIdx,
                                     pbFirstArg);
        }
        else
        {
            // skip group/alias members
            if (!(pParamList->flags & ParamDecl::GROUP_MEMBER) &&
                !(pParamList->flags & ParamDecl::ALIAS_MEMBER))
            {
                for (i = 0; i < m_UsageCounts[*pUsageCountIdx]; i++)
                {
                    if (!*pbFirstArg)
                        Printf(pri, " ");
                    else
                        *pbFirstArg = false;

                    Printf(pri, "%s", pParamList->pParam);

                    // do any arguments
                    if (pParamList->pArgTypes)
                    {
                        for (j = 0; pParamList->pArgTypes[j]; j++)
                        {
                            const UINT32 vi = *pValuesIdx + j;
                            if (!m_ParamValues[vi].empty())
                            {
                                if ((pParamList->flags & ParamDecl::PARAM_MULTI_OK))
                                {
                                    MASSERT(m_UsageCounts[*pUsageCountIdx] == m_ParamValues[vi].size());
                                    Printf(pri, " %s", m_ParamValues[vi][i].c_str());
                                }
                                else
                                {
                                    auto lastValue = m_ParamValues[vi].rbegin();
                                    Printf(pri, " %s", lastValue->c_str());
                                }
                            }
                        }
                    }
                }

                (*pUsageCountIdx)++;
                if (pParamList->pArgTypes)
                    *pValuesIdx += static_cast<UINT32>(strlen(pParamList->pArgTypes));
            }
        }

        pParamList++;
    }
}

//-------------------------------------------------------------------------
//! \brief GenArgHashTable - Build hashtable according pParamList->pParam
//!
//! \param pParamList   : Pointer to the parameter list to generate the
//!                       hash table
//! \param pParamCount  : Return the number of parameters, inlwlding
//!                       subdeclarations.
//! \param pArgCount    : Return the number of argument values.
//!
//! \return 1 if success, 0  otherwise
//!
bool ArgReader::GenArgHashTable
(
    const ParamDecl  *pParamList,
    UINT32           *pParamCount,
    UINT32           *pArgCount
)
{
    UINT32 groupIndex = 0;
    const ParamDecl *pGroupStart = 0;
    const ParamDecl *pAliasStart = 0;

    MASSERT(pParamCount != 0);
    MASSERT(pArgCount != 0);

    while (pParamList->pParam)
    {
        MASSERT(!m_ArgHashTable.empty());

        if (pParamList->flags & ParamDecl::PARAM_SUBDECL_PTR)
        {
            GenArgHashTable((const ParamDecl *)(pParamList->pParam),
                pParamCount, pArgCount);
        }
        else
        {
            UINT32 index = CallwlateHashIndex(pParamList->pParam);
            index %= m_ArgHashTable.size();

            ArgHashItem newItem;
            // 1. pParamDecl and keySize
            newItem.pParamDecl = pParamList;
            newItem.keySize = static_cast<UINT32>(strlen(pParamList->pParam));

            //2. pGroupStart and pAliasStart
            if (pParamList->flags & ParamDecl::GROUP_START)
            {
                pGroupStart = pParamList;
                groupIndex = 0;
            }
            else if (!(pParamList->flags & ParamDecl::GROUP_MEMBER))
            {
                pGroupStart = 0;
            }
            newItem.pGroupStart = pGroupStart;

            if (pParamList->flags & ParamDecl::ALIAS_START)
            {
                pAliasStart = pParamList;
                groupIndex = 0;
            }
            else if (!(pParamList->flags & ParamDecl::ALIAS_MEMBER))
            {
                pAliasStart = 0;
            }
            newItem.pAliasStart = pAliasStart;

            // 3. paramNum and valueStart
            if (pParamList->flags & ParamDecl::GROUP_MEMBER)
                newItem.paramNum = *pParamCount - 1;
            else if (pParamList->flags & ParamDecl::ALIAS_MEMBER)
                newItem.paramNum = *pParamCount - 1;
            else
                newItem.paramNum = *pParamCount;

            if (pParamList->flags & ParamDecl::GROUP_MEMBER)
                newItem.valueStart = *pArgCount - 1;
            else if (pParamList->flags & ParamDecl::ALIAS_MEMBER)
                newItem.valueStart = *pArgCount - 1;
            else
                newItem.valueStart = *pArgCount;

            // 4. groupIdx
            if (!(pParamList->flags & ParamDecl::GROUP_MEMBER) &&
                !(pParamList->flags & ParamDecl::ALIAS_MEMBER))
                newItem.groupIdx = 0;
            else
                newItem.groupIdx = groupIndex;

            // 5. Save to list and continue counting
            m_ArgHashTable[index].push_back(newItem);

            if (!(pParamList->flags & ParamDecl::GROUP_MEMBER) &&
                !(pParamList->flags & ParamDecl::ALIAS_MEMBER))
            {
                (*pParamCount)++;
                if (pParamList->pArgTypes)
                    (*pArgCount) += (UINT32)strlen(pParamList->pArgTypes);
            }
            else
            {
                groupIndex++;
            }
        }
        pParamList++;
    }

    return true;
}

//-------------------------------------------------------------------------
//! \brief CallwlateHashIndex - generate a hash number of a string
//! \param str   : A string to callwlate the hash number
//!
//! \return A callwlated number as hash index
//!
UINT32 ArgReader::CallwlateHashIndex(const char* str) const
{
    UINT32 sum = 0;
    if (0 == str) return 0;

    while(*str != 0)
    {
        sum = sum * 31 + (*str);
        str ++;
    }

    return sum;
}
