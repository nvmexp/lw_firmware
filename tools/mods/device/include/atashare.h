/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_ATASHARE_H
#define INCLUDED_ATASHARE_H

#ifndef INLWDED_TYPES_H
#include "core/include/types.h"
#endif

//********************* ATA/IDE register Constants ****************************/
#define ATA_IRQ_PRI    14
#define ATA_IRQ_SEC    15
//********************* ATA/IDE register Constants ****************************/

//! Hard Drive head location structure
typedef struct _ataHdLocation
{
    UINT08 Head;
    UINT16 Cylinder;
    UINT08 Sector;
} AtaHdLocation;

typedef enum _atapiDataType
{
    AtaPiDataNon
    ,AtaPiDataIn
    ,AtaPiDataOut
} AtaPiDataType;

typedef struct _atapiConfig
{
    UINT16 PacketSize:2;
    UINT16 Reseerv1:3;
    UINT16 DrqType:2;
    UINT16 Rem:1;
    UINT16 Device:5;
    UINT16 Reserv2:1;
    UINT16 Protocal:2;
} AtaPiConfig, *PAtaPiConfig;

typedef struct _atapiCap
{
    UINT16 VendorSpec:8;
    UINT16 Dma:1;
    UINT16 Lba:1;
    UINT16 IordyDe:1;
    UINT16 Iordy:1;
    UINT16 Reserv:1;
    UINT16 Overlap:1;
    UINT16 Proxy:1;
    UINT16 EmDma:1;
} AtaPiCap, *PAtaPiCap;

typedef enum
{
    MODEL=0,
    BASIC=1,
    ADVANCED=2,
    DETAILED=3
}DriveInfoLevel;

/*******************************************************************************
                 Defination about Channel and Drive
*******************************************************************************/
#define ATA_MAX_DRV_PER_CH      2

#define ATA_BYTES_PER_SECTOR    512 // bytes per sector
#define ATA_WORDS_PER_SECTOR    256 // bytes per sector

#define ATA_CH_PRI      0x00
#define ATA_CH_SEC      0x01

#define ATA_DR_MST      0x00
#define ATA_DR_SLV      0x10

#define ATA_DR(dr)      ((dr==0)?ATA_DR_MST:ATA_DR_SLV)

#define ATA_AMODE_CHS   0x00
#define ATA_AMODE_LBA   0x40

#define SATA_DEV0_PRI   0x01
#define SATA_DEV0_SEC   0x02
#define SATA_DEV1_PRI   0x04
#define SATA_DEV1_SEC   0x08

/*******************************************************************************
                 Ata registers
*******************************************************************************/

#define ATA_REG_DATA16      0
#define ATA_REG_ERR_FEAT    1
#define ATA_REG_ERR         ATA_REG_ERR_FEAT
#define ATA_REG_FEA         ATA_REG_ERR_FEAT
#define ATA_REG_SCT_CNT     2
#define ATAPI_REG_CAUSE_INTR  2
#define ATA_REG_SECTOR      3
#define ATA_REG_CYL_LO      4
#define ATA_REG_CYL_HI      5
#define ATA_REG_LBA_LO      3
#define ATA_REG_LBA_MI      4
#define ATA_REG_LBA_HI      5
#define ATA_REG_DRV_HD      6
#define ATA_REG_STS_CMD     7
#define ATA_REG_STS         ATA_REG_STS_CMD //RD:
#define ATA_REG_CMD         ATA_REG_STS_CMD //WR:
#define ATAPI_REG_BYTE_CNT_LO  4
#define ATAPI_REG_BYTE_CNT_HI  5

//control register offset
#define ATA_REG_CTRL        0
#define ATA_REG_ALT_STS     0

//DMA register offset
#define ATA_REG_DMA_CMD     0
#define ATA_REG_DMA_STS     2
#define ATA_REG_DMA_DTBL    4

/*******************************************************************************
           Pci Regs, Base IO locations
*******************************************************************************/

#define ATA_PCI_PROG_IF_OFFSET             0x09  // 1 byte
// Ata Colwientional IO block
#define ATA_COLWENTIONAL_PRI_COMMAND_BLOCK 0x1F0
#define ATA_COLWENTIONAL_PRI_CONTROL_BLOCK 0x3F6
#define ATA_COLWENTIONAL_SEC_COMMAND_BLOCK 0x170
#define ATA_COLWENTIONAL_SEC_CONTROL_BLOCK 0x376

#define ATA_CLWC_PRI_CMD_BASE(ch) (0x1F0-0x80*ch)
#define ATA_CLWC_PRI_CTRL_BASE(ch) (0x3F6-0x80*ch)

// Ata Native IO Block offset
#define ATA_NATIVE_PRI_COMMAND_BLOCK_OFFSET 0x10  /*PCI_BAR_0_OFFSET, IO*/
#define ATA_NATIVE_PRI_CONTROL_BLOCK_OFFSET 0x14  /*PCI_BAR_1_OFFSET, IO*/
#define ATA_NATIVE_SEC_COMMAND_BLOCK_OFFSET 0x18  /*PCI_BAR_3_OFFSET, IO*/
#define ATA_NATIVE_SEC_CONTROL_BLOCK_OFFSET 0x1c  /*PCI_BAR_3_OFFSET, IO*/
#define ATA_NATIVE_DMA_BLOCK_OFFSET         0x20  /*PCI_BAR_4_OFFSET, IO*/
#define ATA_NATIVE_BLOCK_MASK               0xfffe
#define SATA_PCI_SCR_BASE_OFFSET            0x24  /*PCI_BAR_5_OFFSET, Mem*/

// Programing Interface Byte bits
#define ATA_PROGRAM_INTERFACE_BIT_MASK             0xFF
#define ATA_PROGRAM_INTERFACE_PRI_NATIVE           0x01
#define ATA_PROGRAM_INTERFACE_PRI_FIXED            0x02
#define ATA_PROGRAM_INTERFACE_SEC_NATIVE           0x04
#define ATA_PROGRAM_INTERFACE_SEC_FIXED            0x08
#define ATA_PROGRAM_INTERFACE_BUS_MASTER_ENABLED   0x80

/*******************************************************************************
                 Ata register bit definations
*******************************************************************************/
// Status Register
#define ATA_STS_ERR     0x01  // bit 0 Error
#define ATA_STS_IDX     0x02  // bit 1 Index
#define ATA_STS_CORR    0x04  // bit 2 Corrected Data
#define ATA_STS_DRQ     0x08  // bit 3 Data request
#define ATA_STS_DSC     0x10  // bit 4 Drive seek complete
#define ATA_STS_DF      0x20  // bit 5 Drive Fault
#define ATA_STS_DRDY    0x40  // bit 6 Drive Ready
#define ATA_STS_BSY     0x80  // bit 7 Busy

// Error Register
#define ATA_ERR_AMNF    0x01  // bit 0 Address Mark not found
#define ATA_ERR_TK0NF   0x02  // bit 1 Track 0 not found
#define ATA_ERR_ABRT    0x04  // bit 2 Aborted command
#define ATA_ERR_MCR     0x08  // bit 3 Media change requested
#define ATA_ERR_IDFNF   0x10  // bit 4 ID not found
#define ATA_ERR_MC      0x20  // bit 5 Media change
#define ATA_ERR_UNC     0x40  // bit 6 Uncorrectalbe data error
#define ATA_ERR_BBK     0x80  // bit 7 bad block detected

// Ata DriveHead  Register bits
#define ATA_DRHD_FIXED      0xA0 // bit 7 & 5 = 1
#define ATA_DRHD_HEAD_MASK  0x0f
#define ATA_DRHD_DRIVE_MASK 0x10
#define ATA_DRHD_AMODE_MASK 0x40

/*******************************************************************************
                 ATA / ATAPI Bus master regs
*******************************************************************************/

// DMA Command & Status Register
// bit 7:4 reserved
#define ATA_DMA_COMMAND_BUS_MASTER_RD   0x00   // bit 3
#define ATA_DMA_COMMAND_BUS_MASTER_WR   0x08   // bit 3
// bit 2:1 reserved
#define ATA_DMA_COMMAND_STOP            0x00   // bit 0
#define ATA_DMA_COMMAND_START           0x01   // bit 0

// BusMaster ATA Status Register bit values
#define ATA_DMA_STATUS_SIMPLEX_ONLY 0x80  // bit 7
#define ATA_DMA_STATUS_ENABLE_D1    0x40  // bit 6
#define ATA_DMA_STATUS_ENABLE_D0    0x20  // bit 5
// bit 3:4 reserved
#define ATA_DMA_STATUS_INTR         0x04  // bit 2
#define ATA_DMA_STATUS_ERROR        0x02  // bit 1
#define ATA_DMA_STATUS_ACTIVE       0x01  // bit 0

// Dma Channel Offsets from the DMA IoBlock Base
#define ATA_DMA_PRI_CHANNEL_OFFSET 0x0
#define ATA_DMA_SEC_CHANNEL_OFFSET 0x8

/*******************************************************************************
                       Atapi device register bits
******************************************************************************/
// AtaPi ATAPI_REG_ERROR
#define ATAPI_ERROR_ILI              0x01 // bit 0 Illegal length
#define ATAPI_ERROR_EOM              0x02 // bit 1 End of Media detected
#define ATAPI_ERROR_ABRT             0x03 // bit 2
#define ATAPI_ERROR_MCR              0x04 // bit 3
#define ATAPI_ERROR_SENSE_KEY        0xf0 // bit 4 - 7

// AtaPi ATAPI_REG_FEATURE
#define ATAPI_FEATURE_DMA            0x01 // bit 0
#define ATAPI_FEATURE_OVERLAP        0x02 // bit 1
/*
DMADIR - This bit indicates Packet DMA direction and is used only for devices
that implement the Packet Command feature set with a Serial ATA bridge that
require direction indication from the host. Support for this bit is determined
by reading bit 15 of word 62 in the IDENTIFY PACKET DEVICE data.
If bit 15 of word 62 is set to 1, the device requires the use of the DMADIR bit
for Packet DMA commands.
If the device requires the DMADIR bit to be set for Packet DMA operations and
the current operations is DMA (i.e. bit 0, the DMA bit, is set), this bit
indicates the direction of data transfer (0 = transfer to the device; 1 = transfer
to the host). If the device requires the DMADIR bit to be set for Packet DMA
operations but the current operations is PIO (i.e. bit 0, the DMA bit, is cleared),
this bit is ignored.
*/

#define ATAPI_FEATURE_DMAIN         0x04 // bit 1

// AtaPi ATAPI_REG_CAUSE_OF_INTR
#define ATAPI_INTR_COD               0x01 // bit 0  Command(1) or Data(0)
#define ATAPI_INTR_IO                0x02 // bit 1  to-host(1), to-device(0)
#define ATAPI_INTR_RELEASE           0x04 // bit 2
// reserved bit 3 - 7

// AtaPi DriveSelect  Register bits
// See atashare.h

// Status Register
#define ATAPI_STATUS_ERR     0x01  // bit 0
#define ATAPI_STATUS_RSV     0x02  // bit 1
#define ATAPI_STATUS_CORR    0x04  // bit 2
#define ATAPI_STATUS_DRQ     0x08  // bit 3
#define ATAPI_STATUS_SRV     0x10  // bit 4
#define ATAPI_STATUS_DMA     0x20  // bit 5
#define ATAPI_STATUS_DRDY    0x40  // bit 6
#define ATAPI_STATUS_BSY     0x80  // bit 7

// Index to find Device Paramters
#define ATAPI_CONFIGURATION           0
#define ATAPI_CONIG_PACKET_SIZE_MASK  0x0002 // bit 0 - 1
#define ATAPI_CONIG_PACKET_SIZE_12    0x0000 // for all CD_ROM
#define ATAPI_CONIG_PACKET_SIZE_16    0x0001 // for other devices

#define ATAPI_CONIG_DRQ_MODE_MASK     0x0020 // bit 5 - 6
#define ATAPI_CONIG_DRQ_MODE_3MS      0x0000
#define ATAPI_CONIG_DRQ_MODE_10MS     0x0020
#define ATAPI_CONIG_DRQ_MODE_50US     0x0040

#define ATAPI_CONIG_DEVICE_TYPE_MASK  0x0f00 // bit 8 - 11
#define ATAPI_CONIG_CD_ROM            0x0500

#define ATAPI_CONIG_FORMAT_MASK       0x8000 // bit 15
#define ATAPI_CONIG_FORMAT_ATA        0x0000
#define ATAPI_CONIG_FORMAT_ATAPI      0x8000
//---------------------------------------------------
#define ATAPI_CAPABILITIES            49
#define ATAPI_CAP_DMA_SUPPORTED       0x010 //bit 8
#define ATAPI_CAP_LBA_SUPPORTED       0x020 //bit 9
#define ATAPI_CAP_IORDY_SUPPORTED     0x040 //bit 11

#define ATAPI_PIO_TIMEIMG             51  // bit 8 - 15
#define ATAPI_DMA_TIMEIMG             52  // bit 8 - 15
//---------------------------------------------------

/*******************************************************************************
                 Defination of Device Parameters
*******************************************************************************/
// Index to find Device Paramters
#define ATA_DRIVE_PARAM_CONFIG_INDEX            0
#define ATA_DRIVE_PARAM_CONFIG_HD               0x0040
#define ATA_DRIVE_PARAM_CONFIG_MEDIUM           0x0080

#define ATA_DRIVE_PARAM_NUM_LOG_CYLS_INDEX      1  // same as 54
#define ATA_DRIVE_PARAM_NUM_LOG_HEADS_INDEX     3  // same as 55
#define ATA_DRIVE_PARAM_NUM_LOG_SECT_INDEX      6  // same as 56

#define IDENTIFY_DEVICE_SERIAL_NUMBER_START      10
#define IDENTIFY_DEVICE_SERIAL_NUMBER_END        19
#define IDENTIFY_DEVICE_MODEL_NUMBER_START      27
#define IDENTIFY_DEVICE_MODEL_NUMBER_END        46
#define IDENTIFY_DEVICE_FIRMWARE_REVISION_START      23
#define IDENTIFY_DEVICE_FIRMWARE_REVISION_END        26

#define IDENTIFY_DEVICE_MULT_CMD_SECT_CNT        47

#define ATA_DRIVE_PARAM_CAPABILITY_INDEX        49
#define ATA_DRIVE_PARAM_CAP_DMA_SUPPORTED       0x0100
#define ATA_DRIVE_PARAM_CAP_LBA_SUPPORTED       0x0200
#define ATA_DRIVE_PARAM_CAP_IORDY_DEACTIVATED   0x0400
#define ATA_DRIVE_PARAM_CAP_IORDY_SUPPORTED     0x0800
#define ATA_DRIVE_PARAM_CAP_STANDBY_TIMER       0x2000

#define ATA_DRIVE_PARAM_TIMING_PIO_INDEX        51 // bit 8-15
#define ATA_DRIVE_PARAM_TIMING_DMA_INDEX        52 // bit 8-15

#define ATA_DRIVE_PARAM_MAX_CYLN_INDEX    54  // Apparent number of cylinders
#define ATA_DRIVE_PARAM_MAX_HEAD_INDEX    55  // Apparent number of heads
#define ATA_DRIVE_PARAM_MAX_SECT_INDEX    56  // Apparent number of sectors per track
#define ATA_DRIVE_PARAM_TTL_SECT_LS_INDEX 57  // Apparent capacity in sectors
#define ATA_DRIVE_PARAM_TTL_SECT_MS_INDEX 58
#define ATA_DRIVE_PARAM_TTL_SECT_LBA_LS_INDEX   60 // The same as 57
#define ATA_DRIVE_PARAM_TTL_SECT_LBA_MS_INDEX   61 // The same as 58

#define ATA_DRIVE_PARAM_SINGLE_DMA_MODE_INDEX   62 // 0-7 Supported, 8-15 Atcive
#define ATA_DRIVE_PARAM_MDMA_MODE_INDEX         63 // 0-7 Supported, 8-15 Atcive
#define ATA_DRIVE_PARAM_DMA_MODE_INDEX          64
#define ATA_DRIVE_PARAM_CYCLE_MDMA_MIN_INDEX    65
#define ATA_DRIVE_PARAM_CYCLE_MDMA_RCM_INDEX    66
#define ATA_DRIVE_PARAM_CYCLE_PIO_MIN_INDEX     67
#define ATA_DRIVE_PARAM_CYCLE_PIO_MIN_IORY_INDEX 68
#define ATA_DRIVE_PARAM_QUEUED_DEPTH_INDEX      75

#define SATA_DRIVE_PARAM_CAPABILITY_INDEX       76
#define SATA_DRIVE_PARAM_GEN1_SPEED_SUPPORTED   0x2
#define SATA_DRIVE_PARAM_GEN2_SPEED_SUPPORTED   0x4
#define SATA_DRIVE_PARAM_NCQ_SUPPORTED          0x0100
#define SATA_DRIVE_PARAM_HIPM_SUPPORTED         0x0200

#define SATA_DRIVE_PARAM_CAPABILITY_INDEX2      78
#define SATA_DRIVE_PARAM_DIPM_SUPPORTED         0x8
#define SATA_DRIVE_PARAM_ASYNC_NOTIFICATION_SUPPORTED         0x20
#define SATA_DRIVE_PARAM_DEVSLEEP_SUPPORTED     0x100

#define ATA_DRIVE_PARAM_COMMAND_SET_SUPPORT     82
#define ATA_DRIVE_PARAM_COMMAND_SET_SELWRITY    0x2
#define ATA_DRIVE_PARAM_COMMAND_SET_WRITE_CACHE 0x20

#define ATA_DRIVE_PARAM_COMMAND_SET_SUPPORT_2   83
#define ATA_DRIVE_PARAM_COMMAND_SET_2_ADV_POW_MGMT        0x4
#define ATA_DRIVE_PARAM_COMMAND_SET_2_AUTO_ACOUSTIC_MGMT  0x400

#define ATA_DRIVE_PARAM_EXTEN_SUPPORT_INDEX     83 // bit 10
#define ATA_DRIVE_PARAM_EXTEN_SUPPORT_INDEX     83 // bit 10
#define ATA_DRIVE_PARAM_EXTEN_FEATURE_INDEX     84
#define ATA_DRIVE_PARAM_UDMA_MODE_INDEX         88
#define ATA_DRIVE_PARAM_MAX_LBA48_START         100
#define ATA_DRIVE_PARAM_MAX_LBA48_END           103

#define ATA_DRIVE_PARAM_COMMAND_SET_FEATURE_DEFAULT     87
#define ATA_DRIVE_PARAM_GENERAL_PURPOSE_LOG_SUPPORTED   0x20

#define ATA_DRIVE_PARAM_WORD_119    119
#define ATA_DRIVE_PARAM_119_WR_RD_VERIFY_SUPPORTED       0x2
#define ATA_DRIVE_PARAM_119_FREE_FALL_CONTROL_SUPPORTED  0x20

#define ATA_DRIVE_PARAM_EXTEN_SUPPORT_INDEX_ADDR48_SUPPORT_BIT      10
#define ATA_DRIVE_PARAM_EXTEN_SUPPORT_INDEX_DMA_QUEUE_SUPPORT_BIT   1
#define ATA_DRIVE_PARAM_COMMAND_SET_SUPPORT_PM_START  3
#define ATA_DRIVE_PARAM_FEATURE_SET_EXTN_LOG_START  5

/*******************************************************************************
                 Ata commands
*******************************************************************************/

// Reset Channel
#define ATA_CHANNEL_RESET  0x04

// soft reset, used by AHCI
#define ATA_COMMAND_SOFT_RESET      0x00
#define ATA_COMMAND_DEVICE_RESET    0x08
// external Ata device types, used by ATA(SATA) and AHCI
#define ATA_DRV     0x00
#define ATAPI_DRV   0x01
#define ATA_PMP     0x02
#define ATA_DEV_ILW 0xff
// external Ata device signature, used by ATA(SATA) and AHCI
#define ATA_DRV_SIG    0x00000101
#define ATAPI_DRV_SIG  0xeb140101
#define ATA_PMP_SIG    0x96690101

// Mandatory Commands
#define ATA_COMMAND_SET_MULTIPLE                   0xc6
// Class1: read command with Pio
#define ATA_COMMAND_READ_SECTORS                   0x20
#define ATA_COMMAND_READ_SECTORS_NO_RETRIES        0x21
#define ATA_COMMAND_READ_SECTORS_EXT               0x24
#define ATA_COMMAND_READ_MULTIPLE                  0xc4
#define ATA_COMMAND_READ_MULTI_EXT                 0x29

#define ATA_COMMAND_READ_LOG_MULTIPLE              0x2f

// Class2: wrtie command with Pio
#define ATA_COMMAND_WRITE_SECTORS                  0x30
#define ATA_COMMAND_WRITE_SECTORS_NO_RETRIES       0x31
#define ATA_COMMAND_WRITE_SECTORS_EXT              0x34
#define ATA_COMMAND_WRITE_MULTIPLE                 0xc5
#define ATA_COMMAND_WRITE_MULTI_EXT                0x39

// Class3: command without data transfer
#define ATA_COMMAND_EXELWTE_DIAGNOSTICS            0x90
#define ATA_COMMAND_IDENTIFY_DEVICE                0xec
#define ATA_COMMAND_INITIALIZE_DEVICE              0x91
#define ATA_COMMAND_SEEK                           0x70
#define ATA_COMMAND_VERIFY_SECTORS                 0x40
#define ATA_COMMAND_VERIFY_SECTORS_NO_RETRIES      0x41
#define ATA_COMMAND_FLUSH_CACHE                    0xe7

// Optional Commands
// Class4: command with Dma
#define ATA_COMMAND_READ_SECTORS_DMA_EXT           0x25
#define ATA_COMMAND_READ_SECTORS_DMA_QUE_EXT       0x26
#define ATA_COMMAND_READ_SECTORS_DMA_QUEUED        0xc7
#define ATA_COMMAND_READ_SECTORS_DMA               0xc8
#define ATA_COMMAND_READ_SECTORS_DMA_NO_RETRIES    0xc9
#define ATA_COMMAND_WRITE_SECTORS_DMA_EXT          0x35
#define ATA_COMMAND_WRITE_SECTORS_DMA_QUE_EXT      0x36
#define ATA_COMMAND_WRITE_SECTORS_DMA              0xca
#define ATA_COMMAND_WRITE_SECTORS_DMA_NO_RETRIES   0xcb
#define ATA_COMMAND_WRITE_SECTORS_DMA_QUEUED       0xcc

// FPDMA Commands
#define ATA_COMMAND_READ_FPDMA_QUEUEUD      0x60
#define ATA_COMMAND_WRITE_FPDMA_QUEUEUD      0x61

#define ATA_COMMAND_CHECK_POWER_MODE        0xe5

// Port Multiplier Commands
#define ATA_COMMAND_READ_PMP              0xe4
#define ATA_COMMAND_WRITE_PMP             0xe8

// ATAPI Mandatory Commands
#define ATAPI_COMMAND_IDENTIFY_DEVICE     0xa1
#define ATAPI_COMMAND_PACKET              0xa0
#define ATAPI_COMMAND_SOFT_RESET          0x08 // different from ATA reset.
#define ATAPI_COMMAND_CHECK_POWER_MODE    0xe5
#define ATAPI_COMMAND_EXELWTE_DIAG        0x90
#define ATAPI_COMMAND_IDLE_IMMEDIATE      0xe1
#define ATAPI_COMMAND_NOP                 0x00
#define ATAPI_COMMAND_SERVICE             0xa2
#define ATAPI_COMMAND_SLEEP               0xe6
#define ATAPI_COMMAND_STANDBY_IMMEDIATE   0xe0

// Packet commands
#define ATAPI_COMMAND_PACKET                       0xa0
#define ATAPI_PACKET_COMMAND_INQUIRY               0x12  // M
#define ATAPI_PACKET_COMMAND_LOAD_UNLOAD           0xA6
#define ATAPI_PACKET_COMMAND_MACHANISM_STATUS      0xBD
#define ATAPI_PACKET_COMMAND_MODE_SELECT           0x55  // M
#define ATAPI_PACKET_COMMAND_MODE_SENSE            0x5A  // M
#define ATAPI_PACKET_COMMAND_PAUSE_RESUME          0x4B
#define ATAPI_PACKET_COMMAND_PLAY_AUDIO            0x45
#define ATAPI_PACKET_COMMAND_PLAY_AUDIO_MSF        0x47
#define ATAPI_PACKET_COMMAND_PLAY_CD               0xBC
#define ATAPI_PACKET_COMMAND_FORMAT_UINT           0x04  // M
#define ATAPI_PACKET_COMMAND_SET_STREAMING         0xb6  // M

#define ATAPI_PACKET_COMMAND_READ_10               0x28  // M, Legacy
#define ATAPI_PACKET_COMMAND_READ_12               0xA8  // M
#define ATAPI_PACKET_COMMAND_READ_CDROM_CAP        0x25  // M, Legacy
#define ATAPI_PACKET_COMMAND_READ_FORMAT_CAP       0x23  // M
#define ATAPI_PACKET_COMMAND_READ_CD               0xBE  // M
#define ATAPI_PACKET_COMMAND_READ_CD_MSF           0xB9  // M
#define ATAPI_PACKET_COMMAND_READ_HEADER           0x44  // M
#define ATAPI_PACKET_COMMAND_READ_SUB_CHANNEL      0x42  // M
#define ATAPI_PACKET_COMMAND_READ_TOC              0x43  // M
#define ATAPI_PACKET_COMMAND_REQUEST_SENSE         0x03  // M
#define ATAPI_PACKET_COMMAND_SCAN                  0xBA
#define ATAPI_PACKET_COMMAND_SEEK                  0x2B  // M
#define ATAPI_PACKET_COMMAND_SET_CD_SPEED          0xBB
#define ATAPI_PACKET_COMMAND_STOP_PLAY_SCAN        0x4E  // M
#define ATAPI_PACKET_COMMAND_START_STOP_UNIT       0x1B  // M
#define ATAPI_PACKET_COMMAND_TEST_UNIT_READY       0x00  // M

// New command according to doc sff-8070
#define ATAPI_PACKET_COMMAND_VERIFY                0x2f
#define ATAPI_PACKET_COMMAND_WRITE_10              0x2a
#define ATAPI_PACKET_COMMAND_WRITE_12              0xaa
#define ATAPI_PACKET_COMMAND_WRITE_VERIFY          0x2e

/*******************************************************************************
           Set Feature command, needed for set transfer mode
           This part is shared between Ata and Atapi
*******************************************************************************/
// Class3: Command with no data transfer
#define ATA_COMMAND_SET_FEATURE                    0xEF

// Sub Commands of Set_Feature
#define ATA_COMMAND_SET_FEATURE_ENABLE_8BIT_PIO_XFER_MODE       0x01
// set transfer mode according to sector count register value
#define ATA_COMMAND_SET_FEATURE_SET_XFER_MODE                   0x03
#define ATA_COMMAND_SET_FEATURE_DISABLE_8BIT_PIO_XFER_MODE      0x81
// Sector Count values for SET FEATURE  tranfer mode
//tranfer mode               // XferRate
#define ATA_XFER_MODE_DEFAULT      0x00
// Pio transfer mode with flow control, mode nnn  00001nnn(b)
#define ATA_XFER_MODE_PIO_0        0x08
#define ATA_XFER_MODE_PIO_1        0x09
#define ATA_XFER_MODE_PIO_2        0x0a
#define ATA_XFER_MODE_PIO_3        0x0b
#define ATA_XFER_MODE_PIO_4        0x0c

// Multiple Dma transfer mode with flow control, mode nnn  00100nnn(b)
#define ATA_XFER_MODE_MDMA_0       0x20
#define ATA_XFER_MODE_MDMA_1       0x21
#define ATA_XFER_MODE_MDMA_2       0x22
// Ultra Dma ???
#define ATA_XFER_MODE_UDMA_0       0x40
#define ATA_XFER_MODE_UDMA_1       0x41
#define ATA_XFER_MODE_UDMA_2       0x42
#define ATA_XFER_MODE_UDMA_3       0x43
#define ATA_XFER_MODE_UDMA_4       0x44
#define ATA_XFER_MODE_UDMA_5       0x45
#define ATA_XFER_MODE_UDMA_6       0x46
#define ATA_XFER_MODE_UDMA_SLOW    0x40
#define ATA_XFER_MODE_UDMA_DISABLE 0x99

//                                   code       real rate in ns
// Pio transfer rate with flow control
#define ATA_TIMING_CTRL_PIO_0        0xA8    // (330, 270)
#define ATA_TIMING_CTRL_PIO_1        0x47    // (150, 240)
#define ATA_TIMING_CTRL_PIO_2        0x33    // (120, 120)
#define ATA_TIMING_CTRL_PIO_3        0x22    // ( 90,  90)
#define ATA_TIMING_CTRL_PIO_4        0x20    // ( 90,  30)
// Multiple Dma transfer rate with flow control
#define ATA_TIMING_CTRL_MDMA_0       0x77    // (240, 240)
#define ATA_TIMING_CTRL_MDMA_1       0x21    // ( 90,  90)
#define ATA_TIMING_CTRL_MDMA_2       0x20    // ( 90,  30)
// Ultra Dma rate
#define ATA_TIMING_CTRL_UDMA_SLOW    0xc3    // 150
#define ATA_TIMING_CTRL_UDMA_0       0xc2    // 120
#define ATA_TIMING_CTRL_UDMA_1       0xc1    //  90
#define ATA_TIMING_CTRL_UDMA_2       0xc0    //  60
#define ATA_TIMING_CTRL_UDMA_3       0xc4    //  50
#define ATA_TIMING_CTRL_UDMA_4       0xc5    //  30
#define ATA_TIMING_CTRL_UDMA_5       0xc6    //  20
#define ATA_TIMING_CTRL_UDMA_6       0xc7    //  15
#define ATA_TIMING_CTRL_UDMA_DISABLE 0x03    // disable UDMA

// ATA Command Sector Number (ATA/ATAPI standard ANS/T13 Project 1532D rev 4b)
#define ATA_MAX_RDWR_SECTS    0x100          // maximal number of sector is limited to 256

// Values for pci 0x5f and 0x5e
#define ATA_NON_DATA_TIMING_CTRL_PIO_0    0x99  // (300,300)
#define ATA_NON_DATA_TIMING_CTRL_PIO_1    0x92  // (300,90)
#define ATA_NON_DATA_TIMING_CTRL_PIO_2    0x90  // (300,30)
#define ATA_NON_DATA_TIMING_CTRL_PIO_3    0x22  // (90,90)
#define ATA_NON_DATA_TIMING_CTRL_PIO_4    0x20  // (90,30)
#define ATA_NON_DATA_TIMING_CTRL_MDMA_0   0x20  // (90,30)
#define ATA_NON_DATA_TIMING_CTRL_MDMA_1   0x20  // (90,30)
#define ATA_NON_DATA_TIMING_CTRL_MDMA_2   0x20  // (90,30)

// Values for pci 0x5c
#define ATA_SET_UP_TIMING_CTRL_DEFAULT    3
#define ATA_SET_UP_TIMING_CTRL_PIO_0      2
#define ATA_SET_UP_TIMING_CTRL_PIO_1      1
#define ATA_SET_UP_TIMING_CTRL_PIO_2      1
#define ATA_SET_UP_TIMING_CTRL_PIO_3      1
#define ATA_SET_UP_TIMING_CTRL_PIO_4      1
#define ATA_SET_UP_TIMING_CTRL_MDMA_0     2
#define ATA_SET_UP_TIMING_CTRL_MDMA_1     1
#define ATA_SET_UP_TIMING_CTRL_MDMA_2     1

/*************************** Pci registers*************************************/

// PCI Configuration Registers
// Cfg-21
#define PCI_TIMING_CTRL_PRI_D0      0x5b    // Primary Channel, Master Drive
#define PCI_TIMING_CTRL_PRI_D1      0x5a    // Primary Channel, Slave  Drive
#define PCI_TIMING_CTRL_SEC_D0      0x59    // Secondary Channel, Master Drive
#define PCI_TIMING_CTRL_SEC_D1      0x58    // Secondary Channel, Slave Drive

// Cfg-22
#define PCI_NON_DATA_TIMING_CTRL_PRI  0x5f    // Primary Channel, Master Drive
#define PCI_NON_DATA_TIMING_CTRL_SEC  0x5e    // Secondary Channel, Master Drive
//0x5d  reserved
#define PCI_SET_UP_TIMING_CTRL        0x5c    // All 4 drives
#define PCI_SET_UP_TIMING_CTRL_PRI_D0_SHIFT   6 // bit 6:7
#define PCI_SET_UP_TIMING_CTRL_PRI_D1_SHIFT   4 // bit 4:5
#define PCI_SET_UP_TIMING_CTRL_SEC_D0_SHIFT   2 // bit 2:3
#define PCI_SET_UP_TIMING_CTRL_SEC_D1_SHIFT   0 // bit 0:1
#define PCI_SET_UP_TIMING_MASK        0x3     // 2 bits

// Cfg-23
#define PCI_UDMA_TIMING_CTRL_PRI_D0 0x63    // Primary Channel, Master Drive
#define PCI_UDMA_TIMING_CTRL_PRI_D1 0x62    // Primary Channel, Slave  Drive
#define PCI_UDMA_TIMING_CTRL_SEC_D0 0x61    // Secondary Channel, Master Drive
#define PCI_UDMA_TIMING_CTRL_SEC_D1 0x60    // Secondary Channel, Slave Drive

/*******************************************************************************
           End of set transfer mode defines
*******************************************************************************/

/******************************************************************************/
// SAta Relate Pci Registers, shared by ATA(SATA) and ADMA
/******************************************************************************/
// Channel Enabled/disable status
// PCI_CFG_20 ==> move to PCI config offset 0x540 in MCP89
#define ATA_CHANNEL_SEC_EN                0x1
#define ATA_CHANNEL_PRI_EN                0x2
#define ATA_ADMA_SPACE_EN                 0x4
#define ATA_ADMA_SEC_EN                   0x10000
#define ATA_ADMA_PRI_EN                   0x20000

// Pci register about channel/drives
// PCI_CFG_24 ==> no longer found in MCP89
#define PCI_CTRL_MASTER_MASTER_ENABLE     0x1 // bit 0 default 1
#define PCI_CTRL_PRI_SCRAMBLER_DISABLE    0x2 // bit 1 default 0
#define PCI_CTRL_SEC_SCRAMBLER_DISABLE    0x4 // bit 2 default 0

// Pci register about SATA Phy
// PCI_CFG_25 ==> moved to PCI config offset 0xc00 (EMU1) in MCP89
// Reg default to be

// swaps the ordering of the 10-bit interface between south-bridge and
// external phy chip
#define PCI_PHY_SWAP_BIT_ORDER            0x00000001 // bit 0 default 0

// read Only field -- which PHY was selected by the ROM bit
#define PCI_PHY_SEL_MASK                  0x0000000e // bit 3:1 default 0
#define PCI_PHY_SEL_NOTHING               0x00000000 // value 0
#define PCI_PHY_SEL_MARVELL               0x00000002 // value 1
#define PCI_PHY_SEL_SI                    0x00000004 // value 2
#define PCI_PHY_SEL_LWDA_EXT              0x00000006 // value 3
#define PCI_PHY_SEL_LWDA_INT              0x00000008 // value 4

//adjusts the outbound timing of the txclk signals
#define PCI_PHY_DATA_EARLY                0x00000010 // bit 4
//adjusts the outbound timing of the txclk signals
#define PCI_PHY_TXCLK_EARLY               0x00000020 // bit 5
//turns on and off the power management feature.
#define PCI_PHY_PM_EN                     0x00000040 // bit 6

/*
The UART_DIV field adjusts the clock divider used to generate the serial clock
used for communicating with the Marvell external PHY chip. The reference clock
is 150MHz ( the frequency of the transmit clock ). The default value for the
divider is 15, which gives us 10 MHz serial clock ( baud rate ) The UART_SAMPLE
field adjusts the sampling position of the serial line. It should be set to
int(UART_DIV/2) - 1
*/
#define PCI_PHY_UART_DIV_MASK             0x0000ff00 // bit 15:8
#define PCI_PHY_UART_DIV_DEFAULT          0x00000f00 // value 0xf

#define PCI_PHY_UART_SAMPLE_MASK          0x00ff0000 // bit 23:16
#define PCI_PHY_UART_SAMPLE_DEFAULT       0x00060000 // value 6

/*
The BYPASS field tells the phy-interface block to ignore control-signals
information, normally present as an OOB information along with the data signals.
This field should only be used during debug.
*/
#define PCI_PHY_BYPASS_ENABLE             0x01000000 // bit 24 default 0

//toggles the behaviour of the phy-interface block to filter  ALIGN primitives
//in the received data stream. Link layer does not care about ALIGN primitives.
#define PCI_PHY_ABSORB_ALIGN_PRIM         0x02000000 // bit 25 default 0

/*
The USE_RBC1 affects whether the received data on channel 2 (Secondary) is
sampled on falling edge of rbc0, or sampled on rising edge of rbc1. SiliconImage
uses two receive clock, each 180 degree apart with each other.
*/
#define PCI_PHY_USE_RBC1                  0x04000000 // bit 26 default 0

/*
The UART_TIMEOUT field is a 4-bits field signifying the amount of time the uart
block wait for response from the Marvell PHY. The default value of 2 indicates
waiting for 2*128 clock cycles, where the clock is a 150MHz clock. The unit of
128 clock cyles is used here because a 10-bit serial data transmission takes 150
clock cyles. ( baud rate is 10 MHz )
*/
#define PCI_PHY_UART_TIMEOUT_MASK         0xf0000000 // bit 31:28
#define PCI_PHY_UART_TIMEOUT_INIT         0x20000000 // value 2

/******************************************************************************/
// SATA/AHCI Relate Pci Backdoor Registers for LWPU device, shared by SATA and AHCI
/******************************************************************************/
#define PCI_REG_BKDR0_PRE65          0x7f
#define PCI_REG_BKDR0_PRE79          0x7c
#define PCI_REG_BKDR_PRGIF_PRE79     0x62
#define PCI_REG_BKDR_CCODE_PRE65     0x9e
#define PCI_REG_BKDR_CCODE_PRE79     0x9c
#define PCI_REG_BKDR_DEVID_PRE79     0xf8
#define PCI_REG_BAR5                 0x24

#define PCI_REG_BKDR_PRGIF_89     0x54c
#define PCI_REG_BKDR_CCODE_89     0x4a4
#define PCI_REG_BKDR_DEVID_89     0xf8

/******************************************************************************/
// Macro for ATA/ATAPI command feature set
/******************************************************************************/
#define IS_48LBA_CMD(Command)                               \
        ( (Command==ATA_COMMAND_READ_FPDMA_QUEUEUD)         \
          || (Command==ATA_COMMAND_READ_SECTORS_EXT)        \
          || (Command==ATA_COMMAND_READ_MULTI_EXT)          \
          || (Command==ATA_COMMAND_READ_SECTORS_DMA_EXT)    \
          || (Command==ATA_COMMAND_WRITE_FPDMA_QUEUEUD)     \
          || (Command==ATA_COMMAND_WRITE_SECTORS_EXT)       \
          || (Command==ATA_COMMAND_WRITE_MULTI_EXT)         \
          || (Command==ATA_COMMAND_WRITE_SECTORS_DMA_EXT) )

#define IS_PM_CMD(Command)                                  \
        ( (Command==ATA_COMMAND_READ_PMP)                   \
          || (Command==ATA_COMMAND_WRITE_PMP) )

/******************************************************************************/
// Macro for ATA/ATAPI command parameters
/******************************************************************************/

#define CHECK_LBA(LbaAddr)                                        \
        if( LbaAddr >= ((UINT64)1<<28) )                          \
        {                                                         \
            Printf(Tee::PriError, "LbaAddr 0x%llx exceeds "       \
                      "28bit supported by the basic feature set\n",\
                      LbaAddr);                                   \
            return RC::BAD_PARAMETER;                             \
        }

#define CHECK_LBA_EXT(LbaAddr)                                    \
        if( LbaAddr >= ((UINT64)1<<48) )                          \
        {                                                         \
            Printf(Tee::PriError, "LbaAddr 0x%llx exceeds "       \
                      "48bit supported by the 48bit feature set\n", \
                      LbaAddr);                                   \
            return RC::BAD_PARAMETER;                             \
        }

#define CHECK_NUMSECT(NumSectors)                                 \
        if( (NumSectors==0) || (NumSectors>ATA_MAX_RDWR_SECTS) )  \
        {                                                         \
            Printf(Tee::PriError, "NumSectors 0x%x out of range [0x1,0x100]\n", \
                   NumSectors);                                   \
            return RC::BAD_PARAMETER;                             \
        }
#define CHECK_NUMSECT_EXT(NumSectors)                                  \
        if( (NumSectors==0) || (NumSectors>(ATA_MAX_RDWR_SECTS<<8)) )  \
        {                                                              \
            Printf(Tee::PriError, "NumSectors 0x%x out of range [0x1,0x10000]\n", \
                   NumSectors);                                        \
            return RC::BAD_PARAMETER;                                  \
        }

#endif
