/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_JSTESTDEVICE_H
#define INCLUDED_JSTESTDEVICE_H

#include "gpu/include/testdevice.h"

struct JSContext;
struct JSObject;
class JsC2C;
class JsDisplay;
class JsFuse;
class JsGpio;
class JsI2c;
class JsLwLink;
class JsPcie;
class JsPerf;
class JsPortPolicyCtrl;
class JsPower;
class JsRegHal;
class JsXusbHostCtrl;

class JsTestDevice
{
public:
    JsTestDevice() = default;
    RC CreateJSObject(JSContext* cx, JSObject* obj);
    RC EndOfLoopCheck(bool captureReference);
    JSObject* GetJSObject() { return m_pJsTestDeviceObj; }
    RC GetAteRev(UINT32* pVal);
    UINT32 GetBusType();
    string GetChipPrivateRevisionString();
    string GetChipRevisionString();
    string GetChipSkuModifier();
    string GetChipSkuNumber();
    RC GetChipTemp(FLOAT32 *pChipTempC);
    RC GetChipTemps(vector<FLOAT32> *pChipTempsC);
    RC GetClockMHz(UINT32 *pClockMhz);
    RC GetDeviceErrorList(vector<TestDevice::DeviceError>* pErrorList);
    UINT32 GetDeviceId();
    string GetDeviceIdString();
    UINT32 GetDeviceTypeInstance();
    UINT32 GetDevInst();
    RC GetEcidString(string* pStr);
    string GetFoundryString();
    string GetName();
    string GetPdiString();
    string GetRawEcidString();
    RC GetRomVersion(string* pVersion);
    TestDevicePtr GetTestDevice() { return m_pTestDevice; }
    RC GetTestPhase(UINT32* pPhase);
    UINT32 GetType();
    string GetTypeName();
    RC GetVoltageMv(UINT32 *pMv);
    bool HasBug(UINT32 bugNum);
    bool HasFeature(UINT32 feature);
    bool IsEmulation();
    bool IsSOC();
    bool IsSysmem();
    UINT32 RegRd32(UINT32 address);
    void RegWr32(UINT32 address, UINT32 value);
    void Release();
    RC PexReset();
    void SetJsC2C(JsC2C* pC2C) { m_pJsC2C = pC2C; }
    void SetJsDisplay(JsDisplay* pDisplay) { m_pJsDisplay = pDisplay; }
    void SetJsFuse(JsFuse* pFuse) { m_pJsFuse = pFuse; }
    void SetJsGpio(JsGpio* pGpio) { m_pJsGpio = pGpio; }
    void SetJsI2c(JsI2c* pI2c) { m_pJsI2c = pI2c; }
    void SetJsLwLink(JsLwLink* pLwLink) { m_pJsLwLink = pLwLink; }
    void SetJsPcie(JsPcie* pPcie) { m_pJsPcie = pPcie; }
    void SetJsPerf(JsPerf* pPerf) { m_pJsPerf = pPerf; }
    void SetJsPortPolicyCtrl(JsPortPolicyCtrl* pPortPolicyCtrl)
        { m_pJsPortPolicyCtrl = pPortPolicyCtrl; }
    void SetJsPower(JsPower* pPower) { m_pJsPower = pPower; }
    void SetJsRegHal(JsRegHal* pRegHal) { m_pJsRegHal = pRegHal; }
    void SetJsXusbHostCtrl(JsXusbHostCtrl* pXusbHostCtrl)
        { m_pJsXusbHostCtrl = pXusbHostCtrl; }
    RC SetLwLinkSystemParameter(UINT64 linkMask, UINT08 setting, UINT08 val);
    void SetTestDevice(TestDevicePtr pTestDevice) { m_pTestDevice = pTestDevice; }

private:
    TestDevicePtr     m_pTestDevice;
    JSObject*         m_pJsTestDeviceObj = nullptr;
    JsC2C*            m_pJsC2C = nullptr;
    JsDisplay*        m_pJsDisplay = nullptr;
    JsFuse*           m_pJsFuse = nullptr;
    JsGpio*           m_pJsGpio = nullptr;
    JsI2c*            m_pJsI2c = nullptr;
    JsLwLink*         m_pJsLwLink = nullptr;
    JsPcie*           m_pJsPcie = nullptr;
    JsPerf*           m_pJsPerf = nullptr;
    JsPortPolicyCtrl* m_pJsPortPolicyCtrl = nullptr;
    JsPower*          m_pJsPower = nullptr;
    JsRegHal*         m_pJsRegHal = nullptr;
    JsXusbHostCtrl*   m_pJsXusbHostCtrl = nullptr;
};

#endif
