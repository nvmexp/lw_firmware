/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_VARS_H
#define DMVA_VARS_H

extern LwU32 _dataSize[];
#define dataSize ((LwU32)&_dataSize[0])

extern LwU32 _stack[];
#define stackBottom ((LwU32)&_stack[0])

extern LwU32 _ovl1_code_start[];
#define selwreCodeStart ((LwU32)&_ovl1_code_start[0])

extern LwU32 _ovl1_code_end[];
#define selwreCodeEnd ((LwU32)&_ovl1_code_end[0])

extern LwU32 _selwreSharedStart[];
#define selwreSharedStart ((LwU32)&_selwreSharedStart[0])

extern LwU32 _selwreSharedEnd[];
#define selwreSharedEnd ((LwU32)&_selwreSharedEnd[0])

#endif
