/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2013,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZADMAE_H
#define INCLUDED_AZADMAE_H

//!
//! \file
//! \brief
//!
//!

#include "core/include/rc.h"
#include "core/include/tee.h"

class AzaliaController;
class AzaliaFormat;

class AzaliaDmaEngine
{
public:
    enum STATE
    {
        STATE_STOP  = 0,
        STATE_RESET = 1,
        STATE_RUN   = 2
    };

    enum DIR
    {
        DIR_INPUT  = 0,
        DIR_OUTPUT = 1,
        DIR_BIDIR  = 2  // Only valid as an engine type, not stream direction
    };

    enum STRIPE_LINES
    {
        SL_ONE = 0x0,
        SL_TWO = 0x1
    };

    enum TRAFFIC_PRIORITY
    {
        TP_LOW  = 0,
        TP_HIGH = 1
    };

    AzaliaDmaEngine(AzaliaController* pAza, DIR Type, UINT08 Index);
    ~AzaliaDmaEngine();

    RC SetStreamNumber(UINT32 StreamNumber);

    RC GetType(DIR* pType);
    RC GetDir(DIR* pDir);
    RC GetStreamNumber(UINT08 *pIndex);
    RC GetEngineIndex(UINT08 *pIndex);

    RC ProgramEngine(AzaliaFormat* Format, UINT32 BufferLengthInBytes,
                     UINT08 NumBdlEntries, void* pBdl,
                     TRAFFIC_PRIORITY Tp, STRIPE_LINES Sl);

    RC SetEngineState(STATE Target);
    RC GetEngineState(STATE* current);

    UINT32 GetPosition();
    UINT32 GetPositiolwiaDma();

    bool IsFifoReady();
    UINT16 GetFifoSize();

    RC PrintInfo(bool isDebug);

    UINT16 ExpectedFifoSize(AzaliaDmaEngine::DIR dir, AzaliaFormat* pFormat);

    // relevant only for mcp89
    bool IsEcoPresent();

    // Polling functions
    static bool PollIsFifoReady(void *);

private:
    // names for enum STATE
    static const char * m_StateNames[];

    AzaliaController* m_pAza;
    const DIR m_EngineType;
    const UINT08 m_EngineIndex;

    TRAFFIC_PRIORITY m_TrafficPriority;
    STRIPE_LINES m_StripeLines;
    DIR m_StreamDir;
    UINT08 m_StreamNumber;
};

#endif // INCLUDED_AZADMAE_H

