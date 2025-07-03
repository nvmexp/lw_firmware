/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2018, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

class SubstituteRegisters
{
public:
    SubstituteRegisters(const std::string& file);

    RC RegRd(PHYSADDR Offset, void *Data, UINT32 Count);
    RC RegWr(PHYSADDR Offset, const void *Data, UINT32 Count);

private:
    bool ParseSubstitutedRegisterFile();
    bool InSubstituteRegRange(PHYSADDR Offset, UINT32 Count);
    std::string m_SubstituteRegFileName;

    bool m_SubstituteRegister;
    bool m_Initialized;

    vector< std::pair<UINT64, UINT64> > m_SubstituteRegRangeVectors;
    map<UINT64, vector<UINT32>> m_SubstitutedAddrMap;
    map<UINT64, UINT32> m_RegAddrAccessed;
};
