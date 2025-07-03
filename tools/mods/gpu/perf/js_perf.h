/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_JS_PERF_H
#define INCLUDED_JS_PERF_H

#include "jsapi.h"

#include "gpu/perf/perfsub.h"
#include "core/include/rc.h"
#include <string>

struct JSContext;
struct JSObject;

class JsPerf
{
public:
    JsPerf();

    RC              CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *      GetJSObject() {return m_pJsPerfObj;}
    void            SetPerfObj(Perf * pFb);
    Perf *          GetPerfObj();

    UINT08          GetVersion();
    bool            GetIsPState30Supported();
    bool            GetIsPState35Supported();
    bool            GetIsPState40Supported();
    RC              LockArbiter();
    RC              UnlockArbiter();

    static RC JsObjToClkSetting
    (
        JSObject *pJsClkSetting,
        Perf::ClkSetting *clkSetting
    );

    static RC JsObjToSplitVoltageSetting
    (
        JSObject *pJsVoltSetting,
        Perf::SplitVoltageSetting *voltSetting
    );

    static RC JsObjToInjectedPerfPoint
    (
        JSObject *pJsPerfPt,
        Perf::InjectedPerfPoint *pBdPerfPt
    );

    static RC RmVfPointToJsObj
    (
        UINT32 rmVfIdx,
        const LW2080_CTRL_CLK_CLK_VF_POINT_STATUS &vfPoint,
        JSObject *pJsVfPoint
    );

    RC SetOverrideOVOC(bool override);
    RC SetRailVoltTuningOffsetMv(UINT32 Rail, FLOAT32 OffsetInMv);
    RC SetRailClkDomailwoltTuningOffsetMv(UINT32 Rail, UINT32 ClkDomain, FLOAT32 OffsetInMv);
    RC SetFreqClkDomainOffsetkHz(UINT32 ClkDomain, INT32 FrequencyInkHz);
    RC SetFreqClkDomainOffsetPercent(UINT32 ClkDomain, FLOAT32 percent);
    RC SetClkProgrammingTableOffsetkHz(UINT32 TableIdx, INT32 FrequencyInkHz);
    RC SetClkVfRelationshipOffsetkHz(UINT32 RelIdx, INT32 FrequencyInkHz);
    RC GetClkProgrammingTableSlaveRatio
    (
        UINT32  tableIdx,
        UINT32  slaveEntryIdx,
        UINT08 *pRatio
    );
    RC SetClkProgrammingTableSlaveRatio
    (
        UINT32 tableIdx,
        UINT32 slaveEntryIdx,
        UINT08 ratio
    );
    RC SetVFPointOffsetkHz(UINT32 TableIdx, INT32 FrequencyInkHz);
    RC SetVFPointOffsetMv(UINT32 TableIdx, FLOAT32 OffsetInMv);
    RC SetVFPointsOffsetkHz(UINT32 TableIdxStart, UINT32 TableIdxEnd, INT32 FrequencyInkHz);
    RC SetVFPointsOffsetMv(UINT32 TableIdxStart, UINT32 TableIdxEnd, FLOAT32 OffsetInMv);
    RC SetClkFFREnableState(UINT32 clkDomain, bool enable);
    RC SetRegime(UINT32 clkDomain, UINT32 regime);

    RC OverrideVfeEquOutputRange(UINT32 idx, bool bMax, FLOAT32 val);
    RC UseIMPCompatibleLimits();
    RC RestoreDefaultLimits();

    RC GetPStateErrorCodeDigits(UINT32 *pDigits);

    RC EnableArbitration(UINT32 clkDomain);
    RC DisableArbitration(UINT32 clkDomain);

    RC DisableLwstomerBoostMaxLimit();
    RC DisablePerfLimit(UINT32 limitId);

    // IMP v-pstates are set using the ISMODEPOSSIBLE perf limit by RM when MODS
    // does modesets to ensure that IMP requirements are met. These APIs return
    // the indices of the first and last IMP v-pstate (lowest and highest clocks
    // respectively) so that users can set perfpoints that intersect to any
    // available IMP v-pstates, which is useful for display tests.
    UINT32 GetImpFirstIdx() const;
    UINT32 GetImpLastIdx() const;

    bool GetArbiterEnabled() const;

    RC SetFmonTestPoint(UINT32 gpcclk_MHz, UINT32 margin_MHz, UINT32 margin_uV);

    RC ScaleSpeedo(FLOAT32 factor);

    RC OverrideKappa(FLOAT32 factor);

private:
    Perf *          m_pPerf;
    JSObject *      m_pJsPerfObj;
};

#endif // INCLUDED_JS_Perf_H

