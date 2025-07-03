/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef INCLUDED_SIMCALLMUTEX_H
#define INCLUDED_SIMCALLMUTEX_H

class SimCallMutex
{
public:
    explicit SimCallMutex();
    ~SimCallMutex();

    static void * GetMutex();
    static void   SetMutex(void * mutex);
    enum
    {
        UNINITIALIZEDCW = 0xFFFFFFFF
    };
private:
    // Non-copyable
    SimCallMutex(const SimCallMutex & m);
    SimCallMutex& operator=(const SimCallMutex&);

    unsigned int m_FpControlWord;
    unsigned int m_MxcsrControl;

    // The cmodel overrides the FP/mxcsr control word.  We carefully save
    // and restore this on every entry into the chip library so that external
    // code isn't affected.
    static unsigned int s_ChiplibFPCW;
    static unsigned int s_ChiplibMxcsr;
    static void * s_Mutex;
};
#endif
