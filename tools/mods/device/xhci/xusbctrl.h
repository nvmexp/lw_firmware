/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_XUSBMDEV_H
#define INCLUDED_XUSBMDEV_H

#ifndef INCLUDED_XHCICOMN_H
    #include "xhcicomn.h"
#endif
#ifndef INCLUDED_XUSBREG_H
    #include "xusbreg.h"
#endif
#ifndef INCLUDED_XHCITRB_H
    #include "xhcitrb.h"
#endif
#ifndef INCLUDED_XHCIRING_H
    #include "xhciring.h"
#endif
#ifndef INCLUDED_XHCICNTX_H
    #include "xhcicntx.h"
#endif
#ifndef INCLUDED_USBDEXT_H
    #include "usbdext.h"
#endif
#ifndef INCLUDE_BUFBUILD_H
    #include "device/include/bufbuild.h"
#endif
#ifndef INCLUDED_JSCRIPT_h
    #include "core/include/jscript.h"
#endif

#ifndef INCLUDED_STL_VECTOR
    #include <vector>
    #define INCLUDED_STL_VECTOR
#endif

#define XUSB_DEVICE_STATE_DEFAULT               0
#define XUSB_DEVICE_STATE_ADDRESSED             1
#define XUSB_DEVICE_STATE_CONFIGURED            2
#define XUSB_DEVICE_STATE_CLASS_CONFIGURED      3

#define XUSB_BULK_TRANSFER_STATE_IDLE           0
#define XUSB_BULK_TRANSFER_STATE_CBW_RECEIVED   1
#define XUSB_BULK_TRANSFER_STATE_DATA_XFERED    2

#define XUSB_DEFAULT_EVENT_RING_SIZE            32
#define XUSB_DEFAULT_TRANSFER_RING_SIZE         250

#define XUSB_JS_CALLBACK_CONN                   0
#define XUSB_JS_CALLBACK_DISCONN                1
#define XUSB_JS_CALLBACK_U3U0                   2

class XusbController:public XhciComnDev
{
public:
    //==========================================================================
    // Natives
    //==========================================================================
    XusbController();
    virtual ~ XusbController();
    const char* GetName();

    //==========================================================================
    // info
    //==========================================================================
    virtual RC PrintReg( Tee::Priority PriRaw = Tee::PriNormal ) const;

    //! \brief Print misc data structures
    //--------------------------------------------------------------------------
    RC PrintData(UINT08 Type, UINT08 Index = 0);
    //! \brief Print list of all existing Rings
    //--------------------------------------------------------------------------
    RC PrintRingList(Tee::Priority Pri = Tee::PriNormal, bool IsPrintDetail = false);
    //! \brief Print content of specified ring
    //--------------------------------------------------------------------------
    RC PrintRing(UINT32 Uuid, Tee::Priority Pri = Tee::PriNormal);
    RC PrintContext(Tee::Priority Pri = Tee::PriNormal);

    //! \brief Initialize Transfer Ring for specified endpoint
    //! \param [in] EpNum : Endpoint Number
    //! \param [in] IsDataOut : Data Direction
    //! \param [in] IsEp0HasDir : if EP0 support bi-direction, should be true for dev mode
    //! \param [out] ppPhyBase : Physical address of created Transfer Ring
    RC InitXferRing(UINT08 EpNum, 
                    bool IsDataOut, 
                    bool IsEp0HasDir,
                    PHYSADDR * ppPhyBase);

    //! \brief Send out Data and Status as response to host's requests
    //! \param [in] pData : pointer to the data buffer
    //! \param [in] Length : length of data buffer
    //! \param [in] CtrlSeqNum : Control Sequence Number from host's Setup TRB
    RC SentDataStatus(void* pData, UINT32 Length, UINT16 CtrlSeqNum);

    //! \brief Handle Port Reset and Warm Port Reset request from host
    //! Init internal state machine, clear Device Address, remove all Transfer Rings
    //! Init the Endpoint Context for EP0, create Transfer Ring for EP0
    RC PortReset();

    //! \brief Send out status as response to host's requests
    //! \param [in] CtrlSeqNum : Control Sequence Number from host's Setup TRB
    //! \param [in] IsHostToDev : whether the direction of Status is from host to device
    RC SentStatus(UINT16 CtrlSeqNum, bool IsHostToDev = true);
    //! \brief Handle the Port Status Change Evnet
    RC HandlerPortStatusChange();
    //! \brief Get the handle of the specified Transfer Ring
    //! \param [in] EndPoint : Endpoint Number
    //! \param [in] IsDataOut : Data Out / Data In
    //! \param [out] ppRing : the pointer to the pointer of Transfer Ring.
    //! \param [in] StreamId : The target Stream ID if stream is enabled
    RC FindXferRing(UINT08 EndPoint, bool IsDataOut, TransferRing ** ppRing, UINT16 StreamId=0);
    //! \brief Ring the doorbell to notify HW a TRB is ready for tranfer
    //! \param [in] EndPoint : Endpoint Number
    //! \param [in] IsDataOut : Data Out / Data In
    //! \param [in] StreamIdCtrlSeqNum : The target Stream ID or CtrlSeqNum 
    RC RingDoorBell(UINT08 Endpoint, bool IsDataOut, UINT16 StreamIdCtrlSeqNum=0);
    //! \brief Ring doorbell for all the EPs which has pending TRB in its Transfer Ring
    RC RingDoorbells();
    //! \brief Setup TD with specified parameters
    //! \param [in] EndPoint : Endpoint Number
    //! \param [in] IsDataOut : Data Out / Data In
    //! \param [in] pFrags : The pointer to the data buffer's fragment list
    //! \param [in] IsIsoch : If yes, ISO TRB will be used instead of Normal TRB
    //! \param [in] IsDbRing : Whether doorbell should be rung to notify HW
    //! \param [in] StreamId : The target Stream ID if stream is enabled
    //! \param [in] IsSia : For Isoch, whether to set SIA flag (Start Isoch ASAP)
    //! \param [in] FrameId : For Isoch, target frame ID
    //! \param [in] TrbNum : Number of TRB to be enabled, should < pFrags->size(), for underrun test cases
    //! \param [in] SleepUs : Number of US to sleep after issue enabled TRBs, for underrun test cases
    RC SetupTd(UINT08 Endpoint,
               bool IsDataOut,
               MemoryFragment::FRAGLIST* pFrags,
               bool IsIsoch=false,
               bool IsDbRing=false,
               UINT16 StreamId=0,
               bool IsSia = false,
               UINT16 FrameId = 0,
               UINT32 TrbNum = 0,
               UINT32 SleepUs = 50000);
    //! \brief Wait transfer completion for specified endpoint
    //! \param [in] EndPoint : Endpoint Number
    //! \param [in] IsDataOut : Data Out / Data In
    //! \param [in] TimeoutMs : the MS before timeout error
    //! \param [out] pCompCode : the pointer of Completion Code
    //! \param [in] isWaitAll : Whether to wait for completion for all pending TRBs 
    RC WaitXfer(UINT08 Endpoint, 
                bool IsDataOut, 
                UINT32 TimeoutMs= 2000, 
                UINT32 * pCompCode= NULL,
                bool isWaitAll= true);

    //! \brief Allow users to setup Device Descriptor from JS interface
    //! \param [in] pvData : the Descriptor data
    RC SetDeviceDescriptor(vector<UINT08> *pvData);
    //! \brief Allow users to setup reponse data for spcified SCSI command 
    //! \param [in] Cmd : the target SCSI command
    //! \param [in] pvData : the response data
    RC SetScsiResp(UINT08 Cmd, vector<UINT08> *pvData);
    //! \brief Allow users to setup BOS Descriptor from JS interface
    //! \param [in] pvData : the Descriptor data
    RC SetBosDescriptor(vector<UINT08> *pvData);
    //! \brief Allow users to setup Configuration Descriptor from JS interface
    //! \param [in] pvData : the Descriptor data
    RC SetConfigurationDescriptor(vector<UINT08> *pvData);
    //! \brief Allow users to setup String Descriptor from JS interface
    //! Users can setup multiple String Descriptor by repeatedly calling this
    //! \param [in] pvData : the Descriptor data
    RC SetStringDescriptor(vector<UINT08> *pvData);

    //! \brief Handle all the new Event
    //! note: transfer events are not handled here, see also WaitXfer()
    //! \param [in] pEventTrb : pointer to the new Event TRB retreived from Event Ring
    RC EventHandler(XhciTrb* pEventTrb);

    //! \brief Handle Setup Events
    //! \param [in] pEventTrb : pointer to the new Event TRB retreived from Event Ring
    //! \param [in] IsHandled : indicate whether the event has been handled
    RC HandlerSetupEvent(XhciTrb* pEventTrb, bool* IsHandled);

    //! \brief Setup the default size of Transfer Ring
    //! \param [in] XferRingSize : the size
    RC SetXferRingSize(UINT32 XferRingSize);
    //! \brief Setup the default size of Event Ring
    //! \param [in] EventRingSize : the size
    RC SetEventRingSize(UINT32 EventRingSize);

    //! \brief Get device's state
    //! \param [out] DeviceState : the device state
    //! \sa XUSB_DEVICE_STATE_DEFAULT
    RC GetDeviceState(UINT32* DeviceState);


    //! \brief the main function for XUSB 
    //! \param [in] SecToRun : the time before return in second
    RC DevCfg(UINT32 SecToRun = 0);

    //! \brief Get the MPS parameter of specified Endpoint 
    //! \param [in] EndPoint : Endpoint Number
    //! \param [in] IsDataOut : Data Out / Data In
    //! \param [out] pMps : the returned MPS
    RC GetEpMps(UINT08 Endpoint, bool IsDataOut, UINT32 * pMps);

    //! \brief Handle the newly arrived Event TRB, update Transfer Ring's DeqPtr
    //! \param [in] pTrb : the Event TRB to be processed
    virtual RC NewEventTrbHandler(XhciTrb* pTrb);
    //! \brief Update the Event Ring Deq Pointer
    //! \param [in] EventIndex : Index of Event Ring
    //! \param [in] pDeqPtr : the DeqPtr to be written
    //! \param [in] SegmengIndex :  NOT USED
    virtual RC UpdateEventRingDeq(UINT32 EventIndex, XhciTrb * pDeqPtr, UINT32 SegmengIndex);

    //! \brief return Ring's pointer for specified Ring UUID
    //! \param [in] Uuid : Index of Rings (0 is always Event Ring)
    //! \param [in] ppRing : the pointer to Ring
    //! \param [out] pMaxUuid :  max UUID
    RC FindRing(UINT32 Uuid, XhciTrbRing ** ppRing, UINT32 * pMaxUuid = NULL);
    //! \brief set TD size for ISO transfers
    //! \param [in] TdSize : the size
    RC SetIsoTdSize(UINT32 TdSize);
    //! \brief launch the ISOch trnafer engine
    //! \sa EngineBulk()
    RC EngineIsoch();

    //! \brief Enable / disable the buffer management parameters ( buffer alignment / offset )
    //! \param [in] IsEnable : if to enable
    RC EnBufParams(bool IsEnable);
    //! \brief Setup call back functions in JS
    //! \param [in] CbIndex : Callback function index
    //! \param [in] JSFunction : the JS function
    RC SetupJsCallback(UINT32 CbIndex, JSFunction * pCallbackFunc);
    //! \brief Setup buffer management parameters 
    RC SetBufParams(UINT32 SizeMin, UINT32 SizeMax,
                    UINT32 NumMin, UINT32 NumMax,
                    UINT32 OffMin, UINT32 OffMax, UINT32 OffStep,
                    UINT32 BDataWidth = 4, UINT32 Align = 4096, bool IsShuffle = true);
    //! \brief Setup retry times
    //! \param [in] Times : the retry times
    //! \param [Out] pTimesLeft : retry time left
    RC SetRetry(UINT32 Times, UINT32 * pTimesLeft = NULL);
    //! \brief Setup Bulk delay for oerrun/underrun test
    //! \param [in] DelayUs : the delay value
    RC SetBulkDelay(UINT32 DelayUs);
    //! \brief Setup data patterns for transfers 
    //! \param [in] pvData32 : the data pattern
    RC SetPattern32(vector<UINT32> * pvData32);

    //==========================================================================
    //  framework
    //==========================================================================
    RC Reset() {return OK;};
    RC PrintExtDevInfo(Tee::Priority = Tee::PriNormal) const {return OK;};
    RC InitExtDev() { return OK; }
    RC UninitExtDev() { return OK; }
    RC PrintState(Tee::Priority Pri = Tee::PriNormal) const;
    RC PrintAll();

protected:
    //==========================================================================
    //  framwork
    //==========================================================================
    RC InitBar();
    RC InitRegisters();
    RC UninitRegisters();
    RC InitDataStructures();
    RC UninitDataStructures();
    RC ToggleControllerInterrupt(bool IsEnable);
    RC GetIsr(ISR* Func) const;

    RC DeviceEnableInterrupts();
    RC DeviceDisableInterrupts();

    //==========================================================================
    //  helper functions to handle host requests
    //==========================================================================
    RC HandleSetAddress(UINT16 DeviceAddress, UINT16 CtrlSeqNum);
    RC HandleSetFeature(UINT16 FeatureSelector, UINT08 ReqType, UINT16 Index,UINT16 CtrlSeqNum);
    RC HandleClearFeature(UINT16 FeatureSelector, UINT08 ReqType, UINT16 Index,UINT16 CtrlSeqNum);
    RC HandleSetConfiguration(UINT08 Configuratiolwalue, UINT16 CtrlSeqNum);
    RC HandleGetDescriptor(UINT16 Value, UINT16 Index, UINT16 Length, UINT16 CtrlSeqNum);
    RC HandleGetMaxLun(UINT16 Length, UINT16 CtrlSeqNum);

    //! \brief parse specified EndPoint info and setup member variables
    RC EpParserHooker(UINT08 DevClass, EndpointInfo * pEpInfo);

    //! \brief Bulk transfer engine
    //! setup a Data TRB on Out Endpoint with IOC = 1 and wait for CBW to come
    //! parse CBW then call appropriate CmdHandler based on SCSI CMD type
    //! setup Data TRBs in In / Out endpoint according to the SCSI command
    //! wait for Transfer Event for Data TRBs
    //! setup & send TRB for CSW
    RC EngineBulk();

    //! \brief the handler of BULK command (SCSI command)
    //! \param [in] pBufCbw : the recieved CBW from host
    RC BulkCmdHandler(UINT08* pBufCbw);

    //! \brief Get optimised transfer length supported by one TRB
    UINT32 GetOptimisedFragLength();
    //! \brief Get response data for expecified SCSI command
    //! \param [in] Cmd : SCSI command
    //! \param [Out] ppData : pointer to response buffer
    //! \param [Out] pLength : response length
    RC GetScsiResp(UINT08 Cmd, void **ppData, UINT32 *pLength);
    // helper function to avoid Write and Clear bit get overwriten accidentally
    //! \brief Helper function to write to XUSB_REG_PORTSC register
    //! \param [in] Val : value to be written to register
    //! \param [in] WrClearBits : Write to Clear bits 
    RC RegWrPortSc(UINT32 Val, UINT32 WrClearBits = 0);
    //! \brief Helper function to write to XUSB_REG_PORTHALT register
    //! \param [in] Val : value to be written to register
    //! \param [in] WrClearBits : Write to Clear bits 
    RC RegWrPortHalt(UINT32 Val, UINT32 WrClearBits = 0);

    //! \brief Trigger Load Context to notify host that EP has been updated
    //! \param [in] EpNum : Endpoint
    //! \param [in] IsDataOut : Data direction
    RC ReloadEpContext(UINT32 EpNum, bool IsDataOut);
    //! \brief Trigger Load Context on specified EPs and pause on specified EPs
    //! \param [in] EpNum : Endpoint
    //! \param [in] IsDataOut : Data direction
    RC ReloadEpContext(UINT32 EpBitMask, UINT32 PauseMaskBit = 0);

private:

    //! \var pointer to the BufBuild
    BufBuild *  m_pBufBuild;
    //! \var Flag to enable BufBuild
    bool        m_IsEnBufBuild;
    //! \var Index of BufManager that returned by BufBuild 
    UINT64      m_bufIndex;

    //! \var JS callback functions 
    JSFunction* m_pJsCbConn;
    JSFunction* m_pJsCbDisconn;
    JSFunction* m_pJsCbU3U0;

    //! \var Max ERST read from CAP register
    UINT32      m_ErstMax;

    //! \var the size (number of TRBs) used to create Event Ring
    UINT32      m_EventRingSize;
    //! \var the size used to create Transfer Ring
    UINT32      m_XferRingSize;

    //! \var the state of state machine in Device Mode
    UINT32      m_DeviceState;
    //! \var the USB speed of Device Mode
    UINT08      m_UsbSpeed;
    //! \var the USB version of Device Mode
    UINT16      m_UsbVer;

    //! \var CFG index specified by SetConfiguration from host
    UINT08      m_ConfigIndex;
    //! \var class of Device Mode
    UINT08      m_DevClass;
    //! \var pointers for Event Rings which has two segments
    EventRing * m_pEventRing;

    //! \var Device mode Endpoint Context
    DevModeEpContexts* m_pDevmodeEpContexts;

    //! \var to hold device descriptor specified by user
    MemoryFragment::FRAGMENT m_DevDescriptor;
    //! \var to hold BOS descriptor specified by user
    MemoryFragment::FRAGMENT m_BosDescriptor;
    //! \var to hold configuration descriptor specified by the user
    MemoryFragment::FRAGLIST m_vConfigDescriptor;
    //! \var to hold string descriptor specified by the user
    MemoryFragment::FRAGLIST m_vStringDescriptor;
    //! \var to hold responses for various SCSI commands
    MemoryFragment::FRAGLIST m_vScsiCmdResponse;

    //! \var transfer rings created dynamically by endpoints
    vector<TransferRing *>  m_vpRings;

    //! \var Endpoint information retrieved from ConfigDescriptor
    vector<EndpointInfo>    m_vEpInfo;
    //! \var class specific variables for BULK Devices
    UINT08      m_BulkInEp;
    UINT08      m_BulkOutEp;

    //! \var class  specific variables for Isoch Devices
    MemoryFragment::FRAGLIST m_vIsochBuffers;
    UINT32      m_IsoTdSize;

    //! \var Device ID used for chip specific OPs
    UINT32      m_DevId;

    //! \var times to retry
    UINT32      m_RetryTimes;
    UINT32      m_RetryTimesBuf;
    //! \var the time in second to exit Device Mode engines
    UINT64      m_TimeToFinish;

    //! \var for buffer overrun / underrun test purpose
    UINT32      m_BulkDelay;
    //! \var if to reset transfer rings, this happens when user switch configuration
    bool        m_IsRstXferRing;

    //! \var data patterns
    vector<UINT32>          m_vDataPattern32;
};

#endif // INCLUDED_XUSBMDEV_H

