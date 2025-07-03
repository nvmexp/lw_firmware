/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZATMOD_H
#define INCLUDED_AZATMOD_H

#include "core/include/rc.h"
#include "core/include/tee.h"

class AzaliaController;
class AzaliaTest;

class AzaliaTestModule
{
public:
    AzaliaTestModule()
    {
        SetDefaultsBase();
        ResetBase(NULL);
    };

    bool IsFinished() { return m_IsFinished; };
    bool IsError() { return m_IsError; };
    bool IsEnabled() { return m_IsEnabled; };
    RC SetEnabled(bool Enable) { m_IsEnabled = Enable; return OK; };

    virtual RC Reset(AzaliaController* pAza) = 0;
    virtual RC SetDefaults() = 0;
    virtual RC Start() = 0;
    virtual RC Continue() = 0;
    virtual RC Stop() = 0;

    RC PrintInfo(Tee::Priority pri, bool printInput, bool printStatus)
    {
        return DoPrintInfo(pri, printInput, printStatus);
    }

    RC PrintInfo(Tee::PriDebugStub, bool printInput, bool printStatus)
    {
        return PrintInfo(Tee::PriSecret, printInput, printStatus);
    }

protected:
    RC SetDefaultsBase()
    {
        m_IsEnabled = false;
        return OK;
    }

    RC ResetBase(AzaliaController* pAza)
    {
        m_pAza = pAza;
        m_IsFinished = false;
        m_IsError = false;
        return OK;
    };

    bool m_IsEnabled;
    bool m_IsFinished;
    bool m_IsError;
    AzaliaController* m_pAza;

private:
    virtual RC DoPrintInfo(Tee::Priority Pri, bool PrintInput, bool PrintStatus) = 0;
};

#endif // INCLUDED_AZATMOD_H
