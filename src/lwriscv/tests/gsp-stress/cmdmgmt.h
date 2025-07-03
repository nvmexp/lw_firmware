/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CMDMGMT_H
#define CMDMGMT_H

#include <lwtypes.h>
#include <flcnretval.h>

extern void initCmdmgmt(void);
extern void sendRmInitMessage(FLCN_STATUS status);
extern void waitForUnload(void);
extern LwBool pollUnloadCommand(LwBool ackUnload);

#endif // CMDMGMT_H
