/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _ICPUMODEL_H_
#define _ICPUMODEL_H_

#include "ITypes.h"
#include "IIface.h"

enum CPUModelRet {
    CPUMODEL_HANDLED                = 0,
    CPUMODEL_NOTHANDLED             = 1,
    CPUMODEL_ERROR                  = 2
};

enum CPUModelEvent {
    CPUModelEvent_Ilwalid           = 0,
    CPUModelEvent_RequestComplete   = 1,
    CPUModelEvent_ResponseData      = 2,
    CPUModelEvent_ResponseNoData    = 3,
    CPUModelEvent_RegisterWrite     = 4,
    CPUModelEvent_DgdRsp            = 5,
    CPUModelEvent_IngressProbe      = 6,
    CPUModelEvent_IngressWrite      = 7,
    CPUModelEvent_IngressRead       = 8,
    CPUModelEvent_BarrierReleased   = 9,
    CPUModelEvent_CheckDone         = 10,
    CPUModelEvent_ATSMappingRsp     = 11,
    CPUModelEvent_SentATRsp         = 12,
    CPUModelEvent_ReceivedATSDRsp   = 13,
    CPUModelEvent_ATSWriteOnReadOnlyPage = 14,
    CPUModelEvent_ReadCacheHit      = 15
};

enum CPUModelPagePermission {
    CPUModelPage_None               = 0,
    CPUModelPage_ReadOnly           = 1,
    CPUModelPage_WriteOnly          = 2,
    CPUModelPage_ReadWrite          = 3
};

typedef struct {
    CPUModelEvent type;
    LwU032 id;
    LwU064 value;
    bool usesValue;
} CPUModelResponse;

typedef struct {
    bool valid;
    LwU064 gva;
    LwU064 gpa;
    CPUModelPagePermission permission;
} CPUModelAtsResult;

//
// Interface V2
//

class ICPUModel2 : public IIfaceObject {
public:
    // Client to CPUModel request
    virtual void CPUModelInitialize(void) = 0;

    virtual void CPUModelEnableResponse(bool enable) = 0;
    virtual void CPUModelEnableResponseSpecific(bool enable, CPUModelEvent event) = 0;
    virtual bool CPUModelHasResponse(void) = 0;
    virtual void CPUModelGetResponse(CPUModelResponse *response) = 0;

    virtual CPUModelRet CPUModelWrite(LwU032 uniqueId, LwU064 address, LwU032 offset,
                LwU064 data, LwU032 sizeBytes, bool isCoherentlyCaching, bool isProbing, bool isPosted) = 0;
    virtual CPUModelRet CPUModelRead(LwU032 uniqueId, LwU064 address, LwU032 sizeBytes,
                bool isCoherentlyCaching, bool isProbing) = 0;
    virtual CPUModelRet CPUModelGetCacheData(LwU064 gpa, LwU064 *data) = 0;

    virtual void CPUModelAtsShootDown(LwU032 uniqueId, struct LwPciDev bdf, LwU032 pasid,
                LwU064 address, LwU032 atSize, bool isGpa, bool flush, bool isGlobal) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

class ICPUModelCallbacks2 : public IIfaceObject {
public:
    // CPUModel to Client request
    virtual void CPUModelAtsRequest(struct LwPciDev bdf, LwU032 pasid, LwU064 address,
                bool isGpa, LwU032 numPages, bool numPageAlign, LwU032 *pageSize,
                CPUModelAtsResult *results) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

//
// Interface V1 (deprecated)
//

class ICPUModel : public IIfaceObject {
public:
    // Client to CPUModel request
    virtual void CPUModelInitialize(void) = 0;
    virtual void CPUModelEnableResponse(bool enable) = 0;
    virtual void CPUModelEnableResponseSpecific(bool enable, CPUModelEvent event) = 0;
    virtual bool CPUModelHasResponse(void) = 0;
    virtual void CPUModelGetResponse(CPUModelResponse *response) = 0;
    virtual CPUModelRet CPUModelRead(LwU032 uniqueId, LwU064 address, LwU032 sizeBytes,
                bool isCoherentlyCaching, bool isProbing) = 0;
    virtual CPUModelRet CPUModelWrite(LwU032 uniqueId, LwU064 address, LwU064 data,
                LwU032 sizeBytes, bool isCoherentlyCaching, bool isProbing, bool isPosted) = 0;
    virtual void CPUModelAtsShootDown(struct LwPciDev bdf, LwU032 pasid, LwU064 address, LwU032 sizeBytes, bool isGpa, bool flush) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

class ICPUModelCallbacks : public IIfaceObject {
public:
    // CPUModel to Client request
    virtual CPUModelRet CPUModelAtsRequest(struct LwPciDev bdf, LwU032 pasid, LwU064 gva, LwU064 *gpa) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif // _ICPUMODEL_H_
