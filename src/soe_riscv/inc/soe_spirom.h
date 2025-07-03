/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SPIDEV_ROM_H
#define SPIDEV_ROM_H

/* ------------------------- Application Includes -------------------------- */
#include "soe_eraseledger.h"
/* ------------------------- Types Definitions ----------------------------- */
typedef struct
{
    LwU8    manufacturerCode;
    LwU8    packedInfoBits;
    LwU16   deviceCode;
} SPI_DEVICE_ROM_INFO, *PSPI_DEVICE_ROM_INFO;

typedef struct
{
    LwU32 startAddress;
    LwU32 endAddress;
} ROM_REGION, *PROM_REGION;

// typedef struct ROM_REGION INFOROM_REGION, *PINFOROM_REGION;
// Hack to minimize code changes
#define INFOROM_REGION ROM_REGION
#define PINFOROM_REGION PROM_REGION

typedef struct
{
    SPI_DEVICE              super;
    SPI_DEVICE_ROM_INFO     spiRomInfo;
    INFOROM_REGION          *pInforom;
    ERASE_LEDGER_REGION     *pLedgerRegion;
    ROM_REGION              *pBiosRegion;
    LwBool                  bIsWriteProtected;
} SPI_DEVICE_ROM, *PSPI_DEVICE_ROM;

/* ------------------------ Global Variables ------------------------------- */
extern sysTASK_DATA SPI_DEVICE_ROM spiRom;

/* ------------------------- Defines --------------------------------------- */
/*
 * The identification and write interfaces lwrrently only support SPI ROMs
 * that have 4KB sector sizes, a 256-byte page-program command, and can be
 * identified by the JEDEC standard with command 0x9F. GP100+ only 1.8V
 * EEPROM is supported.
 *
 * All supported ROM parts are listed here:
 * https://wiki.lwpu.com/engwiki/index.php/VBIOS/Tools/LWFlash/VBIOS_LWFLASH_EEPROM_Information#Part_Compatibility_List
 *
 * Here is spec of a supported SPI ROM device:
 * //sw/main/apps/lwflashu/EEPROM Datasheets/winbond/Serial/W25Q80BV.pdf
 */

/*
 * Commands common to all parts
 */
#define LW_ROM_CMD_WRSR               (0x01)
#define LW_ROM_CMD_PP                 (0x02)
#define LW_ROM_CMD_READ               (0x03)
#define LW_ROM_CMD_RDSR               (0x05)
#define LW_ROM_CMD_WREN               (0x06)

/*
 * Commands that are unique to some parts
 */
#define LW_ROM_CMD_RDID_JEDEC         (0x9F)
#define LW_ROM_CMD_SE_20H             (0x20)
#define LW_ROM_CMD_SE_D7H             (0xD7)

#define LW_ROM_CMD_LEN                (1)
#define LW_ROM_ADDR_LEN               (3)
#define LW_ROM_ID_LEN                 (3)
#define LW_ROM_STATUS_REG_LEN         (1)
#define LW_ROM_CMD_DATA_LEN           (LW_ROM_CMD_LEN + \
                                       LW_ROM_ADDR_LEN)
#define LW_ROM_READ_START_OFFSET      (32)
#define LW_ROM_SECTOR_SIZE            (4096)

/*
 * SPI ROM specific information packed into single byte.
 */
#define LW_ROM_PACKED_INFO_WPMASK          3:0
#define LW_ROM_PACKED_INFO_WPMASK_INIT     0x0

#define LW_ROM_PACKED_INFO_SECMD           4:4
#define LW_ROM_PACKED_INFO_SECMD_INIT      0x0
#define LW_ROM_PACKED_INFO_SECMD_20H       0x0
#define LW_ROM_PACKED_INFO_SECMD_D7H       0x1

#define LW_ROM_PACKED_INFO_RDONLY          5:5
#define LW_ROM_PACKED_INFO_RDONLY_INIT     0x0
#define LW_ROM_PACKED_INFO_RDONLY_FALSE    0x0
#define LW_ROM_PACKED_INFO_RDONLY_TRUE     0x1

#define LW_ROM_PACKED_INFO_RSRVD           7:6

/*
 * The Write in Progress (WIP) bit is a volatile bit that indicates whether
 * device is busy in a write.
 */
#define LW_ROM_STATUS_REG_WIP         0:0
#define LW_ROM_STATUS_REG_WIP_TRUE    0x01
#define LW_ROM_STATUS_REG_WIP_FALSE   0x00

/*
 * The bit location of 1-byte EEPROM Staus Register in the
 * 32-bit GPU registers is different in SW bitbanging
 * LW_PMGR_ROM_SERIAL_BYPASS interface as compared to
 * HW-SPI controller interface i.e. LW_PMGR_SPI_DATA_ARRAY.
 */
#define LW_ROM_STATUS_REGISTER_SW     7:0
#define LW_ROM_STATUS_REGISTER_HW    15:8

/*
 * All SPI ROM parts support JEDEC standard Manufacturer
 * and Device Identification. The vendor specific unique
 * JEDEC ID can be read from ROM by sending RDID command
 * through SW bitbanging method or HW-SPI controller.
 */
#define LW_ROM_SW_JEDECID_MANUFACTURERCODE           7:0
#define LW_ROM_SW_JEDECID_DEVICECODEHI              15:8
#define LW_ROM_SW_JEDECID_DEVICECODELO             23:16

#define LW_ROM_HW_JEDECID_MANUFACTURERCODE          15:8
#define LW_ROM_HW_JEDECID_DEVICECODEHI             23:16
#define LW_ROM_HW_JEDECID_DEVICECODELO             31:24

/*
 * Manufacturer codes for supported parts
 */
#define LW_ROM_MANUFACTURER_CYPRESS   (0x01)
#define LW_ROM_MANUFACTURER_ATMEL     (0x1F)
#define LW_ROM_MANUFACTURER_ADESTO    (0x1F)
#define LW_ROM_MANUFACTURER_MICRON    (0x20)
#define LW_ROM_MANUFACTURER_AMIC      (0x37)
#define LW_ROM_MANUFACTURER_PMC       (0x7F)
#define LW_ROM_MANUFACTURER_ISSI      (0x9D)
#define LW_ROM_MANUFACTURER_SFM       (0xA1)
#define LW_ROM_MANUFACTURER_MACRONIX  (0xC2)
#define LW_ROM_MANUFACTURER_GD        (0xC8)
#define LW_ROM_MANUFACTURER_WINBOND   (0xEF)

/*
 * These values are chosen to be the maximum of all supported parts
 */
#define LW_ROM_PP_TIMEOUT_US          (12800)
#define LW_ROM_SE_TIMEOUT_US          (500000)
#define LW_ROM_WRSR_TIMEOUT_US        (100000)

#define LW_ROM_MAX_TIMEOUT_US         LW_ROM_SE_TIMEOUT_US

/*
 * Colwenience macros for EEPROM region validation.
 */
#define LW_ROM_IMAGE_ALIGN            (4096)

#define LW_ROM_REGION_INITIALIZED(pRegion)                                 \
    (((pRegion)->startAddress != ~((LwU32)0)) && ((pRegion)->startAddress != 0) && \
     ((pRegion)->endAddress   != ~((LwU32)0)) && ((pRegion)->endAddress   != 0))

#define LW_ROM_ADDRESS_OUTSIDE_BOUNDS(pRegion, address, size)            \
    (((address) < (pRegion)->startAddress) ||                            \
     (((address) + (size)) < (address)) ||                               \
     (((address) + (size)) > (pRegion)->endAddress))

#endif //SPIDEV_ROM_H
