/*
 * Copyright 2008-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 */

#ifndef FALCON_SPR_H
#define FALCON_SPR_H

#define EXCI_TRAP0                 0
#define EXCI_TRAP1                 1
#define EXCI_TRAP2                 2
#define EXCI_TRAP3                 3
#define EXCI_ILLEGAL_INST          8
#define EXCI_ILWALID_CODE          9
#define EXCI_IMEM_MISS            10
#define EXCI_IMEM_DOUBLE_HIT      11
#define EXCI_IBREAKPOINT          15
#define CSW_FIELD_F0            0x00
#define CSW_FIELD_F1            0x01
#define CSW_FIELD_F2            0x02
#define CSW_FIELD_F3            0x03
#define CSW_FIELD_F4            0x04
#define CSW_FIELD_F5            0x05
#define CSW_FIELD_F6            0x06
#define CSW_FIELD_F7            0x07
#define CSW_FIELD_C             0x08
#define CSW_FIELD_V             0x09
#define CSW_FIELD_N             0x0a
#define CSW_FIELD_Z             0x0b
#define CSW_FIELD_IE0           0x10
#define CSW_FIELD_IE1           0x11
#define CSW_FIELD_PIE0          0x14
#define CSW_FIELD_PIE1          0x15
#define CSW_FIELD_E             0x18

#endif // FALCON_SPR_H

