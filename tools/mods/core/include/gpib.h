/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GPIB_H
#define INCLUDED_GPIB_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif

#define OFFLINE 0
#define ONLINE 1
#define SRQTrigger 0x41
#define SRQEvent   0x60
#define MAV        0x10
#define ESB        0x20
#define ESB_MAV    0x30
#define RQS_MAV    0x50
#define RQS_ESB    0x60
#define RQS_TRG    0x41
#define WaitSRQ_Timeout 40

namespace Gpib {

   RC GetGpibErrorStatus();
   RC CheckBufferSize();
   RC Initialize();

   RC FindDevice(string DeviceName, UINT32 *InstrumentAddress,
             string *response_string);

   RC WriteDevice( UINT32 Device , const string &SendString );
   RC WriteDevice( UINT32 Device , const char *SendString );
   RC ReadDevice( UINT32 Device );
   RC ReadStatusByte(INT32 address,short* result);
   RC CheckStatusByte(INT32 address,short mask);
   RC WaitSRQ(short* result);
   RC WaitStatus(INT32 address, short mask);
   RC Reset_GpibBuffer(INT32 size,char** buff);

   char *GetGpibBufferPtr();
   UINT32 GetGpibDescriptor();
   UINT32 GetBufferSize();
}

#endif // ! INCLUDED_GPIB_H
