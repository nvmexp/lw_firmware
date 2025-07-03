/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/pci.h"
#include "device/interface/pcie.h"

struct JSContext;
struct JSObject;

class JsPcie
{
public:
    JsPcie(Pcie* pPcie);

    RC CreateJSObject(JSContext* cx, JSObject* obj);

    bool GetAerEnabled() const;
    RC GetAspmCyaL1SubState(UINT32* pMask);
    UINT32 GetAspmCyaState() const;
    RC GetAspmL1ssEnabled(UINT32* pMask);
    UINT32 GetAspmCap() const;
    UINT32 GetAspmState() const;
    UINT32 GetBusNumber() const;
    UINT32 GetDeviceId() const;
    UINT32 GetDeviceNumber() const;
    UINT32 GetDomainNumber() const;
    RC GetEnableUphyLogging(bool *pbEnableUphyLogging);
    RC GetErrorCounts
    (
        PexErrorCounts            *pLocCounts,
        PexErrorCounts            *pHostCounts,
        PexDevice::PexCounterType  countType
    );
    UINT32 GetFunctionNumber() const;
    bool GetIsUphyLoggingSupported() const;
    UINT32 GetLinkSpeed();
    UINT32 GetLinkSpeedCapability();
    UINT32 GetLinkSpeedExpected(UINT32 expectedSpeed);
    UINT32 GetLinkWidth();
    RC GetLinkWidths(UINT32* pLocWidth, UINT32* pHostWidth);
    RC GetLtrEnabled(bool* pEnabled);
    Pcie* GetPcie() { return m_pPcie; }
    UINT32 GetSubsystemDeviceId();
    UINT32 GetSubsystemVendorId();
    UINT32 GetUphyVersion();
    RC GetUpStreamInfo(PexDevice** pPexDev, UINT32* pPort);
    RC ResetErrorCounters();
    RC SetAerEnabled(bool bEnabled);
    RC SetAspmState(UINT32 state);
    RC SetEnableUphyLogging(bool bEnableUphyLogging);
    RC SetLinkSpeed(UINT32 newSpeed);
    RC SetLinkWidth(UINT32 linkWidth);

private:
    Pcie* m_pPcie = nullptr;
    JSObject* m_pJsPcieObj = nullptr;
};
