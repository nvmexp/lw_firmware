/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef __COMMON_MATS_H_INCLUDED
#define __COMMON_MATS_H_INCLUDED

#include "fakemods.h"

bool FindVideoController (UINT32 Index, UINT32 ClassCode, UINT32* pDomainNumber, UINT32* pBusNumber, UINT32* pDevice, UINT32* pFunction);
UINT32 PciRead32(UINT32 DomainNumber, UINT32 BusNumber, UINT32 Device, UINT32 Function, UINT32 Register);
bool MapPhysicalMemory(UINT32 PhysicalBase, size_t Size, UINT32** pVirtAddress);
bool OpenDriver();
void CloseDriver();

#endif // __COMMON_MATS_H_INCLUDED
