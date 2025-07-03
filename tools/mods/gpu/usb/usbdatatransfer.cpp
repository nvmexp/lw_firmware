/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <boost/algorithm/string.hpp>

#include "core/include/cpu.h"
#include "core/include/xp.h"
#include "gpu/include/gpudev.h"
#include "gpu/usb/usbdatatransfer.h"

//TODO - Remove unnecessary code
namespace
{
    //Loading LibUsb dynamically
    bool IsLibUsbInitialized = false;

    INT32 (*libusb_init)(libusb_context **context);
    void (*libusb_exit)(libusb_context *);
    INT32 (*libusb_open)(libusb_device *, libusb_device_handle **);
    void (*libusb_close)(libusb_device_handle *);

    ssize_t (*libusb_get_device_list)(libusb_context *, libusb_device ***);
    void (*libusb_free_device_list)(libusb_device **, INT32);

    UINT08 (*libusb_get_device_descriptor)(libusb_device *, libusb_device_descriptor *);
    UINT08 (*libusb_get_config_descriptor)(libusb_device *, UINT08,
                                           libusb_config_descriptor **);
    UINT08 (*libusb_get_string_descriptor_ascii)(libusb_device_handle *, UINT08,
                                                 unsigned char *, INT32);

    UINT08 (*libusb_get_bus_number)(libusb_device *);
    UINT08 (*libusb_get_device_address)(libusb_device *);
    UINT08 (*libusb_get_port_number)(libusb_device *);
    libusb_device* (*libusb_get_parent)(libusb_device *);

    INT32 (*libusb_get_configuration)(libusb_device_handle *, INT32 *);
    INT32 (*libusb_set_configuration)(libusb_device_handle *, INT32);

    INT32 (*libusb_claim_interface)(libusb_device_handle *, INT32);
    INT32 (*libusb_release_interface)(libusb_device_handle *, INT32);

    INT32 (*libusb_kernel_driver_active)(libusb_device_handle *, INT32);
    INT32 (*libusb_attach_kernel_driver)(libusb_device_handle *, INT32);
    INT32 (*libusb_detach_kernel_driver)(libusb_device_handle *, INT32);

    INT32 (*libusb_control_transfer)(libusb_device_handle *, UINT08, UINT08, UINT16,
                                     UINT16, unsigned char*, UINT16, UINT32);
    INT32 (*libusb_bulk_transfer)(libusb_device_handle *, unsigned char,
                                  unsigned char *, INT32, INT32 *, UINT32);
    INT32 (*libusb_clear_halt)(libusb_device_handle *, unsigned char);

    const char* (*libusb_strerror)(libusb_error);

    template <typename RetType>
    inline RC GetLibusbFunction
    (
        void *libUsbHandle,
        RetType **ppFunction,
        const char* functionName
    )
    {
        *ppFunction = reinterpret_cast<RetType*>(Xp::GetDLLProc(libUsbHandle, functionName));
        if (!*ppFunction)
        {
            Printf(Tee::PriError, "Cannot load libusb function (%s)\n",
                   functionName);
            return RC::DLL_LOAD_FAILED;
        }
        return OK;
    }

    inline UINT32 colwertBigEndianToInt32(UINT08 pBuf[4])
    {
        return ((pBuf[0] << 24)
                | (pBuf[1] << 16)
                | (pBuf[2] << 8)
                | (pBuf[3]));
    }

    //Universal Serial Bus Mass Storage Class Bulk-Only Transport section 3.2
    const INT32  USB_ACTIVE_CONFIG      = 0x1;
    const INT32  USB_DECRE_DEV_REF_CNT  = 0x1;
    const UINT32 USB_TIMEOUT_MS         = 5000;
    const UINT08 USB_CONFIG_INDEX       = 0x0;
    const UINT08 USB_INTERFACE_NUM      = 0x0;
    const UINT08 USB_MAX_RETRIES        = 10;
    const UINT16 USB_RESERVED_BLOCKS    = 100;

    const UINT08 USB_LEN_SEND_COMMAND   = 31;
    const UINT08 USB_LEN_GET_SENSE      = 18;
    const UINT08 USB_LEN_READ_CAPACITY  = 8;
    const INT32  USB_LEN_READ_STATUS    = 13;

    const UINT08 USB_SENSE_DATA_MASK    = 0x7F;
    const UINT08 USB_SENSE_DATA_PRESENT = 0x70;
    const UINT08 USB_SENSE_KEY_MASK     = 0x0F;
    const UINT08 USB_SENSE_DATA_VALID   = 0x1;

    const UINT08 USB_DEFAULT_ENDPOINT_IN  = 0x81;
    const UINT08 USB_DEFAULT_ENDPOINT_OUT = 0x2;

    const UINT08 USB_REQ_GET_MAX_LUN    = 0xFE;
    const UINT08 USB_RT_GET_MAX_LUN     = LIBUSB_ENDPOINT_IN
                                            | LIBUSB_REQUEST_TYPE_CLASS
                                            | LIBUSB_RECIPIENT_INTERFACE;

    //Universal Serial Bus Mass Storage Class UFI Command Specification
    const string usbSenseData[16] =
    {
        "No Sense Data Available",
        "Successful but some recovery action performed by the device",
        "Device not ready",
        "Flaw in the medium or error in the recorded data",
        "Non-recoverable hardware failure",
        "Illegal parameter in the command packet",
        "Removable medium may have changed or device has been reset",
        "Cannot write data to the desired block number (write protected)",
        "Encountered blank medium while reading or a non-blank medium on write-once device",
        "Vendor Specific Conditions",
        "Reserved",
        "Command aborted by the device",
        "Reserved",
        "Buffered peripheral device has reached the end-of-partition",
        "Source data did not match the data read from the medium",
        "Reserved"
    };

    //tuple of <senseKey, additionalSenseCode, additionalSenseCodeQualifier>
    //which can be used as a key to retrieve detailed explanation of an error.
    //senseKey, additionalSenseCode and additionalSenseCodeQualifier are
    //obtained by sending request for additional sense data, USB_REQ_GET_SENSE
    typedef tuple<UINT08, UINT08, UINT08> tupleSenseData;
    const map <tupleSenseData, string> mapUsbSenseAdditionalData =
    {
        { make_tuple(0x00, 0x00, 0x00), "NO SENSE" },
        { make_tuple(0x01, 0x17, 0x01), "RECOVERED DATA WITH RETRIES" },
        { make_tuple(0x01, 0x18, 0x00), "RECOVERED DATA WITH ECC" },
        { make_tuple(0x02, 0x04, 0x01), "LOGICAL DRIVE NOT READY - BECOMING READY" },
        { make_tuple(0x02, 0x04, 0x02), "LOGICAL DRIVE NOT READY - INITIALIZATION REQUIRED" },
        { make_tuple(0x02, 0x04, 0x04), "LOGICAL UNIT NOT READY - FORMAT IN PROGRESS" },
        { make_tuple(0x02, 0x04, 0xFF), "LOGICAL DRIVE NOT READY - DEVICE IS BUSY" },
        { make_tuple(0x02, 0x06, 0x00), "NO REFERENCE POSITION FOUND" },
        { make_tuple(0x02, 0x08, 0x00), "LOGICAL UNIT COMMUNICATION FAILURE" },
        { make_tuple(0x02, 0x08, 0x01), "LOGICAL UNIT COMMUNICATION TIME-OUT" },
        { make_tuple(0x02, 0x08, 0x80), "LOGICAL UNIT COMMUNICATION OVERRUN" },
        { make_tuple(0x02, 0x3A, 0x00), "MEDIUM NOT PRESENT" },
        { make_tuple(0x02, 0x54, 0x00), "USB TO HOST SYSTEM INTERFACE FAILURE" },
        { make_tuple(0x02, 0x80, 0x00), "INSUFFICIENT RESOURCES" },
        { make_tuple(0x02, 0xFF, 0xFF), "UNKNOWN ERROR" },
        { make_tuple(0x03, 0x02, 0x00), "NO SEEK COMPLETE" },
        { make_tuple(0x03, 0x03, 0x00), "WRITE FAULT" },
        { make_tuple(0x03, 0x10, 0x00), "ID CRC ERROR" },
        { make_tuple(0x03, 0x11, 0x00), "UNRECOVERED READ ERROR" },
        { make_tuple(0x03, 0x12, 0x00), "ADDRESS MARK NOT FOUND FOR ID FIELD" },
        { make_tuple(0x03, 0x13, 0x00), "ADDRESS MARK NOT FOUND FOR DATA FIELD" },
        { make_tuple(0x03, 0x14, 0x00), "RECORDED ENTITY NOT FOUND" },
        { make_tuple(0x03, 0x30, 0x01), "CANNOT READ MEDIUM - UNKNOWN FORMAT" },
        { make_tuple(0x03, 0x31, 0x01), "FORMAT COMMAND FAILED" },
        { make_tuple(0x05, 0x1A, 0x00), "PARAMETER LIST LENGTH ERROR" },
        { make_tuple(0x05, 0x20, 0x00), "INVALID COMMAND OPERATION CODE" },
        { make_tuple(0x05, 0x21, 0x00), "LOGICAL BLOCK ADDRESS OUT OF RANGE" },
        { make_tuple(0x05, 0x24, 0x00), "INVALID FIELD IN COMMAND PACKET" },
        { make_tuple(0x05, 0x25, 0x00), "LOGICAL UNIT NOT SUPPORTED" },
        { make_tuple(0x05, 0x26, 0x00), "INVALID FIELD IN PARAMETER LIST" },
        { make_tuple(0x05, 0x26, 0x01), "PARAMETER NOT SUPPORTED" },
        { make_tuple(0x05, 0x26, 0x02), "PARAMETER VALUE INVALID" },
        { make_tuple(0x05, 0x39, 0x00), "SAVING PARAMETERS NOT SUPPORT" },
        { make_tuple(0x06, 0x28, 0x00), "NOT READY TO READY TRANSITION - MEDIA CHANGED" },
        { make_tuple(0x06, 0x29, 0x00), "POWER ON RESET OR BUS DEVICE RESET OCLWRRED" },
        { make_tuple(0x06, 0x2F, 0x00), "COMMANDS CLEARED BY ANOTHER INITIATOR" },
        { make_tuple(0x07, 0x27, 0x00), "WRITE PROTECTED MEDIA" },
        { make_tuple(0x0B, 0x4E, 0x00), "OVERLAPPED COMMAND ATTEMPTED" }
    };

    enum KernelDriverState
    {
        KERNEL_DRIVER_INACTIVE  = 0
       ,KERNEL_DRIVER_ACTIVE    = 1
    };

    enum IlwokeGetMassStorageSense : bool
    {
        USB_DO_NOT_FETCH_SENSE_DATA_ON_FAILURE
       ,USB_FETCH_SENSE_DATA_ON_FAILURE
    };

    // USB Mass Storage Class Spec Section 5.1: Command Block Wrapper (CBW)
    struct CommandBlockWrapper
    {
        UINT08     dCBWSignature[4];
        UINT32     dCBWTag;
        UINT32     dCBWDataTransferLength;
        UINT08     bmCBWFlags;
        UINT08     bCBWLUN;
        UINT08     bCBWCBLength;
        UINT08     CBWCB[16];
    };

    // USB Mass Storage Class Spec Section 5.2: Command Status Wrapper (CSW)
    struct CommandStatusWrapper
    {
        UINT08     dCSWSignature[4];
        UINT32     dCSWTag;
        UINT32     dCSWDataResidue;
        UINT08     bCSWStatus;
    };

    struct RequestSenseStandardData
    {
        UINT08 errorCode                    = 0;
        UINT08 reserved1                    = 0;
        UINT08 senseKey                     = 0;
        UINT08 information[4]               = { 0 };
        UINT08 additionalSenseLength        = 0;
        UINT08 reserved2[4]                 = { 0 };
        UINT08 additionalSenseCode          = 0;
        UINT08 additionalSenseCodeQualifier = 0;
        UINT08 reserved3[4]                 = { 0 };
    };

    struct ReadCapacityDataWrapper
    {
        UINT08 maxLogicalBlockAddress[4] = { 0 };
        UINT08 blockLengthInBytes[4]     = { 0 };
    };
};

UsbDataTransfer::UsbDataTransfer()
    :m_pSubDevice(nullptr)
    ,m_pLibContext(nullptr)
    ,m_ContextInit(false)
    ,m_DevicesOpen(false)
    ,m_ExpectedUsb3Count(0)
    ,m_ExpectedUsb2Count(0)
    ,m_AllowUsbDeviceOnIntelHost(false)
    ,m_WaitBeforeClaimingUsbDevMs(0)
    ,m_VoltaPciDBDFStored(false)
{
}

UsbDataTransfer::~UsbDataTransfer()
{
    if (IsLibUsbInitialized)
    {
        if (m_ContextInit)
        {
            CloseUsbDevices();
            libusb_exit(m_pLibContext);
            m_ContextInit = false;
        }
    }
}

RC UsbDataTransfer::Initialize(GpuSubdevice *pSubDevice)
{
    RC rc;
    MASSERT(pSubDevice);

    CHECK_RC(InitializeLibUsb());
    if (libusb_init(&m_pLibContext) != LIBUSB_SUCCESS)
    {
        Printf(Tee::PriError,
               "Failed to initialize libusb\n");
        return RC::USBTEST_CONFIG_FAIL;
    }
    m_ContextInit = true;

    m_pSubDevice = pSubDevice;

    Printf(Tee::PriLow, "Initialization of USB data transfer successful\n");
    return rc;
}

RC UsbDataTransfer::OpenAvailUsbDevices()
{
    RC rc;

    if (!m_ContextInit)
    {
        Printf(Tee::PriError,
               "Usb Data Transfer not initialized\n");
        return RC::USBTEST_CONFIG_FAIL;
    }

    libusb_device **ppDevList;
    ssize_t devCnt = libusb_get_device_list(m_pLibContext, &ppDevList);
    if (devCnt == 0)
    {
        Printf(Tee::PriError,
               "No USB device found\n");
        return RC::USBTEST_CONFIG_FAIL;
    }
    DEFER
    {
        libusb_free_device_list(ppDevList,
                                USB_DECRE_DEV_REF_CNT);
    };

    CHECK_RC(FindMassStorageDev(devCnt, ppDevList));
    if (m_AvailDev.size() == 0)
    {
        Printf(Tee::PriError,
               "No mass storage device found, nothing to open\n");
        return RC::DEVICE_NOT_FOUND;
    }
    Printf(Tee::PriLow,
           "Found %llu external mass storage devices\n",
           static_cast<UINT64>(m_AvailDev.size()));

    for (auto& devItr : m_AvailDev)
    {
        CHECK_RC(OpenDevice(&(devItr)));
    }

    CHECK_RC(ReadMaxLUN());
    CHECK_RC(ReadCapacity());

    m_DevicesOpen = true;

    return rc;
}

RC UsbDataTransfer::TransferUsbData
(
    bool reOpenUsbDevice
   ,bool closeUsbDevice
   ,bool showBandwidthData
   ,UINT32 loops
   ,UINT16 blocks
   ,UINT32 dataTraffic
   ,string fileName
   ,UINT32 transferMode
   ,UINT64 timeMs
   ,UINT32 seed
   ,ScsiCommand scsiRdWrCmd
   ,UINT64 dataBubbleWaitMs
)
{
    RC rc;

    if (!m_ContextInit)
    {
        Printf(Tee::PriError,
               "Usb Data Transfer not initialized\n");
        return RC::USBTEST_CONFIG_FAIL;
    }

    m_ShowBandwidthData = showBandwidthData;
    m_Loops = loops;
    m_Blocks = blocks;
    m_DataTraffic = dataTraffic;
    m_TransferMode = transferMode;
    m_TimeMs = timeMs;
    m_Seed = seed;
    m_DataBubbleWaitMs = dataBubbleWaitMs;

    const char* scsiCmdStr = nullptr;
    switch (scsiRdWrCmd)
    {
        case USB_READ_WRITE_10:
            m_WriteOpCode = USB_REQ_WRITE_10;
            m_ReadOpCode = USB_REQ_READ_10;
            scsiCmdStr = "SCSI(10)";
            break;

        case USB_READ_WRITE_12:
            m_WriteOpCode = USB_REQ_WRITE_12;
            m_ReadOpCode = USB_REQ_READ_12;
            scsiCmdStr = "SCSI(12)";
            break;

        case USB_READ_WRITE_16:
            m_WriteOpCode = USB_REQ_WRITE_16;
            m_ReadOpCode = USB_REQ_READ_16;
            scsiCmdStr = "SCSI(16)";
            break;

        default:
            Printf(Tee::PriError,
                   "Incorrect operation code (%u), valid values:\n"
                   "    0 -> Read(10)/Write(10)\n"
                   "    1 -> Read(12)/Write(12)\n"
                   "    2 -> Read(16)/Write(16)\n",
                   static_cast<UINT08>(scsiRdWrCmd));
            return RC::USB_TRANSFER_ERROR;
    }

    if (m_DataTraffic == USB_DATA_MODE_SPEC)
    {
        if (fileName.empty())
        {
            Printf(Tee::PriError, "Provide appropriate file name\n");
            return RC::USB_TRANSFER_ERROR;
        }

        //Read file only if last read file and current file is different
        if (m_FileName.compare(fileName) != 0)
        {
            m_FileName = fileName;
            CHECK_RC(ReadDataPatternFromFile());
        }
    }

    Printf(Tee::PriLow,
           "Arguments for the data transfer:\n"
           "    ReOpenUsbDevice   : %s\n"
           "    CloseUsbDevice    : %s\n"
           "    ShowBandwidthData : %s\n"
           "    Loops             : %u\n"
           "    Blocks            : %u\n"
           "    DataTraffic       : %u\n"
           "    TransferMode      : %u\n"
           "    TimeMs            : %llu\n"
           "    ScsiCommand       : %s\n"
           "    DataBubbleWaitMs  : %llu\n",
           reOpenUsbDevice ? "true" : "false",
           closeUsbDevice ? "true" : "false",
           showBandwidthData ? "true" : "false",
           loops,
           blocks,
           dataTraffic,
           transferMode,
           timeMs,
           scsiCmdStr,
           dataBubbleWaitMs);

    if (reOpenUsbDevice)
    {
       CHECK_RC(OpenAvailUsbDevices()); 
    }

    ClearMassStorageStructData();

    if (m_AvailDev.size() == 1)
    {
        Tasker::DetachThread detach;

        CHECK_RC(TransferData(&m_AvailDev[0]));
    }
    else
    {
        CHECK_RC(MultipleDeviceTransfer());
    }

    if (m_ShowBandwidthData)
    {
        map<TupleDevInfo, BwDataGbps> mapDevBwGbps;
        CHECK_RC(CallwlateBwGbps(&mapDevBwGbps));
        PrintBandwidthData(mapDevBwGbps);
    }

    if (closeUsbDevice)
    {
        CHECK_RC(CloseUsbDevices());
    }

    Printf(Tee::PriLow, "USB data transfer complete\n");
    return rc;
}

RC UsbDataTransfer::CloseUsbDevices()
{
    StickyRC stickyRc;

    if (!m_ContextInit)
    {
        Printf(Tee::PriError,
               "Usb Data Transfer not initialized\n");
        return RC::USBTEST_CONFIG_FAIL;
    }

    if (m_DevicesOpen)
    {
        for (auto& devItr : m_AvailDev)
        {
            stickyRc = CloseDevice(&(devItr));
        }
        m_DevicesOpen = false;
        m_AvailDev.clear();
    }

    return stickyRc;
}

RC UsbDataTransfer::EnumerateDevices()
{
    RC rc;

    if (!m_ContextInit)
    {
        Printf(Tee::PriError,
               "Usb Data Transfer not initialized\n");
        return RC::USBTEST_CONFIG_FAIL;
    }

    libusb_device **ppDevList;
    ssize_t devCnt = libusb_get_device_list(m_pLibContext, &ppDevList);
    if (devCnt == 0)
    {
        Printf(Tee::PriError,
               "No USB device found\n");
        return RC::USBTEST_CONFIG_FAIL;
    }
    DEFER
    {
        libusb_free_device_list(ppDevList,
                                USB_DECRE_DEV_REF_CNT);
    };

    rc = FindMassStorageDev(devCnt, ppDevList);
    if (rc == RC::DEVICE_NOT_FOUND)
    {
        // Enumerate Only Mode should return the total number of
        // devices found, including 0
        rc.Clear();
    }
    CHECK_RC(rc);

    Printf(Tee::PriLow,
           "Found %llu USB mass storage device(s)\n",
           static_cast<UINT64>(m_AvailDev.size()));
    m_AvailDev.clear();

    return rc;
}

RC UsbDataTransfer::InitializeLibUsb()
{
    RC rc;

    if (IsLibUsbInitialized)
    {
        return rc;
    }

    void *libUsbHandle = nullptr;
    string libusbName = "libusb-1.0" + Xp::GetDLLSuffix();
    string libusbAbsPath = Utility::DefaultFindFile(libusbName, true); 
    if (!Xp::DoesFileExist(libusbAbsPath))
    {
        Printf(Tee::PriLow, "%s not found\n", libusbName.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }
    CHECK_RC(Xp::LoadDLL(libusbAbsPath, &libUsbHandle, Xp::NoDLLFlags));

    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_init,
                               "libusb_init"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_exit,
                               "libusb_exit"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_open,
                               "libusb_open"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_close,
                               "libusb_close"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_get_device_list,
                               "libusb_get_device_list"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_free_device_list,
                               "libusb_free_device_list"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_get_device_descriptor,
                               "libusb_get_device_descriptor"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_get_config_descriptor,
                               "libusb_get_config_descriptor"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_get_string_descriptor_ascii,
                               "libusb_get_string_descriptor_ascii"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_get_bus_number,
                               "libusb_get_bus_number"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_get_device_address,
                               "libusb_get_device_address"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_get_port_number,
                               "libusb_get_port_number"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_get_parent,
                               "libusb_get_parent"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_get_configuration,
                               "libusb_get_configuration"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_set_configuration,
                               "libusb_set_configuration"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_claim_interface,
                               "libusb_claim_interface"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_release_interface,
                               "libusb_release_interface"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_kernel_driver_active,
                               "libusb_kernel_driver_active"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_attach_kernel_driver,
                               "libusb_attach_kernel_driver"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_detach_kernel_driver,
                               "libusb_detach_kernel_driver"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_control_transfer,
                               "libusb_control_transfer"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_bulk_transfer,
                               "libusb_bulk_transfer"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_clear_halt,
                               "libusb_clear_halt"));
    CHECK_RC(GetLibusbFunction(libUsbHandle, &libusb_strerror,
                               "libusb_strerror"));

    IsLibUsbInitialized = true;

    return rc;

}

RC UsbDataTransfer::IsUsbAttachedToLwrrTestDevice(libusb_device *pLwrrDev, bool *saveDev)
{
    RC rc;
    MASSERT(pLwrrDev);
    MASSERT(saveDev);

    libusb_device* pParentDevice = libusb_get_parent(pLwrrDev);
    while (pParentDevice)
    {
        INT32 status;
        libusb_device_descriptor devDesc;
        status = libusb_get_device_descriptor(pParentDevice, &devDesc);
        if (status != LIBUSB_SUCCESS)
        {
            Printf(Tee::PriError, "%s\n",
                   libusb_strerror(static_cast<libusb_error>(status)));
            Printf(Tee::PriError,
                   "Cannot read device descriptor for USB device\n");
            return RC::USBTEST_CONFIG_FAIL;
        }

        libusb_device_handle* pDevHandle;
        status = libusb_open(pParentDevice, &pDevHandle);
        if (status != LIBUSB_SUCCESS)
        {
            Printf(Tee::PriError, "%s\n",
                   libusb_strerror(static_cast<libusb_error>(status)));
            Printf(Tee::PriError,
                   "Cannot open USB device with vid:pid = %u:%u\n",
                   devDesc.idVendor,
                   devDesc.idProduct);
            return RC::USBTEST_CONFIG_FAIL;
        }
        DEFER
        {
            libusb_close(pDevHandle);
        };

        UINT08 serialNum[128] = {};
        status = libusb_get_string_descriptor_ascii(pDevHandle,
                                                    devDesc.iSerialNumber,
                                                    serialNum,
                                                    sizeof(serialNum));
        if (status < 0)
        {
            Printf(Tee::PriError, "%s\n",
                   libusb_strerror(static_cast<libusb_error>(status)));
            Printf(Tee::PriError,
                   "Cannot get string desc for USB device with vid:pid = %u:%u\n",
                   devDesc.idVendor,
                   devDesc.idProduct);
            return RC::USBTEST_CONFIG_FAIL;
        }

        string serialNumStr = reinterpret_cast<char*>(serialNum);
        string splitters(":,.");
        vector<string> pciDBDF;
        boost::split(pciDBDF, serialNumStr, boost::is_any_of(splitters));
        //[Domain:Bus:Device.Function] ==> { [Domain], [Bus], [Device], [Function] }

        if (pciDBDF.size() == 4)
        {
            UINT16 usbDomain = strtoul(pciDBDF[0].c_str(), nullptr, 16);
            UINT16 usbBus = strtoul(pciDBDF[1].c_str(), nullptr, 16);
            UINT16 usbDevice = strtoul(pciDBDF[2].c_str(), nullptr, 16);
            UINT16 usbFunction = strtoul(pciDBDF[3].c_str(), nullptr, 16);

            // Check for USB devices connected on Intel Host
            if (m_pSubDevice->GetParentDevice()->GetFamily() < GpuDevice::Turing
                || m_AllowUsbDeviceOnIntelHost)
            {
                if (!m_VoltaPciDBDFStored)
                {
                    m_VoltaPciDBDF.domain = usbDomain;
                    m_VoltaPciDBDF.bus = usbBus;
                    m_VoltaPciDBDF.device = usbDevice;
                    m_VoltaPciDBDF.function = usbFunction;
                    m_VoltaPciDBDFStored = true;
                }
                else if (m_VoltaPciDBDF.domain != usbDomain
                         || m_VoltaPciDBDF.bus != usbBus
                         || m_VoltaPciDBDF.device != usbDevice
                         || m_VoltaPciDBDF.function != usbFunction)
                {
                    Printf (Tee::PriError,
                            "External Mass Storage Devices connected to multiple "
                            "host controllers (%04x:%x:%02x.%x & %04x:%x:%02x.%x)\n",
                            m_VoltaPciDBDF.domain,
                            m_VoltaPciDBDF.bus,
                            m_VoltaPciDBDF.device,
                            m_VoltaPciDBDF.function,
                            usbDomain,
                            usbBus,
                            usbDevice,
                            usbFunction);
                    return RC::USBTEST_CONFIG_FAIL;
                }
                *saveDev = true;
                return rc;
            }

            UINT16 gpuSbDevDomain, gpuSbDevBus, gpuSbDevDevice;
            m_pSubDevice->GetId().GetPciDBDF(&gpuSbDevDomain,
                                             &gpuSbDevBus,
                                             &gpuSbDevDevice);

            if (usbDomain == gpuSbDevDomain
                && usbBus == gpuSbDevBus
                && usbDevice == gpuSbDevDevice)
            {
                *saveDev = true;
                return rc;
            }
        }

        pParentDevice = libusb_get_parent(pParentDevice);
    }

    *saveDev = false;
    return rc;
}

RC UsbDataTransfer::FindEndPoints
(
    UINT08 deviceClass,
    libusb_config_descriptor *pDevConfig,
    UINT08 *pEndPointIn,
    UINT08 *pEndPointOut,
    bool *pSaveDev
)
{
    RC rc;

    MASSERT(pDevConfig);
    MASSERT(pEndPointIn);
    MASSERT(pEndPointOut);
    MASSERT(pSaveDev);

    bool foundDefaultIn = false;
    bool foundDefaultOut = false;
    for (UINT32 interfaceIdx = 0; interfaceIdx < pDevConfig->bNumInterfaces; interfaceIdx++)
    {
        const libusb_interface *pDevInter = &pDevConfig->interface[interfaceIdx];
        for (INT32 altSetIdx = 0; altSetIdx < pDevInter->num_altsetting; altSetIdx++)
        {
            const libusb_interface_descriptor *pDevInterdesc =
                                               &pDevInter->altsetting[altSetIdx];

            if (deviceClass != LIBUSB_CLASS_MASS_STORAGE
                && pDevInterdesc->bInterfaceClass != LIBUSB_CLASS_MASS_STORAGE)
            {
                continue;
            }

            for (UINT32 endPtIdx = 0; endPtIdx < pDevInterdesc->bNumEndpoints; endPtIdx++)
            {
                const libusb_endpoint_descriptor *pEpDesc =
                                                  &pDevInterdesc->endpoint[endPtIdx];

                if (pEpDesc->bmAttributes & LIBUSB_TRANSFER_TYPE_BULK)
                {
                    if (pEpDesc->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                    {
                        if (!foundDefaultIn)
                        {
                            *pEndPointIn = pEpDesc->bEndpointAddress;
                            if (*pEndPointIn == USB_DEFAULT_ENDPOINT_IN)
                            {
                                foundDefaultIn = true;
                            }
                        }
                    }
                    else //LIBUSB_ENDPOINT_OUT
                    {
                        if (!foundDefaultOut)
                        {
                            *pEndPointOut = pEpDesc->bEndpointAddress;
                            if (*pEndPointOut == USB_DEFAULT_ENDPOINT_OUT)
                            {
                                foundDefaultOut = true;
                            }
                        }
                    }

                    *pSaveDev = true;
                    if (foundDefaultIn && foundDefaultOut)
                    {
                        // No need to look for more endpoints if default endpoints are found
                        return rc;
                    }
                }
            }
        }
    }

    // End point 0 is used for control calls and cannot be used for bulk transfers
    if (*pEndPointIn == 0x0 || *pEndPointOut == 0x0)
    {
        *pSaveDev = false;
    }

    return rc;
}
    


RC UsbDataTransfer::FindMassStorageDev(ssize_t devCnt, libusb_device **ppDevList)
{
    RC rc;
    INT32 status;
    bool saveDev;
    UINT08 connectedUsb3Dev = 0;
    UINT08 connectedUsb2Dev = 0;

    for (UINT32 devIdx = 0; devIdx < devCnt; devIdx++)
    {
        libusb_device_descriptor devDesc;
        status = libusb_get_device_descriptor(ppDevList[devIdx],
                                              &devDesc);
        if (status != LIBUSB_SUCCESS)
        {
            Printf(Tee::PriError, "%s\n",
                   libusb_strerror(static_cast<libusb_error>(status)));
            Printf(Tee::PriError,
                   "Cannot read device descriptor for USB device\n");
            return RC::USBTEST_CONFIG_FAIL;
        }

        switch (devDesc.bDeviceClass)
        {
            case LIBUSB_CLASS_MASS_STORAGE:
            case LIBUSB_CLASS_PER_INTERFACE:
            {
                saveDev = false;
                libusb_config_descriptor *pDevConfig;
                MassStorageDevice lwrrDev;

                status = libusb_get_config_descriptor(ppDevList[devIdx],
                                                      USB_CONFIG_INDEX,
                                                      &pDevConfig);
                if (status != LIBUSB_SUCCESS)
                {
                    Printf(Tee::PriError, "%s\n",
                           libusb_strerror(static_cast<libusb_error>(status)));
                    Printf(Tee::PriError,
                           "Cannot get config desc for USB device with vid:pid = %u:%u\n",
                           devDesc.idVendor,
                           devDesc.idProduct);
                    return RC::USBTEST_CONFIG_FAIL;
                }

                CHECK_RC(FindEndPoints(devDesc.bDeviceClass,
                                       pDevConfig,
                                       &lwrrDev.endPointInAddr,
                                       &lwrrDev.endPointOutAddr,
                                       &saveDev));

                if (saveDev)
                {
                    CHECK_RC(IsUsbAttachedToLwrrTestDevice(ppDevList[devIdx], &saveDev));
                }

                if (saveDev)
                {
                    libusb_device_handle* pDevHandle;
                    status = libusb_open(ppDevList[devIdx],
                                         &pDevHandle);
                    if (status != LIBUSB_SUCCESS)
                    {
                        Printf(Tee::PriError, "%s\n",
                               libusb_strerror(static_cast<libusb_error>(status)));
                        Printf(Tee::PriError,
                               "Cannot open USB device with vid:pid = %u:%u\n",
                               devDesc.idVendor,
                               devDesc.idProduct);
                        return RC::USBTEST_CONFIG_FAIL;
                    }
                    DEFER
                    {
                        libusb_close(pDevHandle);
                    };

                    status = libusb_get_string_descriptor_ascii(pDevHandle,
                                                                devDesc.iSerialNumber,
                                                                lwrrDev.serialNum,
                                                                sizeof(lwrrDev.serialNum));
                    if (status < 0)
                    {
                        Printf(Tee::PriError, "%s\n",
                               libusb_strerror(static_cast<libusb_error>(status)));
                        Printf(Tee::PriError,
                               "Cannot get string desc for USB device with vid:pid = %u:%u\n",
                               devDesc.idVendor,
                               devDesc.idProduct);
                        return RC::USBTEST_CONFIG_FAIL;
                    }

                    lwrrDev.pDevice = ppDevList[devIdx];
                    lwrrDev.bcdUSB = devDesc.bcdUSB;
                    lwrrDev.vendorId = devDesc.idVendor;
                    lwrrDev.productId = devDesc.idProduct;
                    lwrrDev.busNo = libusb_get_bus_number(ppDevList[devIdx]);
                    lwrrDev.addr = libusb_get_device_address(ppDevList[devIdx]);

                    // The bcdUSB field reports the highest version of USB the
                    // device supports. The value is in binary coded decimal
                    // with a format of 0xJJMN where JJ is the major version
                    // number, M is the minor version number and N is the sub
                    // minor version number.
                    // e.g. USB 2.0 is reported as 0x0200,
                    //      USB 1.1 as 0x0110
                    const UINT16 bcdUsbMajorVersion =
                            (devDesc.bcdUSB >> UsbSpecificationNumMajorVerStartBit)
                            & UsbSpecificationNumMajorVerMask;

                    if (bcdUsbMajorVersion == USB_3_SPECIFICATION_MAJOR_VER)
                    {
                        connectedUsb3Dev++;
                    }
                    else if (bcdUsbMajorVersion == USB_2_SPECIFICATION_MAJOR_VER)
                    {
                        connectedUsb2Dev++;
                    }

                    m_AvailDev.push_back(lwrrDev);
                }
                break;
            }

            //Ignoring devices like keyboard, mouse
            //which doesn't belong to Mass Storage Class
            default:
                break;
        }
    }

    // If m_ExpectedUsb3Count == 0 and m_ExpectedUsb3Count == 0, means
    // these values have not been set using function SetExpectedUsbDevices(...)
    // in which case we should skip the check all together
    if (m_ExpectedUsb3Count || m_ExpectedUsb3Count)
    {
        if (connectedUsb3Dev != m_ExpectedUsb3Count 
                || connectedUsb2Dev != m_ExpectedUsb2Count)
        {
            Printf(Tee::PriError,
                   "Incorrect number of USB devices found.\n"
                   "    Expected:\n"
                   "        USB 3 : %u\n"
                   "        USB 2 : %u\n"
                   "    Found:\n"
                   "        USB 3 : %u\n"
                   "        USB 2 : %u\n",
                   m_ExpectedUsb3Count,
                   m_ExpectedUsb2Count,
                   connectedUsb3Dev,
                   connectedUsb2Dev);
            return RC::USBTEST_CONFIG_FAIL;
        }
    }

    return rc;
}

RC UsbDataTransfer::OpenDevice(MassStorageDevice *pLwrrDev)
{
    RC rc;
    MASSERT(pLwrrDev);

    INT32 status;

    if (m_WaitBeforeClaimingUsbDevMs)
    {
        Printf(Tee::PriLow, "Waiting for %u ms before claiming USB device\n",
               m_WaitBeforeClaimingUsbDevMs);
        Tasker::Sleep(m_WaitBeforeClaimingUsbDevMs);
    }

    //Open the device and retrieve the handle
    libusb_device_handle* pDevHandle;
    status = libusb_open(pLwrrDev->pDevice,
                         &pDevHandle);
    if (!pDevHandle)
    {
        Printf(Tee::PriError, "%s\n",
               libusb_strerror(static_cast<libusb_error>(status)));
        Printf(Tee::PriError,
               "Cannot open USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               pLwrrDev->vendorId,
               pLwrrDev->productId,
               pLwrrDev->serialNum);
        return RC::USBTEST_CONFIG_FAIL;
    }
    pLwrrDev->isOpened = true;

    //Retrieve current configuration for the USB device
    INT32 config;
    status = libusb_get_configuration(pDevHandle,
                                      &config);
    if (status != LIBUSB_SUCCESS)
    {
        Printf(Tee::PriError, "%s\n",
               libusb_strerror(static_cast<libusb_error>(status)));
        Printf(Tee::PriError,
               "Cannot fetch configuration for USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               pLwrrDev->vendorId,
               pLwrrDev->productId,
               pLwrrDev->serialNum);
        return RC::USBTEST_CONFIG_FAIL;
    }

    if (config != USB_ACTIVE_CONFIG)
    {
        status = libusb_set_configuration(pDevHandle,
                                          USB_ACTIVE_CONFIG);
        if (status != LIBUSB_SUCCESS)
        {
            Printf(Tee::PriError, "%s\n",
                   libusb_strerror(static_cast<libusb_error>(status)));
            Printf(Tee::PriError,
                   "Cannot set configuration for USB device "
                   "with vid:pid:serialNum = 0x%x:0x%x:%s\n",
                   pLwrrDev->vendorId,
                   pLwrrDev->productId,
                   pLwrrDev->serialNum);
            return RC::USBTEST_CONFIG_FAIL;
        }
    }

    //Check if kernel driver is active for interface 0. Detach if active
    status = libusb_kernel_driver_active(pDevHandle,
                                         USB_INTERFACE_NUM);
    if (status == KERNEL_DRIVER_ACTIVE)
    {
        status = libusb_detach_kernel_driver(pDevHandle,
                                             USB_INTERFACE_NUM);
        if (status != LIBUSB_SUCCESS)
        {
            Printf(Tee::PriError, "%s\n",
                   libusb_strerror(static_cast<libusb_error>(status)));
            Printf(Tee::PriError,
                   "Cannot detach kernel driver for USB device "
                   "with vid:pid:serialNum = 0x%x:0x%x:%s\n",
                   pLwrrDev->vendorId,
                   pLwrrDev->productId,
                   pLwrrDev->serialNum);
            return RC::USBTEST_CONFIG_FAIL;
        }
        pLwrrDev->isKernelDrvDetached = true;
    }
    else if (status != KERNEL_DRIVER_INACTIVE)
    {
        Printf(Tee::PriError, "%s\n",
               libusb_strerror(static_cast<libusb_error>(status)));
        Printf(Tee::PriError,
               "Cannot fetch kernel driver state for USB device "
               "with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               pLwrrDev->vendorId,
               pLwrrDev->productId,
               pLwrrDev->serialNum);
        return RC::USBTEST_CONFIG_FAIL;
    }

    //Claim interface for the device
    status = libusb_claim_interface(pDevHandle,
                                    USB_INTERFACE_NUM);
    if (status != LIBUSB_SUCCESS)
    {
        Printf(Tee::PriError, "%s\n",
               libusb_strerror(static_cast<libusb_error>(status)));
        Printf(Tee::PriError,
               "Cannot claim interface %d for USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               USB_INTERFACE_NUM,
               pLwrrDev->vendorId,
               pLwrrDev->productId,
               pLwrrDev->serialNum);
        return RC::USBTEST_CONFIG_FAIL;
    }
    pLwrrDev->isInterfaceClaimed = true;

    //Save the handle for future use
    pLwrrDev->pHandle = pDevHandle;

    return rc;
}

RC UsbDataTransfer::CloseDevice(MassStorageDevice *pLwrrDev)
{
    RC rc;
    MASSERT(pLwrrDev);

    INT32 status;
    //Release the interface
    if (pLwrrDev->isInterfaceClaimed)
    {
        status = libusb_release_interface(pLwrrDev->pHandle,
                                          USB_INTERFACE_NUM);
        if (status != LIBUSB_SUCCESS)
        {
            Printf(Tee::PriError, "%s\n",
                   libusb_strerror(static_cast<libusb_error>(status)));
            Printf(Tee::PriError,
                   "Cannot release interface %d for USB device vid:pid:serialNum = 0x%x:0x%x:%s\n",
                   USB_INTERFACE_NUM,
                   pLwrrDev->vendorId,
                   pLwrrDev->productId,
                   pLwrrDev->serialNum);
            return RC::USBTEST_CONFIG_FAIL;
        }
        pLwrrDev->isInterfaceClaimed = false;
    }

    //Attach kernel driver if not already active
    if (pLwrrDev->isKernelDrvDetached)
    {
        status = libusb_kernel_driver_active(pLwrrDev->pHandle,
                                             USB_INTERFACE_NUM);
        if (status == KERNEL_DRIVER_INACTIVE)
        {
            status = libusb_attach_kernel_driver(pLwrrDev->pHandle,
                                                 USB_INTERFACE_NUM);
            if (status != LIBUSB_SUCCESS)
            {
                Printf(Tee::PriError, "%s\n",
                       libusb_strerror(static_cast<libusb_error>(status)));
                Printf(Tee::PriError,
                       "Cannot attach kernel driver for USB device "
                       "with vid:pid:serialNum 0x%x:0x%x:%s\n",
                       pLwrrDev->vendorId,
                       pLwrrDev->productId,
                       pLwrrDev->serialNum);
                return RC::USBTEST_CONFIG_FAIL;
            }
        }
        else if (status != KERNEL_DRIVER_ACTIVE)
        {
            Printf(Tee::PriError, "%s\n",
                   libusb_strerror(static_cast<libusb_error>(status)));
            Printf(Tee::PriError,
                   "Cannot fetch kernel driver state for USB device "
                   "with vid:pid:serialNum = 0x%x:0x%x:%s\n",
                   pLwrrDev->vendorId,
                   pLwrrDev->productId,
                   pLwrrDev->serialNum);
            return RC::USBTEST_CONFIG_FAIL;
        }
        pLwrrDev->isKernelDrvDetached = false;
    }

    //Close the device
    if (pLwrrDev->isOpened)
    {
        libusb_close(pLwrrDev->pHandle);
        pLwrrDev->isOpened = false;
    }

    return rc;
}

RC UsbDataTransfer::ReadMaxLUN()
{
    RC rc;

    for (auto& devItr : m_AvailDev)
    {
        INT32 bytesTransferred;
        bytesTransferred = libusb_control_transfer(devItr.pHandle,        //Handle to device
                                                   USB_RT_GET_MAX_LUN,    //Request Type
                                                   USB_REQ_GET_MAX_LUN,   //Request
                                                   0x0,                   //value
                                                   USB_INTERFACE_NUM,     //Index
                                                   &devItr.maxLUN,        //buffer for data
                                                   sizeof(devItr.maxLUN), //Length of data
                                                   USB_TIMEOUT_MS);       //Timeout
        // set LUN to 0 in case of stall
        if (bytesTransferred == 0)
        {
            devItr.maxLUN = 0;
        }
        else if (bytesTransferred < 0)
        {
            Printf(Tee::PriError, "%s\n",
                   libusb_strerror(static_cast<libusb_error>(bytesTransferred)));
            Printf(Tee::PriError,
                   "Cannot read max LUN for USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
                   devItr.vendorId,
                   devItr.productId,
                   devItr.serialNum);
            return RC::USB_TRANSFER_ERROR;
        }
    }

    return rc;
}

RC UsbDataTransfer::ReadCapacity()
{
    RC rc;

    for (auto& devItr : m_AvailDev)
    {
        ReadCapacityDataWrapper readCapacityData;
        CommandBlock cdb(USB_REQ_READ_CAPACITY);

        UINT08 *pRcsw = reinterpret_cast<UINT08 *>(&readCapacityData);

        CHECK_RC(BulkTransfer(&devItr,
                              devItr.endPointInAddr,
                              cdb,
                              pRcsw,
                              USB_LEN_READ_CAPACITY,
                              USB_FETCH_SENSE_DATA_ON_FAILURE,
                              NULL));

        devItr.maxLBA = colwertBigEndianToInt32(readCapacityData.maxLogicalBlockAddress);
        devItr.blockSize = colwertBigEndianToInt32(readCapacityData.blockLengthInBytes);
    }

    return rc;
}

RC UsbDataTransfer::SetDeviceBlocks(MassStorageDevice *pDevItr)
{
    RC rc;
    MASSERT(pDevItr);

    pDevItr->blocks = m_Blocks;
    if (m_DataTraffic == USB_DATA_MODE_SPEC)
    {
        UINT64 pendSize = m_FileData.length() - pDevItr->fileDataIdx;
        if (pendSize < pDevItr->blocks * pDevItr->blockSize)
        {
            pDevItr->blocks = static_cast<UINT16>(ceil(
                                (pendSize / static_cast<float>(pDevItr->blockSize))));
        }
    }
    return rc;
}

RC UsbDataTransfer::SendMassStorageCmd
(
    MassStorageDevice *pDevItr,
    CommandBlock cdb,
    UINT08 direction,
    UINT32 dataLength,
    UINT32 *pExpectedTag
)
{
    RC rc;
    MASSERT(pDevItr);
    MASSERT(pExpectedTag);

    CommandBlockWrapper cbw;
    INT32 status;
    const OperationCode operationCode = cdb.GetOperationCode();
    if (operationCode == USB_REQ_ILWALID)
    {
        Printf(Tee::PriError,
               "Operation Code invalid for USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
        return RC::BAD_PARAMETER;
    }
    Printf(Tee::PriLow, "Sending 0x%x command to USB device "
           "with vid:pid:serialNum = 0x%x:0x%x:%s\n",
           static_cast<UINT08>(operationCode),
           pDevItr->vendorId,
           pDevItr->productId,
           pDevItr->serialNum);

    if (mapCmdLength.count(operationCode) == 0)
    {
        Printf(Tee::PriError,
               "Length of Command Block Descriptor not found for "
               "USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
        return RC::BAD_PARAMETER;
    }

    const UINT08 cdbLen = mapCmdLength.at(operationCode);
    if ((cdbLen == 0) || (cdbLen > sizeof(cbw.CBWCB)))
    {
        Printf(Tee::PriError,
               "Length of Command Descriptor Block has to be between (0, %llu), "
               "length of current block = %u, for USB device "
               "with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               static_cast<UINT64>(sizeof(cbw.CBWCB)),
               cdbLen,
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
        return RC::BAD_PARAMETER;
    }

    const UINT08 *pCdb = cdb.GetUint08Ptr();
    *pExpectedTag = pDevItr->tag;

    memset(&cbw, 0, sizeof(cbw));
    cbw.dCBWSignature[0] = 'U';
    cbw.dCBWSignature[1] = 'S';
    cbw.dCBWSignature[2] = 'B';
    cbw.dCBWSignature[3] = 'C';
    cbw.dCBWTag = pDevItr->tag++;
    cbw.dCBWDataTransferLength = dataLength;
    cbw.bmCBWFlags = direction;
    cbw.bCBWLUN = pDevItr->maxLUN;
    cbw.bCBWCBLength = cdbLen;
    memcpy(cbw.CBWCB, pCdb, cdbLen);

    UINT08 tries = 0;
    INT32 retSize = 0;
    UINT08 *pCbw = reinterpret_cast<UINT08 *>(&cbw);
    do
    {
        status = libusb_bulk_transfer(pDevItr->pHandle,
                                      pDevItr->endPointOutAddr,
                                      pCbw,
                                      USB_LEN_SEND_COMMAND,
                                      &retSize,
                                      USB_TIMEOUT_MS);

        if (status == LIBUSB_ERROR_PIPE)
        {
            //TODO - Add error handling around all instances of libusb_clear_halt
            libusb_clear_halt(pDevItr->pHandle,
                              pDevItr->endPointOutAddr);
        }
        tries++;
    } while ((status == LIBUSB_ERROR_PIPE) && (tries < USB_MAX_RETRIES));

    if (status != LIBUSB_SUCCESS)
    {
        Printf(Tee::PriError, "%s\n",
               libusb_strerror(static_cast<libusb_error>(status)));
        Printf(Tee::PriError,
               "Cannot send command to the USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
        return RC::USB_TRANSFER_ERROR;
    }

    return rc;
}

RC UsbDataTransfer::GetMassStorageStatus
(
    MassStorageDevice *pDevItr,
    UINT08 endpointAddr,
    UINT32 expectedTag,
    bool fetchSenseData
)
{
    RC rc;
    MASSERT(pDevItr);

    INT32 status;
    INT32 retSize;
    CommandStatusWrapper csw;
    UINT08 *pCsw = reinterpret_cast<UINT08 *>(&csw);
    UINT08 tries = 0;
    do
    {
        status = libusb_bulk_transfer(pDevItr->pHandle,
                                      endpointAddr,
                                      pCsw,
                                      USB_LEN_READ_STATUS,
                                      &retSize,
                                      USB_TIMEOUT_MS);
        if (status == LIBUSB_ERROR_PIPE)
        {
            libusb_clear_halt(pDevItr->pHandle, endpointAddr);
        }
        tries++;
    } while ((status == LIBUSB_ERROR_PIPE) && (tries < USB_MAX_RETRIES));

    if (status != LIBUSB_SUCCESS)
    {
        Printf(Tee::PriError, "%s\n",
               libusb_strerror(static_cast<libusb_error>(status)));
        Printf(Tee::PriError,
               "Cannot read status of the USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
        return RC::USB_TRANSFER_ERROR;
    }
    else if (retSize != USB_LEN_READ_STATUS)
    {
        Printf(Tee::PriError,
               "Size of status expected %d bytes, received %d bytes "
               "for USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               USB_LEN_READ_STATUS,
               retSize,
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
        return RC::USB_TRANSFER_ERROR;
    }
    else if (csw.dCSWTag != expectedTag)
    {
        Printf(Tee::PriError,
               "Expected tag %u, received tag %u "
               "for USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               expectedTag,
               csw.dCSWTag,
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
        return RC::USB_TRANSFER_ERROR;
    }
    else if (csw.bCSWStatus)
    {
        if (fetchSenseData == USB_FETCH_SENSE_DATA_ON_FAILURE &&
                            csw.bCSWStatus == USB_SENSE_DATA_VALID)
        {
            CHECK_RC(GetMassStorageSense(pDevItr));
        }
        return RC::USB_TRANSFER_ERROR;
    }

    return rc;
}

RC UsbDataTransfer::GetMassStorageSense(MassStorageDevice *pDevItr)
{
    RC rc;
    MASSERT(pDevItr);

    RequestSenseStandardData senseData;
    CommandBlock cdb(USB_REQ_GET_SENSE, USB_LEN_GET_SENSE);
    UINT08 *pSenseData = reinterpret_cast<UINT08 *>(&senseData);

    CHECK_RC(BulkTransfer(pDevItr,
                          pDevItr->endPointInAddr,
                          cdb,
                          pSenseData,
                          USB_LEN_GET_SENSE,
                          USB_DO_NOT_FETCH_SENSE_DATA_ON_FAILURE,
                          NULL));

    if (USB_SENSE_DATA_PRESENT == (senseData.errorCode & USB_SENSE_DATA_MASK))
    {
        UINT08 senseKey = senseData.senseKey & USB_SENSE_KEY_MASK;
        Printf(Tee::PriError, "%s\n", usbSenseData[senseKey].c_str());

        UINT08 additionalSenseCode = senseData.additionalSenseCode;
        UINT08 additionalSenseCodeQualifier = senseData.additionalSenseCodeQualifier;

        tupleSenseData senseDataKey =
                       make_tuple(senseKey, additionalSenseCode, additionalSenseCodeQualifier);

        const auto &usbSenseDataItr = mapUsbSenseAdditionalData.find(senseDataKey);
        if (usbSenseDataItr != mapUsbSenseAdditionalData.end())
        {
            Printf(Tee::PriError, "%s\n", usbSenseDataItr->second.c_str());
        }
    }
    else
    {
        Printf(Tee::PriError, "No sense data found, may have been over written\n");
        return RC::USB_TRANSFER_ERROR;
    }
    return rc;
}

RC UsbDataTransfer::BulkTransfer
(
    MassStorageDevice *pDevItr,
    UINT08 dataEndPointAddr,
    CommandBlock cdb,
    UINT08 *pDataBuffer,
    UINT32 dataLength,
    bool fetchSenseData,
    BandwidthData *pBwData
)
{
    RC rc;
    MASSERT(pDevItr);
    MASSERT(pDataBuffer);

    INT32 status;
    UINT08 tries = 0;
    UINT32 expectedTag;
    INT32 sizeTransferred;
    UINT64 startTimeUs = 0;
    UINT64 endTimeUs = 0;

    //Step 1: Command Phase - Send Command to the device
    CHECK_RC(SendMassStorageCmd(pDevItr,
                                cdb,                     //Command Descriptor Block
                                dataEndPointAddr,        //Direction for Data Phase
                                dataLength,              //Length of the actual data
                                &expectedTag));          //Expected Tag

    Printf(Tee::PriLow,
           "Transferring data to USB mass storage device with 0x%x:0x%x:%s\n",
           pDevItr->vendorId,
           pDevItr->productId,
           pDevItr->serialNum);

    //Step 2: Data Phase - Read or write data
    do
    {
        //TODO find a way to use HW rather than GetWallTimeUS (look at PTimers)
        startTimeUs = Xp::GetWallTimeUS();

        status = libusb_bulk_transfer(pDevItr->pHandle, //handle to the device
                                      dataEndPointAddr, //Endpoint for data transfer
                                      pDataBuffer,      //Buffer for data
                                      dataLength,       //actual length of data
                                      &sizeTransferred, //actual size of data transferred
                                      USB_TIMEOUT_MS);  //Timeout

        endTimeUs = Xp::GetWallTimeUS();

        if (status == LIBUSB_ERROR_PIPE)
        {
            libusb_clear_halt(pDevItr->pHandle,
                              dataEndPointAddr);
        }
        tries++;
    } while ((status == LIBUSB_ERROR_PIPE) && (tries < USB_MAX_RETRIES));

    if (status != LIBUSB_SUCCESS)
    {
        Printf(Tee::PriError, "%s\n",
               libusb_strerror(static_cast<libusb_error>(status)));
        Printf(Tee::PriError,
               "Cannot transfer data to USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
        return RC::USB_TRANSFER_ERROR;
    }

    Printf(Tee::PriLow,
           "Reading status of USB mass storage device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
           pDevItr->vendorId,
           pDevItr->productId,
           pDevItr->serialNum);

    //Step 3: Status Phase - Read status of the previously exelwted command
    CHECK_RC(GetMassStorageStatus(pDevItr,
                                  pDevItr->endPointInAddr,
                                  expectedTag,
                                  fetchSenseData));
    if (pBwData)
    {
        pBwData->timeUS += endTimeUs - startTimeUs;
        pBwData->bytes += dataLength;   // Amount of actual data transferred
        Printf(Tee::PriLow,
               "%0.2f milli-seconds required for %u bytes of data for USB device "
               "with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               (endTimeUs - startTimeUs) / 1000.0,
               dataLength,
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
    }

    return rc;
}

RC UsbDataTransfer::GenerateInitialBlockNumber(MassStorageDevice *pDevItr)
{
    RC rc;
    MASSERT(pDevItr);

    UINT16 blocks = pDevItr->blocks;
    if (m_DataTraffic == USB_DATA_MODE_SPEC)
    {
        blocks = static_cast<UINT16>(ceil(
                   (m_FileName.length() / static_cast<float>(pDevItr->blockSize))));
    }

    if (pDevItr->maxLBA < static_cast<UINT32>(blocks + USB_RESERVED_BLOCKS))
    {
        Printf(Tee::PriError,
               "Number of blocks to be accessed (%u) is greater than maximum logical "
               "block (%u) for USB device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
               pDevItr->blocks,
               pDevItr->maxLBA - USB_RESERVED_BLOCKS,
               pDevItr->vendorId,
               pDevItr->productId,
               pDevItr->serialNum);
        return RC::USB_TRANSFER_ERROR;
    }

    // +1 to be safe from divide by 0 error
    pDevItr->initialBlockNum = Xp::GetWallTimeUS() % (pDevItr->maxLBA - blocks - USB_RESERVED_BLOCKS + 1);
    pDevItr->initialBlockNum += USB_RESERVED_BLOCKS;

    return rc;
}

UINT32 UsbDataTransfer::GetNextBlockNumber(MassStorageDevice *pDevItr)
{
    UINT32 blockNum = pDevItr->nextBlockNum;
    pDevItr->nextBlockNum += pDevItr->blocks;
    if (pDevItr->nextBlockNum > pDevItr->maxLBA)
    {
        blockNum = 1;
        pDevItr->nextBlockNum = 1 + pDevItr->blocks;
    }
    return blockNum;
}

RC UsbDataTransfer::FillDataBuffer
(
    UINT08 *pData,
    MassStorageDevice *pDevItr
)
{
    RC rc;
    MASSERT(pData);
    MASSERT(pDevItr);
    const UINT32 dataSize = pDevItr->blocks * pDevItr->blockSize;

    switch (m_DataTraffic)
    {
        case USB_DATA_MODE_RANDOM:
            {
                MASSERT(dataSize % 4 == 0);
                Random random;
                random.SeedRandom(m_Seed);
                pDevItr->randomData.clear();
                for (UINT32 j = 0; j < dataSize; j++)
                {
                    UINT32 randomNum = random.GetRandom();
                    pData[j]   = (randomNum >> 0) & 0xFF;
                    pData[++j] = (randomNum >> 8) & 0xFF;
                    pData[++j] = (randomNum >> 16) & 0xFF;
                    pData[++j] = (randomNum >> 24) & 0xFF;
                }
                pDevItr->randomData.insert(pDevItr->randomData.end(), pData, pData + dataSize);
            }
            break;

        case USB_DATA_MODE_ZEROES:
            memset(pData, 0, dataSize);
            break;

        case USB_DATA_MODE_ONES:
            for (UINT32 j = 0; j < dataSize; j++)
            {
                pData[j] = 0xFF;
            }
            break;

        case USB_DATA_MODE_SPEC:
            {
                string lwrFileData = m_FileData.substr(pDevItr->fileDataIdx, dataSize).c_str();
                strncpy(reinterpret_cast<char *>(pData), lwrFileData.c_str(), lwrFileData.size());
            }
            break;

        case USB_DATA_MODE_ALTERNATE_AAAAAAAA_55555555:
            //Data Pattern: Alternate 0xAAAAAAAA 0x55555555
            {
                UINT08 pattern = 0x55;
                for (UINT32 j = 0; j < dataSize; j++)
                {
                    if (j % 4 == 0)
                    {
                        pattern = ~pattern;
                    }
                    pData[j] = pattern;
                }
            }
            break;

        default:
            Printf(Tee::PriError,
                   "Invalid option selected for data traffic. Valid Options: "
                   "0:Random Data, 1:All 0s, 2:All 1s, 3:Spec defined data, "
                   "4: Alternate 0xAAAAAAAA 0x55555555\n");
            return RC::USB_TRANSFER_ERROR;
    }
    return rc;
}

RC UsbDataTransfer::CompareDataBuffer
(
    UINT08 *pData,
    MassStorageDevice *pDevItr
)
{
    RC rc;
    MASSERT(pData);
    MASSERT(pDevItr);
    const UINT32 dataSize = pDevItr->blocks * pDevItr->blockSize;

    switch (m_DataTraffic)
    {
        case USB_DATA_MODE_RANDOM:
            {
                if (pDevItr->randomData.size() != dataSize)
                {
                    return RC::USB_TRANSFER_ERROR;
                }
                vector<UINT08> readData(pData, pData + dataSize);
                if (pDevItr->randomData != readData)
                {
                    return RC::USB_TRANSFER_ERROR;
                }
            }
            break;

        case USB_DATA_MODE_ZEROES:
            for (UINT32 j = 0; j < dataSize; j++)
            {
                if (pData[j] != 0)
                {
                    return RC::USB_TRANSFER_ERROR;
                }
            }
            break;

        case USB_DATA_MODE_ONES:
            for (UINT32 j = 0; j < dataSize; j++)
            {
                if (pData[j] != 0xFF)
                {
                    return RC::USB_TRANSFER_ERROR;
                }
            }
            break;

        case USB_DATA_MODE_SPEC:
            if (strcmp(reinterpret_cast<char *>(pData),
                                  m_FileData.substr(pDevItr->fileDataIdx, dataSize).c_str()))
            {
                return RC::USB_TRANSFER_ERROR;
            }
            break;

        case USB_DATA_MODE_ALTERNATE_AAAAAAAA_55555555:
            //Data Pattern: Alternate 0xAAAAAAAA 0x55555555
            {
                UINT08 pattern = 0x55;
                for (UINT32 j = 0; j < dataSize; j++)
                {
                    if (j % 4 == 0)
                    {
                        pattern = ~pattern;
                    }
                    if (pData[j] != pattern)
                    {
                        return RC::USB_TRANSFER_ERROR;
                    }
                }
            }
            break;

        default:
            Printf(Tee::PriError,
                   "Invalid option selected for data traffic. Valid Options: "
                   "0:Random Data, 1:All 0s, 2:All 1s, 3:Spec defined data, "
                   "4: Alternate 0xAAAAAAAA 0x55555555\n");
            return RC::USB_TRANSFER_ERROR;
    }
    return rc;
}

RC UsbDataTransfer::ReadDataPatternFromFile()
{
    RC rc;
    if (!Xp::DoesFileExist(m_FileName))
    {
        Printf(Tee::PriError,
               "%s file not found. Provide appropriate file name\n",
               m_FileName.c_str());
        return RC::USB_TRANSFER_ERROR;
    }

    if (OK != Xp::InteractiveFileRead(m_FileName.c_str(), &m_FileData))
    {
        Printf(Tee::PriError, "Error while reading file %s\n", m_FileName.c_str());
        return RC::USB_TRANSFER_ERROR;
    }
    return rc;
}

RC UsbDataTransfer::WriteMassStorageData
(
    MassStorageDevice *pDevItr,
    UINT32 seed
)
{
    RC rc;
    MASSERT(pDevItr);

    const UINT32 dataSize = pDevItr->blocks * pDevItr->blockSize;
    vector <UINT08> dataBuffer(dataSize + 1);

    UINT32 blockNum = GetNextBlockNumber(pDevItr);
    CHECK_RC(FillDataBuffer(&dataBuffer[0], pDevItr));
    CommandBlock cdb(m_WriteOpCode, blockNum, pDevItr->blocks);

    Printf(Tee::PriLow,
           "Writing data to %u block number on "
           "USB storage device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
           blockNum,
           pDevItr->vendorId,
           pDevItr->productId,
           pDevItr->serialNum);

    CHECK_RC(BulkTransfer(pDevItr,
                          pDevItr->endPointOutAddr,
                          cdb,
                          &dataBuffer[0],
                          dataSize,
                          USB_FETCH_SENSE_DATA_ON_FAILURE,
                          &pDevItr->writeBwData));

    return rc;
}

RC UsbDataTransfer::ReadMassStorageData
(
    MassStorageDevice *pDevItr,
    UINT32 seed
)
{
    RC rc;
    MASSERT(pDevItr);

    const UINT32 dataSize = pDevItr->blocks * pDevItr->blockSize;
    vector <UINT08> dataBuffer(dataSize + 1);

    UINT32 blockNum = GetNextBlockNumber(pDevItr);
    CommandBlock cdb(m_ReadOpCode, blockNum, pDevItr->blocks);

    Printf(Tee::PriLow,
           "Reading data from %u block number on "
           "USB storage device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
           blockNum,
           pDevItr->vendorId,
           pDevItr->productId,
           pDevItr->serialNum);

    CHECK_RC(BulkTransfer(pDevItr,
                          pDevItr->endPointInAddr,
                          cdb,
                          &dataBuffer[0],
                          dataSize,
                          USB_FETCH_SENSE_DATA_ON_FAILURE,
                          &pDevItr->readBwData));

    if (m_TransferMode != USB_DATA_MODE_RD_ONLY)
    {
        rc = CompareDataBuffer(&dataBuffer[0], pDevItr);
        if (rc != OK)
        {
            Printf(Tee::PriError,
                   "Discrepancy found between written and read data for "
                   "usb device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
                   pDevItr->vendorId,
                   pDevItr->productId,
                   pDevItr->serialNum);
            return rc;
        }
    }

    return rc;
}

RC UsbDataTransfer::TransferData(MassStorageDevice *pDevItr)
{
    RC rc;
    MASSERT(pDevItr);

    bool timeUp = false;
    UINT64 startTimeMs = 0;
    if (m_TimeMs)
    {
        startTimeMs = Xp::GetWallTimeMS();
        m_Loops = numeric_limits<UINT32>::max();
    }

    for (UINT32 i = 0; i < m_Loops; i++)
    {
        CHECK_RC(GenerateInitialBlockNumber(pDevItr));

        if (m_TransferMode != USB_DATA_MODE_RD_ONLY)
        {
            pDevItr->nextBlockNum = pDevItr->initialBlockNum;
            pDevItr->fileDataIdx = 0;
            do
            {
                if (m_TimeMs)
                {
                    if (Xp::GetWallTimeMS() - startTimeMs >= m_TimeMs)
                    {
                        timeUp = true;
                        break;
                    }
                }

                CHECK_RC(SetDeviceBlocks(pDevItr));
                CHECK_RC(WriteMassStorageData(pDevItr, i));

                if (m_DataTraffic == USB_DATA_MODE_SPEC)
                {
                    pDevItr->fileDataIdx += pDevItr->blocks * pDevItr->blockSize;
                }
            } while (m_DataTraffic == USB_DATA_MODE_SPEC &&
                            pDevItr->fileDataIdx < m_FileData.length());
        }

        if (m_TransferMode != USB_DATA_MODE_WR_ONLY)
        {
            pDevItr->nextBlockNum = pDevItr->initialBlockNum;
            pDevItr->fileDataIdx = 0;
            do
            {
                if (m_TimeMs)
                {
                    if (Xp::GetWallTimeMS() - startTimeMs >= m_TimeMs)
                    {
                        timeUp = true;
                        break;
                    }
                }

                CHECK_RC(SetDeviceBlocks(pDevItr));
                CHECK_RC(ReadMassStorageData(pDevItr, i));
                if (m_DataTraffic == USB_DATA_MODE_SPEC)
                {
                    pDevItr->fileDataIdx += pDevItr->blocks * pDevItr->blockSize;
                }
            } while (m_DataTraffic == USB_DATA_MODE_SPEC &&
                             pDevItr->fileDataIdx < m_FileData.length());
        }

        if (m_TimeMs)
        {
            if (timeUp)
            {
                break;
            }
            else if (i == m_Loops - 1)
            {
                i = 0;
            }
        }
        if (m_DataBubbleWaitMs != 0)
        {
            Printf(Tee::PriLow,
                   "Waiting for %llu milliseconds before resuming transfer on "
                   "usb device with vid:pid:serialNum = 0x%x:0x%x:%s\n",
                   m_DataBubbleWaitMs,
                   pDevItr->vendorId,
                   pDevItr->productId,
                   pDevItr->serialNum);
            Tasker::Sleep(m_DataBubbleWaitMs);
        }
    }

    return rc;
}

void MultipleDeviceTransferThread(void *pData)
{
    MASSERT(pData);

    Tasker::DetachThread detach;

    UsbDataTransfer::ThreadData *pThreadData = 
                    reinterpret_cast<UsbDataTransfer::ThreadData *>(pData);

    Cpu::AtomicAdd(pThreadData->pWaitingThreadCnt, 1);
    Tasker::WaitOnEvent(pThreadData->pEvent);

    pThreadData->stickyRc = pThreadData->obj->TransferData(pThreadData->pLwrrDev);
}

RC UsbDataTransfer::MultipleDeviceTransfer()
{
    StickyRC stickyRc;

    if (m_AvailDev.size() > USB_MAX_DEVICES_ALLOWED)
    {
        Printf (Tee::PriError, "%u devices supported, %llu connected\n",
                USB_MAX_DEVICES_ALLOWED,
                static_cast<UINT64>(m_AvailDev.size()));
        return RC::USB_TRANSFER_ERROR;
    }

    ThreadData threadData[USB_MAX_DEVICES_ALLOWED];
    Tasker::ThreadID threadId[USB_MAX_DEVICES_ALLOWED];

    ModsEvent *pEvent = Tasker::AllocEvent();
    if (!pEvent)
    {
        Printf(Tee::PriError, "Failed to create Tasker Event\n");
        return RC::USB_TRANSFER_ERROR;
    }

    UINT32 waitingThreadCnt = 0;
    UINT32 threadCnt = 0;
    for (auto& devItr : m_AvailDev)
    {
        threadData[threadCnt].obj = this;
        threadData[threadCnt].pLwrrDev = &devItr;
        threadData[threadCnt].pEvent = pEvent;
        threadData[threadCnt].pWaitingThreadCnt = &waitingThreadCnt;

        threadId[threadCnt] = Tasker::CreateThread(MultipleDeviceTransferThread,
                                                   &threadData[threadCnt],
                                                   Tasker::MIN_STACK_SIZE,
                                                   "MultipleDeviceTransferThread");
        threadCnt++;
    }

    while (Cpu::AtomicRead(&waitingThreadCnt) != threadCnt)
    {
        Tasker::Yield();
    }

    if (!Tasker::IsEventSet(pEvent))
    {
        Tasker::SetEvent(pEvent);
    }

    for (UINT32 i = 0; i < threadCnt; i++)
    {
        Tasker::Join(threadId[i]);
        stickyRc = threadData[i].stickyRc;
    }

    if (pEvent)
    {
        Tasker::FreeEvent(pEvent);
    }

    return stickyRc;
}

void UsbDataTransfer::ClearMassStorageStructData()
{
    // Clear the data members for next call to TransferUsbData
    for (auto& devItr : m_AvailDev)
    {
        devItr.fileDataIdx = 0;

        devItr.readBwData.timeUS = 0;
        devItr.readBwData.bytes = 0;

        devItr.writeBwData.timeUS = 0;
        devItr.writeBwData.bytes = 0;

        devItr.randomData.clear();
    }
}

void UsbDataTransfer::PrintBandwidthData(const map<TupleDevInfo, BwDataGbps> &mapDevBwGbps) const
{
    FLOAT64 totalDataGBits = 0.0;
    FLOAT64 maxReadTimeSec = 0.0;
    FLOAT64 maxWriteTimeSec = 0.0;

    for (const auto& devBwItr : mapDevBwGbps)
    {
        FLOAT64 writeDataGBits = devBwItr.second.writeGbits;
        FLOAT64 writeTimeSec = devBwItr.second.writeTimeSec;
        FLOAT64 writeBw = (writeTimeSec ? writeDataGBits / writeTimeSec : 0);

        FLOAT64 readDataGBits = devBwItr.second.readGbits;
        FLOAT64 readTimeSec = devBwItr.second.readTimeSec;
        FLOAT64 readBw = (readTimeSec ? readDataGBits / readTimeSec : 0);

        FLOAT64 totalTime = writeTimeSec + readTimeSec;
        FLOAT64 avgBw = (totalTime ? (writeDataGBits + readDataGBits) / totalTime : 0);

        totalDataGBits += writeDataGBits + readDataGBits;
        if (maxWriteTimeSec < writeTimeSec)
        {
            maxWriteTimeSec = writeTimeSec;
        }
        if (maxReadTimeSec < readTimeSec)
        {
            maxReadTimeSec = readTimeSec;
        }

        Printf(Tee::PriNormal,
               "Bandwidth usage for usb device with vid:pid:serialNum = 0x%x:0x%x:%s\n"
               "    Write to USB device  : %0.2f Gbit/s\n"
               "    Read from USB device : %0.2f Gbit/s\n"
               "    Average              : %0.2f Gbit/s\n",
               get<0>(devBwItr.first),
               get<1>(devBwItr.first),
               (get<2>(devBwItr.first)).c_str(),
               writeBw,
               readBw,
               avgBw);
    }

    FLOAT64 totalTime = maxWriteTimeSec + maxReadTimeSec;
    if (mapDevBwGbps.size() > 1)
    {
        Printf(Tee::PriNormal,
               "Combined bandwidth usage for %llu devices : %0.2f Gbit/s\n",
               static_cast<UINT64>(m_AvailDev.size()),
               (totalTime ? (totalDataGBits / totalTime) : 0));
    }
}

RC UsbDataTransfer::CallwlateBwGbps(map<TupleDevInfo, BwDataGbps> *pMapDevBwGbps)
{
    RC rc;
    MASSERT(pMapDevBwGbps);

    pMapDevBwGbps->clear();
    for (const auto& devItr : m_AvailDev)
    {
        BwDataGbps bwGpbs;
        bwGpbs.writeGbits = devItr.writeBwData.bytes * 8 / 1024.0 / 1024.0 / 1024.0;
        bwGpbs.writeTimeSec = devItr.writeBwData.timeUS / 1000000.0;

        bwGpbs.readGbits = devItr.readBwData.bytes * 8 / 1024.0 / 1024.0 / 1024.0;
        bwGpbs.readTimeSec = devItr.readBwData.timeUS / 1000000.0;

        const string serialNumStr = reinterpret_cast<const char*>(devItr.serialNum);
        TupleDevInfo devIdentifier = make_tuple
                            (devItr.vendorId, devItr.productId, serialNumStr);
        (*pMapDevBwGbps)[devIdentifier] = bwGpbs;
    }
    return rc;
}

//TODO MP-553: remove after Turing Bringup - needed for Pre Turing setup
RC UsbDataTransfer::GetHostCtrlPciDBDF
(
    UINT32 *pDomain,
    UINT32 *pBus,
    UINT32 *pDevice,
    UINT32 *pFunction
)
{
    RC rc;

    if (!m_ContextInit)
    {
        Printf(Tee::PriError,
               "Usb Data Transfer not initialized\n");
        return RC::USBTEST_CONFIG_FAIL;
    }

    MASSERT(pDomain);
    MASSERT(pBus);
    MASSERT(pDevice);
    MASSERT(pFunction);

    if (!m_VoltaPciDBDFStored)
    {
        libusb_device **ppDevList;
        ssize_t devCnt = libusb_get_device_list(m_pLibContext, &ppDevList);
        if (devCnt == 0)
        {
            Printf(Tee::PriLow,
                   "No USB device found\n");
            return RC::USBTEST_CONFIG_FAIL;
        }

        DEFER
        {
            libusb_free_device_list(ppDevList,
                                    USB_DECRE_DEV_REF_CNT);
        };

        CHECK_RC(FindMassStorageDev(devCnt, ppDevList));
        if (m_AvailDev.size() == 0)
        {
            Printf(Tee::PriLow,
                   "No mass storage device found. Unable to find PCI DBDF for host controller\n");
            return RC::DEVICE_NOT_FOUND;
        }

        m_AvailDev.clear();
    }

    *pDomain = m_VoltaPciDBDF.domain;
    *pBus = m_VoltaPciDBDF.bus;
    *pDevice = m_VoltaPciDBDF.device;
    *pFunction = m_VoltaPciDBDF.function;

    return rc;
}

UsbDataTransfer::CommandBlock::CommandBlock(OperationCode op)
{
    m_OperationCode = op;
}

UsbDataTransfer::CommandBlock::CommandBlock(OperationCode op, UINT08 allocationUnitLen)
{
    m_OperationCode = op;
    m_CmdGetSenseParam.allocationUnitLen = allocationUnitLen;
}

UsbDataTransfer::CommandBlock::CommandBlock(OperationCode op, UINT32 blockNum, UINT16 numOfBlocks)
{
    m_OperationCode = op;

    switch (op)
    {
        case USB_REQ_WRITE_10:
        case USB_REQ_READ_10:
            m_CmdRdWr10Param.lba.maxLbaH = (blockNum >> 24) & 0xFF;
            m_CmdRdWr10Param.lba.maxLbaL = (blockNum >> 16) & 0xFF;
            m_CmdRdWr10Param.lba.minLbaH = (blockNum >> 8) & 0xFF;
            m_CmdRdWr10Param.lba.minLbaL = (blockNum >> 0) & 0XFF;

            m_CmdRdWr10Param.transferLen.transferLenH = (numOfBlocks >> 8) & 0xFF;
            m_CmdRdWr10Param.transferLen.transferLenL = (numOfBlocks >> 0) & 0xFF;
            break;

        case USB_REQ_WRITE_12:
        case USB_REQ_READ_12:
            m_CmdRdWr12Param.lba.maxLbaH = (blockNum >> 24) & 0xFF;
            m_CmdRdWr12Param.lba.maxLbaL = (blockNum >> 16) & 0xFF;
            m_CmdRdWr12Param.lba.minLbaH = (blockNum >> 8) & 0xFF;
            m_CmdRdWr12Param.lba.minLbaL = (blockNum >> 0) & 0XFF;

            m_CmdRdWr12Param.minTransferLen.transferLenH = (numOfBlocks >> 8) & 0xFF;
            m_CmdRdWr12Param.minTransferLen.transferLenL = (numOfBlocks >> 0) & 0xFF;
            break;

        case USB_REQ_WRITE_16:
        case USB_REQ_READ_16:
            m_CmdRdWr16Param.lbaLow.maxLbaH = (blockNum >> 24) & 0xFF;
            m_CmdRdWr16Param.lbaLow.maxLbaL = (blockNum >> 16) & 0xFF;
            m_CmdRdWr16Param.lbaLow.minLbaH = (blockNum >> 8) & 0xFF;
            m_CmdRdWr16Param.lbaLow.minLbaL = (blockNum >> 0) & 0XFF;

            m_CmdRdWr16Param.minTransferLen.transferLenH = (numOfBlocks >> 8) & 0xFF;
            m_CmdRdWr16Param.minTransferLen.transferLenL = (numOfBlocks >> 0) & 0xFF;
            break;

        default:
            Printf (Tee::PriError,
                    "Incorrect Operation Code: %u\n",
                    static_cast<UINT08>(op));
            break;
    }
}
