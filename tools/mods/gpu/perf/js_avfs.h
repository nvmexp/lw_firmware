/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_JS_AVFS_H
#define INCLUDED_JS_AVFS_H

#include "core/include/types.h"
#include "gpu/perf/avfssub.h"

class Avfs;
struct JSObject;
struct JSContext;
class RC;

class JsAvfs
{
public:
    JsAvfs();

    RC          CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *  GetJSObject();
    void        SetAvfs(Avfs *pAvfs);
    Avfs *      GetAvfs();
    RC Initialize();

    static RC AdcDeviceStatusToJsObject
    (
        const LW2080_CTRL_CLK_ADC_DEVICE_STATUS &devStatus,
        JSObject *pJsStatus
    );

    static RC AdcCalibrationStateToJsObject
    (
        const Avfs::AdcCalibrationState &cal,
        JSObject *pJsCal
    );

    static RC JsObjectToAdcCalibrationState
    (
        JSObject *pJsCal,
        Avfs::AdcCalibrationState *cal
    );

    static RC LutVfEntryToJsObject
    (
        const LW2080_CTRL_CLK_LUT_VF_ENTRY &lutVfEntry,
        JSObject *pJsLutVfEntry
    );

    static RC LutVfLwrveToJsObject
    (
        JSContext *pContext,
        const LW2080_CTRL_CLK_LUT_VF_LWRVE &lutVfLwrve,
        UINT32 numEntries,
        JSObject *pJsLutVfLwrve
    );

    RC NafllDeviceStatusToJsObject
    (
        JSContext *pContext,
        const LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS &devStatus,
        UINT32 numLutEntries,
        JSObject *pJsStatus
    );

    RC SetFreqControllerEnable(INT32 dom, bool bEnable);
    RC SetFreqControllersEnable(bool bEnable);
    RC SetQuickSlowdown(UINT32 vfLwrveIdx, UINT32 nafllDevMask, bool enable);

    RC DisableClvc();
    RC EnableClvc();

private:
    Avfs *      m_pAvfs;
    JSObject *  m_pJsAvfsObject;
};

#endif //INCLUDED_JS_CLKAVFS_H
