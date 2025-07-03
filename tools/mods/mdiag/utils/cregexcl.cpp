/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/massert.h"
#include "mdiag/sysspec.h"
#include "cregexcl.h"
#include "mdiag/IRegisterMap.h"

const int   CREGEXCL_MAX_LINE = 8192;
const char* CREGEXCL_DELIMS = ":\n\r";

//-----------------------------------------------------------------------
CRegExclusions::CRegExclusions()
{
}

//-----------------------------------------------------------------------
CRegExclusions::~CRegExclusions()
{
    IndexMap_t::iterator it = m_arrayIndexMap.begin();
    IndexMap_t::iterator itend = m_arrayIndexMap.end();
    for(; it != itend; ++it)
    {
        CArrayExclusions* exclusions = it->second;
        for(UINT32 i = 0; i < exclusions->length; ++i)
        {
            free(exclusions->array[i]);
        }
        free(exclusions->array);
        delete(exclusions);
    }
}

//-----------------------------------------------------------------------
bool CRegExclusions::MapArrayReg(const char* line, const IRegisterMap* cb)
{
    // parse line
    char buffer[CREGEXCL_MAX_LINE + 1];
    strncpy(buffer, line, CREGEXCL_MAX_LINE);
    const char* actnstr = strtok(buffer, CREGEXCL_DELIMS);
    const char* namestr = strtok(NULL, CREGEXCL_DELIMS);
    const char* idx1str = strtok(NULL, CREGEXCL_DELIMS);
    const char* idx2str = strtok(NULL, CREGEXCL_DELIMS);
    const char* overflow = strtok(NULL, CREGEXCL_DELIMS);

    // sanity check name
    if(namestr == NULL)
    {
        ErrPrintf("Register array exception specified without register: %s\n", line);
        return false;
    }

    // check for single or double register array
    if(strcmp(actnstr, "A") == 0)
    {
        // sanity check for extra arguments
        if(idx2str != NULL)
        {
            ErrPrintf("Array register exception specified with extra args: %s\n", line);
            return false;
        }

        // sanity check for needed argument
        if(idx1str == NULL)
        {
            ErrPrintf("Array register exception missing args: %s\n", line);
            return false;
        }

        // force index2 to be zero
        idx2str = "0";
    }
    else if(strcmp(actnstr, "A2") == 0)
    {
        // sanity check for extra arguments
        if(overflow != NULL)
        {
            ErrPrintf("Register array exception specified with extra args: %s\n", line);
            return false;
        }

        // sanity check for needed arguments
        if(idx1str == NULL || idx2str == NULL)
        {
            ErrPrintf("Register array exception missing args: %s\n", line);
            return false;
        }
    }
    else
    {
        ErrPrintf("Invalid register exclusion command: %s\n", line);
        return false;
    }

    // match registers
    IRegVec_t regvec;
    cb->MatchRegisters(regvec, namestr);

    // check for wildcards in index values
    bool wildcard1 = false;
    bool wildcard2 = false;
    if(strcmp(idx1str, "*") == 0)
    {
        wildcard1 = true;
    }
    if(strcmp(idx2str, "*") == 0)
    {
        wildcard2 = true;
    }

    // iterate through matching registers, mark exclusion indices
    IRegVec_t::const_iterator it = regvec.begin();
    IRegVec_t::const_iterator itend = regvec.end();
    for(; it != itend; ++it)
    {
        const char* name = (*it)->GetName();
        UINT32 limit1 = 0x0;
        UINT32 limit2 = 0x0;
        UINT32 stride1 = 0x0;
        UINT32 stride2 = 0x0;
        (*it)->GetFormula2(limit1, limit2, stride1, stride2);
        DebugPrintf("Register: %s limit1: %d limit2: %d\n", name, limit1, limit2);

        // initialize exclusion array, if necessary
        IndexMap_t::iterator idxIt = m_arrayIndexMap.find(name);
        IndexMap_t::iterator idxItEnd = m_arrayIndexMap.end();
        CArrayExclusions* exclusions = NULL;
        if(idxIt == idxItEnd)
        {
            exclusions = new CArrayExclusions();
            exclusions->length = limit1;
            exclusions->height = limit2;
            exclusions->array = (bool**)malloc(sizeof(bool*)*limit1);
            for(UINT32 i = 0; i < limit1; ++i)
            {
                exclusions->array[i] = (bool*)malloc(sizeof(bool)*limit2);
                for(UINT32 j = 0; j < limit2; ++j)
                {
                    exclusions->array[i][j] = false;
                }
            }
            m_arrayIndexMap[name] = exclusions;
        }
        else
        {
            // sanity check exclusion array ptr, length, and height
            exclusions = idxIt->second;
            if(exclusions == NULL)
            {
                MASSERT(!"Exclusion array ptr cannot be null!\n");
            }
            if(exclusions->length != limit1 || exclusions->height != limit2)
            {
                MASSERT(!"Exclusion array dimensions do not match register dimensions!\n");
            }
        }

        // check if we need to do wildcard processing
        char* dummy;
        if(wildcard1 && wildcard2)
        {
            // set entire exclusion array to true
            for(UINT32 i = 0; i < limit1; ++i)
            {
                for(UINT32 j = 0; j < limit2; ++j)
                {
                    DebugPrintf("Setting exclusion array key: %s index %d:%d\n", name, i, j);
                    exclusions->array[i][j] = true;
                }
            }
        }
        else if(wildcard1 && !wildcard2)
        {
            // sanity check idx2 value
            UINT32 idx2 = strtoul(idx2str, &dummy, 0);
            if(idx2 >= limit2)
            {
                ErrPrintf("Specified index value:%d exceeds array height: %d\n", idx2, limit2);
                return false;
            }
            for(UINT32 i = 0; i < limit1; ++i)
            {
                DebugPrintf("Setting exclusion array key: %s index %d:%d\n", name, i, idx2);
                exclusions->array[i][idx2] = true;
            }
        }
        else if(!wildcard1 && wildcard2)
        {
            // sanity check idx2 value
            UINT32 idx1 = strtoul(idx1str, &dummy, 0);
            if(idx1 >= limit1)
            {
                ErrPrintf("Specified index value:%d exceeds array length: %d\n", idx1, limit1);
                return false;
            }
            for(UINT32 j = 0; j < limit2; ++j)
            {
                DebugPrintf("Setting exclusion array key: %s index %d:%d\n", name, idx1, j);
                exclusions->array[idx1][j] = true;
            }
        }
        else if(!wildcard1 && !wildcard2)
        {
            // only one specific index, check if values are within range
            UINT32 idx1 = strtoul(idx1str, &dummy, 0);
            UINT32 idx2 = strtoul(idx2str, &dummy, 0);
            if(idx1 >= limit1)
            {
                ErrPrintf("Specified index value:%d exceeds array length: %d\n", idx1, limit1);
                return false;
            }
            if(idx2 >= limit2)
            {
                ErrPrintf("Specified index value:%d exceeds array height: %d\n", idx2, limit2);
                return false;
            }
            DebugPrintf("Setting exclusion array key: %s index %d:%d\n", name, idx1, idx2);
            exclusions->array[idx1][idx2] = true;
        }
        else
        {
            MASSERT(!"RegTestGpu - Invalid wildcard pattern detected\n");
        }

    }

    return true;
}

//-----------------------------------------------------------------------
bool CRegExclusions::SkipArrayReg
(
    const char* regname,
    UINT32 idx1,
    UINT32 idx2
) const
{
    IndexMap_t::const_iterator it = m_arrayIndexMap.find(regname);
    IndexMap_t::const_iterator itend = m_arrayIndexMap.end();
    if(it == itend)
        return false;

    CArrayExclusions* exclusions = it->second;
    MASSERT(exclusions);

    if(idx1 >= exclusions->length || idx2 >= exclusions->height)
    {
        WarnPrintf("Argument index values: %d:%d exceed exclusion array dimensions: %d:%d\n",
            idx1, idx2, exclusions->length, exclusions->height);
        return false;
    }

    bool ret = exclusions->array[idx1][idx2];
    if(ret)
        DebugPrintf("Excluding array register: %s index: %d:%d\n", regname, idx1, idx2);
    return ret;
}

