/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "../debug/elf.h"
#include <lwtypes.h>

LwBool libosElfGetStaticHeapSize(elf64_header *elf, LwU64 *additionalHeapSize);
LwBool libosElfGetBootEntry(elf64_header *elf, LwU64 *entry_offset);
