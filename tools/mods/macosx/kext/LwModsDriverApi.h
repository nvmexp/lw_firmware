/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _LWMODSDRIVERAPI_H_
#define _LWMODSDRIVERAPI_H_

#include <libkern/OSTypes.h>
#include <AvailabilityMacros.h>

// Version number is in BSD.  Last two digits are the minor number,
// remaining digits are the major number.
//
#define LWMODSDRIVER_VERSION 0x100
#define kLwModsDriver "com_lwidia_driver_LwModsDriver"

#pragma pack(push, 8)

/* Methods exposed by LwModsUserClient for IOConnectMethod* */
enum {
    kLwModsDrvGetApiVersion,       /* Keep first, so that anything can change */
    kLwModsDrvEnterDebugger,
    kLwModsDrvGetPciInfo,
    kLwModsDrvPciConfigRead,
    kLwModsDrvPciConfigWrite,
    kLwModsDrvAllocPages,
    kLwModsDrvFreePages,
    kLwModsDrvMapSystemPages,
    kLwModsDrvMapDeviceMemory,
    kLwModsDrvUnmapMemory,
    kLwModsDrvGetPhysicalAddress,
    kLwModsDrvVirtualToPhysical,
    kLwModsDrvPhysicalToVirtual,
    kLwModsDrvIoRead,
    kLwModsDrvIoWrite,
    kLwModsDrvSetConsoleInfo,
    kLwModsDrvRegisterInterrupt,
    kLwModsDrvUnregisterInterrupt,
    kLwModsDrvCreateInterruptQueue,
    kLwModsDrvReleaseInterruptQueue,
    kLwModsDrvMethodCount       /* Must be last */
};

typedef uint32_t _LwModsDrvBool;

typedef struct _LwModsDrvGetApiVersionOut
{
    // If size changes in the future, user will get an error and know they're out of date
    uint32_t Version;
} __attribute__((aligned(8))) LwModsDrvGetApiVersionOut;

typedef char _LwModsDrvPciPath[512];

typedef struct _LwModsDrvGetPciInfoIn
{
    _LwModsDrvPciPath Path;
} __attribute__((aligned(8))) LwModsDrvGetPciInfoIn;

typedef struct _LwModsDrvGetPciInfoOut
{
    uint32_t ClassCode;
    uint16_t DeviceId;
    uint16_t VendorId;
    uint8_t BusNum;
    uint8_t DevNum;
    uint8_t FuncNum;
} __attribute__((aligned(8))) LwModsDrvGetPciInfoOut;

typedef struct _LwModsDrvPciConfigReadIn
{
    uint64_t Address;
    uint32_t DataSize;
    _LwModsDrvPciPath Path;
} __attribute__((aligned(8))) LwModsDrvPciConfigReadIn;

typedef struct _LwModsDrvPciConfigReadOut
{
    uint64_t Data;
} __attribute__((aligned(8))) LwModsDrvPciConfigReadOut;

typedef struct _LwModsDrvPciConfigWriteIn
{
    uint64_t Address;
    uint64_t Data;
    uint32_t DataSize;
    _LwModsDrvPciPath Path;
} __attribute__((aligned(8))) LwModsDrvPciConfigWriteIn;

#define kLwModsMemoryCached 1
#define kLwModsMemoryUncached 2
#define kLwModsMemoryWriteCombine 3
#define kLwModsMemoryWriteThrough 4
#define kLwModsMemoryWriteProtect 5

typedef struct _LwModsDrvAllocPagesIn
{
    uint64_t NumBytes;
    uint32_t AddressBits;
    uint32_t Attrib;
    _LwModsDrvBool Contiguous;
} __attribute__((aligned(8))) LwModsDrvAllocPagesIn;

typedef struct _LwModsDrvAllocPagesOut
{
    uint64_t MemoryHandle;
} __attribute__((aligned(8))) LwModsDrvAllocPagesOut;

typedef struct _LwModsDrvFreePagesIn
{
    uint64_t MemoryHandle;
} __attribute__((aligned(8))) LwModsDrvFreePagesIn;

typedef struct _LwModsDrvMapSystemPagesIn
{
    uint64_t MemoryHandle;
    uint64_t Offset;
    uint64_t NumBytes;
} __attribute__((aligned(8))) LwModsDrvMapSystemPagesIn;

typedef struct _LwModsDrvMapSystemPagesOut
{
    uint64_t VirtualAddress;
} __attribute__((aligned(8))) LwModsDrvMapSystemPagesOut;

typedef struct _LwModsDrvMapDeviceMemoryIn
{
    uint64_t PhysicalAddress;
    uint64_t NumBytes;
    uint32_t Attrib;
} __attribute__((aligned(8))) LwModsDrvMapDeviceMemoryIn;

typedef struct _LwModsDrvMapDeviceMemoryOut
{
    uint64_t VirtualAddress;
} __attribute__((aligned(8))) LwModsDrvMapDeviceMemoryOut;

typedef struct _LwModsDrvUnmapMemoryIn
{
    uint64_t VirtualAddress;
    uint64_t NumBytes;
} __attribute__((aligned(8))) LwModsDrvUnmapMemoryIn;

typedef struct _LwModsDrvGetPhysicalAddressIn
{
    uint64_t MemoryHandle;
    uint64_t Offset;
} __attribute__((aligned(8))) LwModsDrvGetPhysicalAddressIn;

typedef struct _LwModsDrvGetPhysicalAddressOut
{
    uint64_t PhysicalAddress;
} __attribute__((aligned(8))) LwModsDrvGetPhysicalAddressOut;

typedef struct _LwModsDrvVirtualToPhysicalIn
{
    uint64_t VirtualAddress;
} __attribute__((aligned(8))) LwModsDrvVirtualToPhysicalIn;

typedef struct _LwModsDrvVirtualToPhysicalOut
{
    uint64_t PhysicalAddress;
} __attribute__((aligned(8))) LwModsDrvVirtualToPhysicalOut;

typedef struct _LwModsDrvPhysicalToVirtualIn
{
    uint64_t PhysicalAddress;
} __attribute__((aligned(8))) LwModsDrvPhysicalToVirtualIn;

typedef struct _LwModsDrvPhysicalToVirtualOut
{
    uint64_t VirtualAddress;
} __attribute__((aligned(8))) LwModsDrvPhysicalToVirtualOut;

typedef struct _LwModsDrvIoReadIn
{
    uint64_t Address;
    uint32_t DataSize;
} __attribute__((aligned(8))) LwModsDrvIoReadIn;

typedef struct _LwModsDrvIoReadOut
{
    uint64_t Data;
} __attribute__((aligned(8))) LwModsDrvIoReadOut;

typedef struct _LwModsDrvIoWriteIn
{
    uint64_t Address;
    uint64_t Data;
    uint32_t DataSize;
} __attribute__((aligned(8))) LwModsDrvIoWriteIn;

typedef struct _LwModsDrvSetConsoleInfoIn
{
    uint32_t op;
} __attribute__((aligned(8))) LwModsDrvSetConsoleInfoIn;

#define kLwModsConsoleInfoDisable 1
#define kLwModsConsoleInfoEnable 2

typedef struct
{
    uint32_t Index;
    uint32_t Type;
    _LwModsDrvPciPath Path;
} __attribute__((aligned(8))) LwModsDrvRegisterInterruptIn;

typedef struct
{
    uint32_t Type;
    _LwModsDrvPciPath Path;
} __attribute__((aligned(8))) LwModsDrvUnregisterInterruptIn;

typedef struct
{
    uint64_t QueueAddress;
} __attribute__((aligned(8))) LwModsDrvCreateInterruptQueueOut;

// Interrupts types
#define kLwModsInterruptTypeInt 0
#define kLwModsInterruptTypeMsi 1

// Mach ports
#define kLwModsMachPortInterruptsQueue 0

typedef struct {
    uint32_t index;
} __attribute__((aligned(8))) LwModsInterrupt;

#pragma pack(pop)

#endif /* ifndef _LWMODSDRIVERAPI_H_ */

