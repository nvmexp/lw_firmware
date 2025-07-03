/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  vbios.h
 * @brief VBIOS specific utility functions
 */

#ifndef INCLUDED_VBIOS_H
#define INCLUDED_VBIOS_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#include <vector>

RC VBIOSReverseLVDSTMDS(UINT08 *BiosImage);

#endif // INCLUDED_VBIOS_H

