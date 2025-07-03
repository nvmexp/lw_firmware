 /*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010,2014,2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_GPUSBDEV_H
#include "gpu/include/gpusbdev.h"
#endif

class SpeedoUtil
{
public:
    struct SpeedoValues
    {
        UINT32 type;
        vector<UINT32> values;
        vector<IsmInfo> info;
        SpeedoValues(UINT32 speedoType):type(speedoType){};
    };
    struct CpmValues
    {
        UINT32 type;
        UINT32 jtagClkKhz;
        vector<UINT32> binCounts;
        vector<FLOAT32> valuesMhz;
        vector<IsmSpeedo::CpmInfo> info;
        CpmValues(UINT32 speedoType, UINT32 jtagClk):type(speedoType), jtagClkKhz(jtagClk){};
    };

    enum {
        UseDefault = ~0
    };

    SpeedoUtil()
        :m_Part(0),
         m_OscIdx(0),
         m_Duration(~0),
         m_OutDiv(0),
         m_Mode(0),
         m_Adj(0),
         m_Init(-1),
         m_Finit(-1),
         m_RefClk(-1),
         m_pSubdev(NULL),
         m_Voltage1(0),
         m_Voltage2(0),
         m_Speedo1(0),
         m_Speedo2(0),
         m_Calibrated(false)
    {};

    virtual ~SpeedoUtil(){};

    RC ReadSpeedos(vector<SpeedoValues> *pSpeedos);
    RC CallwlateDroop(float *droop, UINT32 *vdd);
    RC Calibrate();
    RC AvgSpeedos(INT32 *pAvg);

    void SetSubdev(GpuSubdevice* pSubdev){m_pSubdev = pSubdev;}
    void SetPart(UINT32 part){m_Part = part;}
    void SetOscIdx(INT32 idx){m_OscIdx = idx;}
    void SetDuration(UINT32 dur){m_Duration = dur;}
    void SetOutDiv(INT32 outDiv){m_OutDiv = outDiv;}
    void SetMode(INT32 mode) {m_Mode = mode;}
    void SetAdj(INT32 adj) {m_Adj = adj;}
    void SetInit(INT32 init) { m_Init = init; }
    void SetFinit(INT32 finit) { m_Finit = finit; }
    void SetRefClk(INT32 refclk) { m_RefClk = refclk; }
    void AddAltSettings(INT32 startBit, INT32 oscIdx, INT32 outDiv, INT32 mode);

private:

    UINT32          m_Part;
    INT32           m_OscIdx;
    UINT32          m_Duration;
    INT32           m_OutDiv;
    INT32           m_Mode;
    INT32           m_Adj;    // used on TSOSC only
    INT32           m_Init;
    INT32           m_Finit;
    INT32           m_RefClk;
    vector<IsmInfo> m_AltSettings;
    GpuSubdevice   *m_pSubdev;

    INT32 m_Voltage1;
    INT32 m_Voltage2;
    INT32 m_Speedo1;
    INT32 m_Speedo2;
    bool m_Calibrated;
};

