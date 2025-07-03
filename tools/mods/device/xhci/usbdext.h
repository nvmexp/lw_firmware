/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_USBDEXT_H
#define INCLUDED_USBDEXT_H

#ifndef INCLUDED_USBTYPE_H
    #include "usbtype.h"
#endif
#ifndef INCLUDED_PLATFORM_H
    #include "core/include/platform.h"
#endif

#define DESCRIPTOR_BUFFER_SIZE  4096

typedef enum _XhciEndpointType
{
    XhciEndpointTypeNotValid    = 0,
    XhciEndpointTypeIsochOut    = 1,
    XhciEndpointTypeBulkOut     = 2,
    XhciEndpointTypeInterruptOut= 3,
    XhciEndpointTypeControl     = 4,
    XhciEndpointTypeIsochIn     = 5,
    XhciEndpointTypeBulkIn      = 6,
    XhciEndpointTypeInterruptIn = 7
}XhciEndpointType;

typedef enum _XhciUsbSpeed
{   //Table 34 Port Status and Control Register Bit Definitions (PORTSC)
    XhciUsbSpeedUndefined       = 0,
    XhciUsbSpeedFullSpeed       = 1,
    XhciUsbSpeedLowSpeed        = 2,
    XhciUsbSpeedHighSpeed       = 3,
    XhciUsbSpeedSuperSpeed      = 4,
    XhciUsbSpeedSuperSpeedGen2  = 5,
    XhciUsbSpeedUnknown         = 15,
}XhciUsbSpeed;

typedef struct _EndpointInfo
{   // hold the info contained by USB Endpoint Descriptor
    UINT08  ConfigIndex;
    bool    IsInit;
    UINT08  EpNum;
    bool    IsDataOut;
    UINT08  Type;
    UINT08  SyncType;   // for Iso
    UINT08  UsageType;  // for Iso & Intr (USB3 only)
    UINT32  Mps;
    UINT08  TransPerMFrame; // for USB2.0 High Speed Iso & Intr
    UINT08  Attrib;
    UINT32  Interval;
    // below comes from SS Endpoint Companion
    bool    IsSsCompanionAvailable;
    UINT08  MaxBurst;
    UINT32  MaxStreams;
    UINT08  Mult;
    UINT32  BytePerInterval;
    _EndpointInfo()                 //the inline constructor to init member variables
    {
        ConfigIndex = 0;
        IsInit = false;
        IsSsCompanionAvailable = false;
        EpNum = 0;
        IsDataOut = false;
        Type = 0;
        SyncType = 0;
        UsageType = 0;
        Mps = 0;
        TransPerMFrame = 0;
        Attrib = 0;
        Interval = 0;
        MaxBurst = 0;
        MaxStreams = 0;
        Mult = 0;
        BytePerInterval = 0;
    }
} EndpointInfo;

#define USB_CSW_OFFSET_SIGNATURE   0
#define USB_CSW_OFFSET_TAG         4
#define USB_CSW_OFFSET_RESIDUE     8
#define USB_CSW_OFFSET_STATUS      12

typedef struct {
    UINT32 EpNum;
    bool IsDataOut;
    void * pBufAddress;
} EP_INFO;

class XhciController;

//------------------------------------------------------------------------------
// Base class for all USB devices, e.g. USB flash disk. USB mouse. etc.
//------------------------------------------------------------------------------
class UsbDevExt
{
public:
    UsbDevExt(XhciController * pHost, UINT08 SlotID, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString);
    virtual ~UsbDevExt();

    //! \brief The factory of all USB devices
    //--------------------------------------------------------------------------
    static      RC Create(XhciController * pHost,
                          UINT08 SlotID,
                          UINT08 Speed,
                          UINT32 RouteString,
                          UsbDevExt ** ppDev,
                          UINT08 CfgIndex = 1);

    //! \brief Preserve the translated Endpoint information
    //--------------------------------------------------------------------------
    virtual     RC Init(vector<EndpointInfo> *pvEpInfo) {return OK;}
    virtual     RC Init(UINT08 *pBuffer, UINT16 Len) {return OK;} // Only for usb video class

    //! \brief Print Sting Descriptor of USB device
    //--------------------------------------------------------------------------
    static      RC PrintDescrString(XhciController * pHost, UINT08 SlotID,
                                    UINT08 StrIndex, UINT08 Index,
                                    Tee::Priority Pri = Tee::PriNormal);

    //! \brief Print basic information of USB device
    //--------------------------------------------------------------------------
    virtual     RC PrintInfo(Tee::Priority Pri=Tee::PriNormal);

    //! \brief Return type of USB device (Mass Storage, Webcam)
    //--------------------------------------------------------------------------
    UINT32      GetDevClass() {return m_DevClass;}

    //! \brief Parse Endpoint Descriptor and put the info into pEpDesc
    // there's a lot of translation and make up to keep Xhci / USB spec sync.
    //--------------------------------------------------------------------------
    static      RC ParseEpDescriptors(UINT08 * pEpDesc,
                                      UINT32 NumEndpoint,
                                      UINT16 UsbVer,
                                      UINT08 Speed,
                                      vector<EndpointInfo> *pvEpInfo,
                                      UINT08 * pOffset = NULL);

    //! \brief Dump the translated EP information for a eye verification
    //--------------------------------------------------------------------------
    static      RC PrintEpInfo(vector<EndpointInfo> *pvEpInfo, UINT32 Index = 0/*0 means all*/,
                               Tee::Priority Pri = Tee::PriLow);

    //! \brief Parse Device Descriptor and get Mps and Version info
    //--------------------------------------------------------------------------
    static      RC ParseDevDescriptor(UINT08 * pDevDesc,
                                      UINT16 * pBcdUsb,
                                      UINT32 * pMps);

    //! \brief Find next descriptor only for Video devices
    //--------------------------------------------------------------------------
    static      RC FindNextDescriptor(UINT08* pBuffer, INT32 Size,
                                      UINT08 Dt, UINT32 *pOff);

    static      RC GetDescriptorTypeLength(UINT08* pBuffer,
                                           UINT08* pType,
                                           UINT08* pLendth,
                                           UsbDescriptorType ExpectType = UsbDescriptorNA);

    // helper to copy data fields from memory buffer to structure
    static      RC MemToStruct(UsbDeviceDescriptor* pStruct, UINT08 * pBuf);
    static      RC MemToStruct(UsbConfigDescriptor* pStruct, UINT08 * pBuf, bool IsSkipCheck = false);
    static      RC MemToStruct(UsbInterfaceAssocDescriptor* pStruct, UINT08 * pBuf);
    static      RC MemToStruct(UsbInterfaceDescriptor* pStruct, UINT08 * pBuf);
    static      RC MemToStruct(UsbEndPointDescriptor* pStruct, UINT08 * pBuf);
    static      RC MemToStruct(UsbHubDescriptor* pStruct, UINT08 * pBuf);
    static      RC MemToStruct(UsbHubDescriptorSs* pStruct, UINT08 * pBuf);
    static      RC MemToStruct(UsbSsEpCompanion* pStruct, UINT08 * pBuf);
    static      RC MemToStruct(UsbHidDescriptor* pStruct, UINT08 * pBuf);
    static      RC MemToStruct(UsbVideoSpecificInterfaceDescriptor* pStruct, UINT08 * pBuf);
    static      RC MemToStruct(UsbVideoSpecificInterfaceDescriptorHeader* pStruct, UINT08 * pBuf);

    RC GetProperties(UINT08 * pSpeed, UINT32 * pRouteString);

protected:

    XhciController * m_pHost;
    // UINT32      m_FuncAddr;
    // depending on how xHC implements slot assignment vs address allocation, It's possible
    // for two or more slots having the same address. Thus we use SlotId instead of UsbAddress here
    UINT32      m_SlotId;
    bool        m_IsInit;
    UINT16      m_UsbVer;    // 0x210, 0x300
    UINT08      m_Speed;     // Super High Full Low
    UINT32      m_DevClass;  // Storage, mouse, webcam?
    UINT32      m_RouteString;  // device's RouteString

private:
    static      UINT08 MemAdv8(UINT08 ** ppBuf);
    static      UINT16 MemAdv16(UINT08 ** ppBuf);
    static      UINT32 MemAdv32(UINT08 ** ppBuf);
};

//------------------------------------------------------------------------------
// Base class for all USB storage devices, e.g. USB flash disk.
//------------------------------------------------------------------------------
class UsbStorage:public UsbDevExt
{
public:
    UsbStorage(XhciController * pHost, UINT08 SlotID, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString);
    virtual ~UsbStorage();

    virtual     RC PrintInfo(Tee::Priority Pri=Tee::PriNormal);
    virtual     RC Init(vector<EndpointInfo> *pvEpInfo);

    RC          CmdRead10(UINT32 LBA,
                          UINT32 NumSectors,
                          MemoryFragment::FRAGLIST * pFragList,
                          vector<UINT08> *pvCbw = NULL);
    RC          CmdRead16(UINT32 LBA,
                          UINT32 NumSectors,
                          MemoryFragment::FRAGLIST * pFragList,
                          vector<UINT08> *pvCbw = NULL);

    RC          CmdWrite10(UINT32 LBA,
                           UINT32 NumSectors,
                           MemoryFragment::FRAGLIST * pFragList,
                           vector<UINT08> *pvCbw = NULL);
    RC          CmdWrite16(UINT32 LBA,
                           UINT32 NumSectors,
                           MemoryFragment::FRAGLIST * pFragList,
                           vector<UINT08> *pvCbw = NULL);

    RC          CmdReadCapacity(UINT32 *pMaxLBA, UINT32 *pSectorSize);
    RC          CmdTestUnitReady();
    RC          CmdInq(void * pBuf, UINT08 Len);
    RC          CmdGetMaxLun(UINT32 * pMaxLun);
    RC          CmdModeSense(void * pBuf = NULL, UINT08 Len = 0);
    RC          CmdSense(void * pBuf = NULL, UINT08 Len = 0);
    RC          CmdReadFormatCapacity(UINT32 *pNumBlocks, UINT32 *pSectorSize);

    RC          GetSectorSize(UINT32 * pSize);
    RC          GetMaxLba(UINT32 * pMaxLba);

    RC          BulkTransfer(vector<UINT08> *pvCBW,
                             bool IsDataOut,
                             void * pDataBuf, UINT32 Len);

    RC          BulkTransfer(vector<UINT08> *pvCBW,
                             bool IsDataOut,
                             MemoryFragment::FRAGLIST * pFragList);
    RC          XferCbw(vector<UINT08> *pvCbw, UINT32 * pCompletionCode = NULL, EP_INFO *pEpInfo = NULL);
    RC          XferCsw(UINT32 * pCompletionCode,
                        UINT32 *pCSWDataResidue = NULL,
                        UINT08 *pCSWStatus = NULL,
                        EP_INFO * pEpInfo = NULL);
    RC          GetInEpInfo(EP_INFO * pEpInfo);

private:
    RC          SetupCmdCommon(vector<UINT08> * pvCmd, UINT32 xferBytes, bool IsOut, UINT08 CmdLen);
    RC          ResetRecovery();

    UINT08      m_InEp;
    UINT08      m_OutEp;

    UINT32      m_MaxLBA;
    UINT32      m_SectorSize;
};

//------------------------------------------------------------------------------
// Base class for all Hid  devices, e.g. USB mouse, USB keboard etc.
//------------------------------------------------------------------------------
class UsbHidDev:public UsbDevExt
{
public:
    UsbHidDev(XhciController * pHost, UINT08 SlotID, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString);
    ~UsbHidDev();

    RC PrintInfo(Tee::Priority = Tee::PriNormal);
    RC Init(vector<EndpointInfo> *pvEpInfo);
    RC ReadMouse(UINT32 RunSeconds);

private:
    UINT08      m_InEp;
    UINT16      m_Mps;
};

//------------------------------------------------------------------------------
// HUB class
//------------------------------------------------------------------------------
class UsbHubDev:public UsbDevExt
{
public:
    UsbHubDev(XhciController * pHost, UINT08 SlotID, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString, UINT08 NumOfPorts);
    ~UsbHubDev();

    RC PrintInfo(Tee::Priority = Tee::PriNormal);
    RC Init(vector<EndpointInfo> *pvEpInfo);
    RC ReadHub(UINT32 RunSeconds);
    RC ResetHubPort(UINT08 Port, UINT08 * pSpeed);
    RC EnableTestMode(UINT08 Port, UINT32 TestMode);
    RC Search();

protected:
    RC HubPortStatus(UINT08 Port, UINT16 *pPortStat, UINT16 *pPortChange);
    RC ClearPortFeature(UINT08 Port, UINT16 Feature);
    RC SetPortFeature(UINT08 Port, UINT16 Feature);

private:
    UINT08      m_InEp;
    UINT16      m_Mps;
    UINT08      m_NumOfPorts;
};

class UsbConfigurable:public UsbDevExt
{
public:
    UsbConfigurable(XhciController * pHost, UINT08 SlotID, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString);
    ~UsbConfigurable();

    RC PrintInfo(Tee::Priority = Tee::PriNormal);
    RC Init(vector<EndpointInfo> *pvEpInfo);
    RC DataIn(UINT08 Endpoint,
              MemoryFragment::FRAGLIST * pFragList,
              bool IsIsoch = false);
    RC DataOut(UINT08 Endpoint,
               MemoryFragment::FRAGLIST * pFragList,
               bool IsIsoch = false);
    RC RegRd(UINT32 Offset, UINT64 * pData);
    RC RegWr(UINT32 Offset, UINT64 Data);

private:
    vector<EndpointInfo> m_vEpInfo;
};

//------------------------------------------------------------------------------
// Base class for all video class devices, e.g. web camera
//------------------------------------------------------------------------------
// TODO: Put the most frequently accessed fields at the beginning of
// structures to maximize cache efficiency.
typedef struct UvcStreamingControl
{
    INT16  BmHint;
    UINT08 FormatIndex;
    UINT08 FrameIndex;
    UINT32 FrameInterval;
    UINT16 KeyFrameRate;
    UINT16 PFrameRate;
    UINT16 CompQuality;
    UINT16 CompWindowSize;
    UINT16 Delay;
    UINT32 MaxVideoFrameSize;
    UINT32 MaxPayloadTransferSize;
    UINT32 ClockFrequency;
    UINT08 BmFramingInfo;
    UINT08 PreferedVersion;
    UINT08 Milwersion;
    UINT08 MaxVersion;
}UvcStreamingCtl;

/* Values for bmHeaderInfo (Video and Still Image Payload Headers, 2.4.3.3) */
#define UVC_STREAM_EOH  (1 << 7)
#define UVC_STREAM_ERR  (1 << 6)
#define UVC_STREAM_STI  (1 << 5)
#define UVC_STREAM_RES  (1 << 4)
#define UVC_STREAM_SCR  (1 << 3)
#define UVC_STREAM_PTS  (1 << 2)
#define UVC_STREAM_EOF  (1 << 1)
#define UVC_STREAM_FID  (1 << 0)

class UsbVideoDev:public UsbDevExt
{
public:
    UsbVideoDev(XhciController * pHost, UINT08 SlotID, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString);
    ~UsbVideoDev();

    virtual RC PrintInfo(Tee::Priority = Tee::PriNormal);
    RC PrintIntfInfo(Tee::Priority Pri);

    virtual RC Init(UINT08 *pDescriptor, UINT16 Len);
    RC ReadWebCam(UINT32 RunSeconds = 0, UINT32 RunTds = 0, UINT32 TdUsage = 0);
    RC UvcQueryCtrl(UINT08 Request, UINT08 Interface, UINT08 Unit, UINT08 Cs, void *pData = NULL, UINT16 Size = 0);
    RC SaveFrame(const string fileName);

    typedef struct _Ep
    {
        UsbEndPointDescriptor des;
        UsbSsEpCompanion  ssEp;
    }Endpoint;

    typedef struct _interface
    {
        UsbInterfaceDescriptor des;
        vector <Endpoint>  vEp;
    }Intf;

    typedef struct _intfInfo
    {
        UINT32 numSetting;
        vector <Intf> vAltSetting;
    }IntfInfo;

    typedef struct _isoEp
    {
        UINT08      altSetting;
        UINT08      inIsoEp;
        UINT16      isoPayload;
        UsbEndPointDescriptor *pEp;
    }IsoEp;

protected:
    RC FindStreamIsoEp();
    RC ProbeAndCommit();
    RC ParseDesc(UINT08 *pDescriptor, int Size, vector<IntfInfo> * pvIntfInfo);
    RC AllocateIsoBuffer(MemoryFragment::FRAGLIST *pFragList, UINT32 NumFrag, UINT32 Size);
    RC FreeIsoBuffer(MemoryFragment::FRAGLIST *pFragList);
    RC CleanFragLists();
    RC SetWorkingEp();
    // RC SelfAnalysis();
    // Uvc command
    RC UvcGetVideoCtrl(UvcStreamingCtl *ctrl, int probe, UINT08 query);
    RC UvcVideDecodeStart(UINT08 *pData, bool IsDebug =  false);
    RC UvcSetVideoCtrl(UvcStreamingCtl *ctrl, int probe);
    RC PrintStreamingCtrl();

private:

    //  stream infomation
    vector <IsoEp> m_vIsoEp;
    // config, interface and ep inforamtion
    vector <IntfInfo> m_vIntfInfo;

    // the index of the m_vIsoEp which has max payload value
    UINT32      m_MaxPayLoadIndex;
    // pointer to current Ep
    IsoEp *     m_pLwrStreamEp;

    // current interface
    UINT08      m_StreamInfNum;

    // video device control params
    UvcStreamingCtl m_StreamCtrl;

    UINT08      m_LastFid;
    UINT32      m_RcvFrameCount;

    vector  <MemoryFragment::FRAGLIST> m_vFragList;

    // true if UVC version 1.0
    bool m_IsObsolete;
};

#endif
