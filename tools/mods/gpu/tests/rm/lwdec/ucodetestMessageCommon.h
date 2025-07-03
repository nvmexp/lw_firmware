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

#ifndef __UCODETEST_MESSAGE_COMMON_H__
#define __UCODETEST_MESSAGE_COMMON_H__

//----------------------------------------------------------------------------

// Common header of message
typedef struct _MESSAGE_HEADER_ {
    unsigned int id;
    unsigned int seq;
    unsigned int size;
} MESSAGE_HEADER, *MESSAGE_HEADER_PTR;

// Message typedef
typedef struct _MESSAGE_ {
    MESSAGE_HEADER header;
    unsigned char content[1];
} MESSAGE, *MESSAGE_PTR;

// Current message processing status
//    INVALID    - no message is being processed;
//    NEW        - Client had finished send a message;
//    PROCESSING - uCode is in progress of processing a message;
//    FINISHED   - uCode has finished processing a message.
//
typedef enum _MESSAGE_STATE_ {
    MESSAGE_STATE__MIN = 0,
    MESSAGE_STATE__ILWALID = MESSAGE_STATE__MIN,
    MESSAGE_STATE__NEW,
    MESSAGE_STATE__PROCESS,
    MESSAGE_STATE__FINISHED,
    MESSAGE_STATE__MAX
} MESSAGE_STATE, *MESSAGE_STATE_PTR;

// Current message processing context header
//  id     - message id;
//  state  - Current process status;
//  seq    - Sequence number of the message;
//  size   - size of message processing context.
typedef struct _MESSAGE_CONTEXT_HEADER_ {
    unsigned int id;
    MESSAGE_STATE state;
    unsigned int seq;
    unsigned int size;
} MESSAGE_CONTEXT_HEADER, MESSAGE_CONTEXT_HEADER_PTR;

// Message processing context typedef
typedef struct _MESSAGE_CONTEXT_ {
    MESSAGE_CONTEXT_HEADER header;
    unsigned char context[1];
} MESSAGE_CONTEXT, *MESSAGE_CONTEXT_PTR;

// Message handlers
typedef void (*MESSAGE_HANDLER_FUNC_PTR) (MESSAGE_PTR pMessage,
                                          MESSAGE_CONTEXT_PTR pContext);

// Message dispatch table
typedef struct _MESSAGE_DISPATCH_TABLE_ {
    MESSAGE_HANDLER_FUNC_PTR handlers[1];
} MESSAGE_DISPATH_TABLE, *MESSAGE_DISPATH_TABLE_PTR;

// place holder for dispatch handlers
extern MESSAGE_DISPATH_TABLE_PTR pMessageDispather;

#if !defined(DECLARE_HANDLER_FOR_DISPATCH)
#define DECLARE_HANDLER_FOR_DISPATCH(id)                               \
extern void Message##id##_Handler( MESSAGE_PTR pMessage,               \
                                   MESSAGE_CONTEXT_PTR pContext)
#endif

//----------------------------------------------------------------------------
// Macro for define a message
//
#define MESSAGE_SPEC(spec) spec
#define DEFINE_MESSAGE(id, messageSpec, contextSpec)                   \
typedef struct {                                                       \
    MESSAGE_HEADER header;                                             \
    MESSAGE_SPEC messageSpec                                           \
} MESSAGE_##id, *MESSAGE_##id##_PTR;                                   \
typedef struct {                                                       \
    MESSAGE_CONTEXT_HEADER header;                                     \
    MESSAGE_SPEC contextSpec                                           \
} MESSAGE_CONTEXT_##id, *MESSAGE_CONTEXT_##id##_PTR;                   \
DECLARE_HANDLER_FOR_DISPATCH(id)

//----------------------------------------------------------------------------
// Message Queue related type defines
//
// Message queue is living in CtxDma area. It consists processing context area
// and egress message area. Each area lwrrently has a fixed size of 256 bytes.
//
#define MESSAGE_AREA_SIZE                   256
#define MESSAGE_SCRATCH_PAD_SIZE            (1*MESSAGE_AREA_SIZE)

typedef struct _MESSAGE_QUEUE_INFO_ {
    unsigned int  scratchPadDmemAddr;
} MESSAGE_QUEUE_INFO, *MESSATE_QUEUE_INFO_PTR;

typedef struct _MESSAGE_QUEUE_ {
    union {
        MESSAGE message;
        unsigned char byteArray[MESSAGE_AREA_SIZE];
    };
    union {
        MESSAGE_CONTEXT context;
        unsigned char contextByteArray[MESSAGE_AREA_SIZE];
    };
    union {
        MESSAGE_QUEUE_INFO messageQueueInfo;
        unsigned char infoByteArray[MESSAGE_AREA_SIZE];
    };
    unsigned char scratchPad[MESSAGE_SCRATCH_PAD_SIZE];
} MESSAGE_QUEUE, *MESSAGE_QUEUE_PTR;

#endif //__UCODETEST_MESSAGE_COMMON_H__
