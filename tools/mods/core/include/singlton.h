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

// Templates for singleton classes.

#ifndef INCLUDED_SINGLTON_H
#define INCLUDED_SINGLTON_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_MASSERT_H
#include "massert.h"
#endif
#ifndef INCLUDED_TEE_H
#include "tee.h"
#endif

#define SINGLETON(sname)                                            \
class sname ## Ptr                                                  \
{                                                                   \
private:                                                            \
    static sname *s_pInstance;                                      \
    static const char *s_Name;                                      \
                                                                    \
    /* Do not allow copying. */                                     \
    sname ## Ptr (const sname ## Ptr &);                            \
    sname ## Ptr  & operator=(const sname ## Ptr &);                \
                                                                    \
public:                                                             \
    sname ## Ptr() { }                                              \
    sname * Instance()   const { return s_pInstance; }              \
    sname * operator->() const { return s_pInstance; }              \
    sname * operator&()  const { return s_pInstance; }              \
    sname & operator*()  const { return *s_pInstance; }             \
                                                                    \
    static RC Install( sname *p )                                   \
    {                                                               \
        MASSERT(p != 0);                                            \
                                                                    \
        if (s_pInstance)                                            \
            return RC::DID_NOT_INSTALL_SINGLETON;                   \
                                                                    \
        s_pInstance = p;                                            \
        return OK;                                                  \
    }                                                               \
    static void Free()                                              \
    {                                                               \
        if (s_pInstance )                                           \
        {                                                           \
            delete s_pInstance;                                     \
            s_pInstance = 0;                                        \
        }                                                           \
    }                                                               \
   static bool IsInstalled() { return ( 0 != s_pInstance ); }       \
};

#endif // !INCLUDED_SINGLTON_H
