/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * @file   rmt_spi.h
 *
 * @brief  Header file for SPI Test
 *
 */

#ifndef _rmt_spi_h_
#define _rmt_spi_h_

#define LW_SPI_READ_BUFFER_SIZE_IN_BYTES                256
#define LW_SPI_WRITE_BUFFER_SIZE_IN_BYTES                32
#define LW_SPI_ROM_SECTOR_SIZE_IN_BYTES                4096

#define LW_SPI_WRITE_BYTE     0x48
#define LW_SPI_WRITE_BUFFER   { 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, \
                              0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,   \
                              0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,   \
                              0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8 }

#endif // _rmt_spi_h_
