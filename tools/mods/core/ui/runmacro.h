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

//------------------------------------------------------------------------------

#ifndef INCLUDED_RUNMACRO_H
#define INCLUDED_RUNMACRO_H

#include <string>
#include <map>

// gUIMacros is defined in simpleui.cpp
extern map< string, string > gUIMacros;

extern void RunMacro(string Input);

#endif
