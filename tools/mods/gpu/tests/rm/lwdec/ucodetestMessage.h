/*****************************************************************************\
|*                                                                           *|
|*      Copyright 2015 LWPU Corporation.  All rights reserved.             *|
|*                                                                           *|
|*   THE SOFTWARE AND INFORMATION CONTAINED HEREIN IS PROPRIETARY AND        *|
|*   CONFIDENTIAL TO LWPU CORPORATION. THIS SOFTWARE IS FOR INTERNAL USE   *|
|*   ONLY AND ANY REPRODUCTION OR DISCLOSURE TO ANY PARTY OUTSIDE OF LWPU  *|
|*   IS STRICTLY PROHIBITED.                                                 *|
|*                                                                           *|
\*****************************************************************************/

#ifndef __UCODETEST_MESSAGE_H__
#define __UCODETEST_MESSAGE_H__

//----------------------------------------------------------------------------

// Basic message types and macros used to define messages and their handlers

// Defines types of message will be handled by ucodetest
typedef enum _MESSAGE_ID_ {
    MESSAGE_ID__MIN = 0,
    MESSAGE_ID__INIT = MESSAGE_ID__MIN,
    MESSAGE_ID__READ_DMEM,
    MESSAGE_ID__WRITE_DMEM,
    MESSAGE_ID__SE_TEST,
    MESSAGE_ID__MAX
} MESSAGE_ID;

//----------------------------------------------------------------------------
// Define specific messages
//
// To define a message, you need to provide
//   1. message id - for example PROC_DATA;
//   2. message content spec - for example "int d1; int d2;";
//   3. context content spec - for example "int r1; int r2;";
// and use DEFINE_MESSAGE macro as
//   DEFINE_MESSAGE(PROC_DATA, (int d1; int d2;) (int r1; int r2));
// This will define message typedef struct, message context typedef struct
// and the prototype for the message handler with name
//   MessagePROC_DATAHandler()
// This handler function will be added into dispatch table during initialization
// if it is used.
//
//----------------------------------------------------------------------------

// Message -- INIT
//  - Initialize message queue and its context
DEFINE_MESSAGE(INIT, ( ; ), ( ; ));

// Message -- READ_DMEM
//  - Read data from dmem of ucode test app
//  - Input:
//      dmemAddr - 256 bytes aligned dmem address to read from.
//      size     - size to read in bytes
//  - Output:
//      sizeIn   - data actually read in bytes
//
DEFINE_MESSAGE(READ_DMEM, (unsigned int dmemAddr; unsigned int size;), (int sizeIn;));

// Message -- WRITE_DMEM
//  - Write data to dmem of ucode test app
//  - Input:
//      dmemAddr - 256 bytes aligned dmem address to write to.
//      size     - size to write in bytes
//  - Output:
//      sizeOut  - data actually write in bytes
//
DEFINE_MESSAGE(WRITE_DMEM, (unsigned int dmemAddr; unsigned int size;), (int sizeOut;));

// Message -- SE_TEST
//  - Write data to dmem of ucode test app
//  - Input:
//      id       - id of test case to run.
//  - Output:
//      rc       - return code: 0   - succeed
//                              > 0 - error code
//
DEFINE_MESSAGE(SE_TEST, (unsigned int id;), (unsigned int rc;));

#endif //__UCODETEST_MESSAGE_H__
