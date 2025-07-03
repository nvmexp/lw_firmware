/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010,2014,2016,2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_JS_FB_H
#define INCLUDED_JS_FB_H

#include "jsapi.h"
#include "core/include/rc.h"
#include <string>

class FrameBuffer;

class JsFrameBuffer
{
public:
    JsFrameBuffer();

    void            SetFrameBuffer(FrameBuffer * pFb);
    FrameBuffer *   GetFrameBuffer();
    RC              CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *      GetJSObject() {return m_pJsFrameBufferObj;}

    string GetName();
    UINT64 GetGraphicsRamAmount();
    UINT32 GetL2SliceCount();
    string PartitionNumberToLetter(UINT32 virtualFbio);
    UINT32 GetPartitions();
    UINT32 GetFbpCount();
    UINT32 GetLtcCount();
    UINT32 GetL2Caches();
    UINT32 GetL2CacheSize();
    UINT32 GetMaxL2SlicesPerFbp();
    bool   IsL2SliceValid(UINT32 slice, UINT32 virtualFbp);
    bool   GetIsEccOn();
    bool   GetIsRowRemappingOn();
    UINT32 GetRamProtocol();
    string GetRamProtocolToString();
    UINT32 GetRamLocation();
    string GetRamLocationToString();
    UINT32 GetSubpartitions();
    UINT32 GetChannelsPerFbio();
    string GetVendorName();

private:
    FrameBuffer *   m_pFrameBuffer;
    JSObject *      m_pJsFrameBufferObj;
};

#endif // INCLUDED_JS_FB_H
