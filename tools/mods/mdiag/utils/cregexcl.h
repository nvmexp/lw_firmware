/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CREGEXCLUSIONS_H
#define INCLUDED_CREGEXCLUSIONS_H

#include <map>
#include "types.h"
#include "mdiag/IRegisterMap.h"

// string comparision operation for map key
struct ltstr
{
    bool operator()(const char* s1, const char* s2) const
    {
        return strcmp(s1, s2) < 0;
    }
};

class CRegExclusions
{
public:
    // nested struct for register array exclusions
    typedef struct CArrayExclusions
    {
        bool** array;
        UINT32 length;
        UINT32 height;
    } CArrayExclusions;

    CRegExclusions();
    ~CRegExclusions();

    bool MapArrayReg(const char* line, const IRegisterMap* cb);
    bool SkipArrayReg(const char* regname, UINT32 idx1, UINT32 idx2) const;

private:
    typedef map<const char*, CArrayExclusions*, ltstr> IndexMap_t;
    IndexMap_t m_arrayIndexMap;
};

#endif // INCLUDED_CREGISTERCONTROL_H
