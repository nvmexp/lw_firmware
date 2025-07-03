/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_TREP_H
#define INCLUDED_TREP_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_CSTDIO
#include <cstdio>
#define INCLUDED_STL_CSTDIO
#endif

class Trep
{
public:
    Trep();
    virtual ~Trep();

    RC SetTrepFileName( const char *trepName );
    RC AppendTrepString( const char *str );

    RC SetTrepFileName( string name) { return SetTrepFileName(name.c_str()); }
    RC AppendTrepString( string s ) { return AppendTrepString(s.c_str()); }

    // See comments in trep.cpp
    RC AppendTrepResult(UINT32 testNum, INT32 testRC);
private:

    const char * m_FileName;
    FILE * m_pFile;

    //! Account for the fact that mdiag didn't return RCs in the original
    //! implementation
    RC m_FileOpenRC;
};

#endif // INCLUDED_TREP_H
