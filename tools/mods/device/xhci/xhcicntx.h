/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// This is the definition of all Context classes
// including Device Context, InputContext, Device Mods's EP context,
// Device Context Base Address Array and Stream Context

#ifndef INCLUDED_XHCICNTX_H
#define INCLUDED_XHCICNTX_H

#ifndef INCLUDED_TEE_H
    #include "core/include/tee.h"
#endif

#define XHCI_LENGTH_DCBAA_ENTRY                         8

#define XHCI_ALIGNMENT_DEV_CONTEXT                      64
#define XHCI_ALIGNMENT_DEV_CONTEXT_ARRAY                64
#define XHCI_ALIGNMENT_STREAM_CONTEXT                   16
#define XHCI_ALIGNMENT_STREAM_ARRAY                     16
#define XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY                 256
#define XHCI_MAX_SIZE_STREAM_CONTEXT_ARRAY              256
#define XHCI_SIZE_STREAM_CONTEXT                        16

#define XHCI_SLOT_CONTEXT_DW0_ROUTE_STRING              19:0
#define XHCI_SLOT_CONTEXT_DW0_SPEED                     23:20
#define XHCI_SLOT_CONTEXT_DW0_MTT                       25:25
#define XHCI_SLOT_CONTEXT_DW0_HUB                       26:26
#define XHCI_SLOT_CONTEXT_DW0_CNTXTENTRIES              31:27

#define XHCI_SLOT_CONTEXT_DW1_MAX_EXIT_LATENCY          15:0
#define XHCI_SLOT_CONTEXT_DW1_ROOT_PORT_NUM             23:16
#define XHCI_SLOT_CONTEXT_DW1_NUM_OF_PORT               31:24

#define XHCI_SLOT_CONTEXT_DW2_TT_HUB_SLOTID             7:0
#define XHCI_SLOT_CONTEXT_DW2_TT_PORT_NUM               15:8
#define XHCI_SLOT_CONTEXT_DW2_TTT                       17:16
#define XHCI_SLOT_CONTEXT_DW2_INTERRUPTER_TARGET        31:22

#define XHCI_SLOT_CONTEXT_DW3_USB_ADDR                  7:0
#define XHCI_SLOT_CONTEXT_DW3_SLOT_DTATE                31:27

#define XHCI_EP_CONTEXT_DW0_EP_STATE                    3:0
#define EP_STATE_DISABLED                               0
#define EP_STATE_RUNNING                                1
#define EP_STATE_HALTED                                 2
#define EP_STATE_STOPPED                                3
#define EP_STATE_ERROR                                  4

#define XHCI_EP_CONTEXT_DW5_SQR_NUM                     31:27

#define XHCI_EP_CONTEXT_DW0_MULT                        9:8
#define XHCI_EP_CONTEXT_DW0_MAX_PSTREAMS                14:10
#define XHCI_EP_CONTEXT_DW0_LSA                         15:15
#define XHCI_EP_CONTEXT_DW0_INTERVAL                    23:16

#define XHCI_EP_CONTEXT_DW1_FE                          0:0
#define XHCI_EP_CONTEXT_DW1_CREE                        2:1
#define XHCI_EP_CONTEXT_DW1_EP_TYPE                     5:3
#define XHCI_EP_CONTEXT_DW1_HID                         7:7
#define XHCI_EP_CONTEXT_DW1_MAX_BURST_SIZE              15:8
#define XHCI_EP_CONTEXT_DW1_MPS                         31:16

#define XHCI_EP_CONTEXT_DW2_DCS                         0:0
#define XHCI_EP_CONTEXT_DW2_DEQ_POINTER_LO              31:4

#define XHCI_EP_CONTEXT_DW3_DEQ_POINTER_HI              31:0

#define XHCI_EP_CONTEXT_DW4_AVERAGE_TRB_LENGTH          15:0
#define XHCI_EP_CONTEXT_DW4_MAX_ESIT_PAYLOAD           31:16

#define XHCI_STREAM_CONTEXT_DW0_DCS                      0:0
#define XHCI_STREAM_CONTEXT_DW0_SCT                      3:1
#define XHCI_STREAM_CONTEXT_DW0_TR_DEQ_LOW              31:4
#define XHCI_STREAM_CONTEXT_DW1_TR_DEQ_HI               31:0

typedef struct _SlotContext
{
    UINT32 DW0;
    UINT32 DW1;
    UINT32 DW2;
    UINT32 DW3;
    UINT32 RSVD0;
    UINT32 RSVD1;
    UINT32 RSVD2;
    UINT32 RSVD3;
} SlotContext;

typedef struct _EndPContext
{
    UINT32 DW0;
    UINT32 DW1;
    UINT32 DW2;
    UINT32 DW3;
    UINT32 DW4;
    UINT32 RSVD0;
    UINT32 RSVD1;
    UINT32 RSVD2;
} EndPContext;

typedef struct _StreamContext
{
    UINT32 DW0;
    UINT32 DW1;
    UINT32 RSVD0;
    UINT32 RSVD1;
}StreamContext;

enum XhciEndPointType
{
    EP_TYPE_NA          = 0,
    EP_TYPE_ISOCH_OUT   = 1,
    EP_TYPE_BULK_OUT    = 2,
    EP_TYPE_INTR_OUT    = 3,
    EP_TYPE_CONTROL     = 4,
    EP_TYPE_ISOCH_IN    = 5,
    EP_TYPE_BULK_IN     = 6,
    EP_TYPE_INTR_IN     = 7
};

class DeviceContext
{
public:
    DeviceContext(bool Is64Byte);
    virtual ~ DeviceContext();

    // allocate MEM and initialize it to 0
    RC Init(UINT32 NumEntries = 32, UINT32 AddrBit = 32, UINT32 BaseOffset = 0);
    // opposite to Init(), clear all memory to 0
    RC ClearOut();

    // could be fill into Dev context base address array
    RC GetBaseAddr(PHYSADDR * ppThis);

    // fields retrieve from Slot Context
    RC GetContextEntries(UINT32 * pCntxtEntries);
    RC GetHub(bool * pIsHub);
    RC GetUsbDevAddr(UINT08 * pAddr);
    RC GetSpeed(UINT08 * pSpeed);
    RC GetRootHubPort(UINT08 * pPort);
    RC GetRouteString(UINT32 * pRouteString);

    // fields retrieve from Endpoint Context
    RC GetMps(UINT08 EndpNum, bool IsOut, UINT32 * pMps);
    RC GetEpState(UINT08 EndpNum, bool IsOut, UINT08 * pEpState);
    RC GetEpType(UINT08 EndpNum, bool IsOut, UINT08 * pEpType);
    RC GetEpRawData(UINT08 EndpNum, bool IsOut, EndPContext * pEpContext);

    // get DCIs of all Endpoint in running state
    RC GetRunningEps(vector<UINT08> * pvEp);

    virtual RC PrintInfo(Tee::Priority PriRaw = Tee::PriNormal, Tee::Priority PriInfo = Tee::PriLow);
    RC PrintEpContext(UINT08 StartIndex = 1,            // 0 is for SlotContext
                      Tee::Priority PriRaw = Tee::PriNormal,
                      Tee::Priority PriInfo = Tee::PriLow);

    static RC DciToEp(UINT08 Dci, UINT08 * pEp, bool * pIsDataOut);
    static RC EpToDci(UINT08 Ep, bool IsDataOut, UINT08 * pDci, bool IsEp0Dir = false);

    RC ToVector(vector<UINT32> * pvData);
    RC FromVector(vector<UINT32> * pvData, bool IsForceFullCopy = false,  bool IsSkipInputControl = false);
protected:
    // this should be called by all above APIs
    virtual RC GetSlotContextBase(SlotContext ** ppContext);
    virtual RC GetSpContextBase(UINT32 EndPointIndex, bool IsOut, EndPContext ** ppContext);

    // extra bytes as prefix to main data structure, used by child classes
    UINT32  m_ExtraByte;

    UINT32  m_NumEntry;
    bool    m_IsInit;
    bool    m_Is64Byte;
    // could be 32 * 32 or 64 * 32 Byte length depending on CSZ of HCCPARAMS
    void *  m_pData;
    UINT32  m_AlignOffset;
};

class InputContext: public DeviceContext
{
public:
    InputContext(bool Is64Byte);
    virtual ~InputContext();

    RC Init(UINT32 BaseOffset = 0);    // always create full length Device context w/ 32 entries

    RC SetInuputControl(UINT32 DropFlags, UINT32 AddFlags);

    // fields set to Slot Context
    RC SetContextEntries(UINT32 CntxtEntries);
    RC SetRouteString(UINT32 RouteString);
    RC SetSpeed(UINT08 Speed);
    RC SetRootHubPort(UINT08 PortNum);
    RC SetHubBit(bool IsHub);
    RC SetMaxExitLatency(UINT16 MaxLatMs);
    RC SetIntrTarget(UINT16 IntrTarget);
    RC SetTtt(UINT08 Ttt);
    RC SetMtt(bool Mtt);
    RC SetNumOfPorts(UINT08 NumPort);

    // fields set to Endpoint Context
    RC InitEpContext(UINT32 EpNum, bool IsOut,
                     UINT08 Type,
                     UINT08 Interval = 0,
                     UINT08 MaxBurstSize = 0,
                     UINT08 MaxPStreams = 0,
                     UINT08 Mult = 0,
                     UINT08 Cerr = 3,
                     UINT08 Fe = 0,
                     bool IsLinearStreamArray = true,
                     bool IsHostInitiateDisable = false);
    RC SetEpMps(UINT32 EpNum, bool IsOut, UINT32 Mps);
    RC SetEpMult(UINT32 EpNum, bool IsOut, UINT32 Mult);
    RC SetEpMaxBurstSize(UINT32 EpNum, bool IsOut, UINT32 MaxBurstSize);
    RC SetEpDeqPtr(UINT32 EpNum, bool IsOut, PHYSADDR pPa);
    RC SetEpDcs(UINT32 EpNum, bool IsOut, bool Dcs);
    RC SetEpBwParams(UINT32 EpNum, bool IsOut, UINT16 AverageTrbLength, UINT16 MaxESITPayload);

    virtual RC PrintInfo(Tee::Priority PriRaw = Tee::PriNormal, Tee::Priority PriInfo = Tee::PriLow);
protected:
    RC GetInpCtrlContextBase(UINT32 ** ppData);
};

class DevModeEpContexts: public DeviceContext
{   // for used by device mode
public:
    DevModeEpContexts();
    virtual ~DevModeEpContexts();

    RC BindXferRing(UINT08 EndpNum, bool IsOut, PHYSADDR pXferRing);
    // fields set to Endpoint Context
    RC InitEpContext(UINT32 EpNum, bool IsOut,
                     UINT08 Type,
                     UINT08 EpState,
                     UINT32 Mps,
                     bool   Dcs,
                     UINT08 Interval = 0,
                     UINT08 MaxBurstSize = 0,
                     UINT08 MaxPStreams = 0,
                     UINT08 Mult = 0,
                     UINT08 Cerr = 3,
                     UINT08 Fe = 0,
                     bool IsLinearStreamArray = true,
                     bool IsHostInitiateDisable = false);
    RC SetEpState(UINT32 EpNum, bool IsOut, UINT08 EpState);
    RC SetSeqNum(UINT32 EpNum, bool IsOut, UINT08 SeqNum);

protected:
    virtual RC GetSlotContextBase(SlotContext ** ppContext) {Printf(Tee::PriNormal, "Not Implemented! \n"); return RC::SOFTWARE_ERROR;}
    virtual RC GetSpContextBase(UINT32 EndpNum, bool IsOut, EndPContext ** ppContext);
};

class DeviceContextArray
{// see also Spec 6.1
public:
    DeviceContextArray();
    virtual ~DeviceContextArray();

    RC Init(UINT32 AddrBit = 32);
    RC GetBaseAddr(PHYSADDR * ppArray);

    RC DeleteEntry(UINT32 SlotId);
    RC GetEntry(UINT32 SlotId, DeviceContext ** ppDevContext, bool IsSilent = false);
    RC SetEntry(UINT32 SlotId, DeviceContext * pDevContext);
    RC SetScratchBufArray(PHYSADDR pPa);
    RC GetActiveSlots(vector<UINT08> * pvSlots);

    RC PrintInfo(Tee::Priority Pri = Tee::PriNormal, UINT08 SlotId = 0);
private:
    // to store pointers to the original DeviceContext objects
    DeviceContext *  m_DevCntxtArray[XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY];
    void * m_pData; // 64 bit aligned, max 256 entries
};

// prototype of Primary Stream Context and Secondary Stream Context Arrays
class StreamContextArray
{
public:
    StreamContextArray();
    virtual ~StreamContextArray();

    RC Init(UINT16 Size, UINT32 MemBoundary = 0, UINT32 AddrBit = 32);
    RC GetBaseAddr(PHYSADDR * ppArray);

    RC SetupContext(UINT32 Index, bool IsDcs, UINT08 SCT, PHYSADDR pPa);
    RC ClearContext(UINT32 Index);
    RC GetContext(UINT32 Index, StreamContext * pCnxt);

    UINT32 GetSize();
    bool EmptyCheck(UINT32 Index = 0);

    static RC GenSct(UINT08 *pSct, bool IsSecondary, bool IsRingPtr = true, UINT32 ArraySize = 0);
    static RC ParseContext(StreamContext * pCnxt, bool * pIsDcs, UINT08 * pSct, PHYSADDR * ppPa);

    RC PrintInfo(Tee::Priority Pri = Tee::PriNormal);
private:
    RC GetEntryBase(UINT32 Index, UINT32 ** ppData);

    UINT32  m_NumEntry;
    void *  m_pData;
};

class StreamContextArrayWrapper
{
public:
    StreamContextArrayWrapper(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, bool IsLinear);
    virtual ~StreamContextArrayWrapper();

    // framework manipulation
    RC Init(vector<UINT32> * pvSizes);            // all in one setup
    RC Init(UINT32 Size, UINT32 AddrBit = 32);
    RC SetupEntry(UINT32 Index, UINT32 Size);   // setup entries one by one, size = 0 means remove entry
    RC DelEntry(UINT32 Index);

    // content manipulation
    RC SetRing(UINT32 StreamId, PHYSADDR pXferRing, bool IsDcs);
    //RC GetRing(UINT32 StreamId, PHYSADDR * ppXferRing);
    bool IsThatMe(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut);
    bool IsThatMe(UINT08 SlotId);

    RC GetBaseAddr(PHYSADDR * ppArray);         // return base address of Primary Stream Context Array
    RC PrintInfo(Tee::Priority Pri = Tee::PriNormal);

    RC ParseStreamId(UINT32 StreamId, UINT32 * pPsid, UINT32 * pSsid);

private:
    UINT32 m_AddrBit;   // 32 or 64

    StreamContextArray * m_pPrimaryArray;
    StreamContextArray * m_pSecondaryArray[XHCI_MAX_SIZE_STREAM_CONTEXT_ARRAY];        // the maxium size of secondary array is 256

    bool    m_IsLinear;     // if it's true, m_pSecondaryArray will be all NULL
    bool    m_IsInit;

    // each StreamContextArrayWrapper object belongs to a Endpoint
    // each Endpoint can have only one StreamContextArrayWrapper object
    UINT08  m_SlotId;
    UINT08  m_EndPoint;
    bool    m_IsDataOut;
};

// Port Bandwidth Context is only used by GetPortBandwidth command, we don't define it here

#endif
