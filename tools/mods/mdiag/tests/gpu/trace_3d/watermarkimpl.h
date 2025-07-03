/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef WATER_MARK_IMPL_H
#define WATER_MARK_IMPL_H

#include <string.h>
#include <map>
#include "core/include/massert.h"
#include "mdiag/utils/types.h"
#include <vector>
#include <list>
#include "core/include/lwrm.h"
#include <iterator>

class ArgReader;
class Trace3DTest;
class TraceTsg;
typedef list < TraceTsg > TraceTsgObjects;
class SubCtx;

typedef std::pair<UINT32, string> WaterMarkScopeGetInfoKey;
typedef UINT32 BasePtr;

//---------------------------------------------------------
//! \brief WaterMarkImpl works for parse the commandline
//!  -subctx_cwdslot_watermark or -subctx_veid_cwdslot_watermark.
//! This class will help to got the watermark value and set into
//! the subctx class.
//! Format: -subctx_cwdslot_watermark <tsg_name> <subctx_name>:<watermark_hex_32bitdata>,
//!         <subctx_name>:<watermark_hex_32bitdata>,...
//! Format: -subctx_veid_cwdslot_watermark <tsg_name> <subctx_name>:<watermark_hex_32bitdata>,
//!         <subctx_name>:<watermark_hex_32bitdata>,...
class WaterMarkImpl
{
public:
    WaterMarkImpl(const ArgReader * reader,
            Trace3DTest * test);

    ~WaterMarkImpl();
    RC ParseWaterMark();
    RC GetWaterMark(shared_ptr<SubCtx> pSubCtx, UINT32 * pWaterMark) const;

private:
    class WaterMark
    {
    public:
        WaterMark();
        enum Type
        {
            GLOBAL,
            WATER_MARK_SEQUENCE,
            WATER_MARK_VEID,
        };

        UINT32 GetSubCtxId() {return m_SubCtxId++;}
        Type GetType() { return m_Type; }
        void SetType(Type type) { m_Type = type; }
        map<UINT32, UINT32> * GetWaterMarks() { return &m_WaterMarks; }
        string GetTsgName() { return m_TsgName; }
        void SetTsgName(string tsgName) { m_TsgName = tsgName; }
        UINT32 GetBasePtr() { return m_BasePtr; }
        void SetBasePtr(UINT32 basePtr) { m_BasePtr = basePtr; }
        UINT32 GetWaterMarkGetIndex() { return m_WaterMarkGetIndex + m_BasePtr; }
        void UpdateWaterMarkGetIndex() { m_WaterMarkGetIndex++; }
        UINT32 GetWaterMarkValue(shared_ptr<SubCtx> pSubCtx, UINT32 getIndex);
    private:
        Type m_Type;
        UINT32 m_SubCtxId;
        // <veid, watermark> or <sequence id, watermark>
        map<UINT32, UINT32> m_WaterMarks;
        string m_TsgName;
        UINT32 m_BasePtr; // For shared water mark, set the base get ptr
        UINT32 m_WaterMarkGetIndex; // Symbolize which watermark in this scope shold be choosen
    public:
        typedef std::pair<UINT32, UINT32> WaterMarkObject;
    };

    RC QueryWaterMarkInfo(UINT32 testId,
                            const string tsgName,
                            WaterMark ** ppWaterMarkTsg,
                            UINT32 * pWaterMarkGetIndex) const;
    RC ParseTsgName(string * tsgName);
    RC DoParseWaterMark();
    bool AddWaterMarkTsg(const string & tsgName);
    WaterMark * GetWaterMarkTsg(const string & tsgName) const;

    vector<WaterMark *> m_WaterMarkTsgs;
    Trace3DTest * m_Test;
    const ArgReader * m_Params;
    string m_WaterMarkString;

    // store all water mark getting info globally for query
    static map<WaterMarkScopeGetInfoKey, BasePtr> m_GlobalWaterMarkScopeGetInfos;
};

#endif
