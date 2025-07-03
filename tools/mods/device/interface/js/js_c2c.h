/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/pci.h"
#include "device/interface/c2c.h"

struct JSContext;
struct JSObject;

class JsC2C
{
public:
    explicit JsC2C(C2C* pC2C);

    RC CreateJSObject(JSContext* cx, JSObject* obj);

    RC     DumpRegs() const;
    RC     GetErrorCounts(map<UINT32, C2C::ErrorCounts> * pErrorCounts) const;
    UINT32 GetLinksPerPartition() const;
    UINT32 GetMaxLinks() const;
    UINT32 GetMaxPartitions() const;
    string GetRemoteDeviceString(UINT32 partitionId) const;
    bool   IsPartitionActive(UINT32 partitionId) const;
    void   PrintTopology(UINT32 pri);

private:
    C2C* m_pC2C = nullptr;
    JSObject* m_pJsC2CObj = nullptr;
};
