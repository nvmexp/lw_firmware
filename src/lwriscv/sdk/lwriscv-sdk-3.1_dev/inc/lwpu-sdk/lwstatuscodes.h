/*******************************************************************************
    Copyright (c) 2014-2020 LWpu Corporation

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be
        included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef SDK_LWSTATUSCODES_H
#define SDK_LWSTATUSCODES_H

/* XAPIGEN - this file is not suitable for (nor needed by) xapigen.         */
/*           Rather than #ifdef out every such include in every sdk         */
/*           file, punt here.                                               */
#if !defined(XAPIGEN)        /* rest of file */

LW_STATUS_CODE(LW_OK,                                  0x00000000, "Success")
LW_STATUS_CODE(LW_ERR_GENERIC,                         0x0000FFFF, "Failure: Generic Error")

LW_STATUS_CODE(LW_ERR_BROKEN_FB,                       0x00000001, "Frame-Buffer broken")
LW_STATUS_CODE(LW_ERR_BUFFER_TOO_SMALL,                0x00000002, "Buffer passed in is too small")
LW_STATUS_CODE(LW_ERR_BUSY_RETRY,                      0x00000003, "System is busy, retry later")
LW_STATUS_CODE(LW_ERR_CALLBACK_NOT_SCHEDULED,          0x00000004, "The requested callback API not scheduled")
LW_STATUS_CODE(LW_ERR_CARD_NOT_PRESENT,                0x00000005, "Card not detected")
LW_STATUS_CODE(LW_ERR_CYCLE_DETECTED,                  0x00000006, "Call cycle detected")
LW_STATUS_CODE(LW_ERR_DMA_IN_USE,                      0x00000007, "Requested DMA is in use")
LW_STATUS_CODE(LW_ERR_DMA_MEM_NOT_LOCKED,              0x00000008, "Requested DMA memory is not locked")
LW_STATUS_CODE(LW_ERR_DMA_MEM_NOT_UNLOCKED,            0x00000009, "Requested DMA memory is not unlocked")
LW_STATUS_CODE(LW_ERR_DUAL_LINK_INUSE,                 0x0000000A, "Dual-Link is in use")
LW_STATUS_CODE(LW_ERR_ECC_ERROR,                       0x0000000B, "Generic ECC error")
LW_STATUS_CODE(LW_ERR_FIFO_BAD_ACCESS,                 0x0000000C, "FIFO: Invalid access")
LW_STATUS_CODE(LW_ERR_FREQ_NOT_SUPPORTED,              0x0000000D, "Requested frequency is not supported")
LW_STATUS_CODE(LW_ERR_GPU_DMA_NOT_INITIALIZED,         0x0000000E, "Requested DMA not initialized")
LW_STATUS_CODE(LW_ERR_GPU_IS_LOST,                     0x0000000F, "GPU lost from the bus")
LW_STATUS_CODE(LW_ERR_GPU_IN_FULLCHIP_RESET,           0x00000010, "GPU lwrrently in full-chip reset")
LW_STATUS_CODE(LW_ERR_GPU_NOT_FULL_POWER,              0x00000011, "GPU not in full power")
LW_STATUS_CODE(LW_ERR_GPU_UUID_NOT_FOUND,              0x00000012, "GPU UUID not found")
LW_STATUS_CODE(LW_ERR_HOT_SWITCH,                      0x00000013, "System in hot switch")
LW_STATUS_CODE(LW_ERR_I2C_ERROR,                       0x00000014, "I2C Error")
LW_STATUS_CODE(LW_ERR_I2C_SPEED_TOO_HIGH,              0x00000015, "I2C Error: Speed too high")
LW_STATUS_CODE(LW_ERR_ILLEGAL_ACTION,                  0x00000016, "Current action is not allowed")
LW_STATUS_CODE(LW_ERR_IN_USE,                          0x00000017, "Generic busy error")
LW_STATUS_CODE(LW_ERR_INFLATE_COMPRESSED_DATA_FAILED,  0x00000018, "Failed to inflate compressed data")
LW_STATUS_CODE(LW_ERR_INSERT_DUPLICATE_NAME,           0x00000019, "Found a duplicate entry in the requested btree")
LW_STATUS_CODE(LW_ERR_INSUFFICIENT_RESOURCES,          0x0000001A, "Ran out of a critical resource, other than memory")
LW_STATUS_CODE(LW_ERR_INSUFFICIENT_PERMISSIONS,        0x0000001B, "The requester does not have sufficient permissions")
LW_STATUS_CODE(LW_ERR_INSUFFICIENT_POWER,              0x0000001C, "Generic Error: Low power")
LW_STATUS_CODE(LW_ERR_ILWALID_ACCESS_TYPE,             0x0000001D, "This type of access is not allowed")
LW_STATUS_CODE(LW_ERR_ILWALID_ADDRESS,                 0x0000001E, "Address not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_ARGUMENT,                0x0000001F, "Invalid argument to call")
LW_STATUS_CODE(LW_ERR_ILWALID_BASE,                    0x00000020, "Invalid base")
LW_STATUS_CODE(LW_ERR_ILWALID_CHANNEL,                 0x00000021, "Given channel-id not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_CLASS,                   0x00000022, "Given class-id not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_CLIENT,                  0x00000023, "Given client not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_COMMAND,                 0x00000024, "Command passed is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_DATA,                    0x00000025, "Invalid data passed")
LW_STATUS_CODE(LW_ERR_ILWALID_DEVICE,                  0x00000026, "Current device is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_DMA_SPECIFIER,           0x00000027, "The requested DMA specifier is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_EVENT,                   0x00000028, "Invalid event oclwred")
LW_STATUS_CODE(LW_ERR_ILWALID_FLAGS,                   0x00000029, "Invalid flags passed")
LW_STATUS_CODE(LW_ERR_ILWALID_FUNCTION,                0x0000002A, "Called function is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_HEAP,                    0x0000002B, "Heap corrupted")
LW_STATUS_CODE(LW_ERR_ILWALID_INDEX,                   0x0000002C, "Index invalid")
LW_STATUS_CODE(LW_ERR_ILWALID_IRQ_LEVEL,               0x0000002D, "Requested IRQ level is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_LIMIT,                   0x0000002E, "Generic Error: Invalid limit")
LW_STATUS_CODE(LW_ERR_ILWALID_LOCK_STATE,              0x0000002F, "Requested lock state not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_METHOD,                  0x00000030, "Requested method not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_OBJECT,                  0x00000031, "Object not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_OBJECT_BUFFER,           0x00000032, "Object buffer passed is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_OBJECT_HANDLE,           0x00000033, "Object handle is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_OBJECT_NEW,              0x00000034, "New object is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_OBJECT_OLD,              0x00000035, "Old object is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_OBJECT_PARENT,           0x00000036, "Object parent is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_OFFSET,                  0x00000037, "The offset passed is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_OPERATION,               0x00000038, "Requested operation is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_OWNER,                   0x00000039, "Owner not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_PARAM_STRUCT,            0x0000003A, "Invalid structure parameter")
LW_STATUS_CODE(LW_ERR_ILWALID_PARAMETER,               0x0000003B, "At least one of the parameters passed is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_PATH,                    0x0000003C, "The requested path is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_POINTER,                 0x0000003D, "Pointer not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_REGISTRY_KEY,            0x0000003E, "Found an invalid registry key")
LW_STATUS_CODE(LW_ERR_ILWALID_REQUEST,                 0x0000003F, "Generic Error: Invalid request")
LW_STATUS_CODE(LW_ERR_ILWALID_STATE,                   0x00000040, "Generic Error: Invalid state")
LW_STATUS_CODE(LW_ERR_ILWALID_STRING_LENGTH,           0x00000041, "The string length is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_READ,                    0x00000042, "The requested read operation is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_WRITE,                   0x00000043, "The requested write operation is not valid")
LW_STATUS_CODE(LW_ERR_ILWALID_XLATE,                   0x00000044, "The requested translate operation is not valid")
LW_STATUS_CODE(LW_ERR_IRQ_NOT_FIRING,                  0x00000045, "Requested IRQ is not firing")
LW_STATUS_CODE(LW_ERR_IRQ_EDGE_TRIGGERED,              0x00000046, "IRQ is edge triggered")
LW_STATUS_CODE(LW_ERR_MEMORY_TRAINING_FAILED,          0x00000047, "Failed memory training sequence")
LW_STATUS_CODE(LW_ERR_MISMATCHED_SLAVE,                0x00000048, "Slave mismatch")
LW_STATUS_CODE(LW_ERR_MISMATCHED_TARGET,               0x00000049, "Target mismatch")
LW_STATUS_CODE(LW_ERR_MISSING_TABLE_ENTRY,             0x0000004A, "Requested entry missing not found in the table")
LW_STATUS_CODE(LW_ERR_MODULE_LOAD_FAILED,              0x0000004B, "Failed to load the requested module")
LW_STATUS_CODE(LW_ERR_MORE_DATA_AVAILABLE,             0x0000004C, "There is more data available")
LW_STATUS_CODE(LW_ERR_MORE_PROCESSING_REQUIRED,        0x0000004D, "More processing required for the given call")
LW_STATUS_CODE(LW_ERR_MULTIPLE_MEMORY_TYPES,           0x0000004E, "Multiple memory types found")
LW_STATUS_CODE(LW_ERR_NO_FREE_FIFOS,                   0x0000004F, "No more free FIFOs found")
LW_STATUS_CODE(LW_ERR_NO_INTR_PENDING,                 0x00000050, "No interrupt pending")
LW_STATUS_CODE(LW_ERR_NO_MEMORY,                       0x00000051, "Out of memory")
LW_STATUS_CODE(LW_ERR_NO_SUCH_DOMAIN,                  0x00000052, "Requested domain does not exist")
LW_STATUS_CODE(LW_ERR_NO_VALID_PATH,                   0x00000053, "Caller did not speficy a valid path")
LW_STATUS_CODE(LW_ERR_NOT_COMPATIBLE,                  0x00000054, "Generic Error: Incompatible types")
LW_STATUS_CODE(LW_ERR_NOT_READY,                       0x00000055, "Generic Error: Not ready")
LW_STATUS_CODE(LW_ERR_NOT_SUPPORTED,                   0x00000056, "Call not supported")
LW_STATUS_CODE(LW_ERR_OBJECT_NOT_FOUND,                0x00000057, "Requested object not found")
LW_STATUS_CODE(LW_ERR_OBJECT_TYPE_MISMATCH,            0x00000058, "Specified objects do not match")
LW_STATUS_CODE(LW_ERR_OPERATING_SYSTEM,                0x00000059, "Generic operating system error")
LW_STATUS_CODE(LW_ERR_OTHER_DEVICE_FOUND,              0x0000005A, "Found other device instead of the requested one")
LW_STATUS_CODE(LW_ERR_OUT_OF_RANGE,                    0x0000005B, "The specified value is out of bounds")
LW_STATUS_CODE(LW_ERR_OVERLAPPING_UVM_COMMIT,          0x0000005C, "Overlapping unified virtual memory commit")
LW_STATUS_CODE(LW_ERR_PAGE_TABLE_NOT_AVAIL,            0x0000005D, "Requested page table not available")
LW_STATUS_CODE(LW_ERR_PID_NOT_FOUND,                   0x0000005E, "Process-Id not found")
LW_STATUS_CODE(LW_ERR_PROTECTION_FAULT,                0x0000005F, "Protection fault")
LW_STATUS_CODE(LW_ERR_RC_ERROR,                        0x00000060, "Generic RC error")
LW_STATUS_CODE(LW_ERR_REJECTED_VBIOS,                  0x00000061, "Given Video BIOS rejected/invalid")
LW_STATUS_CODE(LW_ERR_RESET_REQUIRED,                  0x00000062, "Reset required")
LW_STATUS_CODE(LW_ERR_STATE_IN_USE,                    0x00000063, "State in use")
LW_STATUS_CODE(LW_ERR_SIGNAL_PENDING,                  0x00000064, "Signal pending")
LW_STATUS_CODE(LW_ERR_TIMEOUT,                         0x00000065, "Call timed out")
LW_STATUS_CODE(LW_ERR_TIMEOUT_RETRY,                   0x00000066, "Call timed out, please retry later")
LW_STATUS_CODE(LW_ERR_TOO_MANY_PRIMARIES,              0x00000067, "Too many primaries")
LW_STATUS_CODE(LW_ERR_UVM_ADDRESS_IN_USE,              0x00000068, "Unified virtual memory requested address already in use")
LW_STATUS_CODE(LW_ERR_MAX_SESSION_LIMIT_REACHED,       0x00000069, "Maximum number of sessions reached")
LW_STATUS_CODE(LW_ERR_LIB_RM_VERSION_MISMATCH,         0x0000006A, "Library version doesn't match driver version")  //Contained within the RMAPI library
LW_STATUS_CODE(LW_ERR_PRIV_SEC_VIOLATION,              0x0000006B, "Priv security violation")
LW_STATUS_CODE(LW_ERR_GPU_IN_DEBUG_MODE,               0x0000006C, "GPU lwrrently in debug mode")
LW_STATUS_CODE(LW_ERR_FEATURE_NOT_ENABLED,             0x0000006D, "Requested Feature functionality is not enabled")
LW_STATUS_CODE(LW_ERR_RESOURCE_LOST,                   0x0000006E, "Requested resource has been destroyed")
LW_STATUS_CODE(LW_ERR_PMU_NOT_READY,                   0x0000006F, "PMU is not ready ot has not yet been initialized")
LW_STATUS_CODE(LW_ERR_FLCN_ERROR,                      0x00000070, "Generic falcon assert or halt")
LW_STATUS_CODE(LW_ERR_FATAL_ERROR,                     0x00000071, "Fatal/unrecoverable error")
LW_STATUS_CODE(LW_ERR_MEMORY_ERROR,                    0x00000072, "Generic memory error")
LW_STATUS_CODE(LW_ERR_ILWALID_LICENSE,                 0x00000073, "License provided is rejected or invalid")
LW_STATUS_CODE(LW_ERR_LWLINK_INIT_ERROR,               0x00000074, "Lwlink Init Error")
LW_STATUS_CODE(LW_ERR_LWLINK_MINION_ERROR,             0x00000075, "Lwlink Minion Error")
LW_STATUS_CODE(LW_ERR_LWLINK_CLOCK_ERROR,              0x00000076, "Lwlink Clock Error")
LW_STATUS_CODE(LW_ERR_LWLINK_TRAINING_ERROR,           0x00000077, "Lwlink Training Error")
LW_STATUS_CODE(LW_ERR_LWLINK_CONFIGURATION_ERROR,      0x00000078, "Lwlink Configuration Error")

// Warnings:
LW_STATUS_CODE(LW_WARN_HOT_SWITCH,                     0x00010001, "WARNING Hot switch")
LW_STATUS_CODE(LW_WARN_INCORRECT_PERFMON_DATA,         0x00010002, "WARNING Incorrect performance monitor data")
LW_STATUS_CODE(LW_WARN_MISMATCHED_SLAVE,               0x00010003, "WARNING Slave mismatch")
LW_STATUS_CODE(LW_WARN_MISMATCHED_TARGET,              0x00010004, "WARNING Target mismatch")
LW_STATUS_CODE(LW_WARN_MORE_PROCESSING_REQUIRED,       0x00010005, "WARNING More processing required for the call")
LW_STATUS_CODE(LW_WARN_NOTHING_TO_DO,                  0x00010006, "WARNING Nothing to do")
LW_STATUS_CODE(LW_WARN_NULL_OBJECT,                    0x00010007, "WARNING NULL object found")
LW_STATUS_CODE(LW_WARN_OUT_OF_RANGE,                   0x00010008, "WARNING value out of range")

#endif // XAPIGEN

#endif /* SDK_LWSTATUSCODES_H */
