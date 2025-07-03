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

#pragma once

#include "libusb.h"

class GpuSubdevice;
class CommandBlock;

class UsbDataTransfer
{
public:
    enum UsbDataMode
    {
        USB_DATA_MODE_RANDOM                      = 0  //Transfer Random data
       ,USB_DATA_MODE_ZEROES                      = 1  //Transfer all 0's
       ,USB_DATA_MODE_ONES                        = 2  //Transfer all 1's
       ,USB_DATA_MODE_SPEC                        = 3  //Transfer spec defined data
       ,USB_DATA_MODE_ALTERNATE_AAAAAAAA_55555555 = 4  //Transfer alternate 0xAAAAAAAA 0x55555555
    };

    enum UsbDataTransferMode
    {
        USB_DATA_MODE_RD_WR     = 0        //Perform Write and Read operations
       ,USB_DATA_MODE_WR_ONLY   = 1        //Perform only write operations
       ,USB_DATA_MODE_RD_ONLY   = 2        //Perform only read operations
    };

    // Different varients of SCSI Read/Write commands supported
    enum ScsiCommand : UINT08
    {
        USB_READ_WRITE_10 = 0x0,    // Use READ(10)/WRITE(10)
        USB_READ_WRITE_12 = 0x1,    // Use READ(12)/WRITE(12)
        USB_READ_WRITE_16 = 0x2     // Use READ(16)/WRITE(16)
    };

    struct BwDataGbps
    {
        FLOAT64 readTimeSec = 0.0;
        FLOAT64 readGbits = 0.0;
        FLOAT64 writeTimeSec = 0.0;
        FLOAT64 writeGbits = 0.0;
    };
    typedef tuple<UINT16, UINT16, string> TupleDevInfo;

    //On Turing the XUSB host controller has 4 USB3 ports and 2 USB2 ports
    //At any given instance only 1 USB3 port and/or 1 USB2 port can be active
    //thus restricting the number of connected USB devices to 2
    //If devices are connected to a hub and that hub is connected to Turing,
    //it would count only as 1 device
    //TODO use vector so that there is no restriction on number of device
    static const UINT08 USB_MAX_DEVICES_ALLOWED = 8;

    //Linux and windows places a limit on maximum packet size
    //Current Packet size: 12800 * 512 bytes (block size) = 6.25 MB
    static const UINT16 USB_DEFAULT_NUM_OF_BLOCKS = 12800;

    UsbDataTransfer();
    ~UsbDataTransfer();
    RC Initialize(GpuSubdevice *pSubDevice);
    RC OpenAvailUsbDevices();
    RC TransferUsbData
    (
        bool reOpenUsbDevice = true                 //Open USB device before data transfer
       ,bool closeUsbDevice = true                  //Close USB device after data transfer
       ,bool showBandwidthData = false              //Print Bandwidth details
       ,UINT32 loops = 1                            //Number of times to transfer same data
       ,UINT16 blocks = USB_DEFAULT_NUM_OF_BLOCKS   //Number of blocks to transfer in 1 packet
       ,UINT32 dataTraffic = USB_DATA_MODE_RANDOM   //Pattern of data to be transferred
       ,string fileName = ""                        //External file to read data pattern from
       ,UINT32 transferMode = USB_DATA_MODE_RD_WR   //Direction of data transfer
       ,UINT64 timeMs = 0                           //Time in milliseconds to run the test
       ,UINT32 seed = 1                             //Seed for Random Data Generation
       ,ScsiCommand scsiRdWrCmd = USB_READ_WRITE_10 //Use READ/WRITE _10/_12/_16
       ,UINT64 dataBubbleWaitMs = 0                 //Time in MS to wait between data transfers
    );
    RC CloseUsbDevices();
    RC EnumerateDevices();
    void PrintBandwidthData(const map<TupleDevInfo, BwDataGbps> &mapDevBwGbps) const;
    RC CallwlateBwGbps(map<TupleDevInfo, BwDataGbps> *pMapDevBwGbps);
    RC SetExpectedUsbDevices(UINT08 expectedUsb3Count, UINT08 expectedUsb2Count)
    {
        m_ExpectedUsb3Count = expectedUsb3Count;
        m_ExpectedUsb2Count = expectedUsb2Count;
        return OK;
    }
    RC AllowUsbDeviceOnIntelHost(bool allowUsbDeviceOnIntelHost)
    {
        m_AllowUsbDeviceOnIntelHost = allowUsbDeviceOnIntelHost;
        return OK;
    }
    RC WaitBeforeClaimingUsbDevMs(UINT32 waitBeforeClaimingUsbDevMs)
    {
        m_WaitBeforeClaimingUsbDevMs = waitBeforeClaimingUsbDevMs;
        return RC::OK;
    }

    //TODO MP-553: remove after Turing Bringup - needed for Pre Turing setup
    RC GetHostCtrlPciDBDF(UINT32 *pDomain, UINT32 *pBus, UINT32 *pDevice, UINT32 *pFunction);

    friend void MultipleDeviceTransferThread(void *pData);

private:

    enum UsbSpecificationNumMajorVer : UINT16
    {
        USB_2_SPECIFICATION_MAJOR_VER = 0x02
       ,USB_3_SPECIFICATION_MAJOR_VER = 0x03
    };
    const UINT16 UsbSpecificationNumMajorVerStartBit = 8;
    const UINT16 UsbSpecificationNumMajorVerMask = 0xFF;

    enum OperationCode : UINT08
    {
        USB_REQ_WRITE_10       = 0x2A
       ,USB_REQ_READ_10        = 0x28
       ,USB_REQ_WRITE_12       = 0xAA
       ,USB_REQ_READ_12        = 0xA8
       ,USB_REQ_WRITE_16       = 0x8A
       ,USB_REQ_READ_16        = 0x88
       ,USB_REQ_GET_SENSE      = 0x3
       ,USB_REQ_READ_CAPACITY  = 0x25
       ,USB_REQ_ILWALID        = 0xFF
    };

    //length for all commands used
    const map <OperationCode, UINT08> mapCmdLength =
    {
        { USB_REQ_WRITE_10,         10 },
        { USB_REQ_READ_10,          10 },
        { USB_REQ_WRITE_12,         12 },
        { USB_REQ_READ_12,          12 },
        { USB_REQ_WRITE_16,         16 },
        { USB_REQ_READ_16,          16 },
        { USB_REQ_GET_SENSE,        6  },
        { USB_REQ_READ_CAPACITY,    10 }
    };

    class CommandBlock
    {
    public:
        CommandBlock(OperationCode op);
        CommandBlock(OperationCode op, UINT08 allocationUnitLen);
        CommandBlock(OperationCode op, UINT32 blockNum, UINT16 numOfBlocks);
        OperationCode GetOperationCode() const { return m_OperationCode; };
        UINT08 *GetUint08Ptr() { return reinterpret_cast<UINT08 *>(this); };
    private:
#pragma pack(push, 1)
        struct LogicalBlockAddress
        {
            UINT08 maxLbaH = 0;
            UINT08 maxLbaL = 0;
            UINT08 minLbaH = 0;
            UINT08 minLbaL = 0;
        };

        struct TransferLen
        {
            UINT08 transferLenH = 0;
            UINT08 transferLenL = 0;
        };

        //Command Type
        //-USB_REQ_GET_SENSE
        struct CmdGetSenseParam
        {
            UINT08 reserved1[2]      = { 0 };
            UINT08 allocationUnitLen = 0;
            UINT08 reserved2[9]      = { 0 };
        };

        //Command Type
        //-USB_REQ_WRITE_10
        //-USB_REQ_READ_10
        struct CmdRdWr10Param
        {
            LogicalBlockAddress lba;
            UINT08 reserved1 = 0;
            TransferLen transferLen;
            UINT08 reserved2[5] = { 0 };
        };

        //Command Type
        //-USB_REQ_WRITE_12
        //-USB_REQ_READ_12
        struct CmdRdWr12Param
        {
            LogicalBlockAddress lba;
            TransferLen         maxTransferLen;
            TransferLen         minTransferLen;
            UINT08 reserved[4] = { 0 };
        };

        //Command Type
        //-USB_REQ_WRITE_16
        //-USB_REQ_READ_16
        struct CmdRdWr16Param
        {
            LogicalBlockAddress lbaHigh;
            LogicalBlockAddress lbaLow;
            TransferLen         maxTransferLen;
            TransferLen         minTransferLen;
        };

        // Total size - 16 bytes
        OperationCode m_OperationCode;              // 1 byte
        UINT08        m_LogicalUnitNum = 0;         // 1 byte
        union
        {                                           // 12 bytes
            CmdGetSenseParam m_CmdGetSenseParam;
            CmdRdWr10Param m_CmdRdWr10Param;
            CmdRdWr12Param m_CmdRdWr12Param;
            CmdRdWr16Param m_CmdRdWr16Param;
            UINT08 m_Reserved1[12] = { 0 };
        };
        UINT08        m_Reserved[2] = { 0 };       // 2 bytes
#pragma pack(pop)
    };

    struct BandwidthData
    {
        UINT64 timeUS = 0;
        UINT64 bytes = 0;
    };

    struct MassStorageDevice
    {
        libusb_device *pDevice             = nullptr;

        //Handle to the device, obtained after opening the device
        libusb_device_handle *pHandle      = nullptr;

        //USB specification release number in binary-coded decimal
        UINT16 bcdUSB                      = 0;

        //Vendor ID of the device
        UINT16 vendorId                    = 0;

        //Product ID of the device
        UINT16 productId                   = 0;

        //Index of string descriptor containing device serial number
        UINT08 serialNum[128]              = { 0 };

        //Device Bus Number
        UINT08 busNo                       = 0;

        //Device Address
        UINT08 addr                        = 0;

        //Maximum Logical Unit Number for the device
        UINT08 maxLUN                      = 0;

        //Address of endpoint to read data in
        UINT08 endPointInAddr              = 0;

        //Address of endpoint to send data out
        UINT08 endPointOutAddr             = 0;

        //Max Logical Block Addressing
        UINT32 maxLBA                      = 0;

        //Size of each block
        UINT32 blockSize                   = 0;

        //Number of blocks to be transfered in each packet
        UINT16 blocks                      = 0;

        //Randomly generate the initial block number to write data
        UINT32 initialBlockNum             = 0;

        //Store the next block number
        UINT32 nextBlockNum                = 0;

        //Flag set true if device is opened
        bool isOpened                      = false;

        //Flag set true if kernel driver is detached
        bool isKernelDrvDetached           = false;

        //Flag set true if interface for the device is claimed
        bool isInterfaceClaimed            = false;

        //Tag sent along with command, device will echo back in status
        UINT32 tag                         = 1;

        //Index of last byte transferred from m_FileData
        UINT32 fileDataIdx                 = 0;

        // Time required and bytes transfered while reading from the device
        BandwidthData readBwData;

        // Time required and bytes transfered while writing to the device
        BandwidthData writeBwData;

        //Vector to store random data
        vector<UINT08> randomData;
    };

    struct ThreadData
    {
        UsbDataTransfer *obj;
        MassStorageDevice *pLwrrDev;
        ModsEvent *pEvent;
        UINT32 *pWaitingThreadCnt;
        StickyRC stickyRc;
    };

    //TODO MP-553: remove after Turing Bringup - needed for Pre Turing setup
    struct VoltaPciDBDF
    {
        UINT32 domain   = ~0U;
        UINT32 bus      = ~0U;
        UINT32 device   = ~0U;
        UINT32 function = ~0U;
    };

    RC InitializeLibUsb();
    RC IsUsbAttachedToLwrrTestDevice(libusb_device *pLwrrDev, bool *saveDev);
    RC FindEndPoints
    (
        UINT08 deviceClass,
        libusb_config_descriptor *pDevConfig,
        UINT08 *pEndPointIn,
        UINT08 *pEndPointOut,
        bool *pSaveDev
    );
    RC FindMassStorageDev(ssize_t devCnt, libusb_device **ppDevList);
    RC OpenDevice(MassStorageDevice *pLwrrDev);
    RC CloseDevice(MassStorageDevice *pLwrrDev);
    RC ReadMaxLUN();
    RC ReadCapacity();
    RC SetDeviceBlocks(MassStorageDevice *pDevItr);

    RC BulkTransfer
    (
        MassStorageDevice *pDevItr,   //Device object
        UINT08 dataEndPointAddr,      //endpoint based on direction of data transfer
        CommandBlock cdb,             //SCSI Command Descriptor Block
        UINT08 *pDataBuffer,          //Buffer to store data
        UINT32 dataLength,            //length of actual data
        bool fetchSenseData,          //should GetMassStorageSense function be ilwoked
        BandwidthData *pBwData        //Stores the amount of time needed and data transferred
    );

    RC SendMassStorageCmd
    (
        MassStorageDevice *pDevItr,
        CommandBlock cdb,       //SCSI Command Descriptor Block
        UINT08 direction,       //endpoint: direction for data transfer IN/OUT
        UINT32 dataLength,      //length of actual data
        UINT32 *pExpectedTag    //Tag used for error checking
    );

    RC GetMassStorageStatus
    (
        MassStorageDevice *pDevItr,
        UINT08 endpointAddr,  //Address of IN endpoint
        UINT32 expectedTag,   //tag used for error checking
        bool fetchSenseData   //should GetMassStorageSense function be ilwoked
    );

    RC GetMassStorageSense(MassStorageDevice *pDevItr);
    RC GenerateInitialBlockNumber(MassStorageDevice *pDevItr);
    UINT32 GetNextBlockNumber(MassStorageDevice *pDevItr);
    RC FillDataBuffer
    (
        UINT08 *pData,
        MassStorageDevice *pDevItr
    );
    RC CompareDataBuffer
    (
        UINT08 *pData,
        MassStorageDevice *pDevItr
    );
    RC ReadDataPatternFromFile();
    RC WriteMassStorageData(MassStorageDevice *pDevItr, UINT32 seed);
    RC ReadMassStorageData(MassStorageDevice *pDevItr, UINT32 seed);
    RC TransferData(MassStorageDevice *pDevItr);
    RC MultipleDeviceTransfer();
    void ClearMassStorageStructData();

    GpuSubdevice                *m_pSubDevice;
    libusb_context              *m_pLibContext;
    vector<MassStorageDevice>   m_AvailDev;
    string                      m_FileData;
    string                      m_FileName;
    UINT32                      m_DataTraffic;
    UINT32                      m_Loops;
    UINT32                      m_TransferMode;
    UINT16                      m_Blocks;
    bool                        m_ShowBandwidthData;
    UINT64                      m_TimeMs;
    UINT32                      m_Seed;
    bool                        m_ContextInit;
    bool                        m_DevicesOpen;
    OperationCode               m_WriteOpCode;
    OperationCode               m_ReadOpCode;
    UINT64                      m_DataBubbleWaitMs;
    UINT08                      m_ExpectedUsb3Count;
    UINT08                      m_ExpectedUsb2Count;
    bool                        m_AllowUsbDeviceOnIntelHost;
    UINT32                      m_WaitBeforeClaimingUsbDevMs;

    //TODO MP-553: remove after Turing Bringup - needed for Pre Turing setup
    VoltaPciDBDF                m_VoltaPciDBDF;
    bool                        m_VoltaPciDBDFStored;
};
