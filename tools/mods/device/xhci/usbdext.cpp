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

#include "core/include/tee.h"
#include "cheetah/include/logmacros.h"
#include "cheetah/include/tegradrf.h"
#include "device/include/memtrk.h"
#include "usbdext.h"
#include "xhcictrl.h"
#include "core/include/cnslmgr.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include <string.h>

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "UsbDevExt"

// The xHCI supports up to 255 USB devices, where each USB device is assigned to a Device Slot.
// depending on how xHC implements slot assignment vs address allocation, It's possible
// for two or more slots having the same address. Thus we use SlotId instead of UsbAddress here.
// When port this class driver to EHCI/OHCI, simply replace SlotID with UsbAddress.
//---------------------------------------------------------------------------------
// Base class for all external USB device classes
//---------------------------------------------------------------------------------
UsbDevExt::UsbDevExt(XhciController * pHost, UINT08 SlotId, UINT16 UsbVer,
                     UINT08 Speed, UINT32 RouteString)
{
    LOG_ENT();
    m_pHost         = pHost;
    m_SlotId        = SlotId;
    m_UsbVer        = UsbVer;
    m_Speed         = Speed;
    m_RouteString   = RouteString;
    m_IsInit        = false;

    // usb properties
    m_DevClass      = 0;
    LOG_EXT();
}

UsbDevExt::~UsbDevExt()
{
    LOG_ENT();
    LOG_EXT();
}

// USB ExtDev factory. Create & configurate USB ext devices.
    // step01 Get full Device Descriptor
    // step02 Get Configuration Descriptor & Interface Descriptor
    // step03 based on the dev class in Interface Descriptor, create corresponding USB device object.
RC UsbDevExt::Create(XhciController * pHost,
                     UINT08 SlotId,
                     UINT08 Speed,
                     UINT32 RouteString,
                     UsbDevExt ** ppDev,
                     UINT08 CfgIndex)
{
    LOG_ENT();
    Printf(Tee::PriLow,"(SlotId %d, Speed %d)\n", SlotId, Speed);

    RC rc = OK;
    if (!ppDev)
    {
        Printf(Tee::PriError, "Null pointer\n");
        return RC::BAD_PARAMETER;
    }
    *ppDev = NULL;
    void* pBuffer = nullptr;

    DEFER
    {
        if ((OK != rc) && (*ppDev))
        {
            delete *ppDev;
            *ppDev = NULL;
        }
        MemoryTracker::FreeBuffer(pBuffer);
    };

    UsbDeviceDescriptor descDev;
    UsbConfigDescriptor descCfg;
    UsbInterfaceDescriptor descIntf;
    UsbHidDescriptor descHid;
    UINT32 interfaceDescOffset = 0;

    vector<EndpointInfo> vEpInfo;

    bool isHub = false;
    // below will be avaliable if isHub = true or
    // todo: If the device is a Low-/Full-speed function or hub accessed through a High-speed hub, then the
    // following values are derived from the "parent" High-speed hub
    // MTT, TT Port Number, TT Hub Slot ID
    UINT08 * pHubBuf = NULL;
    UINT32 ttt = 0;
    // MTT: 1 if this is a High-speed hub (Speed ='3' and Hub ='1') that supports Multiple TTs
    // 2 if this is a Low-/Full-speed device and connected to the xHC through a parentb High-speed hub that supports Multiple TTs
    // otherweiase 0. item 2 is a TODO. call host->FindHubAddress() to get the Mtt info
    bool mtt = false;
    UINT08 numPorts = 0;

    CHECK_RC(MemoryTracker::AllocBuffer(DESCRIPTOR_BUFFER_SIZE, &pBuffer, true, 32, Memory::UC));

    // -------- step01: get full Device Descriptor
    //using 0x12 as length, // otherwise some device can't read data out.
    CHECK_RC(pHost->ReqGetDescriptor(SlotId, UsbDescriptorDevice, 0, pBuffer, 0x12));
    Printf(Tee::PriLow, "  Device Descriptor: \n");
    Memory::Print08(pBuffer, 0x12, Tee::PriLow);

    CHECK_RC(MemToStruct(&descDev, static_cast<UINT08*>(pBuffer)));
    if (descDev.Type != UsbDescriptorDevice
        || descDev.Length != LengthDeviceDescr  )
    {
        Printf(Tee::PriError, "Device Descriptor bad format\n");
        CHECK_RC(RC::HW_STATUS_ERROR);
    }

#if !defined(INCLUDE_PEATRANS)
    // Print Ext Dev Info
    CHECK_RC(PrintDescrString(pHost, SlotId, descDev.Mamufacture, 14, Tee::PriLow));
    CHECK_RC(PrintDescrString(pHost, SlotId, descDev.Product, 15, Tee::PriLow));
    CHECK_RC(PrintDescrString(pHost, SlotId, descDev.SerialNumber, 16, Tee::PriLow));
#endif
    if (descDev.NumConfig != 1)
    {
        Printf(Tee::PriNormal, "  Number of Configurations = %d \n", descDev.NumConfig);
    }
    if ( (CfgIndex > descDev.NumConfig)
        || (0 == CfgIndex) )
    {
        Printf(Tee::PriError, "CfgIndex invalid, valid [1 - %d]", descDev.NumConfig);
        CHECK_RC(RC::BAD_PARAMETER);
    }

    // -------- step02: Get Configuration Descriptor
    CHECK_RC(pHost->ReqGetDescriptor(SlotId, UsbDescriptorConfig, CfgIndex - 1, pBuffer, DESCRIPTOR_BUFFER_SIZE));
    CHECK_RC(MemToStruct(&descCfg, static_cast<UINT08*>(pBuffer), true));
    if (descCfg.Type != UsbDescriptorConfig )
    {   // on some SS HUB, read a large number return incorrect contect (device descriptor instead), so we do a 128 Byte read again
        CHECK_RC(pHost->ReqGetDescriptor(SlotId, UsbDescriptorConfig, CfgIndex - 1, pBuffer, 128));
        CHECK_RC(MemToStruct(&descCfg, static_cast<UINT08*>(pBuffer), true));
        if (descCfg.Type != UsbDescriptorConfig )
        {
            Printf(Tee::PriError, "Configuration Descriptor bad format, raw data(128 Byte):\n");
            Memory::Print08(pBuffer, 128, Tee::PriHigh);
            CHECK_RC(RC::HW_STATUS_ERROR);
        }
    }

    Printf(Tee::PriLow, "Config Descriptor %d: \n", CfgIndex - 1);
    Memory::Print08(pBuffer, descCfg.TotalLength, Tee::PriLow);
    Printf(Tee::PriLow, "\n");
    if (descCfg.NumInterfaces != 1)
    {
        Printf(Tee::PriNormal, "Cfg %d - Number of Interfaces = %d", CfgIndex - 1, descCfg.NumInterfaces);
    }

    // Get First Interface Descriptor
    rc = FindNextDescriptor(static_cast<UINT08*>(pBuffer),
                            descCfg.TotalLength,
                            UsbDescriptorInterface,
                            &interfaceDescOffset);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Interface Descriptor not found\n");
        CHECK_RC(RC::HW_STATUS_ERROR);
    }
    CHECK_RC(MemToStruct(&descIntf, static_cast<UINT08*>(pBuffer) + interfaceDescOffset));

    Printf(Tee::PriNormal,
           "USB Dev on Slot %u Class: 0x%x, SubClass 0x%x, Protocol 0x%x\n"
           "Number of Endpoints %u USB Ver %x\n",
           SlotId, descIntf.Class, descIntf.SubClass, descIntf.Protocol,
           descIntf.NumEP, descDev.Usb);

    // -------- step06: Create USB ext device object
    switch(descIntf.Class)
    {
        case  UsbClassMassStorage:
            // Interpret the Interface descriptor
            if (descIntf.SubClass != 0x06
                || descIntf.Protocol != 0x50)
            {
                Printf(Tee::PriError, "Not a SCSI BULK-Only device\n");
                CHECK_RC(RC::HW_STATUS_ERROR);
            }

            // Get EndPoint Descriptor, this is applied to Mass Storage Device
            if (descIntf.NumEP != 2)
            {
                Printf(Tee::PriWarn, "Found %d endpiont, expect 2", descIntf.NumEP);
                // CHECK_RC(RC::HW_STATUS_ERROR);
            }
            Printf(Tee::PriNormal, "Creating USB Mass Storage Device...\n");
            *ppDev = new UsbStorage(pHost, SlotId, descDev.Usb, Speed, RouteString);

            if (NULL == *ppDev)
            {
                Printf(Tee::PriError, "Can't allocate memory for Ext Dev object\n");
                CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);
            }
            CHECK_RC(ParseEpDescriptors(static_cast<UINT08*>(pBuffer) + interfaceDescOffset + descIntf.Length,
                                                descIntf.NumEP, descDev.Usb, Speed, &vEpInfo));
            break;

        case UsbClassHid:
            if (descIntf.SubClass != UsbHidSubClassBoot)
            {
                Printf(Tee::PriError, "Not boot device\n");
                CHECK_RC(RC::HW_STATUS_ERROR);
            }
            if (descIntf.Protocol != UsbHidProMouse)
            {
                Printf(Tee::PriError, "Software only support USB-mouse lwrrently\n");
                CHECK_RC(RC::HW_STATUS_ERROR);
            }
            if (!descIntf.NumEP)
            {
                Printf(Tee::PriError, "Found %d endpiont, expect at least 1 \n", descIntf.NumEP);
                CHECK_RC(RC::HW_STATUS_ERROR);
            }
            CHECK_RC(MemToStruct(&descHid, static_cast<UINT08*>(pBuffer) + interfaceDescOffset + descIntf.Length));
            if (descHid.Type != UsbDescriptorHid)
            {
                Printf(Tee::PriError, "Don't find HID descriptor\n");
                CHECK_RC(RC::HW_STATUS_ERROR);
            }
            Printf(Tee::PriNormal, "Creating USB HID Device...");
            *ppDev = new UsbHidDev(pHost, SlotId, descDev.Usb, Speed, RouteString);
            if (NULL == *ppDev)
            {
                Printf(Tee::PriError, "Can't allocate memory for Ext Dev object\n");
                CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);
            }
            CHECK_RC(ParseEpDescriptors(static_cast<UINT08*>(pBuffer) + interfaceDescOffset + descIntf.Length + descHid.Length,
                                                descIntf.NumEP, descDev.Usb, Speed, &vEpInfo));
            break;

        case UsbClassHub:
            if (descIntf.NumEP != 1)
            {
                Printf(Tee::PriWarn, "Found %d endpiont for HUB, expect 1", descIntf.NumEP);
            }
            // CHECK_RC(pHost->ReqSetConfiguration(SlotId, 1));
            CHECK_RC(MemoryTracker::AllocBuffer(1024, (void**)&pHubBuf, true, 32, Memory::UC));
            rc = pHost->HubReqGetHubDescriptor(SlotId, pHubBuf, 16, Speed >= XhciUsbSpeedSuperSpeed);
            Printf(Tee::PriLow, "Hub Descriptor: \n");
            Memory::Print08(pHubBuf, 16, Tee::PriLow);

            if (Speed >= XhciUsbSpeedSuperSpeed)
            {
                UsbHubDescriptorSs hubDescSs;
                MemToStruct(&hubDescSs, pHubBuf);
                ttt = 0;
                numPorts = hubDescSs.NPorts;
            }
            else
            {
                UsbHubDescriptor hubDesc;
                MemToStruct(&hubDesc, pHubBuf);
                // read the TTT and MTT by issuing a GetHubDescriptor request
                if (Speed == XhciUsbSpeedHighSpeed )
                {
                    ttt = ( hubDesc.HubCharac >> 5 ) & 0x3;   // see also 11.23.2.1 Hub Descriptor of USB 2.0 spec
                    mtt = ( descDev.Protocol == 2 ); // see also page 414 of USB2.0 spec
                }
                numPorts = hubDesc.NPorts;
            }
            MemoryTracker::FreeBuffer(pHubBuf);
            CHECK_RC(rc);

            Printf(Tee::PriLow, "Creating USB HUB Device...\n");
            *ppDev = new UsbHubDev(pHost, SlotId, descDev.Usb, Speed, RouteString, numPorts);
            if (NULL == *ppDev)
            {
                Printf(Tee::PriError, "Can't allocate memory for Ext Dev object\n");
                CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);
            }

            // CHECK_RC(pHost->ReqSetInterface(SlotId, 0, 0 /*interface*/));
            CHECK_RC(ParseEpDescriptors(static_cast<UINT08*>(pBuffer) + interfaceDescOffset + descIntf.Length,
                                                descIntf.NumEP, descDev.Usb, Speed, &vEpInfo));
            isHub = true;
            // MTT has been retrieved from Device Descriptor at the beginning, SA 11.23.1 of USB2 spec
            break;

        case UsbClassVideo:
            Printf(Tee::PriDebug, "Video Config:\n");
            if (descIntf.SubClass != UsbSubClassVideoControl)
            {
                Printf(Tee::PriError, "The first interface is not video control interface\n");
                CHECK_RC(RC::HW_STATUS_ERROR);
            }
            // pre-create device
            Printf(Tee::PriLow, "Creating USB Video Device...\n");
            *ppDev = new UsbVideoDev(pHost, SlotId, descDev.Usb, Speed, RouteString);
            if (NULL == *ppDev)
            {
                Printf(Tee::PriError, "Can't allocate memory for Ext Dev object\n");
                CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);
            }
            if (descCfg.TotalLength > DESCRIPTOR_BUFFER_SIZE)
            {
                Printf(Tee::PriError, "Buffer size %d is not enough to hold Config Description of size %d, abort!\n", DESCRIPTOR_BUFFER_SIZE, descCfg.TotalLength);
                return RC::SOFTWARE_ERROR;
            }
            // use pBuffer instead of pBug since the latter has been changed to point to Interface descriptor
            CHECK_RC((*ppDev)->Init((UINT08*)pBuffer, descCfg.TotalLength));  // Init the class device
            break;

        case  UsbConfigurableDev:
            Printf(Tee::PriNormal, "Creating Configurable USB Device, with %d Endpoints...", descIntf.NumEP);
            *ppDev = new UsbConfigurable(pHost, SlotId, descDev.Usb, Speed, RouteString);
            if (NULL == *ppDev)
            {
                Printf(Tee::PriError, "Can't allocate memory for Ext Dev object\n");
                CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);
            }
            CHECK_RC(ParseEpDescriptors(static_cast<UINT08*>(pBuffer) + interfaceDescOffset + descIntf.Length,
                                                descIntf.NumEP, descDev.Usb, Speed, &vEpInfo));
            break;

        default:
            Printf(Tee::PriWarn, "Unknown USB Device (class %u, subclass %u, protocal %u)", descIntf.Class, descIntf.SubClass, descIntf.Protocol);
            break;
    }

    // 4.3 USB Device Initialization
    // 9) After reading the Configuration Descriptors software shall issue an Evaluate Context Command
    //  with Valid bit 0 (V0) set to ??to inform the xHC of the values of the Hub and Max Exit Latency parameters.
    CHECK_RC(pHost->SetDevContextMiscBit(SlotId, 0, 0));
    // 10) The Class Driver may then configure the Device Slot using a Configure Endpoint Command

    // parse the Endpoint descriptor
    if ( vEpInfo.size() || isHub )
    {
        if (!isHub)
        {
            rc = pHost->CfgEndpoint(SlotId, &vEpInfo);
        }
        else
        {
            rc = pHost->CfgEndpoint(SlotId, &vEpInfo, isHub, ttt, mtt, numPorts);
        }
        if (rc != OK)
        {   // todo: go back to step05 to choose another configuration / interface
            Printf(Tee::PriError, "Configuration has been rejected by HC. Only Control Pipe is usable\n");
            CHECK_RC(RC::HW_STATUS_ERROR);
        }

        // Send Set Configuration request to device
        CHECK_RC(pHost->ReqSetConfiguration(SlotId, CfgIndex));
        // Init the class device
        CHECK_RC((*ppDev)->Init(&vEpInfo));
    }

    LOG_EXT();
    return rc;
}

RC UsbDevExt::GetProperties(UINT08 * pSpeed, UINT32 * pRouteString)
{
    *pSpeed = m_Speed;
    *pRouteString = m_RouteString;
    return OK;
}

RC UsbDevExt::ParseDevDescriptor(UINT08 * pDevDesc,
                                 UINT16 * pBcdUsb,
                                 UINT32 * pMps)
{
    LOG_ENT();
    RC rc;
    if (!pDevDesc)
    {
        Printf(Tee::PriError, "Device Descriptor null pointer\n");
        return RC::BAD_PARAMETER;
    }
    UsbDeviceDescriptor descDev;
    CHECK_RC(MemToStruct(&descDev, pDevDesc));

    if ( descDev.Type != UsbDescriptorDevice
         || descDev.Length != LengthDeviceDescr )
    {
        Printf(Tee::PriError, "Device Descriptor bad format\n");
        return RC::BAD_PARAMETER;
    }
    if (pBcdUsb)
    {
        *pBcdUsb = descDev.Usb;
        Printf(Tee::PriLow, "USB ver %x\n", *pBcdUsb);
    }

    if (pMps)
    {
        if ((descDev.Usb) >= 0x300)
            *pMps = 1 << (descDev.MaxPacketSize0);    // for USB 3
        else
            *pMps = descDev.MaxPacketSize0;           // for USB 2
        Printf(Tee::PriLow, "Mps %d\n", *pMps);
    }
    return OK;
}

RC UsbDevExt::ParseEpDescriptors(UINT08 * pEpDesc,
                                 UINT32 NumEndpoint,
                                 UINT16 UsbVer,
                                 UINT08 Speed,
                                 vector<EndpointInfo> *pvEpInfo,
                                 UINT08 * pOffset /*= null*/)
{   // retrieve data, do necessary interpret and colwersion USB -> Xhci
    LOG_ENT();
    RC rc = OK;

    if (!pEpDesc)
    {
        Printf(Tee::PriError, "EpDescriptor null pointer\n");
        return RC::BAD_PARAMETER;
    }
    UsbEndPointDescriptor descEndp;
    UsbEndPointDescriptor * pTmpE = & descEndp;
    CHECK_RC(MemToStruct(&descEndp, pEpDesc));

    if ( pTmpE->Type != UsbDescriptorEndpoint )
    {
        Printf(Tee::PriError, "Endpoint Descriptor bad format\n");
        Memory::Print08((void *)pEpDesc, 64, Tee::PriHigh);
        return RC::BAD_PARAMETER;
    }

    pvEpInfo->clear();
    UINT08 offsetIntoEpDesc = 0;
    for (UINT32 i = 0; i< NumEndpoint; i++ )
    {
        CHECK_RC(MemToStruct(&descEndp, pEpDesc + offsetIntoEpDesc));
        offsetIntoEpDesc += pTmpE->Length;
        if ( pTmpE->Type != UsbDescriptorEndpoint
             || pTmpE->Length != LengthEndPointDescr )
        {
            Printf(Tee::PriError, "EndPoint Descriptor bad format\n");
            return RC::BAD_PARAMETER;
        }

        EndpointInfo myEpInfo;
        myEpInfo.IsInit = true;

        myEpInfo.EpNum = pTmpE->Address & 0xf;
        myEpInfo.IsDataOut = ((pTmpE->Address & 0x80) == 0);

        // Get Type
        // 0x00 control, 0x01 Isoch, 0x10 Bulk, 0x11 Interrupt
        // do a USB to XHCI colwert for Endpoint Type
        myEpInfo.Type = (pTmpE->Attibutes & 0x3) + 4 * (myEpInfo.IsDataOut?0:1);
        if ( 0 == (pTmpE->Attibutes & 0x3) )
            myEpInfo.Type = 4; // for control, thereotically this shouldn't be the case

        // Get SyncType & UsageType
        if (myEpInfo.Type == XhciEndpointTypeIsochOut || myEpInfo.Type == XhciEndpointTypeIsochIn)
        {
            myEpInfo.SyncType = pTmpE->Attibutes & 0x0c >> 2;
            myEpInfo.UsageType = pTmpE->Attibutes & 0x30 >> 4;
        }
        else if (myEpInfo.Type == XhciEndpointTypeInterruptOut || myEpInfo.Type == XhciEndpointTypeInterruptIn)
        {
            if (UsbVer >= 0x300)
                myEpInfo.UsageType = pTmpE->Attibutes & 0x30 >> 4;
        }

        // Get Mps
        if (UsbVer >= 0x300)
            myEpInfo.Mps = pTmpE->MaxPacketSize;
        else
        { // For USb2.0, bits 10..0 specify the MPS
            myEpInfo.Mps = pTmpE->MaxPacketSize & 0x7ff;
            myEpInfo.TransPerMFrame = (pTmpE->MaxPacketSize & 0x1800) >> 11;
        }

        // Get Interval
        if (UsbVer >= 0x300)
        {
            if (myEpInfo.Type == XhciEndpointTypeInterruptOut || myEpInfo.Type == XhciEndpointTypeInterruptIn
                || myEpInfo.Type == XhciEndpointTypeIsochOut || myEpInfo.Type == XhciEndpointTypeIsochIn)
            {   // a USB -> Xhci colwersion
                if ( (pTmpE->Interval < 1 )
                     || (pTmpE->Interval > 16) )
                {
                    Printf(Tee::PriWarn, "Invalid Interval value %u, expect [1-16]", myEpInfo.Interval);
                }
                myEpInfo.Interval = pTmpE->Interval - 1;
            }
            else
            {
                myEpInfo.Interval = 0;
            }
        }
        else if (UsbVer >= 0x200)
        {   // USb2.0
            if (myEpInfo.Type == XhciEndpointTypeInterruptOut || myEpInfo.Type == XhciEndpointTypeInterruptIn)
            {   // for Interrupt Ep
                if (Speed == XhciUsbSpeedFullSpeed || Speed == XhciUsbSpeedLowSpeed)
                {
                    // colwert from 8 -> 3 (2^3), 16 -> 4
                    myEpInfo.Interval = pTmpE->Interval * 8;
                    UINT32 i = 0;
                    for (i = 0; i<32; i++)
                    {
                        if (myEpInfo.Interval & 0x80000000)
                            break;
                        myEpInfo.Interval <<= 1;
                    }
                    myEpInfo.Interval = 32 - i - 1;

                    if ( (myEpInfo.Interval < 3 )
                         || (myEpInfo.Interval > 10) )
                    {
                        Printf(Tee::PriWarn, "Invalid Interval value %u, expect [3-10]", myEpInfo.Interval);
                    }
                }
                else
                {   // For high-speed interrupt endpoints
                    myEpInfo.Interval = pTmpE->Interval - 1;
                }
            }
            else if (myEpInfo.Type == XhciEndpointTypeIsochOut || myEpInfo.Type == XhciEndpointTypeIsochIn)
            {   // for Isoch
                myEpInfo.Interval = pTmpE->Interval - 1;
            }
            else
            {   // for BULK & control, this is maximum NAK rate
                myEpInfo.Interval = 0;
            }
        }
        else // 1.1
        {
            UINT32 interval = 0;
            if (myEpInfo.Type == XhciEndpointTypeInterruptOut || myEpInfo.Type == XhciEndpointTypeInterruptIn
                    || myEpInfo.Type == XhciEndpointTypeIsochOut || myEpInfo.Type == XhciEndpointTypeIsochIn)
            {
                    UINT32 tmp = pTmpE->Interval * 8;
                    UINT32 j = 0;
                    for (j = 0; j < 32; j++)
                    {
                        if (tmp & 0x80000000)
                            break;

                        tmp <<= 1;
                    }
                    interval = 32 - j - 1;

                    if (interval > 10)
                        interval = 10;

                    if (interval < 3)
                        interval = 3;

                    Printf(Tee::PriWarn, "ep %#x - rounding interval to %d microframes",
                                myEpInfo.EpNum, 1 << interval);

            }

            myEpInfo.Interval = interval;
        }

        // figure out if there's SsEpCompanian descriptor followed
        UINT08 type, length;
        GetDescriptorTypeLength(pEpDesc + offsetIntoEpDesc, &type, &length);
        if (type == UsbDescriptorSsEpCompanion
            && length == LengthSsEpCompanion)
        {   // find a SS Ep companion desecriptor
            UsbSsEpCompanion descComp;
            UsbSsEpCompanion * pTmpS = &descComp;
            CHECK_RC(MemToStruct(&descComp, pEpDesc + offsetIntoEpDesc));

            offsetIntoEpDesc += LengthSsEpCompanion;
            myEpInfo.IsSsCompanionAvailable = true;

            // Get MaxBurst
            myEpInfo.MaxBurst = pTmpS->MaxBurst;

            if ( myEpInfo.Type == XhciEndpointTypeBulkOut || myEpInfo.Type == XhciEndpointTypeBulkIn )
            {  // is this is a BULK, Get MaxStreams,
               // Note: there's conflict here in USB3 and Xhci
                myEpInfo.MaxStreams = pTmpS->Attibutes & 0x1f;
                if (myEpInfo.MaxStreams)
                {
                    myEpInfo.MaxStreams -= 1;
                    if (myEpInfo.MaxStreams == 0)
                        Printf(Tee::PriWarn, "Device support Max Stream Array size of 2 which is not supported by HC");
                }
            }
            else if ( myEpInfo.Type == XhciEndpointTypeIsochOut || myEpInfo.Type == XhciEndpointTypeIsochIn )
            {   // is this is a Iso, Get Mult
                // Max number of packets within a service interval = MaxBurst * (Mult +1)
                myEpInfo.Mult = pTmpS->Attibutes & 0x3;
            }
            // Get BytesperInterval for periodic ep
            // NOte: Usn3's description on this field is identical to the explanation of Mps in Usb2
            if (myEpInfo.Type == XhciEndpointTypeInterruptOut || myEpInfo.Type == XhciEndpointTypeInterruptIn
                || myEpInfo.Type == XhciEndpointTypeIsochOut || myEpInfo.Type == XhciEndpointTypeIsochIn)
            {
                    myEpInfo.BytePerInterval = pTmpS->BytePerInterval;
            }
        }
        pvEpInfo->push_back(myEpInfo);
    }

    if (pOffset)
    {
        * pOffset = offsetIntoEpDesc;
    }
    LOG_EXT();
    return rc;
}

RC UsbDevExt::PrintEpInfo(vector<EndpointInfo> *pvEpInfom, UINT32 Index, Tee::Priority Pri)
{
    if (Index > pvEpInfom->size())
    {
        Printf(Tee::PriError, "Index %u overflow\n", Index);
        return RC::BAD_PARAMETER;
    }

    UINT32 index = 0;
    if (Index)
        index = Index - 1;

    for (UINT32 i = index; i < pvEpInfom->size(); i++)
    {
        if ((*pvEpInfom)[i].IsInit != true)
        {
            Printf(Tee::PriError, "Data Structure un-init\n");
            return RC::BAD_PARAMETER;
        }
        Printf(Pri,"Endpoint %u, ", (*pvEpInfom)[i].EpNum);
        Printf(Pri,"Data %s, ", (*pvEpInfom)[i].IsDataOut?"Out":"In");
        Printf(Pri,"Type %u \n", (*pvEpInfom)[i].Type);
        Printf(Pri,"SyncType %u, ", (*pvEpInfom)[i].SyncType);
        Printf(Pri,"UsageType %u \n", (*pvEpInfom)[i].UsageType);
        Printf(Pri,"Mps %u, ", (*pvEpInfom)[i].Mps);
        Printf(Pri,"XferPerMFrame %u \n", (*pvEpInfom)[i].TransPerMFrame);
        Printf(Pri,"Attrib %u, ", (*pvEpInfom)[i].Attrib);
        Printf(Pri,"Interval %u \n", (*pvEpInfom)[i].Interval);

        if ((*pvEpInfom)[i].IsSsCompanionAvailable)
        {
            Printf(Pri,"MaxBurst %u,", (*pvEpInfom)[i].MaxBurst);
            Printf(Pri,"MaxStreams %u \n", (*pvEpInfom)[i].MaxStreams);
            Printf(Pri,"Mult %u,", (*pvEpInfom)[i].Mult);
            Printf(Pri,"BytePerInterval %u \n", (*pvEpInfom)[i].BytePerInterval);
        }

        if (Index)
            break;  // print only one Ep info
    }
    return OK;
}

RC UsbDevExt::PrintDescrString(XhciController * pHost, UINT08 SlotId, UINT08 StrIndex, UINT08 Index, Tee::Priority Pri)
{
    LOG_ENT();
    RC rc = OK;
    void * pBuf;
    UINT08 * pBuf08;

    if (0 == StrIndex)
    {
        Printf(Tee::PriDebug, "Invalid StrIndex !!! \n");
        return OK;
    }

    CHECK_RC( MemoryTracker::AllocBuffer(1024, &pBuf, true, 32, Memory::UC) );
    Memory::Fill08(pBuf, 0, 1024);
    CHECK_RC( pHost->ReqGetDescriptor(SlotId, UsbDescriptorString, StrIndex, pBuf, 128) );
    pBuf08 = (UINT08 *) pBuf;
    // data dump
    Memory::Print08(pBuf, 32, Tee::PriDebug);
    if ( MEM_RD08(pBuf08) == 0 )
    {   // maybe we issue the bad LANGID, valid LANGIDs could be get from String Index 0
        Printf(Tee::PriDebug, "BAD String Desctriptor: \n");
        MemoryTracker::FreeBuffer(pBuf);
        return OK;
    }
    if ( MEM_RD08(pBuf08+1) != UsbDescriptorString )
    {
        Printf(Tee::PriWarn, "Get %d of STRING fail, type = %d, expected %d", StrIndex, MEM_RD08(pBuf08+1), UsbDescriptorString);
    }

    switch(Index)
    {
        case 14:
            Printf(Pri,"Manufactory: ");
            break;
        case 15:
            Printf(Pri,"Product: ");
            break;
        case 16:
            Printf(Pri,"Serial No.: ");
            break;
    }

    for ( UINT32 i = 2; i<MEM_RD08(pBuf08); i+=2 )
    {
        Printf(Pri,"%c", *(pBuf08+i));
    }
    Printf(Pri,"\n");
    MemoryTracker::FreeBuffer(pBuf);

    LOG_EXT();
    return rc;
}

RC UsbDevExt::PrintInfo(Tee::Priority Pri)
{
    const char * strSpeed[] = {"Undefined","Full speed","Low speed","High speed","Super speed",
                "Reserved","Reserved","Reserved","Reserved","Reserved",
                "Reserved","Reserved","Reserved","Reserved","Reserved",
                "Unknown/Error"};

    Printf(Pri, " USB SlotID %d, Protocol Version %x, %s(Speed %d) \n",
           m_SlotId, m_UsbVer, strSpeed[m_Speed & 0xf], m_Speed);
    return OK;
}

RC UsbDevExt::FindNextDescriptor(UINT08* pBuffer, INT32 Size, UINT08 Dt, UINT32 *pOff)
{
    RC rc;
    if (!pOff)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    UINT08 type;
    UINT08 length;

    // Find the next descriptor of type Dt
    *pOff = 0;
    while (Size > 0)
    {
        CHECK_RC(GetDescriptorTypeLength(pBuffer + *pOff, &type, &length));
        if ((type == Dt) && (*pOff))
        {
            break;  // found it
        }
        if (0 == length)
        {   // protect for infinite loop
            Printf(Tee::PriError, "Bad length 0\n");
            return RC::BAD_PARAMETER;
        }

        *pOff += length;
        Size -= length;
    }

    if (Size <= 0)
    {
        Printf(Tee::PriError, "Descriptor Type %d not found, %s\n", Dt, (Size != 0)?"with Error":"");
        return RC::HW_STATUS_ERROR;
    }
    return OK;
}

RC UsbDevExt::GetDescriptorTypeLength(UINT08* pBuffer,
                                      UINT08* pType,
                                      UINT08* pLength,
                                      UsbDescriptorType ExpectType)
{
    LOG_ENT();
    if (!pBuffer || !pType || !pLength)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    *pLength = MEM_RD08(pBuffer);
    *pType = MEM_RD08(pBuffer + 1);

    if (UsbDescriptorNA != ExpectType)
    {   // protect
        if (ExpectType != *pType)
        {
            Printf(Tee::PriError, "Bad Descriptor Type %d, expected %d. \n", *pType, ExpectType);
            Memory::Print08(pBuffer, *pLength);
            return RC::BAD_PARAMETER;
        }
        if ( 0 == *pLength)
        {
            Printf(Tee::PriError, "Bad Length %d\n", *pLength);
            return RC::BAD_PARAMETER;
        }
    }
    Printf(Tee::PriDebug, "(Descriptor Type = %d, Length = %d)", *pType, * pLength);
    LOG_EXT();
    return OK;
}

//-----------------------------------------------------------------------------
// USB Mass Storage Device
//-----------------------------------------------------------------------------
UsbStorage::UsbStorage(XhciController * pHost, UINT08 SlotId, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString)
:UsbDevExt(pHost, SlotId, UsbVer, Speed, RouteString)
{
    m_MaxLBA        = 0;
    m_SectorSize    = 0;
    m_InEp          = 0;
    m_OutEp         = 0;

    m_DevClass  = UsbClassMassStorage;
}

UsbStorage::~UsbStorage()
{
}

RC UsbStorage::PrintInfo(Tee::Priority Pri)
{
    if (!m_IsInit)
    {
        Printf(Tee::PriError, "Device hasn't been initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    Printf(Pri, "USB Storage Device Info: \n");
    UsbDevExt::PrintInfo(Pri);
    Printf(Pri, " Max LBA %u(0x%x), Sector Size %u(0x%x),",
           m_MaxLBA, m_MaxLBA, m_SectorSize, m_SectorSize);

    float total;
    const char * unitStr = "MB";
    total = (float)m_MaxLBA;
    total *= (float)m_SectorSize;
    total /= (float)0x100000; //MB
    if (total > 1024.0)
    {
        total /= 1024.0;
        unitStr = "GB";
    }
    if (total > 1024.0)
    {
        total /= 1024.0;
        unitStr = "TB";
    }
    Printf(Pri, " Capacity %.2f%s \n ", total, unitStr);

    Printf(Pri, " In Endpoint %d, Out Endpoint %d \n", m_InEp, m_OutEp);
    return OK;
}

RC UsbStorage::Init(vector<EndpointInfo> *pvEpInfo)
{
    LOG_ENT();
    RC rc = OK;
    /*
    if (2 != pvEpInfo->size())
    {
        Printf(Tee::PriError, "USB storage device invalid number of Endpoint %d, should be 2", (UINT32)pvEpInfo->size());
        return RC::HW_STATUS_ERROR;
    }
    */
    PrintEpInfo(pvEpInfo);

    // Get the In EP & Out EP one by one
    for (UINT32 i = 0; i< pvEpInfo->size(); i++ )
    {
        if ( (*pvEpInfo)[i].Type == XhciEndpointTypeBulkOut )
        {
            if (m_OutEp)
            {
                Printf(Tee::PriWarn, "More than one output Endpoint Descriptor exist");
            }
            m_OutEp = (*pvEpInfo)[i].EpNum;
        }
        else if ( (*pvEpInfo)[i].Type == XhciEndpointTypeBulkIn )
        {
            if (m_InEp)
            {
                Printf(Tee::PriWarn, "More than one input Endpoint Descriptor exist");
            }
            m_InEp =  (*pvEpInfo)[i].EpNum;
        }
    }

    if ( ( 0 == m_InEp )||( 0 == m_OutEp ) )
    {
        Printf(Tee::PriError, "Endpoint Descriptor is missing. InEp  = %d, OutEp = %d\n", m_InEp, m_OutEp);
        return RC::HW_STATUS_ERROR;
    }
    else
    {
        m_IsInit = true;
        Printf(Tee::PriLow, "InEp  = %d, OutEp = %d\n", m_InEp, m_OutEp);
    }
    if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) )
    {
        Printf(Tee::PriLow,  "Slow Enum \n");
    }

    CHECK_RC( CmdInq(NULL, 36) );
    if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) ) {  Tasker::Sleep(200); }
    CHECK_RC( CmdTestUnitReady() );
    if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) ) {  Tasker::Sleep(200); }
    if ( XHCI_ENUM_MODE_INSERT_SENSE & m_pHost->GetEnumMode() )
    {
        CHECK_RC( CmdSense() );
        if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) ) {  Tasker::Sleep(200); }
    }
    rc = CmdReadCapacity(&m_MaxLBA, &m_SectorSize);
    if (OK != rc)
    {   // if stalled, we try it again
        Printf(Tee::PriLow, "trying Mass Storage Init Approach 2\n");
        if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) ) {  Tasker::Sleep(200); }
        CHECK_RC( CmdTestUnitReady() );
        if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) ) {  Tasker::Sleep(200); }
        CHECK_RC( CmdSense() );
        if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) ) {  Tasker::Sleep(200); }
        rc = CmdReadCapacity(&m_MaxLBA, &m_SectorSize);
    }
    if (OK != rc)
    {   // if stalled, we try it for last time
        Printf(Tee::PriLow, "trying Mass Storage Init Approach 3\n");
        if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) ) {  Tasker::Sleep(200); }
        CHECK_RC( CmdTestUnitReady() );
        if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) ) {  Tasker::Sleep(200); }
        CHECK_RC( CmdInq(NULL, 36) );
        if ( !(XHCI_DEBUG_MODE_FAST_TRANS & m_pHost->GetDebugMode()) ) {  Tasker::Sleep(200); }
        CHECK_RC( CmdReadCapacity(&m_MaxLBA, &m_SectorSize) );
    }

    LOG_EXT();
    return rc;
}

RC UsbStorage::GetSectorSize(UINT32 * pSize)
{
    if ( !pSize )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( 0 == m_SectorSize )
    {
        Printf(Tee::PriError, "SectorSize Not Initialized, call InitDrive() first\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    * pSize = m_SectorSize;
    return OK;
}

RC UsbStorage::GetMaxLba(UINT32 * pMaxLba)
{
    if ( !pMaxLba )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( 0 == m_MaxLBA )
    {
        Printf(Tee::PriError, "Max Lba Not Initialized, call InitDrive() first\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    * pMaxLba = m_MaxLBA;
    return OK;
}

// fill the first 15 Bytes of Command Block Wraper
RC UsbStorage::SetupCmdCommon(vector<UINT08> * pvCmd, UINT32 xferBytes, bool IsOut, UINT08 CmdLen)
{
    RC rc = OK;

    pvCmd->clear();
    // signature
    pvCmd->push_back(0x55);
    pvCmd->push_back(0x53);
    pvCmd->push_back(0x42);
    pvCmd->push_back(0x43);
    // Tag
    pvCmd->push_back(0xba);
    pvCmd->push_back(0xbe);
    pvCmd->push_back(0xfa);
    pvCmd->push_back(0xce);
    // Data Length
    pvCmd->push_back((xferBytes >>  0) & 0xff);
    pvCmd->push_back((xferBytes >>  8) & 0xff);
    pvCmd->push_back((xferBytes >> 16) & 0xff);
    pvCmd->push_back((xferBytes >> 24) & 0xff);
    // flag - READ
    if ( IsOut )
    {
        pvCmd->push_back(0x00);
    }
    else
    {
        pvCmd->push_back(0x80);
    }
    // LUN
    pvCmd->push_back(0x0);
    // CMD length
    pvCmd->push_back(CmdLen);

    // padding to 31 Bytes
    for(UINT32 i = 0; i< 16; i++)
    {
        pvCmd->push_back(0);
    }
    return rc;
}

RC UsbStorage::CmdRead10(UINT32 LBA,
                         UINT32 NumSectors,
                         MemoryFragment::FRAGLIST * pFragList,
                         vector<UINT08> *pvCbw)
{
    LOG_ENT();
    RC rc = OK;
    if ( NumSectors > 0xffff )
    {   // this value gets from experiments
        Printf(Tee::PriError, "NumSectors = %d, Max number supported by Read10 is 0xffff\n", NumSectors);
        return RC::BAD_PARAMETER;
    }
    UINT32 maxLba;
    CHECK_RC(GetMaxLba( &maxLba ));
    if ( LBA > maxLba )
    {
        Printf(Tee::PriError, "Lba = %d(0x%x), Max Lba for this device is %d(0x%x)\n", LBA, LBA, maxLba, maxLba);
        return RC::BAD_PARAMETER;
    }

    vector<UINT08> vCmd;
    UINT32 sectorSize;
    CHECK_RC(GetSectorSize(&sectorSize));
    SetupCmdCommon(&vCmd, sectorSize * NumSectors, false, 10 );
    // for SCSI Read10 command
    vCmd[15] = 0x28;   // Read10
    vCmd[16] = 0x00;
    // LBA
    vCmd[17] = (LBA >> 24) & 0xff;
    vCmd[18] = (LBA >> 16) & 0xff;
    vCmd[19] = (LBA >>  8) & 0xff;
    vCmd[20] = (LBA >>  0) & 0xff;
    vCmd[21] = 0x00;
    // Size
    vCmd[22] = (NumSectors >>  8) & 0xff;
    vCmd[23] = (NumSectors >>  0) & 0xff;
    vCmd[24] = 0x00;

    //VectorData::Print(&vCmd);
    if (pvCbw)
    {
        for(UINT32 i = 0; i<vCmd.size(); i++)
        {
            pvCbw->push_back(vCmd[i]);
        }
        return OK;
    }

    rc = BulkTransfer(&vCmd, false, pFragList);
    LOG_EXT();
    return rc;
}

RC UsbStorage::CmdRead16(UINT32 LBA,
                         UINT32 NumSectors,
                         MemoryFragment::FRAGLIST * pFragList,
                         vector<UINT08> *pvCbw)
{
    LOG_ENT();
    RC rc = OK;
    UINT32 maxLba;
    CHECK_RC(GetMaxLba( &maxLba ));
    if ( ( LBA + NumSectors ) > maxLba )
    {
        Printf(Tee::PriError, "Lba = %d(0x%x), Max Lba for this device is %d(0x%x)\n", LBA, LBA, maxLba, maxLba);
        return RC::BAD_PARAMETER;
    }

    vector<UINT08> vCmd;
    UINT32 sectorSize;
    CHECK_RC(GetSectorSize(&sectorSize));
    SetupCmdCommon(&vCmd, sectorSize * NumSectors, false, 12 );

    vCmd[15] = 0x88;   // Read16
    vCmd[16] = 0x00;
    // LBA
    vCmd[17] = 0x00;
    vCmd[18] = 0x00;
    vCmd[19] = 0x00;
    vCmd[20] = 0x00;
    vCmd[21] = (LBA >> 24) & 0xff;
    vCmd[22] = (LBA >> 16) & 0xff;
    vCmd[23] = (LBA >>  8) & 0xff;
    vCmd[24] = (LBA >>  0) & 0xff;
    // sector number
    vCmd[25] = (NumSectors >>  24) & 0xff;
    vCmd[26] = (NumSectors >>  16) & 0xff;
    vCmd[27] = (NumSectors >>   8) & 0xff;
    vCmd[28] = (NumSectors >>   0) & 0xff;
    // group
    vCmd[29] = 0x00;
    // control
    vCmd[30] = 0x00;

    if (pvCbw)
    {
        for(UINT32 i = 0; i<vCmd.size(); i++)
        {
            pvCbw->push_back(vCmd[i]);
        }
        return OK;
    }

    rc = BulkTransfer(&vCmd, false, pFragList);
    LOG_EXT();
    return rc;
}

RC UsbStorage::CmdWrite10(UINT32 LBA,
                          UINT32 NumSectors,
                          MemoryFragment::FRAGLIST * pFragList,
                          vector<UINT08> *pvCbw)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(Lba %d, NumSectors %d)\n", LBA, NumSectors);
    RC rc = OK;
    if ( NumSectors > 0xffff )
    {   // this value gets from experiments
        Printf(Tee::PriError, "NumSectors = %d, Max number supported by Write10 is 0xffff\n", NumSectors);
        return RC::BAD_PARAMETER;
    }
    UINT32 maxLba;
    CHECK_RC(GetMaxLba( &maxLba ));
    if ( LBA > maxLba )
    {
        Printf(Tee::PriError, "Lba = %d(0x%x), Max Lba for this device is %d(0x%x)\n", LBA, LBA, maxLba, maxLba);
        return RC::BAD_PARAMETER;
    }

    vector<UINT08> vCmd;
    UINT32 sectorSize;
    CHECK_RC(GetSectorSize(&sectorSize));
    SetupCmdCommon(&vCmd, sectorSize * NumSectors, true, 10 );
    vCmd[15] = 0x2A;   // Write10
    vCmd[16] = 0x00;
    // LBA
    vCmd[17] = (LBA >> 24) & 0xff;
    vCmd[18] = (LBA >> 16) & 0xff;
    vCmd[19] = (LBA >>  8) & 0xff;
    vCmd[20] = (LBA >>  0) & 0xff;
    vCmd[21] = 0x00;
    // Size
    vCmd[22] = (NumSectors >>  8) & 0xff;
    vCmd[23] = (NumSectors >>  0) & 0xff;
    vCmd[24] = 0x00;

    //VectorData::Print(&vCmd);
    if (pvCbw)
    {
        for(UINT32 i = 0; i<vCmd.size(); i++)
        {
            pvCbw->push_back(vCmd[i]);
        }
        return OK;
    }

    rc = BulkTransfer(&vCmd, true, pFragList);
    LOG_EXT();
    return rc;
}

RC UsbStorage::CmdWrite16(UINT32 LBA,
                          UINT32 NumSectors,
                          MemoryFragment::FRAGLIST * pFragList,
                          vector<UINT08> *pvCbw)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(Lba %d, NumSectors %d)\n", LBA, NumSectors);
    RC rc = OK;
    UINT32 maxLba;
    CHECK_RC(GetMaxLba( &maxLba ));
    if ( ( LBA + NumSectors ) > maxLba )
    {
        Printf(Tee::PriError, "Lba = %d(0x%x), Max Lba for this device is %d(0x%x)\n", LBA, LBA, maxLba, maxLba);
        return RC::BAD_PARAMETER;
    }

    vector<UINT08> vCmd;
    UINT32 sectorSize;
    CHECK_RC(GetSectorSize(&sectorSize));
    SetupCmdCommon(&vCmd, sectorSize * NumSectors, true, 10 );
    vCmd[15] = 0x8a;   // Write16
    vCmd[16] = 0x00;
    // LBA
    vCmd[17] = 0x00;
    vCmd[18] = 0x00;
    vCmd[19] = 0x00;
    vCmd[20] = 0x00;
    vCmd[21] = (LBA >> 24) & 0xff;
    vCmd[22] = (LBA >> 16) & 0xff;
    vCmd[23] = (LBA >>  8) & 0xff;
    vCmd[24] = (LBA >>  0) & 0xff;
    // sector number
    vCmd[25] = (NumSectors >>  24) & 0xff;
    vCmd[26] = (NumSectors >>  16) & 0xff;
    vCmd[27] = (NumSectors >>   8) & 0xff;
    vCmd[28] = (NumSectors >>   0) & 0xff;
    // group
    vCmd[29] = 0x00;
    // control
    vCmd[30] = 0x00;

    if (pvCbw)
    {
        for(UINT32 i = 0; i<vCmd.size(); i++)
        {
            pvCbw->push_back(vCmd[i]);
        }
        return OK;
    }

    rc = BulkTransfer(&vCmd, true, pFragList);
    LOG_EXT();
    return rc;
}

RC UsbStorage::CmdReadCapacity(UINT32 *pMaxLBA, UINT32 *pSectorSize)
{
    LOG_ENT();
    RC rc = OK;

    vector<UINT08> vCmd;
    UINT32 xferSize = 8;    // 8 Bytes return data
    SetupCmdCommon(&vCmd, 8, false, 10 );
    // for SCSI Read10 command
    vCmd[15] = 0x25;   // READ Capacity
    vCmd[16] = 0x00;

    //VectorData::Print(&vCmd);
    UINT08 * pVa = NULL;
    CHECK_RC(MemoryTracker::AllocBufferAligned(xferSize, (void **)&pVa, 4096, 32, Memory::UC));
    Memory::Fill32(pVa, 0, xferSize / 4);

    rc = BulkTransfer(&vCmd, false, pVa, xferSize);
    *pMaxLBA = (MEM_RD08(pVa+0) << 24) | (MEM_RD08(pVa+1) << 16) | (MEM_RD08(pVa+2) << 8) | MEM_RD08(pVa+3);
    *pSectorSize = (MEM_RD08(pVa+4) << 24) | (MEM_RD08(pVa+5) << 16) | (MEM_RD08(pVa+6) << 8) | MEM_RD08(pVa+7);

    Memory::Print08(pVa, xferSize, Tee::PriLow);
    Printf(Tee::PriLow, "MaxLBA %d, SectorSize %d", *pMaxLBA, *pSectorSize);

    MemoryTracker::FreeBuffer(pVa);
    LOG_EXT();
    return rc;
}

RC UsbStorage::CmdReadFormatCapacity(UINT32 *pNumBlocks, UINT32 *pBlockSize)
{
    LOG_ENT();
    RC rc = OK;

    vector<UINT08> vCmd;
    UINT32 xferSize = 12;    // 12 Bytes return data
    SetupCmdCommon(&vCmd, xferSize, false, 12 );
    // for SCSI Read10 command
    vCmd[15] = 0x23;   // READ Format Capacity
    vCmd[16] = 0x00;
    vCmd[17] = 0x00;
    vCmd[18] = 0x00;
    vCmd[19] = 0x00;
    vCmd[20] = 0x00;
    vCmd[21] = 0x00;
    vCmd[22] = 0x00;
    vCmd[23] = xferSize;

    //VectorData::Print(&vCmd);
    UINT08 * pVa = NULL;
    CHECK_RC(MemoryTracker::AllocBufferAligned(xferSize, (void **)&pVa, 4096, 32, Memory::UC));
    Memory::Fill32(pVa, 0, xferSize / 4);

    rc = BulkTransfer(&vCmd, false, pVa, xferSize);
    *pNumBlocks = (MEM_RD08(pVa+4) << 24) | (MEM_RD08(pVa+5) << 16) | (MEM_RD08(pVa+6) << 8) | MEM_RD08(pVa+7);
    *pBlockSize = (MEM_RD08(pVa+8) << 24) | (MEM_RD08(pVa+9) << 16) | (MEM_RD08(pVa+10) << 8) | MEM_RD08(pVa+11);

    Memory::Print32(pVa, xferSize, Tee::PriLow);
    Printf(Tee::PriLow, "NumBlocks %d, BlockSize %d", *pNumBlocks, *pBlockSize);

    MemoryTracker::FreeBuffer(pVa);
    LOG_EXT();
    return rc;
}
RC UsbStorage::CmdTestUnitReady()
{
    LOG_ENT();
    RC rc = OK;

    vector<UINT08> vCmd;
    SetupCmdCommon(&vCmd, 0, false, 6 );
    // for SCSI Read10 command
    vCmd[15] = 0x00;   // Test Unit Ready
    // VectorData::Print(&vCmd);

    rc = BulkTransfer(&vCmd, false, NULL, 0);
    LOG_EXT();
    return rc;
}

RC UsbStorage::CmdInq(void * pBuf, UINT08 Len)
{
    LOG_ENT();
    RC rc = OK;
    UINT08 xferSize = 36;
    if (Len != xferSize)
    {
        Printf(Tee::PriError, "Bad data length %d, expect %d\n", Len, xferSize);
        return RC::SOFTWARE_ERROR;
    }

    if (pBuf == NULL)
    {   // this is a dummy inq
        CHECK_RC(MemoryTracker::AllocBufferAligned(xferSize, (void **)&pBuf, 4096, 32, Memory::UC));
    }

    vector<UINT08> vCmd;
    SetupCmdCommon(&vCmd, xferSize, false, 6 );
    // for SCSI Read10 command
    vCmd[15] = 0x12;   // INQUIRY
    vCmd[16] = 0x00;
    // Data Length
    vCmd[17] = 0x00;
    vCmd[18] = 0x00;
    vCmd[19] = xferSize;
    vCmd[20] = 0x00;

    Memory::Fill32(pBuf, 0, xferSize / 4);

    rc = BulkTransfer(&vCmd, false, pBuf, xferSize);
    Memory::Print08(pBuf, xferSize, Tee::PriLow);
    MemoryTracker::FreeBuffer(pBuf);
    LOG_EXT();
    return rc;
}

RC UsbStorage::CmdModeSense(void * pBuf, UINT08 Len)
{
    RC rc = OK;
    UINT08 * pVa = NULL;
    UINT08 xferSize = 0xc0;

    if ( pBuf && Len != xferSize)
    {
        Printf(Tee::PriError, "Bad data length %d, expect %d\n", Len, xferSize);
        return RC::SOFTWARE_ERROR;
    }

    if ( !pBuf )
    {
        CHECK_RC(MemoryTracker::AllocBufferAligned(xferSize, (void **)&pVa, 4096, 32, Memory::UC));
        Memory::Fill08(pVa, 0, xferSize);
    }
    else
    {
        Memory::Fill08(pBuf, 0, xferSize);
    }

    vector<UINT08> vCmd;
    SetupCmdCommon(&vCmd, xferSize, false, 6 );
    vCmd[15] = 0x1a;   // Request Sense
    vCmd[16] = 0x00;
    // Data Length
    vCmd[17] = 0x3f;
    vCmd[18] = 0x00;
    vCmd[19] = xferSize;
    vCmd[20] = 0x00;

    if ( pBuf )
    {
        rc = BulkTransfer(&vCmd, false, pBuf, xferSize);
    }
    else
    {
        rc = BulkTransfer(&vCmd, false, pVa, xferSize);
        Memory::Print08(pVa, xferSize, Tee::PriLow);
        MemoryTracker::FreeBuffer(pVa);
    }
    return rc;
}

RC UsbStorage::CmdSense(void * pBuf, UINT08 Len)
{
    LOG_ENT();
    RC rc = OK;
    UINT08 * pVa = NULL;
    UINT08 xferSize = 252;

    if ( pBuf && Len != xferSize)
    {
        Printf(Tee::PriError, "Bad data length %d, expect %d\n", Len, xferSize);
        return RC::SOFTWARE_ERROR;
    }

    if ( !pBuf )
    {
        CHECK_RC(MemoryTracker::AllocBufferAligned(xferSize, (void **)&pVa, 4096, 32, Memory::UC));
        Memory::Fill08(pVa, 0, xferSize);
    }
    else
    {
        Memory::Fill08(pBuf, 0, xferSize);
    }

    vector<UINT08> vCmd;
    SetupCmdCommon(&vCmd, xferSize, false, 6 );
    vCmd[15] = 0x03;   // Request Sense
    vCmd[16] = 0x00;
    // Data Length
    vCmd[17] = 0x00;
    vCmd[18] = 0x00;
    vCmd[19] = xferSize;
    vCmd[20] = 0x00;

    if ( pBuf )
    {
        rc = BulkTransfer(&vCmd, false, pBuf, xferSize);
    }
    else
    {
        rc = BulkTransfer(&vCmd, false, pVa, xferSize);
        Memory::Print08(pVa, xferSize, Tee::PriLow);
        UINT08 tmp = MEM_RD08(pVa);
        if ( 0x70!= tmp )
        {
            Printf(Tee::PriWarn, "Sense Data from device seems invalid. First Byte = 0x%02x, should be 0x70 \n ", tmp);
        }
        MemoryTracker::FreeBuffer(pVa);
    }
    LOG_EXT();
    return rc;
}

RC UsbStorage::CmdGetMaxLun(UINT32 * pMaxLun)
{
    RC rc = OK;
    UINT08 xferSize = 8;

    vector<UINT08> vCmd;
    SetupCmdCommon(&vCmd, xferSize, false, 12 );
    // for report Luns command
    vCmd[15] = 0xA0;
    vCmd[16] = 0x00;
    // Data Length
    vCmd[17] = 0x00;
    vCmd[18] = 0x00;
    vCmd[19] = 0x00;
    vCmd[20] = 0x00;
    // length
    vCmd[21] = 0x00;
    vCmd[22] = 0x00;
    vCmd[23] = 0x00;
    vCmd[24] = xferSize;

    //VectorData::Print(&vCmd);
    void * pBuf;
    CHECK_RC(MemoryTracker::AllocBufferAligned(xferSize, (void **)&pBuf, 4096, 32, Memory::UC));
    Memory::Fill08(pBuf, 0, xferSize);

    rc = BulkTransfer(&vCmd, false, pBuf, xferSize);
    * pMaxLun = MEM_RD32(pBuf);
    Printf(Tee::PriNormal, "Max Lun %d\n", * pMaxLun);

    MemoryTracker::FreeBuffer(pBuf);
    return rc;
}

RC UsbStorage::BulkTransfer(vector<UINT08> *pvCBW,
                            bool IsDataOut,
                            void * pDataBuf, UINT32 Len)
{
    // construct a fraglist
    MemoryFragment::FRAGLIST myFragList;
    MemoryFragment::FRAGMENT myFrag = {};

    UINT32 fragSize = m_pHost->GetOptimisedFragLength();
    while(Len)
    {
        UINT32 txSize = (Len > fragSize)?fragSize:Len;
        myFrag.Address = pDataBuf;
        myFrag.ByteSize = txSize;
        myFragList.push_back(myFrag);

        pDataBuf = (void*) ((UINT08*)pDataBuf + txSize);
        Len -= txSize;
    }
    // Create Data TDs
    return BulkTransfer(pvCBW, IsDataOut, &myFragList);
}

RC UsbStorage::XferCbw(vector<UINT08> *pvCbw, UINT32 * pCompletionCode, EP_INFO *pEpInfo)
{
    LOG_ENT();
    RC rc = OK;
    if(!pvCbw)
    {
        Printf(Tee::PriError, "CBW null pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    else if(pvCbw->size() != 31)
    {
        Printf(Tee::PriError, "Invalid CBW length %d, expect 31 \n", (UINT32)pvCbw->size());
        return RC::SOFTWARE_ERROR;
    }

    // CBW phase
    void * pVa;
    CHECK_RC(MemoryTracker::AllocBuffer(pvCbw->size(), &pVa, true, 32, Memory::UC));
    Memory::Fill(pVa, pvCbw);
    Printf(Tee::PriLow, "CBW Data:\n");
    Memory::Print08(pVa, (UINT32)pvCbw->size(), Tee::PriLow);

    MemoryFragment::FRAGLIST myFragList;
    MemoryFragment::FRAGMENT myFrag = {};
    // send out the CBW
    myFrag.Address = pVa;
    myFrag.ByteSize = pvCbw->size();
    myFragList.push_back(myFrag);

    rc = m_pHost->SetupTd(m_SlotId, m_OutEp, true, &myFragList);
    if ((OK == rc) && pEpInfo)
    {   // caller wants to wait for Xfer and release memory by itself
        pEpInfo->EpNum = m_OutEp;
        pEpInfo->IsDataOut = true;
        pEpInfo->pBufAddress = pVa;
        return OK;
    }
    if (OK == rc)
    {
        rc = m_pHost->WaitXfer(m_SlotId, m_OutEp, true, m_pHost->GetTimeout(), pCompletionCode);
    }
    MemoryTracker::FreeBuffer(pVa);
    LOG_EXT();
    if (OK != rc)
    {
        Printf(Tee::PriError, "Failed on sending CBW\n");
    }
    return rc;
}

RC UsbStorage::ResetRecovery()
{   // implementation of 5.3.4 Reset Recovery
//For Reset Recovery the host shall issue in the following order: :
//(a) a Bulk-Only Mass Storage Reset
//(b) a Clear Feature HALT to the Bulk-In endpoint
//(c) a Clear Feature HALT to the Bulk-Out endpoint
    LOG_ENT();
    RC rc;
    CHECK_RC(m_pHost->ReqMassStorageReset(m_SlotId));
    CHECK_RC(m_pHost->ReqClearFeature(m_SlotId, 0, UsbRequestEndpoint, m_InEp | 0x80));
    CHECK_RC(m_pHost->ReqClearFeature(m_SlotId, 0, UsbRequestEndpoint, m_OutEp));
    Tasker::Sleep(500);
    Printf(Tee::PriLow,"ResetRecovery Done on Slot %d \n", m_SlotId);
    LOG_EXT();
    return OK;
}

RC UsbStorage::BulkTransfer(vector<UINT08> *pvCBW,
                            bool IsDataOut,
                            MemoryFragment::FRAGLIST * pFragList)
{   // 1 CBW  2, Data (in/Out), 3, CSW
    LOG_ENT();
    Printf(Tee::PriDebug,"(CBW size %d, IsDataOut %s)\n", (UINT32)pvCBW->size(), IsDataOut?"T":"F");

    UINT32 completionCode;
    RC rc = OK;
    bool isStall = false;

    CHECK_RC(XferCbw(pvCBW,  &completionCode));
    bool isStallTmp;
    CHECK_RC(m_pHost->ValidateCompCode(completionCode,  &isStallTmp));
    if (isStallTmp)
    {
        Printf(Tee::PriLow, "STALL on CBW, retry... ");
        ResetRecovery();
        // retry for one more time
        CHECK_RC(XferCbw(pvCBW,  &completionCode));
        CHECK_RC(m_pHost->ValidateCompCode(completionCode,  &isStallTmp));
        if (isStallTmp)
        {
            Printf(Tee::PriError, "Un-recoverable STALL on CBW \n");
            return RC::HW_STATUS_ERROR;
        }
        else
        {
            Printf(Tee::PriLow, "Succeed \n");
        }
    }

    if (pFragList->size())
    {
        CHECK_RC( m_pHost->SetupTd(m_SlotId, IsDataOut?m_OutEp:m_InEp, IsDataOut, pFragList) );
        rc = m_pHost->WaitXfer(m_SlotId, IsDataOut?m_OutEp:m_InEp, IsDataOut, m_pHost->GetTimeout(), &completionCode);
        if ( OK == rc )
        {
            CHECK_RC(m_pHost->ValidateCompCode(completionCode,  &isStallTmp));
            if (isStallTmp)
            {
                Printf(Tee::PriLow, "STALL in Data Phase\n");
                isStall = true;
            }
        }
    }

    // CSW phase
    // allocate mem for CSW
    CHECK_RC(XferCsw(&completionCode));
    CHECK_RC(m_pHost->ValidateCompCode(completionCode,  &isStallTmp));
    if (isStallTmp)
    {
        Printf(Tee::PriNormal, "STALL on CSW, retry \n");
        // retry CSW according to 6.7.2 Hi - Host expects to receive data from the device
        CHECK_RC(XferCsw(&completionCode));
        CHECK_RC(m_pHost->ValidateCompCode(completionCode,  &isStallTmp));
        if (isStallTmp)
        {
            Printf(Tee::PriError, "STALL twice on CSW\n");
            return RC::HW_STATUS_ERROR;
        }
    }

    LOG_EXT();
    if (OK == rc && isStall)
    {   // if stalled on data stage, it's still a fail even recovered since we don't get any data
        rc = RC::HW_STATUS_ERROR;
    }
    return rc;
}

RC UsbStorage::XferCsw(UINT32 * pCompletionCode,
                       UINT32 *pCSWDataResidue,
                       UINT08 *pCSWStatus,
                       EP_INFO * pEpInfo)
{
    LOG_ENT();
    RC rc = OK;
    MemoryFragment::FRAGLIST myFragList;
    MemoryFragment::FRAGMENT myFrag = {};

    void * pVa;
    CHECK_RC(MemoryTracker::AllocBuffer(13, &pVa, true, 32, Memory::UC));
    Memory::Fill08(pVa, 0xff, 13);
    myFrag.Address = pVa;
    myFrag.ByteSize = 13;
    myFragList.push_back(myFrag);

    rc = m_pHost->SetupTd(m_SlotId, m_InEp, false, &myFragList);
    if ((OK == rc) && pEpInfo)
    {
        pEpInfo->EpNum = m_InEp;
        pEpInfo->IsDataOut = false;
        pEpInfo->pBufAddress = pVa;
        return OK;
    }
    if (OK == rc)
    {
        rc = m_pHost->WaitXfer(m_SlotId, m_InEp, false, m_pHost->GetTimeout(), pCompletionCode);
    }

    if ((OK == rc)
        && (XhciTrbCmpCodeSuccess == *pCompletionCode))
    {
        UINT32 tmp = MEM_RD32((UINT08*)pVa + USB_CSW_OFFSET_SIGNATURE);
        if( tmp != 0x53425355 )
        {
            Printf(Tee::PriWarn, "Bad signiture 0x%08x, CSW dump, perform ResetRecovery...\n", tmp);
            Memory::Print08(pVa, 13);
            rc = ResetRecovery();    // perform reset recovery and head to next cbw
            //rc = RC::HW_STATUS_ERROR;
        }
        else
        {
            if (!pCSWStatus || !pCSWDataResidue)
            {   // handle the status
                tmp = MEM_RD08((UINT08*) pVa + USB_CSW_OFFSET_STATUS);
                if( 0 == (tmp & 0xff) )
                {   // Command Passed
                    Printf(Tee::PriLow, "CSW status 0x%02x, Command Passed \n", tmp);
                }
                else if ( 1 == (tmp & 0xff) )
                {   // Command Failed
                    if (MEM_RD32((UINT08*) pVa + USB_CSW_OFFSET_RESIDUE))
                    {
                        Printf(Tee::PriError,
                               "CSW status 0x%02x, Command Failed, residue = 0x%x \n",
                               tmp, MEM_RD32((UINT08*) pVa + USB_CSW_OFFSET_RESIDUE));
                        rc = RC::HW_STATUS_ERROR;
                    }
                    else
                    {
                        Printf(Tee::PriLow, "CSW status 0x%02x, Command Failed with 0 residue \n", tmp);
                    }
                }
                else
                {   // phase error
                    Printf(Tee::PriError,
                           "CSW status 0x%02x, phase error, perform ResetRecovery... \n", tmp);
                    rc = ResetRecovery();
                }
            }
            else
            {   // user wants to handle the status by themselves
                *pCSWStatus = MEM_RD08((UINT08*) pVa + USB_CSW_OFFSET_STATUS);
                *pCSWDataResidue = MEM_RD32((UINT08*) pVa + USB_CSW_OFFSET_RESIDUE);
            }
        }
    }

    MemoryTracker::FreeBuffer(pVa);
    LOG_EXT();
    if (OK != rc)
    {
        Printf(Tee::PriError, "Failed on receiving CSW\n");
    }
    return rc;
}

RC UsbStorage::GetInEpInfo(EP_INFO * pEpInfo)
{
    if (!m_IsInit)
    {
        Printf(Tee::PriError, "Device hasn't been initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    pEpInfo->EpNum = m_InEp;
    pEpInfo->IsDataOut = false;
    pEpInfo->pBufAddress = nullptr;
    return OK;
}

// Hid classs function
UsbHidDev::UsbHidDev(XhciController * pHost, UINT08 SlotId, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString)
:UsbDevExt(pHost, SlotId, UsbVer, Speed, RouteString)
{
    m_InEp = 0;
    m_DevClass  = UsbClassHid;
    m_Mps = 8;
}

UsbHidDev::~UsbHidDev()
{
}

RC UsbHidDev::Init(vector<EndpointInfo> *pvEpInfo)
{
    LOG_ENT();
    RC rc = OK;
    if (pvEpInfo->size() < 1)
    {
        Printf(Tee::PriError, "USB Hid device invalid number of Endpoint %d. Need at least one IN endpoint\n", (UINT32)pvEpInfo->size());
        return RC::HW_STATUS_ERROR;
    }
    PrintEpInfo(pvEpInfo);

    // Get the In EP & Out EP one by one
    for (UINT32 i = 0; i< pvEpInfo->size(); i++ )
    {
        if ( !(*pvEpInfo)[i].IsDataOut )
        {
            m_InEp  = (*pvEpInfo)[i].EpNum;
            m_Mps   = (*pvEpInfo)[i].Mps;
            break;
        }
    }

    if (m_Mps < 4)
    {   // protect for memory access
        Printf(Tee::PriError, "HID Device Mps < 4\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_IsInit = true;
    Printf(Tee::PriDebug, "Hid InEp = %d, Mps = %d", m_InEp, m_Mps);
    LOG_EXT();
    return rc;
}

// if user abort the function by pressing any key
// there will be a unused TD left
// what worse is the buffer pointed by the unused TD will be released when function exit
// thus we should inform host to do cleanup before release the buffer
RC UsbHidDev::ReadMouse(UINT32 RunSeconds)
{
    LOG_ENT();
    RC rc = OK;
    RC rcTmp = OK;

    UINT32 addrBit = 32;
    void * pVa = NULL;
    CHECK_RC(MemoryTracker::AllocBuffer(2 * m_Mps, &pVa, true, addrBit, Memory::UC));
    if (!pVa)
    {
        Printf(Tee::PriError, "Can't allocate memory\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    Memory::Fill08(pVa, 0, 2 * m_Mps);

    UINT08 *pReport = (UINT08*)pVa;
    UINT08 *pLastReport = pReport + m_Mps;

    MemoryFragment::FRAGMENT myFrag = {};
    myFrag.Address = pVa;
    myFrag.ByteSize = m_Mps;
    MemoryFragment::FRAGLIST myFragList;
    myFragList.push_back(myFrag);

    if (RunSeconds)
        Printf(Tee::PriNormal, "Running for: %d seconds\n", RunSeconds);

    Printf(Tee::PriNormal, "Start to listen report(please move mouse, press any key to abort) \n");

    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);
    UINT64 timeStartMs = Xp::GetWallTimeMS();

    // receive data cirlwlarly
    UINT32 index = 0;
    bool isSkipTrbAdd = false;
    while (true)
    {
        Memory::Fill08(pReport, 0, m_Mps);

        if (pConsole->KeyboardHit())
        {
            pConsole->GetKey();
            Printf(Tee::PriNormal, "User abort\n");
            break;
        }

        if (!isSkipTrbAdd)
        {   // add one TD (data buffer size = m_Mps) to the In Endpoint
            rc = m_pHost->SetupTd(m_SlotId, m_InEp, false, &myFragList);
            if ( OK != rc )
            {
                Printf(Tee::PriError, "Read Mouse Error\n");
                break;
            }
            // skip the subsequent TD adding
            isSkipTrbAdd = true;
        }
        rcTmp = m_pHost->WaitXfer(m_SlotId, m_InEp, false, 100, NULL, false, NULL, true);
        if ( rcTmp == OK )
        {
            isSkipTrbAdd = false;
            Printf(Tee::PriNormal, "Index 0x%x report: button 0x%x, rel_x %d, rel_y %d rel_wheel %d\n",
                          index++, MEM_RD08(pReport), MEM_RD08(pReport+1), MEM_RD08(pReport+2), MEM_RD08(pReport+3));
            if (MEM_RD08(pReport+0) & 0x1)
                Printf(Tee::PriNormal, "left button pressed\n");
            else if (MEM_RD08(pLastReport+0) & 0x1)
                Printf(Tee::PriNormal, "left button released\n");

            if (MEM_RD08(pReport+0) & 0x2)
                Printf(Tee::PriNormal, "right button pressed\n");
            else if (MEM_RD08(pLastReport+0) & 0x2)
                Printf(Tee::PriNormal, "right button released\n");

            if (MEM_RD08(pReport+0) & 0x4)
                Printf(Tee::PriNormal, "middle button pressed\n");
            else if (MEM_RD08(pLastReport+0) & 0x4)
                Printf(Tee::PriNormal, "middle button released\n");

            if (MEM_RD08(pReport+0) & 0x8)
                Printf(Tee::PriNormal, "side button pressed\n");
            else if (MEM_RD08(pLastReport+0) & 0x8)
                Printf(Tee::PriNormal, "side button released\n");

            if (MEM_RD08(pReport+0) & 0x10)
                Printf(Tee::PriNormal, "extra button pressed\n");
            else if (MEM_RD08(pLastReport+0) & 0x10)
                Printf(Tee::PriNormal, "extra button released\n");

            // record the Report
            for (UINT32 i = 0; i < 4; ++i)
            {
                MEM_WR08(pLastReport + i, MEM_RD08(pReport + i));
            }
        }

        UINT64  deltTimeSec = (Xp::GetWallTimeMS() - timeStartMs) / 1000;
        if (RunSeconds && (deltTimeSec >= RunSeconds))
        {
            Printf(Tee::PriNormal, "Time off \n");
            break;
        }
    }
    /*
    if (index > 2 || 0 == RunSeconds)
    {   // got two messages or abort by user
        rc = OK;
    }*/
    // clear the un-used TDs created by SetupTD()
    m_pHost->CleanupTd(m_SlotId, m_InEp, false);

    if (pVa)
        MemoryTracker::FreeBuffer(pVa);

    if(OK==rc)
    {
        Printf(Tee::PriHigh, "$$$ PASSED $$$");
    }
    else
    {
        Printf(Tee::PriHigh, "*** FAILED ***");
    }
    Printf(Tee::PriHigh, " %s::%s()\n", CLASS_NAME, FUNCNAME);

    LOG_EXT();
    return rc;
}

RC UsbHidDev::PrintInfo(Tee::Priority Pri)
{
    if (!m_IsInit)
    {
        Printf(Tee::PriError, "Device hasn't been initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    Printf(Pri, "USB Hid Device Info: \n");
    UsbDevExt::PrintInfo(Pri);
    Printf(Pri, " In Endpoint %d, Mps %d \n", m_InEp, m_Mps);
    return OK;
}

// HUB class
UsbHubDev::UsbHubDev
(
    XhciController * pHost,
    UINT08 SlotId,
    UINT16 UsbVer,
    UINT08 Speed,
    UINT32 RouteString,
    UINT08 NumOfPorts
) :
    UsbDevExt(pHost, SlotId, UsbVer, Speed, RouteString),
    m_InEp(0),
    m_Mps(0)
{
    m_DevClass  = UsbClassHub;
    m_NumOfPorts= NumOfPorts;
    m_IsInit    = false;
}

UsbHubDev::~UsbHubDev()
{
}

RC UsbHubDev::Init(vector<EndpointInfo> *pvEpInfo)
{
    LOG_ENT();
    RC rc = OK;

    if (pvEpInfo->size() != 1)
    {
        Printf(Tee::PriError, "Invalid number of Endpoint %d, expect 1\n", (UINT32)pvEpInfo->size());
        return RC::HW_STATUS_ERROR;
    }
    PrintEpInfo(pvEpInfo);

    m_InEp = (*pvEpInfo)[0].EpNum;
    m_Mps = (*pvEpInfo)[0].Mps;

    // find out the hub depth
    UINT32 mask = 0xf0000;
    UINT32 hubDepth = 0;
    for (; hubDepth < 5; hubDepth++)
    {
        if (m_RouteString & (mask >> (hubDepth*4)))
            break;
    }
    hubDepth = 5 - hubDepth;

    if ( m_Speed >= XhciUsbSpeedSuperSpeed )   // super speed
    {   // issue the request
        Printf(Tee::PriNormal, "Hub initiated @ Hub Depth %d, RouteString 0x%x \n", hubDepth, m_RouteString);
        CHECK_RC(m_pHost->HubReqSetHubDepth(m_SlotId, hubDepth));
    }
    else
    {
        Printf(Tee::PriNormal, "Hub initiated @ RouteString 0x%x \n", m_RouteString);
    }

    // update device context
    // CHECK_RC(m_pHost->SetDevContextMiscBit(m_FuncAddr, 0, 10));     // todo: find out the correct values instead of 10
    // CHECK_RC(m_pHost->ReqSetInterface(m_FuncAddr, 0, 0 /*interface*/));

    // power up all the downstream ports
    for (UINT08 i = 1; i < m_NumOfPorts + 1; i++)
    {
        CHECK_RC(m_pHost->HubReqSetPortFeature(m_SlotId, UsbPortPower, i));
    }
    Tasker::Sleep(100);

    m_IsInit = true;
    LOG_EXT();
    return rc;
}

RC UsbHubDev::PrintInfo(Tee::Priority Pri)
{
    RC rc;
    if (!m_IsInit)
    {
        Printf(Tee::PriError, "Device hasn't been initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    Printf(Pri, "USB HUB Info: \n");
    Printf(Pri, "Number of Port %d, In Endpoint %d Mps %d \n", m_NumOfPorts, m_InEp, m_Mps);

    UsbDevExt::PrintInfo(Pri);
    // print the port status
    UINT32 portStatus;
    for (UINT08 i = 1; i < m_NumOfPorts + 1; i++)
    {
        CHECK_RC(m_pHost->HubReqGetPortStatus(m_SlotId, i, &portStatus));
        if (portStatus & 0x1)
        {   // Device connected
            Printf(Pri, "  Port %d has device, status: 0x%08x\n", i, portStatus);
        }
    }

    return OK;
}

RC UsbHubDev::HubPortStatus(UINT08 Port, UINT16 *pPortStat, UINT16 *pPortChange)
{
    RC rc;

    UINT32 portStatus;
    rc = m_pHost->HubReqGetPortStatus(m_SlotId, Port, &portStatus);
    if (rc == OK)
    {
        *pPortStat = portStatus;
        portStatus >>= 16;
        *pPortChange = portStatus;
    }
    else
    {
        *pPortStat = 0;
        *pPortChange = 0;
    }

    return rc;
}

RC UsbHubDev::ClearPortFeature(UINT08 Port, UINT16 Feature)
{
    RC rc;
    CHECK_RC(m_pHost->HubReqClearPortFeature(m_SlotId, Feature, Port, 0));
    return rc;
}

RC UsbHubDev::SetPortFeature(UINT08 Port, UINT16 Feature)
{
    RC rc;
    CHECK_RC(m_pHost->HubReqSetPortFeature(m_SlotId, Feature, Port));
    return rc;
}

RC UsbHubDev::Search()
{
    // read the number of port
    RC rc;
    // detect device port by port
    Printf(Tee::PriNormal, "Total %d ports, device on: \n", m_NumOfPorts);
    for (UINT16 i = 1; i <= m_NumOfPorts; i++)
    {
        UINT32 portStatus;
        CHECK_RC(m_pHost->HubReqGetPortStatus(m_SlotId, i, &portStatus));
        if (portStatus & 1) // Bit0: Current Connect Status
        {
            Printf(Tee::PriNormal, "%d ", i);
        }
        Tasker::Sleep(50);
    }
    Printf(Tee::PriNormal, "\n");
    return OK;
}

RC UsbHubDev::ResetHubPort(UINT08 Port, UINT08 * pSpeed)
{
    LOG_ENT();
    RC rc = OK;
    if (!pSpeed)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pSpeed = XhciUsbSpeedUndefined;
    // power up the port
    CHECK_RC(m_pHost->HubReqSetPortFeature(m_SlotId, UsbPortPower, Port));
    Tasker::Sleep(100);
    // Reset the port.
    CHECK_RC(m_pHost->HubReqSetPortFeature(m_SlotId, UsbPortReset, Port));
    Tasker::Sleep(300);

    UINT32 portStatus;
    CHECK_RC(m_pHost->HubReqGetPortStatus(m_SlotId, Port, &portStatus));
    // check if port is Enabled by HUB
    if ( !(portStatus & 0x2) )
    {
        Printf(Tee::PriError, "Port %d not enabled by HUB, status 0x%08x", Port, portStatus);
        return RC::HW_STATUS_ERROR;
    }

    if ( m_UsbVer >= 0x300 )
    {   // USB 3.0, the only speed allowed is Super Speed.
        if ( 0 == (portStatus & 0x1) )
        {   Printf(Tee::PriError, "No device connected on HUB Port %d, 3.0 Port status 0x%08x", Port, portStatus);
            return RC::HW_STATUS_ERROR; }

        * pSpeed = XhciUsbSpeedSuperSpeed;
    }
    else
    {   // USB 2.0, read from HUB
        if ( 0 == (portStatus & 0x1) )
        {   Printf(Tee::PriError, "No device connected on HUB Port %d, Port status 0x%08x ", Port, portStatus);
            return RC::HW_STATUS_ERROR; }

        if ( portStatus & ( 1 << 9 ) )
        {   // low speed
            * pSpeed = XhciUsbSpeedLowSpeed;
        }
        else if ( portStatus & ( 1 << 10 ) )
        {   // high speed
            * pSpeed = XhciUsbSpeedHighSpeed;
        }
        else
        {   // full speed
            * pSpeed = XhciUsbSpeedFullSpeed;
        }
    }

    Printf(Tee::PriLow, "Port Status 0x%2x, Speed of device attached %d", portStatus, * pSpeed);
    LOG_EXT();
    return rc;
}

RC UsbHubDev::ReadHub(UINT32 RunSeconds)
{
    LOG_ENT();
    RC rc = OK;
    RC rcTmp = OK;

    UINT32 addrBit = 32;
    void * pVa = NULL;
    CHECK_RC(MemoryTracker::AllocBuffer(m_Mps, &pVa, true, addrBit, Memory::UC));
    if (!pVa)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    Memory::Fill08(pVa, 0, m_Mps);

    MemoryFragment::FRAGLIST myFragList;
    MemoryFragment::FRAGMENT myFrag = {};
    myFrag.Address  = pVa;
    myFrag.ByteSize = m_Mps;
    myFragList.push_back(myFrag);

    char *report = (char*)pVa;

    if (RunSeconds)
    {
        Printf(Tee::PriNormal, "MaxRunTime: %d seconds\n", RunSeconds);
    }
    Printf(Tee::PriNormal, "Start to listen to event(plug and unplug). ");
    Printf(Tee::PriNormal, "Press any key to abort.\n");

    UINT32 index = 0;
    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);
    UINT64 timeStartMs = Xp::GetWallTimeMS();
    // receive data cirlwlarly
    bool isSkipTrbAdd = false;
    while (true)
    {
        Memory::Fill08(report, 0x0, m_Mps);
        if (pConsole->KeyboardHit())
        {
            pConsole->GetKey();
            Printf(Tee::PriNormal, "User abort\n");
            break;
        }

        if (!isSkipTrbAdd)
        {   // add one TD (data buffer size = m_Mps) to the In Endpoint
            rc = m_pHost->SetupTd(m_SlotId, m_InEp, false, &myFragList);
            if ( OK != rc )
            {
                Printf(Tee::PriError, "Add TD failed");
                break;
            }
            // skip the subsequent TD adding
            isSkipTrbAdd = true;
        }

        rcTmp = m_pHost->WaitXfer(m_SlotId, m_InEp, false, 100, NULL, false, NULL, true);
        if (rcTmp == OK)
        {
            isSkipTrbAdd = false;
            if (!MEM_RD08(report))
            {   // all 0 data come
                continue;
            }

            Printf(Tee::PriNormal, "Index %d report: 0x%02x\n", index++, MEM_RD08(report));

            for (UINT08 i = 1; i < m_NumOfPorts + 1; i++)
            {
                UINT16 portStat = 0;
                UINT16 portChange = 0;

                if ((1 << i) & MEM_RD08(report))
                {
                    rc = HubPortStatus(i, &portStat, &portChange);
                    if (rc != OK)
                        break;
                }
                if (!portChange)
                    continue;

                Printf(Tee::PriNormal, "Port %d event status:0x%04x,   change:0x%04x\n",
                        i, portStat, portChange);

                if ((portChange << 16) & (1 << UsbCPortConnection))
                {
                    if (portStat & (1 << UsbPortConnection))
                        Printf(Tee::PriNormal, "Port %d connected\n", i);

                    else
                        Printf(Tee::PriNormal, "Port %d disconnected\n", i);

                    rc = ClearPortFeature(i, UsbCPortConnection);
                    // todo
                    ClearPortFeature(i, UsbCBhPortReset);
                    ClearPortFeature(i, UsbCPortLinkState);
                    if (rc != OK)
                        break;
                    //Tasker::Sleep(100);
                }
            }
         }

        UINT64  deltTimeSec = (Xp::GetWallTimeMS() - timeStartMs) / 1000;
        if (RunSeconds && deltTimeSec >= RunSeconds)
            break;
        // Tasker::Sleep(10); //delay 10ms
    }
/*
    if (index > 2)
        rc = OK;
*/

    if(OK==rc)
    {
        Printf(Tee::PriHigh, "$$$ PASSED $$$");
    }
    else
    {
        Printf(Tee::PriHigh, "*** FAILED ***");
    }
    Printf(Tee::PriHigh, " %s::%s()\n", CLASS_NAME, FUNCNAME);

    if (pVa)
        MemoryTracker::FreeBuffer(pVa);

    LOG_EXT();
    return rc;

}

RC UsbHubDev::EnableTestMode(UINT08 Port, UINT32 TestMode)
{
    // 11.24.2.13 Set Port Feature -- USB2.0 spec
    //CHECK_RC(HubReqSetPortFeature(slotId, UsbPortTest, hubPort, TestMode));
    LOG_ENT();
    RC rc;
    if ( TestMode < 1 || TestMode > 5 )
    {
        Printf(Tee::PriError,  "Invalid TestMode %d, valid range = [1,5] \n", TestMode);
        return RC::BAD_PARAMETER;
    }

    for (UINT16 i = 1; i <= m_NumOfPorts; i++)
    {
        CHECK_RC(SetPortFeature(i, UsbPortSuspend));
        Tasker::Sleep(10);
    }
    CHECK_RC(m_pHost->HubReqSetPortFeature(m_SlotId, UsbPortTest, Port, TestMode));
    Tasker::Sleep(10);
    // The downstream facing port under test can now be tested.
    // ...
    // After the test is completed, the hub (with the port under test) must be reset by the host or user.
    LOG_EXT();
    return OK;
}

//////uvc function
UsbVideoDev::UsbVideoDev(XhciController * pHost, UINT08 SlotId, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString)
:UsbDevExt(pHost, SlotId, UsbVer, Speed, RouteString)
{
    m_DevClass          = UsbClassVideo;
    m_MaxPayLoadIndex   = 0;
    m_StreamInfNum      = 1;
    m_LastFid           = 0xff;
    m_RcvFrameCount     = 0;
    m_pLwrStreamEp      = NULL;
    m_IsObsolete        = false;

    m_vIsoEp.clear();
    m_vIntfInfo.clear();
    m_vFragList.clear();

    Memory::Fill08(&m_StreamCtrl, 0, sizeof(m_StreamCtrl));
}

UsbVideoDev::~UsbVideoDev()
{
    // m_pHost->ReqSetInterface(m_FuncAddr, 0/*altsetting*/, 1 /*interface*/);
    CleanFragLists();
    m_vIntfInfo.clear();
    m_vIsoEp.clear();
}

// input: buffer contains the Interface Association + Interface descriptor
// output: vector<IntfInfo>
RC UsbVideoDev::ParseDesc(UINT08 *pDescriptor, int Size, vector<IntfInfo> * pvIntfInfo)
{
    LOG_ENT();
    RC rc;
    UINT08 type, length;
    UsbInterfaceDescriptor descIntf;
    int preIntfNum  = -1 ;
    UINT32 offset = 0;
    UsbInterfaceAssocDescriptor descIntfAssc;

    while (1)
    {
        rc = FindNextDescriptor((UINT08*)pDescriptor, Size, UsbDescriptorInfAssoc, &offset);
        if (rc != OK)
        {
            Printf(Tee::PriError, "Can't find Association Interface descriptor");
            return RC::HW_STATUS_ERROR;
        }
        pDescriptor = pDescriptor + offset;

        CHECK_RC(MemToStruct(&descIntfAssc, pDescriptor));
        pDescriptor += descIntfAssc.Length;
        Size -= (offset + descIntfAssc.Length);
        if ( (descIntfAssc.Class == UsbClassVideo)
            && (descIntfAssc.SubClass == UsbSubClassVideoInterfaceCollection) )
        {
            break;   // found Video
        }
    }
// Interface 0 is video control
    // Interface 1 is video stream
    //for (;Size > 0; (pDescriptor += pHeader->Length, Size -= pHeader->Length))
    pvIntfInfo->clear();
    while(Size > 0)
    {
        //pHeader = (UsbDesHeader*)pDescriptor;
        CHECK_RC(GetDescriptorTypeLength((UINT08*)pDescriptor, &type, &length));
        UINT32 index = pvIntfInfo->size();

        if (type == UsbDescriptorInterface)
        {
            CHECK_RC(MemToStruct(&descIntf, pDescriptor));

            if (preIntfNum != descIntf.InterfaceNumber) // if empty or new interface, add it.
            {
                if (pvIntfInfo->size() >= 2) // only search 2 interfaces. VC and VS
                {
                    Printf(Tee::PriLow, "Already Found 2 Interface Descriptors, skip the rest\n");
                    break;
                }

                IntfInfo info;
                info.numSetting = 1;
                info.vAltSetting.clear();

                Intf  intf;  //altsetting 0
                intf.vEp.clear();
                //memcpy(&intf.des, &descIntf, descIntf.Length);
                CHECK_RC(MemToStruct(&intf.des, pDescriptor));
                info.vAltSetting.push_back(intf);

                pvIntfInfo->push_back(info);
            }
            else
            {
                Intf  intf; //altsetting > 0
                intf.vEp.clear();
                //memcpy(&intf.des, &descIntf, descIntf.Length);
                CHECK_RC(MemToStruct(&intf.des, pDescriptor));
                m_vIntfInfo[index - 1].numSetting++;
                m_vIntfInfo[index - 1].vAltSetting.push_back(intf);
            }

            preIntfNum = descIntf.InterfaceNumber;
        }
        else if (type == UsbDescriptorEndpoint)
        {//Add ep
            UsbEndPointDescriptor descEp;
            CHECK_RC(MemToStruct(&descEp, pDescriptor));
            Endpoint ep;
            memset(&ep, 0, sizeof(ep));
            memcpy(&ep.des, &descEp, descEp.Length);
            UINT32 epIndex = m_vIntfInfo[index -1].vAltSetting.size();
            m_vIntfInfo[index -1].vAltSetting[epIndex - 1].vEp.push_back(ep);
        }
        else if ((type == UsbDescriptorUvcSpecificCSInterface) && (index == 1))
        {
            UsbVideoSpecificInterfaceDescriptor vIntf;
            CHECK_RC(MemToStruct(&vIntf, pDescriptor));

            if (vIntf.Subtype == VC_HEADER)
            {
                UsbVideoSpecificInterfaceDescriptorHeader vHIntf;
                CHECK_RC(MemToStruct(&vHIntf, pDescriptor));

                if (vHIntf.CdUvc == 0x0100)
                { // Uvc version 1.0
                    Printf(Tee::PriNormal, "Obsolete version 1.0 \n");
                    m_IsObsolete = true;
                }
            }
        }

        pDescriptor += length;
        Size -= length;
    }
    if (pvIntfInfo->size() < 2) // miss interface?
    {
        Printf(Tee::PriError, "The number of found interfaces is less than 2");
        return RC::HW_STATUS_ERROR;
    }

    PrintIntfInfo(Tee::PriLow);
    LOG_EXT();
    return OK;
}

RC UsbVideoDev::Init(UINT08 *pDescriptor, UINT16 Len)
{
    LOG_ENT();
    RC rc;

    vector<EndpointInfo> vEpInfo;
    UsbConfigDescriptor descCfg;
    CHECK_RC(MemToStruct(&descCfg, pDescriptor));

    if (descCfg.Type != UsbDescriptorConfig )
    {
        Printf(Tee::PriError, "Configuration Descriptor bad format");
        CHECK_RC(RC::HW_STATUS_ERROR);
    }

    if (descCfg.NumInterfaces < 2)
    {
        Printf(Tee::PriError, "The number of interface in config descriptor should be greater than or equal to 2");
        return RC::HW_STATUS_ERROR;

    }

    CHECK_RC(ParseDesc(pDescriptor + descCfg.Length, Len - descCfg.Length, &m_vIntfInfo));
    if (m_vIntfInfo.size() != descCfg.NumInterfaces)
    {
        Printf(Tee::PriWarn, "Miss Interface");
    }

    CHECK_RC(FindStreamIsoEp());

    // Send Set Configuration request to device
    CHECK_RC(m_pHost->ReqSetConfiguration(m_SlotId, 1));
    // Set Altsettting to 0 in interface 1.
    CHECK_RC(m_pHost->ReqSetInterface(m_SlotId, 0, 1 /*interface*/));

    CHECK_RC(ProbeAndCommit());
    CHECK_RC(SetWorkingEp());
    // config ep
    CHECK_RC(ParseEpDescriptors((UINT08*)m_pLwrStreamEp->pEp, 1, m_UsbVer, m_Speed, &vEpInfo));

    Printf(Tee::PriNormal, "Try to config iso endpoint: address %d, maxPacket:%d, altSetting:%d\n",
                m_pLwrStreamEp->inIsoEp,
                m_pLwrStreamEp->isoPayload,
                m_pLwrStreamEp->altSetting);

    rc = m_pHost->CfgEndpoint(m_SlotId, &vEpInfo);
    if (rc != OK)
    {   // todo: go back to step05 to choose another configuration / interface
        Printf(Tee::PriError, "Configuration has been rejected by HC. Only Control Pipe is usable");
        CHECK_RC(RC::HW_STATUS_ERROR);
    }
    Printf(Tee::PriNormal, "Config endpoint successfully\n");

    // Send Set Configuration request to device
    CHECK_RC(m_pHost->ReqSetConfiguration(m_SlotId, 1));

    // Set Altsettting to AltSetting in interface 1.
    CHECK_RC(m_pHost->ReqSetInterface(m_SlotId, m_pLwrStreamEp->altSetting/*altsetting*/, 1 /*interface*/));

    LOG_EXT();
    return OK;
}

RC UsbVideoDev::UvcQueryCtrl(UINT08 Request, UINT08 Interface, UINT08 Unit, UINT08 Cs, void *pData, UINT16 Size)
{
    RC rc;
    UINT08 type = (UsbRequestInterface | UsbRequestClass);
    type |= (Request & 0x80) ? UsbRequestDeviceToHost : UsbRequestHostToDevice;

    CHECK_RC(m_pHost->Setup3Stage(m_SlotId,
                                  type,
                                  Request,
                                  Cs << 8,
                                  (Unit << 8) | Interface,
                                  Size,
                                  pData));

    LOG_EXT();
    return OK;
}

RC UsbVideoDev::UvcGetVideoCtrl(UvcStreamingCtl *ctrl, int probe, UINT08 query)
{
    RC rc;
    UINT08 *pData = NULL;
    UINT16 bufSize = 34;
    if (m_IsObsolete)
        bufSize = 26;

    CHECK_RC( MemoryTracker::AllocBuffer(bufSize, (void**)&pData, true, 32, Memory::UC));
    if ( !pData)
        return RC::CANNOT_ALLOCATE_MEMORY;

    Memory::Fill08(pData, 0, bufSize);
    rc = UvcQueryCtrl(query, m_StreamInfNum, 0,
        probe ? VS_PROBE_CONTROL : VS_COMMIT_CONTROL, pData, bufSize);

    if ( rc != OK )
    {
        if ( query == UVC_GET_MIN || query == UVC_GET_MAX )
        {
            /* Some cameras, mostly based on Bison Electronics chipsets,
             * answer a GET_MIN or GET_MAX request with the wCompQuality
             * field only.
             */
            Printf(Tee::PriWarn, "UVC non compliance - GET_MIN/MAX(PROBE) incorrectly "
                "supported. Enabling workaround.\n");
            //ctrl->CompQuality = *((UINT16*)pData);
        }
        else if(query == UVC_GET_DEF && probe == 1)
        {
            /* Many cameras don't support the GET_DEF request on their
             * video probe control. Warn once and return, the caller will
             * fall back to GET_LWR.
             */
            Printf(Tee::PriWarn, "UVC non compliance - GET_DEF(PROBE) not supported. "
                "Enabling workaround.\n");
        }
        else
        {
            Printf(Tee::PriWarn, "Failed to query (%u) UVC %s control : "
                "%d (exp. %u).\n", query, probe ? "probe" : "commit",
                (UINT32)rc, bufSize);
        }
        CHECK_RC_CLEANUP(RC::BAD_PARAMETER);
    }

    //Print08(pData, bufSize, Tee::PriLow);
    Memory::Print08(pData, bufSize, Tee::PriLow);
    ctrl->BmHint = MEM_RD16(pData);
    ctrl->FormatIndex = MEM_RD08(pData+2);
    ctrl->FrameIndex = MEM_RD08(pData+3);
    ctrl->FrameInterval = MEM_RD32(pData+4);
    ctrl->KeyFrameRate = MEM_RD16(pData+8);
    ctrl->PFrameRate = MEM_RD16(pData+10);
    ctrl->CompQuality = MEM_RD16(pData+12);
    ctrl->CompWindowSize = MEM_RD16(pData+14);
    ctrl->Delay = MEM_RD16(pData+16);

    UINT32 tmp;
    memcpy(&tmp, &pData[18], 4);
    ctrl->MaxVideoFrameSize = tmp;

    memcpy(&tmp, &pData[22], 4); // unaligned operation
    ctrl->MaxPayloadTransferSize = tmp;

    if (!m_IsObsolete)
    {
        memcpy(&tmp, &pData[26], 4); // unaligned operation
        ctrl->ClockFrequency = tmp;

        ctrl->BmFramingInfo = MEM_RD08(pData+30);
        ctrl->PreferedVersion = MEM_RD08(pData+31);
        ctrl->Milwersion = MEM_RD08(pData+32);
        ctrl->MaxVersion = MEM_RD08(pData+33);
    }

Cleanup:
    MemoryTracker::FreeBuffer(pData);
    return rc;
}

RC UsbVideoDev::UvcSetVideoCtrl(UvcStreamingCtl *ctrl, int probe)
{
    RC rc;

    UINT08 *pData = NULL;
    UINT16 bufSize = 34;
    if (m_IsObsolete)
        bufSize = 26;

    CHECK_RC( MemoryTracker::AllocBuffer(bufSize, (void**)&pData, true, 32, Memory::UC));
    if (pData == NULL)
        return RC::CANNOT_ALLOCATE_MEMORY;

    MEM_WR16(pData+0, ctrl->BmHint);
    MEM_WR08(pData+2, ctrl->FormatIndex);
    MEM_WR08(pData+3, ctrl->FrameIndex);
    MEM_WR32(pData+4, ctrl->FrameInterval);
    MEM_WR16(pData+8, ctrl->KeyFrameRate);
    MEM_WR16(pData+10, ctrl->PFrameRate);
    MEM_WR16(pData+12, ctrl->CompQuality);
    MEM_WR16(pData+14, ctrl->CompWindowSize);
    MEM_WR16(pData+16, ctrl->Delay);

    memcpy(&pData[18], &ctrl->MaxVideoFrameSize, 4);
    memcpy(&pData[22], &ctrl->MaxPayloadTransferSize, 4);

    if (!m_IsObsolete)
    {
        memcpy(&pData[26], &ctrl->ClockFrequency, 4);

        MEM_WR08(pData+30, ctrl->BmFramingInfo);
        MEM_WR08(pData+31, ctrl->PreferedVersion);
        MEM_WR08(pData+32, ctrl->Milwersion);
        MEM_WR08(pData+33, ctrl->MaxVersion);
    }

    rc = UvcQueryCtrl(UVC_SET_LWR, m_StreamInfNum, 0,
        probe ? VS_PROBE_CONTROL : VS_COMMIT_CONTROL, pData, bufSize);

    if (rc != OK)
    {
        Printf(Tee::PriError, "Failed to set UVC %s control : "
            "rc %d).\n", probe ? "probe" : "commit",
            (UINT32)rc);
    }

    MemoryTracker::FreeBuffer(pData);
    return rc;
}

RC UsbVideoDev::PrintStreamingCtrl()
{
    constexpr auto Pri = Tee::PriDebug;
    Printf(Pri, "Streaming Control:\n");
    Printf(Pri, " BmHint 0x%x\n", m_StreamCtrl.BmHint);
    Printf(Pri, " FormatIndex %d\n", m_StreamCtrl.FormatIndex);
    Printf(Pri, " FrameIndex %d\n", m_StreamCtrl.FrameIndex);
    Printf(Pri, " FrameInterval %d\n", m_StreamCtrl.FrameInterval);
    Printf(Pri, " KeyFrameRate %d\n", m_StreamCtrl.KeyFrameRate);
    Printf(Pri, " PFrameRate %d\n", m_StreamCtrl.PFrameRate);
    Printf(Pri, " CompQuality %d\n", m_StreamCtrl.CompQuality);
    Printf(Pri, " CompWindowSize %d\n", m_StreamCtrl.CompWindowSize);
    Printf(Pri, " Delay %d\n", m_StreamCtrl.Delay);
    Printf(Pri, " MaxVideoFrameSize %d\n", m_StreamCtrl.MaxVideoFrameSize);
    Printf(Pri, " MaxPayloadTransferSize %d\n", m_StreamCtrl.MaxPayloadTransferSize);

    return OK;
}

RC UsbVideoDev::ProbeAndCommit()
{
    LOG_ENT();
    RC rc;

    memset(&m_StreamCtrl, 0, sizeof(m_StreamCtrl));
    m_StreamCtrl.FormatIndex = 1;

    if (!m_IsObsolete)
    {
        UvcSetVideoCtrl(&m_StreamCtrl, 1);
    }

    UINT32 maxEpPayload = m_vIsoEp[m_MaxPayLoadIndex].isoPayload;
    CHECK_RC( UvcGetVideoCtrl(&m_StreamCtrl, 1, UVC_GET_LWR) );
    PrintStreamingCtrl();

    if (m_StreamCtrl.MaxPayloadTransferSize < maxEpPayload)
    {
        Printf(Tee::PriLow, "set Payload to %d\n", m_StreamCtrl.MaxPayloadTransferSize);
        UvcSetVideoCtrl(&m_StreamCtrl, 0);
    }
    Printf(Tee::PriNormal, "  Payload:%d, MaxVideoFrameSize:%d\n",
        m_StreamCtrl.MaxPayloadTransferSize, m_StreamCtrl.MaxVideoFrameSize);

    LOG_EXT();
    return OK;
}

// output: m_pLwrStreamEp
RC UsbVideoDev::SetWorkingEp()
{
    LOG_ENT();

    if (m_vIntfInfo[1].numSetting > 1) //check if bandWidth is enough for stream
    {
        /* Isochronous endpoint, select the alternate setting. */
        UINT32 bandWidth = m_StreamCtrl.MaxPayloadTransferSize;

        if (bandWidth == 0)
        {
            Printf(Tee::PriWarn, "bandWidth, defaulting to lowest.\n");
            bandWidth = 1;
        }
        UINT32 k = 0;
        for (k = 0; k < m_vIsoEp.size(); k++)
        {
            UINT32 payLoad = m_vIsoEp[k].isoPayload;

            if (payLoad >= bandWidth)
                break;
        }
        if (k >= m_vIsoEp.size())
        {
            Printf(Tee::PriWarn, "Can't find valid ep to meet the bandWidth:0x%x. Using altSetting 1", bandWidth);
            k = 0;
        }

        m_pLwrStreamEp = &m_vIsoEp[k];
    }
    else
    {   /*Bulk endpoint*/
        Printf(Tee::PriError, "Don't support the video stream interface with only one altsetting");
        return RC::HW_STATUS_ERROR;
    }
    LOG_EXT();
    return OK;
}

// build up m_vIsoEp, m_MaxPayLoadIndex and m_StreamInfNum
RC UsbVideoDev::FindStreamIsoEp()
{
    LOG_ENT();
    // Interface 1, altSetting 0 should contain iso endpoint

    IntfInfo & info = m_vIntfInfo[1];

    m_StreamInfNum = info.vAltSetting[0].des.InterfaceNumber;
    if (m_StreamInfNum != 1)
    {
        Printf(Tee::PriWarn, "Video stream interface number is %d", m_StreamInfNum);
    }

    // Find in Iso ep from altSetting 1.
    // AltSetting 0 doesn't contain ep or only contain one Bulk ep
    UINT32 maxIsoEpPayLoad = 0;
    m_MaxPayLoadIndex = 0;
    for (UINT32 i = 1; i < info.vAltSetting.size(); i++)
    {
        Intf & intf = info.vAltSetting[i];
        for (UINT32 j = 0; j < intf.vEp.size(); j++)
        {
            UsbEndPointDescriptor &ep = intf.vEp[j].des;
            if ( ( (ep.Attibutes & 0x3) == 1 )
                 && (ep.Address & 0x80) )// Iso In
            {
                IsoEp isoEp;
                isoEp.altSetting = intf.des.AlterNumber;
                isoEp.inIsoEp = (ep.Address & 0xf);
                isoEp.isoPayload = (ep.MaxPacketSize & 0x07ff) * (1 + ((ep.MaxPacketSize >> 11) & 3));

                if (isoEp.isoPayload > maxIsoEpPayLoad)
                {
                    maxIsoEpPayLoad = isoEp.isoPayload;
                    m_MaxPayLoadIndex = m_vIsoEp.size();
                }

                isoEp.pEp = &ep;
                m_vIsoEp.push_back(isoEp);
            }
       }
    }
    if (m_vIsoEp.empty())
    {
        Printf(Tee::PriError, "Can't find Iso In endpoint");
        return RC::HW_STATUS_ERROR;
    }

    // print the IN iso endpoint info
    for (UINT32 k = 0; k < m_vIsoEp.size(); k++)
    {
        Printf(Tee::PriNormal, "  Iso Endpont %d: maxPacket:%d, altSetting:%d\n",
                m_vIsoEp[k].inIsoEp, m_vIsoEp[k].isoPayload, m_vIsoEp[k].altSetting);
    }
    LOG_EXT();
    return OK;
}

RC UsbVideoDev::PrintIntfInfo(Tee::Priority Pri)
{
    UINT32 totalIntf = m_vIntfInfo.size();
    Printf(Pri, "Interface & endpoint info:\n");
    char const *IntfSubClassString[] = {"", "VideoControl", "VideoStreaming"};
    char const *epString[ ] = {"Control", "Iso", "Bulk", "Interrupt"};

    for (UINT32 i = 0; i < totalIntf; i++)
    {
        IntfInfo & info = m_vIntfInfo[i];
        Printf(Pri, "  Interface: %d (total %d): \n", i, totalIntf);

        UINT32 totalAltIntf = info.vAltSetting.size();
        for (UINT32 j = 0; j < totalAltIntf; j++)
        {
            Intf  & intf = info.vAltSetting[j];
            Printf(Pri, "    Interface %d Altnative Setting %d (total %d): ",
                        intf.des.InterfaceNumber, intf.des.AlterNumber, totalAltIntf);
            Printf(Pri, "NumOfEp: 0x%x SubClass: %s \n",
                        intf.des.NumEP, IntfSubClassString[intf.des.SubClass]);

            UINT32 totalEp = intf.vEp.size();
            for (UINT32 k = 0; k < totalEp; k++)
            {
                UsbEndPointDescriptor &ep = intf.vEp[k].des;
                Printf(Pri, "      Ep %d / %d Address:0x%x, Direction: %s, Attr: %s Interval: %d \n",
                       k, totalEp,
                       ep.Address & 0xf,
                       (ep.Address & 0x80) ? "IN" : "OUT",
                       epString[ep.Attibutes & 0x3],
                       ep.Interval);
            }
        }
        // Printf(Pri, "\n");
    }
    return OK;
}

RC UsbVideoDev::AllocateIsoBuffer(MemoryFragment::FRAGLIST *pFragList, UINT32 NumFrag, UINT32 Size)
{
    RC rc;
    if (!pFragList)
    {
        Printf(Tee::PriError, "FreeIsoBuffer. FragList pointer is null");
        return RC::BAD_PARAMETER;
    }
    Utility::CleanupOnReturnArg<UsbVideoDev, RC, MemoryFragment::FRAGLIST*>
        freeFragList(this, &UsbVideoDev::FreeIsoBuffer, pFragList);
    void * pVa = NULL;
    UINT32 addrBit = 32;

    for (UINT32 i = 0; i < NumFrag; i++)
    {
        CHECK_RC(MemoryTracker::AllocBuffer(Size, &pVa, true, addrBit, Memory::UC));
        if (!pVa)
            return RC::CANNOT_ALLOCATE_MEMORY;

        memset(pVa, 0x0, Size);

        MemoryFragment::FRAGMENT myFrag;
        myFrag.Address = pVa;
        myFrag.PhysAddr = 0;
        myFrag.ByteSize = Size;
        pFragList->push_back(myFrag);
    }
    freeFragList.Cancel();
    return OK;
}

RC UsbVideoDev::FreeIsoBuffer(MemoryFragment::FRAGLIST *pFragList)
{
    if (!pFragList)
    {
        Printf(Tee::PriWarn, "FreeIsoBuffer. FragList pointer is null");
        return OK;
    }

    for (UINT32 i  = 0; i < pFragList->size(); i++)
    {
        MemoryFragment::FRAGMENT & frag = (*pFragList)[i];
        if (frag.Address)
            MemoryTracker::FreeBuffer(frag.Address);

        frag.Address = 0;
    }
    pFragList->clear();
    return OK;
}

RC UsbVideoDev::CleanFragLists()
{
    for (UINT32 i = 0; i < m_vFragList.size(); i++)
    {
        MemoryFragment::FRAGLIST *pFragList = &m_vFragList[i];
        if (pFragList)
            FreeIsoBuffer(pFragList);
    }
    m_vFragList.clear();
    return OK;
}

RC UsbVideoDev::UvcVideDecodeStart(UINT08 *pData, bool IsDebug)
{
    /* Sanity checks:
     * - packet must be at least 2 bytes long
     * - bHeaderLength value must be at least 2 bytes (see above)
     * - bHeaderLength value can't be larger than the packet size.
     */
    if (MEM_RD08(pData) < 2)
    {
        if (IsDebug)
            Printf(Tee::PriWarn, "Dropping payload (error bit set)\n");
        return OK;
    }
    /* Skip payloads marked with the error bit ("error frames"). */
    if (MEM_RD08(pData+1) & UVC_STREAM_ERR)
    {
        if (IsDebug)
            Printf(Tee::PriWarn, "Dropping payload (error bit Set).\n");

        return OK;
    }
    UINT08 fid = (MEM_RD08(pData+1) & UVC_STREAM_FID);

    // Count the frame with FID toggle
    if (fid != m_LastFid)
    {
        if (m_LastFid < 2)
            m_RcvFrameCount++;

        m_LastFid = fid;
    }
    return OK;
}

/*
RC UsbVideoDev::SelfAnalysis()
{
    RC rc;
    UINT32 eventId = 0;
    Printf(Tee::PriNormal, "Enter auto analysis process:\n");
    m_pHost->SetDebugMode(true);
    CHECK_RC(m_pHost->FindEventRing(0, &eventId));
    CHECK_RC(m_pHost->PrintRing(eventId, Tee::FileOnly));
    UINT32 xferId = 0;
    CHECK_RC(m_pHost->FindXferRing(m_FuncAddr, m_pLwrStreamEp->inIsoEp, false, &xferId));
    CHECK_RC(m_pHost->PrintRing(xferId, Tee::FileOnly));
    UINT08 slotId = 0;
    CHECK_RC(m_pHost->GetSlotId(m_FuncAddr, &slotId));
    CHECK_RC(m_pHost->PrintData(1, slotId));
    m_pHost->SetDebugMode(false);
    return OK;
}
*/
RC UsbVideoDev::ReadWebCam(UINT32 RunSeconds, UINT32 RunTds, UINT32 TdUsage)
{
    LOG_ENT();
    RC rc = OK;
    UINT32 isoFrameCount = 120;
    if (TdUsage)
        isoFrameCount = TdUsage;

    m_LastFid = 0xFF;
    m_RcvFrameCount = 0;
    bool isQuitProcess = false;

    UINT32 isoNumFrag;
    UINT32 isoFragBufSize;
    UINT32 payLoad = m_pLwrStreamEp->isoPayload;
    UINT32 nPackets = (m_StreamCtrl.MaxVideoFrameSize +  payLoad - 1) / payLoad; // round up
    Printf(Tee::PriNormal, " Video frame size %d, Ep Payload %d\n", m_StreamCtrl.MaxVideoFrameSize, payLoad);

    // one Fraglist should allocate buffers for a video frame
    if (0)
    {
        if (nPackets < 1)
            nPackets = 1;
        if (nPackets > 32)
            nPackets = 32;
        isoNumFrag = nPackets;
        isoFragBufSize = payLoad; // One TD with multiple fragments
    }
    else
    {
        isoNumFrag = 1;
        isoFragBufSize = payLoad; // One TD with one fragment
    }

    Printf(Tee::PriNormal, "Device SlotId: %d, TrbsPerTD %d, Trb Buff Size %d\n",
            m_SlotId, isoNumFrag, isoFragBufSize);

    if (RunSeconds)
        Printf(Tee::PriNormal, "MaxRunTime: %d seconds, ", RunSeconds);
    else
        Printf(Tee::PriNormal, "MaxRunTime: no limit, ");

    if (RunTds)
        Printf(Tee::PriNormal, "MaxRunTds: %d, ", RunTds);
    else
        Printf(Tee::PriNormal, "MaxRunTds: no limit, ");

    Printf(Tee::PriNormal, "MaxAddTds: %d\n", isoFrameCount);

    if (!m_vFragList.empty())
        CleanFragLists();

    for (UINT32 i = 0; i < isoFrameCount; i++)
    {
        MemoryFragment::FRAGLIST fragList;
        fragList.clear();
        m_vFragList.push_back(fragList);
        CHECK_RC(AllocateIsoBuffer(&m_vFragList[i], isoNumFrag, isoFragBufSize));
    }
    Printf(Tee::PriNormal, "Start to receive frame:\n");
    Printf(Tee::PriNormal, "Press any key to abort.\n");

    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);

    // parameter
    bool isSkipTrbAdding = false;
    bool isIssue = false;

    // cirlwlar buffer variable
    UINT32 head = 0;
    UINT32 tail = 0;
    UINT32 i = 0;
    UINT32 finishedTrbCount = 0;

    // returned value;
    UINT32 completionCode = 0;

    // the pointer of frag buffer for compare
    UINT08 *pAddr = NULL;

    bool    isWorkround = false; // handle babble error and service error
    bool    isDebug = m_pHost->GetDebugMode();
    UINT08  epAddr = m_pLwrStreamEp->inIsoEp;
    UINT32  waitTime = 0;

    UINT64  babbleCount = 0;
    UINT64  MissServiceCount = 0;
    UINT64  timeStartMs = Xp::GetWallTimeMS();
    while (true)
    {
        if (!isQuitProcess && pConsole->KeyboardHit())
        {
            pConsole->GetKey();
            Printf(Tee::PriNormal, "\nUser abort\n");
            Printf(Tee::PriNormal, "enter cleaning process\n");
            isQuitProcess = true;
        }
        MemoryFragment::FRAGLIST *pFragList = NULL;

        // When entering quit-process and overrun-process, stop sending commands.
        if (head - tail == isoFrameCount || isQuitProcess) // Already used up all the fraglist. Only wait for trb to finish
        {
            isSkipTrbAdding = true;
            if (isDebug)
                Printf(Tee::PriHigh, "Wait for trb to be finished\n");

        }
        else
        {   // Add new Fraglist
            isSkipTrbAdding = false;
            Printf(Tee::PriLow, "  Add head: 0x%x\n", i);

            i = head++;
            // pFragList= &m_vFragList[i & (isoFrameCount - 1)];
            pFragList= &m_vFragList[i % isoFrameCount];
           // vector <UINT08> pattern;
           // pattern.push_back(0x0);
           // MemoryFragment::FillPattern(pFragList, &pattern);
            UINT08 *pAddr = (UINT08*)(*pFragList)[0].Address;
            if (isoFragBufSize < 32)
                Printf(Tee::PriError, "BufSize SOFTWARE error");
            Memory::Fill08(pAddr, 0, 32);

            if (!isIssue && (head - tail >= isoFrameCount))
            {
                isIssue = true; // First time, issue Xfer after all the fraglists are added
                Printf(Tee::PriLow, "    Issue Xfer Enabled\n");
            }
            if (RunTds && i >= RunTds - 1)
            {
                if (!isIssue)
                {
                    isIssue = true;
                    Printf(Tee::PriLow, "    Issue Xfer Enabled\n");
                }

                Printf(Tee::PriHigh, "enter cleaning process. MaxTdPermit (%d) have been sent.\n", RunTds);
                isQuitProcess = true;
            }
            waitTime = 0;
            //rc = m_pHost->SetupTD(m_FuncAddr, m_OutEp, true, &myFragList);
        }

        completionCode = 0;
        //rc = m_pHost->SetupXfer(m_FuncAddr, epAddr, isDataOut, pFragList, isIsoTransfer,
//                     isIssue, timeOutMs, isSkipTrbAdding, &completionCode);
        if (!isSkipTrbAdding)
        {
            rc = m_pHost->SetupTd(m_SlotId, epAddr, false, pFragList, true, isIssue);
            if (rc == OK)
                continue;       // <!--- skip the rest proecess

            Printf(Tee::PriError, "Can't add trb correctly rc(%d)", (UINT32)rc);
            rc = RC::HW_STATUS_ERROR;
            break;
        }
        else
        {
            rc = m_pHost->WaitXfer(m_SlotId, epAddr, false, 1, &completionCode, false);
            Printf(Tee::PriLow, "head:0x%x tail 0x%x rc:%d completionCode %d \n", head, tail, (UINT32)rc, completionCode);
        }
        // rc should be timeout or OK.
        // timeout means  can't find valid xfer event trb
        // OK is to find valid xfer event trb.
        // Other return rc should be wrong.
        if (rc == RC::TIMEOUT_ERROR)
        {
            waitTime++;
            if (waitTime > 10)
            {
                Printf(Tee::PriError, "Timeout when waiting for any event trb to arrive for %dms.Abort", waitTime);
                break;
             }
            // continue to wait for trb to finish
            continue;
        }else if (rc != OK) // rc should return OK except timeout_error
        {
            rc = RC::HW_STATUS_ERROR;
            break;
        }
        waitTime = 0;

        isWorkround = false;
        bool isQuit = false;
        // RC is now OK, already found valid xfer event trb
        // start to analyse completion code
        switch (completionCode)
        {
            case XhciTrbCmpCodeSuccess: //success
                break;

            case XhciTrbCmpCodeShortPacket: // regard this to be success
                break;

            case XhciTrbCmpCodeIlwalid:
                Printf(Tee::PriHigh, "Received invalid completion code\n");
                rc = RC::HW_STATUS_ERROR;
                break;

            case XhciTrbCmpCodeBabbleDetectErr:
               // Babble is permitted
                if (isDebug)
                    Printf(Tee::PriHigh, "BabbleDetectErr\n");
                babbleCount++;
                isWorkround = true;
                break;

            case XhciTrbCmpCodeMissedServiceErr:
                if (isDebug)
                    Printf(Tee::PriHigh, "MissedServiceErr\n");

                MissServiceCount++;
                isWorkround = true; //mark special handling next time
                break;

            case XhciTrbCmpCodeRingOverrun:
                if (isQuitProcess && head == tail)
                    isQuit = true;
                else
                    Printf(Tee::PriError, "Already overrun");

                rc = RC::HW_STATUS_ERROR;
                break;

            default:
                Printf(Tee::PriHigh, "Received unexpected completion code: %d.\n", completionCode);
                rc = RC::HW_STATUS_ERROR;
                break;
        }

        if (rc != OK)
        {
            if (isQuit)
                rc = OK;
            break;
        }

        UINT32 finishedIndex = tail;
        UINT32 fragIndex = finishedTrbCount;
        finishedTrbCount++;
        if (finishedTrbCount % isoNumFrag == 0) // Each isoNumFrag trbs are grouped to one fraglist
        {
            // Need each trb in one td to set IOC
            // Current design is that each fraglist has the same entries
            tail++;
            if (isDebug)
                Printf(Tee::PriHigh, "finished tail:0x%x\n", finishedIndex);

            // reset it to zero
            finishedTrbCount = 0;

           //once printing data, overrun would happen most likely
           // MemoryFragment::Print08(pFragList);
        }
        if (!isWorkround)
        {
            pFragList= &m_vFragList[finishedIndex % isoFrameCount];
            pAddr = (UINT08*)(*pFragList)[fragIndex].Address;
            // Only simply decode the payload header
            UvcVideDecodeStart(pAddr);
            if (m_RcvFrameCount % 100 == 0)
            {
                UINT64  deltTimeSec = (Xp::GetWallTimeMS() - timeStartMs) / 1000;
                UINT32  hours = deltTimeSec / (60 * 60);
                UINT32  minutes = (deltTimeSec % (60 * 60)) / 60;
                UINT32  secs =  (deltTimeSec % 60);
                if (RunSeconds && deltTimeSec >= RunSeconds)
                    isQuitProcess = true;

                Printf(Tee::ScreenOnly, "Received Frame: %d elapsed time(h/m/s) %02d:%02d:%02d\r",
                                 m_RcvFrameCount, hours, minutes, secs);
            }
        }
    }

    UINT64  deltTimeSec = (Xp::GetWallTimeMS() - timeStartMs) / 1000;
    UINT32  hours = deltTimeSec / (60 * 60);
    UINT32  minutes = (deltTimeSec % (60 * 60)) / 60;
    UINT32  secs =  (deltTimeSec % 60);

    Printf(Tee::PriNormal, "Received Frame: %d. babbleCount %lld missServiceCount %lld. \n",
                    m_RcvFrameCount, babbleCount, MissServiceCount);
    Printf(Tee::PriNormal, "Elapsed time(h/m/s) %02d:%02d:%02d\n", hours, minutes, secs);
    UINT32 unfinishedTd = head - tail;
    if (rc != OK || unfinishedTd)
    {
        rc =RC::HW_STATUS_ERROR;

        Printf(Tee::PriHigh, "head:0x%x tail 0x%x  unfinished TDs %d, rc:%d completionCode %d \n",
                        head, tail, unfinishedTd,
                        (UINT32)rc, completionCode);
        // SelfAnalysis();
    }
    if(OK==rc)
    {
        Printf(Tee::PriHigh, "$$$ PASSED $$$");
    }
    else
    {
        Printf(Tee::PriHigh, "*** FAILED ***");
    }
    Printf(Tee::PriHigh, " %s::%s()\n", CLASS_NAME, FUNCNAME);
    LOG_EXT();
    return rc;
}

RC UsbVideoDev::SaveFrame(const string fileName)
{
    LOG_ENT();
    RC rc = OK;

    UINT32 payLoad = m_pLwrStreamEp->isoPayload;
    UINT32 maxSize = m_StreamCtrl.MaxVideoFrameSize;
    UINT32 nPackets = (maxSize +  payLoad - 1) / payLoad;
    Printf(Tee::PriNormal, "Payload %d, number of packets %d\n", payLoad, nPackets);

    // allocating the uffers
    if (!m_vFragList.empty())
    {
        CleanFragLists();
    }

    for (UINT32 i = 0; i < nPackets; i++)
    {
        MemoryFragment::FRAGLIST fragList;
        fragList.clear();
        m_vFragList.push_back(fragList);
        CHECK_RC(AllocateIsoBuffer(&m_vFragList[i], 1, payLoad));
    }

    UINT08 epAddr = m_pLwrStreamEp->inIsoEp;

    //enlarging the XferRing if needed
    CHECK_RC(m_pHost->ReserveTd(m_SlotId, m_pLwrStreamEp->inIsoEp, false, nPackets));
    /*
    // enlarging  Event Ring
    emptySlots = 0;

    trbPerPage = 256; // pXferRing->GetTrbPerPage() is protected, maybe make it  public?
    while (emptySlots <= 3*nPackets)
    {
        Printf(Tee::PriNormal, "Append %d trb segment to Event Ring\n", trbPerPage);
        CHECK_RC(m_pHost->RingAppend(EventRingID, trbPerPage));
        emptySlots += trbPerPage;
    }
    */

    // Setup TDs
    for(UINT32 i = 0; i < m_vFragList.size(); i++)
    {
        rc = m_pHost->SetupTd(m_SlotId, epAddr, false /*In*/, &m_vFragList[i], true /*Isoch*/, (i == m_vFragList.size() - 1) /*ring doorbell*/);
        if (rc != OK)
        {
            Printf(Tee::PriError, "Can't add TD. RC(%d)", (UINT32)rc);
            return RC::HW_STATUS_ERROR;
        }
    }

/* JPEG DHT Segment for YCrCb omitted from MJPG data */
const UINT08 DHT[420] =
{
0xFF, 0xC4, 0x01, 0xA2,
0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
0x08, 0x09, 0x0A, 0x0B, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00,
0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61,
0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24,
0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34,
0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9,
0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9,
0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
0xF8, 0xF9, 0xFA, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01,
0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 0x15, 0x62,
0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28, 0x29, 0x2A,
0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8,
0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
0xF9, 0xFA
};

    FileHolder saveFile;
    rc = saveFile.Open(fileName, "wb");
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "Could not open file '%s'\n", fileName.c_str());
        return rc;
    }

    UINT08 offset = 0;
    UINT32 lengthSaved = 0;
    bool endOfFrame = false;
    bool endOfSaving = false;
    UINT08 lastByte = 0x00;

    UINT32 completionCode = 0;
    UINT32 transferLength = 0;
    UINT08 *pData = NULL;
    bool isShortPacket = false;

    for(UINT32 i = 0; i < m_vFragList.size(); i++)
    {
        Printf(Tee::PriLow, "waiting for packet# %d \n", i);
        isShortPacket = false;

        rc = m_pHost->WaitXfer(m_SlotId, epAddr, false, 1, &completionCode, false /*wait for any TRB*/, &transferLength);
        Printf(Tee::PriLow, "RC %d CompletionCode %d TransferLength %d \n", (UINT32)rc, completionCode, transferLength);
        if (rc == RC::TIMEOUT_ERROR)
        {
            Printf(Tee::PriError, "Timeout when waiting for packet # %d to arrive\n", i);
            CHECK_RC_CLEANUP(rc);
        }
        if (completionCode != XhciTrbCmpCodeSuccess)
        {
            if (completionCode == XhciTrbCmpCodeShortPacket)
            {
                Printf(Tee::PriLow, "Short packet\n");
                isShortPacket = true;
            }
            else
            {
                Printf(Tee::PriError, "Transfer didn't succeed\n");
                CHECK_RC_CLEANUP(RC::HW_STATUS_ERROR);
            }
        }
        if (rc == OK)
        {
            pData = (UINT08*)m_vFragList[i][0].Address;

            Printf(Tee::PriDebug, "Transfered data for packet# = %d: \n", i);
            Memory::Print08(m_vFragList[i][0].Address, payLoad, Tee::PriDebug);

            if (MEM_RD08(pData+1) & UVC_STREAM_EOF)
            {
                endOfFrame = true;
            }
            offset = MEM_RD08(pData+0);
            pData += offset;
            if (isShortPacket)
            {
                offset += transferLength;
            }
            if (!endOfSaving)
            {
                Printf(Tee::PriDebug, "Saving packet to the file \n");
                for(UINT32 j = 0; j < payLoad - offset; j++)
                {
                    if (lastByte != 0xff)
                    {
                        if (MEM_RD08(pData+j) != 0xff)
                        {
                            CHECK_RC_CLEANUP(Utility::FWrite(pData + j, 1, 1,
                                                             saveFile.GetFile()));
                        }
                    }
                    else
                    {
                        if (MEM_RD08(pData+j) != 0xda)
                        {
                            CHECK_RC_CLEANUP(Utility::FWrite(&lastByte, 1, 1,
                                                             saveFile.GetFile()));
                            if (MEM_RD08(pData+j) != 0xff)
                            {
                                CHECK_RC_CLEANUP(Utility::FWrite(pData + j, 1,
                                                                 1, saveFile.GetFile()));
                            }
                        }
                        else
                        {
                            CHECK_RC_CLEANUP(Utility::FWrite(DHT, 420, 1,
                                                             saveFile.GetFile()));
                            CHECK_RC_CLEANUP(Utility::FWrite(&lastByte, 1, 1,
                                                             saveFile.GetFile()));
                            CHECK_RC_CLEANUP(Utility::FWrite(pData + j, 1, 1,
                                                             saveFile.GetFile()));
                        }
                    }
                    lastByte = MEM_RD08(pData+j);
                }
                lengthSaved += payLoad - offset;
            }
            if (endOfFrame && (!endOfSaving))
            {
                endOfSaving = true;
                Printf(Tee::PriDebug, "Closing the file \n");
                saveFile.Close();
                Printf(Tee::PriNormal, "File saved\n");
            }
        }
        else
        {
            Printf(Tee::PriError, "RC not OK\n");
            CHECK_RC_CLEANUP(rc);
        }
    }

Cleanup:
    if (!m_vFragList.empty())
    {
        CleanFragLists();
    }

    LOG_EXT();
    return rc;
}

RC UsbVideoDev::PrintInfo(Tee::Priority Pri)
{
    if (!m_IsInit)
    {
        Printf(Tee::PriError, "Device hasn't been initialized");
        return RC::WAS_NOT_INITIALIZED;
    }

    Printf(Pri, "UVC Device Info: \n");
    UsbDevExt::PrintInfo(Pri);
    return OK;
}

// USB configurable device
UsbConfigurable::UsbConfigurable(XhciController * pHost, UINT08 SlotId, UINT16 UsbVer, UINT08 Speed, UINT32 RouteString)
:UsbDevExt(pHost, SlotId, UsbVer, Speed, RouteString)
{
    m_DevClass = UsbConfigurableDev;
    m_vEpInfo.clear();
}

UsbConfigurable::~UsbConfigurable()
{
}

RC UsbConfigurable::Init(vector<EndpointInfo> *pvEpInfo)
{
    LOG_ENT();
    RC rc = OK;
    if (pvEpInfo->size() < 1)
    {
        Printf(Tee::PriError, "Ilwalide number of endpoint %d", (UINT32)pvEpInfo->size());
        return RC::HW_STATUS_ERROR;
    }
    PrintEpInfo(pvEpInfo);

    m_vEpInfo.clear();
    // Get the In EP & Out EP one by one
    for (UINT32 i = 0; i< pvEpInfo->size(); i++ )
    {
        m_vEpInfo.push_back((*pvEpInfo)[i]);
    }

    m_IsInit = true;
    LOG_EXT();
    return rc;
}

RC UsbConfigurable::PrintInfo(Tee::Priority Pri)
{
    if (!m_IsInit)
    {
        Printf(Tee::PriError, "Device hasn't been initialized");
        return RC::WAS_NOT_INITIALIZED;
    }

    Printf(Pri, "USB Configurable Device Info: \n");
    UsbDevExt::PrintInfo(Pri);

    Printf(Pri, "Number of Endpoint %d : \n", (UINT32)m_vEpInfo.size());
    for (UINT32 i = 0; i< m_vEpInfo.size(); i++ )
    {
        Printf(Pri, "  Endpoint %d, Type %d, Data %s\n",
               m_vEpInfo[i].EpNum, m_vEpInfo[i].Type, m_vEpInfo[i].IsDataOut?"Out":"In");
    }
    return OK;
}

RC UsbConfigurable::DataIn(UINT08 Endpoint,
                           MemoryFragment::FRAGLIST * pFragList,
                           bool IsIsoch)
{
    RC rc;
    CHECK_RC(m_pHost->SetupTd(m_SlotId, Endpoint, false, pFragList, IsIsoch));
    CHECK_RC(m_pHost->WaitXfer(m_SlotId, Endpoint, false));
    return OK;
}

RC UsbConfigurable::DataOut(UINT08 Endpoint,
                            MemoryFragment::FRAGLIST * pFragList,
                            bool IsIsoch)
{
    RC rc;
    CHECK_RC(m_pHost->SetupTd(m_SlotId, Endpoint, true, pFragList, IsIsoch));
    CHECK_RC(m_pHost->WaitXfer(m_SlotId, Endpoint, true));
    return OK;
}

RC UsbConfigurable::RegRd(UINT32 Offset, UINT64 * pData)
{
    LOG_ENT(); Printf(Tee::PriDebug,"(Reg Offset = %d)\n", Offset);
    RC rc;
// Setup Data Format:
// setup_data[7:0] = 8'b1_10_00000
// setup_data[39:8] = Register Address
    if (!pData)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    UINT64 * pDataIn;
    CHECK_RC( MemoryTracker::AllocBuffer(8, (void**)&pDataIn, true, 32, Memory::UC) );
    *pDataIn = 0;

    rc = m_pHost->Setup3Stage(m_SlotId,
                              0xc0,     //8'b1_10_00000
                              Offset & 0xff,
                              (Offset >> 8 )& 0xffff,
                              (Offset >> 24 )& 0xff,
                              8,
                              pDataIn);

    if ( OK == rc )
    {
        *pData = *pDataIn;
    }
    else
    {
        *pData = 0xffffffff;
    }
    MemoryTracker::FreeBuffer(pDataIn);

    LOG_EXT();
    return rc;
}
RC UsbConfigurable::RegWr(UINT32 Offset, UINT64 Data)
{
    LOG_ENT(); Printf(Tee::PriDebug,"(Reg Offset = %d, Data = 0x%08llx)\n", Offset, Data);
    RC rc;
// Setup Data Format:
// setup_data[7:0] = 8'b0_10_00000
// setup_data[39:8] = Register Address
    UINT64 * pDataOut;
    CHECK_RC( MemoryTracker::AllocBuffer(8, (void**)&pDataOut, true, 32, Memory::UC) );
    *pDataOut = Data;

    rc = m_pHost->Setup3Stage(m_SlotId,
                              0x40,     // 8'b0_10_00000
                              Offset & 0xff,
                              (Offset >> 8 )& 0xffff,
                              (Offset >> 24 )& 0xff,
                              8,
                              pDataOut);
    MemoryTracker::FreeBuffer(pDataOut);

    LOG_EXT();
    return rc;
}

//------------------------------------------------------------------------------
UINT08 UsbDevExt::MemAdv8(UINT08 ** ppBuf)
{
    UINT08 tmp = MEM_RD08(*ppBuf); *ppBuf+=1; return tmp;
};

UINT16 UsbDevExt::MemAdv16(UINT08 ** ppBuf)
{
    UINT08 tmp1 = MemAdv8(ppBuf);
    UINT08 tmp2 = MemAdv8(ppBuf);
    UINT16 tmp = (tmp2 << 8) | tmp1;
    return tmp;
//    UINT16 tmp = MEM_RD08(*ppBuf); *ppBuf+=2;
};

UINT32 UsbDevExt::MemAdv32(UINT08 ** ppBuf)
{
    UINT08 tmp1 = MemAdv8(ppBuf);
    UINT08 tmp2 = MemAdv8(ppBuf);
    UINT08 tmp3 = MemAdv8(ppBuf);
    UINT08 tmp4 = MemAdv8(ppBuf);
    UINT32 tmp = (tmp4 << 24) | (tmp3 << 16) | (tmp2 << 8) | tmp1;
    return tmp;
//    UINT32 tmp = MEM_RD32(*ppBuf); *ppBuf+=4; return tmp;
};

RC UsbDevExt::MemToStruct(UsbDeviceDescriptor* pStruct, UINT08 * pBuf)
{
    LOG_ENT();
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), UsbDescriptorDevice));
    pLocal += 2;

    pStruct->Usb        = MemAdv16(&pLocal);
    pStruct->Class      = MemAdv8(&pLocal);
    pStruct->SubClass   = MemAdv8(&pLocal);
    pStruct->Protocol   = MemAdv8(&pLocal);
    pStruct->MaxPacketSize0 = MemAdv8(&pLocal);
    pStruct->VendorID   = MemAdv16(&pLocal);
    pStruct->ProductID  = MemAdv16(&pLocal);
    pStruct->Device     = MemAdv16(&pLocal);
    pStruct->Mamufacture    = MemAdv8(&pLocal);
    pStruct->Product    = MemAdv8(&pLocal);
    pStruct->SerialNumber   = MemAdv8(&pLocal);
    pStruct->NumConfig  = MemAdv8(&pLocal);
    if ((pLocal - pBuf) != pStruct->Length)
    {
        Printf(Tee::PriWarn, "Lenth %d Bytes, expected %d", (UINT32)(pLocal - pBuf), pStruct->Length);
    }
    LOG_EXT();
    return OK;
}

RC UsbDevExt::MemToStruct(UsbConfigDescriptor* pStruct, UINT08 * pBuf, bool IsSkipCheck)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), IsSkipCheck?UsbDescriptorNA:UsbDescriptorConfig));
    pLocal += 2;

    pStruct->TotalLength    = MemAdv16(&pLocal);
    pStruct->NumInterfaces  = MemAdv8(&pLocal);
    pStruct->ConfigValue    = MemAdv8(&pLocal);
    pStruct->Config     = MemAdv8(&pLocal);
    pStruct->Attributes = MemAdv8(&pLocal);
    pStruct->MaxPower   = MemAdv8(&pLocal);
    if ((pLocal - pBuf) != pStruct->Length)
    {
        Printf(Tee::PriWarn, "Lenth %d Bytes, expected %d", (UINT32)(pLocal - pBuf), pStruct->Length);
    }
    return OK;
}

RC UsbDevExt::MemToStruct(UsbInterfaceAssocDescriptor* pStruct, UINT08 * pBuf)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), UsbDescriptorInfAssoc));
    pLocal += 2;

    pStruct->FirstInterface = MemAdv8(&pLocal);
    pStruct->InterfaceCount = MemAdv8(&pLocal);
    pStruct->Class     = MemAdv8(&pLocal);
    pStruct->SubClass  = MemAdv8(&pLocal);
    pStruct->Protocol  = MemAdv8(&pLocal);
    pStruct->Function  = MemAdv8(&pLocal);

    if ((pLocal - pBuf) != pStruct->Length)
    {
        Printf(Tee::PriWarn, "Lenth %d Bytes, expected %d", (UINT32)(pLocal - pBuf), pStruct->Length);
    }
    return OK;
}

RC UsbDevExt::MemToStruct(UsbInterfaceDescriptor* pStruct, UINT08 * pBuf)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), UsbDescriptorInterface));
    pLocal += 2;

    pStruct->InterfaceNumber= MemAdv8(&pLocal);
    pStruct->AlterNumber    = MemAdv8(&pLocal);
    pStruct->NumEP     = MemAdv8(&pLocal);
    pStruct->Class     = MemAdv8(&pLocal);
    pStruct->SubClass  = MemAdv8(&pLocal);
    pStruct->Protocol  = MemAdv8(&pLocal);
    pStruct->Interface = MemAdv8(&pLocal);
    if ((pLocal - pBuf) != pStruct->Length)
    {
        Printf(Tee::PriWarn, "Lenth %d Bytes, expected %d", (UINT32)(pLocal - pBuf), pStruct->Length);
    }
    return OK;
}

RC UsbDevExt::MemToStruct(UsbEndPointDescriptor* pStruct, UINT08 * pBuf)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pLocal, &(pStruct->Type), &(pStruct->Length), UsbDescriptorEndpoint));
    pLocal += 2;

    pStruct->Address    = MemAdv8(&pLocal);
    pStruct->Attibutes  = MemAdv8(&pLocal);
    pStruct->MaxPacketSize  = MemAdv16(&pLocal);
    pStruct->Interval   = MemAdv8(&pLocal);
    if ((pLocal - pBuf) != pStruct->Length)
    {
        Printf(Tee::PriWarn, "Lenth %d Bytes, expected %d", (UINT32)(pLocal - pBuf), pStruct->Length);
    }
    return OK;
}

RC UsbDevExt::MemToStruct(UsbHubDescriptor* pStruct, UINT08 * pBuf)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), UsbDescriptorHubDescriptor));
    pLocal += 2;

    pStruct->NPorts     = MemAdv8(&pLocal);
    pStruct->HubCharac  = MemAdv16(&pLocal);
    pStruct->PwrOn2PwrGood  = MemAdv8(&pLocal);
    pStruct->CtrlLwrrent    = MemAdv8(&pLocal);
    pStruct->Removable  = MemAdv8(&pLocal); // its length is depending on number of port
    /*
    if ((pLocal - pBuf) != pStruct->Length)
    {
        Printf(Tee::PriWarn, "Lenth %d Bytes, expected %d", (UINT32)(pLocal - pBuf), pStruct->Length);
    }
    */
    return OK;
}

RC UsbDevExt::MemToStruct(UsbHubDescriptorSs* pStruct, UINT08 * pBuf)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), UsbDescriptorHubDescriptorSs));
    pLocal += 2;

    pStruct->NPorts     = MemAdv8(&pLocal);
    pStruct->HubCharac  = MemAdv16(&pLocal);
    pStruct->PwrOn2PwrGood  = MemAdv8(&pLocal);
    pStruct->CtrlLwrrent    = MemAdv8(&pLocal);
    pStruct->HeaderDecLat   = MemAdv8(&pLocal);
    pStruct->HubDelay   = MemAdv16(&pLocal);
    pStruct->DevRemove  = MemAdv16(&pLocal);

    if ((pLocal - pBuf) != pStruct->Length)
    {
        Printf(Tee::PriWarn, "Lenth %d Bytes, expected %d", (UINT32)(pLocal - pBuf), pStruct->Length);
    }
    return OK;
}

RC UsbDevExt::MemToStruct(UsbSsEpCompanion* pStruct, UINT08 * pBuf)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), UsbDescriptorSsEpCompanion));
    pLocal += 2;

    pStruct->MaxBurst   = MemAdv8(&pLocal);
    pStruct->Attibutes  = MemAdv8(&pLocal);
    pStruct->BytePerInterval  = MemAdv16(&pLocal);
    if ((pLocal - pBuf) != pStruct->Length)
    {
        Printf(Tee::PriWarn, "Lenth %d Bytes, expected %d", (UINT32)(pLocal - pBuf), pStruct->Length);
    }
    return OK;
}

RC UsbDevExt::MemToStruct(UsbHidDescriptor* pStruct, UINT08 * pBuf)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), UsbDescriptorHid));
    pLocal += 2;

    pStruct->Hid        = MemAdv16(&pLocal);
    pStruct->CountryCode    = MemAdv8(&pLocal);
    pStruct->NumDescriptors = MemAdv8(&pLocal);
    pStruct->DescriptorType = MemAdv8(&pLocal);
    pStruct->ItemLength = MemAdv16(&pLocal);
    if ((pLocal - pBuf) != pStruct->Length)
    {
        Printf(Tee::PriWarn, "Lenth %d Bytes, expected %d", (UINT32)(pLocal - pBuf), pStruct->Length);
    }
    return OK;
}

RC UsbDevExt::MemToStruct(UsbVideoSpecificInterfaceDescriptor* pStruct, UINT08 * pBuf)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), UsbDescriptorUvcSpecificCSInterface));
    pLocal += 2;

    pStruct->Subtype        = MemAdv8(&pLocal);
    return OK;
}

RC UsbDevExt::MemToStruct(UsbVideoSpecificInterfaceDescriptorHeader* pStruct, UINT08 * pBuf)
{
    RC rc;
    if (!pStruct || !pBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT08 * pLocal = pBuf;
    CHECK_RC(GetDescriptorTypeLength(pBuf, &(pStruct->Type), &(pStruct->Length), UsbDescriptorUvcSpecificCSInterface));
    pLocal += 2;

    pStruct->Subtype        = MemAdv8(&pLocal);
    pStruct->CdUvc          = MemAdv16(&pLocal);
    pStruct->TotalLength    = MemAdv16(&pLocal);
    pStruct->ClockFrequency = MemAdv32(&pLocal);
    pStruct->InCollection   = MemAdv8(&pLocal);
    return OK;
}

