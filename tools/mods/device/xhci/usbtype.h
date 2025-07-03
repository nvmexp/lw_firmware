/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------

#ifndef INCLUDED_USBTYPE_H
#define INCLUDED_USBTYPE_H
#pragma pack(1)

typedef struct _UsbDeviceDescriptor
{
    UINT08 Length;            // Size of this descriptor in bytes
    UINT08 Type;              // DEVICE Descriptor Type = 0x01
    UINT16 Usb;               // USB specification release number in binary-coded decimal
    UINT08 Class;             // Class code asigned by Usb
    UINT08 SubClass;          // SuClass code assigned by Usb
    UINT08 Protocol;          // Protocol code assigned by Usb
    UINT08 MaxPacketSize0;    // Max packet size for EP 0
    UINT16 VendorID;          // Vendor ID assigned by Usb
    UINT16 ProductID;         // Product ID assigned by Usb
    UINT16 Device;            // Device release number in Dinary-coded-decimal
    UINT08 Mamufacture;       // Index of string descriptor describing manufacturer
    UINT08 Product;           // Index of string descriptor describing product
    UINT08 SerialNumber;      // Index of string descriptor describing device's serial number
    UINT08 NumConfig;         // Number of possible configurations
} UsbDeviceDescriptor, *pUsbDeviceDescriptor;

typedef struct _UsbConfigDescriptor
{
    UINT08 Length;            // Size of this descriptor in bytes
    UINT08 Type;              // Config Descriptor Type = 0x02
    UINT16 TotalLength;       // Total length of data returned for this config
    UINT08 NumInterfaces;     // Number of interfaces supported by this config
    UINT08 ConfigValue;       // Values to use as an argument ot Set Config to select this Config
    UINT08 Config;            // Index of string descriptor describing this config
    UINT08 Attributes;        // Config characteristics
    UINT08 MaxPower;          // Max power consmption Expressed in 2ma unit (i.e 50=100ma)
} UsbConfigDescriptor, *pUsbConfigDescriptor;

/* USB_DT_INTERFACE_ASSOCIATION: groups interfaces */
typedef struct _UsbInterfaceAssocDescriptor
{
    UINT08  Length;         // 0x8 Size of this descriptor, in bytes.
    UINT08  Type;           // 0xb INTERFACE ASSOCIATION Descriptor.

    UINT08  FirstInterface; //  0x0 Interface number of the VideoControl interface that is associated with this function
    UINT08  InterfaceCount; //  0x2 Number of contiguous Video interfaces that are associated with this function.
    UINT08  Class;          //  0xe CC_VIDEO
    UINT08  SubClass;       //  0x3 SC_VIDEO_INTERFACE_COLLECTION
    UINT08  Protocol;       //  0x0 Not used. Must be set to PC_PROTOCOL_UNDEFINED.
    UINT08  Function;       //  0x4 Index of string descriptor. Must match the iInterface field of the Standard VC Interface Descriptor.
}UsbInterfaceAssocDescriptor;

typedef struct _UsbInterfaceDescriptor
{
    UINT08 Length;            // Size of this descriptor in bytes
    UINT08 Type;              // Interface Descriptor Type = 0x04
    UINT08 InterfaceNumber;   // Number of interface
    UINT08 AlterNumber;       // Value used to select alternate setting
    UINT08 NumEP;             // Number of endpoint used by this interface (excluding EP0)
    UINT08 Class;             // Class code assigned by Usb
    UINT08 SubClass;          // SubClass code assigned by Usb
    UINT08 Protocol;          // Protocol code assigned by Usb
    UINT08 Interface;         // Index of string descriptor
} UsbInterfaceDescriptor, *pUsbInterfaceDescriptor;

typedef struct _UsbEndPointDescriptor
{
    UINT08 Length;            // Size of this descriptor in bytes
    UINT08 Type;              // Endpoint Descriptor Type = 0x05
    UINT08 Address;           // Addr of EP encoding: 0-3 Number, 4-6 Reserved, 7, Direction: 0=Out, 1=In
    UINT08 Attibutes;         // TransferType: 00 Control, 01 Iso, 10 Bulk, 11 Interrupt
    UINT16 MaxPacketSize;     // Max packet size for this EP
    UINT08 Interval;          // Interval for polling EP for data trasfer. In Ms
} UsbEndPointDescriptor, *pUsbEndPointDescriptor;

typedef struct _UsbHubDescriptor
{
    UINT08 Length;
    UINT08 Type;
    UINT08 NPorts;
    UINT16 HubCharac;
    UINT08 PwrOn2PwrGood;
    UINT08 CtrlLwrrent;
    UINT08 Removable;
} UsbHubDescriptor, *pUsbHubDescriptor;

typedef struct _UsbHubDescriptorSs
{
    UINT08 Length;
    UINT08 Type;
    UINT08 NPorts;
    UINT16 HubCharac;
    UINT08 PwrOn2PwrGood;
    UINT08 CtrlLwrrent;
    UINT08 HeaderDecLat;
    UINT16 HubDelay;
    UINT16 DevRemove;
} UsbHubDescriptorSs, *pUsbHubDescriptorSs;

typedef struct _UsbSsEpCompanion
{
    UINT08 Length;            // Size of this descriptor in bytes
    UINT08 Type;              // UsbDescriptorSsEpCompanion
    UINT08 MaxBurst;          // The max number of packets can be transfered as a burst
    UINT08 Attibutes;         // 1:0-Mult for ISO; 4:0-MaxStreams for Bulk
    UINT16 BytePerInterval;   // Total number of bytes will be transfered every service interval
} UsbSsEpCompanion, *pUsbSsEpCompanion;

typedef enum _UsbDeviceSpeed
{
    UsbSpeedFull   = 0
    ,UsbSpeedLow   = 1
    ,UsbSpeedHigh  = 2
}UsbDeviceSpeed;

// Standard request

typedef enum _UsbRequestRecipient
{
    UsbRequestDevice     = 0x00 // Device
    ,UsbRequestInterface = 0x01 // Interface
    ,UsbRequestEndpoint  = 0x02 // Endpoint
}UsbRequestRecipient;

typedef enum _UsbRequestDirection
{
    UsbRequestHostToDevice = 0x00  // Data Flow from Host to Device
    ,UsbRequestDeviceToHost = 0x80 // Data Flow from Device to host
}UsbRequestDirection;

typedef enum _UsbRequestType
{
    UsbRequestStandard  = (0x0 << 5),
    UsbRequestClass     = (0x1 << 5),
    UsbRequestVendor    = (0x2 << 5),
    UsbRequestRsvd      = (0x3 << 5),
}UsbRequestType;

typedef enum _UsbRequest
{
    UsbGetStatus         = 0x0
    ,UsbClearFeature     = 0x1
    ,UsbSetFeature       = 0x3
    ,UsbSetAddress       = 0x5
    ,UsbGetDescriptor    = 0x6
    ,UsbSetDescriptor    = 0x7 // not used
    ,UsbGetConfiguration = 0x8
    ,UsbSetConfiguration = 0x9
    ,UsbGetInterface     = 0xa
    ,UsbSetInterface     = 0xb
    ,UsbSynchFrame       = 0xc // not used
    // Below comes from USB3.0
    ,UsbSetSel           = 48
    ,UsbSetIsochDelay    = 49
    // class-specific request for Mass Storage Devices
    // see also Universal Serial Bus Mass Storage Class Bulk-Only Transport Rev 1.0
    ,UsbGetMaxLun        = 0xfe
    // Below are vendor specific commands for ISO
    ,UsbIsochReset       = 252
    ,UsbIsochQuery       = 253
    ,UsbIsochStop        = 254
    ,UsbIsochStart       = 255
}UsbRequest;

typedef enum _UsbHubRequest
{
    UsbHubGetStatus         = 0x0
    ,UsbHubClearFeature     = 0x1
    ,UsbHubSetFeature       = 0x3
    ,UsbHubGetDescriptor    = 0x6
    ,UsbHubSetDescriptor    = 0x7 // not used
    ,UsbHubClearTTBuffer    = 0x8
    ,UsbHubResetTT          = 0x9
    ,UsbHubGetTTState       = 0xa
    ,UsbHubStopTT           = 0xb
    // Below comes from USB3.0
    ,UsbHubSetHubDepth      = 0xc
    ,UsbHubGetPortErrCount  = 0xd
}UsbHubRequest;

typedef enum _UsbHubFeatureSelector
{
    UsbCHubLocalPower           = 0
    ,UsbCHubOverLwrrent         = 1
    ,UsbPortConnection          = 0
    ,UsbPortEnable              = 1
    ,UsbPortSuspend             = 2
    ,UsbPortOverLwrrent         = 3
    ,UsbPortReset               = 4
    ,UsbPortLinkState           = 5
    ,UsbPortPower               = 8
    ,UsbPortLowSpeed            = 9
    ,UsbCPortConnection         = 16
    ,UsbCPortEnable             = 17
    ,UsbCPortSuspend            = 18
    ,UsbCPortOverLwrrent        = 19
    ,UsbCPortReset              = 20
    ,UsbPortTest                = 21
    ,UsbPortIndicator           = 22
    // below from USB 3.0
    ,UsbPortU1Timeout           = 23
    ,UsbPortU2Timeout           = 24
    ,UsbCPortLinkState          = 25
    ,UsbCPortConfigError        = 26
    ,UsbPortRemoteWakeMask      = 27
    ,UsbBhPortReset             = 28
    ,UsbCBhPortReset            = 29
    ,UsbForceLinkPmAccept       = 30
}UsbHubFeatureSelector;

typedef enum _UsbFeatureSelector
{
    // see also Table 9-6 of USB3.0 spec
    UsbEndpointHalt             = 0     // to Endpoint
    ,UsbFunctionSuspend         = 0     // to Interface
    ,UsbTestMode                = 2     // to Device
    ,UsbU1Enable                = 48    // to Device
    ,UsbU2Enable                = 49    // to Device
    ,UsbLmtENable               = 50    // to Device
}UsbFeatureSelector;

typedef enum _UsbDescriptorType
{
    UsbDescriptorNA             = 0
    ,UsbDescriptorDevice        = 1
    ,UsbDescriptorConfig        = 2
    ,UsbDescriptorString        = 3
    ,UsbDescriptorInterface     = 4
    ,UsbDescriptorEndpoint      = 5
    // For Hid
    ,UsbDescriptorHid           = 0x21
    // Below comes from USB3.0
    ,UsbDescriptorIntfacePower  = 8
    ,UsbDescriptorOTG           = 9
    ,UsbDescriptorDebug         = 10
    ,UsbDescriptorInfAssoc      = 11  // Interface association descriptor
    ,UsbDescriptorBOS           = 15
    ,UsbDescriptorDevCapability = 16
    ,UsbDescriptorUvcSpecificCSInterface = 0x24
    ,UsbDescriptorHubDescriptor = 0x29
    ,UsbDescriptorHubDescriptorSs = 0x2a
    ,UsbDescriptorSsEpCompanion = 48
}UsbDescriptorType;

typedef struct _UsbDeviceRequest {
    UINT08 bmRequestType;
    UINT08 bRequest;
    UINT16 wValue;
    UINT16 wIndex;
    UINT16 wLength;
}UsbDeviceRequest, *pUsbDeviceRequest;

typedef enum _UsbDescriptorLength
{
    LengthDeviceDescr       = 0x12
    ,LengthCfgDescr         = 0x9
    ,LengthInterfacegDescr  = 0x9
    ,LengthEndPointDescr    = 0x7
    ,LengthSsEpCompanion    = 0x6
}UsbDescriptorLength;

typedef enum _UsbDeviceClass
{
    UsbClassMassStorage     = 0x8
    ,UsbClassHub            = 0x9
    ,UsbClassHid            = 0x3
    ,UsbClassVideo          = 0xe
    ,UsbConfigurableDev     = 0xff
}UsbDeviceClass;

/* All standard descriptors have these 2 fields at the beginning */
typedef struct _UsbDescriptorHeader {
    UINT08 Length;
    UINT08 Type;
} UsbDesHeader;

// For hid class

// Class-specific hid descriptor

//HID sub-descriptor info
typedef struct _HID_SUB_DESCRIPTOR_STRUCT
{
    UINT08  DescriptorType;         // Subdescriptor type
    UINT16  ItemLength;             // Size of subdescriptor
}UsbHidSubDescriptor, *pUsbHidSubDescriptor;

typedef struct _UsbHidDescriptor
{
     UINT08 Length;                 // Size of this descriptor in bytes.                0x9
     UINT08 Type;                   // HID descriptor type (assigned by USB).           0x21
     UINT16 Hid;                    // HID Class Specification release number.          0x101
     UINT08 CountryCode;            // Hardware target country                          0x0
     UINT08 NumDescriptors;         // Number of HID class descriptors to follow        0x1
     UINT08 DescriptorType;         // Report descriptor type.                          0x22
     UINT16 ItemLength;             // Total length of Report descriptor.               0x32
}UsbHidDescriptor,*pUsbHidDescriptor;

typedef enum _UsbHidSubClass
{
    UsbHidSubClassBoot      = 0x1   // Boot Interface subclass
    ,UsbHidSubClassNo       = 0x0   // No sub Class
}UsbHidSubClass;

typedef enum _UsbHidProtocol
{
    UsbHidProNo             = 0x0
    ,UsbHidProKeyboard      = 0x1
    ,UsbHidProMouse         = 0x2
    // 3 ~ 255 reserved.
}UsbHidProtocol;

// For video class
typedef enum _UsbVideoSubClass
{
    UsbSubClassVideoUndefined               = 0x0
    ,UsbSubClassVideoControl                = 0x1
    ,UsbSubClassVideoStreaming              = 0x2
    ,UsbSubClassVideoInterfaceCollection    = 0x3
}UsbVideoSubClass;

typedef enum _videoClassRequest
{
    RC_UNDEFINED    = 0x00,
    UVC_SET_LWR         = 0x01,
    UVC_GET_LWR         = 0x81,
    UVC_GET_MIN         = 0x82,
    UVC_GET_MAX         = 0x83,
    UVC_GET_RES         = 0x84,
    UVC_GET_LEN         = 0x85,
    UVC_GET_INFO        = 0x86,
    UVC_GET_DEF         = 0x87,
}UsbVideoClassRequest;

typedef enum _UsbVideoStreamInterfaceControlSelector
{
    VS_PROBE_CONTROL    = 0x01,
    VS_COMMIT_CONTROL   = 0x02,

}UsbVideoStreamInfCtlSelector;

typedef enum _UsbVideoVCInterfaceDescriptorSubtype
{
    VC_DESCRIPTOR_UNDEFINED = 0x00,
    VC_HEADER               = 0x01,
    VC_INPUT_TERMINAL       = 0x02,
    VC_OUTPUT_TERMINAL      = 0x03,
    VC_SELECTOR_UNIT        = 0x04,
    VC_PROCESSING_UNIT      = 0x05,
    VC_EXTENSION_UNIT       = 0x06,
}UsbVideoVCInterfaceDescriptorSubtype;

typedef struct _UsbVideoSpecificInterfaceDescriptor {
    UINT08 Length;
    UINT08 Type;
    UINT08 Subtype;
}UsbVideoSpecificInterfaceDescriptor;

typedef struct _UsbVideoSpecificInterfaceDescriptorHeader {
    UINT08 Length;
    UINT08 Type;
    UINT08 Subtype;
    UINT16 CdUvc;
    UINT16 TotalLength;
    UINT32 ClockFrequency;
    UINT08 InCollection;
} UsbVideoSpecificInterfaceDescriptorHeader;

#pragma pack()
#endif
