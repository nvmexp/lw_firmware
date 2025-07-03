/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2022 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   traceop.h
 * @brief  various TraceOp classes.
 *
 */
#ifndef INCLUDED_TRACEOP_H
#define INCLUDED_TRACEOP_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_TYPES_H
#include "mdiag/utils/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_MASSERT_H
#include "core/include/massert.h"
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif
#include "gputrace.h"
#include "tracemod.h"
#include "mdiag/utils/tex.h"

class TraceChannel;
class TraceFileMgr;
class TraceOpSendGpEntry;
class GpuDevice;
class GpuSubdevice;
class Trace3DTest;

//------------------------------------------------------------------------------
// TraceOp: abstract base class, represents a single operation from a parsed
// test.hdr file.
//
// The Run() methods in derived classes call back into their GpuTrace
// object to do their work.
//
class TraceOp
{
public:
    TraceOp();
    virtual ~TraceOp();

    virtual RC   Run() = 0;
    virtual void Print() const = 0;
    virtual TraceOpType GetType() const { return TraceOpType::Unknown; }

    // This enum is used by TraceOpWaitValue32.
    enum BoolFunc
    {
        BfEqual,
        BfNotEqual,
        BfGreater,
        BfLess,
        BfGreaterEqual,
        BfLessEqual,

        BfNUM_ITEMS
    };

    //describe traceOp status
    enum TraceOpStatus
    {
        TRACEOP_STATUS_DEFAULT,
        TRACEOP_STATUS_PENDING,
        TRACEOP_STATUS_DONE
    };

    enum
    {
        TIMEOUT_IMMEDIATE = (UINT32)~0,
        TIMEOUT_NONE      = (UINT32)(TIMEOUT_IMMEDIATE - 1)
    };

    static const char * BoolFuncToString(BoolFunc bf);
    static bool Compare(UINT32 val, UINT32 ref, BoolFunc bf)
    {
        switch (bf)
        {
            case BfEqual:           return val == ref;
            case BfNotEqual:        return val != ref;
            case BfGreater:         return val >  ref;
            case BfLess:            return val <  ref;
            case BfGreaterEqual:    return val >= ref;
            case BfLessEqual:       return val <= ref;
            default:
                MASSERT(!"unsupported bool function");
                return false;
        }
    }

    // used by the plugin system to query the contents of
    // the trace ops.
    //
    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const = 0;

    void SetHdrStrProperties(const GpuTrace::HdrStrProps& props) { m_HdrStrProps = props; }
    const GpuTrace::HdrStrProps& GetHdrStrProperties() const { return m_HdrStrProps; }

    UINT32 GetTraceOpId() const { return m_TraceOpId; }

    void SetTraceOpStatus(TraceOpStatus traceOpStatus) { m_TraceOpStatus = traceOpStatus; }
    TraceOpStatus GetTraceOpStatus() { return m_TraceOpStatus; }

    GpuTrace* GetGpuTrace() const { return m_GpuTrace; }

    void InitTraceOpDependency(GpuTrace* gpuTrace);
    void AddTraceOpDependency(UINT32 dependentTraceOpId);
    bool CheckTraceOpDependency();

    TraceOpTriggerPoint GetTriggerPoint() const { return m_TriggerPoint; }
    void SetTriggerPoint(const TraceOpTriggerPoint point) { m_TriggerPoint = point; }
    bool NeedSkip() const { return m_Skip; }
    void SetSkip(bool skip) { m_Skip = skip; }
    const UINT32 GetTraceOpExecOpNum() { return m_TraceOpExecOpNum; }
    void SetTraceOpExecNum(UINT32 traceOpExecNum) { m_TraceOpExecOpNum = traceOpExecNum; }

    static const char* GetOpName(const TraceOpType type);
    // Can't do (TriggerPointA & TriggerPointB) test directly
    // because of enum class type,
    // Use this small function to do so.
    static bool HasTriggerPoint(UINT32 combinedPoint,
        const TraceOpTriggerPoint point)
    {
        return (combinedPoint & static_cast<UINT32>(point)) != 0x0;
    }
    static string GetTriggerPointStr(UINT32 point);

protected:
    friend class GpuTrace;
    virtual bool HasMethods() const;
    virtual bool WillPolling() const;

private:
    //Describe traceOp dependency
    UINT32         m_TraceOpId;
    deque<UINT32>  m_DependentTraceOpIds;
    TraceOpStatus  m_TraceOpStatus;
    GpuTrace*      m_GpuTrace;
    TraceOpTriggerPoint
                   m_TriggerPoint;
    GpuTrace::HdrStrProps
                   m_HdrStrProps;

    bool           m_Skip;
    bool           m_IsDefaultDependency;
    UINT32         m_TraceOpExecOpNum;
};

//------------------------------------------------------------------------------
// TraceOpSendMethods: writes part or all of the methods TraceModule to the
// channel it is associated with.
class TraceOpSendMethods : public TraceOp
{
public:
    TraceOpSendMethods
    (
        TraceModule * pTraceModule,
        UINT32 start,
        UINT32 size
    );
    virtual ~TraceOpSendMethods();

    virtual RC Run();
    virtual void Print() const;

    bool SegmentIsValid() const;

    void GetTraceopProperties( const char * * pName,
                               map<string,string> & properties ) const;

protected:
    virtual bool HasMethods() const;

private:
    TraceModule * m_pTraceModule;
    UINT32  m_Start;
    UINT32  m_Size;
    size_t  m_SegmentH;
    bool    m_SegmentIsValid;
};

//------------------------------------------------------------------------------
// TraceOpSendGpEntry: writes a GPFIFO entry to the channel
class TraceOpSendGpEntry : public TraceOp
{
public:
    TraceOpSendGpEntry
    (
        TraceChannel * pTraceChannel,
        TraceModule  * pTraceModule,
        UINT64         offset,
        UINT32         size,
        bool           sync,
        UINT32         repeat_num
    );

    virtual ~TraceOpSendGpEntry();
    virtual void Print() const;

    virtual RC Run();

    virtual RC ConsolidateGpEntry(UINT32 Size);

    // based on current gpentry info,
    // create a new continuous gpentryop object with new size
    virtual TraceOpSendGpEntry* CreateNewContGpEntryOp(UINT32 Size);

    void GetTraceopProperties( const char * * pName,
                               map<string,string> & properties ) const;
    TraceChannel * GetTraceChannel() const { return m_pTraceChannel; }

protected:
    virtual bool HasMethods() const;

private:
    RC EscWriteGpEntryInfo();

    TraceChannel *  m_pTraceChannel;
    TraceModule *   m_pTraceModule;
    UINT32          m_Offset;
    UINT32          m_Size;
    bool            m_Sync;
    UINT32          m_RepeatNum;
};

//------------------------------------------------------------------------------
// TraceOpFlush: flush a channel
class TraceOpFlush : public TraceOp
{
public:
    TraceOpFlush
    (
        TraceChannel * pTraceChannel
     );
    virtual ~TraceOpFlush();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceChannel * m_pTraceChannel;
};

//------------------------------------------------------------------------------
// TraceOpWaitIdle: wait for a channel to go idle
class TraceOpWaitIdle : public TraceOp
{
public:
    TraceOpWaitIdle();
    virtual ~TraceOpWaitIdle();

    virtual RC Run();
    virtual void Print() const;
    virtual bool WillPolling() const;

    void GetTraceopProperties( const char * * pName,
                               map<string,string> & properties ) const;
    RC AddWFITraceChannel(TraceChannel * pTraceChannel);

private:
    struct WaitIdleTraceChannel
    {
        WaitIdleTraceChannel
        (
            TraceChannel* traceChannelPtr,
            UINT32        traceChannelPayloadValue
        ) :
        TraceChannelPtr(traceChannelPtr),
        TraceChannelPayloadValue(traceChannelPayloadValue){};

        TraceChannel* TraceChannelPtr;
        UINT32        TraceChannelPayloadValue;
    };

    vector<WaitIdleTraceChannel> m_WaitIdleTraceChannels;
    bool m_HaveInsertedSem;
};

//------------------------------------------------------------------------------
// TraceOpWaitValue32: wait for a 32-bit value in a surface to satisfy a boolean
class TraceOpWaitValue32 : public TraceOp
{
public:
    TraceOpWaitValue32
    (
        TraceModule * pTraceModule,
        UINT32 surfOffset,
        TraceOp::BoolFunc compare,
        UINT32 value,
        UINT32 timeoutMs
    );
    virtual ~TraceOpWaitValue32();

    virtual RC Run();
    virtual void Print() const;

    virtual bool WillPolling() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

    bool Poll();

private:
    TraceModule *       m_pTraceModule;
    UINT32              m_SurfOffset;
    TraceOp::BoolFunc   m_Compare;
    UINT32              m_Value;
    UINT32              m_TimeoutMs;
    MdiagSurf *         m_pDmaBuffer;
};

//------------------------------------------------------------------------------
// TraceOpUpdateFile
class TraceOpUpdateFile : public TraceOp
{
public:
    TraceOpUpdateFile
    (
        TraceModule *  pTraceModule,
        UINT32         moduleOffset,
        TraceFileMgr * pTraceFileMgr,
        string         filename,
        bool           flushCache,
        GpuDevice    * gpuDevice,
        UINT32         timeoutMs
     );
    virtual ~TraceOpUpdateFile();

    virtual RC Run();
    virtual void Print() const;
    virtual TraceOpType GetType() const { return TraceOpType::UpdateFile; }

    void GetTraceopProperties( const char * * pName,
                               map<string,string> & properties ) const;

private:
    TraceModule *       m_pTraceModule;
    UINT32              m_ModuleOffset;
    TraceFileMgr *      m_pTraceFileMgr;
    string              m_FileName;
    bool                m_FlushCache;
    GpuDevice *         m_GpuDevice;
    UINT32              m_TimeoutMs;
};

//------------------------------------------------------------------------------
// TraceOpUpdateReloc64
class TraceOpUpdateReloc64 : public TraceOp
{
public:
    TraceOpUpdateReloc64
    (
        string         name,
        TraceModule *  pTraceModule,
        UINT64         mask_out,
        UINT64         mask_in,
        UINT32         offset,
        bool           bswap,
        bool           badd,
        UINT32         peerNum,
        UINT32         subdevNum,
        UINT64         signExtendMask,
        string         signExtendMode
     );
    virtual ~TraceOpUpdateReloc64();

    virtual RC Run();
    virtual void Print() const;
    virtual TraceOpType GetType() const { return TraceOpType::Reloc64; }
    string GetSurfaceName() { return m_SurfName; }

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    string              m_SurfName;
    TraceModule *       m_pTraceModule;
    UINT64              m_MaskOut;
    UINT64              m_MaskIn;
    UINT32              m_Offset;
    bool                m_Swap;
    bool                m_Add;
    UINT32              m_PeerNum;
    UINT32              m_SubdevNum;
    UINT64              m_SignExtendMask;
    string              m_SignExtendMode;
};

//------------------------------------------------------------------------------
// TraceOpUpdateRelocConst32
class TraceOpUpdateRelocConst32 : public TraceOp
{
public:
    TraceOpUpdateRelocConst32
    (
        TraceModule *  pTraceModule,
        UINT32         value,
        UINT32         offset
     );
    virtual ~TraceOpUpdateRelocConst32();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceModule *       m_pTraceModule;
    UINT32              m_Value;
    UINT32              m_Offset;
};

class TraceOpSec : public TraceOp
{

public:
    TraceOpSec
    (
         TraceModule  *pTraceModule,
         UINT32        moduleOffset,
         bool          isContent,
         const char    *contentKey,
         UINT32        subdev
    );
    virtual ~TraceOpSec();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    int getContentKey(unsigned char *out, const char *inStr) ;

    static const UINT32 SKEY_SIZE_BYTES = 16 ;
    TraceModule *       m_pTraceModule;
    UINT32              m_ModuleOffset;
    bool                m_IsContent;
    char *              m_ContentKey ;
    UINT32              m_Subdev;
};

//------------------------------------------------------------------------------
// TraceOpEscapeWrite
class TraceOpEscapeWrite : public TraceOp
{
public:
    TraceOpEscapeWrite
    (
        GpuDevice *pGpuDev,
        UINT32     subdevMask,
        string     path,
        UINT32     index,
        UINT32     size,
        UINT32     data
    );
    virtual ~TraceOpEscapeWrite();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    GpuDevice *m_pGpuDev;
    UINT32     m_SubdevMask;
    string     m_Path;
    UINT32     m_Index;
    UINT32     m_Size;
    UINT32     m_Data;
};

//------------------------------------------------------------------------------
// TraceOpEscapeWriteBuffer
class TraceOpEscapeWriteBuffer : public TraceOp
{
public:
    TraceOpEscapeWriteBuffer
    (
        GpuDevice *pGpuDev,
        UINT32     subdevMask,
        string     path,
        UINT32     index,
        UINT32     size,
        const char *data
    );
    virtual ~TraceOpEscapeWriteBuffer();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    GpuDevice *m_pGpuDev;
    UINT32     m_SubdevMask;
    string     m_Path;
    UINT32     m_Index;
    UINT32     m_Size;
    char      *m_Data;
};

//------------------------------------------------------------------------------
// TraceOpEscapeCompare
class TraceOpEscapeCompare : public TraceOp
{
public:
    TraceOpEscapeCompare
    (
        GpuDevice *pGpuDev,
        UINT32     subdevMask,
        string     path,
        UINT32     index,
        UINT32     size,
        BoolFunc   compare,
        UINT32     desiredValue,
        UINT32     timeoutMs
    );
    virtual ~TraceOpEscapeCompare();

    bool Poll();
    virtual RC Run();
    virtual void Print() const;
    virtual bool WillPolling() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    GpuDevice *m_pGpuDev;
    UINT32     m_SubdevMask;
    string     m_Path;
    UINT32     m_Index;
    UINT32     m_Size;
    BoolFunc   m_Compare;
    UINT32     m_DesiredValue;
    UINT32     m_TimeoutMs;
};

//------------------------------------------------------------------------------
// TraceOpProgramZBCColor
class TraceOpProgramZBCColor : public TraceOp
{
public:
    TraceOpProgramZBCColor
    (
        Trace3DTest   *test,
        UINT32         colorFormat,
        string         colorFormatString,
        const UINT32  *colorFB,
        const UINT32  *colorDS,
        bool          bSkipL2Table
    );
    virtual ~TraceOpProgramZBCColor();
    virtual RC Run();
    virtual void Print() const;
    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;
private:
    Trace3DTest     *m_Test;
    UINT32           m_colorFormat;
    string           m_colorFormatString;
    vector<UINT32>   m_colorFB;
    vector<UINT32>   m_colorDS;
    static const UINT32     ZBCEntriesNum = 4;
};

//------------------------------------------------------------------------------
// TraceOpProgramZBCDepth
class TraceOpProgramZBCDepth : public TraceOp
{
public:
    TraceOpProgramZBCDepth
    (
        Trace3DTest *test,
        UINT32       depthFormat,
        string       depthFormatString,
        UINT32       depth,
        bool         bSkipL2Table
    );
    virtual ~TraceOpProgramZBCDepth();
    virtual RC Run();
    virtual void Print() const;
    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;
private:
    Trace3DTest *m_Test;
    UINT32       m_depthFormat;
    string       m_depthFormatString;
    UINT32       m_depth;
};

//------------------------------------------------------------------------------
// TraceOpProgramZBCStencil
class TraceOpProgramZBCStencil : public TraceOp
{
public:
    TraceOpProgramZBCStencil
    (
        Trace3DTest *test,
        UINT32       stencilFormat,
        string       stencilFormatString,
        UINT32       stencil,
        bool         bSkipL2Table
    );
    virtual ~TraceOpProgramZBCStencil();
    virtual RC Run();
    virtual void Print() const;
    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;
private:
    Trace3DTest *m_Test;
    UINT32       m_stencilFormat;
    string       m_stencilFormatString;
    UINT32       m_stencil;
};

//------------------------------------------------------------------------------
// TraceOpReg
class TraceOpReg : public TraceOp
{
public:
    TraceOpReg
    (
        GpuDevice *pGpuDev,
        UINT32     subdevMask,
        string     reg,
        UINT32     mask,
        UINT32     data
    );
    TraceOpReg
    (
        GpuDevice *pGpuDev,
        UINT32     subdevMask,
        string     reg,
        string     field,
        UINT32     data
    );
    virtual ~TraceOpReg();

    RC SetupRegs();

protected:
    struct PriReg
    {
        UINT32 Offset;
        UINT32 Mask;
        UINT32 Data;
    };
    map<UINT08,PriReg *> m_SubdevRegMap;
    GpuDevice *m_pGpuDev;
    UINT32     m_SubdevMask;
    string     m_Reg;
    string     m_Field;
    UINT32     m_Mask;
    UINT32     m_Data;
private:
    RC GetRegIndexes(int *pDimension, int *pI, int *pJ);
};

//------------------------------------------------------------------------------
// TraceOpPriWrite
class TraceOpPriWrite : public TraceOpReg
{
public:
    TraceOpPriWrite
    (
        Trace3DTest *pTest,
        UINT32      subdevMask,
        string      reg,
        UINT32      mask,
        UINT32      data
    );

    TraceOpPriWrite
    (
        Trace3DTest *pTest,
        UINT32      subdevMask,
        string      reg,
        string      field,
        UINT32      data
    );

    TraceOpPriWrite
    (
        Trace3DTest *pTest,
        UINT32       subdevMask,
        string       reg,
        string       type,
        TraceModule *pMod
    );

    virtual ~TraceOpPriWrite();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    string       m_TypeStr;
    TraceModule  *m_pMod;
    Trace3DTest  *m_Test;
};

//------------------------------------------------------------------------------
// TraceOpPriCompare
class TraceOpPriCompare : public TraceOpReg
{
public:
    TraceOpPriCompare
    (
        Trace3DTest *pTest,
        UINT32      subdevMask,
        string      reg,
        UINT32      mask,
        BoolFunc    compare,
        UINT32      desiredData,
        UINT32     timeoutMs
    );
    TraceOpPriCompare
    (
        Trace3DTest *pTest,
        UINT32      subdevMask,
        string      reg,
        string      field,
        BoolFunc    compare,
        UINT32      desiredData,
        UINT32      timeoutMs
    );
    virtual ~TraceOpPriCompare();

    bool Poll();
    virtual RC Run();
    virtual void Print() const;
    virtual bool WillPolling() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    BoolFunc    m_Compare;
    UINT32      m_TimeoutMs;
    Trace3DTest *m_Test;
};

//------------------------------------------------------------------------------
// TraceOpCfgWrite
class TraceOpCfgWrite : public TraceOpReg
{
public:
    TraceOpCfgWrite
    (
        GpuDevice *pGpuDev,
        UINT32     subdevMask,
        string     reg,
        UINT32     mask,
        UINT32     data
    );

    TraceOpCfgWrite
    (
        GpuDevice *pGpuDev,
        UINT32     subdevMask,
        string     reg,
        string     field,
        UINT32     data
    );

    virtual ~TraceOpCfgWrite();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;
};

//------------------------------------------------------------------------------
// TraceOpCfgCompare
class TraceOpCfgCompare : public TraceOpReg
{
public:
    TraceOpCfgCompare
    (
        GpuDevice *pGpuDev,
        UINT32     subdevMask,
        string     reg,
        UINT32     mask,
        BoolFunc   compare,
        UINT32     desiredData,
        UINT32     timeoutMs
    );
    TraceOpCfgCompare
    (
        GpuDevice *pGpuDev,
        UINT32     subdevMask,
        string     reg,
        string     field,
        BoolFunc   compare,
        UINT32     desiredData,
        UINT32     timeoutMs
    );

    virtual ~TraceOpCfgCompare();

    bool Poll();
    virtual RC Run();
    virtual void Print() const;
    virtual bool WillPolling() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    BoolFunc m_Compare;
    UINT32   m_TimeoutMs;
    RC       m_PollRc;
};

//------------------------------------------------------------------------------
// TraceOpWaitPmuCmd
class TraceOpWaitPmuCmd : public TraceOp
{
public:
    TraceOpWaitPmuCmd(UINT32 cmdId, UINT32 timeoutMs, LwRm* pLwRm);
    virtual ~TraceOpWaitPmuCmd();

    bool Poll();
    virtual RC Run();
    virtual void Print() const;
    virtual bool WillPolling() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

    void Setup(GpuSubdevice *pSubdev);

private:
    UINT32        m_CmdId;
    UINT32        m_TimeoutMs;
    LwRm*         m_pLwRm;
    bool          m_bSetup;
    RC            m_PollRc;
    GpuSubdevice *m_pSubdev;
};

//------------------------------------------------------------------------------
// TraceOpId: abstract base class, represents a single operation from a parsed
// test.hdr file that may send a PMU command.
//
class TraceOpPmu : public TraceOp
{
public:
    TraceOpPmu(UINT32 CmdId);
    virtual ~TraceOpPmu();

    UINT32 GetCmdId() const  { return m_CmdId; }
    void   SetWaitOp(TraceOpWaitPmuCmd *pWaitOp) { m_pWaitOp = pWaitOp; }
    TraceOpWaitPmuCmd *GetWaitOp() const { return m_pWaitOp; }

private:
    UINT32             m_CmdId;
    TraceOpWaitPmuCmd *m_pWaitOp;
};

//------------------------------------------------------------------------------
// TraceOpSysMemCacheCtrl
class TraceOpSysMemCacheCtrl : public TraceOpPmu
{
public:
    TraceOpSysMemCacheCtrl
    (
        UINT32 writeMode,
        UINT32 powerState,
        UINT32 rcmState,
        UINT32 vgaMode,
        GpuSubdevice *gpuSubdev,
        UINT32 cmdId,
        LwRm* pLwRm
    );
    virtual ~TraceOpSysMemCacheCtrl();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    UINT32 m_WriteMode;
    UINT32 m_PowerState;
    UINT32 m_RCMState;
    UINT32 m_VGAMode;
    GpuSubdevice *m_GpuSubdev;
    LwRm* m_pLwRm;
};

//------------------------------------------------------------------------------
// TraceOpFlushCacheCtrl - Flush memory caches
class TraceOpFlushCacheCtrl : public TraceOp
{
public:
    TraceOpFlushCacheCtrl
    (
        string &Surf,
        UINT32 WriteBack,
        UINT32 Ilwalidate,
        UINT32 Mode,
        Trace3DTest *Test,
        GpuSubdevice *gpuSubdev
    );

    virtual ~TraceOpFlushCacheCtrl();
    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    string m_Surf;
    UINT32 m_Aperture;
    UINT32 m_WriteBack;
    UINT32 m_Ilwalidate;
    UINT32 m_Mode;
    Trace3DTest *m_Test;
    GpuSubdevice *m_GpuSubdev;
};

//------------------------------------------------------------------------------
// TraceOpRelocSurfaceProperty - Relocate properties for surfaces
//
class TraceOpRelocSurfaceProperty : public TraceOp
{
public:
    TraceOpRelocSurfaceProperty
    (
        UINT32 offset,
        UINT32 mask_out,
        UINT32 mask_in,
        bool   is_add,
        string &property,
        const string &surfaceName,
        SurfaceType surface_type,
        TraceModule *trace_module,
        bool   swap,
        bool   progZtAsCt0,
        TraceModule::SubdevMaskDataMap *sub_dev_map
    );
    virtual ~TraceOpRelocSurfaceProperty();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    UINT32 m_Offset;
    UINT32 m_MaskOut;
    UINT32 m_MaskIn;
    bool   m_IsAdd;
    string m_Property;
    string m_SurfaceName;
    SurfaceType m_SurfaceType;
    TraceModule *m_TraceModule;
    bool   m_Swap;
    bool   m_ProgZtAsCt0;
    TraceModule::SubdevMaskDataMap *m_SubDevMap;
};

//-----------------------------------------------------------------------------
// TraceOpAllocSurface - Operation for allocation buffers that are created
//                       by the ALLOC_SURFACE trace command.
class TraceOpAllocSurface : public TraceOp
{
public:
    TraceOpAllocSurface
    (
        SurfaceTraceModule *module,
        Trace3DTest *test,
        GpuTrace *trace,
        TraceFileMgr *traceFileMgr
    );
    virtual ~TraceOpAllocSurface();

    virtual RC Run();
    virtual void Print() const;
    virtual TraceOpType GetType() const { return m_Type; }

    void GetTraceopProperties( const char * * pName,
                               map<string,string> & properties ) const;
    MdiagSurf* GetParameterSurface() const { return m_Module->GetParameterSurface(); }

private:
    void SetType(const TraceOpType type) { m_Type = type; }
    RC EscWriteAllocSurfaceInfo();

    SurfaceTraceModule *m_Module;
    Trace3DTest *m_Test;
    TraceFileMgr *m_TraceFileMgr;
    TraceOpType m_Type = TraceOpType::Unknown;
};

//-----------------------------------------------------------------------------
// TraceOpFreeSurface - Operation for deleting buffers that are created
//                        by the ALLOC_SURFACE trace command.
class TraceOpFreeSurface : public TraceOp
{
public:
    TraceOpFreeSurface
    (
        TraceModule *module,
        Trace3DTest *Test,
        bool dumpBeforeFree
    );
    virtual ~TraceOpFreeSurface();

    virtual RC Run();
    virtual void Print() const;
    virtual TraceOpType GetType() const { return TraceOpType::FreeSurface; }

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceModule *m_Module;
    Trace3DTest *m_Test;

    // Dump the writable buffer before freeing if -dump_all_buffer_after_test is specified.
    // Writable ALLOC_SURFACE buffers which are freed during the test and have no check method
    // won't be dumped when option -dump_all_buffer_after_test is specified.
    // This flag is to handle this kind of buffer, dump them before free.
    bool m_DumpBeforeFree;
};

//-----------------------------------------------------------------------------
// TraceOpCheckDynSurf - Operation for checking dynamic surface/view.
//                       Surface check parameters are defined by ALLOC_SURFACE.
class TraceOpCheckDynSurf : public TraceOp
{
public:
    TraceOpCheckDynSurf
    (
        TraceModule* module,
        Trace3DTest* test,
        const string& sufixName,
        const vector<TraceChannel*>& wfiChannels
    );
    virtual ~TraceOpCheckDynSurf();

    virtual RC Run();
    virtual void Print() const;
    virtual TraceOpType GetType() const { return TraceOpType::CheckDynamicSurface; }

    void GetTraceopProperties( const char * * pName,
                               map<string,string> & properties ) const;
private:
    TraceModule* m_Module;
    Trace3DTest* m_Test;
    string       m_SufixName; // Same surface can be checked multiple times
                              // Append sufix name to avoid name conflict
    vector<TraceChannel*> m_WfiChannels; // Idle channels before crc check
};

class TraceOpDynTex : public TraceOp
{
public:
    TraceOpDynTex
    (
        TraceModule *target,
        string surfname,
        TextureMode mode,
        UINT32 index,
        bool no_offset,
        bool no_surf_attr,
        UINT32 peer,
        bool center_spoof,
        TextureHeader::HeaderFormat headerFormat,
        bool l1_promote
    );
    virtual ~TraceOpDynTex();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceModule *m_Target;
    string      m_SurfName;
    TextureMode m_Mode;
    UINT32      m_Index;
    bool        m_NoOffset;
    bool        m_NoSurfAttr;
    UINT32      m_Peer;
    bool        m_CenterSpoof;
    TextureHeader::HeaderFormat m_TextureHeaderFormat;
    bool        m_L1Promote;
};

class TraceOpFile : public TraceOp
{
public:
    TraceOpFile
    (
        TraceModule *target,
        UINT32 offset,
        TraceModule *module,
        UINT32 array_index,
        UINT32 lwbemap_face,
        UINT32 miplevel,
        UINT32 x,
        UINT32 y,
        UINT32 z,
        UINT32 peerNum,
        UINT64 mask_out,
        UINT64 mask_in,
        bool swap
    );
    virtual ~TraceOpFile();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceModule *m_Target;
    UINT32 m_Offset;
    TraceModule *m_Module;
    UINT32 m_ArrayIndex;
    UINT32 m_LwbemapFace;
    UINT32 m_Miplevel;
    UINT32 m_X;
    UINT32 m_Y;
    UINT32 m_Z;
    UINT32 m_PeerNum;
    UINT64 m_MaskOut;
    UINT64 m_MaskIn;
    bool m_Swap;
};

class TraceOpSurface : public TraceOp
{
public:
    TraceOpSurface
    (
        TraceModule *target,
        UINT32 offset,
        string &surf,
        UINT32 array_index,
        UINT32 lwbemap_face,
        UINT32 miplevel,
        UINT32 x,
        UINT32 y,
        UINT32 z,
        UINT32 peerNum,
        GpuDevice *gpudev,
        UINT64 mask_out,
        UINT64 mask_in,
        bool swap
    );
    virtual ~TraceOpSurface();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceModule *m_Target;
    UINT32 m_Offset;
    string m_Surf;
    UINT32 m_ArrayIndex;
    UINT32 m_LwbemapFace;
    UINT32 m_Miplevel;
    UINT32 m_X;
    UINT32 m_Y;
    UINT32 m_Z;
    UINT32 m_PeerNum;
    GpuDevice *m_GpuDev;
    UINT64 m_MaskOut;
    UINT64 m_MaskIn;
    bool m_Swap;
};

//-----------------------------------------------------------------------------
// TraceOpSetChannel: enable or disable indicated channel
class TraceOpSetChannel : public TraceOp
{
public:
    TraceOpSetChannel
    (
        TraceChannel * pTraceChannel,
        bool           enable
    );
    virtual ~TraceOpSetChannel();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceChannel *m_pTraceChannel;
    bool m_SetChEnable;
};

//-----------------------------------------------------------------------------
// TraceOpEvent: handle different kinds of EVENT from ACE2.0 traces
class TraceOpEvent : public TraceOp
{
public:
    TraceOpEvent
    (
        Trace3DTest * test,
        string        name
    );
    virtual ~TraceOpEvent();

    virtual RC Run() = 0;
    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

    Trace3DTest *GetTest() const { return m_Test; }

protected:
    Trace3DTest *m_Test;
    string m_Name;
};

class TraceOpEventMarker : public TraceOpEvent
{
public:
    TraceOpEventMarker
    (
        Trace3DTest * test,
        string        name,
        string        type,
        TraceChannel *pChannel,
        bool          afterTraceEvent
    );
    virtual ~TraceOpEventMarker();

    virtual RC Run();
    virtual void Print() const;

    virtual void GetTraceopProperties(const char **pName,
        map<string,string> &properties) const;

    void AddTraceOperations(TraceOp* traceOp) { m_TraceOperations.push_back(traceOp); }

private:
    string m_Type;

    vector<TraceOp*> m_TraceOperations;

    TraceChannel *m_pChannel;

    bool m_AfterTraceEvent;
};

class TraceOpEventPMStart : public TraceOpEvent
{
public:
    TraceOpEventPMStart
    (
        Trace3DTest *  test,
        TraceChannel * channel,
        string         name
    );
    virtual ~TraceOpEventPMStart();

    virtual RC Run();

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceChannel * m_pTraceChannel;
};

class TraceOpEventPMStop : public TraceOpEvent
{
public:
    TraceOpEventPMStop
    (
        Trace3DTest *  test,
        TraceChannel * channel,
        string         name
    );
    virtual ~TraceOpEventPMStop();

    virtual RC Run();

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceChannel * m_pTraceChannel;
};

class TraceOpEventSyncStart : public TraceOpEvent
{
public:
    TraceOpEventSyncStart
    (
        Trace3DTest *  test,
        vector<TraceChannel*> channels,
        vector<TraceSubChannel*> subChannels,
        string name
    );

    TraceOpEventSyncStart
    (
        Trace3DTest *  test,
        string name
    );

    virtual ~TraceOpEventSyncStart();

    virtual RC Run();

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

protected:
    virtual void PrepareSync();
    virtual RC WaitAllTests();
    virtual void WaitForIdle();
    virtual RC WriteRelease();
    virtual RC WriteHostAcquire();
    virtual string GetEventName() const;
    void WaitForIdleImpl();
    void RegisterChannels();

    vector<TraceChannel*> m_pChannels;
    vector<TraceSubChannel*> m_pSubChannels;
    bool m_Sync;
};

class TraceOpEventSyncStop : public TraceOpEvent
{
public:
    TraceOpEventSyncStop
    (
        Trace3DTest *  test,
        vector<TraceChannel*> channels,
        vector<TraceSubChannel*> subChannels,
        string         name
    );

    TraceOpEventSyncStop
    (
        Trace3DTest *  test,
        string name
    );

    virtual ~TraceOpEventSyncStop();

    virtual RC Run();

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

protected:
    virtual RC WaitAllTests();
    virtual void WaitForIdle();
    void WaitForIdleImpl();
    virtual string GetEventName() const;
    void RegisterChannels();

    vector<TraceChannel*> m_pChannels;
    vector<TraceSubChannel*> m_pSubChannels;
    bool m_Sync;
};

class TraceOpEventSyncPMStart : public TraceOpEventSyncStart
{
public:
    TraceOpEventSyncPMStart
    (
        Trace3DTest *  test,
        TraceChannel * channel,
        TraceSubChannel * subchannel,
        string         name
    );
    virtual ~TraceOpEventSyncPMStart();

    virtual RC Run();

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

protected:
    void WaitForIdle();
    string GetEventName() const;

private:
    void StartPerf();
    RC WritePmTrigger();
};

class TraceOpEventSyncPMStop : public TraceOpEventSyncStop
{
public:
    TraceOpEventSyncPMStop
    (
        Trace3DTest *  test,
        TraceChannel * channel,
        TraceSubChannel * subchannel,
        string         name
    );
    virtual ~TraceOpEventSyncPMStop();

    virtual RC Run();

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

protected:
    void WaitForIdle();
    string GetEventName() const;

private:
    void StopPerf();
    RC WriteWFI();
    RC WritePmTrigger();
    RC WriteRelease();
    RC WriteHostAcquire();
    RC WriteNop();
};

class TraceOpEventDump : public TraceOpEvent
{
public:
    TraceOpEventDump
    (
        Trace3DTest*  test,
        TraceChannel* channel,
        GpuTrace*     trace,
        string        eventName,
        string        moduleName
    );
    virtual ~TraceOpEventDump();

    virtual RC Run();

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceChannel *m_Channel;
    GpuTrace *m_Trace;
    string m_ModuleName;
    static map<string, map<string, UINT32> > s_CountMap;
};

//-----------------------------------------------------------------------------
// TraceOpAllocChannel: allocate channel during running trace
class TraceOpAllocChannel : public TraceOp
{
public:
    TraceOpAllocChannel
    (
        Trace3DTest* pTest,
        TraceChannel* pTraceChannel
    );
    virtual ~TraceOpAllocChannel();

    virtual RC Run();

    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    Trace3DTest*  m_Test;
    TraceChannel* m_pTraceChannel;
};

//-----------------------------------------------------------------------------
// TraceOpAllocSubChannel: allocate subchannel during running trace
class TraceOpAllocSubChannel : public TraceOp
{
public:
    TraceOpAllocSubChannel
    (
        Trace3DTest* pTest,
        TraceChannel* pTraceChannel,
        TraceSubChannel* pTraceSubChannel
    );
    virtual ~TraceOpAllocSubChannel();

    virtual RC Run();

    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    Trace3DTest*     m_Test;
    TraceChannel*    m_pTraceChannel;
    TraceSubChannel* m_pTraceSubChannel;
};

//-----------------------------------------------------------------------------
// TraceOpFreeChannel: free channel during running trace
class TraceOpFreeChannel : public TraceOp
{
public:
    TraceOpFreeChannel
    (
        Trace3DTest* pTest,
        TraceChannel* pTraceChannel
    );
    virtual ~TraceOpFreeChannel();

    virtual RC Run();

    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    Trace3DTest*     m_Test;
    TraceChannel*    m_pTraceChannel;
};

//-----------------------------------------------------------------------------
// TraceOpFreeSubChannel: free subchannel during running trace
class TraceOpFreeSubChannel : public TraceOp
{
public:
    TraceOpFreeSubChannel
    (
        Trace3DTest* pTest,
        TraceChannel* pTraceChannel,
        TraceSubChannel* pTraceSubChannel
    );
    virtual ~TraceOpFreeSubChannel();

    virtual RC Run();

    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    TraceChannel*    m_pTraceChannel;
    TraceSubChannel* m_pTraceSubChannel;
};

//-----------------------------------------------------------------------------
// TraceOpUtlRunFunc: embed the utl function under above module during running trace
class TraceOpUtlRunFunc : public TraceOp
{
public:
    TraceOpUtlRunFunc
    (
        Trace3DTest * pTest,
        string expression,
        string scope
    );
    virtual ~TraceOpUtlRunFunc();

    virtual RC Run();

    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    Trace3DTest*    m_pTest;
    string m_Expression;
    string m_ScopeName;
};

//-----------------------------------------------------------------------------
// TraceOpUtlRunScript: import the utl module during running trace
class TraceOpUtlRunScript : public TraceOp
{
public:
    TraceOpUtlRunScript
    (
        Trace3DTest * pTest,
        string scriptPath,
        string scriptArgs,
        string scopeName
    );
    virtual ~TraceOpUtlRunScript();

    virtual RC Run();

    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    Trace3DTest*    m_pTest;
    string          m_ScriptPath;
    string          m_ScriptArgs;
    string          m_ScopeName;
};

//-----------------------------------------------------------------------------
// TraceOpUtlRunInline: embed the inline utl script during running trace
class TraceOpUtlRunInline : public TraceOp
{
public:
    TraceOpUtlRunInline
    (
        Trace3DTest * pTest,
        string expression,
        string scopeName
    );
    virtual ~TraceOpUtlRunInline();

    virtual RC Run();

    virtual void Print() const;

    virtual void GetTraceopProperties( const char * * pName,
        map<string,string> & properties ) const;

private:
    Trace3DTest*    m_pTest;
    string m_Expression;
    string m_ScopeName;
};

#endif // !INCLUDED_TRACEOP_H
