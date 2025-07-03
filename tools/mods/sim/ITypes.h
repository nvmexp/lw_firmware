/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2011,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _ITYPES_H_
#define _ITYPES_H_

#ifdef _WIN32

typedef unsigned __int64 LwU064;
typedef unsigned int     LwU032;
typedef unsigned short   LwU016;
typedef unsigned char    LwU008;

typedef          __int64 LwS064;
typedef   signed int     LwS032;
typedef   signed short   LwS016;
typedef   signed char    LwS008;

#else

typedef unsigned long long LwU064;
typedef unsigned int       LwU032;
typedef unsigned short     LwU016;
typedef unsigned char      LwU008;

typedef   long long        LwS064;
typedef   signed int       LwS032;
typedef   signed short     LwS016;
typedef   signed char      LwS008;

#endif

enum LwErr {
    LW_PASS = 0,                // everthing's OK
    LW_FAIL = 1,                // generic failure (no informtion)
    LW_UNSUPPORTED = 2,         // function is unsupported
    //------------------------- // add new error codes above this line
    LW_FORCE32 = 0xffffffff     // force enum to 32-bit int
};

struct LwPciDev {
    LwU016 domain;
    LwU016 bus;
    LwU016 device;
    LwU016 function;
};

#endif // _ITYPES_H
