/*
 * _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#if !defined(__SMBUS_H__)
#define __SMBUS_H__

#define LW_SMBUS_SLAVE_ADDRESS                                        0x00000098U

#define LW_SMBUS_GET_BOOT_MODE_OVERRIDE                               0x00000010U
#define LW_SMBUS_SET_BOOT_MODE_OVERRIDE                               0x00000011U
#define LW_SMBUS_RESET_MLW                                            0x00000012U
#define LW_SMBUS_GET_MODE                                             0x00000013U

#define LW_SMBUS_GET_FLASH_MEMORY_SIZE                                0x00000020U
#define LW_SMBUS_GET_PAGE_BUFFER_CHECKSUM                             0x00000021U
#define LW_SMBUS_START_PAGE_ERASE_OPERATION                           0x00000022U
#define LW_SMBUS_GET_PAGE_ERASE_STATUS                                0x00000023U
#define LW_SMBUS_START_PAGE_PROGRAM_OPERATION                         0x00000024U
#define LW_SMBUS_GET_PAGE_PROGRAM_STATUS                              0x00000025U
#define LW_SMBUS_GET_APPLICATION_IMAGE_STATUS                         0x00000026U
#define LW_SMBUS_GET_BOOTLOADER_VERSION                               0x00000027U
#define LW_SMBUS_SET_READ_PAGE                                        0x00000028U

#define LW_SMBUS_WRITE_TO_PAGE_BUFFER(i)                      (0x00000040 + (i))
#define LW_SMBUS_WRITE_TO_PAGE_BUFFER__SIZE_1                                 32U

#define LW_SMBUS_READ_PAGE(i)                                 (0x00000040 + (i))
#define LW_SMBUS_READ_PAGE__SIZE_1                                            32U

#define LW_SMBUS_DATA_BOOT_MODE_OVERRIDE                                     0:0
#define LW_SMBUS_DATA_BOOT_MODE_OVERRIDE_NONE                         0x00000000U
#define LW_SMBUS_DATA_BOOT_MODE_OVERRIDE_BOOTLOADER                   0x00000001U
#define LW_SMBUS_DATA_RESET_MLW_ACTION                                       0:0
#define LW_SMBUS_DATA_RESET_MLW_ACTION_NONE                           0x00000000U
#define LW_SMBUS_DATA_RESET_MLW_ACTION_RESET_MLW                      0x00000001U
#define LW_SMBUS_DATA_MODE                                                   0:0
#define LW_SMBUS_DATA_MODE_BOOTLOADER                                 0x00000000U
#define LW_SMBUS_DATA_MODE_APPLICATION_IMAGE                          0x00000001U

#define LW_SMBUS_DATA_PAGE_ERASE_STATUS                                      1:0
#define LW_SMBUS_DATA_PAGE_ERASE_STATUS_SUCCEEDED                     0x00000000U
#define LW_SMBUS_DATA_PAGE_ERASE_STATUS_IN_PROGRESS                   0x00000001U
#define LW_SMBUS_DATA_PAGE_ERASE_STATUS_FAILED                        0x00000002U
#define LW_SMBUS_DATA_PAGE_PROGRAM_STATUS                                    1:0
#define LW_SMBUS_DATA_PAGE_PROGRAM_STATUS_SUCCEEDED                   0x00000000U
#define LW_SMBUS_DATA_PAGE_PROGRAM_STATUS_IN_PROGRESS                 0x00000001U
#define LW_SMBUS_DATA_PAGE_PROGRAM_STATUS_FAILED                      0x00000002U
#define LW_SMBUS_DATA_APPLICATION_IMAGE_STATUS                               0:0
#define LW_SMBUS_DATA_APPLICATION_IMAGE_STATUS_ILWALID                0x00000000U
#define LW_SMBUS_DATA_APPLICATION_IMAGE_STATUS_VALID                  0x00000001U
#define LW_SMBUS_DATA_BOOTLOADER_VERSION                                    15:0

#define STM32_FLASH_SECTOR_SIZE                                       0x00001000U
#define STM32_FLASH_PAGE_SIZE                                         0x00000400U
#define SMBUS_MAX_TRANSFER_LENGTH                                             32U

#endif /* __SMBUS_H__ */
