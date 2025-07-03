/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * Interface between application and RISC-V.
 */

#ifndef GSPHSIF_H
#define GSPHSIF_H

#define GSPHS_CMD_TIME_CORE_SWITCH  0x0
#define GSPHS_CMD_CORE_SWITCH_NEG   12

#define GSPHS_CMD_RANGE_HS_START    0x10
#define GSPHS_CMD_TIME_HS_SWITCH    GSPHS_CMD_RANGE_HS_START
#define GSPHS_CMD_HS_SANITY         0x11
#define GSPHS_CMD_HS_IRQTEST        0x12
#define GSPHS_CMD_HS_IRQCLR         0x13
#define GSPHS_CMD_HS_SWITCH         0x14
#define GSPHS_CMD_RANGE_HS_END      GSPHS_CMD_HS_SWITCH

#endif // GSPHSIF_H
