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

#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlcci.h"
#include <vector>

struct JSContext;
struct JSObject;
class JsRegHal;

class JsLwLink
{
public:
    JsLwLink(LwLink* pLwLink);

    RC CreateJSObject(JSContext* cx, JSObject* obj);

    bool AreLanesReversed(UINT32 linkId);
    RC ClearErrorCounts(UINT32 linkId);
    RC ClearLPEntryCount(UINT32 linkId);
    RC EnablePerLaneErrorCounts(bool bEnable);
    UINT32 GetCCICount();
    RC GetCCIErrorFlags(LwLinkCableController::CCErrorFlags * pCciErrorFlags);
    const char * GetCCIErrorFlagString(UINT08 cciErrorFlag);
    string GetCCIHwRevision(UINT32 cciId);
    RC GetCCILinks(UINT32 cciId, vector<UINT32> *pCciLinks);
    string GetCCIPartNumber(UINT32 cciId);
    UINT32 GetCCIPhysicalId(UINT32 cciId);
    string GetCCISerialNumber(UINT32 cciId);
    RC GetEnableUphyLogging(bool *pbEnableUphyLogging);
    RC GetFlaCapable(bool *pbEnableFlaCap);
    RC GetFlaEnabled(bool *pbFlaEnabled);
    RC GetErrorCounts(UINT32 linkId, LwLink::ErrorCounts* pErrorCounts);
    RC GetErrorFlags(LwLink::ErrorFlags * pErrorFlags);
    bool GetIsUphyLoggingSupported() const;
    UINT32 GetLineRateMbps(UINT32 linkId);
    string GetLinkGroupName();
    UINT32 GetLinksPerGroup();
    UINT32 GetMaxLinkGroups();
    UINT32 GetMaxLinks();
    LwLink* GetLwLink() { return m_pLwLink; }
    UINT32 GetSublinkWidth(UINT32 linkId);
    UINT32 GetTopologyId();
    bool GetSupportsLPCounters();
    UINT32 GetUphyVersion();
    bool IsCCIErrorFlagFatal(UINT08 cciErrorFlag);
    bool IsLinkActive(UINT32 linkId);
    bool IsLinkValid(UINT32 linkId);
    RC IsTrunkLink(UINT32 linkId, bool *pbTrunkLink);
    RC ReadUphyPadLaneReg(UINT32 link, UINT32 lane, UINT16 addr, UINT16 * pReadData);
    RC SetCollapsedResponses(UINT32 mask);    
    RC SetEnableUphyLogging(bool bEnableUphyLogging);
    void SetJsRegHal(JsRegHal* pRegHal) { m_pJsRegHal = pRegHal; }
    RC SetLinkState(UINT32 linkId, UINT32 linkState);
    RC SetNeaLoopbackLinkMask(UINT64 linkMask);

private:
    LwLink* m_pLwLink = nullptr;
    JSObject* m_pJsLwLinkObj = nullptr;
    JsRegHal* m_pJsRegHal = nullptr;
};
