/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FLASH_ACCESS_H
#define FLASH_ACCESS_H

/*!
 * @file   flash_access.h
 * @brief  This header-file defines the low-level interfaces to access flash
 *         device.
 *
 * The interfaces defined in this header will be implemented by the clients of
 * inforom FS library.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "lwtypes.h"
#include "inforom/status.h"

/*!
 * @brief Reads data from flash device at the given offset.
 *
 * @param[in]  pFlashContext  Generic pointer to flash context.
 * @param[in]  offset         Offset, where to start reading, in the flash device.
 * @param[in]  size           Bytes of data to read.
 * @param[out] pOutBuffer     Output buffer where data will be read to.
 *
 * @return FS_OK
 *     If data is read succesfully
 * @return Other unexpected errors
 */
FS_STATUS readFlash(void *pFlashAccess, LwU32 offset, LwU32 size, LwU8 *pOutBuffer);

/*!
 * @brief Writes data to the flash device at the given offset.
 *
 * @param[in]  pFlashContext  Generic pointer to flash context.
 * @param[in]  offset         Offset, where to start writing, in the flash device.
 * @param[in]  size           Bytes of data to write.
 * @param[in] pInBuffer       Input buffer with the data.
 *
 * @return FS_OK
 *     If data is written succesfully
 * @return Other unexpected errors
 */
FS_STATUS writeFlash(void *pFlashAccess, LwU32 offset, LwU32 size, LwU8 *pInBuffer);

/*!
 * @brief Erases the region of the flash device at the given offset.
 *
 * @param[in]  pFlashContext  Generic pointer to flash context.
 * @param[in]  offset         Offset, where to start erasing, in the flash device.
 * @param[in]  size           Bytes of data to erase.
 *
 * @return FS_OK
 *     If data is written succesfully
 * @return Other unexpected errors
 */
FS_STATUS eraseFlash(void *pFlashAccess, LwU32 offset, LwU32 size);

#ifdef __cplusplus
};
#endif

#endif // FLASH_ACCESS_H
