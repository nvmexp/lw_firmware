/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/rc.h"
#include <vector>

class Serial;

class ColorimetryResearchInstruments
{
public:
    ~ColorimetryResearchInstruments();

    RC FindInstruments(UINT32 *numInstruments);

    RC MeasureFlickerStart
    (
        UINT32 instrumentIdx
    );
    RC MeasureFlicker
    (
        UINT32 instrumentIdx,
        FLOAT64 maximumSearchFrequency,
        FLOAT64 lowPassFilterFrequency,
        UINT32 filterOrder,
        FLOAT64 *flickerFrequency,
        FLOAT64 *flickerLevel,
        FLOAT64 *flickerModulationAmpliture
    );

    RC MeasureLuminance
    (
        UINT32 instrumentIdx,
        FLOAT64 *manualSyncFrequencyHz,
        FLOAT64 *luminanceCdM2
    );

    RC MeasureCIE1931xy
    (
        UINT32 instrumentIdx,
        FLOAT64 *manualSyncFrequencyHz,
        FLOAT32 *x,
        FLOAT32 *y
    );

private:

    struct Instrument
    {
        Serial *pCom;
        string ModelName;
        bool   MeasureFlickerStarted;
    };

    vector<Instrument> m_Instruments;

    RC PerformColorimeterMeasurement
    (
        UINT32 instrumentIdx,
        FLOAT64 *manualSyncFrequencyHz
    );
    RC CaptureSyncFrequency
    (
        UINT32 instrumentIdx,
        FLOAT64 *manualSyncFrequencyHz
    );
};
