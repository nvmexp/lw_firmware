/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_IO_DIO_H
#define LIBLWRISCV_IO_DIO_H

#include <lwriscv/status.h>

/* =============================================================================
 * Public APIs
 * ========================================================================== */
typedef enum
{
    DIO_TYPE_ILWALID = 0,
#if LWRISCV_HAS_DIO_SE
    DIO_TYPE_SE,
#endif // LWRISCV_HAS_DIO_SE
#if LWRISCV_HAS_DIO_SNIC
    DIO_TYPE_SNIC,
#endif // LWRISCV_HAS_DIO_SNIC
#if LWRISCV_HAS_DIO_FBHUB
    DIO_TYPE_FBHUB,
#endif // LWRISCV_HAS_DIO_FBHUB
    DIO_TYPE_END
} DIO_TYPE;

//
// [deprecated] will be removed in liblwriscv 3.x
// DIO library was initially designed for DIO_TYPE_EXTRA being SNIC
//
#if LWRISCV_HAS_DIO_SNIC
#define DIO_TYPE_EXTRA DIO_TYPE_SNIC
#define DIO_TYPE_SNIC_PORT_IDX 0
#endif // LWRISCV_HAS_DIO_SNIC

typedef struct
{
    DIO_TYPE dioType;
    // [deprecated] will be removed in liblwriscv 3.x
    uint8_t portIdx;
} DIO_PORT;

typedef enum
{
    DIO_OPERATION_RD,
    DIO_OPERATION_WR
} DIO_OPERATION;

LWRV_STATUS dioReadWrite(DIO_PORT dioPort, DIO_OPERATION dioOp, uint32_t addr, uint32_t *pData);

#endif // LIBLWRISCV_IO_DIO_H
